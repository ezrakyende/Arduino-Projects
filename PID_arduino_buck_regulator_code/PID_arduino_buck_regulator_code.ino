#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// USER HARDWARE CONFIGURATION
// ==========================================
#define INVERT_PWM       false  // Set to 'true' if using P-Channel + NPN driver
#define TARGET_VOLTAGE   5.00   // Target regulated output voltage
#define MAX_DUTY_CYCLE   230    // Max duty limit (~90% of 255) to protect components

// ==========================================
// CURRENT SENSOR CALIBRATION (TRIM HERE)
// ==========================================
const float MANUAL_CURRENT_TRIM = 0.00; 

// Pin Definitions
const int VIN_PIN = A0;         // Vin Voltage Divider (measures 13.2V)
const int VNODE_PIN = A1;       // Vnode Voltage Divider (measures Inductor Pin 2)
const int CURRENT_PIN = A2;     // ACS712 Output
const int PWM_PIN = 9;          // Timer 1 PWM output (OC1A)

// Calibration Constants
const float V_REF = 5.00;       // Measure your actual 7805 voltage and adjust if needed
const float DIVIDER_RATIO = 5.545; // (10k + 2.2k) / 2.2k
const float CURRENT_SENSITIVITY = 0.100; // 100mV/A for ACS712 20A version

// LCD Configuration
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// PI Controller Constants
const float Kp = 12.0;          // Proportional gain
const float Ki = 4.0;           // Integral gain
float integral = 0.0;

// Timing Variables
unsigned long last_pid_time = 0;
unsigned long last_lcd_time = 0;
unsigned long last_serial_time = 0;
unsigned long start_time = 0;

const unsigned long PID_INTERVAL = 5;    // Run PID every 5ms (200Hz loop)
const unsigned long LCD_INTERVAL = 300;  // Update LCD/Current every 300ms
const unsigned long SERIAL_INTERVAL = 500;// Telemetry output every 500ms

// System States
float current_target = 0.0;
float zero_current_voltage = 2.5; // Calibrated during setup

void setup() {
  Serial.begin(115200);
  
  // Configure High-Frequency PWM on Pin 9 (Timer 1, 62.5 kHz, 8-bit Fast PWM)
  pinMode(PWM_PIN, OUTPUT);
  TCCR1A = _BV(COM1A1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);
  writePWM(0); // Ensure output is off during initialization
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Booting...");
  
  // Perform ACS712 Auto-Calibration
  lcd.setCursor(0, 1);
  lcd.print("Calibrating ACS...");
  calibrateACS712();
  
  lcd.clear();
  start_time = millis();
}

void loop() {
  unsigned long now = millis();
  
  // 1. Soft-Start Voltage Ramping (0V to 5.0V over 1.5 seconds)
  unsigned long elapsed = now - start_time;
  if (elapsed < 1500) {
    current_target = ((float)elapsed / 1500.0) * TARGET_VOLTAGE;
  } else {
    current_target = TARGET_VOLTAGE;
  }
  
  // Read Voltages (Using fast sampling to keep control loop fast)
  float vin = readVoltage(VIN_PIN);
  float vnode = readVoltage(VNODE_PIN);
  
  // Low-Side Buck Calculation: Vout is the difference across the load
  float vout = vin - vnode;
  if (vout < 0.0) {
    vout = 0.0;
  }

  // 2. Run PI Feedback Controller (Every 5ms)
  if (now - last_pid_time >= PID_INTERVAL) {
    float dt = (float)(now - last_pid_time) / 1000.0;
    last_pid_time = now;
    
    // Safety check: shut down if inputs or outputs exceed safe thresholds
    if (vin < 6.0 || vout > 6.5) {
      writePWM(0);
      integral = 0;
    } else {
      float error = current_target - vout;
      
      // Calculate integral with windup clamping
      integral += error * dt;
      integral = constrain(integral, -20.0, 20.0);
      
      float control_effort = (Kp * error) + (Ki * integral);
      int duty = constrain((int)control_effort, 0, MAX_DUTY_CYCLE);
      
      writePWM(duty);
    }
  }

  // 3. Update LCD Screen and Read Current (Non-blocking, every 300ms)
  // This prevents the slow current sensor from lag-blocking the PI loop!
  if (now - last_lcd_time >= LCD_INTERVAL) {
    last_lcd_time = now;

    // Read the current only when the screen is ready to display it
    float iout = readCurrent() + MANUAL_CURRENT_TRIM;
    if (iout < 0.0) {
      iout = 0.0;
    }
    
    float pout = vout * iout;
    if (pout < 0.0) {
      pout = 0.0;
    }

    updateDisplay(vin, vout, iout, pout);
  }

  // 4. Output Serial Diagnostics
  if (now - last_serial_time >= SERIAL_INTERVAL) {
    last_serial_time = now;
    printDiagnostics(vin, vout, readCurrent() + MANUAL_CURRENT_TRIM);
  }
}

// Low-level Helper to write PWM register
void writePWM(int duty) {
  if (INVERT_PWM) {
    OCR1A = 255 - duty; // Inverted for P-channel level shifter configuration
  } else {
    OCR1A = duty;       // Standard active-high PWM
  }
}

// Fast Filtered Voltage Reading (Reduced to 10 samples for speed)
float readVoltage(int pin) {
  long sum = 0;
  const int samples = 10; 
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
  }
  float avg_adc = (float)sum / samples;
  float pin_voltage = (avg_adc * V_REF) / 1023.0;
  return pin_voltage * DIVIDER_RATIO;
}

// Auto-Calibrate Zero Current Level
void calibrateACS712() {
  long sum = 0;
  const int samples = 300;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(CURRENT_PIN);
    delay(2);
  }
  float avg_adc = (float)sum / samples;
  zero_current_voltage = (avg_adc * V_REF) / 1023.0;
  
  // Guard against missing sensor
  if (zero_current_voltage < 1.5 || zero_current_voltage > 3.5) {
    zero_current_voltage = 2.5; 
  }
}

// Highly Filtered Current Reading (Only runs every 300ms)
float readCurrent() {
  long sum = 0;
  const int samples = 200; // Fast enough to not block the LCD frame
  for (int i = 0; i < samples; i++) {
    sum += analogRead(CURRENT_PIN);
    delayMicroseconds(10);
  }
  float avg_adc = (float)sum / samples;
  float pin_voltage = (avg_adc * V_REF) / 1023.0;
  
  // Compute current based on zero-current baseline
  float current = (pin_voltage - zero_current_voltage) / CURRENT_SENSITIVITY;
  return current;
}

// Non-flickering LCD updates with strict 16-character limits and safety padding
void updateDisplay(float vin, float vout, float iout, float pout) {
  char vin_str[6];
  char vout_str[6];
  char iout_str[6];
  char pout_str[6];

  // Convert floats to strictly formatted 4-character strings
  dtostrf(vin, 4, 1, vin_str);   // E.g., "13.2" or " 5.4"
  dtostrf(vout, 4, 2, vout_str); // E.g., "5.00"
  dtostrf(iout, 4, 2, iout_str); // E.g., "0.01"
  dtostrf(pout, 4, 1, pout_str); // E.g., " 0.1"

  char line1[18] = {0};
  char line2[18] = {0};

  // Line 1 Format: "Vi:XX.XV Vo:X.XX" (15-16 characters)
  snprintf(line1, sizeof(line1), "Vi:%sV Vo:%s", vin_str, vout_str);

  // Line 2 Format: "I:X.XXA P:XX.XW" (Total 15-16 characters)
  snprintf(line2, sizeof(line2), "I:%sA P:%sW", iout_str, pout_str);

  // Pad Line 1 to exactly 16 characters with spaces to overwrite old text
  int len1 = strlen(line1);
  while (len1 < 16) {
    line1[len1] = ' ';
    len1++;
  }
  line1[16] = '\0';

  // Pad Line 2 to exactly 16 characters with spaces to overwrite old text
  int len2 = strlen(line2);
  while (len2 < 16) {
    line2[len2] = ' ';
    len2++;
  }
  line2[16] = '\0';

  // Output directly to the screen
  lcd.setCursor(0, 0);
  lcd.print(line1);
  
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// Serial Diagnostic Telemetry
void printDiagnostics(float vin, float vout, float iout) {
  int raw_vin_adc = analogRead(VIN_PIN);
  int raw_vnode_adc = analogRead(VNODE_PIN);
  int raw_curr_adc = analogRead(CURRENT_PIN);
  
  Serial.print("--- TELEMETRY ---");
  Serial.print(" | Target V: "); Serial.print(current_target, 2);
  Serial.print(" | Vin: "); Serial.print(vin, 2); Serial.print("V (ADC: "); Serial.print(raw_vin_adc); Serial.print(")");
  Serial.print(" | Vout: "); Serial.print(vout, 2); Serial.print("V (ADC: "); Serial.print(raw_vnode_adc); Serial.print(")");
  Serial.print(" | Iout: "); Serial.print(iout, 3); Serial.print("A (ADC: "); Serial.print(raw_curr_adc); Serial.print(")");
  Serial.print(" | Duty Register (OCR1A): "); Serial.println(OCR1A);
}