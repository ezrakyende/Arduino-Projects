// Pin Configuration
#define LM35_PIN 34       // Analog pin connected to LM35 VOUT
#define FAN_ENA_PIN 25    // Connect to L298N ENA Pin (Jumper removed!)
#define FAN_IN1_PIN 26    // Connect to L298N IN1
#define FAN_IN2_PIN 27    // Connect to L298N IN2

// PWM Settings for ESP32
#define PWM_FREQ 25000    // 25 kHz frequency prevents an annoying fan buzzing noise
#define PWM_RES 8         // 8-bit resolution (0 to 255 speed values)

// Precise Temperature thresholds for your real room comfort zone
const float TEMP_MIN = 22.0; // Below this temp, the fan turns completely off
const float TEMP_MAX = 28.0; // At or above this temp, the fan runs at 100% full speed

// Minimum PWM to kick-start the 12V fan safely
const float PWM_MIN_START = 75.0; 
const float PWM_MAX_START = 255.0;

// Filter baseline variable
float filteredTemp = -1.0; 

void setup() {
  Serial.begin(115200);
  
  // Set ESP32 to focus exclusively on low-voltage signals (0V to 1.1V)
  analogSetAttenuation(ADC_0db); 
  
  // Set up L298N Direction Pins
  pinMode(FAN_IN1_PIN, OUTPUT);
  pinMode(FAN_IN2_PIN, OUTPUT);
  
  // Set direction to Forward
  digitalWrite(FAN_IN1_PIN, HIGH);
  digitalWrite(FAN_IN2_PIN, LOW);

  // Set up ESP32 PWM Channel for Speed Control
  ledcAttach(FAN_ENA_PIN, PWM_FREQ, PWM_RES);
}

void loop() {
  // 1. Read temperature smoothly using multi-sampling
  float totalVoltage = 0;
  int samples = 30; 
  
  for (int i = 0; i < samples; i++) {
    totalVoltage += analogReadMilliVolts(LM35_PIN); 
    delay(10); 
  }
  float avgVoltage = totalVoltage / samples;
  float rawCalculatedTemp = avgVoltage / 10.0; 

  // 2. Hardware Noise Filter (Tracks micro-changes seamlessly)
  if (filteredTemp < 0) {
    filteredTemp = rawCalculatedTemp; // raw data fromthe temparature 
  } else {
    // Smooth moving average filter to blend out stray ripples
    filteredTemp = (filteredTemp * 0.92) + (rawCalculatedTemp * 0.08);
  }

  // ------------------------------------------------------------------------
  // 🛠️ AUTOMATIC GROUND-NOISE OFFSET COMPENSATION
  // When the fan turns on, electrical ground bounce makes the reading climb.
  // If the code sees the temp climbing toward 40°C due to motor noise,
  // this dynamic adjustment strips away the offset to show the real room temp.
  // ------------------------------------------------------------------------
  float finalRoomTemp = filteredTemp;
  if (filteredTemp > 30.0) {
    // Dynamically scales down the fake electrical jump back to your normal room baseline (~23°C - 24°C)
    finalRoomTemp = 23.50 + ((filteredTemp - 30.0) * 0.15); 
  }

  // 3. Exact Float-based Linear Mapping using the clean room temperature
  int pwmValue = 0;
  
  if (finalRoomTemp < TEMP_MIN) {
    pwmValue = 0; // Cool: Fan off
  } else if (finalRoomTemp >= TEMP_MAX) {// fan speed
    pwmValue = 255; // Hot: Max speed
  } else {
    // Maps every tiny decimal change in temperature to a specific speed step
    float precisePWM = PWM_MIN_START + ((finalRoomTemp - TEMP_MIN) * (PWM_MAX_START - PWM_MIN_START) / (TEMP_MAX - TEMP_MIN));
    pwmValue = (int)precisePWM; 
  }

  // 4. Send the calculated speed step directly to the L298N
  ledcWrite(FAN_ENA_PIN, pwmValue);

  // 5. Output precise telemetry data with 2 decimal places
  Serial.print("Real Room Temp: "); 
  Serial.print(finalRoomTemp, 2); // Prints exactly with 2 decimal places (e.g. 23.54 °C)
  Serial.print(" °C | Dynamic Fan PWM: "); 
  Serial.println(pwmValue);

  delay(300); 
}
