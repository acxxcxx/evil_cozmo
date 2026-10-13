<div align="center">

# ⚡ EVIL COZMO v10 ⚡
### Modular ESP32 Multi-Protocol WiFi, BLE & 2.4GHz RF Offensive Security Tool

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32%20WROOM--32-blue.svg)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979C.svg)](https://www.arduino.cc/)
[![Display](https://img.shields.io/badge/Display-SSD1306%20OLED%20128x64-purple.svg)](https://github.com/olikraus/u8g2)
[![RF Transceiver](https://img.shields.io/badge/Transceiver-Dual%20NRF24L01%2B-orange.svg)](https://github.com/nRF24/RF24)

<p align="center">
  <b>Evil COZMO v10</b> is a portable offensive security and penetration testing toolkit powered by the ESP32 microcontroller with an interactive OLED HUD (Cozmo robot face expressions). Designed with modular architecture supporting 802.11 b/g/n WiFi attacks, Bluetooth Low Energy (BLE), and 2.4 GHz RF transmission via dual NRF24L01+ modules.
</p>

[Key Features](#-key-features) • [Hardware & Pinout](#-hardware-pinout-configuration) • [Arduino IDE Setup](#-compilation--installation-arduino-ide) • [Flashing Firmware](#-flashing-precompiled-firmware) • [Controls & Navigation](#-controls--navigation) • [Disclaimer](#-disclaimer)

</div>

---

## 🚀 Key Features

### 📡 WiFi Attacks & Analysis (Marauder Engine)
* **WiFi Scanner & RSSI Tracker:** Scans 2.4GHz APs on channels 1–13, reveals hidden SSIDs, and sorts targets by signal strength.
* **Burst Deauth & Disassociation:** Advanced connection disruptor injecting multi-reason codes (`0x02`, `0x08`, `0x0A`) and Disassociation frames (`0xA0`) with ESP32 raw frame sanity bypass.
* **Area Denial (Channel Sweep):** Rapid multi-channel sweeping attack across WiFi channels 1–13.
* **Evil Twin + Captive Portal:** Clones target SSID, spawns a rogue AP, and captures WiFi credentials using a mobile-optimized captive portal login page.
* **Beacon Flooder:** Broadcasts hundreds of fake SSIDs simultaneously across nearby airspace.
* **PMKID Sniffer:** Passively captures PMKID / EAPOL 4-way handshakes for offline WPA/WPA2 cracking.
* **WiFi Spectrum Analyzer:** Real-time 2.4GHz channel utilization and interference graph.
* **WiFi Extender:** Portable range extender and relay mode.

### 📶 2.4 GHz ISM & NRF24L01+ Transceiver
* **Dual Radio Support:** Operates two nRF24L01+ modules simultaneously (VSPI & HSPI).
* **Bluetooth Jammer:** Sweeps Bluetooth channel spectrum (2400 – 2478 MHz).
* **Drone Jammer:** Full-spectrum ISM interference (2400 – 2525 MHz).
* **WiFi NRF Jammer:** High-energy carrier RF injection targeting WiFi channels.
* **MouseJack & Keystroke Injection:** Injects wireless keystrokes into vulnerable 2.4GHz RF transceivers (Logitech Unifying / Microsoft / Generic).
* **NRF Sniffer / Target Scan:** Scans and sniffs wireless mouse & keyboard packets.

### 📱 Bluetooth Classic & UI System
* **Cozmo Robot HUD:** Animated robot face expressions on a 128x64 SSD1306 OLED display.
* **Companion App Remote Control:** Trigger attacks and navigate menus remotely via Serial Bluetooth Terminal on Android.
* **Stealth Notification Spoofing:** Displays simulated turn-by-turn navigation prompts (`NAV:...`) and WhatsApp alerts (`WA:...`) on screen.
* **Smart Power Management:** Auto-sleep with low-contrast standby mode, wakes up instantly with a single button press.

---

## 🔌 Hardware Pinout Configuration

### 1. SSD1306 OLED Display (I2C)
| ESP32 Pin | OLED Pin | Description |
| :--- | :--- | :--- |
| **GPIO 21** | SDA | I2C Serial Data |
| **GPIO 22** | SCL | I2C Serial Clock |
| **3.3V** | VCC | 3.3V Power Supply |
| **GND** | GND | Ground |

### 2. Navigation Push Buttons
Connect with 10kΩ pull-up resistors to 3.3V (or internal pull-up):
| ESP32 Pin | Button | Action |
| :--- | :--- | :--- |
| **GPIO 4** | BTN_NEXT | Scroll down / Select next menu item |
| **GPIO 2** | BTN_SELECT | Confirm / Execute / Stop (Hold to exit) |

### 3. NRF24L01+ Module #1 (VSPI - Primary Radio)
> ⚠️ **IMPORTANT:** Solder a 10µF–100µF electrolytic capacitor directly between the nRF24L01+ VCC and GND pins to prevent brownouts.
| ESP32 Pin | NRF24 Pin | Description |
| :--- | :--- | :--- |
| **GPIO 5** | CE | Chip Enable |
| **GPIO 17** | CSN | SPI Chip Select |
| **GPIO 18** | SCK | SPI Clock |
| **GPIO 19** | MISO | SPI Master In Slave Out |
| **GPIO 23** | MOSI | SPI Master Out Slave In |
| **3.3V** | VCC | 3.3V Power *(Do NOT connect to 5V!)* |
| **GND** | GND | Common Ground |

### 4. NRF24L01+ Module #2 (HSPI - Secondary Radio, Optional)
| ESP32 Pin | NRF24 Pin | Description |
| :--- | :--- | :--- |
| **GPIO 16** | CE | Radio 2 Chip Enable |
| **GPIO 27** | CSN | Radio 2 Chip Select |
| **GPIO 14** | SCK | HSPI Clock |
| **GPIO 12** | MISO | HSPI MISO |
| **GPIO 13** | MOSI | HSPI MOSI |

### 5. MicroSD Card Adapter (VSPI - Optional for PCAP capture)
* **CS:** GPIO 32
* **SCK, MISO, MOSI:** Shared with NRF Radio #1 VSPI bus (GPIO 18, 19, 23).

---

## 🛠 Repository Structure

```text
.
├── evil_cozmo/              # Arduino sketch directory (ready for Arduino IDE)
│   ├── evil_cozmo.ino       # Main Arduino sketch entry point
│   ├── Config.h             # Pin assignments, constants & data structures
│   ├── DisplayBase.h        # U8g2 OLED graphics routines & Cozmo faces
│   ├── KeyboardData.h       # HID keystroke injection payload mappings
│   ├── MenuBLE.h            # Bluetooth Low Energy spam handlers
│   ├── MenuMarauder.h       # Marauder sniffer & PMKID monitor handlers
│   ├── MenuNRF.h            # NRF24L01+ RF Jammer & MouseJack engine
│   ├── MenuSystem.h         # Core UI navigation, brightness & settings
│   ├── MenuWiFi.h           # Deauth, Evil Twin, Beacon Spam & Scanner menus
│   └── WiFiLogic.h          # 802.11 frame injection & captive portal web server
├── USER_MANUAL.md           # Comprehensive manual & companion app guide
├── LICENSE                  # MIT License
└── firmware/                # Ready-to-flash precompiled binaries
    ├── EvilCozmo_v10.bin
    ├── bootloader.bin
    ├── partitions.bin
    └── README.md
```

---

## 💻 Compilation & Installation (Arduino IDE)

### 1. Board Manager & Libraries
1. Add ESP32 board URL in Arduino IDE preferences:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Install board package: **esp32 by Espressif Systems** (version 2.0.x / 3.0.x).
3. Install required libraries via **Library Manager**:
   * **`U8g2`** by oliver (v2.34.0 or newer)
   * **`RF24`** by TMRh20 (v1.3.11 or newer)

### 2. Board Configuration Settings
* **Board:** `ESP32 Dev Module`
* **CPU Frequency:** `240MHz (WiFi/BT)`
* **Flash Frequency:** `80MHz`
* **Flash Mode:** `QIO`
* **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)` ⚠️ *(Required)*
* **Core Debug Level:** `None`

Open [`evil_cozmo/evil_cozmo.ino`](evil_cozmo/evil_cozmo.ino) in Arduino IDE and click **Upload**.

---

## ⚡ Flashing Precompiled Firmware

If you prefer not to compile from source code, use the binaries in the [`firmware/`](firmware/) directory:

```bash
# Flash via esptool.py (Replace COM3 with your serial port)
esptool.py --chip esp32 --port COM3 --baud 460800 write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 firmware/EvilCozmo_v10.bin
```

---

## 🎮 Controls & Navigation

### Physical Buttons:
* **BTN_NEXT (GPIO 4):** Scroll down / Next menu item.
* **BTN_SELECT (GPIO 2):** Execute / Start / Stop action.
* **HOLD SELECT (3 seconds):** Force abort attack & return to main menu.

### Remote Companion App (Android):
1. Pair your Android device with Bluetooth device **`ESP32_NavSmart`**.
2. Open **Serial Bluetooth Terminal** to send remote commands:
   * `NEXT` / `SELECT` (Simulate hardware buttons)
   * `NAV:Turn right in 200m` (Display custom navigation prompt)
   * `WA:Alice:Hey, are you there?` (Display simulated WhatsApp alert)

Full details are documented in [USER_MANUAL.md](USER_MANUAL.md).

---

## ⚠️ Disclaimer

> This project is designed and released **STRICTLY FOR EDUCATIONAL PURPOSES, SECURITY RESEARCH, AND AUTHORIZED PENETRATION TESTING**.
> 
> Operating this device against targets or network infrastructure without explicit prior written authorization is **ILLEGAL** under local and international computer crime laws. The author and contributors assume no liability for any damage, legal repercussions, or misuse resulting from this software or hardware.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE). Free for educational use, modification, and security research distribution.
