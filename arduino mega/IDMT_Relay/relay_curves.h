// relay_curves.h
#ifndef RELAY_CURVES_H
#define RELAY_CURVES_H
#include <stdint.h>

/* ── Instantaneous zone ─────────────────────────────────────────────────── */
#define INST_TRIP_TIME_S       0.020f /* 1 cycle @ 50Hz — "definite zero"    */
#define INST_MULTIPLE_DEFAULT  10.0f  /* Default M threshold for instant trip */

typedef enum { IEC_SI, IEC_VI, IEC_EI, IEC_LTI } IEC_Curve;
typedef enum { IEEE_MOD_INV, IEEE_VERY_INV, IEEE_EXT_INV } IEEE_Curve;

/* ── Error / status codes ──────────────────────────────────────────── */
#define RELAY_OK            0
#define RELAY_ERR_NO_TRIP  -1   /* M <= 1.0 : relay will never trip */
#define RELAY_ERR_NULL_PTR -2
#define RELAY_INST_ZONE       INST_TRIP_TIME_S

float calc_rms(const uint16_t *samples, int n, float Vref, float ADC_res, float *out_offset);
float trip_time_iec(float M, float TMS, float inst_multiple, IEC_Curve curve);
float trip_time_ieee(float M, float TDS, float inst_multiple, IEEE_Curve curve);

#endif