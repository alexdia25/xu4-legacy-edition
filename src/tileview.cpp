/*
 * $Id$
 */

#ifdef GPU_RENDER
#include <cassert>
#include <string.h>
#include "event.h"
#include "image32.h"
#include "gpu.h"
#endif

#include "debug.h"
#include "imagemgr.h"
#include "settings.h"
#include "screen.h"
#include "tileanim.h"
#include "tileset.h"
#include "tileview.h"
#include "u4.h"
#include "xu4.h"

using std::vector;

TileView::TileView(int x, int y, int columns, int rows) : View(x, y, columns * TILE_WIDTH, rows * TILE_HEIGHT) {
    this->columns = columns;
    this->rows = rows;
    tileset = xu4.config->tileset();
    tileWidth  = tileset->tileW;
    tileHeight = tileset->tileH;
    areaX = x;
    areaY = y;
    areaColumns = columns;
    areaRows = rows;
    fillMode = false;
    layout();
    animated = Image::create(tileWidth, tileHeight);

#ifdef GPU_RENDER
    scissor = NULL;
    aspect = float(columns) / float(rows);
    scale = 2.0f / float(columns);
    effectCount = 0;
    memset(effect, 0, sizeof(effect));      // Sets method to VE_FREE.
#endif
}

/*
 * Place the tiles in the area laid out for 16x16 tiles.
 *
 * Normally the view has the columns & rows it was made with, and tiles
 * of another size are centered in the area.  With fill on, the view has
 * as many tiles as it takes to cover the whole area (an odd number, so
 * that one tile is at the center) and the tiles on the edges are cut.
 * Tiles that are 16x16 fill the area either way.
 */
void TileView::layout() {
    const int areaW = areaColumns * TILE_WIDTH;
    const int areaH = areaRows * TILE_HEIGHT;

    if (fillMode) {
        columns = (areaW + tileWidth - 1) / tileWidth;
        rows    = (areaH + tileHeight - 1) / tileHeight;
        columns |= 1;
        rows    |= 1;
        if (columns > TILEVIEW_MAX)
            columns = TILEVIEW_MAX;
        if (rows > TILEVIEW_MAX)
            rows = TILEVIEW_MAX;
    } else {
        columns = areaColumns;
        rows    = areaRows;
    }

    // Can be left of or above the area when fill is on.
    originX = areaX + (areaW - columns * tileWidth) / 2;
    originY = areaY + (areaH - rows * tileHeight) / 2;

    // The View is the part of the area that is drawn on.
    x = (originX > areaX) ? originX : areaX;
    y = (originY > areaY) ? originY : areaY;
    width  = columns * tileWidth;
    height = rows * tileHeight;
    if (width > areaW)
        width = areaW;
    if (height > areaH)
        height = areaH;
    View::reinit();
}

/*
 * Switch the Fill Space layout on or off.
 * Returns true if the columns & rows changed.
 */
bool TileView::setFill(bool fill) {
#ifdef GPU_RENDER
    // The GPU map renderer only draws whole tiles.
    fill = false;
#endif
    if (fill == fillMode)
        return false;
    int oldColumns = columns;
    int oldRows = rows;
    fillMode = fill;
    layout();
    return columns != oldColumns || rows != oldRows;
}

/*
 * Draw a tile image at screen position px,py, cutting off any part
 * outside the view.
 */
void TileView::blitTile(const Image* img, int px, int py, int sx, int sy) const {
    int w = tileWidth;
    int h = tileHeight;
    int d;

    if ((d = x - px) > 0) {
        px += d;
        sx += d;
        w  -= d;
    }
    if ((d = y - py) > 0) {
        py += d;
        sy += d;
        h  -= d;
    }
    if ((d = px + w - (x + width)) > 0)
        w -= d;
    if ((d = py + h - (y + height)) > 0)
        h -= d;
    if (w > 0 && h > 0)
        img->drawSubRect(px, py, sx, sy, w, h);
}

TileView::~TileView() {
    delete animated;
}

void TileView::reinit() {
    View::reinit();

    //Scratchpad needs to be re-inited if we rescale...
    if (animated)
    {
        delete animated;
        animated = NULL;
    }
    animated = Image::create(tileWidth, tileHeight);
}

void TileView::loadTile(const MapTile &mapTile)
{
    //This attempts to preload tiles in advance
    const Tile *tile = tileset->get(mapTile.id);
    if (tile)
        tile->getImage();

    //But may fail if the tiles don't exist directly in the expected imagesets
}

/*
 * Draw a tile on the screenImage using the current Image::enableBlend
 * setting.
 */
void TileView::drawTile(const MapTile &mapTile, int x, int y) {
    const Tile *tile = tileset->get(mapTile.id);

    ASSERT(x < columns, "x value of %d out of range", x);
    ASSERT(y < rows, "y value of %d out of range", y);

    // draw the tile to the screen
    if (tile->getAnim()) {
        // First, create our animated version of the tile
        tile->getAnim()->draw(animated, tile, mapTile, DIR_NONE);

        // Then draw it to the screen
        blitTile(animated, x * tileWidth + originX,
                           y * tileHeight + originY, 0, 0);
    }
    else {
        const Image *image = tile->getImage();
        blitTile(image, x * tileWidth + originX,
                        y * tileHeight + originY,
                        0, tileHeight * mapTile.frame);
    }
}

void TileView::drawTile(vector<MapTile> &tiles, int x, int y) {
    ASSERT(x < columns, "x value of %d out of range", x);
    ASSERT(y < rows, "y value of %d out of range", y);
    int layer = 0;

    for (vector<MapTile>::reverse_iterator t = tiles.rbegin();
            t != tiles.rend();
            ++t, ++layer)
    {
        const MapTile& frontTile = *t;
        const Tile *frontTileType = tileset->get(frontTile.id);

        if (!frontTileType)
        {
            //TODO, this leads to an error. It happens after graphics mode changes.
            return;
        }

        // draw the tile to the screen
        if (frontTileType->getAnim()) {
            // First, create our animated version of the tile
            frontTileType->getAnim()->draw(animated, frontTileType, frontTile, DIR_NONE);
        }
        else {
            const Image *image = frontTileType->getImage();
            if (!image)
                return; //This is a problem //FIXME, error message it.

            image->drawSubRectOn(animated,
                                0, 0,
                                0, tileHeight * frontTile.frame,
                                tileWidth,  tileHeight);
        }

        // Enable blending after the first tile (assuming it's the ground).
        Image::enableBlend(1);
    }

    // Keep blending disabled by default.
    Image::enableBlend(0);

    // Then draw it to the screen
    blitTile(animated, x * tileWidth + originX,
                       y * tileHeight + originY, 0, 0);
}

/**
 * Draw a focus rectangle around the tile
 */
void TileView::drawFocus(int x, int y) {
    ASSERT(x < columns, "x value of %d out of range", x);
    ASSERT(y < rows, "y value of %d out of range", y);
    Image* screen = xu4.screenImage;

    /*
     * draw the focus rectangle around the tile
     */
    if ((screenState()->currentCycle * 4 / SCR_CYCLE_PER_SECOND) % 2) {
        /* left edge */
        screen->fillRect(x * tileWidth + originX,
                         y * tileHeight + originY,
                         2, tileHeight,
                         0xff, 0xff, 0xff);

        /* top edge */
        screen->fillRect(x * tileWidth + originX,
                         y * tileHeight + originY,
                         tileWidth, 2,
                         0xff, 0xff, 0xff);

        /* right edge */
        screen->fillRect((x + 1) * tileWidth + originX - 2,
                         y * tileHeight + originY,
                         2, tileHeight,
                         0xff, 0xff, 0xff);

        /* bottom edge */
        screen->fillRect(x * tileWidth + originX,
                         (y + 1) * tileHeight + originY - 2,
                         tileWidth, 2,
                         0xff, 0xff, 0xff);
    }
}

/*
 * Mark the tile of a chosen target with red corner brackets.
 */
void TileView::drawTargetMark(int x, int y) {
    Image* screen = xu4.screenImage;
    int left = x * tileWidth + originX;
    int top  = y * tileHeight + originY;
    int right  = left + tileWidth - 2;
    int bottom = top + tileHeight - 2;
    int len = tileWidth / 4;
    const int r = 0xff, g = 0x40, b = 0x40;
    screen->fillRect(left,  top,    len, 2, r, g, b);
    screen->fillRect(left,  top,    2, len, r, g, b);
    screen->fillRect(right + 2 - len, top, len, 2, r, g, b);
    screen->fillRect(right, top,    2, len, r, g, b);
    screen->fillRect(left,  bottom, len, 2, r, g, b);
    screen->fillRect(left,  bottom + 2 - len, 2, len, r, g, b);
    screen->fillRect(right + 2 - len, bottom, len, 2, r, g, b);
    screen->fillRect(right, bottom + 2 - len, 2, len, r, g, b);
}

#ifdef GPU_RENDER
static void stopEffectAnim(VisualEffect* it) {
    if (it->anim != ANIM_UNUSED) {
        Animator* asys;
        if (it->method == VE_SPRITE_FLOURISH)
            asys = &xu4.eventHandler->flourishAnim;
        else
            asys = &xu4.eventHandler->fxAnim;
        anim_setState(asys, it->anim, ANIM_FREE);
        it->anim = ANIM_UNUSED;
    }
}

/*
 * Free VisualEffects and disable map rendering.
 */
void TileView::clear() {
    VisualEffect* it = effect;
    VisualEffect* end = it + effectCount;
    while (it != end) {
        if (it->method != VE_FREE) {
            stopEffectAnim(it);
            it->method = VE_FREE;
        }
        ++it;
    }

    effectCount = 0;

    // This assumes there is only a single TileView active at any time.
    screenDisableMap();
}

/*
 * Find a free effect slot and initialize it.
 *
 * If moveAnim is ANIM_UNUSED then the VisualEffect::method is set to VE_SPRITE.
 * Otherwise the method will be VE_SPRITE_MOVE.
 *
 * \return Effect id or -1 if effect limit is reached.
 */
int TileView::showEffect(const Coords &coords, TileId tile, AnimId moveAnim) {
    for (int i = 0; i < VE_MAX; ++i) {
        if (effect[i].method == VE_FREE) {
            VisualEffect* it = effect + i;

            it->pos[0] = (float) coords.x;
            it->pos[1] = (float) coords.y;
            it->method = (moveAnim == ANIM_UNUSED) ? VE_SPRITE : VE_SPRITE_MOVE;
            it->vid = tileset->render[tile].vid;
            it->anim = moveAnim;

            if (i >= effectCount)
                effectCount = i + 1;
            return i;
        }
    }
    return -1;
}

/*
 * Initialize a specific effect slot.  This is useful to control the draw
 * order of effects (a higher id is drawn later).
 *
 * If the tile has a frame animation that will be started and the method
 * is set to VE_SPRITE_FLOURISH.  Otherwise the method is set to VE_SPRITE.
 * The anim & method members are only set if the effect slot is already free.
 * Calling useEffect on an already allocated slot will not change either
 * of those members, but will update tile, x, & y.
 *
 * \return VisualEffect pointer of the specified id.
 */
VisualEffect* TileView::useEffect(int id, TileId tile, float x, float y) {
    VisualEffect* it = effect + id;
    assert(id < VE_MAX);
    it->pos[0] = x;
    it->pos[1] = y;
    it->vid = tileset->render[tile].vid;
    if (it->method == VE_FREE) {
        it->anim = tileset->get(tile)->startFrameAnim();
        it->method = (it->anim == ANIM_UNUSED) ? VE_SPRITE
                                               : VE_SPRITE_FLOURISH;
    }
    if (id >= effectCount)
        effectCount = id + 1;
    return it;
}

/*
 * Stop any animation and free the effect slot.
 */
void TileView::removeEffect(int id) {
    if (id >= 0) {
        VisualEffect* it = effect + id;
        stopEffectAnim(it);
        it->method = VE_FREE;

        int last = effectCount - 1;
        if (id == last) {
            do {
                --last;
            } while (last >= 0 && effect[last].method == VE_FREE);
            effectCount = last + 1;
        }
    }
}

/*
 * \param cx        View center X.
 * \param cy        View center Y.
 * \param uvTable   Texture coordinate min & max indexed by VisualId.
 */
void TileView::updateEffects(float cx, float cy, const float* uvTable) {
    if (effectCount) {
        const float VIEW_TILE_SIZE = 1.0f;
        const Animator* fxAnim = &xu4.eventHandler->fxAnim;
        float* animPos;
        float* attr = gpu_beginTris(xu4.gpu, GPU_DLIST_VIEW_FX);
        float rect[4];
        int uvIndex;
        VisualEffect* it = effect;
        VisualEffect* end = it + effectCount;

        rect[2] = rect[3] = VIEW_TILE_SIZE;

        for (; it != end; ++it) {
            switch (it->method) {
                case VE_SPRITE_FLOURISH:
                    uvIndex = VID_INDEX(it->vid) +
                        anim_valueI(&xu4.eventHandler->flourishAnim, it->anim);
                    goto draw;

                case VE_SPRITE_MOVE:
                    animPos = anim_valueF2(fxAnim, it->anim);
                    it->pos[0] = animPos[0];
                    it->pos[1] = animPos[1];
                    // Fall through...

                case VE_SPRITE:
                    uvIndex = VID_INDEX(it->vid);
draw:
                    // Similar to emitSprite() in screen.cpp.
                    rect[0] = (it->pos[0] - cx) - 0.5f;
                    rect[1] = (cy - it->pos[1]) - 0.5f;
                    attr = gpu_emitQuad(attr, rect, uvTable + uvIndex*4);
                    break;
            }
        }
        gpu_endTris(xu4.gpu, GPU_DLIST_VIEW_FX, attr);
    } else {
        gpu_clearTris(xu4.gpu, GPU_DLIST_VIEW_FX);
    }
}
#endif
