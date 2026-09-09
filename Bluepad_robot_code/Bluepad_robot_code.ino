#include <Bluepad32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//================= OLED Settings =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Alternative I2C Pins (Since GPIO 21 and 22 are locked for motors)
const int OLED_SDA = 13;
const int OLED_SCL = 15;

//================= Motor Pins (DO NOT CHANGE) =================
const int enA = 5;
const int enB = 4;

const int up    = 22;
const int down  = 21;
const int left  = 19;
const int right = 18;

//================= New Hardware Pins =================
const int SERVO_PIN = 25;   // SG90 signal pin
const int RED_LED_PIN = 26; // Warning indicator LED

//================= PWM Configuration =================
const int pwmChannelA = 1;
const int pwmChannelB = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;

const int servoChannel = 2; // Separate PWM channel for SG90
const int servoFreq = 50;   // 50Hz for standard analog servos
const int servoResolution = 12; // 12-bit resolution (0-4095)

//================= Variables ==================
int throttle = 0;
int steering = 0;
int servoAngle = 90; // Default startup angle (centered shield)
unsigned long lastServoUpdate = 0;

ControllerPtr myControllers[BP32_MAX_GAMEPADS];
int gamepadBattery = 0; // Stores current gamepad battery level state

//================= Diagnostics and Timing =================
unsigned long loopTime = 0;
unsigned long lastDisplayUpdate = 0;
bool hasWarning = false;
String warningMsg = "NONE";

//================= Celebration Modes =================
enum CelebrationState {
    CELEB_NONE,
    CELEB_TRIANGLE,
    CELEB_CROSS,
    CELEB_SQUARE,
    CELEB_CIRCLE
};

CelebrationState currentCeleb = CELEB_NONE;
unsigned long celebStartTime = 0;
const unsigned long CELEB_DURATION = 5000; // 5 seconds per dance

//================= Servo Driving Helper =================
void setServoAngle(int angle) {
    angle = constrain(angle, 0, 180);
    // Maps 0-180 degrees to standard 0.5ms - 2.5ms servo duty cycle at 12-bit
    int duty = map(angle, 0, 180, 102, 512); 
    ledcWrite(servoChannel, duty);
}

//================= Controller Callback: Connect =================
void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            myControllers[i] = ctl;
            Serial.println("Controller Connected");
            break;
        }
    }
}

//================= Controller Callback: Disconnect =================
void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            myControllers[i] = nullptr;
            Serial.println("Controller Disconnected");
        }
    }
}

//================= Helper to check controller state =================
bool isControllerConnected() {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] != nullptr && myControllers[i]->isConnected()) {
            return true;
        }
    }
    return false;
}

//================= Process Inputs =================
void processGamepad(ControllerPtr ctl) {
    // SWAPPED: Left joystick Y controls Forward/Reverse
    throttle = -ctl->axisY();

    // SWAPPED: Right joystick X controls Steering
    steering = ctl->axisRX();

    // Dead zone filter
    if (abs(steering) < 30) steering = 0;
    if (abs(throttle) < 30) throttle = 0;

    // Get current gamepad battery state
    gamepadBattery = ctl->battery();

    // Servo sweep limit (runs every 15ms for smooth transitions)
    if (millis() - lastServoUpdate > 15) {
        if (ctl->r1()) {
            servoAngle += 3;
            if (servoAngle > 180) servoAngle = 180;
            setServoAngle(servoAngle);
            lastServoUpdate = millis();
        } else if (ctl->l1()) {
            servoAngle -= 3;
            if (servoAngle < 0) servoAngle = 0;
            setServoAngle(servoAngle);
            lastServoUpdate = millis();
        }
    }

    // Celebration interrupt logic
    // R2 button acts as immediate cancellation/stop mechanism
    if (ctl->buttons() & 0x0080) { 
        currentCeleb = CELEB_NONE;
        stopMotors();
    } else {
        if (ctl->y()) { // Triangle pressed
            currentCeleb = CELEB_TRIANGLE;
            celebStartTime = millis();
        } else if (ctl->a()) { // Cross/X pressed
            currentCeleb = CELEB_CROSS;
            celebStartTime = millis();
        } else if (ctl->x()) { // Square pressed
            currentCeleb = CELEB_SQUARE;
            celebStartTime = millis();
        } else if (ctl->b()) { // Circle pressed
            currentCeleb = CELEB_CIRCLE;
            celebStartTime = millis();
        }
    }
}

//================= Loop Controller Processing =================
void processControllers() {
    for (auto ctl : myControllers) {
        if (ctl && ctl->isConnected() && ctl->hasData()) {
            processGamepad(ctl);
        }
    }
}

//================= Stop All Motors =================
void stopMotors() {
    digitalWrite(up, LOW);
    digitalWrite(down, LOW);
    digitalWrite(left, LOW);
    digitalWrite(right, LOW);

    ledcWrite(pwmChannelA, 0);
    ledcWrite(pwmChannelB, 0);
}

//================= Drive Individual Motors =================
void driveMotor(int leftMotor, int rightMotor) {
    // LEFT MOTOR DIRECTION
    if (leftMotor > 0) {
        digitalWrite(up, HIGH);
        digitalWrite(down, LOW);
    } else if (leftMotor < 0) {
        digitalWrite(up, LOW);
        digitalWrite(down, HIGH);
    } else {
        digitalWrite(up, LOW);
        digitalWrite(down, LOW);
    }

    // RIGHT MOTOR DIRECTION
    if (rightMotor > 0) {
        digitalWrite(left, HIGH);
        digitalWrite(right, LOW);
    } else if (rightMotor < 0) {
        digitalWrite(left, LOW);
        digitalWrite(right, HIGH);
    } else {
        digitalWrite(left, LOW);
        digitalWrite(right, LOW);
    }

    ledcWrite(pwmChannelA, abs(leftMotor));
    ledcWrite(pwmChannelB, abs(rightMotor));
}

//================= Execute Celebrations (Non-Blocking) =================
void runCelebration() {
    unsigned long elapsed = millis() - celebStartTime;

    // Halt celebration after 5 seconds
    if (elapsed >= CELEB_DURATION) {
        currentCeleb = CELEB_NONE;
        stopMotors();
        return;
    }

    int celebLeft = 0;
    int celebRight = 0;

    switch (currentCeleb) {
        case CELEB_TRIANGLE: // Alternate fast spins left and right
            if ((elapsed / 250) % 2 == 0) {
                celebLeft = -180;
                celebRight = 180;
            } else {
                celebLeft = 180;
                celebRight = -180;
            }
            break;

        case CELEB_CROSS: // Aggressive shuffling back and forth
            if ((elapsed / 300) % 2 == 0) {
                celebLeft = 200;
                celebRight = 200;
            } else {
                celebLeft = -200;
                celebRight = -200;
            }
            break;

        case CELEB_SQUARE: // Swaying dance (Wiggle side-to-side)
            if ((elapsed / 200) % 2 == 0) {
                celebLeft = 180;
                celebRight = 50;
            } else {
                celebLeft = 50;
                celebRight = 180;
            }
            break;

        case CELEB_CIRCLE: // Crazy Cyclone (Spin increasing in speed)
            {
                int cycle = elapsed % 1000;
                int dynamicSpeed = 120 + (cycle / 10); // Ramps up within cycle
                celebLeft = dynamicSpeed;
                celebRight = -dynamicSpeed;
            }
            break;

        default:
            stopMotors();
            break;
    }

    driveMotor(celebLeft, celebRight);
}

//================= Diagnostic Warnings & Blink Handler =================
void checkStatus() {
    // Determine critical alerts
    if (!isControllerConnected()) {
        hasWarning = true;
        warningMsg = "NO CONTROLLER";
    } else if (gamepadBattery == 1 || gamepadBattery == 2) { // Empty or low states
        hasWarning = true;
        warningMsg = "LOW BATTERY";
    } else if (loopTime > 40) { // Slow CPU loop warning
        hasWarning = true;
        warningMsg = "CPU SLOWDOWN";
    } else {
        hasWarning = false;
        warningMsg = "NONE";
    }

    // Toggle External LED if warning is flagged
    if (hasWarning) {
        if ((millis() / 250) % 2 == 0) {
            digitalWrite(RED_LED_PIN, HIGH);
        } else {
            digitalWrite(RED_LED_PIN, LOW);
        }
    } else {
        digitalWrite(RED_LED_PIN, LOW);
    }
}

//================= Refresh Screen Output =================
void updateDisplay(int lSpeed, int rSpeed) {
    if (millis() - lastDisplayUpdate < 200) return; // Refresh limit (5Hz)
    lastDisplayUpdate = millis();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Header bar
    display.setCursor(0, 0);
    display.print("--- SYS SYSTEM ---");

    // Line 1: Pad status and battery
    display.setCursor(0, 12);
    if (isControllerConnected()) {
        display.print("PAD: Con  Bat: ");
        switch (gamepadBattery) {
            case 1: display.print("Empty"); break;
            case 2: display.print("Low"); break;
            case 3: display.print("Med"); break;
            case 4: display.print("Full"); break;
            default: display.print("Unk"); break;
        }
    } else {
        display.print("PAD: Disconnected");
    }

    // Line 2: CPU Diagnostics
    display.setCursor(0, 24);
    display.printf("CPU:%dMHz  Lp:%dms", ESP.getCpuFreqMHz(), loopTime);

    // Line 3: Motor Output Speed
    display.setCursor(0, 36);
    display.printf("L-In: %d  R-In: %d", lSpeed, rSpeed);

    // Line 4: System Action Status or warning message
    if (hasWarning) {
        display.fillRect(0, 48, 128, 16, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, 52);
        display.print("WARN: " + warningMsg);
    } else {
        display.setCursor(0, 48);
        if (currentCeleb != CELEB_NONE) {
            display.print("STATE: CELEBRATING");
        } else if (lSpeed == 0 && rSpeed == 0) {
            display.print("STATE: IDLE");
        } else {
            display.print("STATE: DRIVING");
        }
    }

    display.display();
}

//================= Setup =================
void setup() {
    Serial.begin(115200);

    // Initialize custom alternate I2C pins for SSD1306
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("OLED allocation failed"));
    }
    display.clearDisplay();
    display.display();

    Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.enableVirtualDevice(false);

    // Motor driver hardware pins
    pinMode(up, OUTPUT);
    pinMode(down, OUTPUT);
    pinMode(left, OUTPUT);
    pinMode(right, OUTPUT);
    pinMode(enA, OUTPUT);
    pinMode(enB, OUTPUT);

    // Output Warning LED pin
    pinMode(RED_LED_PIN, OUTPUT);

    // Drive Motor PWM Setup
    ledcSetup(pwmChannelA, pwmFreq, pwmResolution);
    ledcAttachPin(enA, pwmChannelA);

    ledcSetup(pwmChannelB, pwmFreq, pwmResolution);
    ledcAttachPin(enB, pwmChannelB);

    // Servo Motor PWM Setup
    ledcSetup(servoChannel, servoFreq, servoResolution);
    ledcAttachPin(SERVO_PIN, servoChannel);
    setServoAngle(servoAngle); // Startup servo alignment

    stopMotors();
}

//================= Main Loop =================
void loop() {
    unsigned long loopStart = millis();

    BP32.update();
    processControllers();

    int leftMotor = 0;
    int rightMotor = 0;

    if (currentCeleb == CELEB_NONE) {
        // Standard Driving Operation
        int speed = map(throttle, -512, 512, -255, 255);
        int turn = map(steering, -512, 512, -255, 255);

        leftMotor = speed + turn;
        rightMotor = speed - turn;

        leftMotor = constrain(leftMotor, -255, 255);
        rightMotor = constrain(rightMotor, -255, 255);

        driveMotor(leftMotor, rightMotor);
    } else {
        // Suspends standard driving inputs when celebrations are active
        runCelebration();
    }

    // Diagnostics loop timer capture
    loopTime = millis() - loopStart;

    // Handle warning indicator checks
    checkStatus();

    // Refreshes diagnostic display values
    updateDisplay(leftMotor, rightMotor);

    delay(10);
}