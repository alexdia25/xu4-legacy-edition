/*
 * XU4 - World map viewer
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WORLDMAP_H
#define WORLDMAP_H

#include "controller.h"
#include "gui.h"

struct ScreenState;
struct WorkBuffer;

/*
 * Shows the whole of Britannia drawn with the tiles of the current module,
 * like the cloth map which came with the game.
 */
class WorldMapView : public Controller {
public:
    WorldMapView();
    ~WorldMapView();

    /* controller functions */
    virtual bool present();
    virtual void conclude();
    virtual bool keyPressed(int key);
    virtual bool inputEvent(const InputEvent*);

    enum Widgets {
        WI_MAP,
        WI_ZOOM_OUT,
        WI_ZOOM_IN,
        WI_LANG,
        WI_BACK,
        WI_COUNT
    };

private:
    GuiArea gbox[ WI_COUNT ];
    WorkBuffer* work;
    uint32_t mapTex;            // The whole world (256 x 256 tiles).
    uint32_t partyTex;          // The party tile.
    uint32_t moonTex[8];        // Moon phase characters.
    int partyX, partyY;         // Party map position, or -1 if unknown.
    int buttonDown;
    uint32_t frame;             // Frames drawn, for blinking.
    int dragX, dragY;           // Where the map was pressed.
    int mouseX, mouseY;         // Last pointer position.
    float dragCX, dragCY;       // View center when the map was pressed.
    bool dragged;
    bool english;               // Show the labels in English, not runes.
    int zoomLevel;
    int zoomMax;
    float fitScale;             // Pixels per tile with the whole map in view.
    float scale;                // Current pixels per tile (animated).
    float centerX, centerY;     // Map tile at the view center (animated).
    float targetX, targetY;
    float labelScale;           // scale & center when labels were made.
    float labelX, labelY;

    bool buildTextures();
    void freeTextures();
    void layout();
    void generateLabels();
    float targetScale() const;
    void clampCenter(float s, float& cx, float& cy) const;
    void zoomTo(int level, float px, float py);
    void pan(float dx, float dy);
    bool partyInView() const;
    void mapRect(float* rect) const;
    float markSize(float tiles) const;
    void markRect(int tx, int ty, float size, float* mrect) const;

    static void renderMap(ScreenState* ss, void* data);
};

void worldMapShow();

#endif
