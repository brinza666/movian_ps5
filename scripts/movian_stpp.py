#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""STPP v3 inspection/control client, reconstructed from shipped M7 and wire evidence.

Install websocket-client==1.8.0 for network use. Codec tests need only Python.
Defaults to observing app/systemname; mutations require explicit CLI arguments.
See docs/REMOTE_CONTROL_STPP.md for evidence, scope and PS5 integration status.
"""
import argparse
import json
import struct
import time

MAX_MESSAGE = 1024 * 1024


class ProtocolError(ValueError):
    pass


def u32(value):
    if not isinstance(value, int) or not 0 <= value <= 0xffffffff:
        raise ProtocolError('Expected an unsigned 32-bit reference')
    return struct.pack('<I', value)


def path_bytes(path, terminate=True):
    if len(path) > 64:
        raise ProtocolError('Property path is too deep')
    result = bytearray()
    for part in path:
        value = part.encode('utf-8')
        if not value or len(value) > 255 or b'\0' in value:
            raise ProtocolError('Invalid property path component')
        result += bytes([len(value)]) + value
    return bytes(result) + (b'\0' if terminate else b'')


def address(base, path):
    return u32(base) + path_bytes(path)


def event_string(text):
    value = text.encode('utf-8')
    if len(value) > MAX_MESSAGE - 1024:
        raise ProtocolError('Event string is too large')
    return (bytes([len(value)]) if len(value) < 255 else b'\xff' + u32(len(value))) + value


def subscribe_message(subscription, base, path, flags=0):
    if not 0 <= flags <= 0xffff:
        raise ProtocolError('Invalid subscription flags')
    # Native M7 omits a path terminator for this standalone path.
    return b'\x01' + u32(subscription) + u32(base) + struct.pack('<H', flags) + path_bytes(path, False)


def set_int_message(base, path, value):
    if not isinstance(value, int) or not -0x80000000 <= value <= 0x7fffffff:
        raise ProtocolError('Expected a signed 32-bit value')
    return b'\x03' + address(base, path) + b'\x01' + struct.pack('<i', value)


def action_message(base, path, names):
    values = [name.encode('utf-8') for name in names]
    if not values or any(not value or len(value) > 255 for value in values):
        raise ProtocolError('Invalid action name')
    return b'\x05' + address(base, path) + b'\x01' + b''.join(bytes([len(v)]) + v for v in values)


def open_url_message(url, base=0, path=('navigators', 'current', 'eventSink')):
    return b'\x05' + address(base, path) + b'\x0e\x01' + event_string(url)


def decode_notification(data):
    if len(data) < 6 or len(data) > MAX_MESSAGE or data[0] != 4:
        raise ProtocolError('Invalid STPP notification')
    kind = data[1]
    payload = data[6:]
    result = {'kind': kind, 'subscription': struct.unpack_from('<I', data, 2)[0]}
    if kind in (0, 5, 14, 15):
        if payload:
            raise ProtocolError('Unexpected payload on empty notification')
        if kind == 0:
            result['value'] = None
        elif kind == 5:
            result['directory'] = True
    elif kind in (1, 2):
        if len(payload) != 4:
            raise ProtocolError('Invalid scalar notification')
        result['value'] = struct.unpack('<i' if kind == 1 else '<f', payload)[0]
    elif kind == 3:
        if not payload:
            raise ProtocolError('Missing string kind')
        result.update(string_kind=payload[0], value=payload[1:].decode('utf-8', 'replace'))
    elif kind in (6, 7, 8, 9, 10, 11, 12):
        if len(payload) % 4 or len(payload) > 32768:
            raise ProtocolError('Invalid property reference vector')
        refs = list(struct.unpack('<' + 'I' * (len(payload) // 4), payload))
        if (kind in (8, 10, 11, 12) and len(refs) != 1 or
                kind == 9 and len(refs) not in (1, 2) or kind == 7 and not refs):
            raise ProtocolError('Invalid property reference count')
        result['refs'] = refs
    else:
        result['unknown_payload_hex'] = payload.hex()
    return result


class Client:
    def __init__(self, url, timeout=3):
        import websocket
        if not url.startswith(('ws://', 'wss://')):
            raise ProtocolError('Expected an explicit WebSocket endpoint')
        self.socket = websocket.create_connection(url, timeout=timeout)
        try:
            self.socket.send_binary(b'\x00\x03\x00')
            hello = self.socket.recv()
            if not isinstance(hello, bytes) or len(hello) != 19 or hello[:2] != b'\x00\x03':
                raise ProtocolError('Peer did not negotiate STPP version 3')
        except Exception:
            self.socket.close()
            raise
        self.instance_id = hello[2:18]
        self.serial = 0
        self.values = {}
        self.children = {}
        self.selected = {}

    def send(self, payload):
        if len(payload) > MAX_MESSAGE:
            raise ProtocolError('Message is too large')
        self.socket.send_binary(payload)

    def subscribe(self, path, base=0, flags=0):
        self.serial += 1
        self.send(subscribe_message(self.serial, base, path, flags))
        return self.serial

    def unsubscribe(self, subscription):
        self.send(b'\x02' + u32(subscription))
        self.values.pop(subscription, None)
        self.children.pop(subscription, None)
        self.selected.pop(subscription, None)

    def open(self, url):
        self.send(open_url_message(url))

    def action(self, name, base=0, path=('navigators', 'current', 'eventSink')):
        self.send(action_message(base, path, [name]))

    def set_int(self, path, value, base=0):
        self.send(set_int_message(base, path, value))

    def receive(self, seconds=1):
        import websocket
        deadline = time.monotonic() + seconds
        events = []
        while time.monotonic() < deadline:
            self.socket.settimeout(max(.01, deadline - time.monotonic()))
            try:
                data = self.socket.recv()
            except websocket.WebSocketTimeoutException:
                break
            if not data:
                raise ProtocolError('Peer closed the connection')
            if not isinstance(data, bytes) or data[0] != 4:
                continue
            event = decode_notification(data)
            events.append(event)
            sid, kind = event['subscription'], event['kind']
            if kind == 5:
                self.children[sid] = []
            elif kind in (6, 7, 11):
                refs = list(event['refs'])
                children = self.children.setdefault(sid, [])
                before = refs.pop(0) if kind == 7 else 0
                at = children.index(before) if before in children else len(children)
                for ref in refs:
                    if ref not in children:
                        children.insert(at, ref)
                        at += 1
                if kind == 11:
                    self.selected[sid] = refs[0]
            elif kind == 8:
                children = self.children.setdefault(sid, [])
                if event['refs'][0] in children:
                    children.remove(event['refs'][0])
            elif kind == 9:
                children = self.children.setdefault(sid, [])
                ref = event['refs'][0]
                if ref in children:
                    children.remove(ref)
                    before = event['refs'][1] if len(event['refs']) > 1 else 0
                    children.insert(children.index(before) if before in children else len(children), ref)
            elif kind == 10:
                self.selected[sid] = event['refs'][0]
            elif 'value' in event:
                self.values[sid] = event['value']
        return events

    def close(self):
        self.socket.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', required=True, help='Explicit ws://host:port/api/stpp endpoint')
    parser.add_argument('--path', default='["app","systemname"]', help='Exact property path as a JSON array')
    parser.add_argument('--seconds', type=float, default=2)
    parser.add_argument('--open-url')
    parser.add_argument('--action')
    parser.add_argument('--set-int', type=int)
    args = parser.parse_args()
    path = json.loads(args.path)
    if not isinstance(path, list) or not all(isinstance(p, str) for p in path):
        parser.error('--path must be a JSON string array')
    if not 0 < args.seconds <= 120:
        parser.error('--seconds must be between 0 and 120')
    with Client(args.url) as client:
        sid = client.subscribe(path)
        if args.open_url:
            client.open(args.open_url)
        if args.action:
            client.action(args.action)
        if args.set_int is not None:
            client.set_int(path, args.set_int)
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            for event in client.receive(min(.25, max(.01, deadline - time.monotonic()))):
                if event['subscription'] == sid:
                    print(json.dumps(event, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
