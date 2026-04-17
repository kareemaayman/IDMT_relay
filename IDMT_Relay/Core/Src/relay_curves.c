#include "relay_curves.h"
#include <math.h>
#include <stdint.h>

// ================= IEC =================
typedef struct { float k; float alpha; } IEC_Params;

static const IEC_Params IEC_TABLE[4] = {
    /* IEC_SI  */  { 0.14f,  0.02f },
    /* IEC_VI  */  { 13.5f,  1.00f },
    /* IEC_EI  */  { 80.0f,  2.00f },
    /* IEC_LTI */  { 120.0f, 1.00f }
};

float trip_time_iec(float M, float TMS, IEC_Curve curve)
{
    if (M <= 1.0f) {
        return (float)RELAY_ERR_NO_TRIP;   /* relay does not trip below pickup */
    }

    const IEC_Params *p = &IEC_TABLE[curve];

    float denom = powf(M, p->alpha) - 1.0f;
    return TMS * (p->k / denom);
}

// ================= IEEE =================
typedef struct { float A; float B; float p; } IEEE_Params;

static const IEEE_Params IEEE_TABLE[3] = {
    /* IEEE_MOD_INV  */  { 0.0515f, 0.1140f, 0.02f },
    /* IEEE_VERY_INV */  { 19.61f,  0.4910f, 2.00f },
    /* IEEE_EXT_INV  */  { 28.2f,   0.1217f, 2.00f }
};

float trip_time_ieee(float M, float TDS, IEEE_Curve curve)
{
    if (M <= 1.0f) {
        return (float)RELAY_ERR_NO_TRIP;
    }

    const IEEE_Params *p = &IEEE_TABLE[curve];

    float denom = powf(M, p->p) - 1.0f;
    return TDS * ((p->A / denom) + p->B);
}

// float trip_time(float I_rms, float Ip, float TD, CurveType curve)
// {
//     float M = I_rms / Ip;

//     if (curve <= IEC_LONG)
//         return trip_time_iec(M, TD, curve);

//     else
//         return trip_time_ieee(M, TD, curve);
// }

/* ══════════════════════════════════════════════════════════════════════
 *  calc_rms()
 * ══════════════════════════════════════════════════════════════════════ */
float calc_rms(const uint16_t *samples, int n, uint16_t dc_offset)
{
    if (samples == 0 || n <= 0) {
        return -1.0f;
    }

    float sum_sq = 0.0f;

    for (int i = 0; i < n; i++) {
        /* Remove DC bias BEFORE squaring — critical for ADC-sampled CT signals */
        float s = (float)samples[i] - (float)dc_offset;
        sum_sq += s * s;
    }

    return sqrtf(sum_sq / (float)n);
}