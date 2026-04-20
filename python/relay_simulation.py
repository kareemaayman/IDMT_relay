import numpy as np
from relay_curves import trip_time

# ===== SETTINGS =====
fs = 1000          # sampling frequency (Hz)
Ip = 2.0           # pickup current
TMS = 1.0
curve = "standard_inverse"

# Fault level (change this to test)
M_target = 20
I_fault = M_target * Ip

# ===== SIGNAL =====
T=5.0  # total simulation time (s)
t = np.linspace(0, T, int(fs*T))
signal = I_fault * np.sin(2*np.pi*50*t)

# ===== RELAY LOGIC =====
window = 100
time_acc = 0
dt = 1/fs
trip = False

for i in range(window, len(signal)):
    samples = signal[i-window:i]

    # RMS
    I_rms = np.sqrt(np.mean(samples**2))

    # Fault level
    M = I_rms / Ip

    if M > 1:
        t_trip = trip_time(M, TMS, 1.0, curve)

        time_acc += dt

        if time_acc >= t_trip:
            print(f"TRIPPED at t = {i/fs:.3f}s")
            trip = True
            break
    else:
        
        time_acc = 0
        
print(f"M_actual = {M:.2f}")
print(f"time_acc={time_acc:.3f}, t_trip={t_trip:.3f}")

if not trip:
    print("No trip")