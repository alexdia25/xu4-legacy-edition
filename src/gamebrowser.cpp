#include <math.h>
#include <cstring>
#include <algorithm>
#include <string>

#include "a2disk.h"
#include "config.h"
#include "docreader.h"
#include "event.h"
#include "image32.h"
#include "intro.h"
#include "gpu.h"
#include "module.h"
#include "settings.h"
#include "screen.h"
#include "sound.h"
#include "u4file.h"
#include "xu4.h"

#include "gamebrowser.h"

extern "C" {
#include "processDir.c"
}


#define ATTR_COUNT  7

enum GuiBufferRegions {
    REGION_PANEL,
    REGION_POPUP,
    REGION_LIST
};

static inline bool insideArea(const GuiArea* area, int x, int y)
{
    return (x >= area->x && x < area->x2 &&
            y >= area->y && y < area->y2);
}

void GameBrowser::renderBrowser(ScreenState* ss, void* data)
{
    GameBrowser* gb = (GameBrowser*) data;
    WorkBuffer* work = gb->work;

    if (gb->tipTimer) {
        if (--gb->tipTimer == 0)
            gb->updateWidgetTip(ss);
    }
    if (work->dirty)
        gpu_updateWorkBuffer(xu4.gpu, GPU_DLIST_GUI, work);

    gpu_enableGui(xu4.gpu, gb->buttonDown, gb->buttonMode);
    gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region + REGION_PANEL);

    if (gb->modFormat.used) {
        const GuiArea* area = gb->gbox + WI_LIST;
        int box[4];

        gb->animateScroll();

        // Draw list items.
        box[0] = ss->aspectX + area->x;
        box[1] = ss->aspectY + area->y;
        box[2] = area->x2 - area->x;
        box[3] = area->y2 - area->y;

        gpu_setScissor(box);
        gpu_guiSetOrigin(xu4.gpu, area->x, area->y2 + gb->listScroll);
        gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region + REGION_LIST);
        gpu_guiSetOrigin(xu4.gpu, 0.0f, 0.0f);
        gpu_setScissor(NULL);
    }

    if (gb->tipId >= 0)
        gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region + REGION_POPUP);
}

GameBrowser::GameBrowser()
{
    int rsize[3];

    sel = selGame = selMusic = 0;
    buttonMode = 0;
    buttonDown = WID_NONE;
    tipId      = WID_NONE;
    tipTimer   = 0;
    tipX = tipY = 0;
    listScroll = listScrollTarget = 0.0f;
    psizeList = 20.0f;
    atree = NULL;
    reader = NULL;

    rsize[REGION_PANEL] = ATTR_COUNT * 6 * 100;
    rsize[REGION_POPUP] = ATTR_COUNT * 6 * 300;
    rsize[REGION_LIST]  = ATTR_COUNT * 6 * 400;
    work = gpu_allocWorkBuffer(rsize, 3);
}

GameBrowser::~GameBrowser()
{
    delete reader;
    gpu_freeWorkBuffer(work);
}

void GameBrowser::animateScroll()
{
    if (listScrollTarget != listScroll) {
        listScroll += (listScrollTarget - listScroll) * 0.2f;
        if (fabs(listScrollTarget - listScroll) < 0.5f)
            listScroll = listScrollTarget;
    }
}

float GameBrowser::calcScrollTarget()
{
    const GuiArea* area = gbox + WI_LIST;
    float areaH = area->y2 - area->y;
    float targ = (itemHeightP * sel) - (areaH * 0.5f);
    if (targ < 0.0f) {
        targ = 0.0f;
    } else {
        float endY = (itemHeightP * modFormat.used) - descenderList;
        if (endY < areaH) {
            targ = 0.0f;    // All items fit in view; don't scroll.
        } else {
            endY -= areaH;
            if (targ > endY)
                targ = endY;
        }
    }
    return listScrollTarget = targ;
}

void GameBrowser::updateWidgetTip(const ScreenState* ss)
{
    // Similar to screenMousePos().
    int x = tipX - ss->aspectX;
    int y = (ss->displayH - tipY) - ss->aspectY;

    tipId = WID_NONE;
    const GuiArea* area = gbox + WI_LIST;
    if (insideArea(area, x, y)) {
        int n = listRowAt(area, y);
        if (n >= 0 && n < (int) modFormat.used) {
            int len;
            const char* about = sst_stringL(&infoList[n].modi, MI_ABOUT, &len);
            //printf("about: %s\n", about);
            if (len > 290)
                len = 290;
            if (len && about[len-1] == '\n')
                --len;

            n = ss->displayW / 3;
            if (x > n)
                x = n;

            {
            float rect[4];
            TxfDrawState ds;
            ds.fontTable = ss->fontTable;
            float ox = float(x);
            float oy = float(y) - (itemHeightP * 1.3f);
            txf_begin(&ds, 0, psizeList * 0.66f, ox, oy);
            ds.colorIndex = COL_WHITE;

            float pad = psizeList * 0.3f;
            float* attr = gpu_beginRegion(work, REGION_POPUP);
            float* astart = attr;
            attr += 6 * ATTR_COUNT;
            attr = gui_emitText(&ds, attr, about, len);
            rect[0] = ox - pad;
            rect[1] = ds.y - pad;
            rect[2] = (ds.xMax - ox) + pad + pad;
            rect[3] = (oy + ds.lineSpacing - ds.y) + pad;
            gui_emitQuadCi(astart, rect, COL_SD_BROWN);
            gpu_endRegion(work, REGION_POPUP, attr);
            }

            tipId = WI_LIST;
        }
    }
}

#define NO_PARENT   255

struct ModuleSortContext {
    const StringTable* st;

    bool operator()(const ModuleInfo& a, const ModuleInfo& b) const
    {
        // Graphics (game) modules come before music.
        bool musicA = (a.category == MOD_SOUNDTRACK);
        bool musicB = (b.category == MOD_SOUNDTRACK);
        if (musicA != musicB)
            return musicB;

        if (a.modFileI == b.parent)
            return true;

        const char* nameA;
        const char* nameB;
        const char* files = sst_strings(st);
        int fi;

        if (a.parent == b.parent) {
            if (a.category != b.category)
                return a.category < b.category;

            // Compare module names.
            nameA = files + sst_start(st, a.modFileI);
            nameB = files + sst_start(st, b.modFileI);
        } else {
            // Compare names of parent modules.
            fi = (a.parent == NO_PARENT) ? a.modFileI : a.parent;
            nameA = files + sst_start(st, fi);
            fi = (b.parent == NO_PARENT) ? b.modFileI : b.parent;
            nameB = files + sst_start(st, fi);
        }
        /*
        printf("KR name %d (%d %d) %s  %d (%d %d) %s\n",
                a.modFileI, a.parent, a.category, nameA,
                b.modFileI, b.parent, b.category, nameB);
        */
        return strcmp(nameA, nameB) < 0;
    }
};

static int collectModFiles(const char* name, int type, void* user)
{
    if (type == PDIR_FILE || type == PDIR_LINK) {
        int len;
        if (mod_extension(name, &len))
            sst_append((StringTable*) user, name, len);
    }
    return PDIR_CONTINUE;
}

static bool isExtensionOf(const char* name, const char* /*version*/,
                          const StringTable* childModi)
{
    int len;
    const char* rules = sst_stringL(childModi, MI_RULES, &len);
    const char* pver = (const char*) memchr(rules, '/', len);
    return (pver && memcmp(name, rules, pver - rules) == 0);
}

/*
 * Return true if the game data named in the module "requires" string is
 * present.  Each space separated item is "apple2" (the Ultima IV Apple II
 * disk images) or a file name looked for in the resource paths.
 */
static bool hasRequiredData(const StringTable* modi)
{
    if (modi->used <= MI_REQUIRES)
        return true;

    int len;
    const char* it  = sst_stringL(modi, MI_REQUIRES, &len);
    const char* end = it + len;
    while (it != end) {
        if (*it == ' ') {
            ++it;
            continue;
        }
        const char* start = it;
        while (it != end && *it != ' ')
            ++it;
        std::string item(start, it - start);

        if (item == "apple2") {
            if (! a2_haveDisks())
                return false;
        } else if (item == "u4upgrad.zip") {
            if (! u4isUpgradeAvailable())
                return false;
        } else if (u4find_path(item.c_str(), NULL).empty()) {
            return false;
        }
    }
    return true;
}

/*
 * Return true if the game data that a module file requires is present.
 */
bool moduleDataFound(const char* modulePath)
{
    StringTable modi;
    sst_init(&modi, 5, 80);
    bool found = (mod_query(modulePath, &modi) == MOD_UNKNOWN) ||
                 hasRequiredData(&modi);
    sst_free(&modi);
    return found;
}

//#define TEST_LIST
#ifdef TEST_LIST
#define TEST_COUNT 6
static const char* testFile[TEST_COUNT] = {
    "Dark_Magic.mod",
    "Dark_Magic-Onward!.mod",
    "DM-SomeSoundtrack.mod",
    "DM-Music_1.mod",
    "DM-Music_2.mod",
    "Light_Magic.mod"
};
static const char testCategory[TEST_COUNT] = {
    MOD_BASE,
    MOD_EXTENSION,
    MOD_SOUNDTRACK,
    MOD_SOUNDTRACK,
    MOD_SOUNDTRACK,
    MOD_BASE
};
static const char* testInfo[TEST_COUNT*4] = {
    // about, author, rules, version
    "Dark Magic the adventure.\nIn the mists of time...",
    "xu4 developers", "Dark-Magic", "1.0",

    "The Dark Magic adventure continues...",
    "xu4 developers", "Dark_Magic/1.0", "1.0",

    "Dark Magic alternative music by Some Composer",
    "Some Composer", "Dark_Magic/1.0", "1.0",

    "Dark Magic alternative music by Another Composer",
    "Another Composer", "Dark_Magic/1.0", "1.0",

    "Dark Magic alternative music by Another Composer",
    "Another Composer", "Dark_Magic/1.0", "2.0",

    "Light Magic the adventure.\nIn the mists of time...",
    "xu4 developers", "Dark-Magic", "1.0-beta"
};

static int test_query(int n, StringTable* modInfo)
{
    int i = n * 4;
    int end = i + 4;
    for (; i < end; ++i)
        sst_append(modInfo, testInfo[i], -1);
    return testCategory[n];
}
#endif

// List rows that are not module files have modFileI set to NO_PARENT and
// resPathI set to one of these.
enum SpecialRow {
    ROW_GRAPHICS,       // Heading
    ROW_MUSIC,          // Heading
    ROW_GAME_MUSIC      // Music of the selected game module.
};

static const char* specialLabel[3] = {
    "\x12\x01\x13\x09Graphics\x12\x00",
    "\x12\x01\x13\x09Music\x12\x00",
    "    Game music"
};

static const char* specialAbout[3] = {
    "Pick one game module for the graphics.",
    "Pick one soundtrack module, or the game music.",
    "Play the music of the game module."
};

static void insertSpecialRow(std::vector<ModuleInfo>& infoList, size_t pos,
                             int row)
{
    ModuleInfo info;
    sst_init(&info.modi, 4, 64);
    sst_append(&info.modi, specialAbout[row], -1);
    for (int i = MI_AUTHOR; i <= MI_VERSION; ++i)
        sst_append(&info.modi, "", 0);
    info.resPathI = row;
    info.modFileI = NO_PARENT;
    info.category = (row == ROW_GAME_MUSIC) ? MOD_SOUNDTRACK : MOD_UNKNOWN;
    info.parent   = NO_PARENT;
    infoList.insert(infoList.begin() + pos, info);
}

/*
 * Fill modFiles with module names and infoList with sorted information.
 * The modFormat strings match the infoList order and are edited for display
 * in the list widget.
 */
static void readModuleList(StringTable* modFiles, StringTable* modFormat,
                           std::vector<ModuleInfo>& infoList)
{
    char modulePath[256];
    ModuleInfo info;
    const StringTable* rp = &xu4.resourcePaths;
    const char* rpath;
    const char* files;
    uint32_t i, m;
#ifdef TEST_LIST
    uint32_t testM;
#endif
    int len;

    // Collect .mod files from resourcePaths.

    for (i = 0; i < rp->used; ++i) {
        m = modFiles->used;
        rpath = sst_stringL(rp, i, &len);
        processDir(rpath, collectModFiles, modFiles);

#ifdef TEST_LIST
        testM = modFiles->used;
        if (i == 0) {
            for (int t = 0; t < TEST_COUNT; ++t)
                sst_append(modFiles, testFile[t], -1);
        }
#endif

        memcpy(modulePath, rpath, len);
        modulePath[len] = '/';
        files = sst_strings(modFiles);
        for (; m < modFiles->used; ++m) {
            strcpy(modulePath + len + 1, files + sst_start(modFiles, m));

            sst_init(&info.modi, 4, 80);
            info.resPathI = i;
            info.modFileI = m;
#ifdef TEST_LIST
            if (m >= testM)
                info.category = test_query(m - testM, &info.modi);
            else
#endif
            info.category = mod_query(modulePath, &info.modi);
            info.parent   = NO_PARENT;

            // Modules are only listed if the game data they need is found.
            if (info.category == MOD_UNKNOWN || ! hasRequiredData(&info.modi))
                sst_free(&info.modi);
            else
                infoList.push_back(info);
        }
    }

    // Assign parents.

    files = sst_strings(modFiles);
    for (const auto& it : infoList) {
        if (it.category == MOD_BASE) {
            const char* name = files + sst_start(modFiles, it.modFileI);
            //const char* version = sst_stringL(it.modi, MI_VERSION, &len);

            for (auto& child : infoList) {
                if (child.category == MOD_BASE)
                    continue;
                if (isExtensionOf(name, NULL, &child.modi))
                    child.parent = it.modFileI;
            }
        }
    }

    // Build infoList with children sorted in alphabetical order under their
    // parents.

    {
    ModuleSortContext sortCtx;
    sortCtx.st = modFiles;
    sort(infoList.begin(), infoList.end(), sortCtx);
    }

    // Split the list into Graphics & Music sections.

    {
    size_t m = 0;
    while (m < infoList.size() && infoList[m].category != MOD_SOUNDTRACK)
        ++m;
    insertSpecialRow(infoList, m, ROW_GAME_MUSIC);
    insertSpecialRow(infoList, m, ROW_MUSIC);
    insertSpecialRow(infoList, 0, ROW_GRAPHICS);
    }

    // Create modFormat strings from infoList.  The .mod suffixes are removed
    // and child names are indented.

    const int indentLen = 8;
    strcpy(modulePath, "        ");

    for (const auto& it : infoList) {
        //printf("KR module %d %d/%d %s\n", it.category, it.modFileI, it.parent,
        //       files + sst_start(modFiles, it.modFileI));

        if (it.modFileI == NO_PARENT) {
            sst_append(modFormat, specialLabel[it.resPathI], -1);
            continue;
        }

        rpath = files + sst_start(modFiles, it.modFileI);

        // Strip .mod suffix.
        len = sst_len(modFiles, it.modFileI) - 4;

        memcpy(modulePath + indentLen, rpath, len);
        char* itemText;

        if (it.category != MOD_EXTENSION) {
            // Indent under the section heading.
            itemText = modulePath + 4;
            len += 4;
            if (it.category == MOD_SOUNDTRACK) {
                // Green musical note symbol.
                memcpy(itemText + len,
                       " \x12\x02\x13\x21N\x12\x00\x13\x00", 10);
                len += 10;
            }
        } else {
            // Indent extensions under their base module.
            itemText = modulePath;
            len += indentLen;
        }

        // Add module version number.
        {
        int vlen;
        const char* version = sst_stringL(&it.modi, MI_VERSION, &vlen);
        char* vp = itemText + len;
        *vp++ = '\t';
        *vp++ = 'v';
        memcpy(vp, version, vlen);
        len += vlen + 2;
        }

        sst_append(modFormat, itemText, len);
    }
}

void GameBrowser::layout()
{
    static uint8_t browserGui[] = {
        LAYOUT_V, BG_COLOR_CI, COL_BLACK + 128,
        MARGIN_V_PER, 10, MARGIN_H_PER, 13, SPACING_PER, 6,
        BG_COLOR_CI, COL_BROWN,
        ARRAY_DT_AREA, WI_LIST,
        MARGIN_V_PER, 6,
            LAYOUT_H,
                FONT_COLOR, COL_BEIGE,
                FONT_VSIZE, 38, LABEL_DT_S,
                FONT_N, 1,      LABEL_DT_S,
            LAYOUT_END,
            FONT_N, 0, FONT_VSIZE, 20, LIST_DIM, 32, 9, STORE_AREA,
            FROM_BOTTOM,
            FONT_N, 1, FONT_VSIZE, 22,
            LAYOUT_H, SPACING_PER, 5, FIX_WIDTH_EM, 52,
                BUTTON_DT_S, STORE_AREA,
                BUTTON_DT_S, STORE_AREA,
                BUTTON_DT_S, STORE_AREA,
                BUTTON_DT_S, STORE_AREA,
            LAYOUT_END,
        LAYOUT_END
    };
    const void* guiData[7];
    const void** data = guiData;
    const ScreenState* ss = screenState();

    // Set psizeList to match list FONT_VSIZE.
    psizeList = 20.0f * ss->aspectH / 480.0f;
    descenderList = ss->fontTable[0]->descender * psizeList;
    itemHeightP = lineHeight * psizeList;

    *data++ = gbox;
    *data++ = "xu4 | ";
    *data++ = "Game Modules";
    *data++ = "Play";
    *data++ = "Docs";
    *data++ = "Quit";
    *data   = "Cancel";

    TxfDrawState ds;
    ds.fontTable = ss->fontTable;
    float* attr = gpu_beginRegion(work, REGION_PANEL);
    attr = gui_layout(attr, NULL, &ds, browserGui, guiData);
    gpu_endRegion(work, REGION_PANEL, attr);

    free(atree);
    atree = gui_areaTree(gbox, WI_COUNT);

    generateListItems();
}

/*
 * Generate primitives for items in the module list.
 * The top of the list is at 0,0 to be placed with gpu_guiSetOrigin().
 */
void GameBrowser::generateListItems()
{
    ListDrawState ds;
    const GuiArea* area = gbox + WI_LIST;
    float* attr;
    float rect[4];

    ds.fontTable = screenState()->fontTable;
    txf_begin(&ds, 0, psizeList, 0.0f, 0.0f);

    // Highlight for selected item background.
    rect[0] = 0.0f;
    rect[1] = ds.lineSpacing * -(sel + 1) + descenderList;
    rect[2] = area->x2 - area->x;
    rect[3] = ds.lineSpacing;

    ListCellStyle* cell = ds.cell;
    cell->tabStop   = 0.0f;
    cell->fontScale = 1.0f;
    cell->color     = COL_WHITE;
    cell->selColor  = COL_TX_BLACK;
    ++cell;
    cell->tabStop   = rect[2] - (psizeList * 2.0f);
    cell->fontScale = 0.66f;
    cell->color     = COL_LT_GRAY;
    cell->selColor  = COL_TX_BLACK;

    ds.psizeList = psizeList;

    attr = gpu_beginRegion(work, REGION_LIST);
    attr = gui_emitQuadCi(attr, rect, COL_LT_BLUE);
    attr = gui_emitListItems(attr, &ds, &modFormat, sel);

    // Draw checkmarks on the chosen graphics & music.
    float rowH = ds.lineSpacing;
    ds.tf = ds.fontTable[2];
    txf_setFontSize(&ds, psizeList);
    if (selGame) {
        ds.x = 0.0f;
        ds.y = rowH * -(selGame + 1.0f);
        ds.colorIndex = (sel == selGame) ? COL_TX_BLACK : COL_WHITE;
        attr = gui_emitText(&ds, attr, "c", 1);
    }
    if (selMusic) {
        ds.x = 0.0f;
        ds.y = rowH * -(selMusic + 1.0f);
        ds.colorIndex = COL_LT_GREEN;
        attr = gui_emitText(&ds, attr, "c", 1);
    }
    gpu_endRegion(work, REGION_LIST, attr);
}

bool GameBrowser::present()
{
    lineHeight = screenState()->fontTable[0]->lineHeight;
    screenSetMouseCursor(MC_DEFAULT);
    screenShowMouseCursor(true);

    sst_init(&modFiles, 8, 128);
    sst_init(&modFormat, 8, 50);
    readModuleList(&modFiles, &modFormat, infoList);

    // Select the current modules.
    {
    int len;
    sel = selGame = selMusic = 0;

    for (size_t n = 0; n < infoList.size(); ++n) {
        const ModuleInfo& mi = infoList[n];
        if (mi.modFileI == NO_PARENT) {
            if (mi.resPathI == ROW_GAME_MUSIC && ! selMusic)
                selMusic = n;
            continue;
        }
        const char* mod = sst_stringL(&modFiles, mi.modFileI, &len);
        if (mi.category == MOD_SOUNDTRACK) {
            if (mod_namesEqual(xu4.settings->soundtrack, mod))
                selMusic = n;
        } else if (! selGame || mod_namesEqual(xu4.settings->game, mod)) {
            selGame = n;
        }
    }
    sel = selGame;
    }

    layout();
    listScroll = calcScrollTarget();
    screenSetLayer(LAYER_TOP_MENU, renderBrowser, this);
    return true;
}

void GameBrowser::conclude()
{
    free(atree);
    atree = NULL;

    screenSetLayer(LAYER_TOP_MENU, NULL, NULL);
    sst_free(&modFiles);
    sst_free(&modFormat);

    for (auto& it : infoList)
        sst_free(&it.modi);
    infoList.clear();

    if (! xu4.settings->mouseOptions.enabled)
        screenShowMouseCursor(false);
}

bool GameBrowser::keyPressed(int key)
{
    switch (key) {
        case U4_ENTER:
        {
            int len;
            const char* game;
            const char* music = "";

            pick(sel);
            if (! selGame)
                return true;

            // A soundtrack is only used with the game it was made for.
            const ModuleInfo& gm = infoList[selGame];
            const ModuleInfo& mm = infoList[selMusic];
            game = sst_stringL(&modFiles, gm.modFileI, &len);
            if (selMusic && mm.modFileI != NO_PARENT &&
                (mm.parent == gm.modFileI || mm.parent == gm.parent)) {
                music = sst_stringL(&modFiles, mm.modFileI, &len);
            }
            //printf( "KR Game '%s' '%s'\n", game, music);

            if (mod_namesEqual(xu4.settings->game, game)) {
                if (mod_namesEqual(xu4.settings->soundtrack, music)) {
                    xu4.eventHandler->setControllerDone(true);
                } else {
                    musicStop();
                    xu4.settings->setSoundtrack(music);
                    xu4.settings->write();
                    xu4.config->changeSoundtrack(music);
                    if (xu4.stage == StagePlay)
                        musicPlayLocale();
                    else
                        musicPlay(xu4.intro->selectedMusic());
                    xu4.eventHandler->setControllerDone(true);
                }
            } else {
                xu4.settings->setGame(game);
                xu4.settings->setSoundtrack(music);
                xu4.settings->write();
                xu4.eventHandler->quitGame();
                xu4.gameReset = 1;
            }
        }
            return true;

        case U4_SPACE:
            if (pickable(sel)) {
                pick(sel);
                soundPlay(SOUND_UI_CLICK);
                generateListItems();
            }
            return true;

        case U4_UP:
            moveCursor(-1);
            return true;

        case U4_DOWN:
            moveCursor(1);
            return true;

        case 'd':
        case 'z':       // Gamepad Y
            soundPlay(SOUND_UI_CLICK);
            readDocuments();
            return true;

        case U4_ESC:
            xu4.eventHandler->setControllerDone(true);
            return true;
    }
    return false;
}

/*
 * Show the document reader, then return to the module list.
 */
void GameBrowser::readDocuments()
{
    if (! reader)
        reader = new DocReader;
    xu4.eventHandler->runController(reader);

    // The reader replaced the GUI vertex data & layer.
    buttonDown = WID_NONE;
    tipId = WID_NONE;
    work->dirty = (1 << work->regionCount) - 1;
    screenSetLayer(LAYER_TOP_MENU, renderBrowser, this);
}

int GameBrowser::listRowAt(const GuiArea* area, int screenY)
{
    float y = screenY - listScroll - descenderList;
    float row = (float) (area->y2 - y) / itemHeightP;
    return (int) row;
}

void GameBrowser::selectModule(const GuiArea* area, int screenY)
{
    int n = listRowAt(area, screenY);
    if (n >= 0 && n < (int) modFormat.used && pickable(n)) {
        sel = n;
        pick(n);
        soundPlay(SOUND_UI_CLICK);
        generateListItems();
    }
}

bool GameBrowser::pickable(int row) const
{
    return row < (int) infoList.size() &&
           infoList[row].category != MOD_UNKNOWN;
}

/*
 * Choose a row as the graphics or music module.  Only one of each is used.
 */
void GameBrowser::pick(int row)
{
    if (pickable(row)) {
        if (infoList[row].category == MOD_SOUNDTRACK)
            selMusic = row;
        else
            selGame = row;
    }
}

// Move the cursor up or down, skipping the section headings.
void GameBrowser::moveCursor(int dir)
{
    int n = sel;
    do {
        n += dir;
    } while (n >= 0 && n < (int) modFormat.used && ! pickable(n));

    if (n >= 0 && n < (int) modFormat.used) {
        sel = n;
        soundPlay(SOUND_UI_TICK);
        calcScrollTarget();
        generateListItems();
    }
}

// Translate event mouse position to used area of screen.
static void screenMousePos(const InputEvent* ev, int& x, int& y)
{
    const ScreenState* ss = screenState();
    x = ev->x - ss->aspectX;
    y = (ss->displayH - ev->y) - ss->aspectY;
}

bool GameBrowser::inputEvent(const InputEvent* ev)
{
    int x, y;

    switch (ev->type) {
        case IE_MOUSE_PRESS:
        case IE_MOUSE_RELEASE:
            if (ev->n == CMOUSE_LEFT) {
                screenMousePos(ev, x, y);
                const GuiArea* hit = gui_pick(atree, gbox, x, y);
                if (hit) {
                    if (ev->type == IE_MOUSE_PRESS) {
                        buttonDown = hit->wid;
                        buttonMode = 1;

                        if (hit->wid == WI_LIST)
                            selectModule((const GuiArea*) hit, y);
                    } else {
                        if (buttonDown == hit->wid) {
                            // NOTE: Sound is only played on Cancel to avoid
                            //       abrupt cutoff during reset/quit.
                            switch (hit->wid) {
                            case WI_OK:
                                keyPressed(U4_ENTER);
                                break;
                            case WI_DOCS:
                                keyPressed('d');
                                break;
                            case WI_CANCEL:
                                soundPlay(SOUND_UI_CLICK);
                                keyPressed(U4_ESC);
                                break;
                            case WI_QUIT:
                                xu4.eventHandler->quitGame();
                                break;
                            }
                        }
                        buttonDown = WID_NONE;
                    }
                } else {
                    buttonDown = WID_NONE;
                }
            }
            break;

        case IE_MOUSE_MOVE:
            if (buttonDown >= WI_OK) {
                screenMousePos(ev, x, y);
                if (insideArea(gbox + buttonDown, x, y))
                    buttonMode = 1;
                else
                    buttonMode = 0;
            } else {
                tipId = WID_NONE;
                tipTimer = 24+5;   // ~1.2 seconds
                tipX = ev->x;
                tipY = ev->y;
            }
            break;

        case IE_MOUSE_WHEEL:
            if (ev->y < 0) {
                const GuiArea* area = gbox + WI_LIST;
                float areaH = area->y2 - area->y;
                float endY = (itemHeightP * modFormat.used) - descenderList;
                endY -= areaH;
                listScrollTarget += itemHeightP * 2.0f;     // Scroll down.
                if (listScrollTarget > endY)
                    listScrollTarget = endY;
            } else if (ev->y > 0) {
                listScrollTarget -= itemHeightP * 2.0f;     // Scroll up.
                if (listScrollTarget < 0.0f)
                    listScrollTarget = 0.0f;
            }
            break;
    }
    return true;
}
