"""A new Claude-chan for the hello, after the owner's reference (2026-11-18): a chibi with long wavy hair, a curl on
top, a flower and ribbon at her left, flat half-lidded eyes over big irises, a bow at the collar.

    chan_draw.py <repo> <out folder>

Writes chan_new.txt (four 48 x 48 poses: ahead, left, right, blink, as fonts/claude_chan.txt orders them) and a mock
sheet of the hello as it is laid out now and of a speech-bubble layout, drawn as the hello shows them: dark on the
lit glass.
"""
import math
import sys
from pathlib import Path

REPO, OUT = Path(sys.argv[1]), Path(sys.argv[2])
sys.path.insert(0, str(REPO / "tools" / "ui_mock"))
import menu_12864 as m  # noqa: E402
from panel import H, Panel, W, write_sheet  # noqa: E402

m.use_faces(m.DEFAULT_TEXT, m.DEFAULT_BOLD, m.DEFAULT_FIGURE)
S = 48


class Grid:
    def __init__(self, w=S, h=S):
        self.w, self.h = w, h
        self.px = [[False] * w for _ in range(h)]

    def get(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h and self.px[y][x]

    def set(self, x, y, on=True):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = on

    def line(self, x0, y0, x1, y1, on=True):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            self.set(x0, y0, on)
            if x0 == x1 and y0 == y1:
                return
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def poly(self, points, on=True):
        for (a, b), (c, d) in zip(points, points[1:]):
            self.line(a, b, c, d, on)

    def text(self):
        return ["".join("#" if c else "." for c in row) for row in self.px]


def in_ellipse(x, y, cx, cy, rx, ry):
    return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


# The fringe: rounded strands hanging over the forehead, the outer ones longer to frame the face
STRANDS = [(10, 32, 0.6), (15, 25, 0.7), (20, 22, 0.75), (25, 24, 0.75), (30, 21, 0.75), (34, 25, 0.7),
           (38, 32, 0.6)]


def bangs_edge(x):
    return max(ty - a * (x - cx) ** 2 for cx, ty, a in STRANDS)


def hair_at(x, y):
    if in_ellipse(x, y, 24, 22, 21.5, 19.5):
        return y >= 2
    if y >= 18:
        wave = 1.3 * math.sin(y / 2.4)
        return 1 + wave <= x <= 12 or 35 <= x <= 46 - wave
    return False


def face_at(x, y):
    return in_ellipse(x, y, 24, 30, 14.5, 12.5) and y >= bangs_edge(x)


def figure(look, blink=False):
    """
    One pose.

    Args:
        look: the irises' shift, -2 (to the viewer's left), 0, or 2.
        blink: the eyes shut.
    """
    g = Grid()
    # The hair a checker, the reference's light orange as a mid tone (the owner's choice, 2026-11-18), its edge solid:
    # against the glass and against the face
    for y in range(S):
        for x in range(S):
            if hair_at(x, y) and not face_at(x, y):
                hair_px = (x + y) % 2 == 0
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if not hair_at(nx, ny) or face_at(nx, ny):
                        hair_px = True
                g.set(x, y, hair_px)
    # A shine on the crown, as the reference's: two light arcs
    g.poly([(10, 15), (12, 12), (15, 9), (18, 8)], False)
    g.poly([(31, 8), (34, 9)], False)
    g.poly([(19, 11), (21, 10)], False)
    # The curl on top, solid
    g.poly([(22, 4), (23, 1), (26, 0), (30, 0), (32, 2), (31, 4)])
    g.poly([(23, 4), (24, 2), (26, 1), (29, 1), (31, 2)])
    g.set(30, 4)
    g.set(30, 5)
    # The eyes
    for cx in (17, 31):
        if blink:
            g.poly([(cx - 5, 31), (cx - 3, 33), (cx + 2, 33), (cx + 4, 31)])
            g.poly([(cx - 5, 30), (cx - 3, 32), (cx + 2, 32), (cx + 4, 30)])
            continue
        # The lid: a thick flat bar, its outer end flicked down
        for x in range(cx - 5, cx + 5):
            g.set(x, 26)
            g.set(x, 27)
        outer = cx - 6 if cx < 24 else cx + 5
        g.set(outer, 27)
        g.set(outer, 28)
        # The eye's white below the lid, edged at its sides and bottom
        for y in range(28, 38):
            for x in range(cx - 6, cx + 6):
                if in_ellipse(x + 0.5, y, cx, 31.5, 5.2, 6.0):
                    edge = not in_ellipse(x + 0.5, y, cx, 31.5, 4.2, 5.0)
                    g.set(x, y, edge and y >= 30)
        # The iris, big and dark, looking; its shine and a lighter low glint
        ix = cx + look
        for y in range(28, 37):
            for x in range(cx - 5, cx + 5):
                if in_ellipse(x + 0.5, y, ix, 32, 3.4, 4.4):
                    g.set(x, y)
        for x, y in ((ix - 2, 29), (ix - 1, 29), (ix - 2, 30), (ix - 1, 30), (ix + 1, 34)):
            g.set(x, y, False)
    # The blush: two short strokes under each eye
    for bx in (9, 36):
        for x, y in ((bx, 38), (bx + 1, 37), (bx + 2, 38), (bx + 3, 37)):
            g.set(x, y)
    # The mouth: a small soft line
    g.poly([(22, 40), (23, 41), (25, 41), (26, 40)])
    # The flower at her left: a light daisy on the dark hair, its petals parted by dark lines, a dark heart
    fx, fy = 39, 12
    flower = ["...##.##...",
              "..###.###..",
              ".####.####.",
              "####...####",
              "###.###.###",
              "....#.#....",
              "###.###.###",
              "####...####",
              ".####.####.",
              "..###.###..",
              "...##.##..."]
    del flower
    # Five round light petals on a dark disc, round a dark heart with a light point
    for y in range(fy - 6, fy + 7):
        for x in range(fx - 6, fx + 7):
            if in_ellipse(x, y, fx, fy, 5.7, 5.7):
                g.set(x, y)
    for k in range(5):
        a = -math.pi / 2 + k * 2 * math.pi / 5
        px, py = fx + 3.1 * math.cos(a), fy + 3.1 * math.sin(a)
        for y in range(fy - 6, fy + 7):
            for x in range(fx - 6, fx + 7):
                if in_ellipse(x, y, px, py, 2.1, 2.1):
                    g.set(x, y, False)
    for y in range(fy - 1, fy + 2):
        for x in range(fx - 1, fx + 2):
            g.set(x, y)
    g.set(fx, fy, False)
    # The collar's frill and the bow
    for x in range(15, 34, 2):
        g.set(x, 43)
    bow = ["##.........##", "####.....####", "#####.###.###", "##..#######..", "#####.###.###", "####.....####"]
    for r, row in enumerate(bow):
        for c, ch in enumerate(row):
            g.set(18 + c, 42 + r, ch == "#")
    return g


def on_panel(g, x0, y0, panel):
    for y in range(g.h):
        for x in range(g.w):
            if g.get(x, y):
                panel.set(x0 + x, y0 + y)


def hello_now(g, name):
    """The hello as the firmware lays it out: the figure at the left, the name and words at its right."""
    p = Panel()
    on_panel(g, 2, 16, p)
    p.text(m.BOLD, 54, 24, name)
    p.text(m.TEXT, 54, 36, "is watching")
    p.text(m.TEXT, 54, 48, "ya!")
    return p


def bubble_screen(g, name):
    """
    The reference's layout: the figure at the bottom right, looking up at a speech bubble at the top left with the
    words inside. The bubble is as wide as its words and opaque: a long name's bubble overlaps her hair.
    """
    p = Panel()
    on_panel(g, W - 48 - 2, H - 48 + 8, p)
    words = "is watching ya!"
    inner = max(m.BOLD.width(name), m.TEXT.width(words))
    left, top, bottom = 2, 4, 36
    right = min(W - 2, left + inner + 12)
    r = 6
    # The bubble's inside cleared, then its edge
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            p.set(x, y, False)
    for x in range(left + r, right - r + 1):
        p.set(x, top)
        p.set(x, bottom)
    for y in range(top + r, bottom - r + 1):
        p.set(left, y)
        p.set(right, y)
    for cx, cy, sx, sy in ((left + r, top + r, -1, -1), (right - r, top + r, 1, -1), (left + r, bottom - r, -1, 1),
                           (right - r, bottom - r, 1, 1)):
        for k in range(0, 91, 3):
            a = math.radians(k)
            p.set(round(cx + sx * r * math.cos(a)), round(cy + sy * r * math.sin(a)))
    # The tail, down toward her: a notch in the bottom edge and two sides meeting at its tip
    tail_x = min(right - 14, 64)
    for x in range(tail_x + 1, tail_x + 8):
        p.set(x, bottom, False)
    tip = (tail_x + 12, bottom + 7)
    for y in range(bottom, tip[1] + 1):
        for x in range(tail_x, tip[0] + 1):
            pass
    a = Grid(W, H)
    a.line(tail_x, bottom, tip[0], tip[1])
    a.line(tail_x + 8, bottom, tip[0], tip[1])
    for y in range(H):
        for x in range(W):
            if a.get(x, y):
                p.set(x, y)
    # Two strokes of surprise at the bubble's corner
    for x, y in ((2, 0), (3, 1), (7, 0), (7, 1)):
        p.set(x, y)
    middle = (left + right) // 2
    p.text(m.BOLD, middle - m.BOLD.width(name) // 2, top + 13, name)
    p.text(m.TEXT, middle - m.TEXT.width(words) // 2, top + 26, words)
    return p


BUBBLE_N = 2.5         # the oval's roundness: 2 an ellipse, higher squarer; 2.5 round, and two lines still fit


def bubble_cloud(g, name, bumpy=False):
    """
    The reference's layout with its bubble: a round bubble at the top left, as wide as its words, its tail always
    pointing at her; the figure at the bottom right. The bubble and its tail are opaque and drawn over her, so a long
    name's bubble may cover her hair.

    Args:
        g: her pose.
        name: the agent's name.
        bumpy: soft bumps along the bubble's top, as the reference's cloud.
    """
    p = Panel()
    on_panel(g, W - 48 - 2, 16, p)
    words = "is watching ya!"
    # The bubble: an oval (a superellipse) as small as holds the two lines: their half-width tw, half-height th
    tw, th = max(m.BOLD.width(name), m.TEXT.width(words)) / 2 + 2, 12
    cy, b = 17, 16
    a = tw / (1 - (th / b) ** BUBBLE_N) ** (1 / BUBBLE_N) + 3
    cx = 2 + a
    bumps = []
    if bumpy:
        for k in (-0.55, -0.1, 0.35):
            bx = cx + k * a
            by = cy - b * (1 - abs(k) ** BUBBLE_N) ** (1 / BUBBLE_N) + 3.2
            bumps.append((bx, by, 5.0))

    def inside(x, y):
        if abs((x - cx) / a) ** BUBBLE_N + abs((y - cy) / b) ** BUBBLE_N <= 1:
            return True
        return any(in_ellipse(x, y, bx, by, br, br) for bx, by, br in bumps)

    # The tail: from the bubble's lower right to a tip on her hair, left of her face, wherever the bubble ends
    k = 0.3
    tx = cx + k * a
    ty = cy + b * (1 - k ** BUBBLE_N) ** (1 / BUBBLE_N) - 3
    tip = (max(86, tx + 8), 42)

    def in_tail(x, y):
        # between the two lines from the base's ends to the tip
        if not (ty <= y <= tip[1]):
            return False
        f = (y - ty) / (tip[1] - ty)
        lo = tx - 4 + (tip[0] - (tx - 4)) * f
        hi = tx + 4 + (tip[0] - (tx + 4)) * f
        return lo <= x <= hi

    def shape(x, y):
        return 0 <= x < W and 0 <= y < H and (inside(x, y) or in_tail(x, y))

    for y in range(H):
        for x in range(W):
            if shape(x, y):
                edge = any(not shape(nx, ny) for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
                p.set(x, y, edge)
    # Two strokes of surprise at its corner
    for x, y in ((1, 1), (2, 2), (5, 0), (5, 1)):
        p.set(x, y)
    p.text(m.BOLD, round(cx - m.BOLD.width(name) / 2), cy - 2, name)
    p.text(m.TEXT, round(cx - m.TEXT.width(words) / 2), cy + 10, words)
    return p


def bright(panel):
    """The hello is shown bright: the glass lit, the drawing dark."""
    q = Panel()
    for y in range(H):
        for x in range(W):
            q.px[y][x] = not panel.px[y][x]
    return q


OUT.mkdir(parents=True, exist_ok=True)
sets = [figure(0), figure(-2), figure(2), figure(0, blink=True)]
(OUT / "chan_new.txt").write_text("\n".join(line for g in sets for line in g.text()) + "\n")
old = (REPO / "fonts" / "claude_chan.txt").read_text().split()
panels, captions = [], []
for bumpy, kind in ((False, "Round"),):
    for name, look, blink, label in (("Claude-chan", -2, False, "she looks at it"), ("Claude", 2, False, "a short name"),
                                     ("OpenTrickler-Bot", -2, False, "a long name"),
                                     ("Claude-chan", 0, True, "blink")):
        panels.append(bright(bubble_cloud(figure(look, blink=blink), name, bumpy)))
        captions.append(f"{kind}: {label}")
sheet = OUT / "chan_new.png"
write_sheet(sheet, panels, captions, m.CAPTION, scale=4, columns=4)
print(sheet)
