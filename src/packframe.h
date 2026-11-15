/*
 * XU4 - Window frames drawn with the art of the graphics module
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PACKFRAME_H
#define PACKFRAME_H

#include <stdint.h>

class Image;

/*
 * The eight 8x8 pieces of a window frame of a graphics module (the corners
 * & sides of its own windows), named by the subimages frame_tl, frame_t,
 * frame_tr, frame_l, frame_r, frame_bl, frame_b & frame_br of any of its
 * images.
 */
struct PackFrame {
    enum Piece { TL, T, TR, L, R, BL, B, BR, PIECES };
    uint32_t px[PIECES][2][64]; // RGBA pixels, row by row.
    int cells[PIECES];          // 2 if a side repeats every two cells.

    // Cell n of a side (the sides of the NES chain alternate two links).
    const uint32_t* side(int piece, int n) const {
        return px[piece][n % cells[piece]];
    }
};

bool   packFrame(PackFrame* pf);
Image* packFrameBorders(const PackFrame& pf);
void   packFrameDraw(const PackFrame& pf, uint32_t* dst, int dw, int dh,
                     int x, int y, int w, int h, int scale);

#endif
