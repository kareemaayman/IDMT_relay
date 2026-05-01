import customtkinter as ctk
import serial
import threading
import time

# ================= SERIAL CONFIG =================
PORT = "tty.usbmodem-"   # change if needed
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print("Serial connected")
except Exception as e:
    print("Serial NOT connected:", e)
    ser = None

# ================= SEND FUNCTION =================
def send(cmd):
    if ser is None:
        textbox.insert("end", "\n[NO SERIAL - OFFLINE MODE]\n")
        return

    try:
        ser.write(cmd.encode())
        time.sleep(0.2)
    except Exception as e:
        textbox.insert("end", "\n[SEND ERROR]: {}\n".format(e))

# ================= APPLY SETTINGS =================
def apply_settings():
    try:
        # ---- STANDARD ----
        send('1')

        if system_var.get() == "IEC":
            send('1')
        else:
            send('2')

        # ---- CURVE ----
        send('2')

        if system_var.get() == "IEC":
            mapping = {
                "SI": '1',
                "VI": '2',
                "EI": '3',
                "LTI": '4'
            }
        else:
            mapping = {
                "MODERATE_INV": '1',
                "VERY_INV": '2',
                "EXTREME_INV": '3'
            }

        send(mapping[curve_var.get()])

        # ---- TMS ----
        send('3')
        send(tms_entry.get() + '\r')

        # ---- PICKUP ----
        send('4')
        send(pickup_entry.get() + '\r')

        # ---- INSTANT MULTIPLE ----
        send('5')
        send(inst_entry.get() + '\r')

        textbox.insert("end", "\n[SETTINGS SENT]\n")

    except Exception as e:
        textbox.insert("end", "\n[ERROR]: {}\n".format(e))
        
# ================= START PROTECTION =================
def start_protection():
    send('S')
    textbox.insert("end", "\n[PROTECTION STARTED]\n")
    textbox.see("end")
    start_btn.configure(state="disabled", text="Protection Active")
    reset_btn.configure(state="normal")
    
# ================= RESET FAULT =================
def reset_fault():
    send('6')        # menu option 6 = reset latch
    time.sleep(0.2)
    send('Y')        # confirm reset
    textbox.insert("end", "\n[FAULT LATCH RESET]\n")
    textbox.see("end")
    start_btn.configure(state="normal", text="Start Protection")
    reset_btn.configure(state="disabled")

# ================= SAFE UART READING =================
def update_text(data):
    textbox.insert("end", data)
    textbox.see("end")
    # Auto-update button states based on relay messages
    if "TRIP" in data or "LATCHED" in data:
        reset_btn.configure(state="normal")
        start_btn.configure(state="disabled", text="Protection Active")
    elif "FAULT_CLEARED" in data or "Latch cleared" in data:
        start_btn.configure(state="normal", text="Start Protection")
        reset_btn.configure(state="disabled")

def read_uart():
    while True:
        try:
            if ser and ser.in_waiting:
                data = ser.readline().decode(errors='ignore')
                app.after(0, update_text, data)
        except:
            pass

def start_thread():
    thread = threading.Thread(target=read_uart, daemon=True)
    thread.start()

# ================= CURVE UPDATE =================
def update_curves(*args):
    if system_var.get() == "IEC":
        curve_menu.configure(values=["SI", "VI", "EI", "LTI"])
        curve_var.set("SI")
    else:
        curve_menu.configure(values=["MODERATE_INV", "VERY_INV", "EXTREME_INV"])
        curve_var.set("MODERATE_INV")

# ================= GUI =================
ctk.set_appearance_mode("light")
ctk.set_default_color_theme("blue")

app = ctk.CTk()
app.geometry("600x550")
app.title("Relay Control GUI")

# ---- TITLE ----
title = ctk.CTkLabel(app, text="IDMT Relay Interface",
                     font=("Segoe UI", 18, "bold"))
title.pack(pady=10)

# ---- SYSTEM ----
system_var = ctk.StringVar(value="IEC")
system_var.trace_add('write', update_curves)
system_menu = ctk.CTkOptionMenu(
    app,
    values=["IEC", "IEEE"],
    variable=system_var
)
system_menu.pack(pady=5)

# ---- CURVE ----
curve_var = ctk.StringVar(value="SI")
curve_menu = ctk.CTkOptionMenu(
    app,
    values=["SI", "VI", "EI", "LTI"],
    variable=curve_var
)
curve_menu.pack(pady=5)

# ---- TMS ----
tms_entry = ctk.CTkEntry(app, placeholder_text="Enter TMS (e.g. 0.5)")
tms_entry.pack(pady=5)

# ---- PICKUP ----
pickup_entry = ctk.CTkEntry(app, placeholder_text="Pickup Current (A)")
pickup_entry.pack(pady=5)

# ---- APPLY BUTTON ----
apply_btn = ctk.CTkButton(app, text="Apply Settings", command=apply_settings)
apply_btn.pack(pady=10)

# ---- START / RESET BUTTONS (side by side) ----
btn_frame = ctk.CTkFrame(app, fg_color="transparent")
btn_frame.pack(pady=5)

start_btn = ctk.CTkButton(
    btn_frame,
    text="Start Protection",
    fg_color="#2e7d32",
    hover_color="#1b5e20",
    width=180,
    command=start_protection
)
start_btn.grid(row=0, column=0, padx=10)

reset_btn = ctk.CTkButton(
    btn_frame,
    text="Reset Fault Latch",
    fg_color="#b71c1c",
    hover_color="#7f0000",
    width=180,
    state="disabled",          # only enabled after a trip
    command=reset_fault
)
reset_btn.grid(row=0, column=1, padx=10)

# ---- INSTANT MULTIPLE ----
inst_entry = ctk.CTkEntry(app, placeholder_text="Instant Multiple")
inst_entry.pack(pady=5)

# ---- TERMINAL OUTPUT ----
textbox = ctk.CTkTextbox(app, width=550, height=300)
textbox.pack(pady=10)

# ---- START THREAD ----
app.after(100, start_thread)

# ---- RUN ----
app.mainloop()