#include "controller.h"
#include "gui.h"

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
    StringTable modFiles;
    StringTable modFormat;
    std::vector<ModuleInfo> infoList;
    uint16_t sel;               // Cursor row.
    uint16_t selGame;           // Chosen graphics (game) module row.
    uint16_t selMusic;          // Chosen music row.
    uint16_t buttonMode;
    int16_t  buttonDown;
    int16_t  tipId;
    uint16_t tipTimer;
    uint16_t tipX;
    uint16_t tipY;
    GuiArea gbox[ WI_COUNT ];
    void* atree;
    DocReader* reader;
    WorkBuffer* work;
    float listScroll;
    float listScrollTarget;
    float dragScroll;           // listScrollTarget when the list was pressed.
    int   dragY;                // Where the list was pressed.
    bool  dragged;              // The list was dragged since it was pressed.
public:
    bool  openSettings;         // Settings was picked.
private:
    float lineHeight;
    float psizeList;            // List font point size.
    float descenderList;        // List font descender * point size.
    float itemHeightP;

    bool pickable(int row) const;
    void pick(int row);
    void moveCursor(int dir);
    void readDocuments();
    void showWorldMap();
    void selectModule(const GuiArea* area, int y);
    float maxScroll() const;
    void layout();
    void generateListItems();
    void animateScroll();
    float calcScrollTarget();
    void updateWidgetTip(const ScreenState*);
    int listRowAt(const GuiArea*, int screenY);

    static void renderBrowser(ScreenState* ss, void* data);
    static void displayReset(int, void*, void*);
};
