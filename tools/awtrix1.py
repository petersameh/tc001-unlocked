#!/usr/bin/env python3
"""Helpers to build AWTRIX 1 binary commands and queue them to mini_broker.py.

Usage as CLI:  python3 tools/awtrix1.py <test-name>
"""
import os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
CMD_FILE = os.path.join(HERE, "..", "recon", "broker.cmd")
TOPIC = "awtrixmatrix/"


def u16(v): return int(v).to_bytes(2, "big", signed=False)
def rgb(c): return bytes(c)

def text(x, y, color, s): return b"\x00" + u16(x) + u16(y) + rgb(color) + s.encode("latin-1")
def bmp(x, y, w, h, pixels565): return b"\x01" + u16(x) + u16(y) + bytes([w, h]) + b"".join(u16(p) for p in pixels565)
def circle(x, y, r, color): return b"\x02" + u16(x) + u16(y) + bytes([r]) + rgb(color)
def fill_circle(x, y, r, color): return b"\x03" + u16(x) + u16(y) + bytes([r]) + rgb(color)
def pixel(x, y, color): return b"\x04" + u16(x) + u16(y) + rgb(color)
def rect(x, y, w, h, color): return b"\x05" + u16(x) + u16(y) + bytes([w, h]) + rgb(color)
def line(x0, y0, x1, y1, color): return b"\x06" + u16(x0) + u16(y0) + u16(x1) + u16(y1) + rgb(color)
def fill(color): return b"\x07" + rgb(color)
def show(): return b"\x08"
def clear(): return b"\x09"

CHAR_W = 4  # measured on device: "1234" at x=0 spans columns 0-15
SCREEN_W, SCREEN_H = 32, 8

def text_width(s): return len(s) * CHAR_W

def icon_and_text(icon_px, icon_w, icon_h, s, color, gap=2):
    """Centre an icon + text group horizontally on the 32x8 screen."""
    total = icon_w + gap + text_width(s)
    x0 = max(0, (SCREEN_W - total) // 2)
    return [bmp(x0, 0, icon_w, icon_h, icon_px), text(x0 + icon_w + gap, 1, color, s)]

def rgb565(r, g, b): return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def send(*payloads):
    with open(CMD_FILE, "a") as f:
        for p in payloads:
            f.write(f"{TOPIC} {p.hex()}\n")


RED, GREEN, BLUE, WHITE, YELLOW = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255), (255, 255, 0)

TESTS = {
    # one composite frame: corners + rect + line + circle + filled circle
    "shapes": lambda: send(
        clear(),
        pixel(0, 0, WHITE), pixel(31, 7, WHITE),
        rect(2, 0, 6, 8, RED),
        line(9, 0, 14, 7, BLUE),
        circle(18, 3, 3, GREEN),
        fill_circle(26, 3, 3, YELLOW),
        show(),
    ),
    # 8x8 bitmap: red/white YouTube-ish play button at x=0, plus text
    "bmp": lambda: send(clear(), *icon_and_text(youtube_icon(), 7, 7, "1234", WHITE), show()),
    # scrolling text: move x from 32 to -40, re-sending each frame
    "scroll": lambda: scroll("SCROLLING TEXT", GREEN),
}


def youtube_icon():
    R, W, K = rgb565(255, 0, 0), rgb565(255, 255, 255), 0
    rows = [
        ".RRRRR.",
        "RRWRRRR",
        "RRWWRRR",
        "RRWWWRR",
        "RRWWRRR",
        "RRWRRRR",
        ".RRRRR.",
    ]
    m = {".": K, "R": R, "W": W}
    return [m[c] for row in rows for c in row]


def scroll(s, color, delay=0.05):
    for x in range(32, -len(s) * 4 - 1, -1):
        send(clear(), text(x, 1, color, s), show())
        time.sleep(delay)


if __name__ == "__main__":
    TESTS[sys.argv[1]]()
