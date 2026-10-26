from icons import *

def castle():
    g = G()
    # grassy hill
    g.disc(16, 36, 19, 'G', 9)
    for i in range(0, 32, 3): g.set(i, 29 + (i % 2), 'g')
    # towers
    for cx in (6, 25):
        g.rect(cx-3, 12, 7, 16, 'A')
        g.vline(cx+3, 12, 27, 'a')
        g.rect(cx-1, 16, 2, 3, 'Y'); g.set(cx-1, 16, 'y')
        g.tri(cx+0.5, 4, 11, 5, 'R')
        for j in range(5, 12): g.set(cx+1 + (j-4)*5//7, j, 'r')
        g.hline(cx-4, cx+4, 11, 'r')
    # keep
    g.rect(10, 13, 12, 15, 'A')
    g.rect(20, 13, 2, 15, 'a')
    for i in range(10, 22, 3): g.rect(i, 11, 2, 2, 'A')
    # stone texture
    for (x, y) in ((11, 16), (17, 19), (13, 24), (4, 22), (27, 20), (19, 25), (23, 14)): g.set(x, y, 'a')
    # gate
    g.rect(13, 21, 6, 7, 'V'); g.hline(14, 17, 20, 'V'); g.vline(15, 21, 27, 'W'); g.vline(16, 21, 27, 'T')
    g.set(13, 21, 'b'); g.set(18, 21, 'b')
    # keep windows
    g.rect(12, 15, 2, 2, 'Y'); g.rect(18, 15, 2, 2, 'Y')
    # centre spire + flag pole
    g.tri(16, 5, 10, 3, 'R'); g.vline(16, 0, 4, 'V')
    g.p = [['.']*32]*0 + g.p  # unchanged; shift below
    g.p = [['.']*32 for _ in range(2)] + g.p[:30]
    for i in range(32):
        for j in (29, 30, 31):
            if g.get(i, j) == '.' and abs(i - 15.5) < 14 + (j - 29) * 1.5: g.set(i, j, 'G' if j < 31 else 'g')
    base = g.copy()
    # flag frames (waving)
    flags = [
        [(17,1,'FFFFF'), (17,2,'FFFFFF'), (17,3,'FFFf'), (17,4,'f')],
        [(17,1,'FFFF'), (17,2,'FFFFFF'), (17,3,'FFFFFf'), (17,4,'')],
        [(17,1,'FFF'), (17,2,'FFFFF'), (17,3,'FFFFFf'), (22,4,'f')],
    ]
    frames = []
    for fl in flags:
        f = base.copy()
        for x, y, run in fl:
            for k, c in enumerate(run): f.set(x+k, y, c)
        f.outline(); frames.append(f)
    return frames

def tavern():
    g = G()
    g.disc(16, 37, 20, 'G', 9)
    # timber house body
    g.rect(5, 15, 20, 13, 'W')
    for x in (5, 11, 18, 24): g.vline(x, 15, 27, 'V')
    g.hline(5, 24, 21, 'V')
    # roof (thatched, warm)
    for j in range(6, 15):
        t = (j-6)/8; hw = 4 + t*9
        for i in range(int(15-hw), int(15+hw)+1): g.set(i, j, 'y' if (i+j) % 4 else 'o')
    g.hline(2, 28, 14, 'o')
    # chimney with smoke handled in frames
    g.rect(21, 5, 3, 6, 'b'); g.hline(20, 24, 4, 'a')
    # door + windows glowing
    g.rect(13, 22, 4, 6, 'V'); g.set(16, 25, 'Y')
    g.rect(7, 17, 3, 3, 'Y'); g.set(8, 18, 'y'); g.rect(20, 17, 3, 3, 'Y'); g.set(21, 18, 'y')
    # sign bracket
    g.hline(25, 29, 16, 'V'); g.vline(29, 16, 17, 'V')
    base = g.copy()
    signs = [  # mug sign swinging: offsets
        (27, 18), (28, 18), (29, 18), (28, 18)]
    smokes = [((22, 3), (21, 1)), ((23, 2), (22, 0)), ((22, 2), (23, 0)), ((21, 3), (22, 1))]
    frames = []
    for k in range(4):
        f = base.copy()
        sx, sy = signs[k]
        f.rect(sx, sy, 4, 4, 'W'); f.rect(sx+1, sy+1, 2, 2, 'Y'); f.set(sx+1, sy+1, 'w')
        f.vline(sx+2, 17, sy-1, 'V')
        f.outline()
        for (x, y) in smokes[k]: f.set(x, y, 'a'); f.set(x+1, y, 'A')
        frames.append(f)
    return frames

def forest():
    g = G()
    g.disc(16, 38, 21, 'g', 10)
    def pine(cx, top, tiers, w):
        y = top
        for t in range(tiers):
            hw0, hw1 = 1 + t, 3 + t + w
            for j in range(4):
                hw = hw0 + (hw1 - hw0) * j / 3
                for i in range(int(round(cx - hw)), int(round(cx + hw)) + 1):
                    g.set(i, y + j, 'm' if i > cx + 0.5 else ('L' if j == 0 and i < cx else 'l'))
            y += 3
        g.rect(cx - 1, y + 1, 2, 28 - y, 'T')
    pine(6, 6, 6, 0)
    pine(26, 4, 7, 0)
    # round oak in front
    g.rect(14, 20, 4, 8, 'T'); g.vline(17, 20, 27, 'V')
    g.disc(16, 15, 7.5, 'l', 6.5); g.disc(14.5, 13, 5, 'L', 4)
    for (x, y) in ((19, 17), (21, 14), (11, 18), (18, 10), (13, 16)): g.set(x, y, 'm')
    for (x, y) in ((12, 11), (15, 10)): g.set(x, y, 'w')
    # mushroom
    g.rect(22, 26, 1, 2, 'w'); g.hline(21, 23, 25, 'R'); g.set(22, 25, 'w')
    base = g.copy(); base.outline()
    flies = [[(2, 2), (30, 1), (11, 4)], [(3, 3), (31, 2), (20, 3)], [(2, 4), (12, 3)], [(1, 3), (21, 4), (30, 0)]]
    frames = []
    for fl in flies:
        f = base.copy()
        for x, y in fl:
            if f.get(x, y) == '.': f.set(x, y, 'Z')
        frames.append(f)
    return frames

def spaceport():
    g = G()
    # launch pad
    g.rect(2, 27, 28, 3, 'N'); g.hline(2, 29, 27, 'M'); g.hline(2, 29, 29, 'n')
    for x in range(4, 29, 6): g.set(x, 28, 'C')
    # tower gantry
    g.rect(4, 8, 3, 19, 'n');
    for j in range(9, 26, 3): g.hline(4, 6, j, 'N')
    g.hline(7, 11, 12, 'N'); g.hline(7, 11, 18, 'N')
    g.set(5, 7, 'R')
    # rocket
    g.rect(14, 8, 6, 15, 'M'); g.vline(18, 8, 22, 'N'); g.vline(19, 8, 22, 'n')
    g.tri(17, 2, 8, 3, 'M'); g.set(17, 2, 'R'); g.set(16, 3, 'R')
    g.disc(17, 12, 1.6, 'C'); g.set(16, 11, 'w')
    g.hline(14, 19, 17, 'c')
    # fins
    g.rect(12, 19, 2, 4, 'R'); g.set(13, 18, 'R'); g.rect(20, 19, 2, 4, 'R'); g.set(20, 18, 'R')
    g.rect(15, 23, 4, 1, 'n')
    base = g.copy(); base.outline()
    flames = [
        [(15,24,'OOOO'), (15,25,'yOOy'), (16,26,'oo')],
        [(15,24,'OYYO'), (15,25,'OOOO'), (16,26,'yy'), (16,27,'o')],
        [(15,24,'OOOO'), (16,25,'OO'), (16,26,'o')],
    ]
    frames = []
    for fl in flames:
        f = base.copy()
        for x, y, run in fl:
            for k, c in enumerate(run): f.set(x+k, y, c)
        frames.append(f)
    return frames

def megacity():
    g = G()
    # skyline of towers
    towers = [(1, 14, 6, 'n'), (6, 6, 6, 'N'), (12, 1, 7, 'n'), (19, 9, 5, 'N'), (24, 4, 7, 'n')]
    for x, top, w, c in towers:
        g.rect(x, top, w, 31-top, c)
        g.vline(x+w-1, top, 30, 'B')
    # antenna on tallest
    g.vline(15, 0, 0, 'N')
    # lit windows grid
    import random
    rnd = random.Random(7)
    for x, top, w, c in towers:
        for j in range(top+2, 30, 2):
            for i in range(x+1, x+w-1, 2):
                if rnd.random() < 0.45: g.set(i, j, 'C' if rnd.random() < 0.6 else 'Y')
    # neon signs
    g.rect(7, 12, 1, 6, 'P'); g.rect(8, 12, 2, 1, 'P')   # vertical sign
    g.rect(20, 14, 3, 2, 'p')
    base = g.copy(); base.outline()
    # rain streaks (moving) + neon flicker
    frames = []
    for k in range(4):
        f = base.copy()
        for (x, y) in ((3, 2), (10, 4), (21, 1), (29, 3), (18, 6)):
            yy = (y + k*2) % 8
            if f.get(x, yy) == '.': f.set(x, yy, 'c')
        sign = 'P' if k != 2 else 'p'
        f.rect(7, 12, 1, 6, sign); f.rect(8, 12, 2, 1, sign)
        f.rect(20, 14, 3, 2, 'P' if k % 2 else 'p')
        frames.append(f)
    return frames

def nebula():
    g = G()
    cx, cy = 16, 16
    # two spiral arms of gas, coloured by distance from the core
    for arm in (0, math.pi):
        for s in range(0, 230):
            t = s / 40.0
            r = 1.2 + t * 2.4
            a = arm + t * 1.35
            x, y = cx + r * math.cos(a), cy + r * math.sin(a) * 0.75
            thick = 0.7 + t * 0.32
            for dy in range(-3, 4):
                for dx in range(-3, 4):
                    if dx*dx + dy*dy <= thick*thick:
                        px, py = int(x + dx), int(y + dy)
                        d = math.hypot(px + .5 - cx, (py + .5 - cy) / 0.75)
                        c = 'Q' if d < 3.5 else 'P' if d < 6.5 else 'U' if d < 10 else 'u'
                        if c == 'u' and (px + py) % 2: continue
                        if g.get(px, py) in '.u' or c in 'QP': g.set(px, py, c)
    g.disc(cx, cy, 3, 'Q', 2.5)
    g.rect(15, 15, 2, 2, 'S'); g.set(14, 15, 'w'); g.set(17, 16, 'w'); g.set(15, 14, 'w'); g.set(16, 17, 'w')
    base = g.copy()
    stars = [(3, 4), (28, 5), (5, 27), (27, 27), (24, 2), (2, 17), (30, 14)]
    frames = []
    for k in range(3):
        f = base.copy()
        for n, (x, y) in enumerate(stars):
            if f.get(x, y) != '.': continue
            f.set(x, y, 'S' if (n + k) % 3 else 'C')
            if (n + k) % 3 == 0:
                for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)):
                    if f.get(x+dx, y+dy) == '.': f.set(x+dx, y+dy, 'c')
        if k == 1:
            for x, y in ((13, 15), (18, 16), (15, 13), (16, 18)): f.set(x, y, 'C')
        frames.append(f)
    return frames

ICONS = {'castle': castle(), 'tavern': tavern(), 'forest': forest(), 'spaceport': spaceport(), 'megacity': megacity(), 'nebula': nebula()}
if __name__ == '__main__':
    k = 6
    sheet = Image.new('RGBA', ((32*k+8) * 4, (32*k+8) * 6), (24,20,34,255))
    for row, (name, frames) in enumerate(ICONS.items()):
        for col, f in enumerate(frames):
            f.png('tmp.png', k)
            sheet.paste(Image.open('tmp.png'), (col*(32*k+8), row*(32*k+8)))
    sheet.save('preview.png')
