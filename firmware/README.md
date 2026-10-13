# Evil COZMO v10 - Precompiled Firmware

This directory contains precompiled firmware binaries for ESP32 (4MB Flash). If you prefer not to compile the source code via Arduino IDE, you can flash these files directly to your ESP32 board.

---

## 📦 Firmware Files

| File | Memory Offset | Description |
| :--- | :---: | :--- |
| `bootloader.bin` | `0x1000` | ESP32 Second Stage Bootloader |
| `partitions.bin` | `0x8000` | Custom Partition Table (Huge APP: 3MB No OTA / 1MB SPIFFS) |
| `EvilCozmo_v10.bin` | `0x10000` | Main Application Firmware |

---

## ⚡ Flashing Guide

### Option 1: Using `esptool.py` (CLI / Recommended)

Make sure Python and `esptool` are installed (`pip install esptool`).

1. Connect your ESP32 via a USB data cable and identify the COM port (Windows: `COMx`, Linux/macOS: `/dev/ttyUSB0` or `/dev/ttyACM0`).
2. Erase existing flash memory (recommended):
   ```bash
   esptool.py --chip esp32 --port COM3 erase_flash
   ```
3. Flash all three files with a single command:
   ```bash
   esptool.py --chip esp32 --port COM3 --baud 460800 --before default_reset --after hard_reset write_flash -z \
     0x1000 bootloader.bin \
     0x8000 partitions.bin \
     0x10000 EvilCozmo_v10.bin
   ```
   *(Replace `COM3` with your actual ESP32 port)*

---

### Option 2: Using Espressif Flash Download Tool (Windows GUI)

1. Download and run the official **Flash Download Tool** from Espressif.
2. Select:
   - **Chip Type:** `ESP32`
   - **WorkMode:** `Develop`
   - **LoadMode:** `UART`
3. Check the three file rows and set their offset addresses:
   - `bootloader.bin` ➡️ `@ 0x1000`
   - `partitions.bin` ➡️ `@ 0x8000`
   - `EvilCozmo_v10.bin` ➡️ `@ 0x10000`
4. Configure SPI Settings:
   - **SPI SPEED:** `40MHz`
   - **SPI MODE:** `DIO`
   - **FLASH SIZE:** `32Mbit` (4MB)
5. Select your **COM Port** and baud rate `115200` or `460800`.
6. Click **START** and wait until **FINISH** is displayed.
   *(If stuck on "SYNC", hold the **BOOT** button on your ESP32 board)*
