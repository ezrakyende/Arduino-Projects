# TRONIC ZENERGY — Survival Watch

A DIY wearable survival and safety watch built on the **Waveshare RP2040-Zero**.
It watches your body and the air around you, and alerts you when it matters.

---

## What It Does

Six real tools plus a safety net, all on a board the size of your thumb:

| Feature | What it gives you |
|---|---|
| 🆘 **Fall Alarm** | Detects a hard impact + stillness, gives a 10-second cancel countdown, then strobes a red/white alarm |
| ⛰️ **Altimeter** | Live barometric altitude in metres and feet, relative height, total climb/descent |
| ⛈️ **Storm Watch** | 3-hour pressure trend → storm risk %, orange warning light hours in advance |
| 🌡️ **Thermometer** | Live °C and °F with min/max memory and a plain-English "feel" label |
| 👟 **Pedometer** | Step count, distance, 10 000-step goal with progress bar |
| 🎯 **Motion Meter** | Total acceleration in g and m/s², pitch, roll, spin rate in deg/s |

The **fall alarm works on every screen**, 100 times per second — even if you're looking at the temperature page.

---

## Hardware

| Part | Purpose | Pins |
|---|---|---|
| SSD1306 OLED 128×64 | Main display | SDA GP4, SCL GP5 |
| MPU6050 | Accelerometer + gyroscope (steps, motion, fall detection) | I²C |
| BMP180 **or** BMP280/BME280 | Pressure + temperature (auto-detected) | I²C |
| 3-way DIP switch | Selects the page to show | GP12, GP13, GP14 |
| Push button | Context action (zero, reset, cancel alarm, test) | GP15 |
| Onboard NeoPixel RGB | Alarm / status light | GP16 |

---

## Modes (DIP switch: SW3 SW2 SW1)

| Code | Page |
|---|---|
| 000 | Welcome animation |
| 001 | Altitude and climb |
| 010 | Pressure and storm watch |
| 011 | Temperature |
| 100 | Steps and distance |
| 101 | Motion (acceleration, tilt, spin) |
| 110 | Fall guard status |
| 111 | Summary of everything |

---

## Button Actions

| Page | Action |
|---|---|
| Fall alarm active | **Press** = cancel the alarm |
| Altitude | **Press** = zero relative height · **Hold** = reset climb |
| Pressure | **Hold** = storm demo mode on/off |
| Steps | **Hold** = reset step counter |
| Fall guard | **Hold** = test the fall alarm |

---

## How the Fall Alarm Works

A state machine watches the accelerometer and gyroscope 100× per second:

ARMED → IMPACT → PRE-ALARM → ALARM
↑
(cancel with button or movement)



1. **ARMED** — waiting for a hard hit (over 2.5 g)
2. **IMPACT** — hit detected, ignore the next 600 ms of bouncing
3. Check if you went **still** for 2 seconds (low acceleration + low rotation)
4. **PRE-ALARM** — 10-second countdown with fast orange LED blink; if you move or press the button, it cancels
5. **ALARM** — red/white strobe light, only the button cancels it

If you're fine, you won't even notice — the alarm cancels silently on movement.

---

## How Storm Watch Works

The BMP sensor records pressure every 5 minutes into a rolling 3-hour buffer.
The firmware computes the **hPa/hour trend** and converts it to a **storm risk %**.

Falling pressure = storm coming. The system gives **hours of warning** before the sky turns black.

If you've been walking, the trend is invalidated (motion corrupts barometric readings) and the display says *"Trend: moving"*.

---

## Build & Upload

### Arduino IDE 2.x setup

1. **File → Preferences → Additional Boards Manager URLs** — paste:

   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   2. **Boards Manager** → install **Raspberry Pi Pico/RP2040** by **Earle Philhower**
3. **Tools → Board → Raspberry Pi RP2040 Boards → Waveshare RP2040 Zero**
4. **Tools → Flash Size → 2MB (no FS)**
5. **Library Manager** → install:
- Adafruit SSD1306
- Adafruit GFX Library
- Adafruit NeoPixel
6. Open `TRONIC_Survival_Watch.ino` and click **Upload**

### First-time upload trick
Hold **BOOT** on the RP2040-Zero → press and release **RESET** → release **BOOT**.
An `RPI-RP2` drive appears, then click Upload. After the first flash, normal uploads work directly.

---

## Wiring
RP2040-Zero SSD1306 OLED / MPU6050 / BMPxxx
GP4 ─────────────► SDA
GP5 ─────────────► SCL
3V3 ─────────────► VCC
GND ─────────────► GND

DIP switch (other side to GND):
GP12 ◄─── SW1
GP13 ◄─── SW2
GP14 ◄─── SW3

Push button (other side to GND):
GP15 ◄─── BTN

RGB LED: GP16 (already on the board)

text

---

## Serial Debug

At 115200 baud, the sketch prints one line per second:
mode=1 alt=123.4m p=1013.2hPa t=24.5C steps=4210 g=1.02 fall=0 risk=12%

Useful for tuning step sensitivity, fall thresholds, and storm trend parameters.

---

## Tuning Constants

All the important numbers are at the top of the `.ino`:

| Constant | Default | Meaning |
|---|---|---|
| `SEA_LEVEL_HPA` | 1013.25 | Set to today's local sea-level pressure for true altitude |
| `STRIDE_M` | 0.70 | Your step length in metres |
| `STEP_GOAL` | 10000 | Daily step goal |
| `STEP_THRESH_G` | 0.12 | Step sensitivity (lower = more sensitive) |
| `IMPACT_G` | 2.5 | Fall impact threshold in g |
| `PRE_ALARM_MS` | 10000 | Fall cancel countdown before real alarm |

---

## License

MIT — free to use, modify and share.
