/*
 * XU4 - Window frames drawn with the art of the graphics module
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A graphics module whose own game has framed windows (the SMS & the NES)
 * names the eight pieces of that frame with the subimages frame_tl,
 * frame_t, frame_tr, frame_l, frame_r, frame_bl, frame_b & frame_br (8x8,
 * usually cut from its title).  The frame of the game screen (round the
 * map, the party, food & gold and the messages) is then made of those
 * pieces in place of the DOS one, as are the frames of the full screen
 * pages (the module list, the books & the world map).
 */

#include <string.h>
#include "config.h"
#include "image.h"
#include "imagemgr.h"
#include "packframe.h"
#include "xu4.h"

static bool blank(uint32_t p)
{
    const uint8_t* c = (const uint8_t*) &p;
    return c[3] == 0 || (c[0] | c[1] | c[2]) == 0;
}

/*
 * Get the frame pieces of the graphics module.  Return false if it has
 * none (the DOS frame is used).
 */
bool packFrame(PackFrame* pf)
{
    static const char* names[PackFrame::PIECES] = {
        "frame_tl", "frame_t", "frame_tr", "frame_l", "frame_r",
        "frame_bl", "frame_b", "frame_br"
    };
    if (! xu4.imageMgr || ! xu4.config)
        return false;
    for (int i = 0; i < PackFrame::PIECES; ++i) {
        const SubImage* sub;
        ImageInfo* info = xu4.imageMgr->imageInfo(
                                xu4.config->intern(names[i]), &sub);
        if (! info || ! sub || ! info->image)
            return false;
        const Image* img = info->image;
        if (sub->x < 0 || sub->y < 0 || sub->x + 8 > img->width() ||
            sub->y + 8 > img->height())
            return false;
        const uint32_t* src = img->pixelData();
        for (int y = 0; y < 8; ++y)
            memcpy(pf->px[i] + y * 8,
                   src + (sub->y + y) * img->width() + sub->x, 8 * 4);
    }
    return true;
}

// Copy piece p (or a part of it) to cell x, y of the 320x200 image.
static void putPiece(uint32_t* dst, int cx, int cy, const uint32_t* p,
                     int x0 = 0, int y0 = 0, int x1 = 8, int y1 = 8,
                     bool over = false)
{
    for (int y = y0; y < y1; ++y) {
        uint32_t* row = dst + (cy * 8 + y) * 320 + cx * 8;
        for (int x = x0; x < x1; ++x) {
            uint32_t c = p[y * 8 + x];
            if (! over || ! blank(c))
                row[x] = c;
        }
    }
}

// The first & last rows (or columns) of a piece that are not blank.
static void inkRows(const uint32_t* p, int* first, int* last)
{
    *first = 8;
    *last = -1;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (! blank(p[y * 8 + x])) {
                if (y < *first) *first = y;
                if (y > *last)  *last = y;
            }
}

static void inkCols(const uint32_t* p, int* first, int* last)
{
    *first = 8;
    *last = -1;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (! blank(p[y * 8 + x])) {
                if (x < *first) *first = x;
                if (x > *last)  *last = x;
            }
}

/*
 * The DOS game screen in 8x8 cells: the map (left), the party (top right),
 * food & gold, and the messages (bottom right, which have no frame below
 * or to the right).  The gaps are for the moons (top) & the wind (bottom).
 *
 *   a b c d  corners (top left, top right, bottom left, bottom right)
 *   - _      top & bottom sides     [ ]  left & right sides
 *   T        a side joining from below (top row)
 *   >        a side going right from the middle column
 *   <        a side going left from the right column
 */
static const char* const screenCells[25] = {
    "a----------  ----------T---------------b",
    "[                      [               ]",
    "[                      [               ]",
    "[                      [               ]",
    "[                      [               ]",
    "[                      [               ]",
    "[                      [               ]",
    "[                      [               ]",
    "[                      [               ]",
    "[                      >_______________<",
    "[                      [               ]",
    "[                      >_______________d",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "[                      [                ",
    "c______          ______d                ",
    "                                        "
};

/*
 * Make the frame of the game screen (in place of the DOS start.ega) from
 * the pieces.
 */
Image* packFrameBorders(const PackFrame& pf)
{
    Image* img = Image::create(320, 200);
    if (! img)
        return NULL;
    img->fill(Image::black);
    uint32_t* dst = img->pixels;

    int first, last;
    for (int cy = 0; cy < 25; ++cy) {
        const char* row = screenCells[cy];
        for (int cx = 0; cx < 40; ++cx) {
            switch (row[cx]) {
            case 'a': putPiece(dst, cx, cy, pf.px[PackFrame::TL]); break;
            case 'b': putPiece(dst, cx, cy, pf.px[PackFrame::TR]); break;
            case 'c': putPiece(dst, cx, cy, pf.px[PackFrame::BL]); break;
            case 'd': putPiece(dst, cx, cy, pf.px[PackFrame::BR]); break;
            case '-': putPiece(dst, cx, cy, pf.px[PackFrame::T]);  break;
            case '_': putPiece(dst, cx, cy, pf.px[PackFrame::B]);  break;
            case '[': putPiece(dst, cx, cy, pf.px[PackFrame::L]);  break;
            case ']': putPiece(dst, cx, cy, pf.px[PackFrame::R]);  break;
            case 'T':
                // The top side, with the left side below its line.
                putPiece(dst, cx, cy, pf.px[PackFrame::T]);
                inkRows(pf.px[PackFrame::T], &first, &last);
                putPiece(dst, cx, cy, pf.px[PackFrame::L], 0, last + 1,
                         8, 8, true);
                break;
            case '>':
                // The left side, with the bottom side right of its line.
                putPiece(dst, cx, cy, pf.px[PackFrame::L]);
                inkCols(pf.px[PackFrame::L], &first, &last);
                putPiece(dst, cx, cy, pf.px[PackFrame::B], last + 1, 0,
                         8, 8, true);
                break;
            case '<':
                // The right side, with the bottom side left of its line.
                putPiece(dst, cx, cy, pf.px[PackFrame::R]);
                inkCols(pf.px[PackFrame::R], &first, &last);
                putPiece(dst, cx, cy, pf.px[PackFrame::B], 0, 0,
                         first, 8, true);
                break;
            }
        }
    }
    return img;
}

/*
 * Draw a frame of the pieces round the box x, y, w, h of an image of
 * dw x dh pixels, each piece scaled up by scale.  The inside of the box is
 * left as it is.
 */
void packFrameDraw(const PackFrame& pf, uint32_t* dst, int dw, int dh,
                   int x, int y, int w, int h, int scale)
{
    const int S = 8 * scale;
    if (w < 2 * S || h < 2 * S)
        return;

    auto put = [&](int px, int py, const uint32_t* p, int cw, int ch) {
        for (int j = 0; j < ch; ++j) {
            int ty = py + j;
            if (ty < 0 || ty >= dh)
                continue;
            for (int i = 0; i < cw; ++i) {
                int tx = px + i;
                if (tx >= 0 && tx < dw)
                    dst[ty * dw + tx] = p[(j / scale) * 8 + i / scale];
            }
        }
    };

    // The sides, the last piece cut short, then the corners over them.
    for (int i = S; i < w - S; i += S) {
        int cw = (i + S > w - S) ? w - S - i : S;
        put(x + i, y,         pf.px[PackFrame::T], cw, S);
        put(x + i, y + h - S, pf.px[PackFrame::B], cw, S);
    }
    for (int j = S; j < h - S; j += S) {
        int ch = (j + S > h - S) ? h - S - j : S;
        put(x,         y + j, pf.px[PackFrame::L], S, ch);
        put(x + w - S, y + j, pf.px[PackFrame::R], S, ch);
    }
    put(x,         y,         pf.px[PackFrame::TL], S, S);
    put(x + w - S, y,         pf.px[PackFrame::TR], S, S);
    put(x,         y + h - S, pf.px[PackFrame::BL], S, S);
    put(x + w - S, y + h - S, pf.px[PackFrame::BR], S, S);
}
