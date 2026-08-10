#!/usr/bin/env python3
"""Capture the BUSY Bar's screens and render them as a PNG.

Triggers the on-device `screenshot` CLI command, fetches the raw frame buffers
over the storage API and composes both displays into one image, so on-device UI
work can be inspected from a workstation instead of guessed at.

    ./scripts/devscreen.py out.png
    ./scripts/devscreen.py out.png --key InputKeyUp   # inject, then capture
"""
import argparse
import socket
import struct
import sys
import time
import urllib.request
import zlib

DEVICE = "10.0.4.20"
API_HEADERS = {"X-API-Sem-Ver": "25.0.0"}

FRONT = {"path": "/ext/screen_front.raw", "w": 72, "h": 16, "bpp": 24}
BACK = {"path": "/ext/screen_back.raw", "w": 160, "h": 80, "bpp": 8}

# The back display is physically small; scale both up so details are legible.
FRONT_SCALE = 6
BACK_SCALE = 3
MARGIN = 8
GAP = 10


def cli(commands, settle=1.0):
    """Run CLI commands over the device's telnet-style port and return output."""
    sock = socket.create_connection((DEVICE, 23), timeout=10)
    sock.settimeout(1.5)
    out = b""
    time.sleep(0.8)
    try:
        while True:
            out += sock.recv(4096)
    except Exception:
        pass
    for command in commands:
        sock.sendall((command + "\r\n").encode())
        time.sleep(settle)
        try:
            while True:
                chunk = sock.recv(8192)
                if not chunk:
                    break
                out += chunk
        except Exception:
            pass
    sock.close()
    return out.decode(errors="replace")


def fetch(path):
    request = urllib.request.Request(
        f"http://{DEVICE}/api/storage/read?path={path}", headers=API_HEADERS)
    with urllib.request.urlopen(request, timeout=15) as response:
        return response.read()


def decode(raw, spec):
    """Return rows of (r, g, b) for one display."""
    width, height = spec["w"], spec["h"]
    rows = []
    for y in range(height):
        row = []
        for x in range(width):
            if spec["bpp"] == 24:
                i = (y * width + x) * 3
                row.append((raw[i], raw[i + 1], raw[i + 2]) if i + 2 < len(raw) else (0, 0, 0))
            else:
                i = y * width + x
                value = raw[i] if i < len(raw) else 0
                row.append((value, value, value))
        rows.append(row)
    return rows


def blit(canvas, rows, origin_x, origin_y, scale):
    for y, row in enumerate(rows):
        for x, pixel in enumerate(row):
            for dy in range(scale):
                for dx in range(scale):
                    canvas[origin_y + y * scale + dy][origin_x + x * scale + dx] = pixel


def write_png(path, canvas):
    height = len(canvas)
    width = len(canvas[0])
    raw = b"".join(
        b"\x00" + b"".join(struct.pack("BBB", *px) for px in row) for row in canvas)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as handle:
        handle.write(png)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", default="screen.png")
    parser.add_argument("--key", action="append", default=[],
                        help="Inject a key (e.g. InputKeyUp) before capturing")
    parser.add_argument("--type", default="InputTypeShort")
    parser.add_argument("--open", dest="app",
                        help="Launch an application by name before capturing")
    args = parser.parse_args()

    commands = []
    if args.app:
        commands.append(f"loader open {args.app}")
    for key in args.key:
        commands.append(f"input send {key} {args.type}")
    commands.append("screenshot")

    output = cli(commands)
    if "Error!" in output:
        print(output, file=sys.stderr)
        return 1

    front = decode(fetch(FRONT["path"]), FRONT)
    back = decode(fetch(BACK["path"]), BACK)

    front_w, front_h = FRONT["w"] * FRONT_SCALE, FRONT["h"] * FRONT_SCALE
    back_w, back_h = BACK["w"] * BACK_SCALE, BACK["h"] * BACK_SCALE

    width = MARGIN * 2 + max(front_w, back_w)
    height = MARGIN * 2 + front_h + GAP + back_h
    canvas = [[(24, 24, 24) for _ in range(width)] for _ in range(height)]

    blit(canvas, front, MARGIN, MARGIN, FRONT_SCALE)
    blit(canvas, back, MARGIN, MARGIN + front_h + GAP, BACK_SCALE)

    write_png(args.output, canvas)
    print(f"wrote {args.output} ({width}x{height})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
