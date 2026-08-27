// Copyright 2026 Yoav Orot
// SPDX-License-Identifier: GPL-3.0-or-later

// Declarations shared between keymap.json (which references the custom keycodes
// and the tap dance) and keymap.c. QMK includes this header automatically at the
// top of the keymap.c it generates from keymap.json.

#pragma once

#include QMK_KEYBOARD_H

// Layer index
enum layers { _MAIN, _SYM, _NUM, _NAV, _MOUSE };

// Custom macro keys
enum custom_keycodes { MOUSE_UP_RIGHT = SAFE_RANGE, MOUSE_UP_LEFT, MOUSE_DOWN_RIGHT, MOUSE_DOWN_LEFT, CUSTOM_KEYCODES_END };

// Tap dance keys
enum {
    TD_NAV_MOUSE,
};
