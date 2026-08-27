// Copyright 2026 Yoav Orot
// SPDX-License-Identifier: GPL-3.0-or-later

// The layers are defined in keymap.json. QMK generates a keymap.c from it and
// #includes this file at the end of the generated one (OTHER_KEYMAP_C), so this
// file holds only the custom logic and must not define keymaps[] itself.

#include QMK_KEYBOARD_H
#include "keymap.h"
#include "transactions.h"

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

// Caps Word state, mirrored from the master to the slave half. Key processing (and
// therefore Caps Word) only runs on the master, so the slave has to be told.
static bool caps_word_synced = false;

static void caps_word_sync_slave_handler(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(bool)) {
        memcpy(&caps_word_synced, in_data, sizeof(bool));
    }
}

void keyboard_post_init_user(void) {
    transaction_register_rpc(USER_SYNC_CAPS_WORD, caps_word_sync_slave_handler);
}

void housekeeping_task_user(void) {
    if (!is_keyboard_master()) {
        return;
    }

    static bool     synced     = false;
    static bool     last_state = false;
    static uint32_t last_try   = 0;
    bool            state      = is_caps_word_on();

    if ((!synced || state != last_state) && timer_elapsed32(last_try) > 50) {
        last_try = timer_read32();
        if (transaction_rpc_send(USER_SYNC_CAPS_WORD, sizeof(state), &state)) {
            last_state = state;
            synced     = true;
        }
    }
}

#ifdef HLC_TFT_DISPLAY
#    include "hlc_tft_display/hlc_tft_display.h"
#    include "hlc_tft_display/graphics/fonts/Retron2000-27.qff.h"
#    include "hlc_tft_display/graphics/fonts/Retron2000-underline-27.qff.h"
#    include "hlc_tft_display/graphics/numbers/0.qgf.h"
#    include "hlc_tft_display/graphics/numbers/1.qgf.h"
#    include "hlc_tft_display/graphics/numbers/2.qgf.h"
#    include "hlc_tft_display/graphics/numbers/3.qgf.h"
#    include "hlc_tft_display/graphics/numbers/4.qgf.h"
#    include "hlc_tft_display/graphics/numbers/undef.qgf.h"

// Caps Word state valid on either half
static bool caps_word_state(void) {
    return is_keyboard_master() ? is_caps_word_on() : caps_word_synced;
}


typedef struct {
    const uint8_t *image;
    uint8_t        h, s, v;
} layer_gfx_t;

// Layer number images and colours, reused from the stock display module
static const layer_gfx_t layer_gfx[] = {
    [_MAIN] = {gfx_0, HSV_LAYER_0}, [_SYM] = {gfx_1, HSV_LAYER_1}, [_NUM] = {gfx_2, HSV_LAYER_2}, [_NAV] = {gfx_3, HSV_LAYER_3}, [_MOUSE] = {gfx_4, HSV_LAYER_4},
};

// Replaces the stock screen: layer number plus a Caps Word indicator, without the
// Num/Scroll lock lines. Returning false skips the stock drawing.
bool display_module_housekeeping_task_user(bool second_display) {
    static bool                  first      = true;
    static layer_state_t         last_layer = 0;
    static bool                  last_caps  = false;
    static painter_font_handle_t font, font_underline;
    bool                         dirty = false;

    if (second_display) {
        return true; // Not the status display, keep the stock behaviour
    }

    if (first) {
        font           = qp_load_font_mem(font_Retron2000_27);
        font_underline = qp_load_font_mem(font_Retron2000_underline_27);
    }

    if (first || layer_state != last_layer) {
        uint8_t     layer = get_highest_layer(layer_state | default_layer_state);
        layer_gfx_t gfx   = layer < ARRAY_SIZE(layer_gfx) ? layer_gfx[layer] : (layer_gfx_t){gfx_undef, HSV_LAYER_UNDEF};

        painter_image_handle_t image = qp_load_image_mem(gfx.image);
        qp_drawimage_recolor(lcd_surface, 5, 5, image, gfx.h, gfx.s, gfx.v, HSV_BLACK);
        qp_close_image(image);

        last_layer = layer_state;
        dirty      = true;
    }

    bool caps = caps_word_state();
    if (first || caps != last_caps) {
        int16_t y = LCD_HEIGHT - font->line_height - 5;
        if (caps) {
            qp_drawtext_recolor(lcd_surface, 5, y, font_underline, "Caps", HSV_CAPS_ON, HSV_BLACK);
        } else {
            qp_drawtext_recolor(lcd_surface, 5, y, font, "Caps", HSV_CAPS_OFF, HSV_BLACK);
        }

        last_caps = caps;
        dirty     = true;
    }

    if (dirty) {
        qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    }

    first = false;
    return false;
}
#endif // HLC_TFT_DISPLAY

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
