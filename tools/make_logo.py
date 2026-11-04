#!/usr/bin/env python3
"""Build 32x8 versions of the MR.PS. logo for the TC001 drawing screen.

  A: straight downscale of the PNG (for comparison)
  B: pixel-art rebuild: white 3x5 letters, black outline, red circle behind, red dot after R
Writes hex frames (1536 chars, row-major rrggbb) next to this script's output dir.
"""
import sys
from PIL import Image

SRC = sys.argv[1]
OUT = sys.argv[2]  # output dir
W, H = 32, 8

RED, WHITE, BLACK = (215, 0, 0), (255, 255, 255), (0, 0, 0)

# 3x5 glyphs matching the firmware font (row bits: 4=left, 2=mid, 1=right)
GLYPHS = {
    'M': [5, 7, 7, 5, 5], 'R': [6, 5, 6, 5, 5], 'P': [6, 5, 6, 4, 4], 'S': [3, 4, 2, 1, 6], '.': [0, 0, 0, 0, 2],
}


def to_hex(px):
    return ''.join('%02x%02x%02x' % px[y][x] for y in range(H) for x in range(W))


def ascii_preview(px):
    def ch(c):
        if c == BLACK: return '.'
        if c[0] > 200 and c[1] > 200: return 'W'
        if c[0] > 120 and c[1] < 90: return 'R'
        return '+'
    return '\n'.join(''.join(ch(px[y][x]) for x in range(W)) for y in range(H))


def version_a():
    img = Image.open(SRC).convert('RGBA')
    bg = Image.new('RGBA', img.size, (0, 0, 0, 255))   # transparent -> off
    img = Image.alpha_composite(bg, img).convert('RGB')
    w = round(img.width * H / img.height)
    small = img.resize((w, H), Image.LANCZOS)
    px = [[BLACK] * W for _ in range(H)]
    x0 = (W - w) // 2
    for y in range(H):
        for x in range(w):
            r, g, b = small.getpixel((x, y))
            # snap to logo palette so the LEDs don't show muddy colours
            if r > 170 and g > 170 and b > 170: c = WHITE
            elif r > 120 and g < 90 and b < 90: c = RED
            else: c = BLACK
            px[y][x0 + x] = c
    return px


def version_b():
    text = [('M', WHITE), ('R', WHITE), ('.', RED), ('P', WHITE), ('S', WHITE), ('.', WHITE)]
    widths = [1 if c == '.' else 3 for c, _ in text]
    total = sum(widths) + len(text) - 1
    x0 = (W - total) // 2
    letters = {}  # (x,y) -> colour
    x = x0
    for (c, col), w in zip(text, widths):
        for r, bits in enumerate(GLYPHS[c]):
            if w == 1:
                if bits & 2: letters[(x, 1 + r)] = col
            else:
                for i, m in enumerate((4, 2, 1)):
                    if bits & m: letters[(x + i, 1 + r)] = col
        x += w + 1
    # red circle behind, narrower than the word (as in the logo: the word overhangs the disc)
    cx, cy, rx, ry = W / 2 - 0.5, 3.5, 6.5, 4.4
    px = [[BLACK] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                px[y][x] = RED
    # thick black band behind the word (the logo's black outline), 1 px wider than the text
    xs = [x for x, _ in letters]
    for y in range(1, 6):
        for x in range(min(xs) - 1, max(xs) + 2):
            px[y][x] = BLACK
    for (lx, ly), col in letters.items():
        px[ly][lx] = col
    return px


for name, fn in (('A', version_a), ('B', version_b)):
    px = fn()
    open(f'{OUT}/mrps_{name}.hex', 'w').write(to_hex(px))
    print(f'--- version {name} ---')
    print(ascii_preview(px))
