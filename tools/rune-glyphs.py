#!/usr/bin/env python3
"""
Add the Britannian runes for TH, EE, NG, EA & ST to the runesI font.

The runesI font has a rune for each letter but lacks the five combined
runes shown in the Ultima IV manual's runic alphabet.  This draws them as
strokes, renders signed distance fields into the free rows at the bottom of
module/render/font/cfont.png and appends glyphs for character codes
127-131 to cfont-runesI.txf.  Running it again replaces the glyphs.

  127 TH    128 EE    129 NG    130 EA    131 ST

Usage: tools/rune-glyphs.py [module/render/font]
"""

import math
import struct
import sys
from PIL import Image

FONT_DIR = sys.argv[1] if len(sys.argv) > 1 else "module/render/font"
FIRST_CODE = 127
BASE_COUNT = 95             # Glyphs 32-126 made by msdf-atlas-gen.
HALF_STROKE = 0.028         # Half the stroke width in EM units.
ROW_TOP = 266               # First free atlas row (image rows, top down).

# Strokes of each rune as polylines in EM units (Y up, baseline at 0,
# runes are 0.6 high like the letters of the font).
RUNES = [
    # TH: stem with a pointed flag at the top.
    (0.30, [[(0.05, 0.0), (0.05, 0.6), (0.21, 0.47), (0.05, 0.36)]]),
    # EE: crossed lines at the top & bottom of a diamond.
    (0.42, [[(0.32, 0.6), (0.08, 0.40), (0.34, 0.0)],
            [(0.08, 0.6), (0.32, 0.40), (0.06, 0.0)]]),
    # NG: a diamond on crossed legs.
    (0.42, [[(0.2, 0.6), (0.08, 0.45), (0.34, 0.0)],
            [(0.2, 0.6), (0.32, 0.45), (0.06, 0.0)]]),
    # EA: stem with a winged top.
    (0.46, [[(0.02, 0.50), (0.2, 0.6), (0.32, 0.50), (0.42, 0.58)],
            [(0.2, 0.6), (0.2, 0.0)]]),
    # ST: two posts joined by notched top & bottom bars.
    (0.48, [[(0.04, 0.0), (0.04, 0.6), (0.22, 0.47), (0.40, 0.6),
             (0.40, 0.0), (0.22, 0.13), (0.04, 0.0)]]),
]


def segDist(px, py, ax, ay, bx, by):
    dx = bx - ax
    dy = by - ay
    t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def strokeDist(px, py, lines):
    d = 9.0
    for pts in lines:
        for a, b in zip(pts, pts[1:]):
            d = min(d, segDist(px, py, a[0], a[1], b[0], b[1]))
    return d


def main():
    txfPath = FONT_DIR + "/cfont-runesI.txf"
    pngPath = FONT_DIR + "/cfont.png"

    data = bytearray(open(txfPath, "rb").read())
    texW, texH, count, kernOff = struct.unpack_from("<4H", data, 0)
    fontSize, pixelRange = struct.unpack_from("<2f", data, 8)
    first = struct.unpack_from("<H", data, 28)[0]
    if first + BASE_COUNT != FIRST_CODE or kernOff:
        sys.exit("Unexpected cfont-runesI.txf layout")
    data = data[:28 + BASE_COUNT * 40]

    atlas = Image.open(pngPath).convert("RGBA")
    pix = atlas.load()

    # Clear the rows used for the added glyphs.
    for y in range(ROW_TOP, texH):
        for x in range(texW):
            pix[x, y] = (0, 0, 0, 0)

    texel = 1.0 / fontSize              # EM size of an atlas texel.
    pad = pixelRange * 0.5 * texel + HALF_STROKE
    tx = 2
    for i, (advance, lines) in enumerate(RUNES):
        xs = [p[0] for pts in lines for p in pts]
        ys = [p[1] for pts in lines for p in pts]
        cols = int(math.ceil((max(xs) - min(xs) + 2 * pad) / texel)) + 1
        rows = int(math.ceil((max(ys) - min(ys) + 2 * pad) / texel)) + 1
        emX = min(xs) - pad
        emY = min(ys) - pad
        if tx + cols > texW or rows > texH - ROW_TOP:
            sys.exit("Rune glyphs do not fit in the atlas")

        # Rows are filled from the bottom of the image (texture T = 0).
        for r in range(rows):
            for c in range(cols):
                d = HALF_STROKE - strokeDist(emX + c * texel,
                                             emY + r * texel, lines)
                v = 0.5 + d * fontSize / pixelRange
                v = int(round(max(0.0, min(1.0, v)) * 255))
                pix[tx + c, texH - 1 - r] = (v, v, v, v)

        em = (emX, emY, emX + (cols - 1) * texel, emY + (rows - 1) * texel)
        tc = ((tx + 0.5) / texW, 0.5 / texH,
              (tx + cols - 0.5) / texW, (rows - 0.5) / texH)
        data += struct.pack("<HHf4f4f", FIRST_CODE + i, 0, advance, *em, *tc)
        tx += cols + 2

    struct.pack_into("<H", data, 4, BASE_COUNT + len(RUNES))
    open(txfPath, "wb").write(data)
    atlas.save(pngPath, optimize=True)


main()
