# Authoring aid: composes the 32x32 Place icons from pixel primitives, previews them,
# and prints palette-indexed string maps for Source/UI/PixelArt.cpp.
from PIL import Image
import math, sys
PAL = {
 'K': (34,22,30), 'w': (255,250,232),
 'A': (196,190,178), 'a': (150,142,136), 'b': (98,92,96),
 'R': (214,72,64), 'r': (146,40,52),
 'Y': (255,214,92), 'y': (236,140,52),
 'F': (90,140,236), 'f': (56,84,180),
 'G': (110,190,80), 'g': (60,128,64),
 'W': (196,132,78), 'V': (128,78,50),
 'L': (132,206,92), 'l': (70,156,74), 'm': (38,96,64),
 'T': (112,70,48),
 'M': (176,188,206), 'N': (110,122,146), 'n': (62,70,96),
 'C': (96,240,255), 'c': (32,150,196),
 'P': (255,92,214), 'p': (168,48,160),
 'U': (122,74,200), 'u': (66,42,128),
 'S': (255,255,255),
 'O': (255,170,60), 'o': (230,84,40),
 'Z': (220,255,120),
 'B': (40,30,80), 'Q': (255,150,200),
 'H': (196,156,255), 'h': (124,92,196),
 'E': (88,212,196), 'e': (40,138,140),
 'X': (255,238,206), 'D': (238,204,140), 'd': (190,146,92),
 'I': (150,214,255), 'i': (60,124,206),
 'J': (178,86,64), 'j': (120,52,44),
 'k': (44,48,56), 'v': (150,200,120),
}
class G:
    def __init__(s, w=32, h=32): s.w, s.h = w, h; s.p = [['.']*w for _ in range(h)]
    def set(s, x, y, c):
        if 0 <= x < s.w and 0 <= y < s.h: s.p[y][x] = c
    def get(s, x, y): return s.p[y][x] if 0 <= x < s.w and 0 <= y < s.h else '.'
    def rect(s, x, y, w, h, c):
        for j in range(y, y+h):
            for i in range(x, x+w): s.set(i, j, c)
    def hline(s, x0, x1, y, c):
        for i in range(x0, x1+1): s.set(i, y, c)
    def vline(s, x, y0, y1, c):
        for j in range(y0, y1+1): s.set(x, j, c)
    def disc(s, cx, cy, r, c, ry=None):
        ry = ry or r
        for j in range(s.h):
            for i in range(s.w):
                if ((i+.5-cx)/r)**2 + ((j+.5-cy)/ry)**2 <= 1: s.set(i, j, c)
    def tri(s, apex_x, top, base_y, half, c):   # isosceles, apex (apex_x..apex_x+1 for even)
        for j in range(top, base_y+1):
            t = (j-top)/max(1, base_y-top); hw = t*half
            for i in range(int(math.floor(apex_x-hw)), int(math.ceil(apex_x+hw))):
                s.set(i, j, c)
    def outline(s, c='K'):
        # Every transparent pixel touching a filled one (4-neighbour) becomes outline.
        add = []
        for j in range(s.h):
            for i in range(s.w):
                if s.p[j][i] == '.' and any(s.get(i+dx, j+dy) not in '.K' for dx, dy in ((1,0),(-1,0),(0,1),(0,-1))): add.append((i, j))
        for i, j in add: s.set(i, j, c)
    def poly(s, pts, c):
        # Fills a polygon (pixel centres inside, even-odd).
        n = len(pts)
        for j in range(s.h):
            for i in range(s.w):
                x, y = i + .5, j + .5
                inside = False
                for k in range(n):
                    x0, y0 = pts[k]; x1, y1 = pts[(k + 1) % n]
                    if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0): inside = not inside
                if inside: s.set(i, j, c)
    def ring(s, cx, cy, r0, r1, c, ry=1.0):
        for j in range(s.h):
            for i in range(s.w):
                d = ((i+.5-cx)**2 + ((j+.5-cy)/ry)**2) ** .5
                if r0 <= d <= r1: s.set(i, j, c)
    def put(s, x, y, rows):
        for j, r in enumerate(rows):
            for i, c in enumerate(r):
                if c != ' ': s.set(x + i, y + j, c)
    def line(s, x0, y0, x1, y1, c):
        dx, sx, dy, sy = abs(x1-x0), (1 if x0 < x1 else -1), -abs(y1-y0), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            s.set(x0, y0, c)
            if x0 == x1 and y0 == y1: break
            e2 = 2 * err
            if e2 >= dy: err += dy; x0 += sx
            if e2 <= dx: err += dx; y0 += sy
    def copy(s):
        g = G(s.w, s.h); g.p = [r[:] for r in s.p]; return g
    def rows(s): return [''.join(r) for r in s.p]
    def png(s, name, k=8):
        im = Image.new('RGBA', (s.w*k, s.h*k), (24,20,34,255))
        for j in range(s.h):
            for i in range(s.w):
                c = s.p[j][i]
                if c != '.':
                    for y in range(j*k, j*k+k):
                        for x in range(i*k, i*k+k): im.putpixel((x, y), PAL[c] + (255,))
        im.save(name)

def diff(base, frame):
    # The pixels a frame changes, as (x, y, char) runs per row: "y:x:chars".
    out = []
    for y in range(base.h):
        x = 0
        while x < base.w:
            if base.p[y][x] != frame.p[y][x]:
                x0 = x; run = ''
                while x < base.w and base.p[y][x] != frame.p[y][x]: run += frame.p[y][x]; x += 1
                out.append((y, x0, run))
            else: x += 1
    return out

icons = {}
