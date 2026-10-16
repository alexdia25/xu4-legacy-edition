/*
 * XU4 - Platform independent gamepad handling
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "event.h"
#include "gamepad.h"

#define STICK_DEAD_ZONE 0.5f
#define REPEAT_DELAY    300     // Milliseconds before a held direction repeats.
#define REPEAT_RATE     150

struct Gamepad {
    uint32_t held;          // Buttons (including stick directions) down.
    uint32_t repeatTime;    // When the held direction repeats next.
};

static Gamepad pad;
static bool padInUse = false;   // A gamepad has been used.

static void sendPadKey(PadKeyFunc sendKey, int key) {
    padInUse = true;
    sendKey(key);
}

/*
 * Return true once the gamepad has been used.  The console style menus are
 * used from then on, whichever device is used after that.
 */
bool gamepad_inUse() {
    return padInUse;
}

// Key sent when each button is pressed.
static int buttonKey(int b) {
    switch (b) {
        case PAD_A:     return U4_ENTER;
        case PAD_B:     return U4_PAD_B;
        case PAD_X:     return U4_SPACE;
        case PAD_Y:     return 'z';         // Ztats
        case PAD_BACK:  return U4_PAD_SELECT;
        case PAD_START: return U4_PAD_START;
        case PAD_UP:    return U4_UP;
        case PAD_DOWN:  return U4_DOWN;
        case PAD_LEFT:  return U4_LEFT;
        case PAD_RIGHT: return U4_RIGHT;
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
            if (PAD_BIT(b) & DIR_MASK)
                pad.repeatTime = msec + REPEAT_DELAY;
        }
    }

    // Repeat a held direction.
    uint32_t dir = buttons & DIR_MASK;
    if (dir && ! (pressed & DIR_MASK) && (int32_t) (msec - pad.repeatTime) >= 0) {
        pad.repeatTime = msec + REPEAT_RATE;
        for (int b = PAD_UP; b <= PAD_RIGHT; ++b) {
            if (dir & PAD_BIT(b)) {
                sendPadKey(sendKey, buttonKey(b));
                break;
            }
        }
    }
}
