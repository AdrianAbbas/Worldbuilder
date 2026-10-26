# All 27 Place icons (13b). Fantasy and nature: storybook whimsy. Modern: clean and
# neutral. Sci-fi: crisp neon.
from icons import *
import designs, math, random

def anim(base, n, fn, outline_first=True):
    frames = []
    for k in range(n):
        f = base.copy()
        if outline_first: f.outline()
        fn(f, k)
        frames.append(f)
    return frames

def hill(g, colour='G', dark='g', top=28):
    for i in range(32):
        hgt = top + int(1.5 * math.cos((i - 15.5) / 6.0))
        for j in range(hgt, 32): g.set(i, j, colour if j < 30 else dark)

# ---------------- fantasy (storybook) ----------------
def castle():
    g = G()
    hill(g, 'G', 'g', 28)
    for i in range(1, 31, 4): g.set(i, 29, 'v')
    for cx in (6, 25):                     # round towers with candy cones
        g.rect(cx - 3, 13, 7, 15, 'X'); g.vline(cx + 3, 13, 27, 'D')
        g.tri(cx + 0.5, 4, 12, 5, 'Q')
        for j in range(6, 13): g.set(cx + 1 + (j - 4) * 4 // 8, j, 'H')
        g.hline(cx - 4, cx + 4, 12, 'h')
        g.disc(cx + 0.5, 18, 1.6, 'Y'); g.set(cx, 17, 'w')   # round window
    g.rect(10, 14, 12, 14, 'X'); g.rect(20, 14, 2, 14, 'D')
    for i in range(10, 22, 3): g.rect(i, 12, 2, 2, 'X')
    g.disc(16, 24.5, 3.2, 'V'); g.rect(13, 24, 6, 4, 'V'); g.vline(16, 22, 27, 'T')   # arched door
    g.set(17, 25, 'Y')
    g.disc(13, 17, 1.2, 'Y'); g.disc(19, 17, 1.2, 'Y')
    g.tri(16, 5, 11, 3, 'H'); g.vline(16, 1, 4, 'V')     # lilac spire
    for (x, y) in ((4, 25), (27, 24), (12, 21)): g.set(x, y, 'Q')   # flowers
    base = g.copy()
    flags = [[(17,1,'EEE'),(17,2,'EEEE'),(17,3,'Ee')], [(17,1,'EE'),(17,2,'EEEE'),(17,3,'EEe')], [(17,1,'E'),(17,2,'EEE'),(17,3,'EEEe')]]
    def fn(f, k):
        for x, y, run in flags[k]:
            for i, c in enumerate(run): f.set(x + i, y, c)
        f.set(16, 0, 'Y')
        for (x, y) in ((2, 3), (29, 2), (9, 1)):
            if (x + k) % 3 == 0: f.set(x, y, 'Y')
            elif (x + k) % 3 == 1: f.set(x, y, 'w')
    frames = []
    for k in range(3):
        f = base.copy()
        for x, y, run in flags[k]:
            for i, c in enumerate(run): f.set(x + i, y, c)
        f.outline(); f.set(16, 0, 'Y')
        for n, (x, y) in enumerate(((2, 4), (29, 3), (9, 2))):
            if (n + k) % 3 == 0:
                for dx, dy in ((0,0),(1,0),(-1,0),(0,1),(0,-1)): f.set(x+dx, y+dy, 'Y' if dx==dy==0 else 'y')
            elif (n + k) % 3 == 1: f.set(x, y, 'w')
        frames.append(f)
    return frames

def village():
    g = G()
    hill(g, 'G', 'g', 27)
    # crooked cottage
    g.rect(3, 17, 12, 10, 'X'); g.vline(14, 17, 26, 'D')
    g.poly([(1, 18), (8, 9), (16, 17)], 'R'); g.poly([(8, 9), (16, 17), (13, 17)], 'r')
    g.disc(8, 23.5, 2.2, 'E'); g.rect(6, 23, 5, 4, 'E'); g.set(9, 24, 'Y')   # round door
    g.rect(11, 19, 2, 2, 'Y')
    g.rect(4, 19, 2, 2, 'Y')
    g.rect(11, 9, 2, 5, 'b')   # chimney
    # mushroom house
    g.rect(20, 19, 8, 8, 'X'); g.vline(27, 19, 26, 'D')
    g.disc(24, 17, 7, 'R', 5); 
    for j in range(22, 32): 
        for i in range(32):
            if g.get(i, j) == 'R' and j > 17: g.set(i, j, 'X')
    g.rect(16, 17, 16, 2, '.')
    g.disc(24, 16.5, 7, 'R', 5)
    for j in range(17, 22):
        for i in range(17, 32):
            if g.get(i, j) == 'R': g.set(i, j, '.')
    g.rect(20, 17, 8, 10, 'X'); g.vline(27, 17, 26, 'D')
    for (x, y) in ((21, 13), (25, 12), (27, 15), (19, 15)): g.set(x, y, 'w'); g.set(x + 1, y, 'w')
    g.rect(22, 21, 3, 6, 'V'); g.set(24, 23, 'Y')
    g.rect(25, 18, 2, 2, 'Y')
    # fence
    for x in range(0, 32, 3):
        if 15 < x < 20 or x < 2: g.vline(x, 25, 27, 'W')
    g.set(18, 29, 'Q'); g.set(2, 29, 'H'); g.set(29, 29, 'Q')
    base = g.copy(); base.outline()
    puffs = [((11, 7), (12, 5), (11, 2)), ((12, 7), (11, 4), (12, 1)), ((11, 6), (12, 3), (11, 0))]
    def fn(f, k):
        for n, (x, y) in enumerate(puffs[k]):
            f.set(x, y, 'A'); f.set(x + 1, y, 'a' if n else 'A')
            if n == 1: f.set(x, y - 1, 'A')
    return anim(base, 3, fn, False)

def temple():
    g = G()
    hill(g, 'G', 'g', 29)
    g.rect(4, 26, 24, 3, 'X'); g.hline(4, 27, 28, 'D'); g.rect(6, 24, 20, 2, 'X')   # steps
    for x in (7, 11, 15, 19, 23): g.rect(x, 15, 2, 9, 'X'); g.vline(x + 1, 15, 23, 'D')
    g.rect(5, 13, 22, 2, 'Y'); g.hline(5, 26, 14, 'y')
    g.disc(16, 13, 8, 'E', 7)
    for j in range(13, 32):
        for i in range(32):
            if g.get(i, j) == 'E' and j >= 13: g.set(i, j, '.')
    g.rect(5, 13, 22, 2, 'Y'); g.hline(5, 26, 14, 'y')
    for j in range(6, 13):
        for i in range(17, 24):
            if g.get(i, j) == 'E' and (i - 16) * 1.0 > (j - 5) * 0.3 + 3: g.set(i, j, 'e')
    g.set(12, 9, 'w'); g.set(13, 8, 'w')
    g.vline(16, 2, 5, 'Y'); g.disc(16.5, 2, 1.2, 'Y')
    # lanterns
    g.rect(3, 20, 2, 3, 'O'); g.rect(27, 20, 2, 3, 'O'); g.vline(4, 23, 25, 'V'); g.vline(28, 23, 25, 'V')
    base = g.copy(); base.outline()
    def fn(f, k):
        c = ['O', 'Y', 'y'][k]
        f.rect(3, 20, 2, 3, c); f.rect(27, 20, 2, 3, c)
        f.set(16, 1, ['w', 'Y', 'Y'][k])
        if k == 1:
            for x, y in ((15, 0), (17, 0), (14, 2), (18, 2)): f.set(x, y, 'w')
        if k == 2:
            f.set(4, 19, 'y'); f.set(28, 19, 'y')
    return anim(base, 3, fn, False)

def ruins():
    g = G()
    hill(g, 'G', 'g', 27)
    # broken arch
    g.rect(4, 10, 5, 17, 'A'); g.vline(8, 10, 26, 'a')
    g.rect(19, 15, 5, 12, 'A'); g.vline(23, 15, 26, 'a')
    g.ring(13.5, 11, 5, 9.5, 'A', 1.0)
    for j in range(0, 32):
        for i in range(32):
            if g.get(i, j) == 'A' and (j > 11 and 8 < i < 19): g.set(i, j, '.')
            if g.get(i, j) == 'A' and i > 15 and j < 10: g.set(i, j, '.')
    for (x, y) in ((5, 14), (6, 20), (21, 19), (7, 4), (10, 3)): g.set(x, y, 'a')
    # fallen block, column stump
    g.rect(25, 23, 5, 4, 'A'); g.hline(25, 29, 26, 'a')
    g.rect(13, 22, 4, 5, 'A'); g.vline(16, 22, 26, 'a')
    # ivy and flowers
    for (x, y) in ((4, 11), (4, 12), (5, 13), (4, 15), (5, 16), (4, 18), (8, 22), (7, 23), (19, 16), (20, 17), (19, 19), (3, 6), (2, 5)):
        g.set(x, y, 'l')
    for (x, y) in ((5, 12), (4, 17), (20, 18), (14, 22), (27, 23)): g.set(x, y, 'Q')
    base = g.copy(); base.outline()
    wisp = [(12, 16), (13, 14), (14, 13), (13, 15)]
    def fn(f, k):
        x, y = wisp[k]
        f.rect(x, y, 2, 2, 'I'); f.set(x, y, 'w'); f.set(x + 1, y + 2, 'i'); f.set(x - 1 + (k & 1), y + 3, 'i')
    return anim(base, 4, fn, False)

def palace():
    g = G()
    hill(g, 'G', 'g', 29)
    g.rect(3, 17, 26, 12, 'X'); g.hline(3, 28, 28, 'D')
    for x in range(4, 28, 4): g.disc(x + 1, 22.5, 1.3, 'Y'); g.rect(x, 22, 2, 3, 'Y')
    g.disc(16, 26, 2.8, 'V'); g.rect(14, 26, 5, 3, 'V')
    # onion domes
    def onion(cx, top, r, c, shade):
        g.disc(cx, top + r + 1, r, c, r * 0.9)
        g.tri(cx + 0.5, top - 2, top + 1, 1.2, c)
        for j in range(32):
            for i in range(int(cx) + 1, 32):
                if g.get(i, j) == c and i - cx > r * 0.35: g.set(i, j, shade)
        g.vline(int(cx), top - 4, top - 2, 'Y')
    g.rect(13, 12, 7, 6, 'X'); g.vline(19, 12, 17, 'D')
    onion(16.5, 4, 5, 'Q', 'H')
    g.rect(2, 13, 5, 5, 'X'); g.rect(25, 13, 5, 5, 'X')
    onion(4.5, 8, 3, 'E', 'e'); onion(27.5, 8, 3, 'E', 'e')
    g.hline(3, 28, 17, 'Y')
    base = g.copy(); base.outline()
    def fn(f, k):
        f.set(16, 0, ['Y', 'w', 'Y'][k]); f.set(4, 4, ['w', 'Y', 'Y'][k]); f.set(27, 4, ['Y', 'Y', 'w'][k])
        for n, (x, y) in enumerate(((9, 3), (23, 2), (2, 2))):
            if n == k:
                for dx, dy in ((0,0),(1,0),(-1,0),(0,1),(0,-1)): f.set(x + dx, y + dy, 'Y' if dx == dy == 0 else 'Q')
    return anim(base, 3, fn, False)

def tavern():
    frames = designs.tavern()
    for f in frames:   # a lilac-thatched, starry storybook version
        for j in range(32):
            for i in range(32):
                if f.p[j][i] == 'o': f.p[j][i] = 'H'
                elif f.p[j][i] == 'y' and j < 16: f.p[j][i] = 'Q'
    for k, f in enumerate(frames):
        f.set(3, 3 + (k % 2), 'Y'); f.set(9, 1, 'w' if k % 2 else 'Y')
    return frames

def forest():
    frames = designs.forest()
    for f in frames:
        f.set(4, 29, 'Q'); f.set(10, 30, 'H'); f.set(27, 30, 'Q')
    return frames

def cavern():
    g = G()
    g.poly([(0, 31), (3, 14), (9, 6), (17, 3), (25, 7), (30, 15), (32, 31)], 'a')
    for j in range(32):
        for i in range(32):
            if g.get(i, j) == 'a' and (i * 3 + j * 5) % 11 == 0: g.set(i, j, 'b')
            if g.get(i, j) == 'a' and i + j < 22: g.set(i, j, 'A')
    g.disc(16, 26, 8, 'B', 9)
    g.rect(6, 27, 20, 5, 'B')
    for x, l in ((11, 3), (14, 5), (18, 2), (21, 4)): g.vline(x, 18, 18 + l, 'a')
    # crystals
    def crystal(x, y, h, c, d):
        for j in range(h):
            w = max(0, 1 - abs(j - h // 3) // 3)
            g.set(x, y + j, c); g.set(x + 1, y + j, d if j > 0 else c)
    crystal(9, 25, 6, 'H', 'h'); crystal(13, 23, 8, 'E', 'e'); crystal(20, 24, 7, 'H', 'h'); crystal(23, 27, 4, 'E', 'e')
    g.hline(0, 31, 31, 'b')
    base = g.copy(); base.outline()
    def fn(f, k):
        lit = [(9, 25), (13, 23), (20, 24), (23, 27)][k % 4]
        f.set(lit[0], lit[1], 'w'); f.set(lit[0] - 1, lit[1] + 1, 'I')
        f.set(14, 23 + 5 + k, 'I')   # a drip falling
    return anim(base, 3, fn, False)

def beach():
    g = G()
    g.rect(0, 22, 32, 10, 'i'); 
    for j in range(22, 32):
        for i in range(32):
            if (i + j * 2) % 7 == 0: g.set(i, j, 'I')
    g.poly([(0, 32), (0, 27), (12, 25), (24, 28), (32, 30), (32, 32)], 'D')
    for i in range(0, 32, 5): g.set(i + 1, 30, 'd')
    g.disc(25, 9, 4.5, 'Y'); g.set(23, 7, 'w')
    # palm
    for j in range(10, 27):
        x = 8 + int((26 - j) * 0.25)
        g.set(x, j, 'T'); g.set(x + 1, j, 'V' if j % 3 else 'W')
    for dx, dy, l in ((1, 0, 7), (-1, 0, 7), (1, 1, 5), (-1, 1, 5), (0, -1, 3)):
        for t in range(l):
            x, y = 12 + dx * t, 9 + dy * t // 2 + (t * t) // 14
            g.set(x, y, 'l'); g.set(x, y - 1, 'L')
    g.disc(12, 10, 1.3, 'V')
    # crab
    g.rect(19, 27, 4, 2, 'R'); g.set(18, 26, 'R'); g.set(23, 26, 'R'); g.set(20, 26, 'w'); g.set(21, 26, 'w')
    base = g.copy(); base.outline()
    def fn(f, k):
        for i in range(32):
            if (i + k * 2) % 6 < 2 and f.get(i, 23) in 'iI': f.set(i, 23, 'w')
        f.set(20 + (k % 2), 29, 'R')
    return anim(base, 3, fn, False)

def desert():
    g = G()
    g.disc(22, 9, 5, 'O'); g.disc(22, 9, 3.6, 'Y'); g.set(20, 7, 'w')
    g.poly([(4, 24), (12, 11), (20, 24)], 'D'); g.poly([(12, 11), (20, 24), (14, 24)], 'd')
    for j in range(13, 24, 3): g.hline(12 - (j - 11) * 8 // 13, 12 + (j - 11) * 8 // 13, j, 'd')
    g.poly([(0, 32), (0, 24), (10, 21), (22, 25), (32, 22), (32, 32)], 'D')
    g.poly([(8, 32), (20, 27), (32, 29), (32, 32)], 'd')
    # cactus
    g.rect(26, 16, 2, 9, 'l'); g.rect(24, 18, 2, 1, 'l'); g.rect(24, 16, 1, 3, 'l'); g.rect(28, 19, 2, 1, 'l'); g.rect(29, 17, 1, 3, 'l')
    g.set(26, 15, 'Q')
    base = g.copy(); base.outline()
    def fn(f, k):
        for n, x in enumerate((3, 10, 16)):
            f.set(x + k, 27 - (n % 2), 'X')
        f.set(4, 4 + k % 2, 'w')
    return anim(base, 3, fn, False)

# ---------------- modern (neutral) ----------------
def factory():
    g = G()
    g.rect(0, 29, 32, 3, 'k')
    g.rect(2, 16, 28, 13, 'a'); g.vline(29, 16, 28, 'b')
    for x in range(2, 30, 7):   # saw-tooth roof
        g.poly([(x, 16), (x + 7, 10), (x + 7, 16)], 'A'); g.vline(x + 6, 11, 15, 'I')
    for x in range(4, 28, 5): g.rect(x, 20, 3, 3, 'Y')
    g.rect(13, 24, 5, 5, 'b')
    g.rect(4, 3, 3, 13, 'J'); g.hline(4, 6, 5, 'w'); g.rect(24, 6, 3, 10, 'J'); g.hline(24, 26, 8, 'w')
    base = g.copy(); base.outline()
    def fn(f, k):
        for (x, y0) in ((5, 2), (25, 5)):
            y = y0 - k
            for n in range(3):
                if 0 <= y - n * 2: f.set(x + (n % 2) - (k % 2), y - n * 2, 'A' if n < 2 else 'a')
    return anim(base, 3, fn, False)

def city():
    g = G()
    g.rect(0, 29, 32, 3, 'k')
    towers = [(1, 15, 6, 'M'), (7, 8, 6, 'I'), (13, 3, 6, 'M'), (19, 11, 5, 'i'), (24, 6, 7, 'M')]
    for x, top, w, c in towers:
        g.rect(x, top, w, 29 - top, c); g.vline(x + w - 1, top, 28, 'N')
        for j in range(top + 2, 28, 3):
            for i in range(x + 1, x + w - 1, 2): g.set(i, j, 'I' if c == 'M' else 'w')
    g.vline(16, 0, 2, 'N'); g.set(16, 0, 'R')
    # a little tree and a street lamp
    g.disc(4, 26, 2.5, 'l'); g.vline(4, 27, 28, 'T')
    base = g.copy(); base.outline()
    def fn(f, k):
        f.set(16, 0, 'R' if k != 1 else 'w')
        x = 2 + k * 9
        f.rect(x, 4 - (k % 2), 3, 1, 'w'); f.set(x + 1, 3 - (k % 2), 'w')   # a passing cloud
    return anim(base, 3, fn, False)

def seaship():
    g = G()
    g.rect(0, 24, 32, 8, 'i')
    for j in range(24, 32):
        for i in range(32):
            if (i * 2 + j) % 9 == 0: g.set(i, j, 'I')
    g.poly([(4, 21), (28, 21), (25, 26), (7, 26)], 'V'); g.hline(5, 27, 21, 'W'); g.hline(7, 25, 24, 'w')
    g.vline(15, 3, 20, 'T'); g.vline(22, 7, 20, 'T')
    g.poly([(16, 4), (16, 19), (25, 17)], 'X'); g.poly([(14, 5), (14, 19), (6, 18)], 'X')
    g.poly([(23, 8), (23, 19), (29, 18)], 'X')
    g.set(15, 2, 'R'); g.set(16, 2, 'R'); g.set(17, 2, 'R')
    base = g.copy()
    frames = []
    for k in range(3):
        f = base.copy()
        dy = [0, 1, 0][k]
        if dy:
            f.p = [['.'] * 32] + [r[:] for r in f.p[:31]]
            f.rect(0, 24, 32, 8, 'i')
            for j in range(24, 32):
                for i in range(32):
                    if (i * 2 + j + k) % 9 == 0: f.set(i, j, 'I')
            f.poly([(4, 22), (28, 22), (25, 26), (7, 26)], 'V'); f.hline(7, 25, 25, 'w')
        f.outline()
        for i in range(32):
            if (i + k * 3) % 8 < 2: f.set(i, 24 + (i % 2), 'w')
        frames.append(f)
    return frames

def suburbs():
    g = G()
    g.rect(0, 27, 32, 5, 'G'); g.hline(0, 31, 31, 'g')
    g.rect(5, 15, 16, 12, 'X'); g.vline(20, 15, 26, 'D')
    g.poly([(3, 16), (13, 7), (23, 16)], 'i'); g.poly([(13, 7), (23, 16), (19, 16)], 'B')
    g.rect(16, 8, 2, 5, 'J')
    g.rect(7, 18, 4, 3, 'I'); g.rect(15, 18, 4, 3, 'I'); g.set(7, 18, 'w'); g.set(15, 18, 'w')
    g.rect(11, 21, 3, 6, 'R'); g.set(13, 24, 'Y')
    for x in range(1, 32, 2):
        if x < 4 or x > 21: g.vline(x, 24, 27, 'w')
    g.hline(0, 3, 25, 'w'); g.hline(22, 31, 25, 'w')
    g.disc(27, 16, 4.5, 'l'); g.disc(26, 15, 2.5, 'L'); g.vline(27, 20, 26, 'T')
    g.rect(1, 21, 2, 2, 'i'); g.vline(2, 23, 26, 'k')   # mailbox
    base = g.copy(); base.outline()
    def fn(f, k):
        f.rect(15, 18, 4, 3, 'Y' if k == 1 else 'I'); f.set(15, 18, 'w')
        for n, (x, y) in enumerate(((6, 29), (8, 28), (10, 29))):
            if (n + k) % 2 == 0: f.set(x, y, 'I')
    return anim(base, 2, fn, False)

def mall():
    g = G()
    g.rect(0, 29, 32, 3, 'k')
    g.rect(1, 13, 30, 16, 'X'); g.vline(30, 13, 28, 'D'); g.hline(1, 30, 13, 'A')
    g.rect(3, 18, 26, 9, 'I')
    for x in range(3, 29, 4): g.vline(x, 18, 26, 'M')
    for i in range(4, 28, 3): g.set(i, 20, 'w')
    g.rect(12, 21, 8, 8, 'i'); g.vline(16, 21, 28, 'M')
    g.rect(8, 7, 16, 5, 'k')
    g.vline(16, 3, 6, 'N')
    base = g.copy(); base.outline()
    def fn(f, k):
        on = 'Q' if k == 0 else 'P'
        for i, col in enumerate("MALL"):
            pass
        # four little sign letters (blocks), blinking
        for n, x in enumerate((10, 13, 17, 20)):
            f.rect(x, 8, 2, 3, on if (n + k) % 2 == 0 or k == 0 else 'p')
        f.set(16, 2, 'R' if k == 0 else 'w')
    return anim(base, 2, fn, False)

def military():
    g = G()
    g.rect(0, 27, 32, 5, 'v'); g.hline(0, 31, 31, 'g')
    for i in range(0, 32, 4): g.set(i + 1, 28, 'g')
    g.poly([(2, 27), (11, 13), (20, 27)], 'l'); g.poly([(11, 13), (20, 27), (14, 27)], 'g')
    g.poly([(9, 27), (11, 21), (13, 27)], 'm')
    g.poly([(17, 27), (24, 17), (31, 27)], 'l'); g.poly([(24, 17), (31, 27), (27, 27)], 'g')
    g.rect(22, 24, 4, 3, 'm')
    g.rect(1, 24, 4, 3, 'W'); g.hline(1, 4, 24, 'V')
    g.vline(15, 3, 26, 'N')
    base = g.copy()
    flags = [[(16, 3, 'RRRR'), (16, 4, 'wwww'), (16, 5, 'RRRR')], [(16, 3, 'RRR'), (16, 4, 'wwwww'), (16, 5, 'RRRR')],
             [(16, 3, 'RRRR'), (16, 4, 'wwww'), (16, 5, 'RRR'), (19, 6, 'R')]]
    frames = []
    for k in range(3):
        f = base.copy()
        for x, y, run in flags[k]:
            for i, c in enumerate(run): f.set(x + i, y, c)
        f.outline(); frames.append(f)
    return frames

# ---------------- sci-fi (neon) ----------------
def hive():
    g = G()
    g.rect(0, 28, 32, 4, 'u')
    def pod(x, y, r, c, d):
        g.disc(x, y, r, c, r * 1.1); g.disc(x - r * 0.3, y - r * 0.35, r * 0.4, 'Z')
        for j in range(32):
            for i in range(32):
                if g.get(i, j) == c and (i - x) + (j - y) > r * 0.6: g.set(i, j, d)
    pod(9, 21, 6, 'l', 'm'); pod(21, 18, 7.5, 'p', 'u'); pod(14, 10, 4.5, 'l', 'm'); pod(27, 26, 3.5, 'p', 'u')
    for (x0, y0, x1, y1) in ((3, 28, 6, 25), (30, 28, 28, 23), (14, 28, 15, 26)): g.line(x0, y0, x1, y1, 'm')
    base = g.copy(); base.outline()
    def fn(f, k):
        glow = ['Z', 'L', 'Q'][k]
        for (x, y) in ((8, 20), (20, 17), (14, 10), (27, 26)): f.set(x, y, glow); f.set(x + 1, y + 1, glow if k != 2 else 'P')
    return anim(base, 3, fn, False)

def spaceship():
    g = G()
    g.poly([(3, 18), (9, 11), (24, 11), (30, 16), (24, 21), (9, 21)], 'M')
    g.poly([(3, 18), (30, 16), (24, 21), (9, 21)], 'N')
    g.poly([(18, 12), (24, 12), (27, 15), (19, 15)], 'C'); g.set(20, 13, 'w')
    g.poly([(9, 11), (14, 5), (17, 5), (15, 11)], 'n'); g.poly([(9, 21), (14, 27), (17, 27), (15, 21)], 'n')
    g.hline(10, 26, 17, 'c')
    g.rect(1, 14, 2, 5, 'n')
    base = g.copy(); base.outline()
    def fn(f, k):
        flame = ['OYO', 'YwY', 'OOy'][k]
        for j, c in zip((15, 16, 17), flame): f.set(0, j, c)
        if k != 2: f.set(0, 14 + k, 'y')
        for n, (x, y) in enumerate(((28, 3), (20, 28), (6, 4), (30, 26), (12, 30))):
            xx = (x - k * 3 - n) % 32
            if f.get(xx, y) == '.': f.set(xx, y, 'S'); 
            if f.get(xx + 1, y) == '.': f.set(xx + 1, y, 'c')
    return anim(base, 3, fn, False)

def biodome():
    g = G()
    g.disc(16, 25, 13, 'B', 13)
    for j in range(25, 32):
        for i in range(32):
            if g.get(i, j) == 'B': g.set(i, j, '.')
    for j in range(12, 25):
        for i in range(32):
            if g.get(i, j) == 'B' and (i + j) % 4 == 0 and i < 14 and j < 20: g.set(i, j, 'u')
    g.disc(10, 19, 4, 'l'); g.disc(9, 18, 2, 'L'); g.vline(10, 21, 24, 'T')
    g.disc(20, 16, 5, 'l'); g.disc(19, 14, 2.5, 'L'); g.vline(20, 19, 24, 'T')
    g.rect(13, 22, 3, 2, 'i')
    g.ring(16, 25, 12.2, 13.2, 'C', 1.0)
    for j in range(25, 32):
        for i in range(32):
            if g.get(i, j) == 'C': g.set(i, j, '.')
    for x in (10, 16, 22):
        for j in range(12, 25):
            if g.get(x, j) == 'B' and j % 2 == 0: g.set(x, j, 'c')
    g.rect(1, 25, 30, 4, 'N'); g.hline(1, 30, 25, 'M'); g.hline(1, 30, 28, 'n')
    for x in range(3, 30, 5): g.set(x, 27, 'C')
    base = g.copy(); base.outline()
    def fn(f, k):
        for t in range(3):
            a = math.radians(200 + k * 25 + t * 4)
            f.set(int(16 + 12.6 * math.cos(a)), int(25 + 12.6 * math.sin(a)), 'w')
        f.set(25, 26, 'C' if k % 2 else 'Z')
    return anim(base, 3, fn, False)

def station():
    g = G()
    g.ring(16, 16, 9, 12, 'M', 0.45)
    g.ring(16, 16, 9, 10, 'N', 0.45)
    g.rect(15, 4, 2, 24, 'N')
    g.disc(16, 16, 3.5, 'M'); g.disc(15, 15, 1.5, 'w'); g.set(17, 17, 'n')
    for x0 in (1, 24):   # solar panels
        g.rect(x0, 3, 7, 5, 'i'); g.rect(x0, 24, 7, 5, 'i')
        for x in range(x0 + 2, x0 + 7, 2): g.vline(x, 3, 7, 'c'); g.vline(x, 24, 28, 'c')
    g.hline(8, 14, 5, 'N'); g.hline(17, 23, 5, 'N'); g.hline(8, 14, 26, 'N'); g.hline(17, 23, 26, 'N')
    base = g.copy(); base.outline()
    def fn(f, k):
        lights = [(5, 16), (27, 16), (16, 12), (16, 21)]
        for n, (x, y) in enumerate(lights): f.set(x, y, 'R' if (n + k) % 3 == 0 else 'C')
        f.set(30, 12, 'S' if k == 1 else '.'); f.set(2, 20, 'S' if k == 2 else '.')
    return anim(base, 3, fn, False)

def bridge():
    g = G()
    g.rect(2, 3, 28, 13, 'B'); g.outline_box = None
    for (x, y) in ((6, 6), (12, 11), (20, 5), (26, 9), (9, 13), (17, 8), (24, 13)): g.set(x, y, 'S')
    g.disc(22, 9, 2.5, 'H'); g.disc(21, 8, 1, 'w')
    g.hline(2, 29, 16, 'n')
    g.poly([(0, 31), (3, 20), (29, 20), (32, 31)], 'n')
    g.hline(3, 28, 20, 'N')
    for x in range(5, 28, 3): g.rect(x, 22, 2, 1, 'k')
    g.rect(13, 23, 6, 5, 'N'); g.rect(14, 21, 4, 3, 'M')   # captain's chair
    base = g.copy(); base.outline()
    def fn(f, k):
        cols = ['C', 'Z', 'R', 'Y', 'P']
        for n, x in enumerate(range(5, 28, 3)):
            f.rect(x, 22, 2, 1, cols[(n + k) % 5] if (n + k) % 3 else 'k')
        for (x, y) in ((6, 6), (12, 11), (26, 9), (17, 8)):
            f.set(x, y, '.'); f.set(x - k - 1, y, 'S'); f.set(x - k, y, 'c')
        f.rect(2, 3, 28, 13, f.get(3, 4) if False else 'B') if False else None
    frames = anim(base, 3, fn, False)
    for f in frames:   # keep the stars inside the viewscreen
        for j in range(3, 16):
            for i in range(2, 30):
                if f.get(i, j) in '.K' and not (i in (2, 29) or j in (3, 15)): f.set(i, j, 'B') if f.get(i,j)=='.' else None
    return frames

def mining():
    g = G()
    g.poly([(1, 22), (5, 13), (13, 9), (23, 11), (30, 17), (28, 27), (18, 30), (6, 28)], 'b')
    for j in range(32):
        for i in range(32):
            if g.get(i, j) == 'b' and i + j < 26: g.set(i, j, 'a')
    g.disc(10, 20, 2.5, 'k'); g.disc(22, 24, 2, 'k'); g.disc(19, 15, 1.5, 'k')
    # rig
    g.rect(13, 2, 2, 9, 'N'); g.rect(10, 2, 8, 2, 'M'); g.poly([(11, 10), (17, 10), (14, 14)], 'O')
    g.hline(8, 20, 1, 'Y')
    base = g.copy(); base.outline()
    def fn(f, k):
        f.set(14, 14, 'w')
        for n, (dx, dy) in enumerate(((-2, 1), (2, 0), (-3, -1), (3, 2))):
            if (n + k) % 2 == 0: f.set(14 + dx * (1 + k % 2), 14 + dy - k % 2, 'Y' if n % 2 else 'O')
        f.set(18, 1, 'R' if k == 1 else 'Y')
    return anim(base, 3, fn, False)

def alienruins():
    g = G()
    g.rect(0, 28, 32, 4, 'u')
    g.poly([(10, 28), (12, 4), (19, 4), (21, 28)], 'n')
    g.poly([(16, 4), (19, 4), (21, 28), (17, 28)], 'k')
    g.poly([(12, 4), (15.5, 0), (19, 4)], 'n')
    g.rect(1, 18, 5, 10, 'n'); g.rect(26, 21, 5, 7, 'n'); g.vline(5, 18, 27, 'k'); g.vline(30, 21, 27, 'k')
    base = g.copy(); base.outline()
    glyphs = [(14, 8), (15, 12), (14, 16), (15, 20), (14, 24)]
    def fn(f, k):
        for n, (x, y) in enumerate(glyphs):
            c = 'C' if (n + k) % 3 == 0 else 'c'
            f.set(x, y, c); f.set(x + 1, y, c); f.set(x + 1, y + 1, c)
        f.set(3, 22, 'P' if k == 0 else 'p'); f.set(28, 24, 'P' if k == 2 else 'p')
        for n, (x, y) in enumerate(((5, 10), (25, 12))):
            yy = y - (1 if (n + k) % 2 else 0)
            f.rect(x, yy, 3, 2, 'n'); f.set(x + 1, yy + 2, 'C')
    return anim(base, 3, fn, False)

def empty():
    g = G()
    g.disc(16, 26, 11, 'k', 3.5)
    g.ring(16, 26, 9, 11, 'N', 3.5 / 11)
    g.rect(15, 9, 2, 17, 'W'); g.vline(16, 9, 25, 'V')
    g.poly([(6, 9), (24, 9), (27, 12), (24, 15), (6, 15)], 'X')
    g.hline(7, 23, 14, 'D')
    base = g.copy(); base.outline()
    def fn(f, k):
        c = 'Y' if k == 0 else 'w'
        f.rect(14, 11, 4, 1, c); f.rect(15, 10, 2, 3, c)
        if k == 1:
            f.set(4, 6, 'Y'); f.set(28, 5, 'Y')
    return anim(base, 2, fn, False)

ICONS = {
 'empty': empty(), 'castle': castle(), 'village': village(), 'spaceport': designs.spaceport(), 'factory': factory(), 'city': city(),
 'forest': forest(), 'hive': hive(), 'temple': temple(), 'cavern': cavern(), 'ruins': ruins(), 'spaceship': spaceship(),
 'biodome': biodome(), 'seaship': seaship(), 'suburbs': suburbs(), 'mall': mall(), 'military': military(), 'beach': beach(),
 'desert': desert(), 'palace': palace(), 'tavern': tavern(), 'station': station(), 'megacity': designs.megacity(),
 'bridge': bridge(), 'mining': mining(), 'alienruins': alienruins(), 'nebula': designs.nebula(),
}
if __name__ == '__main__':
    k = 4
    names = list(ICONS)
    cols = 4
    sheet = Image.new('RGBA', ((32*k+6) * cols * 2, (32*k+6) * ((len(names) + 1) // 2)), (24,20,34,255))
    for n, name in enumerate(names):
        frames = ICONS[name]
        ox = (n % 2) * (32*k+6) * cols; oy = (n // 2) * (32*k+6)
        for col, f in enumerate(frames):
            assert len(f.p) == 32 and all(len(r) == 32 for r in f.p), name
            f.png('tmp.png', k)
            sheet.paste(Image.open('tmp.png'), (ox + col*(32*k+6), oy))
    sheet.save('preview2.png')
    print(len(names))
