#!/usr/bin/env python3
"""Hand-designed 32x8 pixel-art MR.PS. mark. Writes assets/mrps_pixel.png/.hex and a 12x preview."""
import sys
from PIL import Image
OUT = sys.argv[1] if len(sys.argv) > 1 else "assets"
W, H = 32, 8
RED, WHITE, BLACK = (220, 0, 0), (255, 255, 255), (0, 0, 0)
G = {  # 6-row bold capitals, '#' = on
 'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#"],
 'R': ["###.", "#..#", "###.", "#.#.", "#..#", "#..#"],
 'P': ["###.", "#..#", "#..#", "###.", "#...", "#..."],
 'S': [".###", "#...", ".##.", "...#", "...#", "###."],
}
DOT = ["##", "##"]
# layout: M R (red dot low) P S (white dot mid)
items = [('M', 0), ('R', 0), ('rdot', 0), ('P', 0), ('S', 0), ('wdot', 0)]
def width(k): return 2 if k.endswith('dot') else len(G[k][0])
total = sum(width(k) for k, _ in items) + len(items) - 1
x0 = (W - total) // 2
px = [[BLACK] * W for _ in range(H)]
# red disc behind the middle of the word
cx, cy, rx, ry = W / 2 - 0.5, 3.5, 8.6, 4.6
for y in range(H):
    for x in range(W):
        if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0: px[y][x] = RED
x = x0
for k, _ in items:
    w = width(k)
    if k in G:
        for r, row in enumerate(G[k]):
            for i, c in enumerate(row):
                if c == '#': px[1 + r][x + i] = WHITE
    elif k == 'rdot':                       # red dot at the baseline, ringed in black
        for yy in range(4, 8):
            for xx in range(x - 1, x + 3):
                if 0 <= xx < W: px[yy][xx] = BLACK
        for yy in (5, 6):
            for xx in (x, x + 1): px[yy][xx] = RED
    elif k == 'wdot':                       # big white dot at mid-height, like the logo
        for yy in (3, 4):
            for xx in (x, x + 1): px[yy][xx] = WHITE
    x += w + 1
img = Image.new('RGB', (W, H))
for y in range(H):
    for xx in range(W): img.putpixel((xx, y), px[y][xx])
img.save(f'{OUT}/mrps_pixel.png')
img.resize((W * 12, H * 12), Image.NEAREST).save(f'{OUT}/mrps_pixel_preview.png')
open(f'{OUT}/mrps_pixel.hex', 'w').write(''.join('%02x%02x%02x' % px[y][xx] for y in range(H) for xx in range(W)))
for y in range(H):
    print(''.join('.' if px[y][xx] == BLACK else 'W' if px[y][xx] == WHITE else 'R' for xx in range(W)))
