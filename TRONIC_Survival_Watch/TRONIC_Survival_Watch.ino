/*
  ============================================================
   TRONIC ZENERGY  -  SURVIVAL WATCH            (RP2040-Zero)
  ============================================================
   Board in Arduino IDE : Raspberry Pi Pico/RP2040 core by Earle Philhower
                          -> choose "Waveshare RP2040 Zero"
   Libraries (Library Manager):
        Adafruit SSD1306
        Adafruit GFX Library
        Adafruit NeoPixel
   (MPU6050 and BMP180/BMP280 are driven directly - no extra library.)

   WIRING
        OLED / MPU6050 / pressure sensor : SDA -> GP4 , SCL -> GP5
        3-switch DIP (mode select)       : SW1 -> GP12, SW2 -> GP13, SW3 -> GP14 (other side GND)
        Push button                      : GP15 -> GND
        Onboard RGB LED (alarm)          : GP16 (already on the board)

   MODE SELECT  (switch ON = 1,  code = SW3 SW2 SW1)
        000  Welcome animation
        001  Altitude and climb
        010  Pressure and storm watch
        011  Temperature
        100  Steps and distance
        101  Motion (acceleration, tilt, spin)
        110  Fall guard status
        111  Summary of everything

   BUTTON
        Fall alarm active ....... press = cancel the alarm
        Altitude page ........... press = zero the relative height, hold = reset climb
        Pressure page ........... hold  = storm DEMO on/off (for filming)
        Steps page .............. hold  = reset steps
        Fall guard page ......... hold  = test the fall alarm

   The fall alarm works on EVERY page, even if the screen shows something else.
  ============================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ============================================================
//  SETTINGS YOU CAN CHANGE
// ============================================================
const char*    NAME_LINE1     = "TRONIC";          // welcome page, big line 1 (max 10 letters)
const char*    NAME_LINE2     = "ZENERGY";         // welcome page, big line 2 (max 10 letters)
const char*    PROJECT_NAME   = "SURVIVAL WATCH";  // welcome page, small line (max 21 letters)

const float    SEA_LEVEL_HPA  = 1013.25f;   // set to today's local sea-level pressure for true altitude
const float    STRIDE_M       = 0.70f;      // your step length in metres
const uint32_t STEP_GOAL      = 10000;      // daily step goal

// Step detection
const float    STEP_THRESH_G  = 0.12f;      // lower = more sensitive
const uint32_t STEP_MIN_MS    = 280;        // fastest possible step rate

// Fall detection
const float    IMPACT_G         = 2.5f;     // hit harder than this = possible fall
const uint32_t IMPACT_SETTLE_MS = 600;      // ignore the bouncing right after the hit
const uint32_t IMPACT_WINDOW_MS = 8000;     // how long we wait for "lying still"
const float    STILL_DEV_G      = 0.12f;    // how still counts as "still"
const float    STILL_GYRO_DPS   = 25.0f;    // rotation allowed while "still"
const uint32_t STILL_NEEDED_MS  = 2000;     // must stay still this long
const uint32_t PRE_ALARM_MS     = 10000;    // countdown before the real alarm
const bool     AUTO_CANCEL_ON_MOVE = true;  // if you get up during countdown, cancel

// Storm watch
const uint32_t TREND_SAMPLE_MS  = 300000UL; // store one pressure sample every 5 minutes
const bool     STORM_LED_ALERT  = true;     // orange pulse on the RGB LED when storm risk is high

// ============================================================
//  PINS
// ============================================================
const uint8_t PIN_SDA = 4;
const uint8_t PIN_SCL = 5;
const uint8_t PIN_SW1 = 12;
const uint8_t PIN_SW2 = 13;
const uint8_t PIN_SW3 = 14;
const uint8_t PIN_BTN = 15;
const uint8_t PIN_RGB = 16;

// ============================================================
//  OBJECTS
// ============================================================
Adafruit_SSD1306  oled(128, 64, &Wire, -1, 400000UL, 400000UL);
Adafruit_NeoPixel rgb(1, PIN_RGB, NEO_GRB + NEO_KHZ800);

#define W     SSD1306_WHITE
#define BLACK SSD1306_BLACK

// ============================================================
//  GLOBAL STATE
// ============================================================
uint8_t  mode        = 0;
uint32_t pageStart   = 0;
bool     oledOk      = false;
bool     haveMpu     = false;
bool     imuOk       = false;
bool     baroReady   = false;

// ---- IMU values ----
float axg = 0, ayg = 0, azg = 1;        // acceleration in g
float gxd = 0, gyd = 0, gzd = 0;        // rotation in deg/s
float aMag = 1.0f;                      // total acceleration in g
float gyroMag = 0;                      // total rotation deg/s
float axs = 0, ays = 0, azs = 1;        // smoothed acceleration (for tilt)

// ---- barometer values ----
enum BaroType { BARO_NONE, BARO_BMP180, BARO_BMP280 };
BaroType    baroType = BARO_NONE;
uint8_t     baroAddr = 0x77;
const char* baroName = "NONE";
float       tempC    = 25.0f;
float       pressPa  = 101325.0f;       // latest raw reading
float       pressF   = 101325.0f;       // smoothed reading
bool        baroNew  = false;
float       altM     = 0;
float       altRef   = 0;
bool        refSet   = false;
uint32_t    firstBaroMs = 0;
float       lastClimbAlt = 0;
float       totalClimb   = 0;
float       totalDescent = 0;
bool        tempInit = false;
float       tMin = 0, tMax = 0;

// ---- steps ----
uint32_t stepCount   = 0;
bool     walking     = false;
uint8_t  pendSteps   = 0;
uint32_t lastStepMs  = 0;
float    stepFilt    = 1.0f;
float    stepAvg     = 1.0f;
bool     stepAbove   = false;
uint32_t lastCandMs  = 0;

// ---- storm trend ----
const uint8_t TREND_N = 36;             // 36 x 5 min = 3 hours
float    hP[TREND_N];
uint32_t hT[TREND_N];
uint32_t hS[TREND_N];
uint8_t  hHead = 0, hCount = 0;
uint32_t lastHist = 0;
float    trendRate  = 0;                // hPa per hour
bool     trendValid = false;
bool     movingFlag = false;
float    stormRisk  = 0;                // percent
bool     stormDemo  = false;

// ---- fall guard ----
enum FallState { F_ARMED, F_IMPACT, F_PREALARM, F_ALARM, F_COOLDOWN };
FallState fallState = F_ARMED;
uint32_t  stateStart = 0;
uint32_t  stillStart = 0;
float     lastPeakG  = 0;
uint16_t  fallCount  = 0;

// ---- buttons / switches ----
bool     btnRaw = false, btnState = false, btnLongFired = false;
uint32_t btnChangeMs = 0, btnDownMs = 0;
bool     evDown = false, evShort = false, evLong = false;
bool     suppressUntilRelease = false;
uint8_t  dipRaw = 0, dipCount = 0;

// ---- led / display bookkeeping ----
uint32_t ledLast = 0xFFFFFFFFUL;
bool     invState = false;

// ---- timers ----
uint32_t tImu = 0, tBaro = 0, tBtn = 0, tDip = 0, tDisp = 0, tSerial = 0;

// ============================================================
//  SMALL HELPERS
// ============================================================
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

int bobY(uint32_t t, int amp) { return (int)(sinf(t / 360.0f) * amp); }

// number -> text (no float printf needed)
void fmtF(char* out, size_t n, float v, uint8_t dec) {
  if (!isfinite(v)) { snprintf(out, n, "--"); return; }
  bool neg = v < 0;
  if (neg) v = -v;
  uint32_t scale = 1;
  for (uint8_t i = 0; i < dec; i++) scale *= 10;
  uint32_t whole = (uint32_t)v;
  uint32_t frac  = (uint32_t)((v - (float)whole) * scale + 0.5f);
  if (frac >= scale) { whole++; frac -= scale; }
  if (whole == 0 && frac == 0) neg = false;
  if (dec == 0) snprintf(out, n, "%s%lu", neg ? "-" : "", (unsigned long)whole);
  else          snprintf(out, n, "%s%lu.%0*lu", neg ? "-" : "", (unsigned long)whole, (int)dec, (unsigned long)frac);
}

void fmtS(char* out, size_t n, float v, uint8_t dec) {   // with + sign
  char t[20];
  fmtF(t, sizeof(t), v, dec);
  if (t[0] != '-' && t[0] != '-' && v >= 0) snprintf(out, n, "+%s", t);
  else snprintf(out, n, "%s", t);
}

void printAt(int x, int y, uint8_t size, const char* s) {
  oled.setTextSize(size);
  oled.setCursor(x, y);
  oled.print(s);
}

void centerText(const char* s, int y, uint8_t size) {
  int w = (int)strlen(s) * 6 * size;
  int x = (128 - w) / 2;
  if (x < 0) x = 0;
  printAt(x, y, size, s);
}

// ============================================================
//  I2C HELPERS
// ============================================================
bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  size_t got = Wire.requestFrom(addr, (size_t)len);
  if (got != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

// ============================================================
//  MPU6050  (motion sensor)
// ============================================================
const uint8_t MPU_ADDR = 0x68;

bool mpuBegin() {
  if (!i2cPresent(MPU_ADDR)) return false;
  i2cWrite8(MPU_ADDR, 0x6B, 0x01);   // wake up, use gyro clock
  delay(50);
  i2cWrite8(MPU_ADDR, 0x1A, 0x03);   // low-pass filter about 44 Hz
  i2cWrite8(MPU_ADDR, 0x1B, 0x08);   // gyro  +-500 deg/s  (65.5 LSB per deg/s)
  i2cWrite8(MPU_ADDR, 0x1C, 0x10);   // accel +-8 g        (4096 LSB per g)
  return true;
}

bool mpuRead() {
  uint8_t b[14];
  if (!i2cReadBytes(MPU_ADDR, 0x3B, b, 14)) return false;
  int16_t ax = (int16_t)((b[0]  << 8) | b[1]);
  int16_t ay = (int16_t)((b[2]  << 8) | b[3]);
  int16_t az = (int16_t)((b[4]  << 8) | b[5]);
  int16_t gx = (int16_t)((b[8]  << 8) | b[9]);
  int16_t gy = (int16_t)((b[10] << 8) | b[11]);
  int16_t gz = (int16_t)((b[12] << 8) | b[13]);
  axg = ax / 4096.0f;  ayg = ay / 4096.0f;  azg = az / 4096.0f;
  gxd = gx / 65.5f;    gyd = gy / 65.5f;    gzd = gz / 65.5f;
  aMag    = sqrtf(axg * axg + ayg * ayg + azg * azg);
  gyroMag = sqrtf(gxd * gxd + gyd * gyd + gzd * gzd);
  axs += 0.10f * (axg - axs);
  ays += 0.10f * (ayg - ays);
  azs += 0.10f * (azg - azs);
  return true;
}

// ============================================================
//  PRESSURE SENSOR  (BMP180 or BMP280/BME280, auto-detected)
// ============================================================
// ---- BMP180 ----
int16_t  ac1, ac2, ac3, bb1, bb2, mb, mc, md;
uint16_t ac4, ac5, ac6;
const uint8_t BMP180_OSS = 2;                       // 0..3 (higher = less noise, slower)
const uint8_t BMP180_WAIT[4] = { 6, 9, 15, 27 };    // conversion time in ms
int32_t  bmpUT = 0, bmpUP = 0, bmpB5 = 0;
bool     haveUT = false;
uint32_t lastUTms = 0, bmpStageMs = 0;
uint8_t  bmpStage = 0;

bool bmp180Begin() {
  uint8_t d[22];
  if (!i2cReadBytes(baroAddr, 0xAA, d, 22)) return false;
  ac1 = (int16_t)((d[0]  << 8) | d[1]);
  ac2 = (int16_t)((d[2]  << 8) | d[3]);
  ac3 = (int16_t)((d[4]  << 8) | d[5]);
  ac4 = (uint16_t)((d[6] << 8) | d[7]);
  ac5 = (uint16_t)((d[8] << 8) | d[9]);
  ac6 = (uint16_t)((d[10] << 8) | d[11]);
  bb1 = (int16_t)((d[12] << 8) | d[13]);
  bb2 = (int16_t)((d[14] << 8) | d[15]);
  mb  = (int16_t)((d[16] << 8) | d[17]);
  mc  = (int16_t)((d[18] << 8) | d[19]);
  md  = (int16_t)((d[20] << 8) | d[21]);
  return true;
}

void bmp180ComputeTemp() {
  int32_t x1 = (((int32_t)bmpUT - (int32_t)ac6) * (int32_t)ac5) >> 15;
  int32_t x2 = ((int32_t)mc * 2048) / (x1 + (int32_t)md);
  bmpB5 = x1 + x2;
  tempC = ((bmpB5 + 8) >> 4) / 10.0f;
}

void bmp180ComputePressure() {
  int32_t b6 = bmpB5 - 4000;
  int32_t x1 = ((int32_t)bb2 * ((b6 * b6) >> 12)) >> 11;
  int32_t x2 = ((int32_t)ac2 * b6) >> 11;
  int32_t x3 = x1 + x2;
  int32_t b3 = ((((int32_t)ac1 * 4 + x3) << BMP180_OSS) + 2) >> 2;
  x1 = ((int32_t)ac3 * b6) >> 13;
  x2 = ((int32_t)bb1 * ((b6 * b6) >> 12)) >> 16;
  x3 = ((x1 + x2) + 2) >> 2;
  uint32_t b4 = ((uint32_t)ac4 * (uint32_t)(x3 + 32768)) >> 15;
  uint32_t b7 = ((uint32_t)bmpUP - (uint32_t)b3) * (uint32_t)(50000 >> BMP180_OSS);
  int32_t p;
  if (b7 < 0x80000000UL) p = (int32_t)((b7 * 2) / b4);
  else                   p = (int32_t)((b7 / b4) * 2);
  x1 = (p >> 8) * (p >> 8);
  x1 = (x1 * 3038) >> 16;
  x2 = (-7357 * p) >> 16;
  p = p + ((x1 + x2 + 3791) >> 4);
  pressPa = (float)p;
}

void bmp180Task(uint32_t now) {
  uint8_t d[3];
  switch (bmpStage) {
    case 0:
      if (!haveUT || (now - lastUTms) > 3000UL) {
        i2cWrite8(baroAddr, 0xF4, 0x2E);                       // start temperature
        bmpStage = 1;
      } else {
        i2cWrite8(baroAddr, 0xF4, 0x34 + (BMP180_OSS << 6));   // start pressure
        bmpStage = 2;
      }
      bmpStageMs = now;
      break;
    case 1:
      if ((now - bmpStageMs) >= 6) {
        if (i2cReadBytes(baroAddr, 0xF6, d, 2)) {
          bmpUT = ((int32_t)d[0] << 8) | d[1];
          bmp180ComputeTemp();
          haveUT = true;
          lastUTms = now;
        }
        bmpStage = 0;
      }
      break;
    case 2:
      if ((now - bmpStageMs) >= BMP180_WAIT[BMP180_OSS]) {
        if (i2cReadBytes(baroAddr, 0xF6, d, 3)) {
          bmpUP = (((int32_t)d[0] << 16) | ((int32_t)d[1] << 8) | d[2]) >> (8 - BMP180_OSS);
          if (haveUT) { bmp180ComputePressure(); baroNew = true; }
        }
        bmpStage = 0;
      }
      break;
  }
}

// ---- BMP280 / BME280 ----
uint16_t dT1;  int16_t dT2, dT3;
uint16_t dP1;  int16_t dP2, dP3, dP4, dP5, dP6, dP7, dP8, dP9;
int32_t  tFine = 0;
uint32_t lastBaroRead = 0;

bool bmp280Begin() {
  uint8_t d[24];
  if (!i2cReadBytes(baroAddr, 0x88, d, 24)) return false;
  dT1 = (uint16_t)(d[0] | (d[1] << 8));
  dT2 = (int16_t)(d[2] | (d[3] << 8));
  dT3 = (int16_t)(d[4] | (d[5] << 8));
  dP1 = (uint16_t)(d[6] | (d[7] << 8));
  dP2 = (int16_t)(d[8] | (d[9] << 8));
  dP3 = (int16_t)(d[10] | (d[11] << 8));
  dP4 = (int16_t)(d[12] | (d[13] << 8));
  dP5 = (int16_t)(d[14] | (d[15] << 8));
  dP6 = (int16_t)(d[16] | (d[17] << 8));
  dP7 = (int16_t)(d[18] | (d[19] << 8));
  dP8 = (int16_t)(d[20] | (d[21] << 8));
  dP9 = (int16_t)(d[22] | (d[23] << 8));
  i2cWrite8(baroAddr, 0xE0, 0xB6);    // reset
  delay(10);
  i2cWrite8(baroAddr, 0xF5, 0x48);    // standby 125 ms, filter 4
  i2cWrite8(baroAddr, 0xF4, 0x53);    // temp x2, pressure x8, normal mode
  return true;
}

bool bmp280Read() {
  uint8_t d[6];
  if (!i2cReadBytes(baroAddr, 0xF7, d, 6)) return false;
  int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  if (adcP == 0x80000 || adcT == 0x80000) return false;

  int32_t v1 = ((((adcT >> 3) - ((int32_t)dT1 << 1))) * ((int32_t)dT2)) >> 11;
  int32_t v2 = (((((adcT >> 4) - ((int32_t)dT1)) * ((adcT >> 4) - ((int32_t)dT1))) >> 12) * ((int32_t)dT3)) >> 14;
  tFine = v1 + v2;
  tempC = ((tFine * 5 + 128) >> 8) / 100.0f;

  int64_t p1 = ((int64_t)tFine) - 128000;
  int64_t p2 = p1 * p1 * (int64_t)dP6;
  p2 = p2 + ((p1 * (int64_t)dP5) << 17);
  p2 = p2 + (((int64_t)dP4) << 35);
  p1 = ((p1 * p1 * (int64_t)dP3) >> 8) + ((p1 * (int64_t)dP2) << 12);
  p1 = (((((int64_t)1) << 47) + p1)) * ((int64_t)dP1) >> 33;
  if (p1 == 0) return false;
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - p2) * 3125) / p1;
  p1 = (((int64_t)dP9) * (p >> 13) * (p >> 13)) >> 25;
  p2 = (((int64_t)dP8) * p) >> 19;
  p = ((p + p1 + p2) >> 8) + (((int64_t)dP7) << 4);
  pressPa = (float)p / 256.0f;
  return true;
}

// ---- detect which chip is fitted ----
void baroBegin() {
  const uint8_t addrs[2] = { 0x77, 0x76 };
  for (uint8_t i = 0; i < 2; i++) {
    if (!i2cPresent(addrs[i])) continue;
    uint8_t id = 0;
    if (!i2cReadBytes(addrs[i], 0xD0, &id, 1)) continue;
    baroAddr = addrs[i];
    if (id == 0x55) {
      if (bmp180Begin()) { baroType = BARO_BMP180; baroName = "BMP180"; return; }
    } else if (id == 0x58 || id == 0x60) {
      if (bmp280Begin()) { baroType = BARO_BMP280; baroName = (id == 0x58) ? "BMP280" : "BME280"; return; }
    }
  }
}

void baroTask(uint32_t now) {
  if (baroType == BARO_BMP180) {
    bmp180Task(now);
  } else if (baroType == BARO_BMP280) {
    if ((now - lastBaroRead) >= 100) {
      lastBaroRead = now;
      if (bmp280Read()) baroNew = true;
    }
  }
}

// called every time a fresh pressure value arrives
void onNewPressure(uint32_t now) {
  if (!baroReady) {
    pressF = pressPa;
    baroReady = true;
    firstBaroMs = now;
  } else {
    pressF += 0.12f * (pressPa - pressF);
  }
  altM = 44330.0f * (1.0f - powf(pressF / (SEA_LEVEL_HPA * 100.0f), 0.190295f));

  if (!refSet && (now - firstBaroMs) > 3000UL) {   // after 3 s: this spot = relative zero
    altRef = altM;
    lastClimbAlt = altM;
    refSet = true;
  }
  if (refSet) {
    float d = altM - lastClimbAlt;
    if (d > 1.0f)       { totalClimb   += d;  lastClimbAlt = altM; }
    else if (d < -1.0f) { totalDescent += -d; lastClimbAlt = altM; }
  }
  if (!tempInit) { tMin = tMax = tempC; tempInit = true; }
  else { if (tempC < tMin) tMin = tempC; if (tempC > tMax) tMax = tempC; }
}

// ============================================================
//  STORM TREND  (pressure change over time)
// ============================================================
void trendTask(uint32_t now) {
  if (!baroReady) return;
  float hpaNow = pressF / 100.0f;

  if (hCount == 0 || (now - lastHist) >= TREND_SAMPLE_MS) {
    hP[hHead] = hpaNow;
    hT[hHead] = now;
    hS[hHead] = stepCount;
    hHead = (hHead + 1) % TREND_N;
    if (hCount < TREND_N) hCount++;
    lastHist = now;
  }

  uint8_t oldest = (hCount < TREND_N) ? 0 : hHead;
  float hours = (now - hT[oldest]) / 3600000.0f;
  if (hours >= 0.25f) {                     // need at least 15 minutes of data
    trendRate  = (hpaNow - hP[oldest]) / hours;
    trendValid = true;
    movingFlag = (stepCount - hS[oldest]) > 200;   // walking around spoils the trend
  } else {
    trendValid = false;
  }

  if (stormDemo)                         stormRisk = 90.0f;
  else if (trendValid && !movingFlag)    stormRisk = clampf(((-trendRate) - 0.2f) / 1.3f * 100.0f, 0.0f, 100.0f);
  else                                   stormRisk = 0;
}

// ============================================================
//  STEP COUNTER
// ============================================================
void onStep(uint32_t now) {
  if ((now - lastStepMs) > 2000UL) { walking = false; pendSteps = 0; }
  lastStepMs = now;
  if (walking) {
    stepCount++;
  } else {
    pendSteps++;
    if (pendSteps >= 4) { walking = true; stepCount += pendSteps; pendSteps = 0; }
  }
}

void stepTask(uint32_t now) {
  stepFilt += 0.25f * (aMag - stepFilt);
  stepAvg  += 0.01f * (stepFilt - stepAvg);
  float thr = stepAvg + STEP_THRESH_G;
  if (!stepAbove && stepFilt > thr && (now - lastCandMs) > STEP_MIN_MS) {
    stepAbove = true;
    lastCandMs = now;
    if (fallState == F_ARMED) onStep(now);
  }
  if (stepAbove && stepFilt < stepAvg + STEP_THRESH_G * 0.4f) stepAbove = false;
  if (walking && (now - lastStepMs) > 2000UL) walking = false;
}

// ============================================================
//  FALL GUARD
// ============================================================
void cancelFall(uint32_t now) {
  fallState  = F_COOLDOWN;
  stateStart = now;
}

void startFallTest(uint32_t now) {
  fallState  = F_PREALARM;
  stateStart = now;
  lastPeakG  = 0;
}

void fallTask(uint32_t now) {
  float dev = fabsf(aMag - 1.0f);
  switch (fallState) {
    case F_ARMED:
      if (aMag > IMPACT_G) {
        fallState  = F_IMPACT;
        stateStart = now;
        stillStart = 0;
        lastPeakG  = aMag;
      }
      break;

    case F_IMPACT:
      if (aMag > lastPeakG) lastPeakG = aMag;
      if ((now - stateStart) > IMPACT_SETTLE_MS) {
        bool still = (dev < STILL_DEV_G) && (gyroMag < STILL_GYRO_DPS);
        if (still) {
          if (stillStart == 0) stillStart = now;
          if ((now - stillStart) >= STILL_NEEDED_MS) {
            fallState  = F_PREALARM;
            stateStart = now;
          }
        } else {
          stillStart = 0;
        }
      }
      if ((now - stateStart) > IMPACT_WINDOW_MS && fallState == F_IMPACT) fallState = F_ARMED;
      break;

    case F_PREALARM:
      if (AUTO_CANCEL_ON_MOVE && (dev > 0.5f || gyroMag > 150.0f)) {
        cancelFall(now);
      } else if ((now - stateStart) >= PRE_ALARM_MS) {
        fallState  = F_ALARM;
        stateStart = now;
        fallCount++;
      }
      break;

    case F_ALARM:
      break;   // only the button cancels a real alarm

    case F_COOLDOWN:
      if ((now - stateStart) > 3000UL) fallState = F_ARMED;
      break;
  }
}

bool fallActive() { return fallState == F_PREALARM || fallState == F_ALARM; }

// ============================================================
//  RGB LED  (the alarm light)
// ============================================================
void ledSet(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (c != ledLast) {
    rgb.setPixelColor(0, r, g, b);
    rgb.show();
    ledLast = c;
  }
}

void ledTask(uint32_t now) {
  switch (fallState) {
    case F_ALARM: {
      uint32_t ph = (now / 100) % 6;                 // red flashes + white strobe
      if (ph == 0 || ph == 2) ledSet(255, 0, 0);
      else if (ph == 4)       ledSet(255, 255, 255);
      else                    ledSet(0, 0, 0);
      return;
    }
    case F_PREALARM: {
      uint32_t left = PRE_ALARM_MS - (now - stateStart);
      uint32_t speed = (left < 4000UL) ? 120 : 250;  // blinks faster near the end
      if ((now / speed) % 2 == 0) ledSet(255, 90, 0); else ledSet(0, 0, 0);
      return;
    }
    case F_IMPACT:
      ledSet(70, 25, 0);                             // checking if you are okay
      return;
    default:
      break;
  }
  if (STORM_LED_ALERT && stormRisk >= 70.0f) {       // slow orange pulse
    float s = (sinf(now / 300.0f) + 1.0f) * 0.5f;
    ledSet((uint8_t)(10 + 150 * s), (uint8_t)(4 + 50 * s), 0);
    return;
  }
  if ((now % 3000UL) < 60) ledSet(0, 25, 0);         // tiny green heartbeat
  else                     ledSet(0, 0, 0);
}

// ============================================================
//  BUTTON AND DIP SWITCHES
// ============================================================
uint8_t readDip() {
  uint8_t v = 0;
  if (digitalRead(PIN_SW1) == LOW) v |= 1;
  if (digitalRead(PIN_SW2) == LOW) v |= 2;
  if (digitalRead(PIN_SW3) == LOW) v |= 4;
  return v;
}

void dipTask(uint32_t now) {
  uint8_t raw = readDip();
  if (raw == dipRaw) {
    if (dipCount < 3) dipCount++;
    if (dipCount >= 3 && raw != mode) { mode = raw; pageStart = now; }
  } else {
    dipRaw = raw;
    dipCount = 0;
  }
}

void buttonTask(uint32_t now) {
  evDown = evShort = evLong = false;
  bool pressed = (digitalRead(PIN_BTN) == LOW);
  if (pressed != btnRaw) { btnRaw = pressed; btnChangeMs = now; }
  if ((now - btnChangeMs) > 25 && pressed != btnState) {
    btnState = pressed;
    if (btnState) { evDown = true; btnDownMs = now; btnLongFired = false; }
    else if (!btnLongFired) { evShort = true; }
  }
  if (btnState && !btnLongFired && (now - btnDownMs) >= 1500UL) {
    btnLongFired = true;
    evLong = true;
  }
}

void handleButton(uint32_t now) {
  if (evDown && fallActive()) { cancelFall(now); suppressUntilRelease = true; }
  else if (evDown && fallState == F_IMPACT) { fallState = F_ARMED; suppressUntilRelease = true; }

  if ((evShort || evLong) && !suppressUntilRelease) {
    if (evShort && mode == 1) {                     // zero relative height
      altRef = altM;
    }
    if (evLong) {
      if (mode == 1) { totalClimb = 0; totalDescent = 0; altRef = altM; lastClimbAlt = altM; }
      if (mode == 2) { stormDemo = !stormDemo; }
      if (mode == 4) { stepCount = 0; pendSteps = 0; }
      if (mode == 6) { startFallTest(now); }
    }
  }
  if (!btnState) suppressUntilRelease = false;
}

// ============================================================
//  ICONS  (all drawn with simple shapes, they float and move)
// ============================================================
const int ICON_CX = 20;
const int ICON_CY = 38;

void iconMountain(int cx, int cy, uint32_t t) {
  oled.fillTriangle(cx - 18, cy + 12, cx - 4, cy - 12, cx + 10, cy + 12, W);
  oled.fillTriangle(cx + 2,  cy + 12, cx + 12, cy - 2, cx + 22, cy + 12, W);
  oled.drawLine(cx - 9, cy - 2, cx - 6, cy - 5, BLACK);       // snow line
  oled.drawLine(cx - 6, cy - 5, cx - 3, cy - 2, BLACK);
  oled.drawLine(cx - 4, cy - 12, cx - 4, cy - 19, W);         // flag
  oled.fillTriangle(cx - 4, cy - 19, cx - 4, cy - 15, cx + 2, cy - 17, W);
  int sx = cx + 16, sy = cy - 12;                             // spinning sun
  oled.fillCircle(sx, sy, 3, W);
  float a = t / 400.0f;
  for (uint8_t k = 0; k < 8; k++) {
    float ang = a + k * 0.7854f;
    oled.drawLine(sx + (int)(5 * cosf(ang)), sy + (int)(5 * sinf(ang)),
                  sx + (int)(7 * cosf(ang)), sy + (int)(7 * sinf(ang)), W);
  }
}

void iconGauge(int cx, int cy, float norm, uint32_t t) {
  oled.drawCircle(cx, cy, 17, W);
  oled.drawCircle(cx, cy, 16, W);
  for (uint8_t i = 0; i < 7; i++) {                           // scale ticks
    float th = (-135.0f + 45.0f * i) * 0.0174533f;
    oled.drawLine(cx + (int)(12 * sinf(th)), cy - (int)(12 * cosf(th)),
                  cx + (int)(15 * sinf(th)), cy - (int)(15 * cosf(th)), W);
  }
  float jitter = sinf(t / 170.0f) * 2.0f;                     // needle trembles a little
  float th = (-135.0f + 270.0f * norm + jitter) * 0.0174533f;
  oled.drawLine(cx, cy, cx + (int)(12 * sinf(th)), cy - (int)(12 * cosf(th)), W);
  oled.fillCircle(cx, cy, 2, W);
}

void iconCloud(int cx, int cy, bool storm, uint32_t t) {
  oled.fillCircle(cx - 9, cy + 1, 7, W);
  oled.fillCircle(cx + 1, cy - 5, 9, W);
  oled.fillCircle(cx + 11, cy + 1, 7, W);
  oled.fillRect(cx - 9, cy + 1, 21, 8, W);
  for (uint8_t i = 0; i < 3; i++) {                           // falling rain
    int x = cx - 8 + i * 9;
    int y0 = cy + 12 + (int)(((t / 60) + i * 7) % 10);
    oled.drawLine(x, y0, x - 2, y0 + 4, W);
  }
  if (storm && ((t / 220) % 2 == 0)) {                        // flashing lightning
    oled.drawLine(cx + 4, cy + 9, cx - 2, cy + 18, BLACK);
    oled.drawLine(cx - 2, cy + 18, cx + 3, cy + 18, BLACK);
    oled.drawLine(cx + 3, cy + 18, cx - 3, cy + 27, BLACK);
  }
}

void iconThermo(int cx, int cy, float norm, uint32_t t) {
  oled.drawRoundRect(cx - 5, cy - 18, 10, 30, 5, W);
  oled.fillCircle(cx, cy + 16, 8, W);
  int h = (int)(norm * 24.0f);
  oled.fillRect(cx - 2, cy + 12 - h, 4, h + 4, W);
  for (uint8_t i = 0; i < 5; i++) {                           // scale marks
    int len = (i % 2 == 0) ? 5 : 3;
    oled.drawLine(cx + 8, cy - 16 + i * 6, cx + 8 + len, cy - 16 + i * 6, W);
  }
  int by = cy + 6 - (int)((t / 70) % 24);                     // heat bubbles rising
  oled.drawCircle(cx + 15, by, 1, W);
}

void iconFoot(int cx, int cy, bool walkingNow, uint32_t t) {
  bool ph = walkingNow ? ((t / 350) % 2 == 0) : false;
  int ly = cy - 12 + (ph ? 0 : 6);
  int ry = cy - 12 + (ph ? 6 : 0);
  oled.fillRoundRect(cx - 11, ly, 8, 11, 3, W);               // left foot
  oled.fillCircle(cx - 7, ly + 14, 3, W);
  oled.fillRoundRect(cx + 3, ry, 8, 11, 3, W);                // right foot
  oled.fillCircle(cx + 7, ry + 14, 3, W);
}

void iconMotion(int cx, int cy, uint32_t t) {
  oled.fillRect(cx - 5, cy - 5, 10, 10, W);                   // the device
  int d = 9 + (int)((t / 110) % 6);                           // arrows pulse outward
  oled.drawLine(cx, cy - d, cx, cy - d - 5, W);
  oled.fillTriangle(cx - 3, cy - d - 3, cx + 3, cy - d - 3, cx, cy - d - 8, W);
  oled.drawLine(cx, cy + d, cx, cy + d + 5, W);
  oled.fillTriangle(cx - 3, cy + d + 3, cx + 3, cy + d + 3, cx, cy + d + 8, W);
  oled.drawLine(cx - d, cy, cx - d - 5, cy, W);
  oled.fillTriangle(cx - d - 3, cy - 3, cx - d - 3, cy + 3, cx - d - 8, cy, W);
  oled.drawLine(cx + d, cy, cx + d + 5, cy, W);
  oled.fillTriangle(cx + d + 3, cy - 3, cx + d + 3, cy + 3, cx + d + 8, cy, W);
  float a = t / 300.0f;                                       // orbiting dot = rotation
  oled.fillCircle(cx + (int)(17 * cosf(a)), cy + (int)(17 * sinf(a)), 2, W);
}

void iconFigure(int cx, int cy, FallState st, uint32_t t) {
  if (st == F_ARMED) {                                        // standing person + radar rings
    int r = 12 + (int)((t / 90) % 10);
    oled.drawCircle(cx, cy, r, W);
    oled.fillCircle(cx, cy - 12, 4, W);
    oled.drawLine(cx, cy - 8, cx, cy + 4, W);
    oled.drawLine(cx - 7, cy - 4, cx + 7, cy - 4, W);
    oled.drawLine(cx, cy + 4, cx - 5, cy + 14, W);
    oled.drawLine(cx, cy + 4, cx + 5, cy + 14, W);
  } else {                                                    // person lying down
    int y = cy + 10;
    oled.fillCircle(cx - 14, y, 4, W);
    oled.drawLine(cx - 10, y, cx + 4, y, W);
    oled.drawLine(cx - 6, y - 5, cx - 2, y + 5, W);
    oled.drawLine(cx + 4, y, cx + 14, y - 4, W);
    oled.drawLine(cx + 4, y, cx + 14, y + 4, W);
    if ((t / 250) % 2 == 0) {                                 // blinking "!"
      oled.fillRect(cx - 1, cy - 16, 3, 12, W);
      oled.fillRect(cx - 1, cy - 1, 3, 3, W);
    }
  }
}

// ============================================================
//  PAGE PARTS
// ============================================================
void header(const char* title) {
  oled.setTextSize(1);
  oled.setTextColor(W);
  oled.setCursor(0, 0);
  oled.print(title);
  char m[6];
  snprintf(m, sizeof(m), "M%d", mode);
  oled.setCursor(128 - 6 * (int)strlen(m), 0);
  oled.print(m);
  oled.drawFastHLine(0, 9, 128, W);
}

void noSensor(const char* what) {
  printAt(46, 18, 1, "NO SENSOR");
  printAt(46, 30, 1, what);
  printAt(46, 44, 1, "check wiring");
}

void setInvert(bool v) {
  if (v != invState) { oled.invertDisplay(v); invState = v; }
}

// ============================================================
//  PAGE 0 : WELCOME ANIMATION
// ============================================================
float easeOut(float k) { k = clampf(k, 0.0f, 1.0f); float u = 1.0f - k; return 1.0f - u * u * u; }

void pageWelcome(uint32_t t) {
  t = t % 11000UL;

  for (uint8_t i = 0; i < 16; i++) {                          // drifting stars
    float ph = t / 900.0f + i * 1.7f;
    int sx = (i * 37 + (int)(sinf(ph) * 7)) % 128;
    int sy = (i * 23 + (int)(cosf(ph * 0.8f) * 5)) % 64;
    if (((t / 200) + i) % 3) oled.drawPixel(sx, sy, W);
  }

  oled.setTextColor(W, BLACK);
  oled.setTextSize(1);

  if (t < 7500UL) {                                           // typed "WELCOME TO"
    const char* w1 = "WELCOME TO";
    int n = (int)(t / 110);
    if (n > 10) n = 10;
    oled.setCursor(34, 3);
    for (int i = 0; i < n; i++) oled.print(w1[i]);
    if (n < 10 && ((t / 250) % 2 == 0)) oled.print('_');
  } else {                                                    // then a hint
    const char* w2 = "SWITCH 001 TO 111";
    int n = (int)((t - 7500UL) / 70);
    int len = (int)strlen(w2);
    if (n > len) n = len;
    oled.setCursor((128 - len * 6) / 2, 3);
    for (int i = 0; i < n; i++) oled.print(w2[i]);
  }

  if (t > 1300UL) {                                           // big name slides in from the left
    float k = easeOut((t - 1300UL) / 700.0f);
    int wpx = (int)strlen(NAME_LINE1) * 12;
    int xt = (128 - wpx) / 2;
    int x = -wpx + (int)((xt + wpx) * k);
    oled.setTextSize(2);
    oled.setCursor(x, 15);
    oled.print(NAME_LINE1);
  }
  if (t > 1900UL) {                                           // second line slides in from the right
    float k = easeOut((t - 1900UL) / 700.0f);
    int wpx = (int)strlen(NAME_LINE2) * 12;
    int xt = (128 - wpx) / 2;
    int x = 128 - (int)((128 - xt) * k);
    oled.setTextSize(2);
    oled.setCursor(x, 33);
    oled.print(NAME_LINE2);
  }

  if (t > 2700UL) {                                           // underline sweeps across
    int len = (int)((t - 2700UL) * 128UL / 700UL);
    if (len > 128) len = 128;
    oled.drawFastHLine(0, 51, len, W);
    if (len >= 128) oled.drawFastHLine((int)((t / 8) % 128), 51, 8, BLACK);   // shimmer
  }

  if (t > 3400UL) {                                           // project name typed out
    int len = (int)strlen(PROJECT_NAME);
    int n = (int)((t - 3400UL) / 85);
    if (n > len) n = len;
    oled.setTextSize(1);
    oled.setCursor((128 - len * 6) / 2, 55);
    for (int i = 0; i < n; i++) oled.print(PROJECT_NAME[i]);
  }
  oled.setTextColor(W);
}

// ============================================================
//  PAGE 1 : ALTITUDE
// ============================================================
void pageAltitude(uint32_t t) {
  header("ALTITUDE");
  iconMountain(ICON_CX, ICON_CY + bobY(t, 3), t);
  if (!baroReady) { noSensor("pressure"); return; }
  char b[24], c[24];
  fmtF(b, sizeof(b), altM, 1);
  printAt(46, 12, 2, b);
  fmtF(b, sizeof(b), altM * 3.28084f, 0);
  snprintf(c, sizeof(c), "m = %s ft", b);
  printAt(46, 31, 1, c);
  fmtF(b, sizeof(b), totalClimb, 0);
  snprintf(c, sizeof(c), "Climb +%s m", b);
  printAt(46, 41, 1, c);
  fmtS(b, sizeof(b), altM - altRef, 1);
  snprintf(c, sizeof(c), "Rel %s m", b);
  printAt(46, 51, 1, c);
}

// ============================================================
//  PAGE 2 : PRESSURE + STORM WATCH
// ============================================================
void pagePressure(uint32_t t) {
  header("PRESSURE");
  if (!baroReady) { iconGauge(ICON_CX, ICON_CY, 0.5f, t); noSensor("pressure"); return; }
  float hpa = pressF / 100.0f;
  if (stormRisk >= 50.0f) iconCloud(ICON_CX, ICON_CY - 6 + bobY(t, 3), true, t);
  else                    iconGauge(ICON_CX, ICON_CY + bobY(t, 3), clampf((hpa - 950.0f) / 100.0f, 0.0f, 1.0f), t);

  char b[24], c[28];
  fmtF(b, sizeof(b), hpa, 1);
  printAt(46, 12, 2, b);
  snprintf(c, sizeof(c), "hPa=%lu Pa", (unsigned long)(pressF + 0.5f));
  printAt(46, 31, 1, c);

  if (stormRisk >= 70.0f && ((t / 400) % 2 == 0)) {
    printAt(46, 41, 1, "STORM WARNING!");
  } else {
    snprintf(c, sizeof(c), "Storm risk %d%%", (int)(stormRisk + 0.5f));
    printAt(46, 41, 1, c);
  }

  oled.setTextSize(1);
  oled.setCursor(46, 51);
  if (stormDemo)            oled.print("DEMO MODE");
  else if (!trendValid)     oled.print("Trend: learning");
  else if (movingFlag)      oled.print("Trend: moving");
  else {
    oled.print("Trend ");
    oled.write(trendRate < 0 ? 25 : 24);                      // down / up arrow
    fmtS(b, sizeof(b), trendRate, 1);
    oled.print(b);
    oled.print("/h");
  }
  oled.drawRect(46, 60, 82, 4, W);                            // risk bar
  int fillW = (int)(stormRisk * 0.80f);
  if (fillW > 0) oled.fillRect(47, 61, fillW, 2, W);
}

// ============================================================
//  PAGE 3 : TEMPERATURE
// ============================================================
void pageTemperature(uint32_t t) {
  header("TEMPERATURE");
  iconThermo(ICON_CX, ICON_CY + bobY(t, 2) - 2, clampf(tempC / 50.0f, 0.0f, 1.0f), t);
  if (!baroReady) { noSensor("temperature"); return; }
  char b[24], c[28];
  fmtF(b, sizeof(b), tempC, 1);
  int len = (int)strlen(b);
  printAt(46, 12, 2, b);
  oled.setTextSize(2);
  oled.setCursor(46 + len * 12, 12);
  oled.write(247);
  oled.print('C');

  fmtF(b, sizeof(b), tempC * 1.8f + 32.0f, 1);
  oled.setTextSize(1);
  oled.setCursor(46, 31);
  oled.print("= ");
  oled.print(b);
  oled.print(' ');
  oled.write(247);
  oled.print('F');

  char lo[12], hi[12];
  fmtF(lo, sizeof(lo), tMin, 1);
  fmtF(hi, sizeof(hi), tMax, 1);
  snprintf(c, sizeof(c), "Lo%s Hi%s", lo, hi);
  printAt(46, 41, 1, c);

  const char* feel = "Hot";
  if (tempC < 10) feel = "Cold";
  else if (tempC < 18) feel = "Cool";
  else if (tempC < 27) feel = "Comfortable";
  else if (tempC < 33) feel = "Warm";
  printAt(46, 51, 1, feel);
}

// ============================================================
//  PAGE 4 : STEPS
// ============================================================
void pageSteps(uint32_t t) {
  header("STEPS");
  iconFoot(ICON_CX, ICON_CY + 2, walking, t);
  if (!imuOk) { noSensor("motion"); return; }
  char b[24], c[28];
  snprintf(b, sizeof(b), "%lu", (unsigned long)stepCount);
  printAt(46, 12, 2, b);
  printAt(46, 31, 1, walking ? "steps  walking" : "steps");
  float dist = stepCount * STRIDE_M;
  if (dist < 1000.0f) { fmtF(b, sizeof(b), dist, 0); snprintf(c, sizeof(c), "Dist %s m", b); }
  else                { fmtF(b, sizeof(b), dist / 1000.0f, 2); snprintf(c, sizeof(c), "Dist %s km", b); }
  printAt(46, 41, 1, c);
  uint32_t pct = (stepCount * 100UL) / STEP_GOAL;
  snprintf(c, sizeof(c), "Goal %lu%%", (unsigned long)pct);
  printAt(46, 51, 1, c);
  oled.drawRect(46, 60, 82, 4, W);
  int fw = (int)(clampf(pct, 0, 100) * 0.80f);
  if (fw > 0) oled.fillRect(47, 61, fw, 2, W);
}

// ============================================================
//  PAGE 5 : MOTION
// ============================================================
void pageMotion(uint32_t t) {
  header("MOTION");
  iconMotion(ICON_CX, ICON_CY + bobY(t, 2), t);
  if (!imuOk) { noSensor("motion"); return; }
  char b[24], c[28];
  fmtF(b, sizeof(b), aMag, 2);
  int len = (int)strlen(b);
  printAt(46, 12, 2, b);
  printAt(46 + len * 12 + 3, 19, 1, "g");

  fmtF(b, sizeof(b), aMag * 9.80665f, 2);
  oled.setTextSize(1);
  oled.setCursor(46, 31);
  oled.print("=");
  oled.print(b);
  oled.print(" m/s");
  oled.write(253);                                            // small 2

  float roll  = atan2f(ays, azs) * 57.29578f;
  float pitch = atan2f(-axs, sqrtf(ays * ays + azs * azs)) * 57.29578f;
  snprintf(c, sizeof(c), "P%+d R%+d ", (int)pitch, (int)roll);
  oled.setCursor(46, 41);
  oled.print(c);
  oled.write(247);

  snprintf(c, sizeof(c), "Spin %d deg/s", (int)(gyroMag + 0.5f));
  printAt(46, 51, 1, c);
}

// ============================================================
//  PAGE 6 : FALL GUARD
// ============================================================
void pageFall(uint32_t t) {
  header("FALL GUARD");
  iconFigure(ICON_CX, ICON_CY + (fallState == F_ARMED ? bobY(t, 2) : 0), fallState, t);
  if (!imuOk) { noSensor("motion"); return; }
  const char* st = "ARMED";
  if (fallState == F_IMPACT)   st = "IMPACT";
  if (fallState == F_COOLDOWN) st = "RESET";
  printAt(46, 12, 2, st);
  char b[20], c[28];
  fmtF(b, sizeof(b), lastPeakG, 1);
  snprintf(c, sizeof(c), "Peak hit %s g", b);
  printAt(46, 31, 1, c);
  snprintf(c, sizeof(c), "Alarms: %u", (unsigned)fallCount);
  printAt(46, 41, 1, c);
  printAt(46, 51, 1, "Hold btn=test");
}

// ============================================================
//  PAGE 7 : SUMMARY
// ============================================================
void pageSummary(uint32_t t) {
  header("SUMMARY");
  oled.drawFastVLine(64, 11, 53, W);
  oled.drawFastHLine(0, 36, 128, W);
  char b[20];
  printAt(2, 11, 1, "ALT m");
  printAt(68, 11, 1, "hPa");
  printAt(2, 39, 1, "TEMP C");
  printAt(68, 39, 1, "STEPS");

  if (baroReady) {
    fmtF(b, sizeof(b), altM, 0);                  printAt(2, 20, 2, b);
    fmtF(b, sizeof(b), pressF / 100.0f, 0);       printAt(68, 20, 2, b);
    fmtF(b, sizeof(b), tempC, 1);                 printAt(2, 48, 2, b);
  } else {
    printAt(2, 24, 1, "--");  printAt(68, 24, 1, "--");  printAt(2, 52, 1, "--");
  }
  snprintf(b, sizeof(b), "%lu", (unsigned long)stepCount);
  printAt(68, 48, 2, b);

  int pulse = (int)((t / 150) % 4);                           // tiny live "dots" in the header
  for (int i = 0; i < pulse; i++) oled.fillCircle(70 + i * 6, 4, 1, W);
}

// ============================================================
//  FALL ALARM SCREEN (shows over any page)
// ============================================================
void drawAlarmScreen(uint32_t now) {
  setInvert(((now / 300) % 2) == 0);
  oled.setTextColor(W);
  if (fallState == F_PREALARM) {
    uint32_t el = now - stateStart;
    int rem = (int)((PRE_ALARM_MS - el + 999UL) / 1000UL);
    if (rem < 0) rem = 0;
    centerText("FALL DETECTED?", 2, 1);
    char b[8];
    snprintf(b, sizeof(b), "%d", rem);
    centerText(b, 15, 4);
    centerText("Press btn if OK", 54, 1);
  } else {
    centerText("ALARM!", 4, 3);
    centerText("FALL - NEED HELP", 32, 1);
    centerText("Press button", 44, 1);
    centerText("to cancel", 54, 1);
  }
}

// ============================================================
//  MAIN DRAW
// ============================================================
void drawUI(uint32_t now) {
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextColor(W);

  if (fallActive()) {
    drawAlarmScreen(now);
    oled.display();
    return;
  }
  setInvert(false);

  uint32_t t = now - pageStart;
  switch (mode) {
    case 0: pageWelcome(t);     break;
    case 1: pageAltitude(t);    break;
    case 2: pagePressure(t);    break;
    case 3: pageTemperature(t); break;
    case 4: pageSteps(t);       break;
    case 5: pageMotion(t);      break;
    case 6: pageFall(t);        break;
    case 7: pageSummary(t);     break;
  }

  if (mode != 0 && t < 260UL) {                               // page change wipe effect
    int rev = (int)(128UL * t / 260UL);
    oled.fillRect(rev, 0, 128 - rev, 64, BLACK);
    oled.drawFastVLine(rev, 0, 64, W);
  }
  oled.display();
}

// ============================================================
//  BOOT SCREEN
// ============================================================
void bootScreen() {
  Serial.println(F("TRONIC ZENERGY Survival Watch"));
  Serial.print(F("OLED   : ")); Serial.println(oledOk ? "OK" : "NOT FOUND");
  Serial.print(F("MPU6050: ")); Serial.println(haveMpu ? "OK" : "NOT FOUND");
  Serial.print(F("Baro   : ")); Serial.println(baroName);
  if (!oledOk) return;

  oled.clearDisplay();
  oled.setTextColor(W);
  printAt(0, 0, 1, "TRONIC ZENERGY");
  oled.drawFastHLine(0, 9, 128, W);
  printAt(0, 14, 1, "OLED     OK");
  printAt(0, 26, 1, haveMpu ? "MPU6050  OK" : "MPU6050  MISSING");
  char c[24];
  if (baroType != BARO_NONE) snprintf(c, sizeof(c), "%-8s OK", baroName);
  else                       snprintf(c, sizeof(c), "PRESSURE MISSING");
  printAt(0, 38, 1, c);
  printAt(0, 54, 1, "starting...");
  oled.display();
  delay(1600);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_SW1, INPUT_PULLUP);
  pinMode(PIN_SW2, INPUT_PULLUP);
  pinMode(PIN_SW3, INPUT_PULLUP);
  pinMode(PIN_BTN, INPUT_PULLUP);

  rgb.begin();
  rgb.clear();
  rgb.show();
  ledSet(0, 30, 30);                                          // cyan while booting

  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();
  Wire.setClock(400000);

  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false);
  if (!oledOk) oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3D, true, false);
  if (oledOk) {
    oled.cp437(true);                                         // makes the degree sign work
    oled.clearDisplay();
    oled.display();
  }

  haveMpu = mpuBegin();
  baroBegin();
  bootScreen();

  mode    = readDip();
  dipRaw  = mode;
  pageStart = millis();
  stateStart = millis();
  ledSet(0, 0, 0);
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  uint32_t now = millis();

  // ---- motion sensor, 100 times per second ----
  if (haveMpu && (now - tImu) >= 10) {
    tImu = now;
    imuOk = mpuRead();
    if (imuOk) {
      stepTask(now);
      fallTask(now);
    }
  }

  // ---- pressure / temperature ----
  if ((now - tBaro) >= 20) {
    tBaro = now;
    baroTask(now);
    if (baroNew) {
      baroNew = false;
      onNewPressure(now);
    }
    trendTask(now);
  }

  // ---- button ----
  if ((now - tBtn) >= 5) {
    tBtn = now;
    buttonTask(now);
    handleButton(now);
  }

  // ---- mode switches ----
  if ((now - tDip) >= 20) {
    tDip = now;
    dipTask(now);
  }

  // ---- screen, about 14 frames per second ----
  if ((now - tDisp) >= 70) {
    tDisp = now;
    drawUI(now);
  }

  // ---- alarm light ----
  ledTask(now);

  // ---- once a second: debug to Serial Monitor ----
  if ((now - tSerial) >= 1000UL) {
    tSerial = now;
    char a[16], p[16], tt[16], g[16];
    fmtF(a,  sizeof(a),  altM, 1);
    fmtF(p,  sizeof(p),  pressF / 100.0f, 1);
    fmtF(tt, sizeof(tt), tempC, 1);
    fmtF(g,  sizeof(g),  aMag, 2);
    char line[96];
    snprintf(line, sizeof(line), "mode=%d alt=%sm p=%shPa t=%sC steps=%lu g=%s fall=%d risk=%d%%",
             mode, a, p, tt, (unsigned long)stepCount, g, (int)fallState, (int)stormRisk);
    Serial.println(line);
  }

  delay(1);
}
