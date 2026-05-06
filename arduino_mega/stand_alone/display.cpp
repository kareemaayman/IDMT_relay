/*
 * display.cpp  —  20×4 I²C LCD + 4×3 keypad standalone HMI
 *
 * Libraries required (install via Library Manager):
 *   - LiquidCrystal_I2C  by Frank de Brabander
 *   - Keypad             by Mark Stanley & Alexander Brevig
 *
 * Keypad wiring (adjust ROW_PINS / COL_PINS to your Mega):
 *
 *   4×3 matrix physical layout:
 *     [ 1 ][ 2 ][ 3 ]
 *     [ 4 ][ 5 ][ 6 ]
 *     [ 7 ][ 8 ][ 9 ]
 *     [ * ][ 0 ][ # ]
 *
 *   Row 0 (top)    → pin 30
 *   Row 1          → pin 31
 *   Row 2          → pin 32
 *   Row 3 (bottom) → pin 33
 *   Col 0 (left)   → pin 34
 *   Col 1 (mid)    → pin 35
 *   Col 2 (right)  → pin 36
 */

#include "display.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════
 *  Keypad layout
 * ══════════════════════════════════════════════════════════════ */
static const char KP_MAP[KP_ROWS][KP_COLS] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
};

/* Adjust these pin numbers to your wiring */
static byte ROW_PINS[KP_ROWS] = { 30, 31, 32, 33 };
static byte COL_PINS[KP_COLS] = { 34, 35, 36 };

/* ══════════════════════════════════════════════════════════════
 *  Menu label strings (stored in SRAM — Mega has 8 kB)
 * ══════════════════════════════════════════════════════════════ */
static const char *MENU_LABELS[MENU_ITEM_COUNT] = {
    "Standard (IEC/IEEE)",
    "Curve type",
    "TMS / TDS",
    "Pickup current (A)",
    "Inst. multiple (x)",
    ">>> START <<<",
    "--- RESET ---"
};

static const char *IEC_CURVE_NAMES[] = {
    "Standard Inv.",
    "Very Inverse",
    "Ext. Inverse",
    "Long-time Inv.",
    "UK LT Inv."
};

static const char *IEEE_CURVE_NAMES[] = {
    "Mod. Inverse",
    "Very Inverse",
    "Ext. Inverse"
};

/* ══════════════════════════════════════════════════════════════
 *  Constructor
 * ══════════════════════════════════════════════════════════════ */
RelayMenu::RelayMenu(Standard   &std_ref,
                     IEC_Curve  &iec_ref,
                     IEEE_Curve &ieee_ref,
                     float      &tms_ref,
                     float      &pickup_ref,
                     float      &inst_ref)
    : m_std(std_ref), m_iec(iec_ref), m_ieee(ieee_ref),
      m_tms(tms_ref), m_pickup(pickup_ref), m_inst(inst_ref),
      m_lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS),
      m_kp(makeKeymap(KP_MAP), ROW_PINS, COL_PINS, KP_ROWS, KP_COLS)
{
    m_latched            = false;
    m_protection_enabled = false;
    m_screen             = SCREEN_MENU;
    m_menu_idx           = 0;
    m_menu_scroll        = 0;
    m_input_len          = 0;
    m_input_buf[0]       = '\0';
    m_blink_ms           = 0;
    m_blink_on           = false;
    m_live_last_ms       = 0;
    m_last_I = m_last_M  = m_last_remain = 0.0f;
    m_last_state[0]      = '\0';
}

/* ══════════════════════════════════════════════════════════════
 *  init()
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::init()
{
    m_lcd.init();
    m_lcd.backlight();
    m_lcd.clear();

    /* Splash screen */
    m_lcd.setCursor(1, 0);  m_lcd.print(F("  IDMT RELAY v1.0   "));
    m_lcd.setCursor(0, 1);  m_lcd.print(F("  Arduino Mega 2560 "));
    m_lcd.setCursor(0, 2);  m_lcd.print(F("  20x4 + 4x3 Keypad "));
    m_lcd.setCursor(0, 3);  m_lcd.print(F(" Press any key...   "));
    delay(2000);

    m_lcd.clear();
    draw_menu();
}

/* ══════════════════════════════════════════════════════════════
 *  tick()  — call every loop()
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::tick()
{
    char k = m_kp.getKey();
    if (!k) return;          /* no key pressed */

    Serial.print(F("[KEY] "));
    Serial.println(k);       /* debug echo */

    switch (m_screen) {
        case SCREEN_LIVE:          handle_key_live(k);          break;
        case SCREEN_MENU:          handle_key_menu(k);          break;
        case SCREEN_EDIT_STANDARD: handle_key_edit_standard(k); break;
        case SCREEN_EDIT_CURVE:    handle_key_edit_curve(k);    break;
        case SCREEN_EDIT_TMS:
            handle_key_edit_numeric(k, 0.01f, 2.0f, m_tms, SCREEN_MENU);
            break;
        case SCREEN_EDIT_PICKUP:
            handle_key_edit_numeric(k, 0.1f, 100.0f, m_pickup, SCREEN_MENU);
            break;
        case SCREEN_EDIT_INST:
            handle_key_edit_numeric(k, INST_M_MIN, 30.0f, m_inst, SCREEN_MENU);
            break;
        case SCREEN_CONFIRM_RESET:
            handle_key_confirm_reset(k);
            break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  update_live()  — call from run_protection() each window
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::update_live(float I, float M, const char *state_str, float remain_s)
{
    if (m_screen != SCREEN_LIVE) return;

    /* Throttle: redraw at most every 250 ms to avoid LCD flicker */
    uint32_t now = millis();
    if ((now - m_live_last_ms) < 250) return;
    m_live_last_ms = now;

    m_last_I = I;  m_last_M = M;  m_last_remain = remain_s;
    strncpy(m_last_state, state_str, sizeof(m_last_state) - 1);

    draw_live_force();
}

/* ══════════════════════════════════════════════════════════════
 *  ── Live screen ──────────────────────────────────────────────
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::draw_live_force()
{
    char buf[LCD_COLS + 1];

    /* Row 0: current & multiple */
    snprintf(buf, sizeof(buf), "I=%-6.2fA  M=%-5.2f", m_last_I, m_last_M);
    lcd_row(0, buf);

    /* Row 1: pickup & TMS */
    snprintf(buf, sizeof(buf), "Ipk=%-5.2fA TMS=%-4.2f", m_pickup, m_tms);
    lcd_row(1, buf);

    /* Row 2: state + remaining */
    if (m_last_remain > 0.001f)
        snprintf(buf, sizeof(buf), "%-8s Trem=%-5.2fs", m_last_state, m_last_remain);
    else
        snprintf(buf, sizeof(buf), "%-20s", m_last_state);
    lcd_row(2, buf);

    /* Row 3: hint */
    lcd_row(3, "* = Menu            ");
}

void RelayMenu::handle_key_live(char k)
{
    if (k == '*') {
        m_screen = SCREEN_MENU;
        m_lcd.clear();
        draw_menu();
    }
}

/* ══════════════════════════════════════════════════════════════
 *  ── Top-level menu ───────────────────────────────────────────
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::draw_menu()
{
    m_lcd.clear();

    /* Header */
    m_lcd.setCursor(0, 0);
    m_lcd.print(F("=== RELAY SETTINGS =="));

    /* Show 3 visible items (rows 1-3), scrolling */
    for (uint8_t row = 0; row < 3; row++) {
        uint8_t idx = m_menu_scroll + row;
        if (idx >= MENU_ITEM_COUNT) break;

        m_lcd.setCursor(0, row + 1);

        /* Cursor indicator */
        if (idx == m_menu_idx)
            m_lcd.print(F(">"));
        else
            m_lcd.print(F(" "));

        /* Label, padded to 19 chars */
        char buf[LCD_COLS + 1];
        snprintf(buf, sizeof(buf), "%-19s", MENU_LABELS[idx]);
        m_lcd.print(buf);
    }
}

void RelayMenu::handle_key_menu(char k)
{
    if (k == '2' || k == '8') {          /* 2 = up, 8 = down (like d-pad) */
        if (k == '8' && m_menu_idx < MENU_ITEM_COUNT - 1) {
            m_menu_idx++;
            if (m_menu_idx >= m_menu_scroll + 3) m_menu_scroll++;
        } else if (k == '2' && m_menu_idx > 0) {
            m_menu_idx--;
            if (m_menu_idx < m_menu_scroll) m_menu_scroll--;
        }
        draw_menu();
        return;
    }

    if (k == '#') {                      /* confirm / enter */
        switch ((MenuItem)m_menu_idx) {

            case MENU_STANDARD:
                m_screen = SCREEN_EDIT_STANDARD;
                m_lcd.clear();
                draw_edit_standard();
                break;

            case MENU_CURVE:
                m_screen = SCREEN_EDIT_CURVE;
                m_lcd.clear();
                draw_edit_curve();
                break;

            case MENU_TMS:
                input_buf_clear();
                m_screen = SCREEN_EDIT_TMS;
                m_lcd.clear();
                draw_edit_numeric("TMS / TDS", "", 0.01f, 2.0f);
                break;

            case MENU_PICKUP:
                input_buf_clear();
                m_screen = SCREEN_EDIT_PICKUP;
                m_lcd.clear();
                draw_edit_numeric("Pickup current", "A", 0.1f, 100.0f);
                break;

            case MENU_INST:
                input_buf_clear();
                m_screen = SCREEN_EDIT_INST;
                m_lcd.clear();
                draw_edit_numeric("Inst. multiple", "x", INST_M_MIN, 30.0f);
                break;

            case MENU_START:
                m_protection_enabled = true;
                m_screen = SCREEN_LIVE;
                m_lcd.clear();
                lcd_row(0, "  PROTECTION ACTIVE ");
                lcd_row(1, "                    ");
                lcd_row(2, "   System running   ");
                lcd_row(3, "                    ");
                delay(1200);
                draw_live_force();
                break;

            case MENU_RESET:
                if (m_latched) {
                    m_screen = SCREEN_CONFIRM_RESET;
                    m_lcd.clear();
                    draw_confirm_reset();
                } else {
                    lcd_row(3, "Nothing to reset!   ");
                    delay(1000);
                    draw_menu();
                }
                break;
        }
        return;
    }

    if (k == '*') {                      /* back → live if protection running */
        if (m_protection_enabled) {
            m_screen = SCREEN_LIVE;
            m_lcd.clear();
            draw_live_force();
        }
        return;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  ── Standard picker (IEC / IEEE) ─────────────────────────────
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::draw_edit_standard()
{
    lcd_row(0, "-- Select Standard --");
    lcd_row(1, m_std == STD_IEC  ? "> IEC 60255        " : "  IEC 60255        ");
    lcd_row(2, m_std == STD_IEEE ? "> IEEE C37.112     " : "  IEEE C37.112     ");
    lcd_row(3, "#=Select  *=Cancel   ");
}

void RelayMenu::handle_key_edit_standard(char k)
{
    if (k == '1' || k == '2') {
        /* Toggle between the two options using arrow-style */
        if (k == '1') m_std = STD_IEC;
        if (k == '2') m_std = STD_IEEE;
        draw_edit_standard();
        return;
    }
    if (k == '#') {
        m_screen = SCREEN_MENU;
        m_lcd.clear();
        draw_menu();
        return;
    }
    if (k == '*') {
        m_screen = SCREEN_MENU;
        m_lcd.clear();
        draw_menu();
        return;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  ── Curve picker ─────────────────────────────────────────────
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::draw_edit_curve()
{
    lcd_row(0, "--- Select Curve ----");

    if (m_std == STD_IEC) {
        for (uint8_t i = 0; i < 4 && i < 3; i++) {   /* show 3 rows */
            char buf[LCD_COLS + 1];
            uint8_t idx = i;
            snprintf(buf, sizeof(buf), "%s%-18s",
                     (m_iec == (IEC_Curve)idx) ? ">" : " ",
                     IEC_CURVE_NAMES[idx]);
            lcd_row(i + 1, buf);
        }
    } else {
        for (uint8_t i = 0; i < 3; i++) {
            char buf[LCD_COLS + 1];
            snprintf(buf, sizeof(buf), "%s%-18s",
                     (m_ieee == (IEEE_Curve)i) ? ">" : " ",
                     IEEE_CURVE_NAMES[i]);
            lcd_row(i + 1, buf);
        }
    }
}

void RelayMenu::handle_key_edit_curve(char k)
{
    if (m_std == STD_IEC) {
        if (k >= '1' && k <= '5') {
            m_iec = (IEC_Curve)(k - '1');
            draw_edit_curve();
            return;
        }
    } else {
        if (k >= '1' && k <= '3') {
            m_ieee = (IEEE_Curve)(k - '1');
            draw_edit_curve();
            return;
        }
    }
    if (k == '#' || k == '*') {
        m_screen = SCREEN_MENU;
        m_lcd.clear();
        draw_menu();
    }
}

/* ══════════════════════════════════════════════════════════════
 *  ── Numeric editor ───────────────────────────────────────────
 *
 *  Entry format:  digits and one optional decimal point.
 *  '0'-'9' → append digit (max 7 chars)
 *  '*'     → append '.'  (only first decimal allowed)
 *  '##'    → confirm & validate
 *  Hold '*'→ backspace (handled as second '*' press)
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::draw_edit_numeric(const char *label, const char *unit,
                                   float lo, float hi)
{
    char buf[LCD_COLS + 1];

    snprintf(buf, sizeof(buf), "%-20s", label);
    lcd_row(0, buf);

    snprintf(buf, sizeof(buf), "Range: %.2f - %.2f %s", lo, hi, unit);
    lcd_row(1, buf);

    snprintf(buf, sizeof(buf), "Enter: %-13s", m_input_buf);
    lcd_row(2, buf);

    lcd_row(3, "#=OK *=dot/back     ");
}

void RelayMenu::handle_key_edit_numeric(char k, float lo, float hi,
                                         float &target, ScreenState next)
{
    if (k >= '0' && k <= '9') {
        if (m_input_len < 7) {
            m_input_buf[m_input_len++] = k;
            m_input_buf[m_input_len]   = '\0';
        }
        /* redraw just input row */
        char buf[LCD_COLS + 1];
        snprintf(buf, sizeof(buf), "Enter: %-13s", m_input_buf);
        lcd_row(2, buf);
        return;
    }

    if (k == '*') {
        /* First '*' = insert decimal point if not present yet */
        bool has_dot = (strchr(m_input_buf, '.') != nullptr);
        if (!has_dot && m_input_len > 0 && m_input_len < 7) {
            m_input_buf[m_input_len++] = '.';
            m_input_buf[m_input_len]   = '\0';
        } else if (m_input_len > 0) {
            /* Second '*' = backspace */
            m_input_buf[--m_input_len] = '\0';
        }
        char buf[LCD_COLS + 1];
        snprintf(buf, sizeof(buf), "Enter: %-13s", m_input_buf);
        lcd_row(2, buf);
        return;
    }

    if (k == '#') {
        if (m_input_len == 0) {
            /* Empty — cancel */
            m_screen = next;
            m_lcd.clear();
            draw_menu();
            return;
        }
        float val = atof(m_input_buf);
        if (val >= lo && val <= hi) {
            target   = val;
            m_screen = next;
            m_lcd.clear();
            lcd_row(0, "     Saved!         ");
            delay(800);
            draw_menu();
        } else {
            char buf[LCD_COLS + 1];
            snprintf(buf, sizeof(buf), "! Out of range !    ");
            lcd_row(3, buf);
            delay(1200);
            draw_edit_numeric("", "", lo, hi);   /* redraw with blank label */
        }
        return;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  ── Reset confirmation ───────────────────────────────────────
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::draw_confirm_reset()
{
    lcd_row(0, "!! CONFIRM RESET !!  ");
    lcd_row(1, "                    ");
    lcd_row(2, "  #=YES    *=NO     ");
    lcd_row(3, "                    ");
}

void RelayMenu::handle_key_confirm_reset(char k)
{
    if (k == '#') {
        m_latched = false;
        m_screen  = SCREEN_LIVE;
        m_lcd.clear();
        lcd_row(0, "  Relay Reset OK    ");
        lcd_row(2, "  Protection live   ");
        delay(1000);
        draw_live_force();
        return;
    }
    if (k == '*') {
        m_screen = SCREEN_MENU;
        m_lcd.clear();
        draw_menu();
        return;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  ── LCD helpers ──────────────────────────────────────────────
 * ══════════════════════════════════════════════════════════════ */
void RelayMenu::lcd_row(uint8_t row, const char *s)
{
    m_lcd.setCursor(0, row);
    char buf[LCD_COLS + 1];
    snprintf(buf, sizeof(buf), "%-20s", s);   /* left-align, pad to 20 */
    m_lcd.print(buf);
}

void RelayMenu::input_buf_clear()
{
    m_input_len    = 0;
    m_input_buf[0] = '\0';
}

float RelayMenu::input_buf_to_float()
{
    return (float)atof(m_input_buf);
}