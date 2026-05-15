#ifndef RELAY_TYPES_H
#define RELAY_TYPES_H

// Standards
typedef enum { STD_IEC, STD_IEEE } Standard;

// Menu states (merged from both display.h and LCD.h)
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

#endif
