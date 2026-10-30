/*
 * XU4 - Full screen pages drawn like the game's own menus
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstring>
#include "textpage.h"
#include "gpu.h"
#include "image.h"
#include "imagemgr.h"
#include "screen.h"
#include "xu4.h"

static uint32_t pixel(uint8_t r, uint8_t g, uint8_t b)
{
    RGBA c;
    rgba_set(c, r, g, b, 255);
    uint32_t v;
    memcpy(&v, &c, 4);
    return v;
}

// These match the EGA colours of the title screen menus.
const uint32_t TextPage::BLACK   = pixel(  0,   0,   0);
const uint32_t TextPage::TEXT    = pixel(204, 204, 204);
const uint32_t TextPage::WHITE   = pixel(255, 255, 255);
const uint32_t TextPage::GREY    = pixel(170, 170, 170);
const uint32_t TextPage::DIM     = pixel( 85,  85,  85);
const uint32_t TextPage::BLUE    = pixel( 85,  85, 255);
const uint32_t TextPage::DK_BLUE = pixel(  0,   0, 170);
const uint32_t TextPage::CYAN    = pixel( 85, 255, 255);
const uint32_t TextPage::GREEN   = pixel( 85, 255,  85);
const uint32_t TextPage::YELLOW  = pixel(255, 255,  85);

TextPage::TextPage()
{
    image32_init(&img);
    charset = NULL;
    tex = 0;
    cellW = 8;
    dirty = true;
}

TextPage::~TextPage()
{
    release();
    image32_freePixels(&img);
}

/*
 * Prepare the page for drawing.  The cell width follows the character set
 * of the graphics module (the Apple II font is 7 pixels wide).
 */
bool TextPage::begin(int cols, int rows)
{
    ImageInfo* info = xu4.imageMgr->get(BKGD_CHARSET);
    charset = info ? info->image : NULL;
    cellW = charset ? charset->width() : 8;

    int w = cols * cellW;
    int h = rows * CELL_H;
    if (img.w != w || img.h != h) {
        release();
        image32_freePixels(&img);
        if (! image32_allocPixels(&img, w, h))
            return false;
    }
    clear();
    return true;
}

void TextPage::clear(uint32_t color)
{
    uint32_t* it  = img.pixels;
    uint32_t* end = it + img.w * img.h;
    while (it != end)
        *it++ = color;
    dirty = true;
}

void TextPage::fill(int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > img.w) w = img.w - x;
    if (y + h > img.h) h = img.h - y;
    for (int j = 0; j < h; ++j) {
        uint32_t* row = img.pixels + (y + j) * img.w + x;
        for (int i = 0; i < w; ++i)
            row[i] = color;
    }
    dirty = true;
}

/*
 * Draw the rounded blue frame of the title menus (with its thin grey inner
 * line) around a box.  The inside of the box is left as it is.
 */
void TextPage::frame(int x, int y, int w, int h, int thick)
{
    // Rows cut from the outer corners, from the top down in pixel pairs.
    static const uint8_t inset[9] = { 18, 12, 8, 6, 4, 4, 2, 2, 2 };
    int i;

    fill(x, y, w, thick, BLUE);
    fill(x, y + h - thick, w, thick, BLUE);
    fill(x, y, thick, h, BLUE);
    fill(x + w - thick, y, thick, h, BLUE);

    // Round the outer corners.
    for (i = 0; i < 9; ++i) {
        int n = inset[i];
        int t = y + i * 2;
        int b = y + h - 2 - i * 2;
        fill(x, t, n, 2, BLACK);
        fill(x + w - n, t, n, 2, BLACK);
        fill(x, b, n, 2, BLACK);
        fill(x + w - n, b, n, 2, BLACK);
    }

    // Grey line inside, which stops short of the corners.
    int in = thick;
    fill(x + in + 2, y + in, w - 2 * in - 4, 2, GREY);
    fill(x + in + 2, y + h - in - 2, w - 2 * in - 4, 2, GREY);
    fill(x + in, y + in + 2, 2, h - 2 * in - 4, GREY);
    fill(x + w - in - 2, y + in + 2, 2, h - 2 * in - 4, GREY);
}

/*
 * Draw characters from the game's character set.  Scale 1 is half the size
 * of the game's text, scale 2 is the same size.
 */
void TextPage::text(int x, int y, const char* str, int len, uint32_t color,
                    int scale)
{
    if (! charset)
        return;
    const uint32_t* src = charset->pixelData();
    int cw = charset->width();
    int chars = charset->height() / CELL_H;

    for (int n = 0; n < len; ++n, x += cw * scale) {
        int ch = (uint8_t) str[n];
        if (ch == ' ' || ch >= chars)
            continue;
        const uint32_t* glyph = src + ch * CELL_H * cw;
        for (int j = 0; j < CELL_H; ++j) {
            for (int i = 0; i < cw; ++i) {
                const uint8_t* p = (const uint8_t*) (glyph + j * cw + i);
                // Ink is any pixel brighter than the background.
                if (p[3] && (p[0] + p[1] + p[2]) > 96)
                    fill(x + i * scale, y + j * scale, scale, scale, color);
            }
        }
    }
    dirty = true;
}

void TextPage::text(int x, int y, const char* str, uint32_t color, int scale)
{
    text(x, y, str, strlen(str), color, scale);
}

void TextPage::centered(int y, const char* str, uint32_t color, int scale)
{
    int len = strlen(str);
    text((img.w - textWidth(len, scale)) / 2, y, str, len, color, scale);
}

/*
 * Copy the page to its texture.  Call before draw() whenever it changed.
 */
void TextPage::upload()
{
    if (! dirty || ! img.pixels)
        return;
    if (tex)
        gpu_blitTexture(tex, 0, 0, &img);
    else
        tex = gpu_makeTexture(&img);
    dirty = false;
}

void TextPage::draw(const ScreenState* ss)
{
    if (! tex)
        return;
    float rect[4];
    rect[0] = 0.0f;
    rect[1] = 0.0f;
    rect[2] = ss->aspectW;
    rect[3] = ss->aspectH;
    gpu_drawImage(xu4.gpu, tex, rect, ss->aspectW, ss->aspectH);
}

void TextPage::release()
{
    if (tex) {
        gpu_freeTexture(tex);
        tex = 0;
    }
    dirty = true;
}

void TextPage::pagePos(const ScreenState* ss, int guiX, int guiY,
                       int* px, int* py) const
{
    *px = ss->aspectW ? guiX * img.w / ss->aspectW : 0;
    *py = ss->aspectH ? (ss->aspectH - guiY) * img.h / ss->aspectH : 0;
}
