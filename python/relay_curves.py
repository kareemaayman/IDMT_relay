import math

# ─── Constants ──────────────────────────────────────────────────────────────
DEFAULT_INST_MULT    = 10.0   # Instantaneous zone: M >= this → instant trip
INST_TRIP_TIME_S     = 0.020  # "Instant" = 1 cycle (20ms), not true zero

# IEC 60255 — t = TMS * k / (M^alpha - 1)
IEC_CURVES = {
    "standard_inverse":   {"k": 0.14,  "alpha": 0.02},
    "very_inverse":       {"k": 13.5,  "alpha": 1.0},
    "extremely_inverse":  {"k": 80.0,  "alpha": 2.0},
    "long_time_inverse":  {"k": 120.0, "alpha": 1.0},
}

# IEEE C37.112 — t = TDS * [A / (M^p - 1) + B]
IEEE_CURVES = {
    "ieee_moderately_inverse": {"A": 0.0515, "B": 0.1140, "p": 0.02},
    "ieee_very_inverse":       {"A": 19.61,  "B": 0.4910, "p": 2.0},
    "ieee_extremely_inverse":  {"A": 28.2,   "B": 0.1217, "p": 2.0},
}

def trip_time_iec(I_rms, Ip, TMS, curve="standard_inverse"):
    c = IEC_CURVES[curve]
    M = I_rms / Ip
    if M <= 1.0:
        return float('inf')
    elif M >= DEFAULT_INST_MULT:
        return INST_TRIP_TIME_S
    return TMS * c["k"] / (M ** c["alpha"] - 1.0)

def trip_time_ieee(I_rms, Ip, TDS, curve="ieee_moderately_inverse"):
    c = IEEE_CURVES[curve]
    M = I_rms / Ip
    if M <= 1.0:
        return float('inf')
    elif M >= DEFAULT_INST_MULT:
        return INST_TRIP_TIME_S
    return TDS * (c["A"] / (M ** c["p"] - 1.0) + c["B"])  # note the + B

def trip_time(I_rms, Ip, TD, curve):
    """Unified entry point — picks the right equation automatically."""
    if curve in IEC_CURVES:
        return trip_time_iec(I_rms, Ip, TD, curve)
    elif curve in IEEE_CURVES:
        return trip_time_ieee(I_rms, Ip, TD, curve)
    else:
        raise ValueError(f"Unknown curve: {curve}")

def calc_rms(samples, dc_offset=None):
    """
    RMS from raw ADC samples.
    dc_offset: ADC midpoint (e.g. 2048 for 12-bit).
               If None, auto-calculates mean from the buffer.
    """
    N = len(samples)
    if dc_offset is None:
        dc_offset = sum(samples) / N       # estimate from buffer mean
    centered = [x - dc_offset for x in samples]
    return math.sqrt(sum(x**2 for x in centered) / N)
for curve in IEC_CURVES:
    print(curve, trip_time(2, 1.0, 1.0, curve))

for curve in IEEE_CURVES:
    print(curve, trip_time(5, 1.0, 1.0, curve))