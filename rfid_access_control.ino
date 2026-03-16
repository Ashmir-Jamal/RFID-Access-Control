/**
 * ============================================================
 *  RFID ACCESS CONTROL SYSTEM
 *  Hardware : Arduino Uno + RC522 RFID Module
 *  Author   : MD ASHMIR JAMAL
 *  Date     : 2024
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
 * @dependencies
 *   - MFRC522 Library by GithubCommunity
 *     Install via: Arduino IDE > Sketch > Include Library >
 *                  Manage Libraries > Search "MFRC522"
 *   - SPI Library (built-in with Arduino IDE)
 * 
 * @license     MIT License
 * 
 * ============================================================
 */

// ============================================================
//  LIBRARY INCLUSIONS
// ============================================================

#include <SPI.h>       // SPI communication protocol (used by RC522)
#include <MFRC522.h>   // RFID RC522 library for card reading


// ============================================================
//  PIN DEFINITIONS
// ============================================================

#define SS_PIN    10   // SDA/SS pin for RC522 (Slave Select for SPI)
#define RST_PIN   9    // Reset pin for RC522

#define GREEN_LED 7    // Green LED -> blinks on Access GRANTED
#define RED_LED   6    // Red LED   -> blinks on Access DENIED
#define BUZZER    5    // Buzzer    -> beeps as audio feedback
#define RELAY_PIN 4    // Relay pin -> controls door lock (optional)


// ============================================================
//  CONSTANTS & TIMING CONFIGURATION
// ============================================================

#define ACCESS_DELAY     2000   // Duration (ms) relay stays ON after access grant
#define DENIED_DELAY     1000   // Duration (ms) denied signal stays ON
#define BEEP_SHORT       100    // Short beep duration (ms) -- used for granted
#define BEEP_LONG        500    // Long beep duration  (ms) -- used for denied
#define SERIAL_BAUD      9600   // Serial monitor baud rate


// ============================================================
//  OBJECT INITIALIZATION
// ============================================================

/**
 * Create an MFRC522 instance.
 * This object handles all communication with the RC522 module
 * via the SPI bus using the defined SS and RST pins.
 */
MFRC522 mfrc522(SS_PIN, RST_PIN);


// ============================================================
//  AUTHORIZED UID DATABASE
// ============================================================

/**
 * Store the UIDs of all authorized RFID cards/tags here.
 *
 * HOW TO FIND YOUR CARD'S UID:
 *   1. Upload this sketch first.
 *   2. Open Serial Monitor (9600 baud).
 *   3. Scan your card -- the UID will be printed.
 *   4. Copy that UID into the array below.
 *
 * Each UID is a byte array. Most MIFARE Classic cards have
 * a 4-byte UID (e.g., {0xDE, 0xAD, 0xBE, 0xEF}).
 *
 * To add more cards, increase NUM_AUTHORIZED_CARDS and
 * add another byte array to authorizedUIDs[][].
 */

#define UID_BYTE_LENGTH       4    // Standard MIFARE card UID length in bytes
#define NUM_AUTHORIZED_CARDS  3    // Total number of authorized cards registered

// Replace these with your actual card UIDs after scanning them
byte authorizedUIDs[NUM_AUTHORIZED_CARDS][UID_BYTE_LENGTH] = {
  {0xA1, 0xB2, 0xC3, 0xD4},   // Card 1 -- e.g., Admin Card
  {0x11, 0x22, 0x33, 0x44},   // Card 2 -- e.g., Student Card
  {0xDE, 0xAD, 0xBE, 0xEF}    // Card 3 -- e.g., Faculty Card
};

// Labels for each authorized card (for Serial Monitor display)
String cardLabels[NUM_AUTHORIZED_CARDS] = {
  "Admin Card",
  "Student Card",
  "Faculty Card"
};


// ============================================================
//  FUNCTION PROTOTYPES (Declarations)
// ============================================================

void grantAccess(int cardIndex);
void denyAccess();
bool checkUID(byte *uid, byte uidSize, int &matchedIndex);
void printUID(byte *uid, byte uidSize);
void beep(int duration);
void blinkLED(int pin, int times, int delayMs);


// ============================================================
//  SETUP FUNCTION -- Runs once on power-up / reset
// ============================================================

void setup() {

  // --- Initialize Serial Communication ---
  // Allows us to print messages to the Arduino Serial Monitor
  // for debugging and displaying card scan results.
  Serial.begin(SERIAL_BAUD);

  // --- Initialize SPI Bus ---
  // The RC522 communicates with Arduino via the SPI protocol.
  // This must be called before initializing the RFID module.
  SPI.begin();

  // --- Initialize the RC522 RFID Module ---
  // Resets the module and prepares it to read cards.
  mfrc522.PCD_Init();

  // --- Configure Output Pins ---
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);
  pinMode(BUZZER,    OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // --- Ensure all outputs start in OFF state ---
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED,   LOW);
  digitalWrite(BUZZER,    LOW);
  digitalWrite(RELAY_PIN, LOW);   // Relay OFF = Door LOCKED

  // --- Startup Confirmation Message ---
  Serial.println("============================================");
  Serial.println("   RFID ACCESS CONTROL SYSTEM READY");
  Serial.println("============================================");
  Serial.print  ("  RC522 Firmware Version: 0x");
  Serial.println(mfrc522.PCD_ReadRegister(mfrc522.VersionReg), HEX);
  Serial.println("  Scan your RFID card to begin...");
  Serial.println("============================================");

  // Startup blink: both LEDs flash twice to confirm system is live
  blinkLED(GREEN_LED, 2, 200);
  blinkLED(RED_LED,   2, 200);
}


// ============================================================
//  LOOP FUNCTION -- Runs repeatedly after setup()
// ============================================================

void loop() {

  // --- Step 1: Check if a new card is present near the reader ---
  // PICC_IsNewCardPresent() returns true if an RFID card is detected.
  // If no card is nearby, skip the rest of the loop.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;   // No card detected -- keep polling
  }

  // --- Step 2: Try to read the card's UID ---
  // PICC_ReadCardSerial() reads the UID of the detected card.
  // Returns false if reading fails (e.g., card moved away too quickly).
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;   // Reading failed -- try again next loop iteration
  }

  // --- Step 3: Print the scanned card's UID to Serial Monitor ---
  Serial.println("\n--------------------------------------------");
  Serial.print("Card Detected! UID: ");
  printUID(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();

  // --- Step 4: Check if the scanned UID is in the authorized list ---
  int matchedIndex = -1;   // Will hold the index of matched card (-1 = no match)

  if (checkUID(mfrc522.uid.uidByte, mfrc522.uid.size, matchedIndex)) {
    // UID found in authorized list -- GRANT ACCESS
    grantAccess(matchedIndex);
  } else {
    // UID not found -- DENY ACCESS
    denyAccess();
  }

  // --- Step 5: Halt the current card and stop encryption ---
  // This is required to allow reading the NEXT card properly.
  // Without this, the same card would be read in a loop continuously.
  mfrc522.PICC_HaltA();         // Halt the current PICC (card)
  mfrc522.PCD_StopCrypto1();    // Stop encrypted communication

  Serial.println("--------------------------------------------");
  Serial.println("  Ready to scan next card...");
}


// ============================================================
//  FUNCTION: grantAccess()
// ============================================================
/**
 * @brief  Executes the Access Granted sequence.
 *
 * Called when an authorized card is detected.
 * - Prints a welcome message to Serial Monitor.
 * - Turns ON the Green LED.
 * - Activates the relay (unlocks the door).
 * - Gives two short beeps as audio confirmation.
 * - Keeps the door unlocked for ACCESS_DELAY milliseconds.
 * - Then re-locks the door and turns off the LED.
 *
 * @param cardIndex  Index of the matched card in authorizedUIDs[][]
 *                   Used to print the card's label.
 */
void grantAccess(int cardIndex) {

  Serial.println(">>> ACCESS GRANTED <<<");
  Serial.print  ("    Welcome! [");
  Serial.print  (cardLabels[cardIndex]);
  Serial.println("]");

  // Turn ON Green LED
  digitalWrite(GREEN_LED, HIGH);

  // Activate relay to unlock the door / trigger the lock mechanism
  digitalWrite(RELAY_PIN, HIGH);

  // Two short beeps -- pleasant audio confirmation
  beep(BEEP_SHORT);
  delay(100);
  beep(BEEP_SHORT);

  // Keep door unlocked for the defined delay period
  delay(ACCESS_DELAY);

  // Deactivate relay -- door re-locks
  digitalWrite(RELAY_PIN, LOW);

  // Turn OFF Green LED
  digitalWrite(GREEN_LED, LOW);
}


// ============================================================
//  FUNCTION: denyAccess()
// ============================================================
/**
 * @brief  Executes the Access Denied sequence.
 *
 * Called when an unrecognized card is detected.
 * - Prints an access denied message to Serial Monitor.
 * - Turns ON the Red LED.
 * - Gives one long beep as a warning tone.
 * - Keeps the red LED on for DENIED_DELAY milliseconds.
 * - Then turns off the LED.
 */
void denyAccess() {

  Serial.println(">>> ACCESS DENIED <<<");
  Serial.println("    Unauthorized card detected!");

  // Turn ON Red LED
  digitalWrite(RED_LED, HIGH);

  // One long beep -- warning tone
  beep(BEEP_LONG);

  // Keep denied signal active for the defined delay
  delay(DENIED_DELAY);

  // Turn OFF Red LED
  digitalWrite(RED_LED, LOW);
}


// ============================================================
//  FUNCTION: checkUID()
// ============================================================
/**
 * @brief  Compares a scanned UID against the authorized UID list.
 *
 * Iterates through authorizedUIDs[][] and compares each stored
 * UID byte-by-byte with the scanned card's UID.
 *
 * @param uid           Pointer to the scanned card's UID byte array
 * @param uidSize       Number of bytes in the scanned UID
 * @param matchedIndex  Reference variable -- set to the index of the
 *                      matched card if found; remains -1 if not found
 *
 * @return  true  if UID matches any authorized card
 *          false if no match found
 */
bool checkUID(byte *uid, byte uidSize, int &matchedIndex) {

  // Only compare if UID length matches expected length
  if (uidSize != UID_BYTE_LENGTH) {
    return false;
  }

  // Loop through all registered authorized cards
  for (int i = 0; i < NUM_AUTHORIZED_CARDS; i++) {

    bool match = true;   // Assume match; disprove byte-by-byte

    // Compare each byte of the scanned UID with the stored UID
    for (int j = 0; j < UID_BYTE_LENGTH; j++) {
      if (uid[j] != authorizedUIDs[i][j]) {
        match = false;   // Mismatch found -- this card is not a match
        break;
      }
    }

    // If all bytes matched, card is authorized
    if (match) {
      matchedIndex = i;   // Store which card index matched
      return true;
    }
  }

  // No match found after checking all authorized cards
  return false;
}


// ============================================================
//  FUNCTION: printUID()
// ============================================================
/**
 * @brief  Prints a card UID to the Serial Monitor in HEX format.
 *
 * Iterates through each byte of the UID and prints it as a
 * two-digit hexadecimal value separated by spaces.
 * Example output: "A1 B2 C3 D4"
 *
 * @param uid      Pointer to the UID byte array
 * @param uidSize  Number of bytes in the UID
 */
void printUID(byte *uid, byte uidSize) {

  for (byte i = 0; i < uidSize; i++) {

    // Print leading zero for single-digit hex values (e.g., "0F" not "F")
    if (uid[i] < 0x10) {
      Serial.print("0");
    }

    Serial.print(uid[i], HEX);   // Print byte as uppercase HEX

    if (i < uidSize - 1) {
      Serial.print(" ");          // Add space between bytes
    }
  }
}


// ============================================================
//  FUNCTION: beep()
// ============================================================
/**
 * @brief  Activates the buzzer for a specified duration.
 *
 * Turns the buzzer ON, waits for the given duration, then
 * turns it OFF. Acts as a simple blocking beep function.
 *
 * @param duration  Duration of the beep in milliseconds
 */
void beep(int duration) {
  digitalWrite(BUZZER, HIGH);   // Buzzer ON
  delay(duration);               // Wait for specified time
  digitalWrite(BUZZER, LOW);    // Buzzer OFF
}


// ============================================================
//  FUNCTION: blinkLED()
// ============================================================
/**
 * @brief  Blinks a specified LED a given number of times.
 *
 * Used for visual startup confirmation and status signaling.
 * Each blink consists of one ON period and one OFF period,
 * both of length delayMs milliseconds.
 *
 * @param pin      The Arduino pin number connected to the LED
 * @param times    Number of times to blink the LED
 * @param delayMs  Duration (ms) for each ON and OFF phase
 */
void blinkLED(int pin, int times, int delayMs) {

  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);    // LED ON
    delay(delayMs);
    digitalWrite(pin, LOW);     // LED OFF
    delay(delayMs);
  }
}


// ============================================================
//  END OF FILE
// ============================================================
/**
 * ============================================================
 *  FUTURE IMPROVEMENTS / EXTENSIONS:
 * ============================================================
 *
 *  1. LCD Display    -- Add a 16x2 I2C LCD to show "Welcome!"
 *                       or "Access Denied" messages on screen.
 *
 *  2. EEPROM Storage -- Store authorized UIDs in EEPROM so they
 *                       persist even after power loss.
 *
 *  3. Master Card    -- Implement an enrollment mode where a
 *                       special master card adds new UIDs
 *                       dynamically without reprogramming.
 *
 *  4. Logging        -- Add an SD card module to log every
 *                       access attempt with timestamps.
 *
 *  5. RTC Module     -- Use a DS3231 Real-Time Clock to add
 *                       time-based access restrictions
 *                       (e.g., access only between 9am-5pm).
 *
 *  6. Wi-Fi / IoT    -- Replace Arduino Uno with an ESP32 and
 *                       send access logs to a cloud dashboard.
 *
 * ============================================================
 */
