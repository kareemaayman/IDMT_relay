# IDMT Protection Relay - LCD+Keypad Version
## Standalone Arduino Mega 2560 with I²C LCD 20×4 & 4×3 Matrix Keypad

### Overview
This is a standalone IDMT (Inverse Definite Minimum Time) protection relay implementation for Arduino Mega 2560. All configuration is done via a 20×4 I²C LCD display and 4×3 matrix keypad—no PC connection needed.

---

## Hardware Components

| Component | Qty | Details |
|-----------|-----|---------|
| Arduino Mega 2560 | 1 | Main microcontroller |
| I²C LCD 20×4 | 1 | Display module (typical address: 0x27 or 0x3F) |
| 4×3 Matrix Keypad | 1 | 12-button input device |
| Current Transformer (CT) | 1 | Secondary rated for your application |
| Signal Conditioning Circuit | 1 | To bias CT signal to mid-rail (ADC 512 at zero current) |
| Relay Driver / Opto-coupler | 1 | To drive protection relay from pin 7 |
| Status LEDs | 3 | Green, Yellow, Red (with current-limiting resistors) |
| Push-button Reset (optional) | 1 | For manual reset |

---

## Pin Configuration

### Arduino Mega 2560

#### I²C Interface (I²C LCD)
- **SDA**: Pin 20 (connect to LCD SDA)
- **SCL**: Pin 21 (connect to LCD SCL)

#### Matrix Keypad
- **Row Pins**: 50, 51, 52, 53 (R1, R2, R3, R4)
- **Column Pins**: 48, 49, 47 (C1, C2, C3)
// 49, 50, 48, 53, 47, 52, 51

#### Analog Input
- **A0**: CT secondary signal (input to ADC)

#### Digital Outputs
- **Pin 7**: TRIP output (active LOW) → relay driver / opto-coupler
- **Pin 8**: GREEN LED (active HIGH)
- **Pin 9**: YELLOW LED (active HIGH)
- **Pin 10**: RED LED (active HIGH)

---

## Keypad Mapping

### Physical Layout (Standard 4×3 Keypad)
```
Col1  Col2  Col3
┌─────┬─────┬─────┐
│ 1   │ 2   │ 3   │  Row1
├─────┼─────┼─────┤
│ 4   │ 5   │ 6   │  Row2
├─────┼─────┼─────┤
│ 7   │ 8   │ 9   │  Row3
├─────┼─────┼─────┤
│  *  │ 0   │  #  │  Row4
└─────┴─────┴─────┘
```

### Menu Key Functions
| Key | Function | Description |
|-----|----------|-------------|
| **1** | Standard | Select IEC or IEEE |
| **2** | Curve | Select protection curve |
| **3** | TMS | Enter time dial (0.01-10) |
| **4** | Pickup | Enter pickup current (A) |
| **5** | Inst M | Enter instantaneous multiple (≥1.01) |
| **\*** | Reset/Accept | Start protection OR reset latch OR confirm entry |
| **0** | Status | View relay status during protection |
| **.** | Decimal | For numeric input |
| **#** | Unused | Not programmed |
| **6-9** | Unused | Not programmed (for future expansion) |

---

## Menu Structure

### Main Menu
Press any key to access menu functions:
- **1**: Select Standard (IEC/IEEE)
- **2**: Select Curve
- **3**: Edit TMS
- **4**: Edit Pickup Current
- **5**: Edit Instantaneous Multiple
- **\***: Reset Latch
- **0**: View Status

### Standard Selection
- **1**: IEC standard
- **2**: IEEE standard

### Curve Selection (IEC)
- **1**: SI curve
- **2**: VI curve
- **3**: EI curve
- **4**: LTI curve

### Curve Selection (IEEE)
- **1**: MOD INV
- **2**: VERY INV
- **3**: EXT INV

### Numeric Entry (TMS, Pickup, Inst M)
- Use keys: **1-9** and **0** for digits
- Use **.** for decimal point
- **\*** (press **\***): Confirm entry and return to main menu

### Reset Latch
- **1**: YES - reset the latch, relay can trip again
- **2**: NO - keep latch active

### Status View
- Shows latch status, standard, and TMS value
- Press **0** to return to main menu

---

## Protection Modes

### 1. Configuration Mode
- Default on startup
- Use keypad to set protection parameters:
  - **Standard**: IEC 60255 or IEEE C37.112
  - **Curve**: SI, VI, EI, LTI (IEC) or MOD INV, VERY INV, EXT INV (IEEE)
  - **TMS**: Time dial (0.01 - 10.0)
  - **Pickup**: Threshold current in amps (>0)
  - **Inst M**: Instantaneous multiple (≥1.01)

### 2. Protection Active Mode
- Press **\* (J)** from main menu to START protection
- LCD shows real-time current (I) and multiplier (M)
- Three LED states:
  - **GREEN**: Normal operation (I < pickup)
  - **YELLOW**: Fault pending (I ≥ pickup, waiting for trip time)
  - **RED**: Tripped (relay open)

### 3. Tripped/Latched State
- Relay remains open (trip pin = LOW)
- Press **H (3)** to access reset menu
- LED remains RED until latch is reset

---

## Wiring Example

### I²C LCD Connection (0x27)
```
LCD Pin → Arduino Pin
VCC → 5V
GND → GND
SDA → 20
SCL → 21
```

### Matrix Keypad Connection
```
Row pins:  50, 51, 52, 53
Col pins:  48, 49, 47

Standard 4×3 keypad layout:
{1, 2, 3}
{4, 5, 6}
{7, 8, 9}
{*, 0, #}
```

### CT Secondary + Signal Conditioning
```
CT Secondary → Signal Conditioning Circuit → A0
Bias circuit ensures:
  - 0 current → ADC ~512 (2.5V)
  - Peak AC signal swings around midpoint
  - Keep Vrms < Vref/2 for headroom
```

### Trip Output
```
Pin 7 (active LOW) → Relay Driver / Opto-coupler input
When pin 7 = LOW, relay driver triggers protection relay.
```

### Status LEDs (with 330Ω resistor)
```
Pin 8 (GREEN) → Resistor → LED → GND
Pin 9 (YELLOW) → Resistor → LED → GND
Pin 10 (RED) → Resistor → LED → GND
```

---

## Calibration

### Sensitivity Adjustment
Edit in `LCD+Keypad.ino`:
```cpp
float sensitivity = 0.85;      // Adjust after calibration
```

To calibrate:
1. Apply known fault current (via auxiliary CT)
2. Monitor the measured current on LCD
3. Adjust sensitivity factor until displayed value matches reality

### ADC Resolution
- Arduino Mega 2560 has **10-bit ADC** (0-1023)
- Default Vref = 5.0V
- LSB = 4.88 mV

---

## Software Requirements

### Required Arduino Libraries
- **LiquidCrystal_I2C** (for I²C LCD)
- **Keypad** (by Mark Stanley & Alexander Brevig)

### Installation
In Arduino IDE:
1. Sketch → Include Library → Manage Libraries
2. Search for "LiquidCrystal_I2C" → Install
3. Search for "Keypad" → Install

---

## Calibration & Testing

### Initial Setup
1. Upload sketch to Arduino Mega 2560
2. Power on—LCD shows "IDMT Relay Init..."
3. Configure protection settings via keypad (use keys 1-5)
4. Press **\*** (asterisk) to START protection

### Testing Trip Function
1. Apply gradually increasing fault current
2. Observe LCD for I and M values
3. Verify relay trips when M ≥ Inst M (immediate)
4. Or when trip_time accumulates to desired value (timed trip)

### Manual Reset
1. Press **\*** during protection to access reset menu
2. Press **1** for YES to reset latch
3. Relay can now trip again

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| LCD not showing | Check I²C address (0x27 or 0x3F) in code |
| Keypad not responding | Verify row/column pin assignments |
| Wrong current reading | Calibrate sensitivity factor |
| Relay won't reset | Press **\*** to access reset menu, select **1** for YES |
| LEDs always off | Check pin assignments and current-limiting resistors |

---

## File Structure

```
arduino_mega/LCD+Keypad/
├── LCD+Keypad.ino       (Main sketch)
├── display.h             (Menu class definition)
├── display.cpp           (Menu class implementation)
├── relay_curves.h        (Protection curves header)
├── relay_curves.cpp      (Protection curves implementation)
└── README.md            (This file)
```

---

## Key Features

✅ **Standalone Operation** - No PC needed after configuration  
✅ **I²C LCD Display** - Easy-to-read 20×4 LCD menu  
✅ **Matrix Keypad Input** - Intuitive number pad for settings  
✅ **IEC & IEEE Standards** - Support for both protection standards  
✅ **Real-time Monitoring** - Current and multiplier display  
✅ **Latch Function** - Manual reset after trip  
✅ **Visual Feedback** - RGB LED status indicators  
✅ **Modular Code** - Easy to extend and modify  

---

## License
This project is part of the IDMT Protection Relay suite. Use and modify as needed.

---

**Version**: 1.0  
**Last Updated**: May 2026
