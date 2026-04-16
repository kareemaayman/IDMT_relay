from relay_curves import trip_time, IEC_CURVES, IEEE_CURVES  # ← same library
import matplotlib.pyplot as plt
import numpy as np

M_values = np.linspace(1.05, 20, 500)

fig1, ax1 = plt.subplots(figsize=(10, 6))

for name in IEC_CURVES:
    times = [trip_time(m, 1.0, 1.0, name) for m in M_values]
    ax1.plot(M_values, times, label=name)

ax1.set_xscale("log")
ax1.set_yscale("log")
ax1.set_xlabel("M (I / Ip)")
ax1.set_ylabel("Trip time (s)")
ax1.set_title("IEC 60255 Curves (TMS = 1.0)")
ax1.legend()
ax1.grid(True, which="both", ls="--", alpha=0.4)

plt.savefig("iec_curves.png", dpi=150)
print("Saved IEC curves to iec_curves.png")

fig2, ax2 = plt.subplots(figsize=(10, 6))

for name in IEEE_CURVES:
    times = [trip_time(m, 1.0, 1.0, name) for m in M_values]
    ax2.plot(M_values, times, label=name)

ax2.set_xscale("log")
ax2.set_yscale("log")
ax2.set_xlabel("M (I / Ip)")
ax2.set_ylabel("Trip time (s)")
ax2.set_title("IEEE C37.112 Curves (TDS = 1.0)")
ax2.legend()
ax2.grid(True, which="both", ls="--", alpha=0.4)

plt.savefig("ieee_curves.png", dpi=150)
print("Saved IEEE curves to ieee_curves.png")

fig3, ax3 = plt.subplots(figsize=(10, 6))

# IEC → solid
for name in IEC_CURVES:
    times = [trip_time(m, 1.0, 1.0, name) for m in M_values]
    ax3.plot(M_values, times, label=f"IEC - {name}", linestyle="solid")

# IEEE → dashed
for name in IEEE_CURVES:
    times = [trip_time(m, 1.0, 1.0, name) for m in M_values]
    ax3.plot(M_values, times, label=f"IEEE - {name}", linestyle="dashed")

ax3.set_xscale("log")
ax3.set_yscale("log")
ax3.set_xlabel("M (I / Ip)")
ax3.set_ylabel("Trip time (s)")
ax3.set_title("IEC vs IEEE Curves (TMS/TDS = 1.0)")
ax3.legend()
ax3.grid(True, which="both", ls="--", alpha=0.4)

plt.savefig("combined_curves.png", dpi=150)
print("Saved combined curves to combined_curves.png")