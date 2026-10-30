#include <vector>
#include "stringTable.h"
#include "controller.h"
#include "gui.h"
#include "textpage.h"

struct ModuleInfo {
    StringTable modi;
    uint8_t resPathI;
    uint8_t modFileI;
    uint8_t category;
    uint8_t parent;
};

bool moduleDataFound(const char* modulePath);

class DocReader;
struct TxfHeader;
struct ScreenState;
struct WorkBuffer;

class GameBrowser : public Controller {
public:
    GameBrowser();
    ~GameBrowser();

    /* controller functions */
    virtual bool present();
    virtual void conclude();
    virtual bool keyPressed(int key);
    virtual bool inputEvent(const InputEvent*);

    enum Widgets {
        WI_LIST,
        WI_OK,
        WI_DOCS,
        WI_MAP,
        WI_SETTINGS,
        WI_QUIT,
        WI_CANCEL,
        WI_COUNT
    };

private:
    struct HitArea {
        int16_t x, y, x2, y2;
        int16_t id;
    };

    TextPage page;
    StringTable modFiles;
    StringTable modFormat;
    std::vector<ModuleInfo> infoList;
    std::vector<HitArea> hits;
    uint16_t sel;               // Cursor row.
    uint16_t selGame;           // Chosen graphics (game) module row.
    uint16_t selMusic;          // Chosen music row.
    int listTop;                // First row in view.
    int pressed;                // HitArea id pressed by the pointer.
    DocReader* reader;
public:
    bool  openSettings;         // Settings was picked.
private:
    bool pickable(int row) const;
    void pick(int row);
    void moveCursor(int dir);
    void readDocuments();
    void showWorldMap();
    void selectRow(int n);
    void keepVisible();
    void redraw();
    void addHit(int x, int y, int w, int h, int id);
    int  hitAt(int x, int y) const;

    static void renderBrowser(ScreenState* ss, void* data);
};
