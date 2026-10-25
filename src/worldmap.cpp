/*
 * XU4 - World map viewer
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Draws all of world.map with the tiles of the current module, marks the
 * moongates with the moon phase characters of the module font, and places
 * the names written on the cloth map that came with Ultima IV, in runes or
 * in English.
 */

#include <math.h>
#include <cstring>

#include "worldmap.h"
#include "context.h"
#include "config.h"
#include "event.h"
#include "gamepad.h"
#include "image.h"
#include "gpu.h"
#include "imagemgr.h"
#include "location.h"
#include "map.h"
#include "mapmgr.h"
#include "party.h"
#include "screen.h"
#include "settings.h"
#include "sound.h"
#include "textview.h"
#include "tileset.h"
#include "u4.h"
#include "xu4.h"

#define ATTR_COUNT  7
#define QUAD_ATTR   (6 * ATTR_COUNT)
#define MAP_DIM     256         // World map size in tiles.
#define FONT_RUNES  3           // Index in ScreenState::fontTable.
#define FONT_LATIN  1
#define LABEL_EM    7.0f        // Label font size in tiles.
#define MOON_TILES  4           // Size of the moongate signs in tiles.

enum MapRegions {
    REGION_FRAME,
    REGION_LABELS
};

static const int regionQuads[2] = { 300, 1200 };

// Rune font codes for the combined runes (see tools/rune-glyphs.py).
#define TH  "\x7f"
#define EE  "\x80"
#define EA  "\x82"

struct MapLabel {
    const char* runes;      // As spelled on the cloth map.
    const char* english;
    uint8_t x, y;           // Center in map tiles.
};

// Names and places of the cloth map (lines split as there).
static const MapLabel labels[] = {
    { TH "E D" EE "P FOREST", "The Deep Forest",     56,  36 },
    { "LOST RIVER",           "Lost River",          20,  71 },
    { TH "E\nHIGH\nSTEPES",   "The\nHigh\nSteppes", 109,  45 },
    { "LOST\nHOPE\nBAY",      "Lost\nHope\nBay",    144,  33 },
    { "LOCK LAKE",            "Lock Lake",          127,  78 },
    { "BLOODY\nPLAINS",       "Bloody\nPlains",     174,  59 },
    { "DAGGER\nISLE",         "Dagger\nIsle",       243,  52 },
    { "VERITY\nISLE",         "Verity\nIsle",       244, 103 },
    { "SERPENTS SPINE",       "Serpent's Spine",     79,  71 },
    { "BRITANNIA\nBAY",       "Britannia\nBay",      92, 122 },
    { "SPIRITWOOD",           "Spiritwood",          53, 134 },
    { "FENS\nOF " TH "E\nD" EA "D", "Fens\nof the\nDead", 113, 158 },
    { "VALORIAN\nISLE",       "Valorian\nIsle",      56, 219 },
    { "CAPE OF HEROES",       "Cape of Heroes",     111, 219 },
    { TH "E ISLE\nOF D" EE "DS", "The Isle\nof Deeds", 176, 245 },
    { TH "E LANDS\nOF\nBRITANNIA", "The Lands\nof\nBritannia", 167, 172 },
};

#define LABEL_COUNT (sizeof(labels) / sizeof(labels[0]))

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

static float* emitCentered(float* attr, TxfDrawState* ds, const GuiArea* area,
                           const char* text, float colorIndex)
{
    int len = strlen(text);
    float w = txf_emWidth(ds->tf, (const uint8_t*) text, len) * ds->psize;
    float h = area->y2 - area->y;
    ds->x = ds->marginL = (area->x + area->x2 - w) * 0.5f;
    ds->y = area->y + (h - ds->psize * 0.7f) * 0.5f;
    ds->prev = NULL;
    ds->colorIndex = colorIndex;
    return gui_emitText(ds, attr, text, len);
}

/*
 * Return the location on the world map (the outermost location).
 */
static const Location* worldLocation()
{
    if (! c || ! c->location)
        return NULL;
    const Location* loc = c->location;
    while (loc->prev)
        loc = loc->prev;
    return loc->map->isWorldMap() ? loc : NULL;
}

/*
 * Copy a character of the font onto the map, enlarged by an integer factor
 * and centered on cx, cy.
 */
static void drawGlyph(Image32* dest, const Image* charset, int chr,
                      int cx, int cy, int mag)
{
    int cw = charset->width();
    int ch = CHAR_HEIGHT;
    const uint32_t* src = charset->pixelData() + chr * ch * cw;
    int x0 = cx - (cw * mag) / 2;
    int y0 = cy - (ch * mag) / 2;

    for (int y = 0; y < ch * mag; ++y) {
        int dy = y0 + y;
        if (dy < 0 || dy >= dest->h)
            continue;
        const uint32_t* row = src + (y / mag) * cw;
        uint32_t* drow = dest->pixels + dy * dest->w;
        for (int x = 0; x < cw * mag; ++x) {
            int dx = x0 + x;
            if (dx >= 0 && dx < dest->w)
                drow[dx] = row[x / mag];
        }
    }
}

/*
 * Halve the size of an image by averaging each 2x2 block of pixels.
 */
static void halveImage(Image32* img)
{
    int w = img->w / 2;
    int h = img->h / 2;
    const uint8_t* src = (const uint8_t*) img->pixels;
    uint8_t* dst = (uint8_t*) img->pixels;
    int pitch = img->w * 4;

    for (int y = 0; y < h; ++y) {
        const uint8_t* s0 = src + (y * 2) * pitch;
        const uint8_t* s1 = s0 + pitch;
        for (int x = 0; x < w * 4; ++x) {
            int i = (x / 4) * 8 + (x & 3);
            *dst++ = (s0[i] + s0[i + 4] + s1[i] + s1[i + 4] + 2) / 4;
        }
    }
    img->w = w;
    img->h = h;
}

WorldMapView::WorldMapView()
{
    int rsize[2];
    for (int i = 0; i < 2; ++i)
        rsize[i] = regionQuads[i] * QUAD_ATTR;
    work = gpu_allocWorkBuffer(rsize, 2);
    mapTex = partyTex = 0;
    buttonDown = WID_NONE;
    mouseX = mouseY = 0;
    english = false;
    zoomLevel = zoomMax = 0;
    fitScale = scale = 1.0f;
    centerX = centerY = targetX = targetY = MAP_DIM * 0.5f;
    labelScale = 0.0f;
}

WorldMapView::~WorldMapView()
{
    freeTextures();
    gpu_freeWorkBuffer(work);
}

/*
 * Draw the world map & the party tile into textures.
 */
bool WorldMapView::buildTextures()
{
    Map* world = xu4.config->map(MAP_WORLD);
    if (! world || ! world->tileset)
        return false;
    const Tileset* ts = world->tileset;
    int tw = ts->tileW;
    int th = ts->tileH;
    if (tw < 1 || th < 1)
        return false;

    Image32 img;
    if (! image32_allocPixels(&img, MAP_DIM * tw, MAP_DIM * th))
        return false;

    RGBA black;
    rgba_set(black, 0, 0, 0, 255);
    image32_fill(&img, &black);

    for (int y = 0; y < MAP_DIM; ++y) {
        for (int x = 0; x < MAP_DIM; ++x) {
            const Tile* tile = ts->get(world->getTileFromData(Coords(x, y)));
            const Image* timg = tile ? tile->getImage() : NULL;
            if (timg)
                image32_blitRect(&img, x * tw, y * th, timg, 0, 0, tw, th, 0);
        }
    }

    // The moongates are marked by the moon phase which opens each one, as
    // shown in the status bar.
    ImageInfo* cinfo = xu4.imageMgr->get(BKGD_CHARSET);
    if (cinfo && cinfo->image) {
        const Image* charset = cinfo->image;
        int mag = (MOON_TILES * tw) / charset->width();
        if (mag < 1)
            mag = 1;
        for (int phase = 0; phase < 8; ++phase) {
            const Coords* gate = xu4.config->moongateCoords(phase);
            if (! gate)
                continue;
            int chr = phase ? MOON_CHAR + phase - 1 : MOON_CHAR + 7;
            drawGlyph(&img, charset, chr, gate->x * tw + tw / 2,
                      gate->y * th + th / 2, mag);
        }
    }

    int maxDim = gpu_maxTextureSize();
    while (maxDim > 0 && (img.w > maxDim || img.h > maxDim))
        halveImage(&img);

    mapTex = gpu_makeTextureMip(&img);
    image32_freePixels(&img);

    // The party (on foot, horse, ship or balloon) or the avatar if the
    // party is in a town or dungeon.
    partyX = partyY = -1;
    const Location* loc = worldLocation();
    if (loc) {
        partyX = loc->coords.x;
        partyY = loc->coords.y;

        const Tile* tile;
        if (loc == c->location)
            tile = ts->get(c->party->getTransport().id);
        else
            tile = ts->getByName(Tile::sym.avatar);
        const Image* timg = tile ? tile->getImage() : NULL;
        if (timg && image32_allocPixels(&img, tw, th)) {
            image32_blitRect(&img, 0, 0, timg, 0, 0, tw, th, 0);
            partyTex = gpu_makeTexture(&img);
            image32_freePixels(&img);
        }
    }
    return true;
}

void WorldMapView::freeTextures()
{
    if (mapTex) {
        gpu_freeTexture(mapTex);
        mapTex = 0;
    }
    if (partyTex) {
        gpu_freeTexture(partyTex);
        partyTex = 0;
    }
}

bool WorldMapView::present()
{
    if (! buildTextures()) {
        freeTextures();
        return false;
    }
    buttonDown = WID_NONE;
    frame = 0;
    layout();

    // Start at the party position, or show the whole map.
    zoomLevel = 0;
    scale = fitScale;
    targetX = targetY = MAP_DIM * 0.5f;
    centerX = targetX;
    centerY = targetY;
    labelScale = 0.0f;

    screenSetMouseCursor(MC_DEFAULT);
    screenShowMouseCursor(true);
    screenSetLayer(LAYER_TOP_MENU, renderMap, this);
    return true;
}

void WorldMapView::conclude()
{
    screenSetLayer(LAYER_TOP_MENU, NULL, NULL);
    freeTextures();
    if (! xu4.settings->mouseOptions.enabled)
        screenShowMouseCursor(false);
}

/*
 * Generate the frame, buttons & help line.
 */
void WorldMapView::layout()
{
    const ScreenState* ss = screenState();
    TxfDrawState ds;
    float W = ss->aspectW;
    float H = ss->aspectH;
    float mx = W * 0.02f;
    float my = H * 0.02f;
    float pad = H * 0.015f;
    float footH = H * 0.06f;
    float rect[4];
    float* attr;

    ds.fontTable = ss->fontTable;

    // Buttons at the bottom right, map above.
    float x2 = W - mx;
    float by = my;
    float bw[4] = { footH * 1.3f, footH * 1.3f, W * 0.13f, W * 0.11f };
    int wid[4] = { WI_BACK, WI_LANG, WI_ZOOM_IN, WI_ZOOM_OUT };
    for (int i = 0; i < 4; ++i) {
        setArea(gbox + wid[i], x2 - bw[3 - i], by, x2, by + footH, wid[i]);
        x2 -= bw[3 - i] + pad * 0.5f;
    }
    setArea(gbox + WI_MAP, mx, my + footH + pad, W - mx, H - my, WI_MAP);

    const GuiArea* area = gbox + WI_MAP;
    float aw = area->x2 - area->x;
    float ah = area->y2 - area->y;
    fitScale = ((aw < ah) ? aw : ah) / MAP_DIM;

    // Zoom in by doubling until the tiles are about as big as in the game
    // view.
    float maxScale = H / 11.0f;
    zoomMax = 0;
    while (fitScale * (2 << zoomMax) <= maxScale * 1.2f)
        ++zoomMax;

    attr = gpu_beginRegion(work, REGION_FRAME);

    rect[0] = 0.0f;
    rect[1] = 0.0f;
    rect[2] = W;
    rect[3] = H;
    attr = gui_emitQuadCi(attr, rect, COL_BLACK);

    txf_begin(&ds, 1, footH * 0.55f, 0.0f, 0.0f);
    static const char* buttonText[4] = { "-", "+", NULL, "Back" };
    for (int i = 0; i < 4; ++i) {
        const GuiArea* button = gbox + WI_ZOOM_OUT + i;
        const char* text = buttonText[i];
        if (! text)
            text = english ? "Runes" : "English";
        attr = emitArea(attr, button, COL_DK_BROWN);
        attr = emitCentered(attr, &ds, button, text, COL_BEIGE);
    }

    const char* help = gamepad_inUse() ?
        "D-pad: Pan   A, X: Zoom   Y: Runes/English   B: Back" :
        "Arrows: Pan   +, -: Zoom   R: Runes/English   Esc: Back";
    float hsize = footH * 0.45f;
    txf_begin(&ds, 0, hsize, mx, my + (footH - hsize * 0.7f) * 0.5f);
    ds.colorIndex = COL_BEIGE;
    attr = gui_emitText(&ds, attr, help, strlen(help));

    gpu_endRegion(work, REGION_FRAME, attr);
}

/*
 * Return the screen rectangle (x, y, w, h) of the whole map.
 */
void WorldMapView::mapRect(float* rect) const
{
    const GuiArea* area = gbox + WI_MAP;
    float mid = (area->x + area->x2) * 0.5f;
    float top = (area->y + area->y2) * 0.5f;
    rect[2] = rect[3] = MAP_DIM * scale;
    rect[0] = mid - centerX * scale;
    rect[1] = top + centerY * scale - rect[3];
}

/*
 * Generate the place names for the current zoom & position.
 */
void WorldMapView::generateLabels()
{
    TxfDrawState ds;
    float rect[4];
    float psize = LABEL_EM * scale;
    float maxSize = screenState()->aspectH * 0.08f;
    if (psize > maxSize)
        psize = maxSize;
    float lineH = psize * (english ? 0.8f : 0.75f);
    float capH  = psize * (english ? 0.65f : 0.6f);
    float shadow = psize * 0.05f + 1.0f;

    mapRect(rect);
    ds.fontTable = screenState()->fontTable;
    txf_begin(&ds, english ? FONT_LATIN : FONT_RUNES, psize, 0.0f, 0.0f);

    float* attr = gpu_beginRegion(work, REGION_LABELS);
    for (size_t n = 0; n < LABEL_COUNT; ++n) {
        const MapLabel* lab = labels + n;
        const char* text = english ? lab->english : lab->runes;
        float cx = rect[0] + (lab->x + 0.5f) * scale;
        float cy = rect[1] + (MAP_DIM - lab->y - 0.5f) * scale;

        int lines = 1;
        for (const char* it = text; *it; ++it)
            if (*it == '\n')
                ++lines;
        float y = cy + lines * lineH * 0.5f - lineH + (lineH - capH) * 0.5f;

        while (*text) {
            const char* end = strchr(text, '\n');
            int len = end ? end - text : strlen(text);
            float w = txf_emWidth(ds.tf, (const uint8_t*) text, len) * psize;
            float x = cx - w * 0.5f;

            for (int pass = 0; pass < 2; ++pass) {
                ds.x = ds.marginL = x + (pass ? 0.0f : shadow);
                ds.y = y - (pass ? 0.0f : shadow);
                ds.prev = NULL;
                ds.colorIndex = pass ? COL_WHITE : COL_TX_BLACK;
                attr = gui_emitText(&ds, attr, text, len);
            }
            y -= lineH;
            text += len;
            if (*text)
                ++text;
        }
    }
    gpu_endRegion(work, REGION_LABELS, attr);

    labelScale = scale;
    labelX = centerX;
    labelY = centerY;
}

void WorldMapView::renderMap(ScreenState* ss, void* data)
{
    WorldMapView* wm = (WorldMapView*) data;
    WorkBuffer* work = wm->work;
    const GuiArea* area = wm->gbox + WI_MAP;
    float rect[4];

    // Animate zoom & scrolling.
    float ts = wm->targetScale();
    if (wm->scale != ts || wm->centerX != wm->targetX ||
        wm->centerY != wm->targetY) {
        wm->scale   += (ts - wm->scale) * 0.3f;
        wm->centerX += (wm->targetX - wm->centerX) * 0.3f;
        wm->centerY += (wm->targetY - wm->centerY) * 0.3f;
        if (fabsf(ts - wm->scale) < 0.01f &&
            fabsf(wm->targetX - wm->centerX) < 0.05f &&
            fabsf(wm->targetY - wm->centerY) < 0.05f) {
            wm->scale = ts;
            wm->centerX = wm->targetX;
            wm->centerY = wm->targetY;
        }
    }
    if (wm->labelScale != wm->scale || wm->labelX != wm->centerX ||
        wm->labelY != wm->centerY)
        wm->generateLabels();

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

    wm->mapRect(rect);
    gpu_drawImage(xu4.gpu, wm->mapTex, rect, ss->aspectW, ss->aspectH);

    // Blink the party position.
    if (wm->partyTex && ((wm->frame++ / 12) & 1) == 0) {
        float s = wm->scale;
        float size = (s < wm->fitScale * 4.0f) ? wm->fitScale * 4.0f : s;
        float prect[4];
        prect[0] = rect[0] + (wm->partyX + 0.5f) * s - size * 0.5f;
        prect[1] = rect[1] + (MAP_DIM - wm->partyY - 0.5f) * s - size * 0.5f;
        prect[2] = prect[3] = size;
        gpu_drawImage(xu4.gpu, wm->partyTex, prect, ss->aspectW, ss->aspectH);
    }

    gpu_enableGui(xu4.gpu, WID_NONE, 0);
    gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region + REGION_LABELS);
    gpu_setScissor(NULL);
}

float WorldMapView::targetScale() const
{
    return fitScale * (1 << zoomLevel);
}

/*
 * Keep as much of the map in view as possible at scale s.
 */
void WorldMapView::clampCenter(float s, float& cx, float& cy) const
{
    const GuiArea* area = gbox + WI_MAP;
    float halfW = (area->x2 - area->x) * 0.5f / s;
    float halfH = (area->y2 - area->y) * 0.5f / s;
    float mid = MAP_DIM * 0.5f;

    if (halfW >= mid)
        cx = mid;
    else if (cx < halfW)
        cx = halfW;
    else if (cx > MAP_DIM - halfW)
        cx = MAP_DIM - halfW;

    if (halfH >= mid)
        cy = mid;
    else if (cy < halfH)
        cy = halfH;
    else if (cy > MAP_DIM - halfH)
        cy = MAP_DIM - halfH;
}

/*
 * Change the zoom level, keeping the map point under screen position px, py
 * in place.
 */
void WorldMapView::zoomTo(int level, float px, float py)
{
    if (level < 0)
        level = 0;
    if (level > zoomMax)
        level = zoomMax;
    if (level == zoomLevel)
        return;

    const GuiArea* area = gbox + WI_MAP;
    float mx = (area->x + area->x2) * 0.5f;
    float my = (area->y + area->y2) * 0.5f;
    float s = targetScale();
    float tx = targetX + (px - mx) / s;
    float ty = targetY - (py - my) / s;

    zoomLevel = level;
    s = targetScale();
    targetX = tx - (px - mx) / s;
    targetY = ty + (py - my) / s;
    clampCenter(s, targetX, targetY);
    soundPlay(SOUND_UI_TICK);
}

/*
 * Move the view by a fraction of its size.
 */
void WorldMapView::pan(float dx, float dy)
{
    const GuiArea* area = gbox + WI_MAP;
    float s = targetScale();
    targetX += dx * (area->x2 - area->x) / s;
    targetY += dy * (area->y2 - area->y) / s;
    clampCenter(s, targetX, targetY);
}

/*
 * Return true if the party tile is inside the view.
 */
bool WorldMapView::partyInView() const
{
    const GuiArea* area = gbox + WI_MAP;
    float s = targetScale();
    float halfW = (area->x2 - area->x) * 0.5f / s;
    float halfH = (area->y2 - area->y) * 0.5f / s;
    return fabsf(partyX + 0.5f - targetX) < halfW &&
           fabsf(partyY + 0.5f - targetY) < halfH;
}

bool WorldMapView::keyPressed(int key)
{
    const GuiArea* area = gbox + WI_MAP;
    float mx = (area->x + area->x2) * 0.5f;
    float my = (area->y + area->y2) * 0.5f;

    switch (key) {
        case U4_UP:
            pan(0.0f, -0.25f);
            return true;
        case U4_DOWN:
            pan(0.0f, 0.25f);
            return true;
        case U4_LEFT:
            pan(-0.25f, 0.0f);
            return true;
        case U4_RIGHT:
            pan(0.25f, 0.0f);
            return true;
        case '+':
        case '=':
        case U4_ENTER:
        case U4_PAD_R:
        {
            // Zoom in on the party when it is in view.
            bool onParty = (partyX >= 0 && partyInView());
            zoomTo(zoomLevel + 1, mx, my);
            if (onParty) {
                targetX = partyX + 0.5f;
                targetY = partyY + 0.5f;
                clampCenter(targetScale(), targetX, targetY);
            }
        }
            return true;
        case '-':
        case U4_SPACE:
        case U4_PAD_L:
            zoomTo(zoomLevel - 1, mx, my);
            return true;
        case U4_HOME:
            zoomTo(0, mx, my);
            return true;
        case 'r':
        case 'e':
        case 'z':       // Gamepad Y
        case U4_TAB:
            soundPlay(SOUND_UI_CLICK);
            english = ! english;
            layout();
            labelScale = 0.0f;
            return true;
        case 'm':
        case U4_ESC:
            xu4.eventHandler->setControllerDone(true);
            return true;
    }
    return false;
}

bool WorldMapView::inputEvent(const InputEvent* ev)
{
    const ScreenState* ss = screenState();
    int x = ev->x - ss->aspectX;
    int y = (ss->displayH - ev->y) - ss->aspectY;
    int i;

    if (ev->type == IE_MOUSE_WHEEL) {
        // The wheel event holds the wheel motion, not the pointer position.
        x = mouseX;
        y = mouseY;
    } else {
        mouseX = x;
        mouseY = y;
    }

    switch (ev->type) {
        case IE_MOUSE_PRESS:
            buttonDown = WID_NONE;
            if (ev->n != CMOUSE_LEFT)
                break;
            for (i = 0; i < WI_COUNT; ++i) {
                if (insideArea(gbox + i, x, y))
                    buttonDown = i;
            }
            if (buttonDown == WI_MAP) {
                dragX = x;
                dragY = y;
                dragCX = targetX;
                dragCY = targetY;
                dragged = false;
            }
            break;

        case IE_MOUSE_MOVE:
            if (buttonDown == WI_MAP) {
                int dx = x - dragX;
                int dy = y - dragY;
                if (dragged || dx > 4 || dx < -4 || dy > 4 || dy < -4) {
                    float s = targetScale();
                    targetX = dragCX - dx / s;
                    targetY = dragCY + dy / s;
                    clampCenter(s, targetX, targetY);
                    centerX = targetX;
                    centerY = targetY;
                    dragged = true;
                }
            }
            break;

        case IE_MOUSE_RELEASE:
            if (ev->n == CMOUSE_LEFT && buttonDown != WID_NONE &&
                insideArea(gbox + buttonDown, x, y)) {
                switch (buttonDown) {
                    case WI_MAP:
                        // A tap zooms in there, or out from the closest view.
                        if (! dragged)
                            zoomTo((zoomLevel < zoomMax) ? zoomLevel + 1 : 0,
                                   x, y);
                        break;
                    case WI_ZOOM_OUT:
                        keyPressed('-');
                        break;
                    case WI_ZOOM_IN:
                        keyPressed('+');
                        break;
                    case WI_LANG:
                        keyPressed('r');
                        break;
                    case WI_BACK:
                        soundPlay(SOUND_UI_CLICK);
                        xu4.eventHandler->setControllerDone(true);
                        break;
                }
            }
            buttonDown = WID_NONE;
            break;

        case IE_MOUSE_WHEEL:
            if (insideArea(gbox + WI_MAP, x, y)) {
                if (ev->y > 0)
                    zoomTo(zoomLevel + 1, x, y);
                else if (ev->y < 0)
                    zoomTo(zoomLevel - 1, x, y);
            }
            break;
    }
    return true;
}

/*
 * Show the world map until it is closed.
 */
void worldMapShow()
{
    static WorldMapView* view = NULL;
    if (! view)
        view = new WorldMapView;
    xu4.eventHandler->runController(view);
}
