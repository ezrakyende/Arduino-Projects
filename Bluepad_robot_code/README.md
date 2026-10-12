# Bluepad Robot — Bluetooth Gamepad-Controlled Car

An **ESP32**-powered robot car driven wirelessly from a **Bluetooth gamepad** (PS4, PS5, Xbox, Switch Pro, 8BitDo, etc.) using the **Bluepad32** library.

Includes an **OLED heads-up display**, **turret servo control**, **external warning LED**, and four built-in **"celebration" dance routines** triggered from the controller buttons.

---

## What It Does

| Feature | Description |
|---|---|
| 🎮 **Wireless Gamepad Control** | Any Bluetooth Classic/BLE gamepad paired via Bluepad32 |
| 🚗 **Differential Drive** | Left joystick = throttle · Right joystick = steering |
| 🔋 **Battery Telemetry** | Shows the gamepad battery level on the OLED |
| 🎯 **Turret Servo** | SG90 servo aimed with R1 / L1 shoulder buttons |
| 🕺 **Celebration Modes** | Four pre-programmed dance routines (one per face button) |
| 🚨 **Warning System** | External red LED + OLED banner for faults (no pad, low battery, slow loop) |
| 📺 **Live Diagnostics** | CPU frequency, loop time, motor outputs, current state |

---

## Hardware

| Component | Purpose | Pin (ESP32) |
|---|---|---|
| L298N / TB6612 motor driver | Drives two DC gear motors | see below |
| **enA** | Left motor speed (PWM) | GPIO 5 |
| **enB** | Right motor speed (PWM) | GPIO 4 |
| **up** | Left motor direction A | GPIO 22 |
| **down** | Left motor direction B | GPIO 21 |
| **left** | Right motor direction A | GPIO 19 |
| **right** | Right motor direction B | GPIO 18 |
| SG90 servo | Turret / shield | GPIO 25 |
| Red LED | Warning indicator | GPIO 26 |
| SSD1306 OLED 128×64 | Status display | SDA GPIO 13, SCL GPIO 15 |

> ⚠️ **GPIO 21 and 22 are used for the motors**, so the OLED is moved to **GPIO 13 (SDA)** and **GPIO 15 (SCL)**. This is why `Wire.begin(OLED_SDA, OLED_SCL)` is called with custom pins instead of the default ESP32 I²C pins.

### Power notes

- The **motor driver** should have its own battery pack — **do not** power motors from the ESP32's 3.3 V/5 V rail.
- The **SG90 servo** draws up to ~700 mA when moving — feed it from a 5 V regulator, share GND with the ESP32.
- ESP32 and OLED run at **3.3 V logic**. Add a level shifter if you use a 5 V-only OLED module.

---

## Controls

| Input | Action |
|---|---|
| **Left joystick Y** | Throttle (forward / reverse) |
| **Right joystick X** | Steering (left / right) |
| **R1** | Sweep turret servo right (in 3° steps) |
| **L1** | Sweep turret servo left (in 3° steps) |
| **△ Triangle** | Celebration 1 — fast alternating spins |
| **✕ Cross** | Celebration 2 — aggressive shuffle forward/back |
| **□ Square** | Celebration 3 — side-to-side sway dance |
| **○ Circle** | Celebration 4 — accelerating cyclone spin |
| **R2** | Emergency stop / cancel celebration |

A **30-unit dead zone** on both sticks prevents drift when the pad is idle.

---

## Celebrations Explained

Each face button launches a **5-second non-blocking dance** — you can still hit **R2** to abort immediately. During a celebration, normal driving input is suspended.

| Button | Name | Motion |
|---|---|---|
| △ | Spinner | Full-speed left-right spin flip every 250 ms |
| ✕ | Shuffle | Both motors reverse direction every 300 ms |
| □ | Sway | Asymmetric speeds create a side-to-side wiggle |
| ○ | Cyclone | Rotation speed ramps up cyclically every 1 s |

These are all handled by `runCelebration()` using `millis()` — no `delay()` anywhere, so the loop never blocks.

---

## OLED Display Layout
--- SYS SYSTEM ---
PAD: Con Bat: Full
CPU:240MHz Lp:11ms
L-In: 180 R-In: -60
STATE: DRIVING

| Line | Content |
|---|---|
| 0 | Header bar |
| 1 | Controller connection + battery level (Empty / Low / Med / Full) |
| 2 | ESP32 CPU frequency and current main-loop duration in ms |
| 3 | Left and right motor outputs (-255 to +255) |
| 4 | Current state: **IDLE**, **DRIVING**, **CELEBRATING**, or inverted **WARN: …** banner |

Screen refreshes at **5 Hz** to save CPU time.

---

## Warning System

The external red LED blinks at **2 Hz** whenever:

| Condition | Message |
|---|---|
| No controller paired | `NO CONTROLLER` |
| Gamepad battery empty or low | `LOW BATTERY` |
| Main loop takes longer than 40 ms | `CPU SLOWDOWN` |

When a warning is active, the OLED draws an **inverted black-on-white banner** in the bottom row so it's impossible to miss.

---

## How to Build & Upload

### 1. Arduino IDE setup

1. **File → Preferences → Additional Boards Manager URLs** — add: 
| Line | Content |
|---|---|
| 0 | Header bar |
| 1 | Controller connection + battery level (Empty / Low / Med / Full) |
| 2 | ESP32 CPU frequency and current main-loop duration in ms |
| 3 | Left and right motor outputs (-255 to +255) |
| 4 | Current state: **IDLE**, **DRIVING**, **CELEBRATING**, or inverted **WARN: …** banner |

Screen refreshes at **5 Hz** to save CPU time.

---

## Warning System

The external red LED blinks at **2 Hz** whenever:

| Condition | Message |
|---|---|
| No controller paired | `NO CONTROLLER` |
| Gamepad battery empty or low | `LOW BATTERY` |
| Main loop takes longer than 40 ms | `CPU SLOWDOWN` |

When a warning is active, the OLED draws an **inverted black-on-white banner** in the bottom row so it's impossible to miss.

---

## How to Build & Upload

### 1. Arduino IDE setup

1. **File → Preferences → Additional Boards Manager URLs** — add: 
| Line | Content |
|---|---|
| 0 | Header bar |
| 1 | Controller connection + battery level (Empty / Low / Med / Full) |
| 2 | ESP32 CPU frequency and current main-loop duration in ms |
| 3 | Left and right motor outputs (-255 to +255) |
| 4 | Current state: **IDLE**, **DRIVING**, **CELEBRATING**, or inverted **WARN: …** banner |

Screen refreshes at **5 Hz** to save CPU time.

---

## Warning System

The external red LED blinks at **2 Hz** whenever:

| Condition | Message |
|---|---|
| No controller paired | `NO CONTROLLER` |
| Gamepad battery empty or low | `LOW BATTERY` |
| Main loop takes longer than 40 ms | `CPU SLOWDOWN` |

When a warning is active, the OLED draws an **inverted black-on-white banner** in the bottom row so it's impossible to miss.

---

## How to Build & Upload

### 1. Arduino IDE setup

1. **File → Preferences → Additional Boards Manager URLs** — add: 
| Line | Content |
|---|---|
| 0 | Header bar |
| 1 | Controller connection + battery level (Empty / Low / Med / Full) |
| 2 | ESP32 CPU frequency and current main-loop duration in ms |
| 3 | Left and right motor outputs (-255 to +255) |
| 4 | Current state: **IDLE**, **DRIVING**, **CELEBRATING**, or inverted **WARN: …** banner |

Screen refreshes at **5 Hz** to save CPU time.

---

## Warning System

The external red LED blinks at **2 Hz** whenever:

| Condition | Message |
|---|---|
| No controller paired | `NO CONTROLLER` |
| Gamepad battery empty or low | `LOW BATTERY` |
| Main loop takes longer than 40 ms | `CPU SLOWDOWN` |

When a warning is active, the OLED draws an **inverted black-on-white banner** in the bottom row so it's impossible to miss.

---

## How to Build & Upload

### 1. Arduino IDE setup

1. **File → Preferences → Additional Boards Manager URLs** — add:
    
| Line | Content |
|---|---|
| 0 | Header bar |
| 1 | Controller connection + battery level (Empty / Low / Med / Full) |
| 2 | ESP32 CPU frequency and current main-loop duration in ms |
| 3 | Left and right motor outputs (-255 to +255) |
| 4 | Current state: **IDLE**, **DRIVING**, **CELEBRATING**, or inverted **WARN: …** banner |

Screen refreshes at **5 Hz** to save CPU time.

---

## Warning System

The external red LED blinks at **2 Hz** whenever:

| Condition | Message |
|---|---|
| No controller paired | `NO CONTROLLER` |
| Gamepad battery empty or low | `LOW BATTERY` |
| Main loop takes longer than 40 ms | `CPU SLOWDOWN` |

When a warning is active, the OLED draws an **inverted black-on-white banner** in the bottom row so it's impossible to miss.

---

## How to Build & Upload

### 1. Arduino IDE setup

1. **File → Preferences → Additional Boards Manager URLs** — add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
2. **Boards Manager** → install **esp32** by Espressif
3. **Tools → Board → ESP32 Arduino → ESP32 Dev Module** (or your specific board)
4. **Library Manager** → install:
- **Bluepad32 for Arduino** (search "Bluepad32")
- **Adafruit SSD1306**
- **Adafruit GFX Library**

### 2. Bluepad32 note

Bluepad32 requires **custom ESP32 firmware** for full compatibility with all controllers (especially PS4/PS5/PS5 DualSense, Switch Pro). Install it by following the official guide:
**https://github.com/ricardoquesada/bluepad32/blob/main/docs/plat_arduino.md**

At minimum, the library itself + standard ESP32 board support will work for most Xbox and 8BitDo pads.

### 3. Upload

Click **Upload** in the Arduino IDE. Open Serial Monitor at **115200 baud** to see: 
Firmware: v4.x.x
Controller Connected

---

## Pairing a Gamepad

1. Power on the robot — the ESP32 starts advertising as a Bluetooth host.
2. Put your gamepad into **pairing mode**:
   - **PS4/PS5**: hold **SHARE + PS** until the light bar flashes rapidly
   - **Xbox One/Series**: hold the **pair button** until the Xbox logo flashes
   - **Switch Pro**: hold the **sync button** on top
   - **8BitDo**: hold **START + a face button** (varies by model)
3. On the OLED you'll see `PAD: Con  Bat: <level>`.

If it doesn't pair on the first try, unplug/replug the ESP32 and try again with the gamepad already in pairing mode.

---

## Diagnostics

The main loop measures its own execution time and warns if it exceeds 40 ms:

loopTime = millis() - loopStart;
Watch the OLED line 2 — Lp:11ms means the loop is running comfortably fast. If it climbs above 40 ms you'll get a CPU SLOWDOWN warning, which usually means:

The Bluepad32 stack is stalling

Something is blocking (a delay(), a serial print flood)

The CPU frequency is too low (Tools → CPU Frequency)

Motor Mixing
Standard differential drive math: leftMotor  = speed + turn;
rightMotor = speed - turn;
This means:

Full forward, no turn → both motors +255

Spin in place left → left -255, right +255

Forward-right arc → both forward, right slightly slower

Both outputs are constrain()ed to ±255 before being sent to ledcWrite.

Known Quirks
Left joystick Y is inverted (-ctl->axisY()), because the raw axis value is negative when pushed forward on most pads. Remove the minus if your pad behaves the opposite way.

The OLED uses GPIO 13/15 instead of the default 21/22 — the default pins are taken by the motor driver.

Servo PWM uses channel 2 at 50 Hz / 12-bit; motor PWM uses channels 0 and 1 at 5 kHz / 8-bit. Don't mix them up or the servo will jitter.

License
MIT — free to use, modify and share.

---

## Step 4 — Commit

Scroll down → **Commit changes...** → leave on "Commit directly to the `main` branch" → **Commit changes**.

---

## Quick note on the folder name

Looking back at your earlier GitHub screenshot, I saw these folders in your repo:
- `Bluepad_robot_code`
- `Robot_code`
- `wavingrobot_sketch`
- `radar_2`
- ...

This sketch is clearly the **Bluepad one** (Bluepad32 + OLED + servo + gamepad). So the README title should go in **`Bluepad_robot_code/`** — that's what I've written above.

