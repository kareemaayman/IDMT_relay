#include "relay_curves.h"
#include <math.h>

/* ══════════════════════════════════════════════════════════════
 *  IEC 60255  curves
 * ══════════════════════════════════════════════════════════════ */
typedef struct { float k; float alpha; } IEC_Params;

static const IEC_Params IEC_TABLE[4] = {
    /* IEC_SI  */  { 0.14f,   0.02f },
    /* IEC_VI  */  { 13.5f,   1.00f },
    /* IEC_EI  */  { 80.0f,   2.00f },
    /* IEC_LTI */  { 120.0f,  1.00f }
};

float trip_time_iec(float M, float TMS, float inst_multiple, IEC_Curve curve)
{
    if (M <= 1.0f)        return RELAY_ERR_NO_TRIP;
    if (M >= inst_multiple) return RELAY_INST_ZONE;

    const IEC_Params *p = &IEC_TABLE[curve];
    float denom = powf(M, p->alpha) - 1.0f;
    return TMS * (p->k / denom);
}

/* ══════════════════════════════════════════════════════════════
 *  IEEE C37.112  curves
 * ══════════════════════════════════════════════════════════════ */
typedef struct { float A; float B; float p; } IEEE_Params;

static const IEEE_Params IEEE_TABLE[3] = {
    /* IEEE_MOD_INV  */  { 0.0515f,  0.1140f, 0.02f },
    /* IEEE_VERY_INV */  { 19.61f,   0.4910f, 2.00f },
    /* IEEE_EXT_INV  */  { 28.2f,    0.1217f, 2.00f }
};

float trip_time_ieee(float M, float TDS, float inst_multiple, IEEE_Curve curve)
{
    if (M <= 1.0f)        return RELAY_ERR_NO_TRIP;
    if (M >= inst_multiple) return RELAY_INST_ZONE;

    const IEEE_Params *p = &IEEE_TABLE[curve];
    float denom = powf(M, p->p) - 1.0f;
    return TDS * ((p->A / denom) + p->B);
}

/* ══════════════════════════════════════════════════════════════
 *  RMS helper  (works for any ADC word-width)
 * ══════════════════════════════════════════════════════════════ */
float calc_rms(const uint16_t *samples, int n, float Vref, float ADC_res, float *out_offset)
{
    if (samples == 0 || n <= 0) return -1.0f;

    // 1. Measure OFFSET
    float offset = 0;
    for (int i = 0; i < n; i++) {
        offset += samples[i];
    }
    offset /= n;

    if (out_offset) {
        *out_offset = offset;
    }

    // 2. Measure RMS
    float sum = 0;
    for (int i = 0; i < n; i++) {
        float raw = samples[i];
        float voltage = raw * (Vref / ADC_res);
        float offset_voltage = offset * (Vref / ADC_res);
        float centered = voltage - offset_voltage;
        sum += centered * centered;
    }

    return sqrtf(sum / n);
}
