/**
 * test_main.c
 *
 * PC test harness for relay_curves.c
 * Mirrors the same checks from test_curves.py so you can confirm
 * the C math matches the Python prototype before touching the MCU.
 *
 * Compile:  gcc test_main.c relay_curves.c -o test_relay -lm
 * Run:      ./test_relay
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "relay_curves.h"

/* ── tiny test framework ───────────────────────────────────────────── */
static int tests_run    = 0;
static int tests_passed = 0;

static void check(const char *name, float got, float expected, float tol)
{
    tests_run++;
    float diff = fabsf(got - expected);
    if (diff <= tol) {
        printf("  [PASS] %-40s  got=%.4f\n", name, got);
        tests_passed++;
    } else {
        printf("  [FAIL] %-40s  got=%.4f  expected=%.4f  diff=%.4f\n",
               name, got, expected, diff);
    }
}

/* ── RMS tests ─────────────────────────────────────────────────────── */
static void test_rms(void)
{
    printf("\n=== RMS Tests ===\n");

    /*
     * Build a synthetic 50 Hz sine wave biased to ADC midpoint (2048).
     * 64 samples per cycle.  Amplitude = 1000 counts.
     * Expected RMS of AC part = 1000 / sqrt(2) ≈ 707.1
     */
    #define N_SAMPLES 64
    #define DC_OFFSET 2048
    #define AMPLITUDE 1000.0f

    uint16_t buf[N_SAMPLES];
    for (int i = 0; i < N_SAMPLES; i++) {
        /* sinf returns -1..+1; scale then add DC bias */
        float angle = 2.0f * 3.14159265f * (float)i / (float)N_SAMPLES;
        buf[i] = (uint16_t)(DC_OFFSET + (int)(AMPLITUDE * sinf(angle)));
    }

    float rms = calc_rms(buf, N_SAMPLES, DC_OFFSET);
    check("RMS sine 64 samples (expect ~707.1)", rms, 707.1f, 2.0f);

    /* All samples equal to DC offset → AC part = 0, RMS = 0 */
    for (int i = 0; i < N_SAMPLES; i++) buf[i] = DC_OFFSET;
    rms = calc_rms(buf, N_SAMPLES, DC_OFFSET);
    check("RMS flat DC only (expect 0.0)", rms, 0.0f, 0.01f);

    /* NULL pointer guard */
    rms = calc_rms(0, N_SAMPLES, DC_OFFSET);
    check("RMS null pointer (expect -1.0)", rms, -1.0f, 0.001f);
}

/* ── IEC curve tests ───────────────────────────────────────────────── */
static void test_iec(void)
{
    printf("\n=== IEC Curve Tests ===\n");

    /*
     * Reference values computed from Python prototype (test_curves.py).
     * M=2.0, TMS=1.0 for all — easy to hand-verify.
     *
     * IEC SI:   0.14 / (2^0.02  - 1) = 0.14 / 0.01396  ≈ 10.03 s
     * IEC VI:   13.5 / (2^1.0   - 1) = 13.5 / 1.0       = 13.50 s
     * IEC EI:   80.0 / (2^2.0   - 1) = 80.0 / 3.0        ≈ 26.67 s
     * IEC LTI: 120.0 / (2^1.0   - 1) = 120.0 / 1.0       = 120.0 s
     */
    check("IEC SI  M=2 TMS=1  (~10.03s)", trip_time_iec(2.0f, 1.0f, IEC_SI),   10.03f, 0.05f);
    check("IEC VI  M=2 TMS=1  (13.50s)", trip_time_iec(2.0f, 1.0f, IEC_VI),   13.50f, 0.05f);
    check("IEC EI  M=2 TMS=1  (26.67s)", trip_time_iec(2.0f, 1.0f, IEC_EI),   26.67f, 0.05f);
    check("IEC LTI M=2 TMS=1  (120.0s)", trip_time_iec(2.0f, 1.0f, IEC_LTI), 120.00f, 0.10f);

    /* TMS scaling: halving TMS must halve the trip time */
    float t_full = trip_time_iec(5.0f, 1.0f, IEC_SI);
    float t_half = trip_time_iec(5.0f, 0.5f, IEC_SI);
    check("IEC SI  TMS halving scales time", t_half, t_full * 0.5f, 0.001f);

    /* M <= 1.0 must return error */
    float err = trip_time_iec(0.8f, 1.0f, IEC_SI);
    check("IEC SI  M<1 returns RELAY_ERR_NO_TRIP", err, (float)RELAY_ERR_NO_TRIP, 0.001f);
}

/* ── IEEE curve tests ──────────────────────────────────────────────── */
static void test_ieee(void)
{
    printf("\n=== IEEE Curve Tests ===\n");

    /*
     * IEEE Moderately Inverse, M=2, TDS=1:
     *   A=0.0515, p=0.02, B=0.1140
     *   denom = 2^0.02 - 1 = 0.01396
     *   t = 1.0 * (0.0515/0.01396 + 0.1140) = 3.690 + 0.114 = 3.804 s
     *
     * IEEE Very Inverse, M=2, TDS=1:
     *   A=19.61, p=2, B=0.491
     *   denom = 2^2 - 1 = 3.0
     *   t = 1.0 * (19.61/3 + 0.491) = 6.537 + 0.491 = 7.028 s
     *
     * IEEE Extremely Inverse, M=2, TDS=1:
     *   A=28.2, p=2, B=0.1217
     *   denom = 3.0
     *   t = 1.0 * (28.2/3 + 0.1217) = 9.400 + 0.1217 = 9.522 s
     */
    check("IEEE MOD_INV  M=2 TDS=1  (~3.80s)", trip_time_ieee(2.0f, 1.0f, IEEE_MOD_INV),  3.804f, 0.05f);
    check("IEEE VERY_INV M=2 TDS=1  (~7.03s)", trip_time_ieee(2.0f, 1.0f, IEEE_VERY_INV), 7.028f, 0.05f);
    check("IEEE EXT_INV  M=2 TDS=1  (~9.52s)", trip_time_ieee(2.0f, 1.0f, IEEE_EXT_INV),  9.522f, 0.05f);

    /* TDS scaling */
    float t1 = trip_time_ieee(5.0f, 1.0f, IEEE_VERY_INV);
    float t2 = trip_time_ieee(5.0f, 2.0f, IEEE_VERY_INV);
    check("IEEE VERY_INV TDS doubling scales time", t2, t1 * 2.0f, 0.001f);

    /* M <= 1.0 guard */
    float err = trip_time_ieee(1.0f, 1.0f, IEEE_MOD_INV);
    check("IEEE MOD_INV  M=1 returns RELAY_ERR_NO_TRIP", err, (float)RELAY_ERR_NO_TRIP, 0.001f);
}

/* ── summary ───────────────────────────────────────────────────────── */
static void print_summary(void)
{
    printf("\n────────────────────────────────────────\n");
    printf("  Results: %d / %d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        printf("  ALL TESTS PASSED — safe to port to STM32\n");
    else
        printf("  FAILURES DETECTED — fix before porting!\n");
    printf("────────────────────────────────────────\n");
}

/* ── entry point ───────────────────────────────────────────────────── */
int main(void)
{
    printf("relay_curves.c — PC test harness\n");
    test_rms();
    test_iec();
    test_ieee();
    print_summary();
    return (tests_passed == tests_run) ? 0 : 1;
}
