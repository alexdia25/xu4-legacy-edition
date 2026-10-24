/*
 * XU4 - Commodore 64 image formats
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Decodes the graphics of Ultima IV for Commodore 64:
 *
 *   c64tiles  256 map tiles, 16x16 pixels in hi-res mode.  The data is the
 *             tile file (the top halves of all tiles, 16 bytes each, then
 *             the bottom halves) followed by 256 color bytes, one for each
 *             tile (foreground in the high nybble, background in the low).
 *             Ultima IV Remastered has four tables of 256 colors, one for
 *             each 8x8 cell (top left, top right, bottom left, bottom
 *             right).  The four field tiles are filled with random pixels
 *             by the original game as they are drawn; one such frame is
 *             made here.
 *   c64font   128 text glyphs, 8x8.  The printable characters (32-127)
 *             replace those of the DOS charset.ega, which keeps the symbols
 *             the game uses for its own glyphs 0-31, except for the cursor
 *             (28-31), which becomes the C64 one (124-127).
 *   c64hires  A hi-res bitmap of 24 character rows, optionally followed by
 *             its screen colors.  Without colors it is white on black.
 *   c64multi  A multicolor picture of the intro: 5888 bytes of bitmap
 *             (40 cells wide), 768 bytes of screen colors & 768 bytes of
 *             color RAM.  The background is black.
 *   c64tree   The c64multi picture of the hills & tree, with the moongate
 *             (drawn by the game with two blue sprites) and the ground
 *             it rises from put below the picture.
 *   c64cards  The eight virtue cards, 80x112 pixels each, in multicolor
 *             mode: the card bitmaps, their screen colors (144 bytes per
 *             card) and the color RAM of the abacus picture they are drawn
 *             over.  The cards are placed side by side.
 *   c64rcards The cards of Ultima IV Remastered: as c64cards, but the
 *             color RAM is a color for each row of a card (16 bytes per
 *             card) at offset $480 of the screen colors.
 *   c64beads  The two abacus beads (14 bytes of multicolor bitmap each)
 *             followed by the abacus picture whose colors they use.  The
 *             result has a bead for each of the 8 columns, the white
 *             (blue on the C64) beads above the black ones.
 *
 * Screens & pictures are put at the top left of a 320x200 image.
 */

#include <string.h>
#include <vector>
#include "c64disk.h"

static const uint8_t c64Palette[16][3] = {
    {   0,   0,   0 },  // Black
    { 255, 255, 255 },  // White
    { 104,  55,  43 },  // Red
    { 112, 164, 178 },  // Cyan
    { 111,  61, 134 },  // Purple
    {  88, 141,  67 },  // Green
    {  53,  40, 121 },  // Blue
    { 184, 199, 111 },  // Yellow
    { 111,  79,  37 },  // Orange
    {  67,  57,   0 },  // Brown
    { 154, 103,  89 },  // Light red
    {  68,  68,  68 },  // Dark grey
    { 108, 108, 108 },  // Grey
    { 154, 210, 132 },  // Light green
    { 108,  94, 181 },  // Light blue
    { 149, 149, 149 }   // Light grey
};

#define C64_COLS        40
#define C64_HIRES_ROWS  24
#define C64_TILE_COUNT  256
#define C64_CARD_W      10      // Cells
#define C64_CARD_H      14

static void c64Put(Image* img, int x, int y, int color)
{
    const uint8_t* c = c64Palette[color & 15];
    img->putPixel(x, y, c[0], c[1], c[2], IM_OPAQUE);
}

/*
 * Draw an 8x8 hi-res cell.  Set bits use the high nybble of the color.
 */
static void c64HiresCell(Image* img, int x, int y, const uint8_t* bits,
                         int color)
{
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int on = bits[r] & (0x80 >> c);
            c64Put(img, x + c, y + r, on ? color >> 4 : color);
        }
    }
}

/*
 * Draw rows of an 8x8 multicolor cell, two pixels wide each.
 */
static void c64MultiCell(Image* img, int x, int y, const uint8_t* bits,
                         int rows, int screen, int cram, int bg)
{
    const int colors[4] = { bg, screen >> 4, screen & 15, cram & 15 };
    for (int r = 0; r < rows; ++r) {
        for (int p = 0; p < 4; ++p) {
            int color = colors[(bits[r] >> (6 - 2*p)) & 3];
            c64Put(img, x + p*2,     y + r, color);
            c64Put(img, x + p*2 + 1, y + r, color);
        }
    }
}

static Image* c64Tiles(std::vector<uint8_t>& data)
{
    if (data.size() < 0x2000 + C64_TILE_COUNT)
        return NULL;
    bool cellColors = (data.size() >= 0x2000 + 4 * C64_TILE_COUNT);

    // A frame of the random pixels of the poison, energy, fire & sleep
    // fields (tiles 68-71).
    uint32_t seed = 0x2F6B;
    for (int t = 68; t < 72 && ! cellColors; ++t) {
        for (int half = 0; half < 2; ++half) {
            uint8_t* it = data.data() + half * 0x1000 + t * 16;
            for (int i = 0; i < 16; ++i) {
                seed = seed * 1103515245 + 12345;
                it[i] = uint8_t(seed >> 16);
            }
        }
    }

    const uint8_t* colors = data.data() + 0x2000;
    const int next = cellColors ? C64_TILE_COUNT : 0;
    Image* img = Image::create(16, 16 * C64_TILE_COUNT);
    for (int t = 0; t < C64_TILE_COUNT; ++t) {
        const uint8_t* top = data.data() + t * 16;
        const uint8_t* bot = top + 0x1000;
        const uint8_t* col = colors + t;
        int y = t * 16;
        c64HiresCell(img, 0, y,     top,     col[0]);
        c64HiresCell(img, 8, y,     top + 8, col[next]);
        c64HiresCell(img, 0, y + 8, bot,     col[next * 2]);
        c64HiresCell(img, 8, y + 8, bot + 8, col[next * 3]);
    }
    return img;
}

static void c64PutGlyph(Image* img, const uint8_t* bits, int dest)
{
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (bits[r] & (0x80 >> c))
                img->putPixel(c, dest * 8 + r, 255, 255, 255, IM_OPAQUE);
        }
    }
}

static Image* c64Font(const std::vector<uint8_t>& data)
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
        c64PutGlyph(img, &data[g * 8], g);

    // The C64 cursor (glyphs 124-127) also replaces the DOS cursor (28-31).
    img->fillRect(0, 28 * 8, 8, 4 * 8, 0, 0, 0);
    for (int g = 0; g < 4; ++g)
        c64PutGlyph(img, &data[(124 + g) * 8], 28 + g);
    return img;
}

static Image* c64Hires(const std::vector<uint8_t>& data)
{
    const size_t bitmapSize = C64_COLS * C64_HIRES_ROWS * 8;
    if (data.size() < bitmapSize)
        return NULL;
    const uint8_t* screen = NULL;
    if (data.size() >= bitmapSize + C64_COLS * C64_HIRES_ROWS)
        screen = data.data() + bitmapSize;

    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (int i = 0; i < C64_COLS * C64_HIRES_ROWS; ++i) {
        c64HiresCell(img, (i % C64_COLS) * 8, (i / C64_COLS) * 8,
                     &data[i * 8], screen ? screen[i] : 0x10);
    }
    return img;
}

#define C64_MULTI_BITMAP    5888
#define C64_MULTI_SCREEN    768
#define C64_MULTI_SIZE      (C64_MULTI_BITMAP + 2 * C64_MULTI_SCREEN)

static Image* c64Multi(const std::vector<uint8_t>& data)
{
    if (data.size() < C64_MULTI_SIZE)
        return NULL;
    const uint8_t* screen = data.data() + C64_MULTI_BITMAP;
    const uint8_t* cram = screen + C64_MULTI_SCREEN;

    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (int i = 0; i < C64_MULTI_BITMAP / 8; ++i) {
        c64MultiCell(img, (i % C64_COLS) * 8, (i / C64_COLS) * 8,
                     &data[i * 8], 8, screen[i], cram[i], 0);
    }
    return img;
}

/*
 * The moongate rises from the hill in front of the tree.  It is two blue
 * sprites, 24 pixels wide & 27 lines in all, with every other pixel set.
 * The gate is put at 0,152 and the plain ground at 24,152 (see the tree
 * image of the module).
 */
#define C64_GATE_X  78
#define C64_GATE_Y  51
#define C64_GATE_W  24
#define C64_GATE_H  27

static Image* c64Tree(const std::vector<uint8_t>& data)
{
    Image* img = c64Multi(data);
    if (img) {
        img->drawSubRectOn(img, 0, 152, C64_GATE_X, C64_GATE_Y,
                           C64_GATE_W, C64_GATE_H);
        img->drawSubRectOn(img, 24, 152, C64_GATE_X, C64_GATE_Y,
                           C64_GATE_W, C64_GATE_H);
        for (int y = 0; y < C64_GATE_H; ++y) {
            for (int x = 1; x < C64_GATE_W; x += 2)
                c64Put(img, x, 152 + y, 6);
        }
    }
    return img;
}

/*
 * The cards are drawn at screen column 2 (left) or 28 (right), rows 2-15,
 * so their color RAM is that of the abacus picture there.
 */
static Image* c64Cards(const std::vector<uint8_t>& data)
{
    const int cells = C64_CARD_W * C64_CARD_H;
    const size_t bitmapSize = 8 * cells * 8;
    if (data.size() < bitmapSize + 0x500 + C64_MULTI_SCREEN)
        return NULL;
    const uint8_t* screen = data.data() + bitmapSize;
    const uint8_t* cram = screen + 0x500;

    Image* img = Image::create(8 * C64_CARD_W * 8, C64_CARD_H * 8);
    for (int card = 0; card < 8; ++card) {
        const uint8_t* bitmap = data.data() + card * cells * 8;
        int col = (card & 1) ? 28 : 2;
        for (int i = 0; i < cells; ++i) {
            int cx = i % C64_CARD_W;
            int cy = i / C64_CARD_W;
            c64MultiCell(img, (card * C64_CARD_W + cx) * 8, cy * 8,
                         bitmap + i * 8, 8, screen[card * 144 + i],
                         cram[(cy + 2) * C64_COLS + col + cx], 0);
        }
    }
    return img;
}

static Image* c64RowCards(const std::vector<uint8_t>& data)
{
    const int cells = C64_CARD_W * C64_CARD_H;
    const size_t bitmapSize = 8 * cells * 8;
    if (data.size() < bitmapSize + 0x480 + 8 * 16)
        return NULL;
    const uint8_t* screen = data.data() + bitmapSize;
    const uint8_t* rowColor = screen + 0x480;

    Image* img = Image::create(8 * C64_CARD_W * 8, C64_CARD_H * 8);
    for (int card = 0; card < 8; ++card) {
        const uint8_t* bitmap = data.data() + card * cells * 8;
        for (int i = 0; i < cells; ++i) {
            int cx = i % C64_CARD_W;
            int cy = i / C64_CARD_W;
            c64MultiCell(img, (card * C64_CARD_W + cx) * 8, cy * 8,
                         bitmap + i * 8, 8, screen[card * 144 + i],
                         rowColor[card * 16 + cy], 0);
        }
    }
    return img;
}

/*
 * The beads are drawn in character columns 16-23 from line 17 down.
 */
static Image* c64Beads(const std::vector<uint8_t>& data)
{
    if (data.size() < 28 + C64_MULTI_SIZE)
        return NULL;
    const uint8_t* screen = data.data() + 28 + C64_MULTI_BITMAP;
    const uint8_t* cram = screen + C64_MULTI_SCREEN;

    Image* img = Image::create(8 * 8, 2 * 14);
    for (int bead = 0; bead < 2; ++bead) {
        for (int col = 0; col < 8; ++col) {
            for (int r = 0; r < 14; ++r) {
                int cell = ((17 + r) / 8) * C64_COLS + 16 + col;
                c64MultiCell(img, col * 8, bead * 14 + r, &data[bead * 14 + r],
                             1, screen[cell], cram[cell], 0);
            }
        }
    }
    return img;
}

static Image* loadImage_c64(U4FILE* file, int ftype)
{
    std::vector<uint8_t> data(file->length());
    data.resize(file->read(data.data(), 1, data.size()));

    switch (ftype) {
        case FTYPE_C64TILES:
            return c64Tiles(data);
        case FTYPE_C64FONT:
            return c64Font(data);
        case FTYPE_C64HIRES:
            return c64Hires(data);
        case FTYPE_C64MULTI:
            return c64Multi(data);
        case FTYPE_C64TREE:
            return c64Tree(data);
        case FTYPE_C64CARDS:
            return c64Cards(data);
        case FTYPE_C64RCARDS:
            return c64RowCards(data);
        case FTYPE_C64BEADS:
            return c64Beads(data);
    }
    return NULL;
}
