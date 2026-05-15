#ifndef LCD_H
#define LCD_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "relay_curves.h"
#include "relay_types.h"
#include "relay_state.h"

class LCDKeypadMenu {
public:
    // Constructor taking shared state and LCD reference
    LCDKeypadMenu(LiquidCrystal_I2C& lcd, RelaySharedState& state);

    // Call this in setup() to display the initial menu
    void init();

    // Call this in loop() with the character received from keypad
    void processInput(char c);
    
    /* Accessors for shared state */
    bool& get_latched() { return *m_state.latched; }
    
    // Public menu display functions (for protection mode)
    void show_reset_menu();
    void show_status_menu();
    void show_main_menu();

private:
    void show_standard_menu();
    void show_curve_menu();
    void show_tms_menu();
    void show_pickup_menu();
    void show_inst_multiple_menu();
    void update_display_line(int line, const char* text);
    void clear_input_line();

    // References
    LiquidCrystal_I2C& m_lcd;
    RelaySharedState&  m_state;

    // Internal menu state
    MenuState m_menu_state;
    char m_rx_buf[20];
    int m_rx_index;
};

#endif
