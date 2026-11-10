#!/usr/bin/env python3
"""Generate a 512x512 RGBA PNG launcher icon without third-party packages."""
from pathlib import Path
import struct
import zlib


def chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def main() -> None:
    w = h = 512
    rows = []
    for y in range(h):
        row = bytearray([0])
        for x in range(w):
            r, g, b = 11, 13, 16
            dx, dy = x - 256, y - 256
            d2 = dx * dx + dy * dy
            if d2 < 170 * 170:
                r, g, b = 26, 21, 18
            if 150 * 150 < d2 < 170 * 170:
                r, g, b = 142, 162, 255
            if 180 <= y <= 340:
                if 180 <= x <= 210 or 300 <= x <= 330:
                    r, g, b = 241, 236, 233
                if 210 <= x <= 300:
                    t = abs((x - 255) / 45.0)
                    y0 = 180 + int(t * 80)
                    if y0 <= y <= y0 + 30:
                        r, g, b = 241, 236, 233
            row += bytes((r, g, b, 255))
        rows.append(bytes(row))
    raw = b"".join(rows)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )
    out = Path(__file__).resolve().parents[1] / "app" / "sce_sys" / "icon0.png"
    out.write_bytes(png)
    print(out, out.stat().st_size)


if __name__ == "__main__":
    main()
