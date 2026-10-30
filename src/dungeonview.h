/*
 * dungeonview.h
 */

#ifndef DUNGEONVIEW_H
#define DUNGEONVIEW_H

#include <vector>
#include "direction.h"
#include "tileview.h"

typedef enum {
    DNGGRAPHIC_NONE,
    DNGGRAPHIC_WALL,
    DNGGRAPHIC_LADDERUP,
    DNGGRAPHIC_LADDERDOWN,
    DNGGRAPHIC_LADDERUPDOWN,
    DNGGRAPHIC_DOOR,
    DNGGRAPHIC_DNGTILE,
    DNGGRAPHIC_BASETILE,
    DNGGRAPHIC_TRAP
} DungeonGraphicType;

class Context;
class Dungeon;
class ImageInfo;
class SubImage;

class DungeonView : public TileView {
public:
    DungeonView(int x, int y, int columns, int rows);

    void cacheGraphicData();
    void display(Context * c, TileView *view);
    void detectTraps();

    bool toggle3DDungeonView() {
        return screen3dDungeonViewEnabled = ! screen3dDungeonViewEnabled;
    }

private:
    void drawInDungeon(const MapTile& mt, int x_offset, int distance,
                       Direction orientation);
    int graphicIndex(const Coords& loc, int xoffset, int distance,
                     Direction orientation, DungeonGraphicType type);
    DungeonGraphicType tilesToGraphic(const Dungeon*,
                                      const std::vector<MapTile> &tiles);
    void drawWall(int graphic);
    void markSeen(const Dungeon* dungeon);
    void drawTopDown(const Dungeon* dungeon, TileView* view);
    void drawMiniMap(const Dungeon* dungeon);
    bool loadApple2Geometry();
    void drawApple2(Context* c, Direction dir);
    int  apple2Shape(const MapTile& mt) const;
    int  apple2NamedShape(const char* name) const;

    struct GraphicData {
        const ImageInfo* info;
        const SubImage* sub;
    };

    MapTile black;
    MapTile avatar;
    TileId corridor;
    TileId up_ladder;
    TileId down_ladder;
    TileId updown_ladder;
    TileId wall;
    TileId secret_door;
    uint8_t seenNow[8];         // Cells in sight this turn (bit x of row y).
    int      viewBottom;
    int      spotTrapRange;
    uint32_t spotTrapTime;
    bool screen3dDungeonViewEnabled;
    bool egaGraphics;
    bool apple2;                // Draw the view like the Apple II version.
    GraphicData graphic[84];
    uint8_t a2data[168];        // Apple II view tables from the disk.
    std::vector<uint8_t> a2shapes;  // Apple II tile bytes (SHP0 + SHP1).
};

#endif /* DUNGEONVIEW_H */
