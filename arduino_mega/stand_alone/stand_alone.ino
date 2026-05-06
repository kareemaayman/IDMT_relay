/*
 * IDMT Protection Relay — Arduino Mega 2560
 * ==========================================
 * Hardware HMI: 20×4 I²C LCD + 4×3 matrix keypad (standalone, no PC needed)
 *
 * Key mapping:
 *   2 / 8  →  scroll menu up / down
 *   #      →  confirm / select
 *   *      →  decimal point (numeric entry) | back | open menu (live screen)
 *   1–5    →  quick-pick in option lists
 *
 * Wiring summary:
 *   LCD SDA  → Mega pin 20 (SDA)
 *   LCD SCL  → Mega pin 21 (SCL)
 *   LCD VCC  → 5 V,  GND → GND
 *   KP rows  → pins 30-33
 *   KP cols  → pins 34-36
 *   CT out   → A0 (via signal-conditioning circuit)
 *   Trip     → pin 7 (active LOW → relay driver)
 *   LEDs     → pins 8 (green), 9 (yellow), 10 (red)
 */

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <stdlib.h>
#include "relay_curves.h"
#include "display.h"

/* ═══════════════════════════════════════════════════════════════
 *  Hardware constants
 * ═══════════════════════════════════════════════════════════════ */
#define ADC_PIN          A0
#define TRIP_PIN         7
#define GREEN_LED_PIN    8
#define YELLOW_LED_PIN   9
#define RED_LED_PIN      10

const float Vref       = 5.0f;
const float ADC_res    = 1023.0f;
float       sensitivity = 0.85f;   /* V/A — calibrate after CT install */

#define ADC_SAMPLES      500

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
static uint16_t adc_buf[ADC_SAMPLES];

Standard   user_standard      = STD_IEC;
IEC_Curve  user_iec_curve     = IEC_SI;
IEEE_Curve user_ieee_curve    = IEEE_MOD_INV;
float      user_tms           = TMS_DEFAULT;
float      user_pickup        = I_PICKUP_DEFAULT;
float      user_inst_multiple = INST_MULTIPLE_DEFAULT;

static float trip_fraction = 0.0f;

RelayState relay_state    = RELAY_NORMAL;
uint32_t   fault_start_ms = 0;
float      trip_time_ms   = 0.0f;

uint32_t   blink_counter  = 0;
bool       red_led_on     = false;

/* RelayMenu owns the LCD + keypad + all menu logic */
RelayMenu relayMenu(user_standard, user_iec_curve, user_ieee_curve,
                    user_tms, user_pickup, user_inst_multiple);

/* ═══════════════════════════════════════════════════════════════
 *  Forward declarations
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void);
void update_leds(RelayState state);

/* ═══════════════════════════════════════════════════════════════
 *  setup()
 * ═══════════════════════════════════════════════════════════════ */
void setup()
{
    Serial.begin(115200);          /* kept for debug output only */
    Wire.begin();

    pinMode(TRIP_PIN,       OUTPUT);
    pinMode(GREEN_LED_PIN,  OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN,    OUTPUT);
    digitalWrite(TRIP_PIN, HIGH);  /* relay closed = normal */
    update_leds(RELAY_NORMAL);

    relayMenu.init();              /* splash screen + draw main menu */
}

/* ═══════════════════════════════════════════════════════════════
 *  loop()
 * ═══════════════════════════════════════════════════════════════ */
void loop()
{
    /* ── Poll keypad every loop iteration ── */
    relayMenu.tick();

    /* ── Protection runs only after user presses START in menu ── */
    if (relayMenu.m_protection_enabled) {
        run_protection();
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  run_protection()  — unchanged logic, LCD update added
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void)
{
    /* If latched, hold relay open and wait for RESET via menu */
    if (relayMenu.m_latched) {
        digitalWrite(TRIP_PIN, LOW);
        update_leds(RELAY_TRIPPED);
        relayMenu.update_live(0.0f, 0.0f, "TRIPPED-LATCH", 0.0f);
        return;
    }

    /* Collect one window of ADC samples */
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_buf[i] = (uint16_t)analogRead(ADC_PIN);
        delayMicroseconds(200);
    }

    float offset = 0.0f;
    float Vrms   = calc_rms(adc_buf, ADC_SAMPLES, Vref, ADC_res, &offset);
    float I_fault = Vrms * sensitivity;
    float M       = I_fault / user_pickup;

    /* Debug to Serial (optional — harmless if no PC connected) */
    Serial.print(F("I="));  Serial.print(I_fault, 3);
    Serial.print(F(" M=")); Serial.println(M, 3);

    if (M > 1.0f)
    {
        float t_trip;
        if (user_standard == STD_IEC)
            t_trip = trip_time_iec (M, user_tms, user_inst_multiple, user_iec_curve);
        else
            t_trip = trip_time_ieee(M, user_tms, user_inst_multiple, user_ieee_curve);

        /* ── Instantaneous zone ── */
        if (M >= user_inst_multiple)
        {
            relay_state         = RELAY_TRIPPED;
            relayMenu.m_latched = true;
            digitalWrite(TRIP_PIN, LOW);
            update_leds(RELAY_TRIPPED);
            relayMenu.update_live(I_fault, M, "INST-TRIP", 0.0f);
            Serial.println(F("INST_TRIP"));
            return;
        }

        switch (relay_state)
        {
            case RELAY_NORMAL:
                relay_state    = RELAY_FAULT_PENDING;
                fault_start_ms = millis();
                trip_fraction  = 0.0f;
                trip_time_ms   = t_trip * 1000.0f;
                update_leds(RELAY_FAULT_PENDING);
                Serial.print(F("FAULT_START M="));
                Serial.print(M, 2);
                Serial.print(F(" Ttrip="));
                Serial.println(t_trip, 3);
                break;

            case RELAY_FAULT_PENDING:
            {
                float window_s  = (float)ADC_SAMPLES * 200e-6f;  /* ≈ 0.1 s */
                trip_fraction  += window_s / t_trip;

                if (trip_fraction >= 1.0f)
                {
                    relay_state         = RELAY_TRIPPED;
                    relayMenu.m_latched = true;
                    digitalWrite(TRIP_PIN, LOW);
                    update_leds(RELAY_TRIPPED);
                    relayMenu.update_live(I_fault, M, "TRIPPED", 0.0f);
                    Serial.println(F("TRIP_EXECUTED"));
                }
                else
                {
                    float remaining = (1.0f - trip_fraction) * t_trip;
                    relayMenu.update_live(I_fault, M, "FAULT", remaining);
                }
                break;
            }

            case RELAY_TRIPPED:
                relayMenu.update_live(I_fault, M, "TRIPPED", 0.0f);
                break;
        }
    }
    else
    {
        /* ── Fault cleared ── */
        if (relay_state != RELAY_NORMAL && !relayMenu.m_latched)
        {
            relay_state   = RELAY_NORMAL;
            trip_fraction = 0.0f;
            digitalWrite(TRIP_PIN, HIGH);
            update_leds(RELAY_NORMAL);
            Serial.println(F("FAULT_CLEARED"));
        }
        relayMenu.update_live(I_fault, M, "OK", 0.0f);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  update_leds()  — unchanged
 * ═══════════════════════════════════════════════════════════════ */
void update_leds(RelayState state)
{
    switch (state)
    {
        case RELAY_NORMAL:
            red_led_on    = false;
            blink_counter = millis();
            digitalWrite(GREEN_LED_PIN,  HIGH);
            digitalWrite(YELLOW_LED_PIN, LOW);
            digitalWrite(RED_LED_PIN,    LOW);
            break;

        case RELAY_FAULT_PENDING:
            red_led_on    = false;
            blink_counter = millis();
            digitalWrite(GREEN_LED_PIN,  LOW);
            digitalWrite(YELLOW_LED_PIN, HIGH);
            digitalWrite(RED_LED_PIN,    LOW);
            break;

        case RELAY_TRIPPED:
            digitalWrite(GREEN_LED_PIN,  LOW);
            digitalWrite(YELLOW_LED_PIN, LOW);
            if ((millis() - blink_counter) >= 500) {
                blink_counter = millis();
                red_led_on    = !red_led_on;
            }
            digitalWrite(RED_LED_PIN, red_led_on ? HIGH : LOW);
            break;
    }
}
