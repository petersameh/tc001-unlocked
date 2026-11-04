#!/usr/bin/env python3
"""Pixel-exact ASCII mockup of a 2-line compact dashboard with a 3x4 proportional font."""
F = {  # glyph: (width, rows top->bottom as bit strings)
 '0':(3,['111','101','101','111']), '1':(1,['1','1','1','1']),
 '2':(3,['111','011','100','111']), '3':(3,['111','011','001','111']),
 '4':(3,['101','101','111','001']), '5':(3,['111','110','001','111']),
 '6':(3,['100','111','101','111']), '7':(3,['111','001','010','010']),
 '8':(3,['111','111','101','111']), '9':(3,['111','101','111','001']),
 '.':(1,['0','0','0','1']), ':':(1,['0','1','0','1']), '°':(2,['11','11','00','00']),
 'K':(3,['101','110','110','101']), '-':(3,['000','111','000','000']),
}
def width(s): return sum(F[c][0] for c in s) + len(s) - 1
def draw(buf, x, y, s, col):
    for c in s:
        w, rows = F[c]
        for r, bits in enumerate(rows):
            for i, b in enumerate(bits):
                if b == '1' and 0 <= x+i < 32: buf[y+r][x+i] = col
        x += w + 1
def battery(buf, x, y, pct):
    for i in range(5): buf[y][x+i] = buf[y+3][x+i] = 'w'
    for j in range(4): buf[y+j][x] = buf[y+j][x+4] = 'w'
    buf[y+1][x+5] = buf[y+2][x+5] = 'w'
    for i in range((pct*3+50)//100):
        buf[y+1][x+1+i] = buf[y+2][x+1+i] = 'G'
def frame(clock, temp, price, subs, pct, up=True):
    b = [['.']*32 for _ in range(8)]
    draw(b, 0, 0, clock, 'W')                         # top-left: clock
    draw(b, 24 - width(temp), 0, temp, 'Y')           # top-middle-right: weather temp
    battery(b, 26, 0, pct)                            # top-right: battery
    draw(b, 0, 4, price, 'G' if up else 'R')          # bottom-left: stock price (green up / red down)
    draw(b, 32 - width(subs), 4, subs, 'w')           # bottom-right: YouTube subs
    print('    ' + ''.join(str(i % 10) for i in range(32)))
    for y, row in enumerate(b): print(f'  {y} ' + ''.join(row))
print("Current values:  16:22 | 31° | battery 79% / COMI 126.5 (up) | 10100 subs\n")
frame('16:22', '31°', '126.5', '10100', 79)
print("\nWorst-ish case:  20:58 | 28° | battery 30% / COMI 98.35 (down) | 10.2K subs\n")
frame('20:58', '28°', '98.35', '10.2K', 30, up=False)
