/*
 * IDMT Protection Relay — Arduino Mega 2560 port
 * ================================================
 * Ported from STM32F103 (HAL / CubeMX) to Arduino Mega (AVR, 10-bit ADC).
 *
 * Hardware differences vs STM32:
 *   - ADC  : 10-bit (0-1023), DC midpoint = 512  (was 12-bit / 4095 / 2048)
 *   - COUNTS_TO_AMPS recalculated for 10-bit range (same Vref & CT ratio)
 *   - Sampling : analogRead() loop          (was DMA + TIM3 trigger)
 *   - Timing   : millis()                   (was HAL_GetTick())
 *   - Serial   : Serial (USB / UART0)       (was USART1 + HAL interrupt)
 *   - Trip pin : pin 7, active LOW          (was RELAY_TRIP_Pin via HAL_GPIO)
 *
 * Wiring:
 *   CT secondary → signal-conditioning circuit → A0 (analogue input)
 *   Trip output  → relay driver / opto-coupler  → pin 7
 *   Serial monitor at 115200 baud for menu + status.
 */

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "relay_curves.h"
#include "relay_state.h"
#include "display.h"
#include "LCD.h"

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
#define TRIP_PIN         7          /* digital output → relay driver     */
#define GREEN_LED_PIN    8
#define YELLOW_LED_PIN   9
#define RED_LED_PIN      10

/*
 * Calibration parameters
 */
const float Vref = 5.0;        // Arduino reference voltage
const float ADC_res = 1023.0;

// 👉 Adjust this after calibration
float sensitivity = 1.2;

#define ADC_SAMPLES      500         /* samples per RMS window             */

/* ═══════════════════════════════════════════════════════════════
 *  Default relay settings
 * ═══════════════════════════════════════════════════════════════ */
#define I_PICKUP_DEFAULT  0.5f      /* amps                               */
#define TMS_DEFAULT       0.5f
#define INST_M_MIN        1.01f     /* minimum allowed inst multiple      */

/* ═══════════════════════════════════════════════════════════════
 *  State enumerations
 * ═══════════════════════════════════════════════════════════════ */
typedef enum { RELAY_NORMAL, RELAY_FAULT_PENDING, RELAY_TRIPPED } RelayState;

/* ═══════════════════════════════════════════════════════════════
 *  Global variables
 * ═══════════════════════════════════════════════════════════════ */

/* ADC buffer */
static uint16_t adc_buf[ADC_SAMPLES];

/* User-configurable relay parameters */
Standard   user_standard     = STD_IEC;
IEC_Curve  user_iec_curve    = IEC_SI;
IEEE_Curve user_ieee_curve   = IEEE_MOD_INV;
float      user_tms          = TMS_DEFAULT;
float      user_pickup       = I_PICKUP_DEFAULT;
float      user_inst_multiple = INST_MULTIPLE_DEFAULT;
static float trip_fraction = 0.0f; 

/* Relay protection state machine */
RelayState relay_state    = RELAY_NORMAL;
uint32_t   fault_start_ms = 0;
float      trip_time_ms   = 0.0f;
bool       protection_enabled = false;   /* protection starts only after user config */
bool       shared_latch = false;         /* shared latch flag synchronized between menus */
uint32_t   blink_counter  = 0;
bool       red_led_on     = false;
uint32_t   last_sample_ms = 0;           /* timestamp of last sampling window */
uint32_t   trip_event_ms  = 0;           /* timestamp of actual trip event */

// Status display
float      last_current   = 0.0f;
float      last_M         = 0.0f;

/* Create shared state for menu synchronization */
RelaySharedState shared_state(protection_enabled, shared_latch, user_standard, user_iec_curve, 
                              user_ieee_curve, user_tms, user_pickup, user_inst_multiple);

/* Instantiate the Menu Library with shared state */
RelayMenu relayMenu(shared_state);
/* I²C LCD */
LCDKeypadMenu lcdMenu(lcd, shared_state);

/* ═══════════════════════════════════════════════════════════════
 *  Forward declarations
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void);
void update_leds(RelayState state);
//static void print_status_1f(const __FlashStringHelper *label, uint32_t t, float M);
static void print_fault_start(uint32_t t, float M, float t_trip);
//static void print_fault_pending(uint32_t t, float M, float remaining, float t_trip);
// for lcdMenu
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

    // Initialize Serial
    Serial.begin(115200);
    pinMode(TRIP_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(TRIP_PIN, HIGH);
    update_leds(relay_state);
    
    Serial.print(F("\r\n"));
    Serial.print(F("═══════════════════════════════════════════\r\n"));
    Serial.print(F("  IDMT PROTECTION RELAY - CONFIGURATION\r\n"));
    Serial.print(F("═══════════════════════════════════════════\r\n"));
    Serial.print(F("\r\nConfigure settings below, then press 'S' to START PROTECTION:\r\n\r\n"));
    
    relayMenu.init();
    lcdMenu.init();
}

/* ═══════════════════════════════════════════════════════════════
 *  loop()
 * ═══════════════════════════════════════════════════════════════ */
void loop()
{
    /* ── UART input (non-blocking) ───────────────────── */
    while (Serial.available()) {
        char c = (char)Serial.read();
        Serial.print(c);          /* echo back so user sees what they typed */
        
        /* Check for START command */
        if (!protection_enabled && (c == 'S' || c == 's')) {
            protection_enabled = true;
            Serial.print(F("\r\n\r\n"));
            Serial.print(F("═══════════════════════════════════════════\r\n"));
            Serial.print(F("  PROTECTION SYSTEM ACTIVE\r\n"));
            Serial.print(F("═══════════════════════════════════════════\r\n"));
            Serial.print(F("\r\n"));
        } else {
            relayMenu.processInput(c);
        }
    }
    /* ── Keypad input (non-blocking) ───────────────────── */
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
                            shared_latch = false;
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

    /* ── ADC sampling + protection logic ─────────────── */
    if (protection_enabled) {
        run_protection();
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  run_protection() - Sampling + trip state machine
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void)
{
    /* If latched, hold relay open and wait for manual reset via menu or button */
    if (shared_latch) {
        digitalWrite(TRIP_PIN, LOW);     // ensure it stays open
        update_leds(relay_state);
        return;
    }
    if (shared_latch) {
        digitalWrite(TRIP_PIN, LOW);
        update_leds(relay_state);
        display_relay_state();
        return;
    }
    
    uint32_t sample_start_ms = millis();
    
    /* Collect one window of ADC samples */
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_buf[i] = (uint16_t)analogRead(ADC_PIN);
        delayMicroseconds(200);
    }
    
    uint32_t sample_end_ms = millis();
    float window_s = (float)(sample_end_ms - sample_start_ms) / 1000.0f;  /* actual elapsed time in seconds */

    float offset = 0.0f;
    float Vrms = calc_rms(adc_buf, ADC_SAMPLES, Vref, ADC_res, &offset);

    float I_fault = Vrms * sensitivity;
    float M       = I_fault / user_pickup;
    last_current  = I_fault;  // Store for LCD display
    last_M        = M;        // Store for LCD display
    Serial.print(F("DEBUG: I="));
    Serial.print(I_fault);
    Serial.print(F(" M="));
    Serial.print(M);
    Serial.print(F("\r\n"));

    if (M > 1.0f)
    {
        /* Calculate trip time (returns RELAY_INST_ZONE if M >= inst_multiple) */
        float t_trip;
        if (user_standard == STD_IEC)
            t_trip = trip_time_iec (M, user_tms, user_inst_multiple, user_iec_curve);
        else
            t_trip = trip_time_ieee(M, user_tms, user_inst_multiple, user_ieee_curve);

        /* ── Instantaneous zone: bypass FAULT_PENDING, trip immediately ── */
        if (M >= user_inst_multiple)
        {
            relay_state = RELAY_TRIPPED;
            shared_latch = true;
            trip_event_ms = millis();     /* CAPTURE TRIP TIME FIRST */
            digitalWrite(TRIP_PIN, LOW);  /* TRIGGER IMMEDIATELY */
            lcd.clear();
            Serial.print(F("INST_TRIP t="));
            Serial.print(trip_event_ms);
            Serial.print(F(" M="));
            Serial.print(M, 2);
            Serial.print(F("\r\n"));
            update_leds(relay_state);
            display_relay_state();
            return;
        }

        switch (relay_state)
        {
            case RELAY_NORMAL:
                /* New fault detected */
                relay_state    = RELAY_FAULT_PENDING;
                fault_start_ms = millis();
                trip_fraction  = 0.0f; 
                trip_time_ms   = t_trip * 1000.0f;
                print_fault_start(millis(), M, t_trip);
                update_leds(relay_state);
                display_protection_status();
                break;

            case RELAY_FAULT_PENDING:
            {
                trip_fraction += window_s / t_trip;     
                if (trip_fraction >= 1.0f){
                    relay_state = RELAY_TRIPPED;
                    shared_latch = true;
                    trip_event_ms = millis();     /* CAPTURE TRIP TIME FIRST */
                    digitalWrite(TRIP_PIN, LOW);  /* TRIGGER IMMEDIATELY */
                    lcd.clear();
                    Serial.print(F("TRIP_EXECUTED t="));
                    Serial.print(trip_event_ms);
                    Serial.print(F(" elapsed="));
                    Serial.print(trip_event_ms - fault_start_ms);
                    Serial.print(F("ms theory="));
                    Serial.print(t_trip, 3);
                    Serial.print(F("s\r\n"));
                    update_leds(relay_state);
                    display_relay_state();
                }else {
                    float remaining = (1.0f - trip_fraction) * t_trip;
                    Serial.print(F("FAULT t="));
                    Serial.print(millis());
                    Serial.print(F(" M="));
                    Serial.print(M, 2);
                    Serial.print(F(" Tremain="));
                    Serial.print(remaining, 3);
                    Serial.print(F("s\r\n"));
                    display_protection_status();
                }
                break;
            }

            case RELAY_TRIPPED:
                Serial.print(F("TRIPPED t="));
                Serial.print(millis());
                Serial.print(F(" M="));
                Serial.print(M, 2);
                Serial.print(F("\r\n"));
                update_leds(relay_state);
                display_relay_state();
                break;
        }
    }
    else{
            // Fault cleared — but only reclose if NOT latched
            if (relay_state != RELAY_NORMAL && !shared_latch) 
            {
                relay_state = RELAY_NORMAL;
                digitalWrite(TRIP_PIN, HIGH);
                Serial.print(F("FAULT_CLEARED\r\n"));
                update_leds(relay_state);
            }
            else if (!shared_latch)
            {
                Serial.print(F("OK t="));
                Serial.print(millis());
                Serial.print(F(" M="));
                Serial.print(M, 2);
                Serial.print(F("\r\n"));
                display_protection_status();
                update_leds(relay_state);
            }
            else {
                Serial.print(F("FAULT_CLEARED_BUT_LATCHED\r\n"));
                update_leds(relay_state);
                display_relay_state();
            }
            // if latched: do nothing, relay stays open
    }
}

void update_leds(RelayState state)
{
    switch (state)
    {
        case RELAY_NORMAL:
            red_led_on = false;
            blink_counter = millis();
            digitalWrite(GREEN_LED_PIN, HIGH);
            digitalWrite(YELLOW_LED_PIN, LOW);
            digitalWrite(RED_LED_PIN, LOW);
            break;

        case RELAY_FAULT_PENDING:
            red_led_on = false;
            blink_counter = millis();
            digitalWrite(GREEN_LED_PIN, LOW);
            digitalWrite(YELLOW_LED_PIN, HIGH);
            digitalWrite(RED_LED_PIN, LOW);
            break;

        case RELAY_TRIPPED:
            digitalWrite(GREEN_LED_PIN, LOW);
            digitalWrite(YELLOW_LED_PIN, LOW);
            if ((millis() - blink_counter) >= 500) {
                blink_counter = millis();
                red_led_on = !red_led_on;
            }
            digitalWrite(RED_LED_PIN, red_led_on ? HIGH : LOW);
            break;
    }
}

static void print_fault_start(uint32_t t, float M, float t_trip)
{
    Serial.print(F("FAULT_START t="));
    Serial.print(t);
    Serial.print(F(" M="));
    Serial.print(M, 2);
    Serial.print(F(" Ttrip_theory="));
    Serial.print(t_trip, 3);
    Serial.print(F("s\r\n"));
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
    if (shared_latch) {
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
