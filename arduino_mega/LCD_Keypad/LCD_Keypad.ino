/*
 * IDMT Protection Relay — Arduino Mega 2560 with I²C LCD + 4×3 Matrix Keypad
 * ════════════════════════════════════════════════════════════════════════════
 * Standalone version: no PC needed, all configuration via 20×4 I²C LCD and keypad
 *
 * Hardware:
 *   - I²C LCD 20×4 (typical address 0x27 or 0x3F)
 *   - 4×3 Matrix Keypad (pins defined below)
 *   - CT secondary → signal-conditioning → A0
 *   - Trip output → relay driver / opto-coupler → pin 7
 *   - Status LEDs: GREEN (pin 8), YELLOW (pin 9), RED (pin 10)
 *
 * Keypad Matrix:
 *   {1, 2, 3}
 *   {4, 5, 6}
 *   {7, 8, 9}
 *   {*, 0, #}
 *
 * Key Mapping:
 *   1 = Standard (STD)   | 2 = Curve (CRV)
 *   3 = TMS              | 4 = Pickup (PIK)
 *   5 = Inst M (INS)     | # = Reset (RST)
 *   9 = Status (STS)     | * = Decimal point
 */

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "relay_curves.h"
#include "display.h"

/* ═══════════════════════════════════════════════════════════════
 *  Hardware Configuration
 * ═══════════════════════════════════════════════════════════════ */

// I²C LCD: 20 columns, 4 rows, address 0x27 (adjust if needed)
// line 2,3 are shifted 4 columns so they start at -4 instead of 0 
LiquidCrystal_I2C lcd(0x27, 20, 4);

// 4×3 Matrix Keypad
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {50, 51, 52, 53};       // Row pins (R1, R2, R3, R4)
byte colPins[COLS] = {48, 49, 47};           // Column pins (C1, C2, C3)

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Hardware pins
#define ADC_PIN          A0
#define TRIP_PIN         7          // digital output → relay driver
#define GREEN_LED_PIN    8          // Indicates normal operation (ON)
#define YELLOW_LED_PIN   9          // Indicates fault pending
#define RED_LED_PIN      10         // Indicates tripped state (Blinking)

/* ═══════════════════════════════════════════════════════════════
 *  Calibration & ADC parameters
 * ═══════════════════════════════════════════════════════════════ */
const float Vref = 5.0;        // Arduino reference voltage
const float ADC_res = 1023.0;
float sensitivity = 1.2;      // Adjust after calibration
#define ADC_SAMPLES      500   // Samples per RMS window

/* ═══════════════════════════════════════════════════════════════
 *  Default relay settings
 * ═══════════════════════════════════════════════════════════════ */
#define I_PICKUP_DEFAULT  0.5f
#define TMS_DEFAULT       0.5f
#define INST_M_MIN        1.01f

/* ═══════════════════════════════════════════════════════════════
 *  State enumerations
 * ═══════════════════════════════════════════════════════════════ */
typedef enum { RELAY_NORMAL, RELAY_FAULT_PENDING, RELAY_TRIPPED } RelayState;

/* ═══════════════════════════════════════════════════════════════
 *  Global variables
 * ═══════════════════════════════════════════════════════════════ */

// ADC buffer
static uint16_t adc_buf[ADC_SAMPLES];

// User-configurable relay parameters
Standard   user_standard     = STD_IEC;
IEC_Curve  user_iec_curve    = IEC_SI;
IEEE_Curve user_ieee_curve   = IEEE_MOD_INV;
float      user_tms          = TMS_DEFAULT;
float      user_pickup       = I_PICKUP_DEFAULT;
float      user_inst_multiple = INST_MULTIPLE_DEFAULT;
static float trip_fraction = 0.0f;

// Relay protection state machine
RelayState relay_state    = RELAY_NORMAL;
uint32_t   fault_start_ms = 0;
float      trip_time_ms   = 0.0f;
bool       protection_enabled = false;
uint32_t   blink_counter  = 0;
bool       red_led_on     = false;
uint32_t   last_sample_ms = 0;
uint32_t   trip_event_ms  = 0;

// Status display
float      last_current   = 0.0f;
float      last_M         = 0.0f;

// Instantiate the Menu Library
LCDKeypadMenu lcdMenu(lcd, user_standard, user_iec_curve, user_ieee_curve, 
                       user_tms, user_pickup, user_inst_multiple);

/* ═══════════════════════════════════════════════════════════════
 *  Forward declarations
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void);
void update_leds(RelayState state);
char getValidKeypress(void);
void display_protection_status(void);
void display_relay_state(void);

/* ═══════════════════════════════════════════════════════════════
 *  setup()
 * ═══════════════════════════════════════════════════════════════ */
void setup()
{
    // Initialize I²C LCD
    Wire.begin();
    // lcd.begin(20, 4);
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IDMT Relay Init...");
    
    // Configure pins
    pinMode(TRIP_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(TRIP_PIN, HIGH);
    update_leds(relay_state);
    
    delay(1000);
    
    // Initialize menu
    lcdMenu.init();
}

/* ═══════════════════════════════════════════════════════════════
 *  loop()
 * ═══════════════════════════════════════════════════════════════ */
void loop()
{
    // Update blinking LED independently
    if (relay_state == RELAY_TRIPPED) {
        if ((millis() - blink_counter) >= 500) {
            blink_counter = millis();
            red_led_on = !red_led_on;
            digitalWrite(RED_LED_PIN, red_led_on ? HIGH : LOW);
        }
    }
    
    // Check for keypad input
    char key = keypad.getKey();
    
    if (key) {
        // Check for START command (press '7' to start)
        if (!protection_enabled && key == '7') {
            protection_enabled = true;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("PROTECTION ACTIVE");
            delay(1500);
        } else if (!protection_enabled) {
            // In configuration mode, pass keys directly to menu
            lcdMenu.processInput(key);
        } else {
            // In protection mode, handle reset and status
            if (key == '#') {
                // Show reset menu
                lcdMenu.show_reset_menu();
                // Wait for confirmation (1=yes, 2=no)
                bool waiting = true;
                uint32_t timeout = millis() + 5000;  // 5 second timeout
                while (waiting && millis() < timeout) {
                    char confirmKey = keypad.getKey();
                    if (confirmKey) {
                        if (confirmKey == '1') {
                            lcdMenu.m_latched = false;
                            lcd.setCursor(-4, 3);
                            lcd.print("Latch reset      ");
                            delay(1000);
                        } else if (confirmKey == '2') {
                            lcd.setCursor(-4, 3);
                            lcd.print("Latch remains   ");
                            delay(1000);
                        }
                        waiting = false;
                    }
                    delay(10);
                }
            } else if (key == '9') {
                // Show status info for 3 seconds
                lcdMenu.show_status_menu();
                bool waiting = true;
                uint32_t timeout = millis() + 3000;
                while (waiting && millis() < timeout) {
                    char statusKey = keypad.getKey();
                    if (statusKey == '0') {
                        waiting = false;
                    }
                    delay(10);
                }
            } else if (key == '7') {
                // Enter menu editing mode — show main menu and allow editing
                lcdMenu.show_main_menu();
                bool editing = true;
                uint32_t menu_timeout = millis() + 30000;  // 30 second timeout
                while (editing && millis() < menu_timeout) {
                    char menuKey = keypad.getKey();
                    if (menuKey) {
                        if (menuKey == '7') {
                            // Press 7 again to exit menu and return to protection status
                            editing = false;
                        } else {
                            // Process menu input (1-5 for settings, # for reset, 0 for status)
                            lcdMenu.processInput(menuKey);
                        }
                    }
                    delay(10);
                }
                // Return to protection status display
                display_protection_status();
            }
        }
    }

    // Run protection logic
    if (protection_enabled) {
        run_protection();
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  run_protection() - Sampling + trip state machine
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void)
{
    // If latched, hold relay open and show latch status
    if (lcdMenu.m_latched) {
        digitalWrite(TRIP_PIN, LOW);
        display_relay_state();
        return;
    }
    
    uint32_t sample_start_ms = millis();
    
    // Collect one window of ADC samples
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_buf[i] = (uint16_t)analogRead(ADC_PIN);
        delayMicroseconds(200);
    }
    
    uint32_t sample_end_ms = millis();
    float window_s = (float)(sample_end_ms - sample_start_ms) / 1000.0f;

    float offset = 0.0f;
    float Vrms = calc_rms(adc_buf, ADC_SAMPLES, Vref, ADC_res, &offset);

    float I_fault = Vrms * sensitivity;
    float M       = I_fault / user_pickup;
    
    last_current = I_fault;
    last_M = M;

    if (M > 1.0f)
    {
        // Calculate trip time
        float t_trip;
        if (user_standard == STD_IEC)
            t_trip = trip_time_iec (M, user_tms, user_inst_multiple, user_iec_curve);
        else
            t_trip = trip_time_ieee(M, user_tms, user_inst_multiple, user_ieee_curve);

        // Instantaneous zone: bypass FAULT_PENDING, trip immediately
        if (M >= user_inst_multiple)
        {
            relay_state = RELAY_TRIPPED;
            lcdMenu.m_latched = true;
            trip_event_ms = millis();
            digitalWrite(TRIP_PIN, LOW);
            update_leds(relay_state);
            display_relay_state();
            return;
        }

        switch (relay_state)
        {
            case RELAY_NORMAL:
                // New fault detected
                relay_state    = RELAY_FAULT_PENDING;
                fault_start_ms = millis();
                trip_fraction  = 0.0f;
                trip_time_ms   = t_trip * 1000.0f;
                update_leds(relay_state);
                display_protection_status();
                break;

            case RELAY_FAULT_PENDING:
            {
                trip_fraction += window_s / t_trip;
                if (trip_fraction >= 1.0f) {
                    relay_state = RELAY_TRIPPED;
                    lcdMenu.m_latched = true;
                    trip_event_ms = millis();
                    digitalWrite(TRIP_PIN, LOW);
                    update_leds(relay_state);
                    display_relay_state();
                } else {
                    //float remaining = (1.0f - trip_fraction) * t_trip;
                    display_protection_status();
                }
                break;
            }

            case RELAY_TRIPPED:
                display_relay_state();
                update_leds(relay_state);
                break;
        }
    }
    else {
        // Fault cleared — but only reclose if NOT latched
        if (relay_state != RELAY_NORMAL && !lcdMenu.m_latched)
        {
            relay_state = RELAY_NORMAL;
            digitalWrite(TRIP_PIN, HIGH);
            update_leds(relay_state);
        }
        
        if (relay_state == RELAY_NORMAL && !lcdMenu.m_latched) {
            display_protection_status();
        } else if (lcdMenu.m_latched) {
            display_relay_state();
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Update LEDs based on relay state
 * ═══════════════════════════════════════════════════════════════ */
void update_leds(RelayState state)
{
    switch (state)
    {
        case RELAY_NORMAL:
            digitalWrite(GREEN_LED_PIN, HIGH);
            digitalWrite(YELLOW_LED_PIN, LOW);
            digitalWrite(RED_LED_PIN, LOW);
            break;

        case RELAY_FAULT_PENDING:
            digitalWrite(GREEN_LED_PIN, LOW);
            digitalWrite(YELLOW_LED_PIN, HIGH);
            digitalWrite(RED_LED_PIN, LOW);
            break;

        case RELAY_TRIPPED:
            digitalWrite(GREEN_LED_PIN, LOW);
            digitalWrite(YELLOW_LED_PIN, LOW);
            // Red LED blinking is now handled in main loop
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Display protection status on LCD
 * ═══════════════════════════════════════════════════════════════ */
void display_protection_status(void)
{
    static uint32_t last_update = 0;
    if (millis() - last_update < 500) return;  // Update every 500ms
    last_update = millis();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PROTECTION ACTIVE");
    
    lcd.setCursor(0, 1);
    lcd.print("I=");
    lcd.print(last_current, 2);
    lcd.print("A M=");
    lcd.print(last_M, 2);
    
    lcd.setCursor(-4, 2);
    switch (relay_state) {
        case RELAY_NORMAL:
            lcd.print("Status: NORMAL");
            break;
        case RELAY_FAULT_PENDING:
            lcd.print("Status: PENDING");
            break;
        case RELAY_TRIPPED:
            lcd.print("Status: TRIPPED");
            break;
    }
    
    lcd.setCursor(-4, 3);
    lcd.print("#=RST,9=STS,7=MNU");
}

/* ═══════════════════════════════════════════════════════════════
 *  Display relay state (for tripped/latched status)
 * ═══════════════════════════════════════════════════════════════ */
void display_relay_state(void)
{
    static uint32_t last_update = 0;
    if (millis() - last_update < 500) return;  // Update every 500ms
    last_update = millis();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RELAY STATE");
    
    lcd.setCursor(0, 1);
    if (lcdMenu.m_latched) {
        lcd.print("Status: LATCHED");
    } else if (relay_state == RELAY_TRIPPED) {
        lcd.print("Status: TRIPPED");
    }
    
    lcd.setCursor(-4, 2);
    lcd.print("I=");
    lcd.print(last_current, 2);
    lcd.print("A M=");
    lcd.print(last_M, 2);
    
    lcd.setCursor(-4, 3);
    lcd.print("#=RST,9=STS,7=MNU");
}
