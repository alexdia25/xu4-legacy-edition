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
 *             Each glyph is put in an 8x8 cell.
 *   a2hgr     A full 280x192 HGR screen in Apple memory order.
 *   a2spk     A 280x144 picture packed with run length & pattern codes,
 *             stored column by column from the bottom right.
 *
 * Colors follow the usual NTSC artifact rules: a lit pixel alone is violet
 * or green (blue or orange when the byte's high bit is set) by the parity of
 * its column, and adjacent lit pixels are white.
 */

#include <vector>

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

static Image* a2Font(const std::vector<uint8_t>& data)
{
    if (data.size() < 1024)
        return NULL;
    Image* img = Image::create(8, 8 * 256);
    img->fill(Image::black);
    for (int g = 0; g < 128; ++g) {
        for (int r = 0; r < 8; ++r) {
            uint8_t b = data[r * 128 + g];
            for (int i = 0; i < 7; ++i) {
                if ((b >> i) & 1)
                    img->putPixel(i, g * 8 + r, 255, 255, 255, IM_OPAQUE);
            }
        }
    }
    return img;
}

static Image* a2Screen(const uint8_t* page, int lines)
{
    std::vector<uint8_t> row;
    Image* img = Image::create(280, lines);
    for (int y = 0; y < lines; ++y) {
        a2DecodeRow(page + a2LineOffset(y), 40, 0, row);
        a2PutRow(img, 0, y, row);
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

static Image* a2Packed(const std::vector<uint8_t>& data)
{
    if (data.size() < 2)
        return NULL;
    std::vector<uint8_t> bytes;
    a2Unpack(data.data() + 2, data.data() + data.size(), data[0], data[1],
             bytes, 0);

    std::vector<uint8_t> page(8192, 0);
    size_t n = 0;
    for (int col = 39; col >= 0; --col) {
        for (int y = 143; y >= 0; --y) {
            if (n < bytes.size())
                page[a2LineOffset(y) + col] = bytes[n++];
        }
    }
    return a2Screen(page.data(), 144);
}

static Image* loadImage_apple2(U4FILE* file, int ftype)
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
    }
    return NULL;
}
