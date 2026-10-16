/*
 * dungeonview.cpp
 */

#include <assert.h>
#include <string.h>
#include "a2disk.h"
#include "context.h"
#include "debug.h"
#include "dungeon.h"
#include "dungeonview.h"
#include "error.h"
#include "imageloader.h"
#include "imagemgr.h"
#include "settings.h"
#include "scale.h"
#include "screen.h"
#include "tileanim.h"
#include "tileset.h"
#include "u4.h"
#include "utils.h"
#include "xu4.h"


DungeonView::DungeonView(int x, int y, int columns, int rows) : TileView(x, y, rows, columns)
, screen3dDungeonViewEnabled(true)
{
    viewBottom = y + height;
    spotTrapRange = -1;

    black  = tileset->getByName(Tile::sym.black)->getId();
    avatar = tileset->getByName(Tile::sym.avatar)->getId();

    corridor      = tileset->getByName(Tile::sym.dungeonFloor)->getId();
    up_ladder     = tileset->getByName(SYM_UP_LADDER)->getId();
    down_ladder   = tileset->getByName(SYM_DOWN_LADDER)->getId();
    updown_ladder = tileset->getByName(SYM_UP_DOWN_LADDER)->getId();

    cacheGraphicData();
}

/*
 * Sets coords relative to party and fills tiles from that location.
 */
static void dungeonGetTiles(Coords& coords, std::vector<MapTile>& tiles,
                            int fwd, int side) {
    coords = c->location->coords;

    switch (c->saveGame->orientation) {
    case DIR_WEST:
        coords.x -= fwd;
        coords.y -= side;
        break;

    case DIR_NORTH:
        coords.x += side;
        coords.y -= fwd;
        break;

    case DIR_EAST:
        coords.x += fwd;
        coords.y += side;
        break;

    case DIR_SOUTH:
        coords.x -= side;
        coords.y += fwd;
        break;

    case DIR_ADVANCE:
    case DIR_RETREAT:
    default:
        ASSERT(0, "Invalid dungeon orientation");
    }

    // Wrap the coordinates if necessary
    map_wrap(coords, c->location->map);

    bool focus;
    tiles.clear();
    c->location->getTilesAt(tiles, coords, focus);
}

void DungeonView::display(Context * c, TileView *view)
{
    static const int8_t wallSides[3] = { -1, 1, 0 };
    Dungeon* dungeon = dynamic_cast<Dungeon *>(c->location->map);
    vector<MapTile> tiles;
    Coords drawLoc;
    int x, y;

    /* 1st-person perspective */
    if (screen3dDungeonViewEnabled) {
        //Note: This shouldn't go above 4, unless we check opaque tiles each step of the way.
        const int farthest_non_wall_tile_visibility = 4;

        screenEraseMapArea();
        if (apple2) {
            Direction dir = (Direction) c->saveGame->orientation;
            int seen = drawApple2(c, dir);

            // Draw the objects & monsters in view, farthest first.
            if (c->party->getTorchDuration() <= 0)
                seen = -1;
            for (y = seen; y >= 0; y--) {
                dungeonGetTiles(drawLoc, tiles, y, 0);
                DungeonGraphicType type = tilesToGraphic(dungeon, tiles);
                if (type == DNGGRAPHIC_DNGTILE || type == DNGGRAPHIC_BASETILE)
                    drawInDungeon(tiles.front(), 0, y, dir);
            }
        }
        else if (c->party->getTorchDuration() > 0) {
            vector<MapTile> distant_tiles;

            for (y = 3; y >= 0; y--) {
                DungeonGraphicType type;
                Direction dir = (Direction) c->saveGame->orientation;

                // Draw walls player can see.
                Image::enableBlend(1);
                for (x = 0; x < 3; ++x) {
                    dungeonGetTiles(drawLoc, tiles, y, wallSides[x]);
                    type = tilesToGraphic(dungeon, tiles);
                    drawWall(graphicIndex(drawLoc, wallSides[x], y, dir, type));
                }
                Image::enableBlend(0);

                //This only checks that the tile at y==3 is opaque
                if (y == 3 && !tiles.front().getTileType()->isOpaque())
                {
                    for (int y_obj = farthest_non_wall_tile_visibility; y_obj > y; y_obj--)
                    {
                    dungeonGetTiles(drawLoc, distant_tiles, y_obj, 0);
                    DungeonGraphicType distant_type =
                        tilesToGraphic(dungeon, distant_tiles);

                    if ((distant_type == DNGGRAPHIC_DNGTILE) ||
                        (distant_type == DNGGRAPHIC_BASETILE))
                        drawInDungeon(distant_tiles.front(), 0, y_obj, dir);
                    }
                }
                if ((type == DNGGRAPHIC_DNGTILE) ||
                    (type == DNGGRAPHIC_BASETILE))
                    drawInDungeon(tiles.front(), 0, y, dir);
            }
        }
    }

    /* 3rd-person perspective */
    else {
        for (y = 0; y < VIEWPORT_H; y++) {
            for (x = 0; x < VIEWPORT_W; x++) {
                dungeonGetTiles(drawLoc, tiles,
                                (VIEWPORT_H / 2) - y, x - (VIEWPORT_W / 2));

                /* Only show blackness if there is no light */
                if (c->party->getTorchDuration() <= 0)
                    view->drawTile(black, x, y);
                else if (x == VIEWPORT_W/2 && y == VIEWPORT_H/2)
                    view->drawTile(avatar, x, y);
                else
                    view->drawTile(tiles, x, y);
            }
        }
    }
}

void DungeonView::drawInDungeon(const MapTile& mt, int x_offset, int distance, Direction orientation) {
    const static int nscale_vga[] = { 12, 8, 4, 2, 1};
    const static int nscale_ega[] = { 8, 4, 2, 1, 0};

    const int lscale_vga[] = { 22, 18, 10, 4, 1};
    const int lscale_ega[] = { 22, 14, 6, 3, 1};

    const int * lscale;
    const int * nscale;
    int offset_multiplier = 0;
    int offset_adj = 0;
    if (! egaGraphics)
    {
        lscale = & lscale_vga[0];
        nscale = & nscale_vga[0];
        offset_multiplier = 1;
        offset_adj = 2;
    }
    else
    {
        lscale = & lscale_ega[0];
        nscale = & nscale_ega[0];
        offset_adj = 1;
        offset_multiplier = 4;
    }

    //Put tile on animated scratchpad
    const Tile* tile = tileset->get(mt.id);
    if (tile->getAnim())
        tile->getAnim()->draw(animated, tile, mt, orientation);
    else
        tile->getImage()->drawOn(animated, 0, 0);

    /* scale is based on distance; 1 means half size, 2 regular, 4 means scale by 2x, etc. */
    bool tiledWall = tile->isTiledInDungeon();
    const int *dscale = tiledWall ? lscale : nscale;
    Image *scaled;

    if (dscale[distance] == 0)
        return;
    else if (dscale[distance] == 1)
        scaled = scaleDown(animated, 2);
    else
        scaled = scaleUp(animated, dscale[distance] / 2, 1, 0);

    if (tiledWall) {
        int i_x = ((VIEWPORT_W * tileWidth  / 2) + this->x) - (scaled->width() / 2);
        int i_y = ((VIEWPORT_H * tileHeight / 2) + this->y) - (scaled->height() / 2);
        int f_x = i_x + scaled->width();
        int f_y = i_y + scaled->height();
        int d_x = animated->width();
        int d_y = animated->height();

        for (int x = i_x; x < f_x; x+=d_x) {
            for (int y = i_y; y < f_y; y+=d_y)
                animated->drawSubRect(x, y, 0, 0, f_x - x, f_y - y);
        }
    }
    else {
        int y_offset = std::max(0,(dscale[distance] - offset_adj) * offset_multiplier);
        int x = ((VIEWPORT_W * tileWidth / 2) + this->x) - (scaled->width() / 2);
        int y = ((VIEWPORT_H * tileHeight / 2) + this->y + y_offset) - (scaled->height() / 8);

        // Clip VGA tiles at distance 0 to bottom of view.
        int bottom = y + scaled->h;
        if (bottom > viewBottom)
            scaled->h -= bottom - viewBottom;

        Image::enableBlend(1);
        scaled->draw(x, y);
        Image::enableBlend(0);
    }

    delete scaled;
}

/*
 * Begin trap detection for the current view.
 * One trap in range may be shown after a delay.
 */
void DungeonView::detectTraps() {
    spotTrapRange = xu4_random(4);
    if (spotTrapRange < 3)
        spotTrapTime = c->commandTimer + 200 + xu4_random(3000);
    else
        spotTrapRange = -1;
}

int DungeonView::graphicIndex(const Coords& loc, int xoffset, int distance,
                              Direction orientation, DungeonGraphicType type) {
    int index;
    assert(distance < 4);

    if (type == DNGGRAPHIC_LADDERUP && xoffset == 0)
        return 48 +
        (distance * 2) +
        (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH) ? 1 : 0);

    if (type == DNGGRAPHIC_LADDERDOWN && xoffset == 0)
        return 56 +
        (distance * 2) +
        (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH) ? 1 : 0);

    if (type == DNGGRAPHIC_LADDERUPDOWN && xoffset == 0)
        return 64 +
        (distance * 2) +
        (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH) ? 1 : 0);

    if (type == DNGGRAPHIC_TRAP) {
#if 1
        if (xoffset == 0 && spotTrapRange == distance &&
             c->commandTimer >= spotTrapTime)
#else
        if (xoffset == 0)   // For Testing
#endif
        {
            index = static_cast<Dungeon *>(c->location->map)->subTokenAt(loc);
            if (index == TRAP_FALLING_ROCK)
                return 78 + distance;
            if (index == TRAP_PIT)
                return 81 + distance;
        }
        return -1;
    }

    /* FIXME */
    if (type != DNGGRAPHIC_WALL && type != DNGGRAPHIC_DOOR)
        return -1;

    index = 0;
    if (type == DNGGRAPHIC_DOOR)
        index += 24;

    index += (xoffset + 1) * 2;
    index += distance * 6;

    if (DIR_IN_MASK(orientation, MASK_DIR_SOUTH | MASK_DIR_NORTH))
        index++;

    return index;
}

DungeonGraphicType DungeonView::tilesToGraphic(const Dungeon* dungeon,
                                        const std::vector<MapTile> &tiles) {
    MapTile tile = tiles.front();

    /*
     * check if the dungeon tile has an annotation or object on top
     * (always displayed as a tile, unless a ladder)
     */
    if (tiles.size() > 1) {
        if (tile.id == up_ladder)
            return DNGGRAPHIC_LADDERUP;
        else if (tile.id == down_ladder)
            return DNGGRAPHIC_LADDERDOWN;
        else if (tile.id == updown_ladder)
            return DNGGRAPHIC_LADDERUPDOWN;
        else if (tile.id == corridor)
            return DNGGRAPHIC_NONE;
        else
            return DNGGRAPHIC_BASETILE;
    }

    /*
     * if not an annotation or object, then the tile is a dungeon
     * token
     */
    DungeonToken token = dungeon->tokenForTile(tile.id);
    switch (token) {
    case DUNGEON_TRAP:
        return DNGGRAPHIC_TRAP;
    case DUNGEON_CORRIDOR:
        return DNGGRAPHIC_NONE;
    case DUNGEON_WALL:
    case DUNGEON_SECRET_DOOR:
        return DNGGRAPHIC_WALL;
    case DUNGEON_ROOM:
    case DUNGEON_DOOR:
        return DNGGRAPHIC_DOOR;
    case DUNGEON_LADDER_UP:
        return DNGGRAPHIC_LADDERUP;
    case DUNGEON_LADDER_DOWN:
        return DNGGRAPHIC_LADDERDOWN;
    case DUNGEON_LADDER_UPDOWN:
        return DNGGRAPHIC_LADDERUPDOWN;

    default:
        return DNGGRAPHIC_DNGTILE;
    }
}

#define GRAPHIC_COUNT   84

const struct {
    const char* imageName;
    uint8_t ega_x2, ega_y2;
    uint8_t vga_x2, vga_y2;
    uint8_t subimage2;
} dngGraphicInfo[GRAPHIC_COUNT] = {
    { "dung0_lft_ew", 0,0,0,0,0 },
    { "dung0_lft_ns", 0,0,0,0,0 },
    { "dung0_mid_ew", 0,0,0,0,0 },
    { "dung0_mid_ns", 0,0,0,0,0 },
    { "dung0_rgt_ew", 0,0,0,0,0 },
    { "dung0_rgt_ns", 0,0,0,0,0 },
        // 6
    { "dung1_lft_ew", 0, 32, 0, 8, 72 },        // + "dung1_xxx_ew"
    { "dung1_lft_ns", 0, 32, 0, 8, 73 },        // + "dung1_xxx_ns"
    { "dung1_mid_ew", 0,0,0,0,0 },
    { "dung1_mid_ns", 0,0,0,0,0 },
    { "dung1_rgt_ew", 144, 32, 160, 8, 72 },    // + "dung1_xxx_ew"
    { "dung1_rgt_ns", 144, 32, 160, 8, 73 },    // + "dung1_xxx_ns"
        // 12
    { "dung2_lft_ew", 0, 64, 0, 48, 74 },       // + "dung2_xxx_ew"
    { "dung2_lft_ns", 0, 64, 0, 48, 75 },       // + "dung2_xxx_ns"
    { "dung2_mid_ew", 0,0,0,0,0 },
    { "dung2_mid_ns", 0,0,0,0,0 },
    { "dung2_rgt_ew", 112, 64, 128, 48, 74 },   // + "dung2_xxx_ew"
    { "dung2_rgt_ns", 112, 64, 128, 48, 75 },   // + "dung2_xxx_ns"
        // 18
    { "dung3_lft_ew", 0, 80, 48, 72, 76 },      // + "dung3_xxx_ew"
    { "dung3_lft_ns", 0, 80, 48, 72, 77 },      // + "dung3_xxx_ns"
    { "dung3_mid_ew", 0,0,0,0,0 },
    { "dung3_mid_ns", 0,0,0,0,0 },
    { "dung3_rgt_ew", 96, 80, 104, 72, 76 },    // + "dung3_xxx_ew"
    { "dung3_rgt_ns", 96, 80, 104, 72, 77 },    // + "dung3_xxx_ns"
        // 24
    { "dung0_lft_ew_door", 0,0,0,0,0 },
    { "dung0_lft_ns_door", 0,0,0,0,0 },
    { "dung0_mid_ew_door", 0,0,0,0,0 },
    { "dung0_mid_ns_door", 0,0,0,0,0 },
    { "dung0_rgt_ew_door", 0,0,0,0,0 },
    { "dung0_rgt_ns_door", 0,0,0,0,0 },
        // 30
    { "dung1_lft_ew_door", 0, 32, 0, 8, 72 },       // + "dung1_xxx_ew"
    { "dung1_lft_ns_door", 0, 32, 0, 8, 73 },       // + "dung1_xxx_ns"
    { "dung1_mid_ew_door", 0,0,0,0,0 },
    { "dung1_mid_ns_door", 0,0,0,0,0 },
    { "dung1_rgt_ew_door", 144, 32, 160, 8, 72 },   // + "dung1_xxx_ew"
    { "dung1_rgt_ns_door", 144, 32, 160, 8, 73 },   // + "dung1_xxx_ns"
        // 36
    { "dung2_lft_ew_door", 0, 64, 0, 48, 74 },      // + "dung2_xxx_ew"
    { "dung2_lft_ns_door", 0, 64, 0, 48, 75 },      // + "dung2_xxx_ns"
    { "dung2_mid_ew_door", 0,0,0,0,0 },
    { "dung2_mid_ns_door", 0,0,0,0,0 },
    { "dung2_rgt_ew_door", 112, 64, 128, 48, 74 },  // + "dung2_xxx_ew"
    { "dung2_rgt_ns_door", 112, 64, 128, 48, 75 },  // + "dung2_xxx_ns"
        // 42
    { "dung3_lft_ew_door", 0, 80, 48, 72, 76 },     // + "dung3_xxx_ew"
    { "dung3_lft_ns_door", 0, 80, 48, 72, 77 },     // + "dung3_xxx_ns"
    { "dung3_mid_ew_door", 0,0,0,0,0 },
    { "dung3_mid_ns_door", 0,0,0,0,0 },
    { "dung3_rgt_ew_door", 96, 80, 104, 72, 76 },   // + "dung3_xxx_ew"
    { "dung3_rgt_ns_door", 96, 80, 104, 72, 77 },   // + "dung3_xxx_ns"
        // 48
    { "dung0_ladderup",      0,0,0,0,0 },
    { "dung0_ladderup_side", 0,0,0,0,0 },
    { "dung1_ladderup",      0,0,0,0,0 },
    { "dung1_ladderup_side", 0,0,0,0,0 },
    { "dung2_ladderup",      0,0,0,0,0 },
    { "dung2_ladderup_side", 0,0,0,0,0 },
    { "dung3_ladderup",      0,0,0,0,0 },
    { "dung3_ladderup_side", 0,0,0,0,0 },
        // 56
    { "dung0_ladderdown",      0,0,0,0,0 },
    { "dung0_ladderdown_side", 0,0,0,0,0 },
    { "dung1_ladderdown",      0,0,0,0,0 },
    { "dung1_ladderdown_side", 0,0,0,0,0 },
    { "dung2_ladderdown",      0,0,0,0,0 },
    { "dung2_ladderdown_side", 0,0,0,0,0 },
    { "dung3_ladderdown",      0,0,0,0,0 },
    { "dung3_ladderdown_side", 0,0,0,0,0 },
        // 64
    { "dung0_ladderupdown",      0,0,0,0,0 },
    { "dung0_ladderupdown_side", 0,0,0,0,0 },
    { "dung1_ladderupdown",      0,0,0,0,0 },
    { "dung1_ladderupdown_side", 0,0,0,0,0 },
    { "dung2_ladderupdown",      0,0,0,0,0 },
    { "dung2_ladderupdown_side", 0,0,0,0,0 },
    { "dung3_ladderupdown",      0,0,0,0,0 },
    { "dung3_ladderupdown_side", 0,0,0,0,0 },
        // 72
    { "dung1_xxx_ew", 0,0,0,0,0 },
    { "dung1_xxx_ns", 0,0,0,0,0 },
    { "dung2_xxx_ew", 0,0,0,0,0 },
    { "dung2_xxx_ns", 0,0,0,0,0 },
    { "dung3_xxx_ew", 0,0,0,0,0 },
    { "dung3_xxx_ns", 0,0,0,0,0 },
        // 78
    { "dung0_hole",   0,0,0,0,0 },
    { "dung1_hole",   0,0,0,0,0 },
    { "dung2_hole",   0,0,0,0,0 },
    { "dung0_pit",    0,0,0,0,0 },
    { "dung1_pit",    0,0,0,0,0 },
    { "dung2_pit",    0,0,0,0,0 }
};

/*
 * Cache wall graphic pointers at setup to avoid lookup by name and image
 * loading during drawWall().
 */
void DungeonView::cacheGraphicData() {
    Symbol name;
    int i;

    for (i = 0; i < GRAPHIC_COUNT; ++i) {
        name = xu4.config->intern(dngGraphicInfo[i].imageName);
        graphic[i].info = xu4.imageMgr->imageInfo(name, &graphic[i].sub);
    }

    egaGraphics = (graphic[1].info->fixup == FIXUP_DUNGNS);
    apple2 = loadApple2Geometry();
}

static void drawGraphic(const ImageInfo* info, const SubImage* subimage,
                        int x, int y) {
    x += BORDER_WIDTH;
    y += BORDER_HEIGHT;

    if (subimage) {
        info->image->drawSubRect(x, y, subimage->x, subimage->y,
                                 subimage->width, subimage->height);
    } else
        info->image->draw(x, y);
}

void DungeonView::drawWall(int index) {
    const SubImage* subimage;
    int x, y;
    int i2;

    if (index < 0)
        return;
    if (! graphic[index].info)
        return;

    // TODO: Make all graphics subimages of a single atlas image. This cannot
    // be done until the screen render position of walls is separated from
    // their subimage position.

    subimage = graphic[index].sub;
    if (subimage) {
        x = subimage->x;
        y = subimage->y;
    } else {
        x = y = 0;
    }
    drawGraphic(graphic[index].info, subimage, x, y);

    // FIXME: subimage2 is a horrible hack, needs to be cleaned up
    i2 = dngGraphicInfo[index].subimage2;
    if (i2) {
        if (egaGraphics) {
            x = dngGraphicInfo[index].ega_x2;
            y = dngGraphicInfo[index].ega_y2;
        } else {
            x = dngGraphicInfo[index].vga_x2;
            y = dngGraphicInfo[index].vga_y2;
        }
        drawGraphic(graphic[i2].info, graphic[i2].sub, x, y);
    }
}


//----------------------------------------------------------------------------
// Apple II view

/*
 * The Apple II program draws its dungeon view with lines and filled pixel
 * columns on the hi-res screen.  The positions of the walls, doors, holes and
 * ladders at each distance are read from tables in the DNGD file of the
 * Underworld disk.  The graphics config names the tables with an image of
 * filetype a2dngd called "dung_apple2".
 */
enum A2GeomTable {
    A2G_WALL_LEFT     = 0,      // 5 bytes each
    A2G_WALL_RIGHT    = 5,
    A2G_WALL_TOP      = 10,
    A2G_WALL_BOTTOM   = 15,
    A2G_DOOR_LEFT0    = 20,     // 4 bytes each
    A2G_DOOR_LEFT1    = 24,
    A2G_DOOR_RIGHT0   = 28,
    A2G_DOOR_RIGHT1   = 32,
    A2G_DOOR_TOP0     = 36,
    A2G_DOOR_TOP1     = 40,
    A2G_DOOR_BOTTOM0  = 44,
    A2G_DOOR_BOTTOM1  = 48,
    A2G_CEIL_TOP      = 52,
    A2G_CEIL_BOTTOM   = 56,
    A2G_FLOOR_BOTTOM  = 60,
    A2G_FLOOR_TOP     = 64,
    A2G_HOLE_LEFT0    = 68,     // Near edge
    A2G_HOLE_LEFT1    = 72,     // Far edge
    A2G_HOLE_RIGHT0   = 76,
    A2G_HOLE_RIGHT1   = 80,
    A2G_LADDER_LEFT   = 84,
    A2G_LADDER_RIGHT  = 88,
    A2G_RUNG_UP       = 92,     // 3 tables: top, middle & bottom rung
    A2G_RUNG_DOWN     = 104,
    A2G_SIZE          = 116
};

#define A2_VIEW_TOP     8
#define A2_VIEW_BOTTOM  183
#define A2_VIEW_LEFT    7
#define A2_VIEW_RIGHT   160
#define A2_HORIZON      0x5f
#define A2_LADDER_SIDE  0x53

bool DungeonView::loadApple2Geometry() {
    const ImageInfo* info =
        xu4.imageMgr->lookup(xu4.config->intern("dung_apple2"));
    if (! info || info->filetype != FTYPE_A2DNGD)
        return false;

    const char* fn = xu4.config->confString(info->filename);
    if (strncmp(fn, "a2/", 3) != 0)
        return false;
    U4FILE* file = a2_open(fn + 3);
    if (! file)
        return false;
    size_t n = file->read(a2geom, 1, sizeof(a2geom));
    u4fclose(file);
    if (n != sizeof(a2geom))
        return false;

    // Check that the walls get smaller with distance.
    const uint8_t* g = a2geom;
    for (int d = 0; d < 4; ++d) {
        if (g[A2G_WALL_LEFT + d] >= g[A2G_WALL_LEFT + d + 1] ||
            g[A2G_WALL_RIGHT + d] <= g[A2G_WALL_RIGHT + d + 1] ||
            g[A2G_WALL_TOP + d] >= g[A2G_WALL_TOP + d + 1] ||
            g[A2G_WALL_BOTTOM + d] <= g[A2G_WALL_BOTTOM + d + 1])
            return false;
    }
    if (g[A2G_WALL_LEFT] != A2_VIEW_LEFT || g[A2G_WALL_TOP] != A2_VIEW_TOP)
        return false;
    return true;
}

// Pixel column fill modes.
enum A2Fill {
    A2_CLEAR,
    A2_EVEN_COLUMNS,
    A2_ODD_COLUMNS,
    A2_SOLID
};

struct A2Page {
    uint8_t bytes[192][40];

    void clear() {
        memset(bytes, 0, sizeof(bytes));
        for (int y = A2_VIEW_TOP; y <= A2_VIEW_BOTTOM; ++y)
            memset(bytes[y] + 1, 0x80, 22);
    }

    void pixel(int x, int y) {
        if (x >= 0 && x < 280 && y >= 0 && y < 192)
            bytes[y][x / 7] |= 1 << (x % 7);
    }

    void column(int x, int y0, int y1, A2Fill fill) {
        if (x < 0 || x >= 280)
            return;
        if (fill == A2_EVEN_COLUMNS || fill == A2_ODD_COLUMNS) {
            if ((x & 1) != (fill == A2_ODD_COLUMNS))
                return;
        }
        if (y0 > y1) {
            int t = y0; y0 = y1; y1 = t;
        }
        if (y0 < 0)
            y0 = 0;
        if (y1 > 191)
            y1 = 191;
        uint8_t bit = 1 << (x % 7);
        for (int y = y0; y <= y1; ++y) {
            uint8_t& b = bytes[y][x / 7];
            if (fill == A2_CLEAR)
                b &= ~bit;
            else
                b |= bit | 0x80;
        }
    }

    // Fill the area between two lines, column by column.  The top edge goes
    // from (x0, top0) to (x1, top1) and the bottom from (x0, bot0) to
    // (x1, bot1).
    void fill(int x0, int x1, int top0, int top1, int bot0, int bot1,
              A2Fill fill) {
        int w = abs(x1 - x0);
        int step = (x1 >= x0) ? 1 : -1;
        for (int i = 0; i <= w; ++i) {
            int top = top0, bot = bot0;
            if (w) {
                top += ((top1 - top0) * i + (w / 2) * sign(top1 - top0)) / w;
                bot += ((bot1 - bot0) * i + (w / 2) * sign(bot1 - bot0)) / w;
            }
            column(x0 + i * step, top, bot, fill);
        }
    }

    void line(int x0, int y0, int x1, int y1) {
        int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
        int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            pixel(x0, y0);
            if (x0 == x1 && y0 == y1)
                break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    static int sign(int n) { return (n > 0) - (n < 0); }
};

static void a2Hole(A2Page& pg, const uint8_t* g, int d, int nearY, int farY) {
    int nl = g[A2G_HOLE_LEFT0 + d], fl = g[A2G_HOLE_LEFT1 + d];
    int nr = g[A2G_HOLE_RIGHT0 + d], fr = g[A2G_HOLE_RIGHT1 + d];
    pg.line(nl,     nearY, fl,     farY);
    pg.line(nl + 1, nearY, fl + 1, farY);
    pg.line(fl + 1, farY,  fr,     farY);
    pg.line(fr,     farY,  nr,     nearY);
    pg.line(fr - 1, farY,  nr - 1, nearY);
    pg.line(nl,     nearY, nr - 1, nearY);
}

static void a2Ladder(A2Page& pg, const uint8_t* g, int d, bool up,
                     bool headOn) {
    int end = up ? g[A2G_CEIL_TOP + d] : g[A2G_FLOOR_BOTTOM + d];
    int mid = up ? A2_HORIZON : A2_HORIZON + 1;

    if (up)
        a2Hole(pg, g, d, g[A2G_CEIL_TOP + d], g[A2G_CEIL_BOTTOM + d]);
    else
        a2Hole(pg, g, d, g[A2G_FLOOR_BOTTOM + d], g[A2G_FLOOR_TOP + d]);

    if (! headOn) {
        pg.column(A2_LADDER_SIDE,     end, mid, A2_SOLID);
        pg.column(A2_LADDER_SIDE + 1, end, mid, A2_SOLID);
        return;
    }

    int left  = g[A2G_LADDER_LEFT + d];
    int right = g[A2G_LADDER_RIGHT + d];
    pg.column(left,      end, mid, A2_SOLID);
    pg.column(left + 1,  end, mid, A2_SOLID);
    pg.column(right,     end, mid, A2_SOLID);
    pg.column(right - 1, end, mid, A2_SOLID);

    const uint8_t* rung = g + (up ? A2G_RUNG_UP : A2G_RUNG_DOWN) + d;
    for (int i = 0; i < 3; ++i)
        pg.line(left, rung[i * 4], right, rung[i * 4]);
}

/*
 * Draw the view on the screen like the Apple II version.
 * Return the farthest distance that can be seen.
 */
int DungeonView::drawApple2(Context* c, Direction dir) {
    static A2Page pg;
    const Dungeon* dungeon = static_cast<const Dungeon*>(c->location->map);
    const uint8_t* g = a2geom;
    // Walls facing the viewer & side walls use different colors, which swap
    // when turning.
    int ew = (dir == DIR_EAST || dir == DIR_WEST);
    A2Fill frontFill = ew ? A2_ODD_COLUMNS  : A2_EVEN_COLUMNS;
    A2Fill sideFill  = ew ? A2_EVEN_COLUMNS : A2_ODD_COLUMNS;
    Coords loc;
    std::vector<MapTile> tiles;
    int d, seen = 0;

    pg.clear();
    if (c->party->getTorchDuration() > 0) {
        for (d = 0; d < 4; ++d) {
            seen = d;
            dungeonGetTiles(loc, tiles, d, 0);
            int token = dungeon->tokenAt(loc);

            int wl0 = g[A2G_WALL_LEFT + d],  wl1 = g[A2G_WALL_LEFT + d + 1];
            int wr0 = g[A2G_WALL_RIGHT + d], wr1 = g[A2G_WALL_RIGHT + d + 1];
            int wt0 = g[A2G_WALL_TOP + d],   wt1 = g[A2G_WALL_TOP + d + 1];
            int wb0 = g[A2G_WALL_BOTTOM + d], wb1 = g[A2G_WALL_BOTTOM + d + 1];
            bool stop = false;
            bool sides = true;      // Fronts that fill the view hide the sides.

            switch (token) {
            case DUNGEON_LADDER_UP:
                a2Ladder(pg, g, d, true, ew);
                break;
            case DUNGEON_LADDER_DOWN:
                a2Ladder(pg, g, d, false, ew);
                break;
            case DUNGEON_LADDER_UPDOWN:
                a2Ladder(pg, g, d, true, ew);
                a2Ladder(pg, g, d, false, ew);
                break;
            case DUNGEON_CEILING_HOLE:
                a2Hole(pg, g, d, g[A2G_CEIL_TOP + d], g[A2G_CEIL_BOTTOM + d]);
                break;
            case DUNGEON_FLOOR_HOLE:
                a2Hole(pg, g, d, g[A2G_FLOOR_BOTTOM + d],
                                 g[A2G_FLOOR_TOP + d]);
                break;
            case DUNGEON_FIELD:
                stop = true;
                sides = false;
                break;
            case DUNGEON_DOOR:
            case DUNGEON_ROOM:
                pg.fill(wl0, wr0, wt0, wt0, wb0, wb0, frontFill);
                // A door ahead, or the door frame when standing in it.
                pg.fill(wl1, wr1, wt1, wt1, wb0, wb0,
                        d ? A2_SOLID : A2_CLEAR);
                stop = (d > 0 || token == DUNGEON_ROOM);
                sides = false;
                break;
            case DUNGEON_SECRET_DOOR:
            case DUNGEON_WALL:
                pg.fill(wl0, wr0, wt0, wt0, wb0, wb0, frontFill);
                stop = true;
                sides = false;
                break;
            default:
                break;
            }

            for (int side = -1; sides && side <= 1; side += 2) {
                dungeonGetTiles(loc, tiles, d, side);
                int st = dungeon->tokenAt(loc);
                int x0 = (side < 0) ? wl0 : wr0;
                int x1 = (side < 0) ? wl1 : wr1;

                if (st < DUNGEON_DOOR) {
                    // Open; the wall across the corridor shows.
                    pg.fill(x0, x1, wt1, wt1, wb1, wb1, frontFill);
                    continue;
                }
                pg.fill(x0, x1, wt0, wt1, wb0, wb1, sideFill);
                if (st < DUNGEON_SECRET_DOOR) {
                    int t = (side < 0) ? A2G_DOOR_LEFT0 : A2G_DOOR_RIGHT0;
                    pg.fill(g[t + d], g[t + 4 + d],
                            g[A2G_DOOR_TOP0 + d], g[A2G_DOOR_TOP1 + d],
                            g[A2G_DOOR_BOTTOM0 + d], g[A2G_DOOR_BOTTOM1 + d],
                            A2_SOLID);
                }
            }

            if (stop)
                break;
        }
    }

    // The view is 154 x 176 pixels at Apple screen position 7, 8.
    a2_drawHgr(xu4.screenImage, x, y, pg.bytes[A2_VIEW_TOP],
               A2_VIEW_BOTTOM - A2_VIEW_TOP + 1, A2_VIEW_LEFT,
               A2_VIEW_LEFT + 153);
    return seen;
}
