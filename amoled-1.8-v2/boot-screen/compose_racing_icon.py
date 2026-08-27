#!/usr/bin/env python3
"""Racing launcher icon (#258): full-bleed opaque BLACK tile so no orange
desktop bleeds through the edges. Two inset orange racing stripes (stop before
the edge), a checkerboard finish band, and a top-down car. 120x120 to fill the
120dp icon tile. Run with /home/tunas/venv/bin/python."""
from PIL import Image, ImageDraw

W = H = 120
S = 4                       # supersample
BLACK = (0, 0, 0)
ORANGE = (249, 103, 2)      # #F96702
WHITE = (245, 245, 245)
RED = (222, 40, 40)
GLASS = (30, 34, 44)
OUT = "/tmp/claude-1000/-home-tunas-DesktopShare/a6a415e2-33f4-4224-96c1-b40638cd4173/scratchpad/racing_icon.png"

im = Image.new("RGB", (W * S, H * S), BLACK)
d = ImageDraw.Draw(im)


def R(x0, y0, x1, y1, fill, radius=0):
    if radius:
        d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=radius * S, fill=fill)
    else:
        d.rectangle([x0 * S, y0 * S, x1 * S, y1 * S], fill=fill)


# --- two long orange racing stripes down the middle, inset from top/bottom
#     (stop before edge). Long enough to show above and below the car. ---
R(51, 10, 58, 86, ORANGE)
R(62, 10, 69, 86, ORANGE)

# --- checkerboard finish band near the bottom, inset from side edges ---
cb_x0, cb_x1, cb_y0, cb_y1 = 16, 104, 92, 108
cell = (cb_x1 - cb_x0) / 8.0
rows = 2
rh = (cb_y1 - cb_y0) / rows
for r in range(rows):
    for c in range(8):
        if (r + c) % 2 == 0:
            x0 = cb_x0 + c * cell
            y0 = cb_y0 + r * rh
            R(x0, y0, x0 + cell, y0 + rh, WHITE)

# --- top-down car, centered over the stripes (shorter, so orange shows
#     above and below it) ---
# body
R(44, 30, 76, 78, RED, radius=8)
# windshield + rear window
d.polygon([(49 * S, 38 * S), (71 * S, 38 * S), (67 * S, 47 * S), (53 * S, 47 * S)], fill=GLASS)
d.polygon([(53 * S, 62 * S), (67 * S, 62 * S), (70 * S, 70 * S), (50 * S, 70 * S)], fill=GLASS)
# wheels (black, poking out past the body edges)
R(40, 34, 46, 45, BLACK)
R(74, 34, 80, 45, BLACK)
R(40, 62, 46, 73, BLACK)
R(74, 62, 80, 73, BLACK)

im = im.resize((W, H), Image.LANCZOS)
im.save(OUT)
print("wrote", OUT, im.size)
