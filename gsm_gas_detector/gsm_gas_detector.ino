#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// LCD Setup (try 0x27 or 0x20)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// GSM Setup
SoftwareSerial gsm(2, 3);  // RX = D2, TX = D3

// MQ-5 Sensor
const int mq5Pin = A0;
const int gasThreshold = 300;

// ============================================
// PHONE NUMBERS - UPDATE THESE!
// ============================================
// The SIM in the GSM module is AIRTEL
String senderNumber = "+254100548762";    // ← Your Airtel SIM number

// This is your Safaricom number to receive alerts
String recipientNumber = "+254799256626";  // ← YOUR SAFARICOM NUMBER

int gasValue = 0;
bool alarmSent = false;

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("Gas Detector");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  
  lcd.clear();
  lcd.print("GSM Starting...");
  initGSM();
  
  lcd.clear();
  lcd.print("System Ready!");
  delay(1000);
  lcd.clear();
}

void loop() {
  gasValue = analogRead(mq5Pin);
  
  lcd.setCursor(0, 0);
  lcd.print("Gas: ");
  lcd.print(gasValue);
  lcd.print("   ");
  
  if (gasValue > gasThreshold) {
    lcd.setCursor(0, 1);
    lcd.print("DANGER! GAS LEAK");
    
    if (!alarmSent) {
      sendSMS("DANGER! LPG/Gas leak detected! Please check immediately.");
      alarmSent = true;
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Safe      ");
    alarmSent = false;
  }
  
  delay(500);
}

void initGSM() {
  gsm.println("AT");
  delay(1000);
  gsm.println("AT+CMGF=1");
  delay(1000);
  gsm.println("AT+CNMI=2,2,0,0,0");
  delay(1000);
  
  if (gsm.find("OK")) {
    lcd.print("GSM Ready!");
  } else {
    lcd.print("GSM Error!");
  }
  delay(2000);
}

void sendSMS(String message) {
  lcd.clear();
  lcd.print("Sending SMS...");
  
  gsm.println("AT+CMGF=1");
  delay(500);
  
  gsm.print("AT+CMGS=\"");
  gsm.print(recipientNumber);
  gsm.println("\"");
  delay(500);
  
  gsm.print(message);
  delay(500);
  
  gsm.write(26);  // Ctrl+Z to send
  delay(5000);
  
  lcd.clear();
  lcd.print("SMS Sent!");
  delay(2000);
}