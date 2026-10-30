/*
 * XU4 - Full screen pages drawn like the game's own menus
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TEXTPAGE_H
#define TEXTPAGE_H

#include <stdint.h>
#include "image32.h"

class Image;
struct ScreenState;

/*
 * An 80 column by 50 row page (twice the resolution of the game screen)
 * drawn with the game's character set inside the blue frame of the title
 * menus.  Text can be drawn at full size (the size of the game's own
 * text) or half size, which fits the printed documents across the page.
 */
class TextPage {
public:
    enum {
        COLS = 80,
        ROWS = 50,
        CELL_H = 8
    };

    // Colours of the title menus.
    static const uint32_t BLACK;
    static const uint32_t TEXT;         // Light grey of the menu text.
    static const uint32_t WHITE;
    static const uint32_t GREY;         // Inner frame line & bullets.
    static const uint32_t DIM;          // Dark grey for unavailable items.
    static const uint32_t BLUE;         // The frame & cursor.
    static const uint32_t DK_BLUE;      // Highlight background.
    static const uint32_t CYAN;
    static const uint32_t GREEN;
    static const uint32_t YELLOW;

    TextPage();
    ~TextPage();

    bool begin();
    void clear(uint32_t color = BLACK);
    void fill(int x, int y, int w, int h, uint32_t color);
    void frame(int x, int y, int w, int h, int thick = 8);
    void text(int x, int y, const char* str, int len, uint32_t color,
              int scale = 1);
    void text(int x, int y, const char* str, uint32_t color, int scale = 1);
    void textCells(int col, int row, const char* str, uint32_t color) {
        text(col * cellW, row * CELL_H, str, color, 1);
    }
    void centered(int y, const char* str, uint32_t color, int scale = 1);
    int  textWidth(int len, int scale = 1) const { return len * cellW * scale; }

    void upload();
    void draw(const ScreenState* ss);
    void release();

    // Convert a pointer event position (GUI pixels) to page pixels.
    void pagePos(const ScreenState* ss, int guiX, int guiY,
                 int* px, int* py) const;

    int width() const  { return img.w; }
    int height() const { return img.h; }
    int cellWidth() const { return cellW; }

private:
    Image32 img;
    const Image* charset;
    uint32_t tex;
    int cellW;
    bool dirty;
};

#endif
