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
    uint32_t px[PIECES][64];    // RGBA pixels, row by row.
};

bool   packFrame(PackFrame* pf);
Image* packFrameBorders(const PackFrame& pf);
void   packFrameDraw(const PackFrame& pf, uint32_t* dst, int dw, int dh,
                     int x, int y, int w, int h, int scale);

#endif
