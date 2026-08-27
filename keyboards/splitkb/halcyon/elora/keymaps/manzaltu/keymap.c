// Copyright 2026 Yoav Orot
// SPDX-License-Identifier: GPL-3.0-or-later

// The layers are defined in keymap.json. QMK generates a keymap.c from it and
// #includes this file at the end of the generated one (OTHER_KEYMAP_C), so this
// file holds only the custom logic and must not define keymaps[] itself.

#include QMK_KEYBOARD_H
#include "keymap.h"

// Custom double hold tap dance action
#define ACTION_TAP_DANCE_LAYER_DOUBLE_HOLD(layer1, layer2)                          \
    {                                                                               \
        .fn        = {NULL, td_double_hold_finished, td_double_hold_reset, NULL},   \
        .user_data = (void *)&((tap_dance_double_hold_t){layer1, layer2, TD_NONE}), \
    }

// Tap dance states
typedef enum { TD_NONE, TD_UNKNOWN, TD_SINGLE_TAP_OR_HOLD, TD_DOUBLE_TAP_OR_HOLD } td_state_t;

// Tap dance context
typedef struct {
    uint8_t    layer1;
    uint8_t    layer2;
    td_state_t state;
} tap_dance_double_hold_t;

// Double key combination macro
typedef struct {
    uint16_t kc1;
    uint16_t kc2;
} double_key_combination_t;

// Functions that control what our tap dance keys do
td_state_t cur_dance(tap_dance_state_t *state);
void       td_double_hold_finished(tap_dance_state_t *state, void *user_data);
void       td_double_hold_reset(tap_dance_state_t *state, void *user_data);

// Tap dance key action configuration
tap_dance_action_t tap_dance_actions[] = {
    // Hold for the nav layer, double tap and hold for the mouse layer
    [TD_NAV_MOUSE] = ACTION_TAP_DANCE_LAYER_DOUBLE_HOLD(_NAV, _MOUSE),
};

// Combination macro definitions
const double_key_combination_t combo_macros[] = {
    [MOUSE_UP_RIGHT - SAFE_RANGE]   = {.kc1 = MS_UP, .kc2 = MS_RGHT},
    [MOUSE_UP_LEFT - SAFE_RANGE]    = {.kc1 = MS_UP, .kc2 = MS_LEFT},
    [MOUSE_DOWN_RIGHT - SAFE_RANGE] = {.kc1 = MS_DOWN, .kc2 = MS_RGHT},
    [MOUSE_DOWN_LEFT - SAFE_RANGE]  = {.kc1 = MS_DOWN, .kc2 = MS_LEFT},
};

// Process custom macro keys
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Check if this is a defined macro key
    if (keycode >= SAFE_RANGE && keycode < CUSTOM_KEYCODES_END) {
        // Execute combo macro
        double_key_combination_t combo = combo_macros[keycode - SAFE_RANGE];

        if (record->event.pressed) {
            register_code(combo.kc1);
            register_code(combo.kc2);
        } else {
            unregister_code(combo.kc1);
            unregister_code(combo.kc2);
        }
    }

    return true;
}

// Determine the current tap dance state
td_state_t cur_dance(tap_dance_state_t *state) {
    if (state->count == 1) {
        return TD_SINGLE_TAP_OR_HOLD;
    } else if (state->count == 2) {
        return TD_DOUBLE_TAP_OR_HOLD;
    }

    return TD_UNKNOWN;
}

// Tap dance state finish
void td_double_hold_finished(tap_dance_state_t *state, void *user_data) {
    tap_dance_double_hold_t *td_user_data = (tap_dance_double_hold_t *)user_data;
    td_user_data->state                   = cur_dance(state);
    switch (td_user_data->state) {
        case TD_SINGLE_TAP_OR_HOLD:
            layer_on(td_user_data->layer1);
            break;
        case TD_DOUBLE_TAP_OR_HOLD:
            layer_on(td_user_data->layer2);
            break;
        default:
            break;
    }
}

// Tap dance state reset
void td_double_hold_reset(tap_dance_state_t *state, void *user_data) {
    tap_dance_double_hold_t *td_user_data = (tap_dance_double_hold_t *)user_data;
    switch (td_user_data->state) {
        case TD_SINGLE_TAP_OR_HOLD:
            layer_off(td_user_data->layer1);
            break;
        case TD_DOUBLE_TAP_OR_HOLD:
            layer_off(td_user_data->layer2);
            break;
        default:
            break;
    }

    td_user_data->state = TD_NONE;
}

#if defined(ENCODER_MAP_ENABLE)
// The Halcyon userspace adds a second (module) encoder slot to each half, so every
// layer has four entries: left, left module, right, right module.
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_MAIN]  = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_PGUP, KC_PGDN), ENCODER_CCW_CW(KC_PGUP, KC_PGDN)},
    [_SYM]   = {ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
    [_NUM]   = {ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
    [_NAV]   = {ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
    [_MOUSE] = {ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______), ENCODER_CCW_CW(_______, _______)},
};
#endif // ENCODER_MAP_ENABLE
