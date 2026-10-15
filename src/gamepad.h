/*
 * XU4 - Platform independent gamepad handling
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Each platform backend only fills in a PadState from its own API.
 * Everything else (dead zone, D-pad repeat, button mapping) happens here so
 * the game behaves the same on every platform.
 */

#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdint.h>

// Buttons in the standard (Xbox style) layout.  The SMS controller has
// two buttons; button 1 (cancel) is B and button 2 (pick) is A.
enum PadButton {
    PAD_A,
    PAD_B,
    PAD_X,
    PAD_Y,
    PAD_BACK,
    PAD_START,
    PAD_LSHOULDER,
    PAD_RSHOULDER,
    PAD_UP,
    PAD_DOWN,
    PAD_LEFT,
    PAD_RIGHT,
    PAD_BUTTON_COUNT
};

#define PAD_BIT(b)  (1 << (b))

struct PadState {
    uint32_t buttons;   // Mask of PAD_BIT(PadButton).
    float axisX;        // Left stick, -1.0 (left) to 1.0 (right).
    float axisY;        // Left stick, -1.0 (up) to 1.0 (down).
};

typedef void (*PadKeyFunc)(int key);

void gamepad_reset();
void gamepad_update(const PadState* ps, uint32_t msec, PadKeyFunc sendKey);
bool gamepad_inUse();

#endif
