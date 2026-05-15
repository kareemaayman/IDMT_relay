#include "LCD.h"

// Minimum allowed instantaneous multiple
#define INST_M_MIN 1.01f

LCDKeypadMenu::LCDKeypadMenu(LiquidCrystal_I2C& lcd, RelaySharedState& state)
    : m_lcd(lcd), m_state(state), m_menu_state(MENU_MAIN), m_rx_index(0)
{
    m_rx_buf[0] = '\0';
}

void LCDKeypadMenu::init() {
    m_lcd.backlight();
    show_main_menu();
}

void LCDKeypadMenu::update_display_line(int line, const char* text) {
    if (line<2){
    m_lcd.setCursor(0, line);
    m_lcd.print("                    ");  // Clear line
    m_lcd.setCursor(0, line);
    m_lcd.print(text);
    }
    else {
        m_lcd.setCursor(-4, line);
        m_lcd.print("                    ");  // Clear line
        m_lcd.setCursor(-4, line);
        m_lcd.print(text);
    }
}

void LCDKeypadMenu::clear_input_line() {
    m_lcd.setCursor(-4, 3);
    m_lcd.print("                    ");
    m_lcd.setCursor(-4, 3);
}

void LCDKeypadMenu::processInput(char c) {
    switch (m_menu_state) {
        /* ── Main menu ──────────────────────────────────── */
        case MENU_MAIN:
            switch (c) {
                case '1': m_menu_state = MENU_STANDARD; show_standard_menu(); break;
                case '2': m_menu_state = MENU_CURVE;    show_curve_menu();    break;
                case '3': m_menu_state = MENU_TMS;      show_tms_menu();      break;
                case '4': m_menu_state = MENU_PICKUP;   show_pickup_menu();   break;
                case '5': m_menu_state = MENU_INST_MULTIPLE; show_inst_multiple_menu(); break;
                case '#': m_menu_state = MENU_RESET; show_reset_menu(); break;
                case '9': m_menu_state = MENU_STATUS; show_status_menu(); break;
                default: break;
            }
            break;

        /* ── Standard selection ─────────────────────────── */
        case MENU_STANDARD:
            if      (c == '1') { *m_state.std = STD_IEC;  update_display_line(3, "Set to IEC"); }
            else if (c == '2') { *m_state.std = STD_IEEE; update_display_line(3, "Set to IEEE"); }
            else { break; }
            delay(1000);
            m_menu_state = MENU_MAIN;
            show_main_menu();
            break;

        /* ── Curve selection ────────────────────────────── */
        case MENU_CURVE:
            if (*m_state.std == STD_IEC) {
                if      (c == '1') *m_state.iec = IEC_SI;
                else if (c == '2') *m_state.iec = IEC_VI;
                else if (c == '3') *m_state.iec = IEC_EI;
                else if (c == '4') *m_state.iec = IEC_LTI;
                else { break; }
            } else {
                if      (c == '1') *m_state.ieee = IEEE_MOD_INV;
                else if (c == '2') *m_state.ieee = IEEE_VERY_INV;
                else if (c == '3') *m_state.ieee = IEEE_EXT_INV;
                else { break; }
            }
            delay(1000);
            m_menu_state = MENU_MAIN;
            show_main_menu();
            break;

        /* ── TMS entry ──────────────────────────────────── */
        case MENU_TMS:
            if (c == '#') {  // Accept input with #
                if (m_rx_index == 0) break;
                m_rx_buf[m_rx_index] = '\0';
                float val = atof(m_rx_buf);
                if (val > 0.0f && val <= 10.0f) {
                    *m_state.tms = val;
                    update_display_line(3, "TMS updated");
                } else {
                    update_display_line(3, "Invalid (0.01-10)");
                    delay(500); // Show error for 0.5s
                    m_rx_index = 0; // Clear buffer index to allow re-entry
                    clear_input_line();
                    break;
                }
                delay(1000);
                m_rx_index = 0;
                m_menu_state = MENU_MAIN;
                show_main_menu();
            } else if ((c >= '0' && c <= '9') || c == '*') {
                if (c == '*') c = '.';
                if (m_rx_index < (int)(sizeof(m_rx_buf) - 1)) {
                    m_rx_buf[m_rx_index++] = c;
                    m_rx_buf[m_rx_index] = '\0';
                    clear_input_line();
                    m_lcd.print(m_rx_buf);
                }
            }
            break;

        /* ── Pickup entry ───────────────────────────────── */
        case MENU_PICKUP:
            if (c == '#') {
                if (m_rx_index == 0) break;
                m_rx_buf[m_rx_index] = '\0';
                float val = atof(m_rx_buf);
                if (val > 0.0f) {  // Pickup must be > 0, never zero
                    *m_state.pickup = val;
                    update_display_line(3, "Pickup updated");
                } else {
                    update_display_line(3, "Must be > 0");
                    delay(500); // Show error for 0.5s
                    m_rx_index = 0; // Clear buffer index to allow re-entry
                    clear_input_line();
                    break;
                }
                delay(1000);
                m_rx_index = 0;
                m_menu_state = MENU_MAIN;
                show_main_menu();
            } else if ((c >= '0' && c <= '9') || c == '*') {
                if (c == '*') c = '.';
                if (m_rx_index < (int)(sizeof(m_rx_buf) - 1)) {
                    m_rx_buf[m_rx_index++] = c;
                    m_rx_buf[m_rx_index] = '\0';
                    clear_input_line();
                    m_lcd.print(m_rx_buf);
                }
            }
            break;

        /* ── Instantaneous multiple entry ───────────────── */
        case MENU_INST_MULTIPLE:
            if (c == '#') {
                if (m_rx_index == 0) break;
                m_rx_buf[m_rx_index] = '\0';
                float val = atof(m_rx_buf);
                if (val >= INST_M_MIN) {
                    *m_state.inst_m = val;
                    update_display_line(3, "Inst M updated");
                } else {
                    update_display_line(3, "Must be >= 1.01");
                    delay(500); // Show error for 0.5s
                    m_rx_index = 0; // Clear buffer index to allow re-entry
                    clear_input_line();
                    break;
                }
                delay(1000);
                m_rx_index = 0;
                m_menu_state = MENU_MAIN;
                show_main_menu();
            } else if ((c >= '0' && c <= '9') || c == '*') {
                if (c == '*') c = '.';
                if (m_rx_index < (int)(sizeof(m_rx_buf) - 1)) {
                    m_rx_buf[m_rx_index++] = c;
                    m_rx_buf[m_rx_index] = '\0';
                    clear_input_line();
                    m_lcd.print(m_rx_buf);
                }
            }
            break;

        /* ── Reset latch menu ───────────────────────────── */
        case MENU_RESET:
            if (c == '1') {  // 1 for YES
                *m_state.latched = false;
                update_display_line(3, "Latch reset");
            } else if (c == '2') {  // 2 for NO
                update_display_line(3, "Latch remains");
            } else {
                break;
            }
            delay(1000);
            m_menu_state = MENU_MAIN;
            show_main_menu();
            break;

        /* ── Status menu ────────────────────────────────── */
        case MENU_STATUS:
            if (c == '0') {  // Press 0 to return
                m_menu_state = MENU_MAIN;
                show_main_menu();
            }
            break;
    }
}

void LCDKeypadMenu::show_main_menu() {
    const char *std_str;
    const char *curve_str;

    if (*m_state.std == STD_IEC) {
        std_str = "IEC";
        const char *iec_names[] = {"SI","VI","EI","LTI"};
        curve_str = iec_names[*m_state.iec];
    } else {
        std_str = "IEEE";
        const char *ieee_names[] = {"MOD","VERY","EXT"};
        curve_str = ieee_names[*m_state.ieee];
    }

    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("MENU:");
    m_lcd.setCursor(0, 1);
    m_lcd.print("1=STD,2=CRV,3=TM");
    m_lcd.setCursor(-4, 2);
    m_lcd.print("4=PIK,5=INS,#=RS");
    m_lcd.setCursor(-4, 3);
    m_lcd.print("9=STS,7=PROT");
}

void LCDKeypadMenu::show_standard_menu() {
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("Select Standard");
    m_lcd.setCursor(0, 1);
    m_lcd.print("1 = IEC");
    m_lcd.setCursor(-4, 2);
    m_lcd.print("2 = IEEE");
    m_lcd.setCursor(-4, 3);
    m_lcd.print("Press key");
}

void LCDKeypadMenu::show_curve_menu() {
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("Select Curve");
    if (*m_state.std == STD_IEC) {
        m_lcd.setCursor(0, 1);
        m_lcd.print("1=SI 2=VI");
        m_lcd.setCursor(-4, 2);
        m_lcd.print("3=EI 4=LTI");
    } else {
        m_lcd.setCursor(0, 1);
        m_lcd.print("1=MOD 2=VERY");
        m_lcd.setCursor(-4, 2);
        m_lcd.print("3=EXT");
    }
    m_lcd.setCursor(-4, 3);
    m_lcd.print("Press key");
}

void LCDKeypadMenu::show_tms_menu() {
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("Enter TMS (0.01-10)");
    m_lcd.setCursor(0, 1);
    m_lcd.print("Current: ");
    m_lcd.print(*m_state.tms, 2);
    m_lcd.setCursor(-4, 2);
    m_lcd.print("Use 0-9 and * ");
    m_lcd.setCursor(-4, 3);
    m_lcd.print("Press # to OK");
}

void LCDKeypadMenu::show_pickup_menu() {
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("Enter Pickup (A)");
    m_lcd.setCursor(0, 1);
    m_lcd.print("Current: ");
    m_lcd.print(*m_state.pickup, 2);
    m_lcd.setCursor(-4, 2);
    m_lcd.print("Use 0-9 and * ");
    m_lcd.setCursor(-4, 3);
    m_lcd.print("Press # to OK");
}

void LCDKeypadMenu::show_inst_multiple_menu() {
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("Enter Inst M (>1.01)");
    m_lcd.setCursor(0, 1);
    m_lcd.print("Current: ");
    m_lcd.print(*m_state.inst_m, 2);
    m_lcd.setCursor(-4, 2);
    m_lcd.print("Use 0-9 and * ");
    m_lcd.setCursor(-4, 3);
    m_lcd.print("Press # to OK");
}

void LCDKeypadMenu::show_reset_menu() {
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("Reset Latch?");
    m_lcd.setCursor(0, 1);
    m_lcd.print("1 = YES");
    m_lcd.setCursor(-4, 2);
    m_lcd.print("2 = NO");
    m_lcd.setCursor(-4, 3);
    m_lcd.print("Press key");
}

void LCDKeypadMenu::show_status_menu() {
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print("Latch: ");
    m_lcd.print(*m_state.latched ? "ACTIVE" : "CLEAR");
    m_lcd.setCursor(0, 1);
    m_lcd.print("Std: ");
    m_lcd.print(*m_state.std == STD_IEC ? "IEC" : "IEEE");
    m_lcd.setCursor(-4, 2);
    m_lcd.print("TMS: ");
    m_lcd.print(*m_state.tms, 2);
    m_lcd.setCursor(-4, 3);
    m_lcd.print("0=Back");
}
