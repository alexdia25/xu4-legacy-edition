/*
 * XU4 - Ultima IV document reader
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Shows the History, Wisdom, Hints & Keyboard documents so that they can be
 * read without leaving the game.  The text keeps the layout of the original
 * fixed width files: runs of spaces are aligned to character columns while
 * words are drawn with the proportional GUI font.
 */

#include <math.h>
#include <cstring>
#include <algorithm>

#include "docreader.h"
#include "event.h"
#include "gamepad.h"
#include "image32.h"
#include "gpu.h"
#include "screen.h"
#include "settings.h"
#include "sound.h"
#include "xu4.h"

using std::string;

#define ATTR_COUNT  7
#define QUAD_ATTR   (6 * ATTR_COUNT)
#define MIN_COLUMNS 60      // Narrowest text area in columns.
#define MAX_FONT_SIZE 0.045f    // Largest text, as a fraction of the height.

enum ReaderRegions {
    REGION_FRAME,
    REGION_TEXT,
    REGION_BAR
};

static const int regionQuads[3] = { 200, 2400, 4 };

static inline bool insideArea(const GuiArea* area, int x, int y)
{
    return (x >= area->x && x < area->x2 &&
            y >= area->y && y < area->y2);
}

static void setArea(GuiArea* area, float x, float y, float x2, float y2,
                    int wid)
{
    area->x  = (uint16_t) x;
    area->y  = (uint16_t) y;
    area->x2 = (uint16_t) x2;
    area->y2 = (uint16_t) y2;
    area->wid = wid;
}

static float* emitArea(float* attr, const GuiArea* area, float colorIndex)
{
    float rect[4];
    rect[0] = area->x;
    rect[1] = area->y;
    rect[2] = area->x2 - area->x;
    rect[3] = area->y2 - area->y;
    return gui_emitQuadCi(attr, rect, colorIndex);
}

// Emit text centered in an area.
static float* emitCentered(float* attr, TxfDrawState* ds, const GuiArea* area,
                           const char* text, float colorIndex)
{
    int len = strlen(text);
    float w = txf_emWidth(ds->tf, (const uint8_t*) text, len) * ds->psize;
    float h = area->y2 - area->y;
    ds->x = ds->marginL = (area->x + area->x2 - w) * 0.5f;
    ds->y = area->y + (h - ds->psize * 0.7f) * 0.5f;
    ds->colorIndex = colorIndex;
    return gui_emitText(ds, attr, text, len);
}

/*
 * Split text into lines, removing carriage returns and expanding tabs.
 */
static void splitLines(const string& text, std::vector<string>& lines)
{
    string line;
    lines.clear();
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == '\n') {
            lines.push_back(line);
            line.clear();
        } else if (ch == '\t') {
            line.append(8 - (line.size() & 7), ' ');
        } else if (ch != '\r') {
            line.push_back(ch);
        }
    }
    if (! line.empty())
        lines.push_back(line);
    while (! lines.empty() &&
           lines.back().find_first_not_of(' ') == string::npos)
        lines.pop_back();
}

/*
 * Call func for each segment of a line, with the segment X position in
 * units of the font size.  Runs of three or more spaces (and indents) align
 * to a column so that tables keep their layout.  Return the line width.
 */
template<typename Func>
static float layoutLine(const string& line, const TxfHeader* tf,
                        float columnEm, float spaceEm, Func func)
{
    size_t len = line.size();
    size_t i = 0;
    float endX = 0.0f;

    while (i < len) {
        size_t sp = i;
        while (i < len && line[i] == ' ')
            ++i;
        if (i == len)
            break;
        size_t start = i;
        while (i < len && ! (line[i] == ' ' && i + 2 < len &&
                             line[i+1] == ' ' && line[i+2] == ' '))
            ++i;

        size_t gap = start - sp;
        float x = endX + gap * spaceEm;
        if (sp == 0 || gap >= 3)
            x = std::max(x, start * columnEm);

        const uint8_t* seg = (const uint8_t*) line.data() + start;
        func(x, seg, i - start);
        endX = x + txf_emWidth(tf, seg, i - start);
    }
    return endX;
}

void DocReader::renderReader(ScreenState* ss, void* data)
{
    DocReader* dr = (DocReader*) data;
    WorkBuffer* work = dr->work;
    const GuiArea* area = dr->gbox + WI_TEXT;

    // Animate scrolling.
    if (dr->scroll != dr->scrollTarget) {
        dr->scroll += (dr->scrollTarget - dr->scroll) * 0.3f;
        if (fabsf(dr->scrollTarget - dr->scroll) < 0.5f)
            dr->scroll = dr->scrollTarget;

        float areaH = area->y2 - area->y;
        int first = int((dr->scroll - dr->textPad) / dr->lineSpacing);
        int last  = int((dr->scroll + areaH) / dr->lineSpacing) + 1;
        if (first < dr->firstLine || last > dr->firstLine + dr->lineCount)
            dr->generateText();
        dr->generateScrollBar();
    }

    if (work->dirty)
        gpu_updateWorkBuffer(xu4.gpu, GPU_DLIST_GUI, work);

    gpu_enableGui(xu4.gpu, WID_NONE, 0);
    gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region + REGION_FRAME);

    int box[4];
    box[0] = ss->aspectX + area->x;
    box[1] = ss->aspectY + area->y;
    box[2] = area->x2 - area->x;
    box[3] = area->y2 - area->y;
    gpu_setScissor(box);
    gpu_guiSetOrigin(xu4.gpu, area->x + dr->textPad, area->y2 + dr->scroll);
    gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region + REGION_TEXT);
    gpu_guiSetOrigin(xu4.gpu, 0.0f, 0.0f);
    gpu_setScissor(NULL);

    gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region + REGION_BAR);
}

DocReader::DocReader()
{
    int rsize[3];
    for (int i = 0; i < 3; ++i)
        rsize[i] = regionQuads[i] * QUAD_ATTR;
    work = gpu_allocWorkBuffer(rsize, 3);
    doc = DOC_HISTORY;
    firstLine = lineCount = 0;
    buttonDown = WID_NONE;
    scroll = scrollTarget = 0.0f;
}

DocReader::~DocReader()
{
    gpu_freeWorkBuffer(work);
}

void DocReader::openDocument(int n)
{
    string text;

    doc = n;
    if (! docsLoad(GameDocument(n), text)) {
        text = "\n\n"
            "    This document was not found.\n\n"
            "    The free GOG release of Ultima IV includes the printed\n"
            "    documents as ULTIMA4.TXT.  Place it with the game files\n"
            "    and xu4 will split it into separate documents.  Releases\n"
            "    with separate History.txt, Wisdom.txt, Hints.txt and\n"
            "    Keyboard.txt files are also found there, or in a docs\n"
            "    folder beside the game files.\n\n"
            "    The documents can also be put in the docs folder of\n"
            "    ";
        text += xu4.settings->getUserPath();
        text += "\n    with the names History.txt, Wisdom.txt, Hints.txt\n"
                "    and Keyboard.txt.\n";
    }
    splitLines(text, lines);
    scroll = scrollTarget = 0.0f;
    layout();
}

bool DocReader::present()
{
    openDocument(doc);
    screenSetLayer(LAYER_TOP_MENU, renderReader, this);
    return true;
}

void DocReader::conclude()
{
    screenSetLayer(LAYER_TOP_MENU, NULL, NULL);
}

/*
 * Generate the frame, document tabs & footer.
 */
void DocReader::layout()
{
    const ScreenState* ss = screenState();
    TxfDrawState ds;
    float W = ss->aspectW;
    float H = ss->aspectH;
    float mx = W * 0.05f;
    float my = H * 0.04f;
    float pad = H * 0.02f;
    float tabH = H * 0.075f;
    float footH = H * 0.055f;
    float panelX2 = W - mx;
    float panelY2 = H - my;
    float rect[4];
    float* attr;
    int i;

    ds.fontTable = ss->fontTable;

    // Tabs across the top, Back button at the bottom right.
    float tabY = panelY2 - pad - tabH;
    float tabW = W * 0.17f;
    float x = mx + pad;
    for (i = 0; i < DOC_COUNT; ++i) {
        setArea(gbox + WI_TAB + i, x, tabY, x + tabW, tabY + tabH, WI_TAB + i);
        x += tabW + pad * 0.5f;
    }
    float backW = W * 0.14f;
    setArea(gbox + WI_BACK, panelX2 - pad - backW, my + pad,
            panelX2 - pad, my + pad + footH, WI_BACK);
    setArea(gbox + WI_TEXT, mx + pad, my + pad + footH + pad,
            panelX2 - pad, tabY - pad * 0.5f, WI_TEXT);

    // Size the text so that the widest line of the document fits.
    const GuiArea* area = gbox + WI_TEXT;
    const TxfHeader* tf = ss->fontTable[0];
    float barW = W * 0.012f;
    textPad = pad;
    float textW = (area->x2 - area->x) - textPad * 2.0f - barW;
    float colEm = txf_emWidth(tf, (const uint8_t*) "n", 1);
    float spaceEm = txf_emWidth(tf, (const uint8_t*) " ", 1);
    float widest = MIN_COLUMNS * colEm;
    for (const string& line : lines) {
        float w = layoutLine(line, tf, colEm, spaceEm,
                             [](float, const uint8_t*, size_t) {});
        if (widest < w)
            widest = w;
    }
    psize = std::min(textW / widest, H * MAX_FONT_SIZE);
    columnW = colEm * psize;
    lineSpacing = tf->lineHeight * psize;
    descender = tf->descender * psize;

    attr = gpu_beginRegion(work, REGION_FRAME);

    rect[0] = 0.0f;
    rect[1] = 0.0f;
    rect[2] = W;
    rect[3] = H;
    attr = gui_emitQuadCi(attr, rect, COL_BLACK + 128);
    rect[0] = mx;
    rect[1] = my;
    rect[2] = W - mx - mx;
    rect[3] = H - my - my;
    attr = gui_emitQuadCi(attr, rect, COL_BROWN);
    attr = emitArea(attr, area, COL_DK_BROWN);

    txf_begin(&ds, 1, tabH * 0.55f, 0.0f, 0.0f);
    for (i = 0; i < DOC_COUNT; ++i) {
        const GuiArea* tab = gbox + WI_TAB + i;
        bool cur = (i == doc);
        attr = emitArea(attr, tab, cur ? COL_LT_BLUE : COL_DK_BROWN);
        attr = emitCentered(attr, &ds, tab, docsTitle(GameDocument(i)),
                            cur ? COL_TX_BLACK : COL_BEIGE);
    }
    attr = emitArea(attr, gbox + WI_BACK, COL_DK_BROWN);
    attr = emitCentered(attr, &ds, gbox + WI_BACK, "Back", COL_BEIGE);

    const char* help = gamepad_inUse() ?
        "Up/Down: Scroll    X: Page    Left/Right: Document    B: Back" :
        "Up/Down, Space: Scroll    Left/Right: Document    Esc: Back";
    float hsize = footH * 0.5f;
    txf_begin(&ds, 0, hsize, mx + pad,
              my + pad + (footH - hsize * 0.7f) * 0.5f);
    ds.colorIndex = COL_BEIGE;
    attr = gui_emitText(&ds, attr, help, strlen(help));

    gpu_endRegion(work, REGION_FRAME, attr);

    generateText();
    generateScrollBar();
}

/*
 * Generate the lines of text which are in view.  The top of the document is
 * at 0,0 to be placed with gpu_guiSetOrigin().
 */
void DocReader::generateText()
{
    const GuiArea* area = gbox + WI_TEXT;
    float areaH = area->y2 - area->y;
    int rows = int(areaH / lineSpacing) + 2;
    TxfDrawState ds;

    ds.fontTable = screenState()->fontTable;
    txf_begin(&ds, 0, psize, 0.0f, 0.0f);
    float spaceEm = txf_emWidth(ds.tf, (const uint8_t*) " ", 1);
    float columnEm = columnW / psize;

    firstLine = std::max(0, int((scroll - textPad) / lineSpacing) - 2);
    int end = std::min(int(lines.size()), firstLine + rows + 4);

    float* attr = gpu_beginRegion(work, REGION_TEXT);
    const WorkRegion* reg = work->region + REGION_TEXT;
    const float* limit = work->attr + reg->start + reg->avail;
    int n;

    ds.colorIndex = COL_WHITE;
    for (n = firstLine; n < end; ++n) {
        const string& line = lines[n];
        float y = -(n + 1) * lineSpacing - textPad - descender;

        if (attr + line.size() * QUAD_ATTR > limit)
            break;

        layoutLine(line, ds.tf, columnEm, spaceEm,
            [&](float x, const uint8_t* seg, size_t len) {
                ds.x = ds.marginL = x * psize;
                ds.y = y;
                ds.prev = NULL;
                attr = gui_emitText(&ds, attr, (const char*) seg, len);
            });
    }
    lineCount = n - firstLine;
    gpu_endRegion(work, REGION_TEXT, attr);
}

void DocReader::generateScrollBar()
{
    const GuiArea* area = gbox + WI_TEXT;
    float areaH = area->y2 - area->y;
    float total = maxScroll() + areaH;
    float barW = screenState()->aspectW * 0.012f;
    float rect[4];

    float* attr = gpu_beginRegion(work, REGION_BAR);
    if (maxScroll() > 0.0f) {
        float thumbH = std::max(areaH * areaH / total, barW * 2.0f);
        float travel = areaH - thumbH;
        rect[0] = area->x2 - barW * 1.5f;
        rect[1] = area->y + travel * (1.0f - scroll / maxScroll());
        rect[2] = barW;
        rect[3] = thumbH;
        attr = gui_emitQuadCi(attr, rect, COL_LT_GRAY);
    }
    gpu_endRegion(work, REGION_BAR, attr);
}

float DocReader::maxScroll() const
{
    const GuiArea* area = gbox + WI_TEXT;
    float areaH = area->y2 - area->y;
    float h = lines.size() * lineSpacing + textPad * 2.0f;
    return (h > areaH) ? h - areaH : 0.0f;
}

void DocReader::scrollTo(float y)
{
    float end = maxScroll();
    if (y > end)
        y = end;
    if (y < 0.0f)
        y = 0.0f;
    scrollTarget = y;
}

void DocReader::scrollLines(int n)
{
    scrollTo(scrollTarget + n * lineSpacing);
}

bool DocReader::keyPressed(int key)
{
    const GuiArea* area = gbox + WI_TEXT;
    int page = int((area->y2 - area->y) / lineSpacing) - 2;

    switch (key) {
        case U4_UP:
            scrollLines(-1);
            return true;
        case U4_DOWN:
            scrollLines(1);
            return true;
        case U4_SPACE:
        case U4_ENTER:
        case U4_PAGE_DOWN:
            scrollLines(page);
            return true;
        case U4_BACKSPACE:
        case U4_PAGE_UP:
            scrollLines(-page);
            return true;
        case U4_HOME:
            scrollTo(0.0f);
            return true;
        case U4_END:
            scrollTo(maxScroll());
            return true;
        case U4_LEFT:
            soundPlay(SOUND_UI_TICK);
            openDocument((doc + DOC_COUNT - 1) % DOC_COUNT);
            return true;
        case U4_RIGHT:
        case U4_TAB:
        case 'z':
            soundPlay(SOUND_UI_TICK);
            openDocument((doc + 1) % DOC_COUNT);
            return true;
        case U4_ESC:
            xu4.eventHandler->setControllerDone(true);
            return true;
        default:
            if (key >= '1' && key < '1' + DOC_COUNT) {
                soundPlay(SOUND_UI_TICK);
                openDocument(key - '1');
                return true;
            }
            break;
    }
    return false;
}

bool DocReader::inputEvent(const InputEvent* ev)
{
    const ScreenState* ss = screenState();
    int x = ev->x - ss->aspectX;
    int y = (ss->displayH - ev->y) - ss->aspectY;
    int i;

    switch (ev->type) {
        case IE_MOUSE_PRESS:
            buttonDown = WID_NONE;
            if (ev->n != CMOUSE_LEFT)
                break;
            for (i = WI_TAB; i < WI_COUNT; ++i) {
                if (insideArea(gbox + i, x, y))
                    buttonDown = i;
            }
            break;

        case IE_MOUSE_RELEASE:
            if (ev->n == CMOUSE_LEFT && buttonDown != WID_NONE &&
                insideArea(gbox + buttonDown, x, y)) {
                soundPlay(SOUND_UI_CLICK);
                if (buttonDown == WI_BACK)
                    xu4.eventHandler->setControllerDone(true);
                else if (buttonDown - WI_TAB != doc)
                    openDocument(buttonDown - WI_TAB);
            }
            buttonDown = WID_NONE;
            break;

        case IE_MOUSE_WHEEL:
            if (ev->y < 0)
                scrollLines(3);
            else if (ev->y > 0)
                scrollLines(-3);
            break;
    }
    return true;
}
