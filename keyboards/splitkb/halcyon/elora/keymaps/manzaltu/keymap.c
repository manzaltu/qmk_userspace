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

// Display on/off choice, in the user EEPROM word because the Halcyon idle timeout
// overwrites the backlight's own EEPROM state. Zero (the reset default) is on.
typedef union {
    uint32_t raw;
    struct {
        bool display_off : 1;
    };
} user_config_t;

static user_config_t user_config;

// Re-apply the off choice after the Halcyon code turns the backlight back on.
// Only the master matters, QMK mirrors its backlight state to the slave.
static void display_apply_off(void) {
    if (is_keyboard_master() && user_config.display_off && is_backlight_enabled()) {
        backlight_set(0); // Cut the PWM before the EEPROM write below
        backlight_disable();
    }
}

static void display_toggle(void) {
    user_config.display_off = !user_config.display_off;
    eeconfig_update_user(user_config.raw);

    if (user_config.display_off) {
        backlight_disable();
    } else {
        backlight_enable();
        if (get_backlight_level() == 0) {
            backlight_level(BACKLIGHT_LEVELS); // Same fix-up as backlight_wakeup()
        }
    }
}

// State the display needs but only the master knows, since key processing runs
// there alone. Mirrored to the slave over a split transaction.
typedef struct {
    uint16_t key; // Last key pressed, KC_NO before the first one
    bool     key_held;
    bool     caps_word;
} display_state_t;

static display_state_t display_state;

// Process custom keys
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Remember the key for the display, apart from what the modifier row and the
    // layer name already show. Shifted symbols such as KC_AT keep their shift, the
    // display names them.
    uint16_t key = keycode;
    if (IS_QK_MODS(keycode) && QK_MODS_GET_MODS(keycode) != MOD_LSFT) {
        key = QK_MODS_GET_BASIC_KEYCODE(keycode);
    }
    if (key != KC_NO && !IS_MODIFIER_KEYCODE(key) && !IS_QK_MOMENTARY(key) && key != TD(TD_NAV_MOUSE)) {
        if (record->event.pressed) {
            display_state.key      = key;
            display_state.key_held = true;
        } else if (key == display_state.key) {
            display_state.key_held = false;
        }
    }

    if (keycode == DISPLAY_TOGGLE) {
        if (record->event.pressed) {
            display_toggle();
        }
        return false;
    }

    // Check if this is a defined macro key
    if (keycode >= SAFE_RANGE && keycode < SAFE_RANGE + ARRAY_SIZE(combo_macros)) {
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

static void display_sync_slave_handler(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(display_state)) {
        memcpy(&display_state, in_data, sizeof(display_state));
    }
}

void keyboard_post_init_user(void) {
    transaction_register_rpc(USER_SYNC_DISPLAY, display_sync_slave_handler);

    // The display module init turned the backlight on
    user_config.raw = eeconfig_read_user();
    display_apply_off();
}

void housekeeping_task_user(void) {
    if (!is_keyboard_master()) {
        return;
    }

    // After the idle timeout wakeup in halcyon.c
    display_apply_off();

    static display_state_t last_sent;
    static bool            synced   = false;
    static uint32_t        last_try = 0;

    display_state.caps_word = is_caps_word_on();
    if ((!synced || memcmp(&display_state, &last_sent, sizeof(display_state)) != 0) && timer_elapsed32(last_try) > 50) {
        last_try = timer_read32();
        if (transaction_rpc_send(USER_SYNC_DISPLAY, sizeof(display_state), &display_state)) {
            last_sent = display_state;
            synced    = true;
        }
    }
}

#ifdef HLC_TFT_DISPLAY
#    include "hlc_tft_display/hlc_tft_display.h"
#    include "hlc_tft_display/graphics/fonts/Retron2000-27.qff.h"
#    include "hlc_tft_display/graphics/fonts/Retron2000-underline-27.qff.h"

// Key line names: punctuation as the character itself, short names for the custom keys
KEYCODE_STRING_NAMES_USER(
    {KC_GRV, "`"}, {KC_MINS, "-"}, {KC_EQL, "="}, {KC_LBRC, "["}, {KC_RBRC, "]"}, {KC_BSLS, "\\"}, {KC_SCLN, ";"}, {KC_QUOT, "'"}, {KC_COMM, ","}, {KC_DOT, "."}, {KC_SLSH, "/"},
    {KC_TILD, "~"}, {KC_EXLM, "!"}, {KC_AT, "@"}, {KC_HASH, "#"}, {KC_DLR, "$"}, {KC_PERC, "%"}, {KC_CIRC, "^"}, {KC_AMPR, "&"}, {KC_ASTR, "*"}, {KC_LPRN, "("}, {KC_RPRN, ")"},
    {KC_UNDS, "_"}, {KC_PLUS, "+"}, {KC_LCBR, "{"}, {KC_RCBR, "}"}, {KC_PIPE, "|"}, {KC_COLN, ":"}, {KC_DQUO, "\""}, {KC_LT, "<"}, {KC_GT, ">"}, {KC_QUES, "?"},
    {MOUSE_UP_RIGHT, "MS_UR"}, {MOUSE_UP_LEFT, "MS_UL"}, {MOUSE_DOWN_RIGHT, "MS_DR"}, {MOUSE_DOWN_LEFT, "MS_DL"}, {DISPLAY_TOGGLE, "DISP"},
);

typedef struct {
    const char *name;
    uint8_t     h, s, v;
} layer_gfx_t;

// Layer names and the stock display module's layer colours
static const layer_gfx_t layer_gfx[] = {
    [_MAIN] = {"MAIN", HSV_LAYER_0}, [_SYM] = {"SYM", HSV_LAYER_1}, [_NUM] = {"NUM", HSV_LAYER_2}, [_NAV] = {"NAV", HSV_LAYER_3}, [_MOUSE] = {"MOUSE", HSV_LAYER_4},
};

typedef struct {
    const char *label;
    uint8_t     mask;
    uint8_t     hue;
} mod_gfx_t;

// Modifier row in Emacs notation, either side counts. Dim when released, bright and
// underlined when held, with the same saturation and value as the stock Caps colours.
#define HSV_MOD_OFF(hue) hue, 104, 77
#define HSV_MOD_ON(hue) hue, 191, 245

// Key line in white, dim once released
#define HSV_KEY_OFF 0, 0, 77
#define HSV_KEY_ON 0, 0, 245

static const mod_gfx_t mod_gfx[] = {
    {"C", MOD_MASK_CTRL, 0}, {"M", MOD_MASK_ALT, 85}, {"s", MOD_MASK_GUI, 142}, {"S", MOD_MASK_SHIFT, 202}, // Red, green, blue, magenta
};

// Replaces the stock screen: layer name, last key, modifier row and a Caps Word
// indicator, without the Num/Scroll lock lines. Returning false skips the stock drawing.
bool display_module_housekeeping_task_user(bool second_display) {
    static bool                  first      = true;
    static layer_state_t         last_layer = 0;
    static uint16_t              last_key   = KC_NO;
    static bool                  last_held  = false;
    static uint8_t               last_mods  = 0;
    static bool                  last_caps  = false;
    static painter_font_handle_t font, font_underline;
    bool                         dirty = false;

    if (second_display) {
        return true; // Not the status display, keep the stock behaviour
    }

    // Before the first draw: the backlight is turned on when the halves connect
    display_apply_off();

    if (first) {
        font           = qp_load_font_mem(font_Retron2000_27);
        font_underline = qp_load_font_mem(font_Retron2000_underline_27);
    }

    if (first || layer_state != last_layer) {
        uint8_t     layer = get_highest_layer(layer_state | default_layer_state);
        layer_gfx_t gfx   = layer < ARRAY_SIZE(layer_gfx) ? layer_gfx[layer] : (layer_gfx_t){"?", HSV_LAYER_UNDEF};

        // Names differ in width, so blank the line before drawing the new one
        qp_rect(lcd_surface, 5, 5, LCD_WIDTH - 1, 5 + font->line_height - 1, HSV_BLACK, true);
        qp_drawtext_recolor(lcd_surface, 5, 5, font, gfx.name, gfx.h, gfx.s, gfx.v, HSV_BLACK);

        last_layer = layer_state;
        dirty      = true;
    }

    if (first || display_state.key != last_key || display_state.key_held != last_held) {
        int16_t y = LCD_HEIGHT - 3 * font->line_height - 15;

        qp_rect(lcd_surface, 5, y, LCD_WIDTH - 1, y + font->line_height - 1, HSV_BLACK, true);
        if (display_state.key != KC_NO) {
            const char *name = get_keycode_string(display_state.key);
            char        label[12];

            if (strncmp(name, "KC_", 3) == 0) {
                name += 3; // Plain keys read better without the prefix
            }
            strncpy(label, name, sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';

            size_t len = strlen(label);
            while (len > 0 && qp_textwidth(font, label) > LCD_WIDTH - 10) {
                label[--len] = '\0'; // Clip what does not fit on the line
            }

            if (display_state.key_held) {
                qp_drawtext_recolor(lcd_surface, 5, y, font_underline, label, HSV_KEY_ON, HSV_BLACK);
            } else {
                qp_drawtext_recolor(lcd_surface, 5, y, font, label, HSV_KEY_OFF, HSV_BLACK);
            }
        }

        last_key  = display_state.key;
        last_held = display_state.key_held;
        dirty     = true;
    }

    // Real and one-shot modifiers only: weak ones are what QMK adds for a keycode
    // such as KC_AT, and those should not light up
    uint8_t mods = get_mods() | get_oneshot_mods();
    if (first || mods != last_mods) {
        int16_t y    = LCD_HEIGHT - 2 * font->line_height - 10;
        int16_t step = (LCD_WIDTH - 10) / ARRAY_SIZE(mod_gfx);

        for (uint8_t i = 0; i < ARRAY_SIZE(mod_gfx); i++) {
            const mod_gfx_t *m = &mod_gfx[i];
            int16_t          x = 5 + i * step;
            if (mods & m->mask) {
                qp_drawtext_recolor(lcd_surface, x, y, font_underline, m->label, HSV_MOD_ON(m->hue), HSV_BLACK);
            } else {
                qp_drawtext_recolor(lcd_surface, x, y, font, m->label, HSV_MOD_OFF(m->hue), HSV_BLACK);
            }
        }

        last_mods = mods;
        dirty     = true;
    }

    bool caps = display_state.caps_word;
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
