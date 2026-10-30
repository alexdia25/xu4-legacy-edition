#include <math.h>
#include <cstring>
#include <algorithm>
#include <string>

#include "a2disk.h"
#include "c64disk.h"
#include "smsrom.h"
#include "config.h"
#include "docreader.h"
#include "worldmap.h"
#include "event.h"
#include "image32.h"
#include "intro.h"
#include "gamepad.h"
#include "gpu.h"
#include "module.h"
#include "settings.h"
#include "screen.h"
#include "sound.h"
#include "u4.h"
#include "u4file.h"
#include "xu4.h"

#include "gamebrowser.h"

using std::string;

extern "C" {
#include "processDir.c"
}


#define ATTR_COUNT  7

enum GuiBufferRegions {
    REGION_PANEL,
    REGION_POPUP,
    REGION_LIST
};

// Page layout (in page pixels).
#define EDGE        16
#define TITLE_Y     24
#define LIST_Y      58
#define ROW_H       17
#define LIST_ROWS   14
#define RULE_Y      300
#define ABOUT_Y     308
#define BUTTON_Y    342
#define FOOT_Y      370
#define HIT_ROW     100

static const char* buttonLabels[6] = {
    "Play", "Docs", "Map", "Settings", "Quit", "Cancel"
};

void GameBrowser::renderBrowser(ScreenState* ss, void* data)
{
    GameBrowser* gb = (GameBrowser*) data;
    if (gb->padHelp != gamepad_prompts())
        gb->redraw();
    gb->page.upload();
    gb->page.draw(ss);
}

GameBrowser::GameBrowser()
{
    sel = selGame = selMusic = 0;
    listTop = 0;
    pressed = -1;
    openSettings = false;
    padHelp = false;
    reader = NULL;
}

GameBrowser::~GameBrowser()
{
    delete reader;
}

#define NO_PARENT   255

// List rows that are not module files have modFileI set to NO_PARENT and
// resPathI set to one of these.
enum SpecialRow {
    ROW_GRAPHICS,       // Heading
    ROW_MUSIC,          // Heading
    ROW_GAME_MUSIC      // Music of the selected game module.
};

/*
 * Scroll the list so that the cursor row is in view.
 */
void GameBrowser::keepVisible()
{
    int count = infoList.size();
    if (sel < listTop)
        listTop = sel;
    else if (sel >= listTop + LIST_ROWS)
        listTop = sel - LIST_ROWS + 1;
    // Show the Graphics or Music heading above the first item under it.
    while (listTop > 0 && sel - (listTop - 1) < LIST_ROWS) {
        const ModuleInfo& mi = infoList[listTop - 1];
        if (mi.modFileI != NO_PARENT || mi.resPathI == ROW_GAME_MUSIC)
            break;
        --listTop;
    }
    if (listTop > count - LIST_ROWS)
        listTop = count - LIST_ROWS;
    if (listTop < 0)
        listTop = 0;
}

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
 * disk images), "c64" (the Program disk of Ultima IV for C64),
 * "c64remaster" (a disk image of Ultima IV Remastered for C64), "sms" (the
 * ROM of Ultima IV for the Sega Master System) or a file name looked for in
 * the resource paths.
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
        } else if (item == "c64") {
            if (! c64_haveDisk(C64_PROGRAM))
                return false;
        } else if (item == "c64remaster") {
            if (! c64_haveRemaster())
                return false;
        } else if (item == "sms") {
            if (! sms_haveRom())
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

// The font carries over from one row to the next, so the headings select
// font 1 for the module names below them.  (A trailing "\x12\x00" here
// was cut off by sst_append, which stops at the zero.)
static const char* specialLabel[3] = {
    "\x12\x01\x13\x09Graphics",
    "\x12\x01\x13\x09Music",
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
                // Green musical note symbol, then back to font 1.
                memcpy(itemText + len,
                       " \x12\x02\x13\x21N\x12\x01\x13\x00", 10);
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

void GameBrowser::addHit(int x, int y, int w, int h, int id)
{
    HitArea a;
    a.x = x;
    a.y = y;
    a.x2 = x + w;
    a.y2 = y + h;
    a.id = id;
    hits.push_back(a);
}

int GameBrowser::hitAt(int x, int y) const
{
    for (const HitArea& a : hits) {
        if (x >= a.x && x < a.x2 && y >= a.y && y < a.y2)
            return a.id;
    }
    return -1;
}

/*
 * Draw the module list like the title menus: the cursor beside the current
 * row, a green mark on the graphics & music being used.
 */
void GameBrowser::redraw()
{
    int W = page.width();
    int cw = page.cellWidth();
    int big = cw * 2;
    char mark;
    int i;

    hits.clear();
    page.clear();
    page.frame(0, 0, W, page.height(), 14);
    page.centered(TITLE_Y, "Graphics & Music", TextPage::WHITE, 2);
    page.fill(EDGE + 4, LIST_Y - 10, W - 2 * EDGE - 8, 2, TextPage::BLUE);

    int count = infoList.size();
    for (int r = 0; r < LIST_ROWS; ++r) {
        int n = listTop + r;
        if (n >= count)
            break;
        const ModuleInfo& mi = infoList[n];
        int y = LIST_Y + r * ROW_H;
        int x = EDGE + 8;

        if (n == sel) {
            mark = CHARSET_PROMPT;
            page.text(x, y, &mark, 1, TextPage::BLUE, 2);
        }
        addHit(EDGE, y - 1, W - 2 * EDGE, ROW_H, HIT_ROW + n);

        if (mi.modFileI == NO_PARENT && mi.resPathI != ROW_GAME_MUSIC) {
            const char* label = (mi.resPathI == ROW_MUSIC) ? "Music"
                                                           : "Graphics";
            page.text(x + 2 * big, y, label, TextPage::WHITE, 2);
            continue;
        }

        string name;
        if (mi.modFileI == NO_PARENT) {
            name = "Game music";
        } else {
            int len;
            const char* file = sst_stringL(&modFiles, mi.modFileI, &len);
            name.assign(file, len - 4);     // Without .mod
        }
        int nx = x + 4 * big;
        if (mi.category == MOD_EXTENSION)
            nx += 2 * big;

        bool chosen = (n == selGame || n == selMusic);
        mark = chosen ? CHARSET_ORB : CHARSET_BULLET;
        page.text(nx - 2 * big, y, &mark, 1,
                  chosen ? TextPage::GREEN : TextPage::GREY, 2);
        page.text(nx, y, name.c_str(), (int) name.size(),
                  (n == sel) ? TextPage::WHITE : TextPage::TEXT, 2);

        if (mi.modFileI != NO_PARENT) {
            int vlen;
            const char* version = sst_stringL(&mi.modi, MI_VERSION, &vlen);
            string v = "v" + string(version, vlen);
            page.text(W - EDGE - 12 - page.textWidth(v.size()), y + 6,
                      v.c_str(), (int) v.size(), TextPage::GREY);
        }
    }

    // Scroll marks.
    if (listTop > 0)
        page.text(W - EDGE - 20, LIST_Y - 2, "^", TextPage::BLUE);
    if (listTop + LIST_ROWS < count)
        page.text(W - EDGE - 20, LIST_Y + LIST_ROWS * ROW_H - 10, "v",
                  TextPage::BLUE);

    page.fill(EDGE + 4, RULE_Y, W - 2 * EDGE - 8, 2, TextPage::BLUE);

    // About the module under the cursor, in up to three lines.
    if (sel < infoList.size()) {
        int len;
        const char* about = sst_stringL(&infoList[sel].modi, MI_ABOUT, &len);
        string text(about, len);
        for (char& ch : text)
            if (ch == '\n' || ch == '\t')
                ch = ' ';
        size_t cols = (W - 2 * (EDGE + 8)) / cw;
        size_t pos = 0;
        for (int line = 0; line < 3 && pos < text.size(); ++line) {
            while (pos < text.size() && text[pos] == ' ')
                ++pos;
            size_t end = pos + cols;
            if (end < text.size()) {
                size_t sp = text.rfind(' ', end);
                if (sp != string::npos && sp > pos)
                    end = sp;
            } else
                end = text.size();
            page.text(EDGE + 8, ABOUT_Y + line * 10, text.c_str() + pos,
                      (int) (end - pos), TextPage::TEXT);
            pos = end;
        }
    }

    // Buttons.
    int total = 0;
    for (i = 0; i < 6; ++i)
        total += strlen(buttonLabels[i]) + 4;
    int x = (W - (total - 2) * cw) / 2;
    for (i = 0; i < 6; ++i) {
        int w = (strlen(buttonLabels[i]) + 2) * cw;
        page.fill(x, BUTTON_Y - 3, w, 14, TextPage::DK_BLUE);
        page.text(x + cw, BUTTON_Y, buttonLabels[i], TextPage::WHITE);
        addHit(x, BUTTON_Y - 4, w, 16, WI_OK + i);
        x += w + 2 * cw;
    }

    padHelp = gamepad_prompts();
    const char* help = padHelp ?
        "Up/Down Choose  X Select  A Use  Y Docs  L Map  B Cancel" :
        "Up/Down Choose  Space Select  Enter Use  Esc Cancel";
    page.centered(FOOT_Y, help, TextPage::GREY);
}

bool GameBrowser::present()
{
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

    if (! page.begin())
        return false;
    listTop = 0;
    keepVisible();
    redraw();
    screenSetLayer(LAYER_TOP_MENU, renderBrowser, this);
    return true;
}

void GameBrowser::conclude()
{
    screenSetLayer(LAYER_TOP_MENU, NULL, NULL);
    page.release();
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
                redraw();
            }
            return true;

        case U4_UP:
            moveCursor(-1);
            return true;

        case U4_DOWN:
            moveCursor(1);
            return true;

        case 'd':
        case 'z':
        case U4_PAD_Y:
            soundPlay(SOUND_UI_CLICK);
            readDocuments();
            return true;

        case 'm':
        case U4_PAD_L:
            soundPlay(SOUND_UI_CLICK);
            showWorldMap();
            return true;

        case 's':
            // The settings are run by xu4_selectGame() once this closes.
            soundPlay(SOUND_UI_CLICK);
            openSettings = true;
            xu4.eventHandler->setControllerDone(true);
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

    // The reader replaced the layer.
    pressed = -1;
    redraw();
    screenSetLayer(LAYER_TOP_MENU, renderBrowser, this);
}

/*
 * Show the world map, then return to the module list.
 */
void GameBrowser::showWorldMap()
{
    worldMapShow();

    // The map replaced the layer.
    pressed = -1;
    redraw();
    screenSetLayer(LAYER_TOP_MENU, renderBrowser, this);
}

void GameBrowser::selectRow(int n)
{
    if (n >= 0 && n < (int) infoList.size() && pickable(n)) {
        sel = n;
        pick(n);
        soundPlay(SOUND_UI_CLICK);
        redraw();
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
        keepVisible();
        redraw();
    }
}

bool GameBrowser::inputEvent(const InputEvent* ev)
{
    const ScreenState* ss = screenState();
    int x, y;
    page.pagePos(ss, ev->x - ss->aspectX, (ss->displayH - ev->y) - ss->aspectY,
                 &x, &y);

    switch (ev->type) {
        case IE_MOUSE_PRESS:
            pressed = (ev->n == CMOUSE_LEFT) ? hitAt(x, y) : -1;
            break;

        case IE_MOUSE_RELEASE:
            if (ev->n == CMOUSE_LEFT && pressed >= 0 &&
                hitAt(x, y) == pressed) {
                // NOTE: Sound is only played on Cancel to avoid
                //       abrupt cutoff during reset/quit.
                switch (pressed) {
                case WI_OK:
                    keyPressed(U4_ENTER);
                    break;
                case WI_DOCS:
                    keyPressed('d');
                    break;
                case WI_MAP:
                    keyPressed('m');
                    break;
                case WI_SETTINGS:
                    keyPressed('s');
                    break;
                case WI_CANCEL:
                    soundPlay(SOUND_UI_CLICK);
                    keyPressed(U4_ESC);
                    break;
                case WI_QUIT:
                    xu4.eventHandler->quitGame();
                    break;
                default:
                    if (pressed >= HIT_ROW)
                        selectRow(pressed - HIT_ROW);
                    break;
                }
            }
            pressed = -1;
            break;

        case IE_MOUSE_WHEEL:
        {
            int count = infoList.size();
            listTop += (ev->y < 0) ? 2 : -2;
            if (listTop > count - LIST_ROWS)
                listTop = count - LIST_ROWS;
            if (listTop < 0)
                listTop = 0;
            redraw();
        }
            break;
    }
    return true;
}
