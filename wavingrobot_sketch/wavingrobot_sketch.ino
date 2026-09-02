#include <SPI.h>

const int csPin = 5; // GP5 on RP2040-Zero

uint8_t invert_setting = 1; // Default starting assumption
bool system_trained = false;

// Statistics gathering variables
int count_ones = 0;
int count_zeros = 0;
int sample_index = 0;
const int TOTAL_SAMPLES = 50; // We will take 50 samples on boot

void setup() {
  Serial.begin(115200);
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 2000));

  Serial.println("=========================================");
  Serial.println("  SELF-SUPERVISED REFLEX TRAINING STARTING");
  Serial.println("  The Child is observing the environment...");
  Serial.println("=========================================");
  Serial.println("Keep the sensor clear during this 5-second boot window.");

  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, HIGH);

  SPI.setRX(4);  // MISO to GP4
  SPI.setTX(3);  // MOSI to GP3
  SPI.setSCK(2);  // SCLK to GP2
  SPI.begin();
}

void loop() {
  // Phase 1: Self-Calibration (Unsupervised Learning)
  if (sample_index < TOTAL_SAMPLES) {
    // Read the sensor via SPI
    digitalWrite(csPin, LOW);
    SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    uint8_t child_response = SPI.transfer(invert_setting);
    SPI.endTransaction();
    digitalWrite(csPin, HIGH);

    uint8_t raw_ir_sensor = child_response & 0x01;

    // Log the statistical sample
    if (raw_ir_sensor == 1) {
      count_ones++;
    } else {
      count_zeros++;
    }

    sample_index++;

    // Print a visual progress bar
    Serial.print("Gathering data: [");
    for (int i = 0; i < TOTAL_SAMPLES; i++) {
      if (i < sample_index) Serial.print("|");
      else Serial.print(".");
    }
    Serial.println("]");

    delay(100); // Sample every 100ms (5 seconds total)
    return;
  }

  // Phase 2: Analyze the gathered statistics (One-Time Execution)
  if (!system_trained) {
    Serial.println("\nAnalyzing environment statistics...");
    Serial.print("Total Samples: "); Serial.println(TOTAL_SAMPLES);
    Serial.print("-> Observed '1' (HIGH) state: "); Serial.print(count_ones); Serial.println(" times");
    Serial.print("-> Observed '0' (LOW) state: "); Serial.print(count_zeros); Serial.println(" times");

    // Make the deduction based on the dominant state
    if (count_ones > count_zeros) {
      // 1 was dominant, so 1 = CLEAR, meaning 0 = OBSTACLE. (Active-Low Sensor)
      invert_setting = 0; 
      Serial.println("\nDeduction: '1' is the clear state. '0' is the obstacle.");
      Serial.println("Result: This is an ACTIVE-LOW sensor.");
    } else {
      // 0 was dominant, so 0 = CLEAR, meaning 1 = OBSTACLE. (Active-High Sensor)
      invert_setting = 1; 
      Serial.println("\nDeduction: '0' is the clear state. '1' is the obstacle.");
      Serial.println("Result: This is an ACTIVE-HIGH sensor.");
    }

    // Program the final decision into the FPGA
    digitalWrite(csPin, LOW);
    SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    SPI.transfer(invert_setting);
    SPI.endTransaction();
    digitalWrite(csPin, HIGH);

    system_trained = true;
    Serial.println("\n=======================================================");
    Serial.println(">>> TRAINING SUCCESSFUL (Autonomous Self-Calibration)!");
    Serial.println(">>> The Child's synaptic reflex logic is locked.");
    Serial.println(">>> You can now unplug the Parent!");
    Serial.println("=======================================================\n");
  }

  // Phase 3: Active Monitoring
  digitalWrite(csPin, LOW);
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
  uint8_t child_response = SPI.transfer(invert_setting);
  SPI.endTransaction();
  digitalWrite(csPin, HIGH);

  uint8_t raw_ir_sensor = child_response & 0x01;

  Serial.print("Live Monitor: [");
  if (raw_ir_sensor == invert_setting) {
    Serial.println("OBSTACLE DETECTED - ALARM!]");
  } else {
    Serial.println("CLEAR - SAFE]");
  }

  delay(500);
}