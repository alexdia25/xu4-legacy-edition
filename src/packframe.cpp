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
    // Black, or nearly (the NES keeps its black opaque as 8,8,8).
    const uint8_t* c = (const uint8_t*) &p;
    return c[3] == 0 || (c[0] < 16 && c[1] < 16 && c[2] < 16);
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
        bool across = (i == PackFrame::T || i == PackFrame::B);
        bool down   = (i == PackFrame::L || i == PackFrame::R);
        int n = ((across && sub->width >= 16) ||
                 (down && sub->height >= 16)) ? 2 : 1;
        int w = (across && n == 2) ? 16 : 8;
        int h = (down && n == 2) ? 16 : 8;
        if (sub->x < 0 || sub->y < 0 || sub->x + w > img->width() ||
            sub->y + h > img->height())
            return false;
        pf->cells[i] = n;
        const uint32_t* src = img->pixelData();
        for (int k = 0; k < n; ++k) {
            int x0 = sub->x + (across ? 8 * k : 0);
            int y0 = sub->y + (down ? 8 * k : 0);
            for (int y = 0; y < 8; ++y)
                memcpy(pf->px[i][k] + y * 8,
                       src + (y0 + y) * img->width() + x0, 8 * 4);
        }
        if (n == 1)
            memcpy(pf->px[i][1], pf->px[i][0], sizeof(pf->px[i][0]));
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

// True if every column of a piece is one colour from top to bottom.
static bool banded(const uint32_t* p)
{
    for (int x = 0; x < 8; ++x)
        for (int y = 1; y < 8; ++y)
            if (p[y * 8 + x] != p[x])
                return false;
    return true;
}

/*
 * The side between the map & the party list faces both.  It is made of
 * the right side's inner stripes, the outer stripe, then the left side's
 * inner stripes (so both edges look like the inside of the frame).  Sides
 * that are not plain stripes (the NES chain) take the left half of the
 * right side & the right half of the left side.
 */
static bool middleSide(const PackFrame& pf, uint32_t* both, int* outer0,
                       int* outer1)
{
    const uint32_t* l = pf.px[PackFrame::L][0];
    const uint32_t* r = pf.px[PackFrame::R][0];
    int lf, ll, rf, rl;
    inkCols(l, &lf, &ll);
    inkCols(r, &rf, &rl);
    if (! banded(l) || ! banded(r) || ll < 0 || rl < 0) {
        for (int i = 0; i < 64; ++i)
            both[i] = ((i & 7) < 4) ? r[i] : l[i];
        return false;
    }

    // The stripes (runs of one colour) from the map side to the party side.
    uint32_t col[24];
    int run[24], n = 0;
    auto add = [&](uint32_t c) {
        if (n && col[n - 1] == c)
            ++run[n - 1];
        else {
            col[n] = c;
            run[n++] = 1;
        }
    };
    int lo = lf;                        // The outer stripe of the left side.
    while (lo + 1 <= ll && l[lo + 1] == l[lf])
        ++lo;
    int ro = rl;                        // The outer stripe of the right side.
    while (ro - 1 >= rf && r[ro - 1] == r[rl])
        --ro;
    for (int x = rf; x < ro; ++x)
        add(r[x]);
    for (int x = lf; x <= lo; ++x)
        add(l[x]);
    for (int x = lo + 1; x <= ll; ++x)
        add(l[x]);

    // Narrow the widest stripes until they fit in a cell.
    int w = 0;
    for (int i = 0; i < n; ++i)
        w += run[i];
    while (w > 8) {
        int m = 0;
        for (int i = 1; i < n; ++i)
            if (run[i] > run[m] || (run[i] == run[m] && i == n / 2))
                m = i;
        if (run[m] < 2)
            break;
        --run[m];
        --w;
    }

    uint32_t line[8];
    for (int x = 0; x < 8; ++x)
        line[x] = 0xff000000;   // Opaque black (RGBA bytes).

    int x = (8 - w) / 2;
    for (int i = 0; i < n; ++i) {
        if (col[i] == l[lf])
            *outer0 = x;
        for (int k = 0; k < run[i] && x < 8; ++k)
            line[x++] = col[i];
        if (col[i] == l[lf])
            *outer1 = x - 1;
    }
    for (int i = 0; i < 64; ++i)
        both[i] = line[i & 7];
    return true;
}

/*
 * The DOS game screen in 8x8 cells: the map (left), the party (top right),
 * food & gold, and the messages (bottom right, which have no frame below
 * or to the right).  The gaps are for the moons (top) & the wind (bottom).
 *
 *   a b c d  corners (top left, top right, bottom left, bottom right)
 *   - _      top & bottom sides     [ ]  left & right sides
 *   |        the side between the map & the party, a left & right side
 *   T        a side joining from below (top row)
 *   >        a side going right from the middle column
 *   <        a side going left from the right column
 */
static const char* const screenCells[25] = {
    "a----------  ----------T---------------b",
    "[                      |               ]",
    "[                      |               ]",
    "[                      |               ]",
    "[                      |               ]",
    "[                      |               ]",
    "[                      |               ]",
    "[                      |               ]",
    "[                      |               ]",
    "[                      >_______________<",
    "[                      |               ]",
    "[                      >_______________d",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
    "[                      |                ",
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

    // Count the cells of each straight run, so that a side of two cells
    // starts with its first after a corner.
    int first, last;
    int across = 0, down[40];
    memset(down, 0, sizeof(down));
    // The side between the map & the party list faces both.
    uint32_t both[64];
    // Sides of plain stripes join at a T: the outer stripes meet & the
    // inner ones turn the corner.
    int mid0 = 0, mid1 = 7;
    bool stripes = middleSide(pf, both, &mid0, &mid1);
    const uint32_t* top = pf.px[PackFrame::T][0];
    const uint32_t* bot = pf.px[PackFrame::B][0];
    const uint32_t* rgt = pf.px[PackFrame::R][0];
    int rOuter = 8;             // Where the right side's outer stripe starts.
    if (stripes) {
        inkCols(rgt, &first, &last);
        rOuter = last;
        while (rOuter - 1 >= first && rgt[rOuter - 1] == rgt[last])
            --rOuter;
    }

    for (int cy = 0; cy < 25; ++cy) {
        const char* row = screenCells[cy];
        for (int cx = 0; cx < 40; ++cx) {
            char ch = row[cx];
            int a = across, d = down[cx];
            across  = (ch == '-' || ch == '_') ? across + 1 : 0;
            down[cx] = (ch == '[' || ch == ']') ? d + 1 : 0;
            switch (ch) {
            case 'a': putPiece(dst, cx, cy, pf.px[PackFrame::TL][0]); break;
            case 'b': putPiece(dst, cx, cy, pf.px[PackFrame::TR][0]); break;
            case 'c': putPiece(dst, cx, cy, pf.px[PackFrame::BL][0]); break;
            case 'd': putPiece(dst, cx, cy, pf.px[PackFrame::BR][0]); break;
            case '-': putPiece(dst, cx, cy, pf.side(PackFrame::T, a)); break;
            case '_': putPiece(dst, cx, cy, pf.side(PackFrame::B, a)); break;
            case '[': putPiece(dst, cx, cy, pf.side(PackFrame::L, d)); break;
            case '|': putPiece(dst, cx, cy, both); break;
            case ']': putPiece(dst, cx, cy, pf.side(PackFrame::R, d)); break;
            case 'T':
                if (stripes) {
                    // The middle side's stripes cross the top's inner ones.
                    putPiece(dst, cx, cy, top);
                    inkRows(top, &first, &last);
                    for (int y = 0; y < 8; ++y)
                        if (top[y * 8] != top[first * 8])
                            putPiece(dst, cx, cy, both, 0, y, 8, y + 1);
                    break;
                }
                // The top side, with the left side below its line.
                putPiece(dst, cx, cy, pf.px[PackFrame::T][0]);
                inkRows(pf.px[PackFrame::T][0], &first, &last);
                putPiece(dst, cx, cy, both, 0, last + 1, 8, 8, true);
                break;
            case '>':
                if (stripes) {
                    // The bottom side from the middle's outer stripe on.
                    putPiece(dst, cx, cy, both);
                    putPiece(dst, cx, cy, bot, mid1 + 1, 0, 8, 8);
                    break;
                }
                // The left side, with the bottom side right of its line.
                putPiece(dst, cx, cy, both);
                inkCols(both, &first, &last);
                putPiece(dst, cx, cy, pf.px[PackFrame::B][0], last + 1, 0,
                         8, 8, true);
                break;
            case '<':
                if (stripes) {
                    // The bottom side up to the right's outer stripe.
                    putPiece(dst, cx, cy, rgt);
                    putPiece(dst, cx, cy, bot, 0, 0, rOuter, 8);
                    break;
                }
                // The right side, with the bottom side left of its line.
                putPiece(dst, cx, cy, pf.px[PackFrame::R][0]);
                inkCols(pf.px[PackFrame::R][0], &first, &last);
                putPiece(dst, cx, cy, pf.px[PackFrame::B][0], 0, 0,
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
    for (int i = S, n = 0; i < w - S; i += S, ++n) {
        int cw = (i + S > w - S) ? w - S - i : S;
        put(x + i, y,         pf.side(PackFrame::T, n), cw, S);
        put(x + i, y + h - S, pf.side(PackFrame::B, n), cw, S);
    }
    for (int j = S, n = 0; j < h - S; j += S, ++n) {
        int ch = (j + S > h - S) ? h - S - j : S;
        put(x,         y + j, pf.side(PackFrame::L, n), S, ch);
        put(x + w - S, y + j, pf.side(PackFrame::R, n), S, ch);
    }
    put(x,         y,         pf.px[PackFrame::TL][0], S, S);
    put(x + w - S, y,         pf.px[PackFrame::TR][0], S, S);
    put(x,         y + h - S, pf.px[PackFrame::BL][0], S, S);
    put(x + w - S, y + h - S, pf.px[PackFrame::BR][0], S, S);
}
