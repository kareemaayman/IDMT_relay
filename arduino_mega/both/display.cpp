#include "display.h"

// Minimum allowed instantaneous multiple
#define INST_M_MIN 1.01f

RelayMenu::RelayMenu(Standard& std, IEC_Curve& iec, IEEE_Curve& ieee, float& tms, float& pickup, float& inst_m)
    : m_std(std), m_iec(iec), m_ieee(ieee), m_tms(tms), m_pickup(pickup), m_inst_m(inst_m),
      m_state(MENU_MAIN), m_rx_index(0), m_latched(false)

{
    // Initialize rx buffer
    m_rx_buf[0] = '\0';
}

void RelayMenu::init() {
    show_main_menu();
}

void RelayMenu::processInput(char c) {
    switch (m_state) {
        /* ── Main menu ──────────────────────────────────── */
        case MENU_MAIN:
            switch (c) {
                case '1': m_state = MENU_STANDARD; show_standard_menu(); break;
                case '2': m_state = MENU_CURVE;    show_curve_menu();    break;
                case '3': m_state = MENU_TMS;      show_tms_menu();       break;
                case '4': m_state = MENU_PICKUP;   show_pickup_menu(); break;
                case '5': m_state = MENU_INST_MULTIPLE; show_inst_multiple_menu(); break;
                case '6': m_state = MENU_RESET; show_reset_menu(); break;
                default: show_main_menu(); break;
            }
            break;

        /* ── Standard selection ─────────────────────────── */
        case MENU_STANDARD:
            if      (c == '1') { m_std = STD_IEC;  Serial.print(F("\r\nSet to IEC."));  }
            else if (c == '2') { m_std = STD_IEEE; Serial.print(F("\r\nSet to IEEE.")); }
            else { Serial.print(F("\r\nInvalid. Enter 1 or 2.")); show_standard_menu(); break; }
            m_state = MENU_MAIN;
            show_main_menu();
            break;

        /* ── Curve selection ────────────────────────────── */
        case MENU_CURVE:
            if (m_std == STD_IEC) {
                if      (c == '1') m_iec = IEC_SI;
                else if (c == '2') m_iec = IEC_VI;
                else if (c == '3') m_iec = IEC_EI;
                else if (c == '4') m_iec = IEC_LTI;
                else { Serial.print(F("\r\nInvalid. Enter 1-4.")); show_curve_menu(); break; }
            } else {
                if      (c == '1') m_ieee = IEEE_MOD_INV;
                else if (c == '2') m_ieee = IEEE_VERY_INV;
                else if (c == '3') m_ieee = IEEE_EXT_INV;
                else { Serial.print(F("\r\nInvalid. Enter 1-3.")); show_curve_menu(); break; }
            }
            m_state = MENU_MAIN;
            show_main_menu();
            break;

        /* ── TMS entry ──────────────────────────────────── */
        case MENU_TMS:
            if (c == '\r' || c == '\n') {
                if (m_rx_index == 0) break;  /* ignore empty input */
                m_rx_buf[m_rx_index] = '\0';
                float val = atof(m_rx_buf);
                if (val > 0.0f && val <= 10.0f) {
                    m_tms = val;
                    Serial.print(F("\r\nTMS updated."));
                } else {
                    Serial.print(F("\r\nInvalid. Range: 0.01 - 10.0"));
                }
                m_rx_index = 0;
                m_state = MENU_MAIN;
                show_main_menu();
            } else {
                if (m_rx_index < (int)(sizeof(m_rx_buf) - 1)) m_rx_buf[m_rx_index++] = c;
            }
            break;

        /* ── Pickup entry ───────────────────────────────── */
        case MENU_PICKUP:
            if (c == '\r' || c == '\n') {
                if (m_rx_index == 0) break;  /* ignore empty input */
                m_rx_buf[m_rx_index] = '\0';
                float val = atof(m_rx_buf);
                if (val > 0.0f) {
                    m_pickup = val;
                    Serial.print(F("\r\nPickup updated."));
                } else {
                    Serial.print(F("\r\nInvalid value."));
                }
                m_rx_index = 0;
                m_state = MENU_MAIN;
                show_main_menu();
            } else {
                if (m_rx_index < (int)(sizeof(m_rx_buf) - 1)) m_rx_buf[m_rx_index++] = c;
            }
            break;

        /* ── Instantaneous multiple entry ───────────────── */
        case MENU_INST_MULTIPLE:
            if (c == '\r' || c == '\n') {
                if (m_rx_index == 0) break;  /* ignore empty input */
                m_rx_buf[m_rx_index] = '\0';
                float val = atof(m_rx_buf);
                if (val >= INST_M_MIN) {
                    m_inst_m = val;
                    Serial.print(F("\r\nInstant trip M updated."));
                } else {
                    Serial.print(F("\r\nInvalid. Must be >= 1.01"));
                }
                m_rx_index = 0;
                m_state = MENU_MAIN;
                show_main_menu();
            } else {
                if (m_rx_index < (int)(sizeof(m_rx_buf) - 1)) m_rx_buf[m_rx_index++] = c;
            }
            break;
        /* ── Reset latch menu ───────────────────────────── */
        case MENU_RESET:
            if (c == 'Y' || c == 'y') {
                m_latched = false;  /* reset the latch */
                Serial.print(F("\r\nLatch reset. Relay can trip again."));
            } else if (c == 'N' || c == 'n') {
                Serial.print(F("\r\nLatch remains set. Relay will not trip."));
            } else {
                Serial.print(F("\r\nInvalid. Enter Y or N."));
                break; 
            }
            m_state = MENU_MAIN;
            show_main_menu();
            break; 
    }
}

void RelayMenu::show_main_menu() {
    const char *std_str;
    const char *curve_str;

    if (m_std == STD_IEC) {
        std_str = "IEC";
        const char *iec_names[] = {"SI","VI","EI","LTI"};
        curve_str = iec_names[m_iec];
    } else {
        std_str = "IEEE";
        const char *ieee_names[] = {"MOD_INV","VERY_INV","EXT_INV"};
        curve_str = ieee_names[m_ieee];
    }

    Serial.print(F("\r\n======= IDMT RELAY =======\r\n"));
    Serial.print(F(" Standard : "));
    Serial.print(std_str);
    Serial.print(F("\r\n Curve    : "));
    Serial.print(curve_str);
    Serial.print(F("\r\n TMS/TDS  : "));
    Serial.print(m_tms, 2);
    Serial.print(F("\r\n Pickup   : "));
    Serial.print(m_pickup, 2);
    Serial.print(F(" A\r\n Inst M   : "));
    Serial.print(m_inst_m, 2);
    Serial.print(F("\r\n--------------------------\r\n"
                   " 1) Change standard\r\n"
                   " 2) Change curve\r\n"
                   " 3) Change TMS/TDS\r\n"
                   " 4) Change pickup current\r\n"
                   " 5) Change instant trip M\r\n"
                   " 6) Reset fault latch\r\n" 
                   "==========================\r\n"
                   "Enter choice: "));
}

void RelayMenu::show_standard_menu() {
    Serial.print(F("\r\nSelect standard:\r\n"
                 " 1) IEC 60255\r\n"
                 " 2) IEEE C37.112\r\n"
                 "Enter choice: "));
}

void RelayMenu::show_curve_menu() {
    if (m_std == STD_IEC) {
        Serial.print(F("\r\nSelect IEC curve:\r\n"
                     " 1) Standard Inverse  (SI)\r\n"
                     " 2) Very Inverse      (VI)\r\n"
                     " 3) Extremely Inverse (EI)\r\n"
                     " 4) Long-Time Inverse (LTI)\r\n"
                     "Enter choice: "));
    } else {
        Serial.print(F("\r\nSelect IEEE curve:\r\n"
                     " 1) Moderately Inverse\r\n"
                     " 2) Very Inverse\r\n"
                     " 3) Extremely Inverse\r\n"
                     "Enter choice: "));
    }
}
void RelayMenu::show_tms_menu() {
    Serial.print(F("\r\nEnter TMS (e.g. 0.50): "));
}
void RelayMenu::show_pickup_menu() {
    Serial.print(F("\r\nEnter pickup amps (e.g. 5.00): "));
}
void RelayMenu::show_inst_multiple_menu() {
    Serial.print(F("\r\nEnter instant trip M (e.g. 10.00): "));
}
void RelayMenu::show_reset_menu() {
    Serial.print(F("\r\nRelay is latched open.\r\n"
                   "Reset latch? (Y/N): "));
}
