#ifndef LCD_H
#define LCD_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "relay_curves.h"

// Standards
typedef enum { STD_IEC, STD_IEEE } Standard;

// Menu states
typedef enum {
    MENU_MAIN,
    MENU_STANDARD,
    MENU_CURVE,
    MENU_TMS,
    MENU_PICKUP,
    MENU_INST_MULTIPLE,
    MENU_RESET,
    MENU_STATUS
} MenuState;

class LCDKeypadMenu {
public:
    // Constructor taking references to the global settings
    LCDKeypadMenu(LiquidCrystal_I2C& lcd, Standard& std, IEC_Curve& iec, IEEE_Curve& ieee, 
                  float& tms, float& pickup, float& inst_m);

    // Call this in setup() to display the initial menu
    void init();

    // Call this in loop() with the character received from keypad
    void processInput(char c);
    
    bool m_latched; /* set to true when an instantaneous trip occurs, to bypass pending logic */
    
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
    Standard&   m_std;
    IEC_Curve&  m_iec;
    IEEE_Curve& m_ieee;
    float&      m_tms;
    float&      m_pickup;
    float&      m_inst_m;

    // Internal menu state
    MenuState m_state;
    char m_rx_buf[20];
    int m_rx_index;
};

#endif
