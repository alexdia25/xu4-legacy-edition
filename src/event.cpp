/*
 * event.cpp
 */

#include <cassert>
#include <cctype>
#include <cstring>
#include <list>

#include "event.h"

#include "config.h"
#include "context.h"
#include "location.h"
#include "savegame.h"
#include "settings.h"
#include "gamepad.h"
#include "screen.h"
#include "sound.h"
#include "textview.h"
#include "u4.h"
#include "xu4.h"

using std::string;

#ifdef DEBUG
#include "irecord.c"
#endif

static void frameSleepInit(FrameSleep* fs, int frameDuration) {
    fs->frameInterval = frameDuration;
    fs->realTime = 0;
    for (int i = 0; i < 8; ++i)
        fs->ftime[i] = frameDuration;
    fs->ftimeSum = frameDuration * 8;
    fs->ftimeIndex = 0;
    fs->fsleep = frameDuration - 6;
}

/**
 * Constructs the event handler object.
 */
EventHandler::EventHandler(int gameCycleDuration, int frameDuration) :
    timerInterval(gameCycleDuration),
    runRecursion(0),
    updateScreen(NULL)
{
    controllerDone = ended = paused = false;
    anim_init(&flourishAnim, 64, NULL, NULL);
    anim_init(&fxAnim, 32, NULL, NULL);
    frameSleepInit(&fs, frameDuration);

#ifdef DEBUG
    irec_init(&inputRec);
#endif
}

EventHandler::~EventHandler() {
#ifdef DEBUG
    irec_endRecording(&inputRec);
#endif
    anim_free(&flourishAnim);
    anim_free(&fxAnim);
}

void EventHandler::setTimerInterval(int msecs) {
    timerInterval = msecs;
}

void EventHandler::runController(Controller* con) {
    if (con->present()) {
        pushController(con);
        run();
        popController();
        setControllerDone(false);
        con->conclude();
    }
}

/** Sets the controller exit flag for the event handler */
void EventHandler::setControllerDone(bool done)
{
    controllerDone = done;
}

/** Returns the current value of the controller exit flag */
bool EventHandler::getControllerDone() {
    return controllerDone;
}

/** Signals that the game should immediately exit. */
void EventHandler::quitGame() {
    ended = true;
    xu4.stage = StageExitGame;
}

TimedEventMgr* EventHandler::getTimer() { return &timedEvents; }

Controller *EventHandler::pushController(Controller *c) {
    controllers.push_back(c);
    int interval = c->getTimerInterval();
    if (interval)
        timedEvents.add(&Controller::timerCallback, interval, c);
    return c;
}

Controller *EventHandler::popController() {
    if (controllers.empty())
        return NULL;

    Controller* con = controllers.back();
    if (con->getTimerInterval())
        timedEvents.remove(&Controller::timerCallback, con);

    controllers.pop_back();
    if (con->deleteOnPop())
        delete con;

    return getController();
}

Controller *EventHandler::getController() const {
    if (controllers.empty())
        return NULL;

    return controllers.back();
}

void EventHandler::setController(Controller *c) {
    while (popController() != NULL) {}
    pushController(c);
}

const MouseArea* EventHandler::mouseAreaForPoint(int x, int y) const {
    int i;
    const MouseArea *areas = getMouseAreaSet();

    if (!areas)
        return NULL;

    screenPointToMouseArea(&x, &y);
    for (i = 0; areas[i].npoints != 0; i++) {
        if (pointInMouseArea(x, y, &(areas[i]))) {
            return &(areas[i]);
        }
    }
    return NULL;
}

void EventHandler::setScreenUpdate(void (*updateFunc)(void)) {
    updateScreen = updateFunc;
}

#define GPU_PAUSE

void EventHandler::togglePause() {
    if (paused) {
        paused = false;
    } else {
#ifdef GPU_PAUSE
        // Don't pause if Game Browser (or other LAYER_TOP_MENU user) is open.
        if (screenLayerUsed(LAYER_TOP_MENU))
            return;
#endif
        paused = true;
        // Set ended to break the run loop.  runPause() resets it so that
        // the game does not end.
        ended = true;
    }
}

void EventHandler::expose() {
    if (paused)
        screenSwapBuffers();
}

#include "support/getTicks.c"

#ifdef GPU_PAUSE
#include "gpu.h"
#include "gui.h"

static void renderPause(ScreenState* ss, void* data)
{
    gpu_drawGui(xu4.gpu, GPU_DLIST_GUI, WID_NONE, 0);
}
#endif

// Return true if game should end.
bool EventHandler::runPause() {
    Controller waitCon;

    soundSuspend(1);
    screenSetMouseCursor(MC_DEFAULT);
#ifdef GPU_PAUSE
    static uint8_t pauseGui[] = {
        LAYOUT_V, BG_COLOR_CI, 128,
        MARGIN_V_PER, 42, FONT_VSIZE, 38, LABEL_DT_S,
        LAYOUT_END
    };
    const void* guiData[1];
    guiData[0] = "\x13\xAPaused";
    TxfDrawState ds;
    ds.fontTable = screenState()->fontTable;
    float* attr = gpu_beginTris(xu4.gpu, GPU_DLIST_GUI);
    attr = gui_layout(attr, NULL, &ds, pauseGui, guiData);
    gpu_endTris(xu4.gpu, GPU_DLIST_GUI, attr);

    screenSetLayer(LAYER_TOP_MENU, renderPause, this);
#else
    screenTextAt(16, 12, "( Paused )");
    screenUploadToGPU();
#endif
    screenSwapBuffers();

    ended = false;
    do {
        msecSleep(333);
        handleInputEvents(&waitCon, NULL);
    } while (paused && ! ended);

#ifdef GPU_PAUSE
    screenSetLayer(LAYER_TOP_MENU, NULL, NULL);
#endif
    soundSuspend(0);
    return ended;
}

/*
 * Return non-zero if waitTime has been reached or passed.
 */
static int frameSleep(FrameSleep* fs, uint32_t waitTime) {
    uint32_t now;
    int32_t elapsed, elapsedLimit, frameAdjust;
    int i;

    now = getTicks();
    elapsed = now - fs->realTime;
    fs->realTime = now;

    elapsedLimit = fs->frameInterval * 8;
    if (elapsed > elapsedLimit)
        elapsed = elapsedLimit;

    i = fs->ftimeIndex;
    fs->ftimeSum += elapsed - fs->ftime[i];
    fs->ftime[i] = elapsed;
    fs->ftimeIndex = i ? i - 1 : FS_SAMPLES - 1;

    frameAdjust = int(fs->frameInterval) - fs->ftimeSum / FS_SAMPLES;
#if 0
    // Adjust by 1 msec.
    if (frameAdjust > 0) {
        if (fs->fsleep < fs->frameInterval - 1)
            ++fs->fsleep;
    } else if (frameAdjust < 0) {
        if (fs->fsleep)
            --fs->fsleep;
    }
#else
    // Adjust by 2 msec.
    if (frameAdjust > 0) {
        if (frameAdjust > 2)
            frameAdjust = 2;
        int sa = int(fs->fsleep) + frameAdjust;
        int limit = fs->frameInterval - 1;
        fs->fsleep = (sa > limit) ? limit : sa;
    } else if (frameAdjust < 0) {
        if (frameAdjust < -2)
            frameAdjust = -2;
        int sa = int(fs->fsleep) + frameAdjust;
        fs->fsleep = (sa < 0) ? 0 : sa;
    }
#endif

    if (waitTime && now >= waitTime)
        return 1;
#if 0
    printf("KR fsleep %d ela:%d avg:%d adj:%d\n",
           fs->fsleep, elapsed, fs->ftimeSum / FS_SAMPLES, frameAdjust);
#endif
    if (fs->fsleep)
        msecSleep(fs->fsleep);
    return 0;
}

/**
 * Delays program execution for the specified number of milliseconds.
 * This doesn't actually stop events, but it stops the user from interacting
 * while some important event happens (e.g., getting hit by a cannon ball or
 * a spell effect).
 *
 * This method is not expected to handle msec values of less than the display
 * refresh interval.
 *
 * \return true if game should exit.
 */
bool EventHandler::wait_msecs(unsigned int msec) {
    Controller waitCon;     // Base controller consumes key events.
    EventHandler* eh = xu4.eventHandler;
    uint32_t waitTime = getTicks() + msec;

    while (! eh->ended) {
        eh->handleInputEvents(&waitCon, NULL);
#ifdef DEBUG
        int key;
        while ((key = eh->recordedKey()))
            waitCon.notifyKeyPressed(key);
        eh->recordTick();
#endif
        if (eh->runTime >= eh->timerInterval) {
            eh->runTime -= eh->timerInterval;
            eh->timedEvents.tick();
        }
        eh->runTime += eh->fs.frameInterval;

        screenSwapBuffers();
        if (frameSleep(&eh->fs, waitTime))
            break;
    }

    return eh->ended;
}

/*
 * Execute the game with a deterministic loop until the current controller
 * is done or the game exits.
 *
 * \return true if game should exit.
 */
bool EventHandler::run() {
    if (updateScreen)
        (*updateScreen)();

    if (! runRecursion) {
        runTime = 0;
        fs.realTime = getTicks();
    }
    ++runRecursion;

resume:
    while (! ended && ! controllerDone) {
        handleInputEvents(NULL, updateScreen);
#ifdef DEBUG
        int key;
        while ((key = recordedKey())) {
            if (getController()->notifyKeyPressed(key) && updateScreen)
                (*updateScreen)();
        }
        recordTick();
#endif
        if (runTime >= timerInterval) {
            runTime -= timerInterval;
            timedEvents.tick();
        }
        runTime += fs.frameInterval;

        screenSwapBuffers();
        frameSleep(&fs, 0);
    }

    if (paused && ! runPause())
        goto resume;

    --runRecursion;
    return ended;
}

//----------------------------------------------------------------------------

/* TimedEvent functions */
TimedEvent::TimedEvent(TimedEvent::Callback cb, int i, void *d) :
    callback(cb),
    data(d),
    interval(i),
    current(0)
{}

TimedEvent::Callback TimedEvent::getCallback() const    { return callback; }
void *TimedEvent::getData()                             { return data; }

/**
 * Advances the timed event forward a tick.
 * When (current >= interval), then it executes its callback function.
 */
void TimedEvent::tick() {
    if (++current >= interval) {
        (*callback)(data);
        current = 0;
    }
}

/**
 * Destructs a timed event manager object.
 */
TimedEventMgr::~TimedEventMgr() {
    while(! events.empty()) {
        delete events.front(), events.pop_front();
    }
    while(! deferredRemovals.empty()) {
        delete deferredRemovals.front(), deferredRemovals.pop_front();
    }
}

/**
 * Adds a timed event to the event queue.
 */
void TimedEventMgr::add(TimedEvent::Callback callback, int interval, void *data) {
    events.push_back(new TimedEvent(callback, interval, data));
}

/**
 * Removes a timed event from the event queue.
 */
TimedEventMgr::List::iterator TimedEventMgr::remove(List::iterator i) {
    if (isLocked()) {
        deferredRemovals.push_back(*i);
        return i;
    }
    else {
        delete *i;
        return events.erase(i);
    }
}

void TimedEventMgr::remove(TimedEvent* event) {
    List::iterator i;
    for (i = events.begin(); i != events.end(); i++) {
        if ((*i) == event) {
            remove(i);
            break;
        }
    }
}

void TimedEventMgr::remove(TimedEvent::Callback callback, void *data) {
    List::iterator i;
    for (i = events.begin(); i != events.end(); i++) {
        if ((*i)->getCallback() == callback && (*i)->getData() == data) {
            remove(i);
            break;
        }
    }
}

/**
 * Runs each of the callback functions of the TimedEvents associated with this manager.
 */
void TimedEventMgr::tick() {
    List::iterator i;

    // Lock the event list so it cannot be modified during iteration.
    locked = true;

    for (i = events.begin(); i != events.end(); i++)
        (*i)->tick();

    locked = false;

    // Remove events that have been deferred for removal
    for (i = deferredRemovals.begin(); i != deferredRemovals.end(); i++)
        events.remove(*i);
}

void EventHandler::pushMouseAreaSet(const MouseArea *mouseAreas) {
    mouseAreaSets.push_front(mouseAreas);
}

void EventHandler::popMouseAreaSet() {
    if (mouseAreaSets.size())
        mouseAreaSets.pop_front();
}

/**
 * Get the currently active mouse area set off the top of the stack.
 */
const MouseArea* EventHandler::getMouseAreaSet() const {
    if (mouseAreaSets.size())
        return mouseAreaSets.front();
    else
        return NULL;
}

//----------------------------------------------------------------------------

/**
 * A controller to read a string, terminated by the enter key.
 */
class ReadStringController : public WaitableController<string> {
public:
    ReadStringController(int maxlen, int screenX, int screenY,
                         TextView* view = NULL,
                         const char* accepted_chars = NULL);

    virtual bool keyPressed(int key);

protected:
    int maxlen, screenX, screenY;
    TextView *view;
    uint8_t accepted[16];   // Character bitset.

    friend EventHandler;
};

#define TEST_BIT(bits, c)   (bits[c >> 3] & 1 << (c & 7))
#define MAX_BITS    128

static void addCharBits(uint8_t* bits, const char* cp) {
    int c;
    while ((c = *cp++))
        bits[c >> 3] |= 1 << (c & 7);
}

/**
 * @param maxlen the maximum length of the string
 * @param screenX the screen column where to begin input
 * @param screenY the screen row where to begin input
 * @param textView  Pointer to associated TextView or NULL.
 * @param accepted_chars a string characters to be accepted for input
 */
ReadStringController::ReadStringController(int maxlen, int screenX, int screenY, TextView* textView, const char* accepted_chars) {
    // Set 0-9, A-Z, a-z, backspace, new line, carriage return, & space.
    static const uint8_t alphaNumBitset[MAX_BITS/8] = {
        0x00, 0x25, 0x00, 0x00, 0x01, 0x00, 0xFF, 0x03,
        0xFE, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF, 0x07
    };

    this->maxlen = maxlen;
    this->screenX = screenX;
    this->screenY = screenY;
    view = textView;

    if (accepted_chars) {
        memset(accepted, 0, MAX_BITS/8);
        addCharBits(accepted, accepted_chars);
    } else {
        memcpy(accepted, alphaNumBitset, MAX_BITS/8);
    }
}

static void soundInvalidInput() {
    soundPlay(SOUND_BLOCKED);
}

bool ReadStringController::keyPressed(int key) {
    if (key < MAX_BITS && TEST_BIT(accepted, key)) {
        int len = value.length();

        if (key == U4_BACKSPACE) {
            if (len > 0) {
                /* remove the last character */
                value.erase(len - 1, 1);

                if (view) {
                    view->textAt(screenX + len - 1, screenY, " ");
                    view->setCursorPos(screenX + len - 1, screenY);
                } else {
                    screenHideCursor();
                    screenTextAt(screenX + len - 1, screenY, " ");
                    screenSetCursorPos(screenX + len - 1, screenY);
                    screenShowCursor();
                }
            } else
                soundInvalidInput();
        }
        else if (key == '\n' || key == '\r') {
            doneWaiting();
        }
        else if (key == U4_ESC) {
            value.erase(0, value.length());
            doneWaiting();
        }
        else if (len < maxlen) {
            /* add a character to the end */
            value += key;

            if (view) {
                view->textAtFmt(screenX + len, screenY, "%c", key);
                view->setCursorPos(screenX + len + 1, screenY);
            } else {
                screenHideCursor();
                screenTextAt(screenX + len, screenY, "%c", key);
                screenSetCursorPos(screenX + len + 1, screenY);
                c->col = len + 1;
                screenShowCursor();
            }
        }
        else
            soundInvalidInput();
        return true;
    } else {
        bool valid = EventHandler::defaultKeyHandler(key);
        if (! valid)
            soundInvalidInput();
        return valid;
    }
}

//----------------------------------------------------------------------------

/**
 * A controller to read a single key from a provided list.
 */
class ReadChoiceController : public WaitableController<int> {
public:
    ReadChoiceController(const string &choices);
    virtual bool keyPressed(int key);

protected:
    string choices;
};

ReadChoiceController::ReadChoiceController(const string &choices) {
    this->choices = choices;
}

bool ReadChoiceController::keyPressed(int key) {
    // isupper() accepts 1-byte characters, yet the modifier keys
    // (ALT, SHIFT, ETC) produce values beyond 255
    if ((key <= 0x7F) && (isupper(key)))
        key = tolower(key);

    if (choices.empty() || choices.find_first_of(key) < choices.length()) {
        // If the value is printable, display it
        const ScreenState* ss = screenState();
        if (ss->cursorVisible && key > ' ' && key <= 0x7F)
            screenShowCharPixel(toupper(key), ss->cursorX, ss->cursorY);
        value = key;
        doneWaiting();
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------

/**
 * A controller to read a direction enter with the arrow keys.
 */
class ReadDirController : public WaitableController<Direction> {
public:
    ReadDirController();
    virtual bool keyPressed(int key);
};

ReadDirController::ReadDirController() {
    value = DIR_NONE;
}

bool ReadDirController::keyPressed(int key) {
    Direction d = keyToDirection(key);
    bool valid = (d != DIR_NONE);

    switch(key) {
    case U4_ESC:
    case U4_SPACE:
    case U4_ENTER:
        value = DIR_NONE;
        doneWaiting();
        return true;

    default:
        if (valid) {
            value = d;
            doneWaiting();
            return true;
        }
        break;
    }

    return false;
}

//----------------------------------------------------------------------------

class AnyKeyController : public Controller {
public:
    void wait();
    void waitTimeout();
    virtual bool keyPressed(int key);
    virtual bool inputEvent(const InputEvent*);
    virtual void timerFired();
};

void AnyKeyController::wait() {
    timerInterval = 0;
    xu4.eventHandler->runController(this);
}

// Wait briefly (10 seconds) for a key press.
void AnyKeyController::waitTimeout() {
    timerInterval = 10000 / xu4.eventHandler->getTimerInterval();
    xu4.eventHandler->runController(this);
}

bool AnyKeyController::keyPressed(int key) {
    xu4.eventHandler->setControllerDone();
    return true;
}

bool AnyKeyController::inputEvent(const InputEvent* ev) {
    if (ev->type == IE_MOUSE_PRESS && ev->n == CMOUSE_LEFT)
        xu4.eventHandler->setControllerDone();
    return true;
}

void AnyKeyController::timerFired() {
    xu4.eventHandler->setControllerDone();
}

//----------------------------------------------------------------------------

/**
 * A controller to read a player number.
 */
class ReadPlayerController : public ReadChoiceController {
public:
    ReadPlayerController();
    ~ReadPlayerController();
    virtual bool keyPressed(int key);

    int getPlayer();
    int waitFor();
};

ReadPlayerController::ReadPlayerController() : ReadChoiceController("12345678 \033\n") {
}

ReadPlayerController::~ReadPlayerController() {
}

bool ReadPlayerController::keyPressed(int key) {
    bool valid = ReadChoiceController::keyPressed(key);
    if (valid) {
        if (value < '1' ||
            value > ('0' + c->saveGame->members))
            value = '0';
    } else {
        value = '0';
    }
    return valid;
}

int ReadPlayerController::getPlayer() {
    return value - '1';
}

int ReadPlayerController::waitFor() {
    ReadChoiceController::waitFor();
    return getPlayer();
}

//----------------------------------------------------------------------------

/**
 * A controller to handle input for commands requiring a letter
 * argument in the range 'a' - lastValidLetter.
 */
class AlphaActionController : public WaitableController<int> {
public:
    AlphaActionController(char letter, const string &p) : lastValidLetter(letter), prompt(p) {}
    bool keyPressed(int key);

private:
    char lastValidLetter;
    string prompt;
};

bool AlphaActionController::keyPressed(int key) {
    if (islower(key))
        key = toupper(key);

    if (key >= 'A' && key <= toupper(lastValidLetter)) {
        screenMessage("%c\n", key);
        value = key - 'A';
        doneWaiting();
    } else if (key == U4_SPACE || key == U4_ESC || key == U4_ENTER) {
        screenMessage("\n");
        value = -1;
        doneWaiting();
    } else {
        screenMessage("\n%s", prompt.c_str());
        return EventHandler::defaultKeyHandler(key);
    }
    return true;
}

//----------------------------------------------------------------------------

/**
 * A controller to pick one item from a vertical list drawn in the message
 * area, as the Sega Master System version does for its menus.
 * Up/Down move the cursor, Enter picks, Escape cancels.  A letter jumps to
 * the next item starting with it.
 */
class ListPickerController : public WaitableController<int> {
public:
    ListPickerController(const char* const* items, int count, int start,
                         int rows, int line)
        : items(items), count(count), rows(rows), line(line), top(0) {
        value = -1;
        sel = (start >= 0 && start < count) ? start : 0;
        scrollToSel();
    }

    virtual bool keyPressed(int key);
    void draw() const;

private:
    void scrollToSel() {
        if (sel < top)
            top = sel;
        else if (sel >= top + rows)
            top = sel - rows + 1;
    }

    const char* const* items;
    int count;
    int rows;
    int line;       // First message area line used.
    int top;        // First visible item.
    int sel;
};

void ListPickerController::draw() const {
    for (int i = 0; i < rows; ++i) {
        int n = top + i;
        int more = ' ';
        if (i == 0 && top > 0)
            more = '^';
        else if (i == rows - 1 && n < count - 1)
            more = 'v';
        screenTextAt(TEXT_AREA_X, TEXT_AREA_Y + line + i, "%c%-*.*s%c",
                     (n == sel) ? CHARSET_BULLET : ' ',
                     TEXT_AREA_W - 2, TEXT_AREA_W - 2,
                     (n < count) ? items[n] : "", more);
    }
}

bool ListPickerController::keyPressed(int key) {
    switch (key) {
    case U4_UP:
    case U4_LEFT:
        if (sel > 0)
            --sel;
        break;
    case U4_DOWN:
    case U4_RIGHT:
        if (sel < count - 1)
            ++sel;
        break;
    case U4_ENTER:
    case U4_KEYPAD_ENTER:
    case U4_SPACE:
        value = sel;
        doneWaiting();
        return true;
    case U4_ESC:
        value = -1;
        doneWaiting();
        return true;
    default:
        if (key < 0x80 && isalpha(key)) {
            key = toupper(key);
            for (int i = 1; i <= count; ++i) {
                int n = (sel + i) % count;
                if (toupper(items[n][0]) == key) {
                    sel = n;
                    break;
                }
            }
        } else
            return EventHandler::globalKeyHandler(key);
        break;
    }
    scrollToSel();
    draw();
    return true;
}

//----------------------------------------------------------------------------

/*
 * A controller to pick a number with the direction keys.
 * Up/Right add one, Down/Left subtract one, digits may also be typed.
 */
class NumberPickerController : public WaitableController<int> {
public:
    NumberPickerController(int maxValue, int x, int y)
        : maxValue(maxValue), x(x), y(y), num(0) {
        value = 0;
    }

    virtual bool keyPressed(int key);
    void draw() const {
        screenTextAt(x, y, "%c%d%c   ", '<', num, '>');
    }

private:
    int maxValue;
    int x, y;
    int num;
};

bool NumberPickerController::keyPressed(int key) {
    switch (key) {
    case U4_UP:
    case U4_RIGHT:
        num = (num < maxValue) ? num + 1 : 0;
        break;
    case U4_DOWN:
    case U4_LEFT:
        num = (num > 0) ? num - 1 : maxValue;
        break;
    case U4_ENTER:
    case U4_KEYPAD_ENTER:
    case U4_SPACE:
        value = num;
        doneWaiting();
        return true;
    case U4_ESC:
        value = 0;
        doneWaiting();
        return true;
    case U4_BACKSPACE:
        num /= 10;
        break;
    default:
        if (key >= '0' && key <= '9') {
            int n = num * 10 + key - '0';
            num = (n <= maxValue) ? n : key - '0';
        } else
            return EventHandler::globalKeyHandler(key);
        break;
    }
    draw();
    return true;
}

//----------------------------------------------------------------------------

/*
 * A controller to enter text by picking letters from a grid, so that words
 * (names, mantras, virtues, stone colors) can be entered with a gamepad.
 * The direction keys move, Enter or Space picks a cell, Escape deletes the
 * last letter (or cancels when there are none).  Typing on a keyboard
 * still works; Enter then finishes the word.
 *
 * The grid is drawn in a TextView, or the message area if view is NULL.
 */
class LetterGridController : public WaitableController<int> {
public:
    LetterGridController(int maxlen, int cols, TextView* view,
                         int textX, int textY, int gridX, int gridY)
        : maxlen(maxlen), cols(cols), view(view),
          textX(textX), textY(textY), gridX(gridX), gridY(gridY),
          row(0), col(0), typed(false) {
        value = 0;
        rows = (26 + cols - 1) / cols + 1;
    }

    virtual bool keyPressed(int key);
    void draw() const;
    void drawText() const;
    void clear() const;
    const std::string& text() const { return buf; }

private:
    int rowLength(int r) const {
        if (r == rows - 1)
            return 3;
        int left = 26 - r * cols;
        return (left < cols) ? left : cols;
    }
    void add(int ch) {
        if ((int) buf.size() < maxlen)
            buf += (char) ch;
        else
            soundInvalidInput();
    }
    void textAt(int x, int y, const char* text) const {
        if (view)
            view->textAt(x, y, text);
        else
            screenTextAt(TEXT_AREA_X + x, TEXT_AREA_Y + y, "%s", text);
    }

    std::string buf;
    int maxlen;
    int cols, rows;
    TextView* view;
    int textX, textY;
    int gridX, gridY;
    int row, col;
    bool typed;
};

static const char* gridSpecial[3] = { "Spc", "Del", "End" };

void LetterGridController::draw() const {
    char cell[40];
    int width = cols * 2;
    for (int r = 0; r < rows; ++r) {
        memset(cell, ' ', width);
        cell[width] = '\0';
        int len = rowLength(r);
        for (int i = 0; i < len; ++i) {
            if (r < rows - 1) {
                int x = i * 2;
                if (r == row && i == col)
                    cell[x] = CHARSET_BULLET;
                cell[x + 1] = 'A' + r * cols + i;
            } else {
                int x = i * 5;
                if (r == row && i == col)
                    cell[x] = CHARSET_BULLET;
                memcpy(cell + x + 1, gridSpecial[i], 3);
            }
        }
        textAt(gridX, gridY + r, cell);
    }
}

void LetterGridController::drawText() const {
    char line[40];
    snprintf(line, sizeof(line), "%s_%*s", buf.c_str(),
             maxlen - (int) buf.size(), "");
    textAt(textX, textY, line);
}

void LetterGridController::clear() const {
    char line[40];
    int width = cols * 2;
    memset(line, ' ', width);
    line[width] = '\0';
    for (int r = 0; r < rows; ++r)
        textAt(gridX, gridY + r, line);
}

bool LetterGridController::keyPressed(int key) {
    switch (key) {
    case U4_UP:
        row = (row > 0) ? row - 1 : rows - 1;
        break;
    case U4_DOWN:
        row = (row < rows - 1) ? row + 1 : 0;
        break;
    case U4_LEFT:
        col = (col > 0) ? col - 1 : rowLength(row) - 1;
        break;
    case U4_RIGHT:
        col = (col < rowLength(row) - 1) ? col + 1 : 0;
        break;
    case U4_ENTER:
    case U4_KEYPAD_ENTER:
    case U4_SPACE:
        if (typed && key != U4_SPACE) {
            doneWaiting();
            return true;
        }
        if (row < rows - 1)
            add('a' + row * cols + col);
        else if (col == 0)
            add(' ');
        else if (col == 1) {
            if (! buf.empty())
                buf.erase(buf.size() - 1);
        } else {
            doneWaiting();
            return true;
        }
        break;
    case U4_ESC:
        if (buf.empty()) {
            value = -1;
            doneWaiting();
            return true;
        }
        buf.erase(buf.size() - 1);
        break;
    case U4_BACKSPACE:
        if (! buf.empty())
            buf.erase(buf.size() - 1);
        break;
    default:
        if (key < 0x80 && isalpha(key)) {
            add(tolower(key));
            typed = true;
        } else
            return EventHandler::globalKeyHandler(key);
        break;
    }
    if (col >= rowLength(row))
        col = rowLength(row) - 1;
    draw();
    drawText();
    return true;
}

//----------------------------------------------------------------------------

/*
 * A controller to pick one of a few short items shown in place at the
 * cursor of a TextView.  The direction keys cycle, Enter or Space picks,
 * and typing the first letter of an item picks it directly.
 */
class InlineChoiceController : public WaitableController<int> {
public:
    InlineChoiceController(TextView* view, const char* const* items,
                           int count)
        : view(view), items(items), count(count), sel(0) {
        x = view->cursorX();
        y = view->cursorY();
        width = 0;
        for (int i = 0; i < count; ++i) {
            int len = strlen(items[i]);
            if (len > width)
                width = len;
        }
        value = 0;
    }

    virtual bool keyPressed(int key) {
        switch (key) {
        case U4_UP:
        case U4_LEFT:
            sel = (sel > 0) ? sel - 1 : count - 1;
            break;
        case U4_DOWN:
        case U4_RIGHT:
            sel = (sel < count - 1) ? sel + 1 : 0;
            break;
        case U4_ENTER:
        case U4_KEYPAD_ENTER:
        case U4_SPACE:
            value = sel;
            doneWaiting();
            return true;
        default:
            if (key < 0x80 && isalpha(key)) {
                for (int i = 0; i < count; ++i) {
                    if (tolower(items[i][0]) == tolower(key)) {
                        sel = value = i;
                        draw();
                        doneWaiting();
                        return true;
                    }
                }
            }
            return EventHandler::globalKeyHandler(key);
        }
        draw();
        return true;
    }

    void draw() const {
        view->textAtFmt(x, y, "%c%-*s", CHARSET_BULLET, width, items[sel]);
    }

    void drawChoice() const {
        view->textAtFmt(x, y, "%-*s", width + 1, items[value]);
    }

private:
    TextView* view;
    const char* const* items;
    int count;
    int sel;
    int x, y, width;
};

//----------------------------------------------------------------------------

/*
 * A controller that ignores keypresses
 */
class IgnoreKeysController : public Controller {
public:
    virtual bool keyPressed(int key) {
        EventHandler::globalKeyHandler(key);
        return true;
    }
};

//----------------------------------------------------------------------------

/**
 * Handles any and all keystrokes.
 * Generally used to exit the application, switch applications,
 * minimize, maximize, etc.
 */
bool EventHandler::globalKeyHandler(int key) {
    switch(key) {
    case U4_PAUSE:
    case U4_ALT + 'p':
        xu4.eventHandler->togglePause();
        return true;

    case U4_PAD_START:
        xu4.eventHandler->togglePause();
        return true;

    case U4_PAD_SELECT:
        // Same menu as Escape on the keyboard.  Pressed again it closes
        // the menu (or the document reader opened from it).
        if (screenLayerUsed(LAYER_TOP_MENU))
            xu4.eventHandler->setControllerDone(true);
        else
            xu4_selectGame();
        return true;

#if defined(MACOSX)
    case U4_META + 'q': /* Cmd+q */
    case U4_META + 'x': /* Cmd+x */
#endif
    case U4_ALT + 'x': /* Alt+x */
#if defined(WIN32)
    case U4_ALT + U4_FKEY + 3:
#endif
        xu4.eventHandler->quitGame();
        return true;

    default:
        return false;
    }
}

/**
 * A default key handler that should be valid everywhere
 */
bool EventHandler::defaultKeyHandler(int key) {
    switch (key) {
#ifdef DEBUG
    case '`':
        if (c && c->location) {
            const Location* loc = c->location;
            const Tile* tile = loc->map->tileTypeAt(loc->coords, WITH_OBJECTS);
            printf("x = %d, y = %d, level = %d, tile = %d (%s)\n",
                    loc->coords.x, loc->coords.y, loc->coords.z,
                    xu4.config->usaveIds()->ultimaId( MapTile(tile->id) ),
                    tile->nameStr());
        }
        break;
#endif

    case U4_ESC:
        xu4_selectGame();
        break;

    default:
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------

/*
 * Read a player number.
 * Return -1 if none is selected.
 */
int EventHandler::choosePlayer()
{
    if (smsMenus()) {
        const char* names[8];
        int count = c->saveGame->members;
        for (int i = 0; i < count; ++i)
            names[i] = c->saveGame->players[i].name;
        return chooseItem(names, count);
    }

    ReadPlayerController ctrl;
    xu4.eventHandler->pushController(&ctrl);
    return ctrl.waitFor();
}

/**
 * Handle input for commands requiring a letter argument in the
 * range 'a' - lastValidLetter.
 */
int EventHandler::readAlphaAction(char lastValidLetter, const char* prompt)
{
    AlphaActionController ctrl(lastValidLetter, prompt);
    xu4.eventHandler->pushController(&ctrl);
    return ctrl.waitFor();
}

/*
 * Read a single key from a provided list.
 */
char EventHandler::readChoice(const char* choices)
{
    if (smsMenus()) {
        // List the printable choices; control keys are left to the picker.
        char keys[64];
        int count = 0;
        for (const char* cp = choices; *cp && count < 63; ++cp) {
            if (*cp > ' ' && *cp < 0x7f)
                keys[count++] = *cp;
        }
        keys[count] = '\0';

        if (count == 2 && strchr(keys, 'y') && strchr(keys, 'n')) {
            static const char* yesNo[2] = { "Yes", "No" };
            int n = chooseItem(yesNo, 2);
            return (n < 0) ? U4_ESC : "yn"[n];
        }
        if (count > 0) {
            static char label[64][2];
            const char* items[64];
            for (int i = 0; i < count; ++i) {
                label[i][0] = toupper(keys[i]);
                label[i][1] = '\0';
                items[i] = label[i];
            }
            int n = chooseItem(items, count);
            return (n < 0) ? U4_ESC : keys[n];
        }
    }

    ReadChoiceController ctrl(choices);
    xu4.eventHandler->pushController(&ctrl);
    return ctrl.waitFor();
}

/*
 * Read a direction entered with the arrow keys.
 */
Direction EventHandler::readDir()
{
    ReadDirController ctrl;
    xu4.eventHandler->pushController(&ctrl);
    return ctrl.waitFor();
}

/**
 * Read an integer, terminated by the enter key.
 * Non-numeric keys are ignored.
 */
int EventHandler::readInt(int maxlen)
{
    if (smsMenus()) {
        int maxValue = 9;
        while (--maxlen > 0)
            maxValue = maxValue * 10 + 9;
        return chooseNumber(maxValue);
    }

    ReadStringController ctrl(maxlen, TEXT_AREA_X + c->col,
                                      TEXT_AREA_Y + c->line,
                              NULL, "0123456789 \n\r\010");

    xu4.eventHandler->pushController(&ctrl);
    string s = ctrl.waitFor();
    return static_cast<int>(strtol(s.c_str(), NULL, 10));
}

/*
 * Read a string, terminated by the enter key.
 */
const char* EventHandler::readString(int maxlen, const char *extraChars)
{
    assert(size_t(maxlen) < sizeof(xu4.eventHandler->readStringBuf));
    if (smsMenus())
        return readLetters(maxlen);
    ReadStringController ctrl(maxlen, TEXT_AREA_X + c->col,
                                      TEXT_AREA_Y + c->line);
    if (extraChars)
        addCharBits(ctrl.accepted, extraChars);

    xu4.eventHandler->pushController(&ctrl);
    ctrl.waitFor();

    char* buf = xu4.eventHandler->readStringBuf;
    strcpy(buf, ctrl.value.c_str());
    return buf;
}

/*
 * Read a string, terminated by the enter key.
 */
const char* EventHandler::readStringView(int maxlen, TextView *view,
                                         const char* extraChars) {
    assert(size_t(maxlen) < sizeof(xu4.eventHandler->readStringBuf));
    if (smsMenus()) {
        // Letter grid on the lines below the cursor.
        int x = view->cursorX();
        int y = view->cursorY();
        view->hideCursor();
        LetterGridController ctrl(maxlen, 13, view, x, y,
                                  (view->columns() - 26) / 2, y + 1);
        ctrl.draw();
        ctrl.drawText();
        xu4.eventHandler->pushController(&ctrl);
        int status = ctrl.waitFor();
        ctrl.clear();

        char* buf = xu4.eventHandler->readStringBuf;
        strcpy(buf, (status < 0) ? "" : ctrl.text().c_str());
        view->textAtFmt(x, y, "%-*s", maxlen + 1, buf);
        view->setCursorPos(x + strlen(buf), y);
        view->showCursor();
        return buf;
    }
    ReadStringController ctrl(maxlen, view->cursorX(), view->cursorY(),
                              view);
    if (extraChars)
        addCharBits(ctrl.accepted, extraChars);

    xu4.eventHandler->pushController(&ctrl);
    ctrl.waitFor();

    char* buf = xu4.eventHandler->readStringBuf;
    strcpy(buf, ctrl.value.c_str());
    return buf;
}

/*
 * Wait until a key is pressed.
 */
void EventHandler::waitAnyKey()
{
#if 1
    AnyKeyController ctrl;
    ctrl.wait();
#else
    ReadChoiceController ctrl("");
    xu4.eventHandler->pushController(&ctrl);
    ctrl.waitFor();
#endif
}

/*
 * Wait briefly (10 seconds) for a key press.
 */
void EventHandler::waitAnyKeyTimeout()
{
    AnyKeyController ctrl;
    ctrl.waitTimeout();
}

/*
 * Pick one of the items from a list shown in the message area.
 * Return the item index or -1 if cancelled.  The list is erased afterwards
 * and the text cursor left at the start of where it was.
 */
int EventHandler::chooseItem(const char* const* items, int count, int start,
                             int maxRows)
{
    if (count < 1)
        return -1;

    int rows = (count < maxRows) ? count : maxRows;
    if (rows > TEXT_AREA_H - 1)
        rows = TEXT_AREA_H - 1;

    // Reserve the lines, scrolling the message area as needed.
    if (c->col)
        screenCrLf();
    for (int i = 0; i < rows; ++i)
        screenCrLf();
    int line = c->line - rows;

    bool cursor = screenState()->cursorVisible;
    screenHideCursor();

    ListPickerController ctrl(items, count, start, rows, line);
    ctrl.draw();
    xu4.eventHandler->pushController(&ctrl);
    int n = ctrl.waitFor();

    for (int i = 0; i < rows; ++i)
        screenTextAt(TEXT_AREA_X, TEXT_AREA_Y + line + i, "%*s",
                     TEXT_AREA_W, "");
    c->line = line;
    c->col = 0;
    screenShowCursor(cursor);
    return n;
}

/*
 * Return true if the console (SMS) style menus are enabled.
 */
bool EventHandler::smsMenus()
{
    return xu4.settings->enhancementsOptions.smsInterface || gamepad_inUse();
}

/*
 * Pick one of a few short items at the cursor of a TextView.
 * Returns the item index.
 */
int EventHandler::chooseInline(TextView* view, const char* const* items,
                               int count)
{
    view->hideCursor();
    InlineChoiceController ctrl(view, items, count);
    ctrl.draw();
    xu4.eventHandler->pushController(&ctrl);
    int n = ctrl.waitFor();
    ctrl.drawChoice();
    return n;
}

/*
 * Pick a number from 0 to maxValue at the text cursor.
 * Returns 0 if cancelled.
 */
int EventHandler::chooseNumber(int maxValue)
{
    bool cursor = screenState()->cursorVisible;
    screenHideCursor();

    NumberPickerController ctrl(maxValue, TEXT_AREA_X + c->col,
                                TEXT_AREA_Y + c->line);
    ctrl.draw();
    xu4.eventHandler->pushController(&ctrl);
    int n = ctrl.waitFor();

    screenTextAt(TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line, "%d%*s",
                 n, 5, "");
    c->col += snprintf(NULL, 0, "%d", n);
    screenShowCursor(cursor);
    return n;
}

/*
 * Read a word by picking letters from a grid drawn in the message area.
 * The word is shown on its own line.  Returns an empty string if cancelled.
 */
const char* EventHandler::readLetters(int maxlen)
{
    if (maxlen > TEXT_AREA_W - 1)
        maxlen = TEXT_AREA_W - 1;

    const int cols = 7;
    const int rows = (26 + cols - 1) / cols + 1;

    // Reserve a line for the word and the grid lines.
    for (int i = 0; i <= rows; ++i)
        screenCrLf();
    int line = c->line - rows;
    LetterGridController ctrl(maxlen, cols, NULL, 0, line - 1, 0, line);

    bool cursor = screenState()->cursorVisible;
    screenHideCursor();

    ctrl.draw();
    ctrl.drawText();
    xu4.eventHandler->pushController(&ctrl);
    int status = ctrl.waitFor();

    char* buf = xu4.eventHandler->readStringBuf;
    strcpy(buf, (status < 0) ? "" : ctrl.text().c_str());

    ctrl.clear();
    screenTextAt(TEXT_AREA_X, TEXT_AREA_Y + line - 1, "%-*s",
                 TEXT_AREA_W, buf);
    c->line = line - 1;
    c->col = strlen(buf);
    screenShowCursor(cursor);
    return buf;
}

/*
 * Ignore non-global key & mouse events forever.
 */
void EventHandler::ignoreInput()
{
    IgnoreKeysController ctrl;
    xu4.eventHandler->runController(&ctrl);
}
