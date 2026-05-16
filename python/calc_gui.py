import tkinter as tk
from tkinter import ttk, messagebox
inst_multiple_default = 2.4  # Default M threshold for instant trip
# ================= IEC CURVES =================
IEC_CURVES = {
    "Standard Inverse (SI)": {"k": 0.14, "alpha": 0.02},
    "Very Inverse (VI)": {"k": 13.5, "alpha": 1},
    "Extremely Inverse (EI)": {"k": 80, "alpha": 2},
    "Long Time Inverse (LTI)": {"k": 120, "alpha": 1}
}

# ================= IEEE CURVES =================
IEEE_CURVES = {
    "Moderately Inverse (MI)": {"A": 0.0515, "B": 0.114, "p": 0.02},
    "Very Inverse (VI)": {"A": 19.61, "B": 0.491, "p": 2},
    "Extremely Inverse (EI)": {"A": 28.2, "B": 0.1217, "p": 2},
    "Short Time Inverse (STI)": {"A": 5.95, "B": 0.18, "p": 1}
}

# ================= UPDATE CURVES =================
def update_curves(event=None):
    system = system_var.get()

    if system == "IEC":
        curves = list(IEC_CURVES.keys())
    else:
        curves = list(IEEE_CURVES.keys())

    curve_menu['values'] = curves
    curve_var.set(curves[0])

# ================= CALCULATION =================
def calculate_time():
    try:
        system = system_var.get()
        curve = curve_var.get()

        TMS = float(tms_entry.get())
        Ip = float(ipickup_entry.get())
        I = float(ifault_entry.get())

        if Ip <= 0:
            messagebox.showerror("Error", "Pickup current must be > 0")
            return

        ratio = I / Ip

        if ratio <= 1:
            messagebox.showerror("Error", "Fault current must be greater than pickup current.")
            return

        # ===== IEC =====
        if system == "IEC":
            k = IEC_CURVES[curve]["k"]
            alpha = IEC_CURVES[curve]["alpha"]
            if ratio >= inst_multiple_default:
                t = 0.02  # Instantaneous trip
            else:
                t = TMS * (k / (ratio ** alpha - 1))

        # ===== IEEE =====
        else:
            A = IEEE_CURVES[curve]["A"]
            B = IEEE_CURVES[curve]["B"]
            p = IEEE_CURVES[curve]["p"]
            if ratio >= inst_multiple_default:
                t = 0.02  # Instantaneous trip
            else:
                t = TMS * ((A / (ratio ** p - 1)) + B)

        result_var.set(f"Operating Time: {t:.4f} sec")

    except ValueError:
        messagebox.showerror("Error", "Please enter valid numeric values.")

# ================= GUI =================
root = tk.Tk()
root.title("IDMT Overcurrent Relay Calculator")
root.geometry("420x400")

# ===== VARIABLES =====
system_var = tk.StringVar(value="IEC")
curve_var = tk.StringVar()
result_var = tk.StringVar()

# ===== SYSTEM =====
tk.Label(root, text="Select System").pack()

system_menu = ttk.Combobox(root, textvariable=system_var, values=["IEC", "IEEE"])
system_menu.pack()
system_menu.bind("<<ComboboxSelected>>", update_curves)

# ===== CURVE =====
tk.Label(root, text="Select Curve").pack()

curve_menu = ttk.Combobox(root, textvariable=curve_var)
curve_menu.pack()

# ===== INPUTS =====
tk.Label(root, text="TMS").pack()
tms_entry = tk.Entry(root)
tms_entry.pack()

tk.Label(root, text="Pickup Current (Ip)").pack()
ipickup_entry = tk.Entry(root)
ipickup_entry.pack()

tk.Label(root, text="Fault Current (I)").pack()
ifault_entry = tk.Entry(root)
ifault_entry.pack()

# ===== BUTTON =====
tk.Button(root, text="Calculate Operating Time", command=calculate_time).pack(pady=10)

# ===== RESULT =====
tk.Label(root, textvariable=result_var, fg="blue").pack()

# ===== INIT =====
update_curves()

root.mainloop()