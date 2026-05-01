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
#include "relay_curves.h"
#include "display.h"

/* ═══════════════════════════════════════════════════════════════
 *  Hardware constants
 * ═══════════════════════════════════════════════════════════════ */
#define ADC_PIN          A0
#define TRIP_PIN         7          /* digital output → relay driver     */

/*
 * Calibration parameters
 */
const float Vref = 5.0;        // Arduino reference voltage
const float ADC_res = 1023.0;

// 👉 Adjust this after calibration
float sensitivity = 0.85;

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

/* Instantiate the Menu Library */
RelayMenu relayMenu(user_standard, user_iec_curve, user_ieee_curve, user_tms, user_pickup, user_inst_multiple);

/* ═══════════════════════════════════════════════════════════════
 *  Forward declarations
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void);
static void print_status_1f(const __FlashStringHelper *label, uint32_t t, float M);
static void print_fault_start(uint32_t t, float M, float t_trip);
static void print_fault_pending(uint32_t t, float M, float remaining);

/* ═══════════════════════════════════════════════════════════════
 *  setup()
 * ═══════════════════════════════════════════════════════════════ */
void setup()
{
    Serial.begin(115200);
    pinMode(TRIP_PIN, OUTPUT);
    digitalWrite(TRIP_PIN, HIGH);
    
    Serial.print(F("\r\n"));
    Serial.print(F("═══════════════════════════════════════════\r\n"));
    Serial.print(F("  IDMT PROTECTION RELAY - CONFIGURATION\r\n"));
    Serial.print(F("═══════════════════════════════════════════\r\n"));
    Serial.print(F("\r\nConfigure settings below, then press 'S' to START PROTECTION:\r\n\r\n"));
    
    relayMenu.init();
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

    /* ── ADC sampling + protection logic ─────────────── */
    if (protection_enabled) {
        run_protection();
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Sampling + trip state machine
 *  (mirrors the sample_ready block in STM32 main.c)
 * ═══════════════════════════════════════════════════════════════ */
void run_protection(void)
{
    /* If latched, hold relay open and wait for manual reset via menu */
    if (relayMenu.m_latched) {
        digitalWrite(TRIP_PIN, LOW);     // ensure it stays open
        return;
    }
    /* Collect one window of ADC samples */
    for (int i = 0; i < ADC_SAMPLES; i++) {
        adc_buf[i] = (uint16_t)analogRead(ADC_PIN);
        delayMicroseconds(200);
    }

    float offset = 0.0f;
    float Vrms = calc_rms(adc_buf, ADC_SAMPLES, Vref, ADC_res, &offset);

    float I_fault = Vrms * sensitivity;
    float M       = I_fault / user_pickup;
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
            relayMenu.m_latched = true; 
            digitalWrite(TRIP_PIN, LOW);
            print_status_1f(F("INST_TRIP"), millis(), M);
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
                break;

            case RELAY_FAULT_PENDING:
                float window_s = (float)ADC_SAMPLES * 200e-6f;  // one window ≈ 0.1 s
                trip_fraction += window_s / t_trip;     
                if (trip_fraction >= 1.0f){
                    relay_state = RELAY_TRIPPED;
                    relayMenu.m_latched = true;
                    digitalWrite(TRIP_PIN, LOW);
                    Serial.print(F("TRIP_EXECUTED\r\n"));
                }
                else
                {
                    float remaining = (1.0f - trip_fraction) * t_trip;
                    print_fault_pending(millis(), M, remaining, t_trip);
                }
                break;
                // if ((millis() - fault_start_ms) >= (uint32_t)trip_time_ms)
                // {
                //     relay_state = RELAY_TRIPPED;
                //     relayMenu.m_latched = true;
                //     digitalWrite(TRIP_PIN, LOW);
                //     Serial.print(F("TRIP_EXECUTED\r\n"));
                // }
                // else
                // {
                //     uint32_t elapsed_ms = millis() - fault_start_ms;
                //     float remaining = (trip_time_ms - (float)elapsed_ms) / 1000.0f;
                //     print_fault_pending(millis(), M, remaining);
                // }
                // break;

            case RELAY_TRIPPED:
                print_status_1f(F("TRIPPED"), millis(), M);
                break;
        }
    }
    else{
            // Fault cleared — but only reclose if NOT latched
            if (relay_state != RELAY_NORMAL && !relayMenu.m_latched)
            {
                relay_state = RELAY_NORMAL;
                digitalWrite(TRIP_PIN, HIGH);
                Serial.print(F("FAULT_CLEARED\r\n"));
            }
            else if (!relayMenu.m_latched)
            {
                print_status_1f(F("OK"), millis(), M);
            }
            // if latched: do nothing, relay stays open
    }
}

static void print_status_1f(const __FlashStringHelper *label, uint32_t t, float M)
{
    Serial.print(label);
    Serial.print(F(" t="));
    Serial.print(t);
    Serial.print(F(" M="));
    Serial.print(M, 2);
    Serial.print(F("\r\n"));
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

static void print_fault_pending(uint32_t t, float M, float remaining, float t_trip)
{
    Serial.print(F("FAULT t="));
    Serial.print(t);
    Serial.print(F(" M="));
    Serial.print(M, 2);
    Serial.print(F(" Tremain="));
    Serial.print(remaining, 3);
    Serial.print(F("s\r\n"));
    Serial.print(F("Ttrip_theory="));
    Serial.print(t_trip, 3);
    Serial.print(F("s\r\n"));
}
