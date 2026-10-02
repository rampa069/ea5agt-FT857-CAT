#!/usr/bin/env python3
"""Convierte PPM (P6) a PNG escalado sin dependencias: ppm2png.py entrada.ppm salida.png [escala]"""
import struct
import sys
import zlib


def main(src, dst, scale=2):
    with open(src, "rb") as f:
        assert f.readline().strip() == b"P6"
        w, h = map(int, f.readline().split())
        f.readline()
        data = f.read()
    rows = []
    for y in range(h):
        row = data[y * w * 3:(y + 1) * w * 3]
        wide = b"".join(row[x * 3:x * 3 + 3] * scale for x in range(w))
        rows.extend([b"\x00" + wide] * scale)

    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload))

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
    with open(dst, "wb") as f:
        f.write(png)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 2)
