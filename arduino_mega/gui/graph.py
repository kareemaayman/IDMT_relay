import serial
import matplotlib.pyplot as plt
from collections import deque
import re

PORT = '/dev/cu.usbmodem101'
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Serial connected on {PORT} at {BAUD} baud")
except serial.SerialException as e:
    raise SystemExit(f"Could not open serial port {PORT}: {e}")

# The firmware only streams graphable status after protection is started.
ser.write(b'S')
print("Sent start command: S")
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
    match = re.search(r't=([\d.]+)', line)
    if not match:
        return None
    return float(match.group(1)) / 1000.0

def parse_m(line):
    match = re.search(r'\bM=([\d.]+)', line)
    if not match:
        return None
    return float(match.group(1))

def append_sample(line):
    t = parse_time_s(line)
    M = parse_m(line)

    if M is None:
        return None, None

    if t is None:
        if t_data:
            t = t_data[-1]
        else:
            t = 0.0

    t_data.append(t)
    i_data.append(M)
    return t, M

def redraw():
    ax.clear()
    ax.plot(t_data, i_data, color='blue', linewidth=1.5, label="M (x pickup)")

    fault_label_used = False
    for (t, m) in fault_start:
        label = None if fault_label_used else "Fault Start"
        ax.axvline(x=t, color='orange', linestyle='--', linewidth=1, label=label)
        fault_label_used = True

    trip_label_used = False
    for (t, m) in trip_times:
        label = None if trip_label_used else "TRIP"
        ax.scatter(t, m, color='red', s=120, zorder=5, label=label)
        trip_label_used = True

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("M (multiples of pickup)")
    ax.set_title("IDMT Relay Monitor")
    if t_data or fault_start or trip_times:
        ax.legend()
    plt.pause(0.01)

while True:
    line = ser.readline().decode(errors='ignore').strip()
    # And change readline to read() if using the simulator subprocess approach:
    # line = ser.readline().strip()
    if not line:
        print("Waiting for relay data...")
        plt.pause(0.01)
        continue

    print(line)

    try:
        # Check exact/specific states before generic FAULT parsing.
        if line.startswith("FAULT_CLEARED"):
            fault_start.clear()
            trip_times.clear()
            print(">> Fault cleared")
            redraw()

        # ── OK t=1.234 M=0.75 ─────────────────────────────
        elif line.startswith("OK") or line.startswith("DEBUG:"):
            append_sample(line)
            redraw()

        # ── FAULT_START t=1.234 M=2.10 Ttrip_theory=1.234s ─
        elif line.startswith("FAULT_START"):
            t, M = append_sample(line)
            t_trip = re.search(r'Ttrip_theory=([\d.]+)', line).group(1)
            if t is not None and M is not None:
                fault_start.append((t, M))
            print(f">> Fault started — trip in {t_trip}s")
            redraw()

        # ── FAULT M=2.10 Tremain=0.800s ────────────────────
        elif line.startswith("FAULT"):
            append_sample(line)
            rem = re.search(r'Tremain=([\d.]+)', line).group(1)
            print(f">> Fault ongoing — {rem}s remaining")
            redraw()

        # ── TRIP_EXECUTED ───────────────────────────────────
        elif line.startswith("TRIP_EXECUTED"):
            if t_data:
                trip_times.append((t_data[-1], i_data[-1]))
            print(">> TRIPPED")
            redraw()

        # ── INST_TRIP t=1.234 M=4.00 ───────────────────────
        elif line.startswith("INST_TRIP"):
            t, M = append_sample(line)
            if t is not None and M is not None:
                trip_times.append((t, M))
            print(">> INSTANT TRIP")
            redraw()

        # ── TRIPPED M=2.10 (post-trip reports) ─────────────
        elif line.startswith("TRIPPED"):
            append_sample(line)
            redraw()
    except Exception as e:
        print("Parse error:", line, "->", e)
