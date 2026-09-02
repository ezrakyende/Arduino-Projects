// --- Pin Assignments ---
const int INA = 5;  // PWM Pin to control speed
const int INB = 6;  // Reference Pin (Kept HIGH for Active-LOW logic)

// --- Safe Speed Thresholds (Inverted for Active-LOW) ---
// For Active-LOW: 255 is completely OFF, and 0 is absolute MAX speed.
const int MIN_SPEED = 205;  // Starts slowly (255 - 50 = 205)
const int MAX_SPEED = 65;   // Safe top speed ceiling (255 - 190 = 65)

// --- Timing Controls ---
const int STEP_DELAY = 40;      // 40ms per step for smooth acceleration
const int TOP_SPEED_HOLD = 4000; // Hold top speed for 4 seconds
const int IDLE_HOLD = 3000;      // Stay completely off for 3 seconds

void setup() {
  pinMode(INA, OUTPUT);
  pinMode(INB, OUTPUT);
  
  // Set INB to HIGH. This provides the 5V reference for Active-LOW switching.
  digitalWrite(INB, HIGH);
  
  // Start with the fan completely off
  turnFanOff();
}

void loop() {
  // 1. GRADUAL SPIN UP (Acceleration)
  // To speed up an Active-LOW fan, we must DECREASE the PWM value toward 0
  for (int currentSpeed = MIN_SPEED; currentSpeed >= MAX_SPEED; currentSpeed--) {
    analogWrite(INA, currentSpeed);
    delay(STEP_DELAY); 
  }

  // 2. HOLD TOP SPEED
  delay(TOP_SPEED_HOLD);

  // 3. GRADUAL SPIN DOWN (Deceleration)
  // To slow down an Active-LOW fan, we INCREASE the PWM value back toward 255
  for (int currentSpeed = MAX_SPEED; currentSpeed <= MIN_SPEED; currentSpeed++) {
    analogWrite(INA, currentSpeed);
    delay(STEP_DELAY);
  }

  // 4. SYSTEM IDLE / OFF
  turnFanOff();
  delay(IDLE_HOLD);
}

// Helper function to handle Active-LOW shutdown
void turnFanOff() {
  digitalWrite(INA, HIGH); // Both pins HIGH = zero voltage difference = motor stops
  digitalWrite(INB, HIGH);
}
