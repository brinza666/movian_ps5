"""Serve the two halves of a synthetic torrent on loopback."""
import hashlib
import os
import re
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import threading

PIECES, SPAN = 400, 256 * 1024


def piece(index):
    value = bytes([index & 255]) * SPAN
    return b"\x1a\x45\xdf\xa3" + value[4:] if index == 0 else value


def packet(body):
    return struct.pack("!I", len(body)) + body


def serve_connection(listener, parity, count, identity, served, window, disconnect=False):
    with listener.accept()[0] as conn, conn.makefile("rb") as src:
        conn.settimeout(12)
        hello = src.read(68)
        assert hello[28:48] == identity
        conn.sendall(hello[:48] + b"-TEST00-000000000001")
        assert src.read(5) == packet(b"\x02")
        mask = 0xFF if parity is None else 0x55 if parity else 0xAA
        conn.sendall(packet(b"\x05" + bytes([mask]) * (count // 8)) + packet(b"\x01"))
        try:
            replies = []
            sliding_checked = False
            while header := src.read(4):
                assert struct.unpack("!I", header)[0] == 13
                kind, index, offset, size = struct.unpack("!BIII", src.read(13))
                assert kind == 6 and (parity is None or index % 2 == parity)
                assert 0 < size <= 16384 and offset + size <= SPAN
                if disconnect and index != 0:
                    return
                replies.append(packet(struct.pack("!BII", 7, index, offset) + piece(index)[offset:offset+size]))
                if len(replies) == window and window == 4 and not sliding_checked:
                    conn.sendall(replies.pop())
                    assert src.read(4) == struct.pack("!I", 13)
                    kind, next_index, next_offset, next_size = struct.unpack("!BIII", src.read(13))
                    assert (kind, next_index, next_offset, next_size) == (6, index, offset + size, 16384)
                    replies.append(packet(struct.pack("!BII", 7, index, next_offset) +
                                          piece(index)[next_offset:next_offset+next_size]))
                    sliding_checked = True
                if len(replies) == window or offset + size == SPAN:
                    conn.sendall(b"".join(reversed(replies)))
                    replies.clear()
                if offset + size == SPAN:
                    served.add(index)
        except (BrokenPipeError, ConnectionResetError):
            pass


def serve(listener, parity, count, identity, served):
    with listener:
        if parity == 0:
            serve_connection(listener, parity, count, identity, served, 16, disconnect=True)
            serve_connection(listener, parity, count, identity, served, 4)
        else:
            serve_connection(listener, parity, count, identity, served, 1 if parity is None else 16)


def udp_tracker(tracker, identity, ports, total, announced):
    with tracker:
        tracker.settimeout(5)
        request, source = tracker.recvfrom(2048)
        announced.set()
        magic, action, transaction = struct.unpack("!QII", request)
        assert magic == 0x41727101980 and action == 0
        token = 0x123456789abcdef0
        tracker.sendto(struct.pack("!IIQ", 0, transaction, token), source)
        request, source = tracker.recvfrom(2048)
        assert len(request) == 98
        connection, action, transaction = struct.unpack_from("!QII", request)
        assert connection == token and action == 1 and request[16:36] == identity
        assert struct.unpack_from("!Q", request, 64)[0] == total
        peers = b"".join(socket.inet_aton("127.0.0.1") + struct.pack("!H", int(port)) for port in ports)
        tracker.sendto(struct.pack("!IIIII", 1, transaction ^ 1, 60, 0, 2) + peers, source)
        tracker.sendto(struct.pack("!IIIII", 1, transaction, 60, 0, 2) + peers, source)


def stall_handshake(server, announced):
    with server:
        peer, _ = server.accept()
        with peer:
            # Hold both worker slots until the UDP tracker sees a connect.
            announced.wait(5)


def dht_reply(server, identity, fields, wrong_transaction=False):
    server.settimeout(7)
    packet, client = server.recvfrom(2048)
    assert b"9:get_peers" in packet and b"9:info_hash20:" + identity in packet
    transaction = re.search(rb"1:t4:(.{4})", packet, re.DOTALL)[1]
    prefix = b"d1:rd2:id20:01234567890123456789" + fields + b"e1:t4:"
    if wrong_transaction:
        invalid = bytes([transaction[0] ^ 1]) + transaction[1:]
        empty = b"d1:rd2:id20:01234567890123456789e1:t4:"
        server.sendto(empty + invalid + b"1:y1:re", client)
    server.sendto(prefix + transaction + b"1:y1:re", client)


def dht_bootstrap(server, identity, ports):
    with server, dht_socket() as referred:
        host, port = referred.getsockname()
        node = b"12345678901234567890" + socket.inet_aton(host) + struct.pack("!H", port)
        dht_reply(server, identity, b"5:nodes26:" + node + b"5:token1:a")
        peers = b"".join(b"6:" + socket.inet_aton("127.0.0.1") +
                         struct.pack("!H", int(peer)) for peer in ports)
        dht_reply(referred, identity, b"5:token1:a6:valuesl" + peers + b"e", wrong_transaction=True)


def dht_socket():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
        route.connect(("192.0.2.1", 9))
        host = route.getsockname()[0]
    bootstrap = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    bootstrap.bind((host, 0))
    return bootstrap


def verify_private_dht(binary):
    url = b"http://tracker.invalid/announce"
    count = PIECES
    hashes = b"".join(hashlib.sha1(piece(i)).digest() for i in range(count))
    info = (b"d6:lengthi%de4:name11:fixture.mkv12:piece lengthi%de6:pieces%d:" %
            (count * SPAN, SPAN, len(hashes))) + hashes + b"7:privatei1ee"
    with tempfile.TemporaryDirectory() as tmp, dht_socket() as bootstrap:
        root = pathlib.Path(tmp)
        (root / "fixture.torrent").write_bytes(b"d8:announce%d:" % len(url) + url + b"4:info" + info + b"e")
        host, port = bootstrap.getsockname()
        env = {**os.environ, "MOVIAN_TEST_DHT_HOST": host, "MOVIAN_TEST_DHT_PORT": str(port)}
        subprocess.run([binary, "0", "0", "private-dht"], cwd=root, timeout=5, check=True, env=env)
        bootstrap.setblocking(False)
        try:
            bootstrap.recvfrom(2048)
        except BlockingIOError:
            print("Private torrent used its tracker without contacting DHT", flush=True)
        else:
            raise AssertionError("Private torrent contacted DHT")


def run(binary, small, use_udp=False, use_dht=False):
    url = b"http://tracker.invalid/announce"
    count = 8 if small else PIECES
    hashes = b"".join(hashlib.sha1(piece(i)).digest() for i in range(count))
    info = (b"d6:lengthi%de4:name11:fixture.mkv12:piece lengthi%de6:pieces%d:" %
            (count * SPAN, SPAN, len(hashes))) + hashes + b"e"
    identity = hashlib.sha1(info).digest()
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        announce = b"" if use_dht else b"8:announce%d:" % len(url) + url
        (root / "fixture.torrent").write_bytes(b"d" + announce + b"4:info" + info + b"e")
        tracker = None
        if not small and not use_dht:
            config = root / "data" / "movian" / "trackers.txt"
            config.parent.mkdir(parents=True)
            retracker = "http://retracker.invalid/announce"
            if use_udp:
                tracker = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                tracker.bind(("127.0.0.1", 0))
                retracker = f"udp://127.0.0.1:{tracker.getsockname()[1]}/announce"
            config.write_text(f"http://tracker.invalid/announce\n{retracker}\n")
            bundled = root / "app0" / "assets" / "trackers.txt"
            bundled.parent.mkdir(parents=True)
            bundled.write_bytes((pathlib.Path(__file__).resolve().parents[1] /
                                 "app" / "assets" / "trackers.txt").read_bytes())
        before = set(root.rglob("*"))
        servers = [socket.create_server(("127.0.0.1", 0)) for _ in range(1 if small else 2)]
        ports = [str(server.getsockname()[1]) for server in servers]
        served = [set(), set()]
        workers = [threading.Thread(target=serve, args=(s, None if small else i, count, identity, served[i]), daemon=True)
                   for i, s in enumerate(servers)]
        stalled = []
        if tracker:
            announced = threading.Event()
            stalled = [socket.create_server(("127.0.0.1", 0)) for _ in range(2)]
            workers.extend(threading.Thread(target=stall_handshake, args=(server, announced), daemon=True)
                           for server in stalled)
            stalled_ports = [str(server.getsockname()[1]) for server in stalled]
            workers.append(threading.Thread(target=udp_tracker,
                                            args=(tracker, identity, ports, count * SPAN, announced),
                                            daemon=True))
        env = os.environ.copy()
        if use_dht:
            bootstrap = dht_socket()
            host, port = bootstrap.getsockname()
            env["MOVIAN_TEST_DHT_HOST"] = host
            env["MOVIAN_TEST_DHT_PORT"] = str(port)
            workers.append(threading.Thread(target=dht_bootstrap,
                                            args=(bootstrap, identity, ports), daemon=True))
        for worker in workers:
            worker.start()
        command = [binary, ports[0], "0", "small"] if small else [binary, *ports]
        if use_udp:
            command.extend(stalled_ports)
        if use_dht:
            command.append("dht")
        subprocess.run(command, cwd=root, timeout=25, check=True, env=env)
        for worker in workers:
            worker.join(timeout=3)
        if tracker:
            assert announced.is_set(), "UDP discovery waited for the busy peer slots"
        assert set(root.rglob("*")) == before
        if small:
            assert served[0] == set(range(count))
        else:
            assert 398 in served[0] and 399 in served[1]


if __name__ == "__main__":
    run(sys.argv[1], False)
    run(sys.argv[1], False, use_udp=True)
    run(sys.argv[1], False, use_dht=True)
    verify_private_dht(sys.argv[1])
    run(sys.argv[1], True)
