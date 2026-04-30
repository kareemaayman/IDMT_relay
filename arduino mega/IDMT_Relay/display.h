#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
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
    MENU_INST_MULTIPLE
} MenuState;

class RelayMenu {
public:
    // Constructor taking references to the global settings
    RelayMenu(Standard& std, IEC_Curve& iec, IEEE_Curve& ieee, float& tms, float& pickup, float& inst_m);

    // Call this in setup() to display the initial menu
    void init();

    // Call this in loop() with the character received from Serial
    void processInput(char c);

private:
    void show_main_menu();
    void show_standard_menu();
    void show_curve_menu();

    // References to global configuration variables
    Standard&   m_std;
    IEC_Curve&  m_iec;
    IEEE_Curve& m_ieee;
    float&      m_tms;
    float&      m_pickup;
    float&      m_inst_m;

    // Internal menu state
    MenuState m_state;
    char      m_rx_buf[16];
    uint8_t   m_rx_index;
};

#endif
