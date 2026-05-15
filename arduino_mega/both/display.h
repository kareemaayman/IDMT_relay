#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "relay_curves.h"
#include "relay_types.h"
#include "relay_state.h"

class RelayMenu {
public:
    // Constructor taking shared state
    RelayMenu(RelaySharedState& state);

    // Call this in setup() to display the initial menu
    void init();

    // Call this in loop() with the character received from Serial
    void processInput(char c);
    
    /* Accessors for shared state */
    bool& get_latched() { return *m_state.latched; }

private:
    void show_main_menu();
    void show_standard_menu();
    void show_curve_menu();
    void show_tms_menu();
    void show_pickup_menu();
    void show_inst_multiple_menu();
    void show_reset_menu();

    // Shared state reference
    RelaySharedState& m_state;

    // Internal menu state
    MenuState m_menu_state;
    char      m_rx_buf[16];
    uint8_t   m_rx_index;
};

#endif
