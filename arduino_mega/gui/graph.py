import serial
import matplotlib.pyplot as plt
from collections import deque
import re

ser = serial.Serial('COM5', 115200)
# Add this instead to use the simulator output:
# import subprocess
# import io
# proc = subprocess.Popen(['python', 'simulator.py'], stdout=subprocess.PIPE)
# ser = io.TextIOWrapper(proc.stdout)

t_data      = deque(maxlen=200)
i_data      = deque(maxlen=200)
trip_times  = []   # stores (t, M) of every TRIP_EXECUTED
fault_start = []   # stores (t, M) of every FAULT_START

plt.ion()
fig, ax = plt.subplots()

def parse_time_s(line):
    return float(re.search(r't=([\d.]+)', line).group(1)) / 1000.0

def redraw():
    ax.clear()
    ax.plot(t_data, i_data, color='blue', linewidth=1.5, label="M (x pickup)")

    for (t, m) in fault_start:
        ax.axvline(x=t, color='orange', linestyle='--', linewidth=1, label="Fault Start")

    for (t, m) in trip_times:
        ax.scatter(t, m, color='red', s=120, zorder=5, label="TRIP")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("M (multiples of pickup)")
    ax.set_title("IDMT Relay Monitor")
    ax.legend()
    plt.pause(0.01)

while True:
    line = ser.readline().decode(errors='ignore').strip()
    # And change readline to read() if using the simulator subprocess approach:
    # line = ser.readline().strip()
    # if not line:
    #     continue

    try:
        # ── OK t=1.234 M=0.75 ─────────────────────────────
        if line.startswith("OK"):
            t = parse_time_s(line)
            M = float(re.search(r'M=([\d.]+)', line).group(1))
            t_data.append(t)
            i_data.append(M)
            redraw()

        # ── FAULT_START t=1.234 M=2.10 Ttrip_theory=1.234s ─
        elif line.startswith("FAULT_START"):
            t = parse_time_s(line)
            M = float(re.search(r'M=([\d.]+)', line).group(1))
            t_trip = re.search(r'Ttrip_theory=([\d.]+)', line).group(1)
            t_data.append(t)
            i_data.append(M)
            fault_start.append((t, M))
            print(f">> Fault started — trip in {t_trip}s")
            redraw()

        # ── FAULT M=2.10 Tremain=0.800s ────────────────────
        elif line.startswith("FAULT"):
            t = parse_time_s(line)
            M = float(re.search(r'M=([\d.]+)', line).group(1))
            rem = re.search(r'Tremain=([\d.]+)', line).group(1)
            t_data.append(t)
            i_data.append(M)
            print(f">> Fault ongoing — {rem}s remaining")
            redraw()

        # ── FAULT_CLEARED ───────────────────────────────────
        elif line.startswith("FAULT_CLEARED"):
            fault_start.clear()
            trip_times.clear()
            print(">> Fault cleared")
            redraw()

        # ── TRIP_EXECUTED ───────────────────────────────────
        elif line.startswith("TRIP_EXECUTED"):
            if t_data:
                trip_times.append((t_data[-1], i_data[-1]))
            print(">> TRIPPED")
            redraw()

        # ── INST_TRIP t=1.234 M=4.00 ───────────────────────
        elif line.startswith("INST_TRIP"):
            t = parse_time_s(line)
            M = float(re.search(r'M=([\d.]+)', line).group(1))
            t_data.append(t)
            i_data.append(M)
            trip_times.append((t, M))
            print(">> INSTANT TRIP")
            redraw()

        # ── TRIPPED M=2.10 (post-trip reports) ─────────────
        elif line.startswith("TRIPPED"):
            t = parse_time_s(line)
            M = float(re.search(r'M=([\d.]+)', line).group(1))
            t_data.append(t)
            i_data.append(M)
            redraw()
    except Exception as e:
        print("Parse error:", line, "->", e)
