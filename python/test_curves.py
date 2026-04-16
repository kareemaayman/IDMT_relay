from relay_curves import trip_time, calc_rms 
import math

print("=== IEC curve checks ===")

t = trip_time(2.0, 1.0, 1.0, "standard_inverse")
print(f"Standard Inverse  M=2  TMS=1 → {t:.3f}s   (expected ~10.03s)")

t = trip_time(2.0, 1.0, 1.0, "very_inverse")
print(f"Very Inverse      M=2  TMS=1 → {t:.3f}s   (expected ~13.5s)")

t = trip_time(10.0, 1.0, 1.0, "extremely_inverse")
print(f"Extremely Inverse M=10 TMS=1 → {t:.3f}s (expected ~0.808s)")

print("\n=== IEEE curve checks ===")

t = trip_time(2.0, 1.0, 1.0, "ieee_moderately_inverse")
print(f"IEEE Mod Inverse  M=2  TDS=1 → {t:.3f}s (expected ~3.803s)")

t = trip_time(2.0, 1.0, 1.0, "ieee_very_inverse")
print(f"IEEE Very Inverse M=2  TDS=1 → {t:.3f}s (expected ~7.028s)")

print("\n=== RMS checks ===")

# Perfect sine wave — result must be amplitude / sqrt(2)
N = 1000
amplitude = 500
dc = 2048
samples = [dc + amplitude * math.sin(2 * math.pi * i / N) for i in range(N)]
rms = calc_rms(samples, dc_offset=dc)
expected = amplitude / math.sqrt(2)
err = abs(rms - expected) / expected * 100
print(f"Sine RMS → {rms:.3f}  expected {expected:.3f}  error {err:.4f}%")

# All flat → RMS should be 0
rms_zero = calc_rms([2048] * 100, dc_offset=2048)
print(f"Flat signal RMS → {rms_zero}  (expected 0.0)")