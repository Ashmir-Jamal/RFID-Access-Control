# 🔐 RFID Access Control System
### Arduino Uno + RC522 RFID Reader

Working video link: https://youtu.be/L6WhyPVYpMc?si=B3zTktvDBDAFQbEY
![Arduino](https://img.shields.io/badge/Platform-Arduino%20Uno-00979D?logo=arduino&logoColor=white)
![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus&logoColor=white)
![Module](https://img.shields.io/badge/Module-RC522%20RFID-blueviolet)
![License](https://img.shields.io/badge/License-MIT-green)

A fully functional **RFID-based door access control system** built for a college project using an **Arduino Uno** and the popular **MFRC522 (RC522)** RFID reader module. Scan a card — if authorised, the door unlocks; if not, an alarm triggers.

---

## 📸 System Overview

```
[ RFID Card ] ──scan──> [ RC522 Module ] ──SPI──> [ Arduino Uno ]
                                                          │
                              ┌───────────────────────────┤
                              │                           │
                        Authorised?                  Not Authorised?
                              │                           │
                    ┌─────────▼──────────┐     ┌─────────▼──────────┐
                    │ ✅ Green LED ON     │     │ ❌ Red LED ON       │
                    │ 🔔 1 Beep           │     │ 🚨 3 Beeps          │
                    │ 🔓 Servo Unlocks    │     │ 🔒 Servo Stays Lock │
                    │ 📟 "Access Granted" │     │ 📟 "Access Denied"  │
                    └────────────────────┘     └────────────────────┘
```

---

## 🧰 Components Required

| Component | Quantity |
|---|---|
| Arduino Uno | 1 |
| RC522 RFID Reader Module | 1 |
| RFID Cards / Key Fobs | 2–3 |
| SG90 Servo Motor | 1 |
| 16×2 LCD with I2C Module | 1 |
| Green LED | 1 |
| Red LED | 1 |
| Active Buzzer | 1 |
| 220Ω Resistors | 2 |
| Jumper Wires | Several |
| Breadboard | 1 |

---

## 🔌 Wiring / Circuit Diagram

### RC522 → Arduino Uno
| RC522 Pin | Arduino Pin |
|---|---|
| SDA (SS) | 10 |
| SCK | 13 |
| MOSI | 11 |
| MISO | 12 |
| IRQ | Not Connected |
| GND | GND |
| RST | 9 |
| 3.3V | 3.3V |

### Other Components
| Component | Arduino Pin |
|---|---|
| Green LED (+ 220Ω) | 6 |
| Red LED (+ 220Ω) | 7 |
| Active Buzzer | 8 |
| Servo Motor Signal | 5 |
| LCD SDA (I2C) | A4 |
| LCD SCL (I2C) | A5 |

> ⚠️ **Important:** The RC522 module runs on **3.3V**, NOT 5V. Powering it with 5V will damage the module.

---

## 📚 Required Libraries

Install the following libraries via **Arduino IDE → Sketch → Include Library → Manage Libraries**:

| Library | Author |
|---|---|
| `MFRC522` | GithubCommunity |
| `LiquidCrystal_I2C` | Frank de Brabander |
| `Servo` | Built-in (Arduino) |
| `Wire` | Built-in (Arduino) |
| `SPI` | Built-in (Arduino) |

---

## 🚀 How to Set Up & Use

### Step 1 – Find Your Card's UID
1. Upload the sketch to Arduino
2. Open **Serial Monitor** (baud rate: `9600`)
3. Scan your RFID card
4. Note the UID printed, e.g., `A3 4F 2B 11`

### Step 2 – Add Authorised UIDs
Open `rfid_access_control.ino` and edit this section:

```cpp
const byte AUTHORISED_UIDS[][4] = {
  { 0xA3, 0x4F, 0x2B, 0x11 },  // Card 1 – Your card UID here
  { 0xDE, 0xAD, 0xBE, 0xEF },  // Card 2 – Add more as needed
};
```

Replace the hex values with your actual card UIDs.

### Step 3 – Upload & Test
1. Connect all components as per the wiring table
2. Upload the sketch to Arduino Uno
3. Scan an authorised card → door unlocks ✅
4. Scan an unknown card → access denied ❌

---

## ⚙️ Configurable Parameters

You can easily tweak these `#define` values at the top of the sketch:

```cpp
#define LOCK_ANGLE    0     // Servo angle when LOCKED (degrees)
#define UNLOCK_ANGLE  90    // Servo angle when UNLOCKED (degrees)
#define ACCESS_DELAY  3000  // How long door stays unlocked (milliseconds)
```

---

## 📂 Project Structure

```
rfid-access-control/
│
├── rfid_access_control.ino   ← Main Arduino sketch (upload this)
└── README.md                 ← This file
```

---

## 🧠 How It Works – Code Flow

```
setup()
  └── Initialise SPI, RC522, Servo, LCD, Pins
  └── Lock the door (servo to 0°)
  └── Display standby message on LCD

loop()
  └── Poll RC522 for new card
  └── If card detected → read UID
      └── isAuthorised(uid) ?
            YES → grantAccess()
                  → Green LED, 1 beep, unlock servo, LCD message
                  → Wait 3 seconds
                  → Lock servo again
            NO  → denyAccess()
                  → Red LED, 3 beeps, LCD message
                  → Wait 2 seconds
  └── Halt card, reset LCD, repeat
```

---

## 📄 License

This project is open-source under the [MIT License](LICENSE).  
Feel free to use, modify, and distribute for educational purposes.

---

## 🙋 Author

**[Your Name]**  
College Project — RFID Access Control System  
Department of [Your Department] | [Your College Name] | [Year]

---

> 💡 **Tip:** To add more authorised cards in the future, simply scan them (UID will print to Serial Monitor) and add the bytes to the `AUTHORISED_UIDS` array — no other code changes needed!
