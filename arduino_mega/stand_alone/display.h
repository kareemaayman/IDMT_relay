#pragma once
/*
 * display.h  —  20×4 I²C LCD + 4×3 keypad standalone HMI
 *
 * Hardware:
 *   LCD  : 20×4, I²C backpack (PCF8574), default address 0x27
 *   Keys : 4×3 matrix, rows R0-R3 on Arduino pins, cols C0-C2 on Arduino pins
 *
 * Key mapping:
 *   1-9 / 0  →  digit / numeric entry
 *   *        →  Back / Cancel / open menu
 *   #        →  Confirm / Select / Enter
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "relay_curves.h"

/* ── I²C LCD address (try 0x27 or 0x3F) ── */
#define LCD_I2C_ADDR  0x27
#define LCD_COLS      20
#define LCD_ROWS      4

/* ── Keypad wiring — adjust pins to match your Mega layout ── */
#define KP_ROWS  4
#define KP_COLS  3

/* ── Menu states ── */
typedef enum {
    SCREEN_LIVE,          /* normal live-data view        */
    SCREEN_MENU,          /* top-level parameter list     */
    SCREEN_EDIT_STANDARD, /* pick IEC / IEEE              */
    SCREEN_EDIT_CURVE,    /* pick IEC or IEEE curve       */
    SCREEN_EDIT_TMS,      /* numeric entry                */
    SCREEN_EDIT_PICKUP,   /* numeric entry                */
    SCREEN_EDIT_INST,     /* numeric entry                */
    SCREEN_CONFIRM_RESET  /* confirm reset after trip     */
} ScreenState;

/* ── Top-level menu items ── */
#define MENU_ITEM_COUNT  7
typedef enum {
    MENU_STANDARD = 0,
    MENU_CURVE,
    MENU_TMS,
    MENU_PICKUP,
    MENU_INST,
    MENU_START,
    MENU_RESET
} MenuItem;

/* ════════════════════════════════════════════════════════════════
 *  RelayMenu class
 * ════════════════════════════════════════════════════════════════ */
class RelayMenu {
public:
    /* ── Constructor ── */
    RelayMenu(Standard      &std_ref,
              IEC_Curve     &iec_ref,
              IEEE_Curve    &ieee_ref,
              float         &tms_ref,
              float         &pickup_ref,
              float         &inst_ref);

    /* ── Lifecycle ── */
    void init();                        /* call once in setup()            */
    void tick();                        /* call every loop() — polls keys  */

    /* ── Live screen update (call from run_protection) ── */
    void update_live(float I, float M, const char *state_str, float remain_s);

    /* ── Public flags read by main.ino ── */
    bool m_latched;                     /* true = relay tripped & latched  */
    bool m_protection_enabled;          /* true = protection running       */

private:
    /* References to relay parameters in main */
    Standard   &m_std;
    IEC_Curve  &m_iec;
    IEEE_Curve &m_ieee;
    float      &m_tms;
    float      &m_pickup;
    float      &m_inst;

    /* LCD & keypad objects */
    LiquidCrystal_I2C m_lcd;
    Keypad            m_kp;

    /* Screen state */
    ScreenState m_screen;
    uint8_t     m_menu_idx;        /* highlighted menu row              */
    uint8_t     m_menu_scroll;     /* top visible menu row              */

    /* Numeric entry buffer */
    char    m_input_buf[12];       /* e.g. "13.50"                      */
    uint8_t m_input_len;

    /* Cursor blink */
    uint32_t m_blink_ms;
    bool     m_blink_on;

    /* Last live update — throttle redraws */
    uint32_t m_live_last_ms;
    float    m_last_I, m_last_M, m_last_remain;
    char     m_last_state[12];

    /* ── Internal helpers ── */
    void draw_menu();
    void draw_edit_standard();
    void draw_edit_curve();
    void draw_edit_numeric(const char *label, const char *unit,
                           float lo, float hi);
    void draw_live_force();
    void draw_confirm_reset();

    void handle_key_live(char k);
    void handle_key_menu(char k);
    void handle_key_edit_standard(char k);
    void handle_key_edit_curve(char k);
    void handle_key_edit_numeric(char k, float lo, float hi,
                                 float &target, ScreenState next);
    void handle_key_confirm_reset(char k);

    void input_buf_clear();
    float input_buf_to_float();

    void lcd_print_padded(const char *s, uint8_t width);
    void lcd_row(uint8_t row, const char *s); /* clear row, print s */
};

#endif /* DISPLAY_H */