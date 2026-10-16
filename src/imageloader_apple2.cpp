/*
 * XU4 - Apple II image formats
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Decodes the hi-res (HGR) graphics of Ultima IV for Apple II:
 *
 *   a2shapes  256 map tiles, 14x16 pixels.  The data is the SHP0 file (left
 *             byte of each tile) followed by SHP1 (right byte).  Each file
 *             holds row 0 of all tiles, then row 1, and so on.
 *   a2font    128 text glyphs, 7x8 pixels, stored row by row like shapes.
 *             Each glyph is put in an 8x8 cell.  The printable characters
 *             (32-127) replace those of the DOS charset.ega, which keeps the
 *             symbols the game uses for its own glyphs 0-31, except for
 *             the cursor (28-31), which becomes the Apple one.
 *   a2hgr     A full 280x192 HGR screen in Apple memory order.
 *   a2spk     A 280x144 picture packed with run length & pattern codes,
 *             stored column by column from the bottom right.
 *   a2cards   The eight 70x112 virtue cards (row by row, 10 bytes per row),
 *             placed side by side.
 *   a2beads   The four abacus bead shapes (white & black, for even & odd
 *             columns, 14 bytes each) followed by the packed abacus picture.
 *   a2view    One or more 154x176 pictures of the map view, packed like
 *             a2spk.  Each is drawn over the ones before it, as the game
 *             builds up the codex symbol one answer at a time.
 *   a2reveal  The 154x176 picture uncovered when the codex opens.  It is
 *             stored as pairs of columns from the middle outwards, each
 *             column from the bottom up.
 *
 * View pictures are put in the map area of a 320x200 image.
 *
 * Screens & pictures are put at the top center of a 320x200 image (x 20)
 * so that they line up with the 320x200 layout of the DOS screens.
 *
 * Colors follow the usual NTSC artifact rules: a lit pixel alone is violet
 * or green (blue or orange when the byte's high bit is set) by the parity of
 * its column, and adjacent lit pixels are white.
 */

#include <string.h>
#include <vector>
#include "a2disk.h"

enum A2Color {
    A2_BLACK, A2_WHITE, A2_VIOLET, A2_GREEN, A2_BLUE, A2_ORANGE
};

static const uint8_t a2Palette[6][3] = {
    {   0,   0,   0 },
    { 255, 255, 255 },
    { 255,  68, 253 },
    {  20, 245,  60 },
    {  20, 207, 253 },
    { 255, 106,  60 }
};

static int a2ColorOf(int x, int highBit)
{
    if (highBit)
        return (x & 1) ? A2_ORANGE : A2_BLUE;
    return (x & 1) ? A2_GREEN : A2_VIOLET;
}

/*
 * Decode a run of HGR bytes into colors.  startCol is the screen byte column
 * of the first byte, which decides the color parity.
 */
static void a2DecodeRow(const uint8_t* bytes, int count, int startCol,
                        std::vector<uint8_t>& out)
{
    int n = count * 7;
    std::vector<uint8_t> bit(n), high(n);
    for (int i = 0; i < n; ++i) {
        uint8_t b = bytes[i / 7];
        bit[i]  = (b >> (i % 7)) & 1;
        high[i] = b >> 7;
    }

    out.resize(n);
    int x0 = startCol * 7;
    for (int i = 0; i < n; ++i) {
        int left  = (i > 0) ? bit[i-1] : 0;
        int right = (i + 1 < n) ? bit[i+1] : 0;
        if (bit[i]) {
            out[i] = (left || right) ? A2_WHITE : a2ColorOf(x0 + i, high[i]);
        } else if (left && right &&
                   ! (i > 1 && bit[i-2]) && ! (i + 2 < n && bit[i+2])) {
            // A gap between two lone pixels takes their color.
            out[i] = a2ColorOf(x0 + i - 1, high[i-1]);
        } else {
            out[i] = A2_BLACK;
        }
    }
}

static void a2PutRow(Image* img, int x, int y, const std::vector<uint8_t>& row)
{
    for (size_t i = 0; i < row.size(); ++i) {
        const uint8_t* c = a2Palette[ row[i] ];
        img->putPixel(x + i, y, c[0], c[1], c[2], IM_OPAQUE);
    }
}

// Byte offset of each HGR scan line.
static int a2LineOffset(int y)
{
    return (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28;
}

static Image* a2Shapes(const std::vector<uint8_t>& data)
{
    if (data.size() < 8192)
        return NULL;
    const uint8_t* left  = data.data();
    const uint8_t* right = left + 4096;
    std::vector<uint8_t> row;
    uint8_t pair[2];

    Image* img = Image::create(14, 16 * 256);
    for (int t = 0; t < 256; ++t) {
        for (int r = 0; r < 16; ++r) {
            pair[0] = left [r * 256 + t];
            pair[1] = right[r * 256 + t];
            // Tiles are drawn with the left byte in an odd byte column.
            a2DecodeRow(pair, 2, 1, row);
            a2PutRow(img, 0, t * 16 + r, row);
        }
    }
    return img;
}

static void a2PutGlyph(Image* img, const std::vector<uint8_t>& data,
                       int glyph, int dest)
{
    for (int r = 0; r < 8; ++r) {
        uint8_t b = data[r * 128 + glyph];
        for (int i = 0; i < 7; ++i) {
            if ((b >> i) & 1)
                img->putPixel(i, dest * 8 + r, 255, 255, 255, IM_OPAQUE);
        }
    }
}

static Image* a2Font(const std::vector<uint8_t>& data)
{
    if (data.size() < 1024)
        return NULL;

    int first = 0;
    Image* img = NULL;
    U4FILE* dos = u4fopen("charset.ega");
    if (dos) {
        img = loadImage_u4(dos, FTYPE_U4RAW, 8, 8 * 256, 4);
        u4fclose(dos);
    }
    if (img) {
        first = ' ';
        img->fillRect(0, first * 8, 8, (128 - first) * 8, 0, 0, 0);
    } else {
        img = Image::create(8, 8 * 256);
        img->fill(Image::black);
    }

    for (int g = first; g < 128; ++g)
        a2PutGlyph(img, data, g, g);

    // The Apple cursor (glyphs 124-127) also replaces the DOS cursor (28-31).
    img->fillRect(0, 28 * 8, 8, 4 * 8, 0, 0, 0);
    for (int g = 0; g < 4; ++g)
        a2PutGlyph(img, data, 124 + g, 28 + g);
    return img;
}

#define A2_SCREEN_X    20

/*
 * Draw HGR rows of 40 bytes each (in display order, not Apple memory order)
 * onto an image.  Only screen pixels x0 to x1 (inclusive) are drawn, starting
 * at dx, dy.
 */
void a2_drawHgr(Image* dest, int dx, int dy, const uint8_t* rows,
                int count, int x0, int x1)
{
    std::vector<uint8_t> row;
    for (int y = 0; y < count; ++y) {
        a2DecodeRow(rows + y * 40, 40, 0, row);
        for (int x = x0; x <= x1; ++x) {
            const uint8_t* c = a2Palette[ row[x] ];
            dest->putPixel(dx + x - x0, dy + y, c[0], c[1], c[2], IM_OPAQUE);
        }
    }
}

static Image* a2Screen(const uint8_t* page, int lines)
{
    std::vector<uint8_t> row;
    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (int y = 0; y < lines; ++y) {
        a2DecodeRow(page + a2LineOffset(y), 40, 0, row);
        a2PutRow(img, A2_SCREEN_X, y, row);
    }
    return img;
}

#define A2_CARD_COLS    10
#define A2_CARD_ROWS    112

static Image* a2Cards(const std::vector<uint8_t>& data)
{
    const int cardSize = A2_CARD_COLS * A2_CARD_ROWS;
    if (data.size() < size_t(cardSize * 8))
        return NULL;
    std::vector<uint8_t> row;
    Image* img = Image::create(8 * A2_CARD_COLS * 7, A2_CARD_ROWS);
    for (int c = 0; c < 8; ++c) {
        const uint8_t* card = data.data() + c * cardSize;
        for (int y = 0; y < A2_CARD_ROWS; ++y) {
            // Cards are drawn starting at an even screen column.
            a2DecodeRow(card + y * A2_CARD_COLS, A2_CARD_COLS, 0, row);
            a2PutRow(img, c * A2_CARD_COLS * 7, y, row);
        }
    }
    return img;
}

/*
 * Unpack a picture.  The first two bytes are the codes for a run
 * (code count byte) and a pattern (code length count bytes...).
 */
static void a2Unpack(const uint8_t* it, const uint8_t* end, int runCode,
                     int patCode, std::vector<uint8_t>& out, int depth)
{
    while (it < end && depth < 8) {
        int code = *it++;
        if (code == runCode && end - it >= 2) {
            out.insert(out.end(), it[0], it[1]);
            it += 2;
        } else if (code == patCode && end - it >= 2) {
            int len   = it[0];
            int count = it[1];
            it += 2;
            if (len > end - it)
                len = end - it;
            for (int i = 0; i < count; ++i)
                a2Unpack(it, it + len, runCode, patCode, out, depth + 1);
            it += len;
        } else {
            out.push_back(code);
        }
    }
}

// Unpack a picture into an HGR page.
static bool a2UnpackPage(const uint8_t* data, size_t size,
                         std::vector<uint8_t>& page)
{
    if (size < 2)
        return false;
    std::vector<uint8_t> bytes;
    a2Unpack(data + 2, data + size, data[0], data[1], bytes, 0);

    page.assign(8192, 0);
    size_t n = 0;
    for (int col = 39; col >= 0; --col) {
        for (int y = 143; y >= 0; --y) {
            if (n < bytes.size())
                page[a2LineOffset(y) + col] = bytes[n++];
        }
    }
    return true;
}

static Image* a2Packed(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> page;
    if (! a2UnpackPage(data.data(), data.size(), page))
        return NULL;
    return a2Screen(page.data(), 144);
}

#define A2_BEAD_ROWS    14
#define A2_BEAD_BYTES   (4 * A2_BEAD_ROWS)

/*
 * Draw the abacus beads into the abacus picture which follows the bead data,
 * so that their edges take the colors of the picture around them.  The
 * result has a white bead for each of the 8 columns, with the black beads
 * below.
 */
static Image* a2Beads(const std::vector<uint8_t>& data)
{
    const int firstCol = 0x10;
    const int top = 17;
    std::vector<uint8_t> page, row;
    if (data.size() < A2_BEAD_BYTES ||
        ! a2UnpackPage(data.data() + A2_BEAD_BYTES,
                       data.size() - A2_BEAD_BYTES, page))
        return NULL;

    Image* img = Image::create(8 * 7, 2 * A2_BEAD_ROWS);
    for (int black = 0; black < 2; ++black) {
        for (int v = 0; v < 8; ++v) {
            int col = firstCol + v;
            // White even, white odd, black even, black odd.
            const uint8_t* bead = data.data() +
                                  (black * 2 + (col & 1)) * A2_BEAD_ROWS;
            for (int y = 0; y < A2_BEAD_ROWS; ++y) {
                uint8_t line[3];
                memcpy(line, &page[a2LineOffset(top + y) + col - 1], 3);
                line[1] = bead[y];
                a2DecodeRow(line, 3, col - 1, row);
                row.erase(row.begin(), row.begin() + 7);
                row.resize(7);
                a2PutRow(img, v * 7, black * A2_BEAD_ROWS + y, row);
            }
        }
    }
    return img;
}

#define A2_VIEW_COLS    22
#define A2_VIEW_ROWS    176
#define A2_VIEW_TOP     8
#define A2_VIEW_X       19      // Centered in the 176x176 map area.

/*
 * Unpack one view picture (codes as for a2Unpack) and return a pointer to
 * the data after it.
 */
static const uint8_t* a2UnpackView(const uint8_t* it, const uint8_t* end,
                                   std::vector<uint8_t>& out)
{
    const size_t size = A2_VIEW_COLS * A2_VIEW_ROWS;
    out.clear();
    if (end - it < 2)
        return end;
    int runCode = it[0];
    int patCode = it[1];
    it += 2;
    while (it < end && out.size() < size) {
        int code = *it++;
        if (code == runCode && end - it >= 2) {
            out.insert(out.end(), it[0], it[1]);
            it += 2;
        } else if (code == patCode && end - it >= 2) {
            int len   = it[0];
            int count = it[1];
            it += 2;
            if (len > end - it)
                len = end - it;
            std::vector<uint8_t> pat;
            a2Unpack(it, it + len, runCode, patCode, pat, 1);
            for (int i = 0; i < count; ++i)
                out.insert(out.end(), pat.begin(), pat.end());
            it += len;
        } else {
            out.push_back(code);
        }
    }
    out.resize(size, 0);
    return it;
}

// Put the view area of an HGR page (Apple x 7-160, y 8-183) in a 320x200 image.
static Image* a2ViewImage(const uint8_t* page)
{
    std::vector<uint8_t> row;
    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (int y = 0; y < A2_VIEW_ROWS; ++y) {
        a2DecodeRow(page + a2LineOffset(A2_VIEW_TOP + y), 40, 0, row);
        row.erase(row.begin(), row.begin() + 7);
        row.resize(A2_VIEW_COLS * 7);
        a2PutRow(img, A2_VIEW_X, A2_VIEW_TOP + y, row);
    }
    return img;
}

static Image* a2View(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> page(8192, 0), bytes;
    const uint8_t* it  = data.data();
    const uint8_t* end = it + data.size();
    while (end - it > 2) {
        it = a2UnpackView(it, end, bytes);
        size_t n = 0;
        for (int col = A2_VIEW_COLS; col > 0; --col) {
            for (int y = A2_VIEW_ROWS - 1; y >= 0; --y)
                page[a2LineOffset(A2_VIEW_TOP + y) + col] |= bytes[n++];
        }
    }
    return a2ViewImage(page.data());
}

static Image* a2Reveal(const std::vector<uint8_t>& data)
{
    const int half = A2_VIEW_COLS / 2;
    if (data.size() < size_t(A2_VIEW_COLS * A2_VIEW_ROWS))
        return NULL;
    std::vector<uint8_t> page(8192, 0);
    const uint8_t* it = data.data();
    for (int s = 0; s < half; ++s) {
        // The last pair is the outer edges.
        int left  = (s < half - 1) ? half - s : 1;
        int right = (s < half - 1) ? half + 1 + s : A2_VIEW_COLS;
        for (int y = A2_VIEW_ROWS - 1; y >= 0; --y) {
            int line = a2LineOffset(A2_VIEW_TOP + y);
            page[line + left]  = *it++;
            page[line + right] = *it++;
        }
    }
    return a2ViewImage(page.data());
}

static Image* loadImage_apple2(U4FILE* file, int ftype, int width)
{
    std::vector<uint8_t> data(file->length());
    data.resize(file->read(data.data(), 1, data.size()));

    switch (ftype) {
        case FTYPE_A2SHAPES:
            return a2Shapes(data);
        case FTYPE_A2FONT:
            return a2Font(data);
        case FTYPE_A2HGR:
            data.resize(8192, 0);
            return a2Screen(data.data(), 192);
        case FTYPE_A2SPK:
            return a2Packed(data);
        case FTYPE_A2CARDS:
            return a2Cards(data);
        case FTYPE_A2BEADS:
            return a2Beads(data);
        case FTYPE_A2VIEW:
            return a2View(data);
        case FTYPE_A2REVEAL:
            return a2Reveal(data);
    }
    return NULL;
}
