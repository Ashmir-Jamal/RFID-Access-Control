/**
 * ============================================================
 *  RFID ACCESS CONTROL SYSTEM
 *  Hardware : Arduino Uno + RC522 RFID Module
 *  Author   : [Your Name]
 *  Date     : 2025
 *  Version  : 1.0
 * ============================================================
 *
 *  DESCRIPTION:
 *  This project implements an RFID-based access control system
 *  using an Arduino Uno and the MFRC522 RFID reader module.
 *  When an RFID card/tag is scanned:
 *    - If the UID matches an authorised card → GREEN LED ON,
 *      Buzzer beeps once, Servo unlocks the door, LCD shows "Access Granted"
 *    - If the UID does NOT match              → RED LED ON,
 *      Buzzer beeps three times, Servo stays locked, LCD shows "Access Denied"
 *
 *  WIRING SUMMARY:
 *  ┌─────────────┬──────────────────┐
 *  │  RC522 Pin  │  Arduino Uno Pin │
 *  ├─────────────┼──────────────────┤
 *  │  SDA (SS)   │  10              │
 *  │  SCK        │  13              │
 *  │  MOSI       │  11              │
 *  │  MISO       │  12              │
 *  │  IRQ        │  (not used)      │
 *  │  GND        │  GND             │
 *  │  RST        │  9               │
 *  │  3.3V       │  3.3V            │
 *  └─────────────┴──────────────────┘
 *
 *  Other connections:
 *    Green LED  → Pin 6 (through 220Ω resistor)
 *    Red LED    → Pin 7 (through 220Ω resistor)
 *    Buzzer     → Pin 8
 *    Servo      → Pin 5
 *    LCD (I2C)  → SDA → A4, SCL → A5 (address 0x27)
 *
 *  LIBRARIES REQUIRED (install via Arduino Library Manager):
 *    1. MFRC522  by GithubCommunity
 *    2. Servo    (built-in)
 *    3. LiquidCrystal_I2C  by Frank de Brabander
 *    4. Wire     (built-in)
 * ============================================================
 */

// ─── Library Includes ────────────────────────────────────────
#include <SPI.h>               // SPI communication (used by RC522)
#include <MFRC522.h>           // RC522 RFID reader library
#include <Servo.h>             // Servo motor control
#include <Wire.h>              // I2C communication (used by LCD)
#include <LiquidCrystal_I2C.h> // I2C LCD library

// ─── Pin Definitions ─────────────────────────────────────────
#define SS_PIN    10   // RC522 Slave Select (SDA) pin
#define RST_PIN   9    // RC522 Reset pin
#define GREEN_LED 6    // Green LED → Access Granted indicator
#define RED_LED   7    // Red LED   → Access Denied indicator
#define BUZZER    8    // Buzzer pin (active buzzer recommended)
#define SERVO_PIN 5    // Servo signal pin

// ─── Constants ────────────────────────────────────────────────
#define LOCK_ANGLE    0    // Servo angle when door is LOCKED
#define UNLOCK_ANGLE  90   // Servo angle when door is UNLOCKED
#define ACCESS_DELAY  3000 // Time (ms) door stays unlocked after access

// ─── Object Instantiation ────────────────────────────────────
MFRC522 rfid(SS_PIN, RST_PIN);   // Create RC522 RFID object
Servo   doorServo;                // Create Servo object
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD at I2C address 0x27, 16 cols × 2 rows

// ─── Authorised Card UIDs ─────────────────────────────────────
/*
 *  Add the UID bytes of every authorised RFID card/tag here.
 *  Each UID is typically 4 bytes (shown in HEX).
 *  You can find a card's UID by running the ReadUID example
 *  sketch that comes with the MFRC522 library, or by opening
 *  the Serial Monitor after uploading this sketch and scanning
 *  an unknown card – the UID will be printed for you.
 *
 *  Format: { 0xXX, 0xXX, 0xXX, 0xXX }
 */
const byte AUTHORISED_UIDS[][4] = {
  { 0xA3, 0x4F, 0x2B, 0x11 },  // Card 1 – e.g. Student ID
  { 0xDE, 0xAD, 0xBE, 0xEF },  // Card 2 – e.g. Faculty Card
  { 0x12, 0x34, 0x56, 0x78 }   // Card 3 – e.g. Admin Fob
};

// Calculate the total number of authorised cards automatically
const byte TOTAL_AUTHORISED = sizeof(AUTHORISED_UIDS) / sizeof(AUTHORISED_UIDS[0]);

// ─── Function Prototypes ──────────────────────────────────────
bool  isAuthorised(byte *uid, byte uidSize);
void  grantAccess();
void  denyAccess();
void  beep(int times, int duration, int gap);
void  showLCD(String line1, String line2);
void  printUID(byte *uid, byte uidSize);
void  lockDoor();
void  unlockDoor();

// =============================================================
//  SETUP  – runs once when Arduino is powered on / reset
// =============================================================
void setup() {

  // ── Serial Monitor (for debugging / UID discovery) ────────
  Serial.begin(9600);
  Serial.println(F("------------------------------------"));
  Serial.println(F("  RFID Access Control System Ready  "));
  Serial.println(F("------------------------------------"));

  // ── SPI Bus Init (required by RC522) ─────────────────────
  SPI.begin();

  // ── RC522 Module Init ─────────────────────────────────────
  rfid.PCD_Init();
  Serial.println(F("RC522 initialised. Scan a card..."));

  // ── Pin Modes ─────────────────────────────────────────────
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);
  pinMode(BUZZER,    OUTPUT);

  // ── Servo Init ────────────────────────────────────────────
  doorServo.attach(SERVO_PIN);
  lockDoor(); // Ensure door starts in LOCKED position

  // ── LCD Init ──────────────────────────────────────────────
  lcd.init();
  lcd.backlight();
  showLCD("  RFID  System  ", "  Scan Your Card");

  // ── Startup Beep (system ready indicator) ─────────────────
  beep(1, 100, 0);
  delay(500);
}

// =============================================================
//  LOOP  – runs repeatedly after setup()
// =============================================================
void loop() {

  // ── Wait for a new card to be present in the RF field ─────
  // rfid.PICC_IsNewCardPresent() returns true when a card is detected
  // rfid.PICC_ReadCardSerial()   returns true when UID is successfully read
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return; // No card detected → restart loop
  }

  // ── Card Detected: Print UID to Serial Monitor ────────────
  Serial.print(F("Card UID detected: "));
  printUID(rfid.uid.uidByte, rfid.uid.size);

  // ── Check if this card is authorised ──────────────────────
  if (isAuthorised(rfid.uid.uidByte, rfid.uid.size)) {
    grantAccess();
  } else {
    denyAccess();
  }

  // ── Halt the card and stop encryption to allow next scan ──
  rfid.PICC_HaltA();       // Put card into HALT state
  rfid.PCD_StopCrypto1();  // Stop the encryption on the PCD

  // ── Reset LCD to standby message ──────────────────────────
  showLCD("  RFID  System  ", "  Scan Your Card");
}

// =============================================================
//  FUNCTION: isAuthorised
//  PURPOSE : Compares the scanned card UID against every entry
//            in the AUTHORISED_UIDS array.
//  PARAMS  : uid      – pointer to the UID byte array
//            uidSize  – number of bytes in the UID
//  RETURNS : true  → card is authorised
//            false → card is NOT authorised
// =============================================================
bool isAuthorised(byte *uid, byte uidSize) {

  // Only 4-byte UIDs are stored in our authorised list
  if (uidSize != 4) return false;

  // Iterate over each authorised UID
  for (byte card = 0; card < TOTAL_AUTHORISED; card++) {
    bool match = true;

    // Compare each byte of the scanned UID with the stored UID
    for (byte i = 0; i < 4; i++) {
      if (uid[i] != AUTHORISED_UIDS[card][i]) {
        match = false; // Byte mismatch → not this card
        break;
      }
    }

    if (match) return true; // All 4 bytes matched → authorised!
  }

  return false; // No match found in authorised list
}

// =============================================================
//  FUNCTION: grantAccess
//  PURPOSE : Actions performed when an authorised card is scanned
//             • Turns ON green LED
//             • Single beep
//             • Unlocks door (servo rotates to UNLOCK_ANGLE)
//             • Displays "Access Granted" on LCD
//             • Waits ACCESS_DELAY ms, then re-locks
// =============================================================
void grantAccess() {
  Serial.println(F(">> ACCESS GRANTED <<"));

  // Visual indicator
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED,   LOW);

  // Audio indicator – one short beep
  beep(1, 200, 0);

  // LCD feedback
  showLCD(">> WELCOME! <<  ", "Access  Granted ");

  // Physically unlock the door
  unlockDoor();

  // Keep door unlocked for ACCESS_DELAY milliseconds
  delay(ACCESS_DELAY);

  // Lock the door again
  lockDoor();

  // Turn off green LED
  digitalWrite(GREEN_LED, LOW);
}

// =============================================================
//  FUNCTION: denyAccess
//  PURPOSE : Actions performed when an unauthorised card is scanned
//             • Turns ON red LED
//             • Three rapid beeps (alarm pattern)
//             • Door stays LOCKED
//             • Displays "Access Denied" on LCD
// =============================================================
void denyAccess() {
  Serial.println(F(">> ACCESS DENIED <<"));

  // Visual indicator
  digitalWrite(RED_LED,   HIGH);
  digitalWrite(GREEN_LED, LOW);

  // Audio indicator – three beeps (warning pattern)
  beep(3, 150, 100);

  // LCD feedback
  showLCD("!! DENIED !!    ", "Unauthorised Card");

  // Hold the denied state briefly so the user can read the LCD
  delay(2000);

  // Turn off red LED
  digitalWrite(RED_LED, LOW);
}

// =============================================================
//  FUNCTION: unlockDoor
//  PURPOSE : Rotates the servo to the UNLOCKED position
// =============================================================
void unlockDoor() {
  doorServo.write(UNLOCK_ANGLE);
  Serial.println(F("Door: UNLOCKED"));
  delay(500); // Small delay for servo to reach position
}

// =============================================================
//  FUNCTION: lockDoor
//  PURPOSE : Rotates the servo back to the LOCKED position
// =============================================================
void lockDoor() {
  doorServo.write(LOCK_ANGLE);
  Serial.println(F("Door: LOCKED"));
  delay(500); // Small delay for servo to reach position
}

// =============================================================
//  FUNCTION: beep
//  PURPOSE : Generates a beep pattern on the buzzer
//  PARAMS  : times    – how many beeps to produce
//            duration – length of each beep in milliseconds
//            gap      – pause between beeps in milliseconds
// =============================================================
void beep(int times, int duration, int gap) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER, HIGH); // Buzzer ON
    delay(duration);
    digitalWrite(BUZZER, LOW);  // Buzzer OFF
    if (i < times - 1) {
      delay(gap);               // Pause between beeps (skip after last)
    }
  }
}

// =============================================================
//  FUNCTION: showLCD
//  PURPOSE : Clears the LCD and prints two lines of text
//  PARAMS  : line1 – string for the first  row (max 16 chars)
//            line2 – string for the second row (max 16 chars)
// =============================================================
void showLCD(String line1, String line2) {
  lcd.clear();

  lcd.setCursor(0, 0); // Column 0, Row 0
  lcd.print(line1);

  lcd.setCursor(0, 1); // Column 0, Row 1
  lcd.print(line2);
}

// =============================================================
//  FUNCTION: printUID
//  PURPOSE : Prints the scanned card's UID to the Serial Monitor
//            in HEX format, e.g.  A3 4F 2B 11
//  PARAMS  : uid     – pointer to the UID byte array
//            uidSize – number of bytes in the UID
// =============================================================
void printUID(byte *uid, byte uidSize) {
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] < 0x10) Serial.print(F("0")); // Leading zero for single-digit hex
    Serial.print(uid[i], HEX);
    if (i < uidSize - 1) Serial.print(F(" "));
  }
  Serial.println(); // New line after full UID
}
