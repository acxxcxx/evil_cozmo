# 📖 Evil COZMO v10 - User Manual & Operating Guide

Comprehensive guide for operating the Evil COZMO v10 device, hardware menu navigation, Bluetooth companion app, and feature details.

---

## 📑 Table of Contents
1. [Flashing Firmware](#1-flashing-firmware)
2. [Hardware Navigation](#2-hardware-navigation)
3. [Companion App (Bluetooth Serial)](#3-companion-app-bluetooth-serial)
4. [Key Features Overview](#4-key-features-overview)
5. [Troubleshooting](#5-troubleshooting)

---

## 1. Flashing Firmware

If downloading precompiled binaries from the [`firmware/`](firmware/) directory:

### ESP32 Flash Memory Layout (4MB Flash):
* `bootloader.bin` ➡️ `0x1000`
* `partitions.bin` ➡️ `0x8000`
* `EvilCozmo_v10.bin` ➡️ `0x10000`

### Using `esptool.py` (CLI):
```bash
# 1. Erase flash memory (Recommended)
esptool.py --chip esp32 --port COM3 erase_flash

# 2. Flash complete firmware bundle
esptool.py --chip esp32 --port COM3 --baud 460800 --before default_reset --after hard_reset write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 firmware/EvilCozmo_v10.bin
```

### Using Espressif Flash Download Tool:
1. Open the tool, select **ESP32** -> **Develop** -> **UART**.
2. Select files and set offset addresses as listed in the table above.
3. Configure: **SPI SPEED:** `40MHz`, **SPI MODE:** `DIO`, **FLASH SIZE:** `32Mbit` (4MB).
4. Select COM port and baud rate (`115200` or `460800`), then click **START**.

---

## 2. Hardware Navigation

Evil COZMO uses a simple **2-button** control interface:

| Button | ESP32 Pin | Function |
| :--- | :---: | :--- |
| **BTN_NEXT** | GPIO 4 | Scroll down / Move to next menu item |
| **BTN_SELECT** | GPIO 2 | Confirm selection / Start Attack / Stop Attack |
| **LONG PRESS SELECT** | GPIO 2 (Hold 3-5s) | Force exit / Return to main menu from any active screen |

### Boot & Power Management:
* **Boot:** When powered on via USB or battery, the OLED displays the Cozmo robot face animation.
* **Auto Sleep:** After 5 minutes of button inactivity, the device enters low-power sleep mode (*Low Contrast Face*). Press `BTN_SELECT` to wake it up.

---

## 3. Companion App (Bluetooth Serial)

Evil COZMO can be controlled remotely via classic Bluetooth using an Android smartphone.

### Setup:
1. Install **Serial Bluetooth Terminal** from Google Play Store.
2. Turn on your phone's Bluetooth and pair with **`ESP32_NavSmart`** (no PIN required).
3. Open Serial Bluetooth Terminal, navigate to **Devices**, and connect to **`ESP32_NavSmart`**.

### Supported Commands:
| Command | Description | Example |
| :--- | :--- | :--- |
| `NEXT` | Simulates pressing physical NEXT button | `NEXT` |
| `SELECT` | Simulates pressing physical SELECT button | `SELECT` |
| `NAV:[Message]` | Displays custom navigation prompt on OLED HUD | `NAV:Turn right in 500m` |
| `WA:[Sender]:[Message]` | Displays simulated WhatsApp notification popup on OLED | `WA:Boss:Where is the report?` |
| `EXT_PASS:[Password]` | Sets password for WiFi Extender mode | `EXT_PASS:secret123` |

---

## 4. Key Features Overview

Evil COZMO v10 provides 14 advanced operational modes:

### 1. WiFi Scanner (Marauder Sniffer)
* Scans all 2.4GHz Access Points (APs) across Channels 1–13.
* Sorts targets by signal strength (RSSI).
* Detects hidden SSIDs and reveals target BSSIDs.

### 2. WiFi Analyzer
* Real-time spectrum visualization displaying 2.4GHz channel utilization and interference.

### 3. WiFi Jammer (Deauth & Disassoc Burst)
* **Burst Mode:** Transmits multi-reason deauthentication frames (`0x02`, `0x08`, `0x0A`) and disassociation frames (`0xA0`).
* **Targeted:** Disconnects clients from selected APs with precision.
* **Area Denial (Channel Sweep):** Rapid sweep jamming across all channels (1–13).

### 4. Evil Twin (Rogue AP + Captive Portal)
* Clones target SSID while disconnecting victims from legitimate APs via deauth.
* Runs internal DNS and Web Server to present a captive portal phishing page.
* Displays harvested credentials on OLED and Serial output in real-time.

### 5. Beacon Spam
* Floods nearby 2.4GHz airwaves with dozens to hundreds of broadcast SSIDs to disrupt scanner apps and devices.

### 6. PMKID Capture
* Passively captures EAPOL frames (4-way handshake / PMKID) for WPA/WPA2 security audits.

### 7. NRF24L01+ Multi-Mode RF Jammer
* **Bluetooth Jammer:** Sweeps Bluetooth channel spectrum (2400–2478 MHz).
* **Drone Jammer:** Full ISM band disruption (2400–2525 MHz).
* **WiFi NRF Jam:** High-power RF carrier injection targeting WiFi frequencies.
* **Multi-Channel:** Dual-radio coordinated jamming (Radio #1 + Radio #2).
* **Mouse/Peripheral Jammer:** Disrupts wireless 2.4GHz mice and keyboards.

### 8. MouseJack (HID Keystroke Injection)
* Injects keystrokes wirelessly into vulnerable RF transceivers (Logitech Unifying / Microsoft / Generic 2.4GHz).

### 9. WiFi Extender / Repeater
* Portable WiFi range extender and relay functionality.

### 10. System Settings
* OLED brightness contrast adjustment (Low, Med, Max) for covert operations.
* Sleep timer configuration and interface refresh rates.

---

## 5. Troubleshooting

* **Blank OLED Display:** Check I2C SDA (GPIO 21) and SCL (GPIO 22) connections. Verify 3.3V power stability.
* **nRF24L01+ Module Not Detected:** Solder/attach a 10µF capacitor between nRF24 VCC and GND to prevent voltage drops during transmission.
* **ESP32 Reboot Loops (Brownout Detector):** Ensure a reliable USB cable and power supply rated for at least 5V 2A.
