/*
 * XU4 - Platform independent gamepad handling
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "event.h"
#include "gamepad.h"
#include "settings.h"
#include "xu4.h"

#define STICK_DEAD_ZONE 0.5f
#define REPEAT_DELAY    300     // Milliseconds before a held direction repeats.
#define REPEAT_RATE     150

struct Gamepad {
    uint32_t held;          // Buttons (including stick directions) down.
    uint32_t repeatTime;    // When the held direction repeats next.
    uint32_t scene;         // EventHandler::sceneCount at the press.
};

static Gamepad pad;
static bool padInUse = false;   // A gamepad has been used.
static bool padLast = false;    // The gamepad was used after the keyboard.

static void sendPadKey(PadKeyFunc sendKey, int key) {
    padInUse = padLast = true;
    sendKey(key);
}

/*
 * Return true once the gamepad has been used.  The console style menus are
 * used from then on, whichever device is used after that.
 */
bool gamepad_inUse() {
    return padInUse;
}

/*
 * Return true if the gamepad was used more recently than the keyboard, so
 * button prompts can name the device being played with.
 */
bool gamepad_prompts() {
    return padLast;
}

void gamepad_markUsed() {
    padInUse = padLast = true;
}

void gamepad_keyboardUsed() {
    padLast = false;
}

// Key sent when each button is pressed.
static int buttonKey(int b) {
    switch (b) {
        case PAD_A:     return U4_PAD_A;
        case PAD_B:     return U4_PAD_B;
        case PAD_X:     return U4_PAD_X;
        case PAD_Y:     return U4_PAD_Y;
        case PAD_BACK:  return U4_PAD_SELECT;
        case PAD_START: return U4_PAD_START;
        case PAD_LSHOULDER: return U4_PAD_L;
        case PAD_RSHOULDER: return U4_PAD_R;
        case PAD_UP:    return U4_UP;
        case PAD_DOWN:  return U4_DOWN;
        case PAD_LEFT:  return U4_LEFT;
        case PAD_RIGHT: return U4_RIGHT;
    }
    return 0;
}

const char* padActionNames[PadAct_Count + 1] = {
    "Nothing", "Action Menu", "Smart Enter", "Pass Turn", "Ztats",
    "World Map", "Journal", NULL
};

const char* padButtonNames[PadMap_Count] = {
    "A", "B", "X", "Y", "L", "R"
};

/*
 * Set the classic button layout: A the action menu, B nothing (it backs
 * out of questions), X pass, Y Ztats, L the world map, R Smart Enter.
 */
void gamepad_defaultMap(uint8_t* map) {
    map[PadMap_A] = PadAct_Actions;
    map[PadMap_B] = PadAct_None;
    map[PadMap_X] = PadAct_Pass;
    map[PadMap_Y] = PadAct_Ztats;
    map[PadMap_L] = PadAct_WorldMap;
    map[PadMap_R] = PadAct_Smart;
}

/*
 * Turn a gamepad button key into the key of its action in play.
 * Returns 0 for a button with nothing to do, or -1 if key is not one of
 * the mapped buttons.
 */
int gamepad_playKey(int key, const uint8_t* map) {
    int b;
    switch (key) {
        case U4_PAD_A: b = PadMap_A; break;
        case U4_PAD_B: b = PadMap_B; break;
        case U4_PAD_X: b = PadMap_X; break;
        case U4_PAD_Y: b = PadMap_Y; break;
        case U4_PAD_L: b = PadMap_L; break;
        case U4_PAD_R: b = PadMap_R; break;
        default:
            return -1;
    }
    switch (map[b]) {
        case PadAct_Actions:  return U4_ACTIONS;
        case PadAct_Smart:    return U4_SMART;
        case PadAct_Pass:     return U4_SPACE;
        case PadAct_Ztats:    return 'z';
        case PadAct_WorldMap: return U4_WORLD_MAP;
        case PadAct_Journal:  return 'j' + U4_ALT;
    }
    return 0;
}

#define DIR_MASK    (PAD_BIT(PAD_UP) | PAD_BIT(PAD_DOWN) | \
                     PAD_BIT(PAD_LEFT) | PAD_BIT(PAD_RIGHT))

void gamepad_reset() {
    pad.held = 0;
    pad.repeatTime = 0;
}

/*
 * Turn the current pad state into key presses.  Call once per input poll.
 */
void gamepad_update(const PadState* ps, uint32_t msec, PadKeyFunc sendKey)
{
    uint32_t buttons = ps->buttons;

    // The stick acts as a D-pad.  Only the dominant axis counts so that
    // diagonals don't send two directions.
    float ax = ps->axisX;
    float ay = ps->axisY;
    float fx = (ax < 0.0f) ? -ax : ax;
    float fy = (ay < 0.0f) ? -ay : ay;
    if (fx > STICK_DEAD_ZONE || fy > STICK_DEAD_ZONE) {
        if (fx > fy)
            buttons |= PAD_BIT((ax < 0.0f) ? PAD_LEFT : PAD_RIGHT);
        else
            buttons |= PAD_BIT((ay < 0.0f) ? PAD_UP : PAD_DOWN);
    }

    uint32_t pressed = buttons & ~pad.held;
    pad.held = buttons;

    for (int b = 0; b < PAD_BUTTON_COUNT; ++b) {
        if (pressed & PAD_BIT(b)) {
            int key = buttonKey(b);
            if (key)
                sendPadKey(sendKey, key);
            if (PAD_BIT(b) & DIR_MASK) {
                int delay;
                pad.scene = EventHandler::sceneCount;
                pad.repeatTime = msec + (xu4.settings->heldRepeat(&delay)
                                         ? delay : REPEAT_DELAY);
            }
        }
    }

    // Repeat a held direction.
    uint32_t dir = buttons & DIR_MASK;
    if (dir && ! (pressed & DIR_MASK) && (int32_t) (msec - pad.repeatTime) >= 0 &&
        ! EventHandler::repeatStopped(pad.scene)) {
        int delay, rate = xu4.settings->heldRepeat(&delay);
        pad.repeatTime = msec + (rate ? rate : REPEAT_RATE);
        for (int b = PAD_UP; b <= PAD_RIGHT; ++b) {
            if (dir & PAD_BIT(b)) {
                sendPadKey(sendKey, buttonKey(b));
                break;
            }
        }
    }
}
