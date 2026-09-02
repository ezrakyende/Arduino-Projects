#include <Keypad.h>
#include <Servo.h>

// --- Configuration Constants ---
const String CORRECT_PASSWORD = "0943";

// Servo Motor Positions (Adjust these numbers if your lock moves the wrong way)
const int SERVO_LOCKED_POS = 0;
const int SERVO_UNLOCKED_POS = 90;

// Arduino Nano Pin Definitions
const int RED_LED_PIN = A0;
const int GREEN_LED_PIN = A1;
const int BUZZER_PIN = A2;
const int SERVO_PIN = 11;

// --- Keypad Setup ---
const byte ROWS = 4; 
const byte COLS = 3; 

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

// Connect keypad rows and columns to these digital pins
byte rowPins[ROWS] = {2, 3, 4, 5}; 
byte colPins[COLS] = {6, 7, 8};    

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo myServo;

// --- System Variables ---
String inputPassword = "";

void setup() {
  // Initialize Pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize Servo
  myServo.attach(SERVO_PIN);
  
  // Default State: Lock the door on startup
  lockDoor();
}

void loop() {
  char key = keypad.getKey();
  
  if (key) {
    // Play a quick beep for every key pressed
    shortBeep();
    
    if (key == '#') {
      // User is done entering the password. Check it.
      if (inputPassword == CORRECT_PASSWORD) {
        unlockDoor();
      } else {
        triggerAlarm();
      }
      // Clear the password input after checking
      inputPassword = ""; 
    } 
    else if (key == '*') {
      // Reset key pressed: Clear input and make sure door stays locked
      inputPassword = "";
      lockDoor();
      shortBeep(); // Extra beep to show reset happened
    } 
    else {
      // Save the pressed number to our password string
      inputPassword += key;
    }
  }
}

// --- Action Functions ---

void lockDoor() {
  myServo.write(SERVO_LOCKED_POS);
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void unlockDoor() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  myServo.write(SERVO_UNLOCKED_POS);
  
  // Long continuous correct sound (2 seconds)
  tone(BUZZER_PIN, 1000); 
  delay(2000);
  noTone(BUZZER_PIN);
  
  // Wait 3 more seconds before automatically locking the door again
  delay(3000); 
  lockDoor();
}

// --- Sound Functions ---

void shortBeep() {
  tone(BUZZER_PIN, 2000, 100); // 2000Hz frequency for 100 milliseconds
}

void triggerAlarm() {
  // Police siren alarm style sound (alternating frequencies)
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, 600); // Low tone
    delay(250);
    tone(BUZZER_PIN, 1200); // High tone
    delay(250);
  }
  noTone(BUZZER_PIN); // Turn off buzzer
}
