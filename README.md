================================================================================
                        EVIL COZMO v10 - HARDWARE SETUP
                               Modular WiFi/NRF 
================================================================================

PROJECT OVERVIEW
================================================================================
ESP32-based multi-functional penetration testing tool featuring:
  • WiFi Scanning & Network Analysis (Marauder Sniffer)
  • Advanced WiFi Deauth Jammer (Burst Mode: Deauth + Disassoc)
  • Area Denial (Channel Sweep Attack)
  • Evil Twin
  • Beacon Spam Attack
  • BLE Device Spam
  • NRF24L01+ Multi-Mode Jammer (Bluetooth, Drone, WiFi, Multi-Channel)
  • Mini OLED Display (128x64)
  • Bluetooth Serial Interface
  

MICROCONTROLLER SPECIFICATIONS
================================================================================
Processor:      ESP32-WROOM-32
Processing:     Dual-core @ 240MHz
RAM:            520 KB SRAM
Flash:          4 MB
Interfaces:     WiFi 802.11 b/g/n, Bluetooth 4.2, SPI, I2C, UART
Development:    Arduino IDE (with esp32 board package)


================================================================================
                           PIN CONFIGURATION
================================================================================

─────────────────────────────────────────────────────────────────────────────
I2C DISPLAY CONNECTION (OLED SSD1306 128x64)
─────────────────────────────────────────────────────────────────────────────

Component:      SSD1306 OLED Display Module
Display Size:   128x64 pixels
Interface:      I2C
Address:        0x3C (default)
Connection:

    ESP32 PIN       →    OLED PIN
    ─────────────────────────────
    GPIO 21 (SDA)   →    SDA (Data Line)
    GPIO 22 (SCL)   →    SCL (Clock Line)
    3.3V            →    VCC
    GND             →    GND

Config Reference:
    #define I2C_SDA    21 
    #define I2C_SCL    22 
    
I2C Frequency:  400 kHz


─────────────────────────────────────────────────────────────────────────────
PUSH BUTTON CONNECTIONS
─────────────────────────────────────────────────────────────────────────────

Button 1 - NEXT (Menu Navigation)
    ESP32 PIN:  GPIO 4
    Connection: GND ─[Button]─ GPIO 4 ─[10kΩ Pull-up]─ 3.3V
    Config:     #define BTN_NEXT    4

Button 2 - SELECT (Confirm/Execute)
    ESP32 PIN:  GPIO 2
    Connection: GND ─[Button]─ GPIO 2 ─[10kΩ Pull-up]─ 3.3V
    Config:     #define BTN_SELECT  2

Debounce:       200 ms (BUTTON_DEBOUNCE_MS)


─────────────────────────────────────────────────────────────────────────────
NRF24L01+ RADIO MODULE #1 (VSPI - Primary)
─────────────────────────────────────────────────────────────────────────────

Component:      NRF24L01+ with PCB Antenna
Voltage:        3.3V (use capacitor 10µF across VCC-GND)
SPI Bus:        VSPI
Interface:

    ESP32 PIN       →    NRF24 PIN
    ─────────────────────────────
    GPIO 23 (MOSI)  →    MOSI (DI - Data In)
    GPIO 19 (MISO)  →    MISO (DO - Data Out)
    GPIO 18 (SCK)   →    SCK  (Serial Clock)
    GPIO 5 (CE)     →    CE   (Chip Enable)
    GPIO 17 (CSN)   →    CSN  (Chip Select)
    3.3V            →    VCC  (with 10µF capacitor to GND)
    GND             →    GND  (Ground, common with all)

Config Reference:
    #define NRF_RADIO1_CE     5
    #define NRF_RADIO1_CSN    17
    #define NRF_SPI1_SCK      18
    #define NRF_SPI1_MISO     19
    #define NRF_SPI1_MOSI     23

Default Channel:    45 (2.4 GHz ISM Band)
Data Rate:          2 Mbps
TX Power:           Max (RF24_PA_MAX)
CRC:                Disabled


─────────────────────────────────────────────────────────────────────────────
MICRO SD CARD (VSPI - Shared with Radio #1)
─────────────────────────────────────────────────────────────────────────────

Component:      MicroSD Card Adapter (or direct wiring)
Voltage:        3.3V (IMPORTANT: Do not use 5V!)
SPI Bus:        VSPI (Shared with NRF24 Module #1)
Interface:

    ESP32 PIN       →    SD CARD PIN
    ────────────────────────────────
    GPIO 23 (MOSI)  →    DI (Data In)
    GPIO 19 (MISO)  →    DO (Data Out)
    GPIO 18 (SCK)   →    CLK (Clock)
    GPIO 32 (CS)    →    CS  (Chip Select)
    3.3V            →    VCC
    GND             →    GND

Config Reference:
    #define SD_CS     32

Note:
  • Uses the same SPI bus as NRF Radio #1 (VSPI).[cite: 3]
  • Ensure 3.3V voltage is used to avoid card damage.[cite: 3]


─────────────────────────────────────────────────────────────────────────────
NRF24L01+ RADIO MODULE #2 (HSPI - Secondary, Optional)
─────────────────────────────────────────────────────────────────────────────

Component:      NRF24L01+ with PCB Antenna (Optional for dual-radio jamming)
Voltage:        3.3V (use capacitor 10µF across VCC-GND)
SPI Bus:        HSPI
Interface:

    ESP32 PIN       →    NRF24 PIN
    ─────────────────────────────
    GPIO 13 (MOSI)  →    MOSI (DI - Data In)
    GPIO 12 (MISO)  →    MISO (DO - Data Out)
    GPIO 14 (SCK)   →    SCK  (Serial Clock)
    GPIO 16 (CE)    →    CE   (Chip Enable)
    GPIO 27 (CSN)   →    CSN  (Chip Select)
    3.3V            →    VCC  (with 10µF capacitor to GND)
    GND             →    GND  (Ground, common with all)

Config Reference:
    #define NRF_RADIO2_CE     16
    #define NRF_RADIO2_CSN    27
    #define NRF_SPI2_SCK      14
    #define NRF_SPI2_MISO     12
    #define NRF_SPI2_MOSI     13

Note: 
  • HSPI may conflict with SD card slot if present
  • Both radio modules can run simultaneously for enhanced coverage
  • If not using radio2, leave pins unconnected


─────────────────────────────────────────────────────────────────────────────
POWER SUPPLY RECOMMENDATIONS
─────────────────────────────────────────────────────────────────────────────

Primary Supply:     USB 5V via Micro-USB (ESP32 onboard regulator)
Current Draw:       ~100 mA (idle)
                    ~300-400 mA (WiFi active)
                    ~150-200 mA per NRF24L01+

Recommended PSU:    At least 2A capacity
                    Quality 5V source (avoid cheap adapters)

For Battery Operation:
  • 4x AA Alkaline (6V) through 3.3V LDO regulator
  • USB Power Bank (5V, 2A+)
  • Avoid direct connection to battery on GPIO pins


─────────────────────────────────────────────────────────────────────────────
ESP32 PINOUT REFERENCE
─────────────────────────────────────────────────────────────────────────────

USED PINS:
    GPIO 2   - BTN_SELECT
    GPIO 4   - BTN_NEXT
    GPIO 5   - NRF Radio1 CE
    GPIO 12  - NRF Radio2 MISO
    GPIO 13  - NRF Radio2 MOSI
    GPIO 14  - NRF Radio2 SCK
    GPIO 16  - NRF Radio2 CE
    GPIO 17  - NRF Radio1 CSN
    GPIO 18  - NRF Radio1 SCK
    GPIO 19  - NRF Radio1 MISO
    GPIO 21  - I2C SDA (OLED)
    GPIO 22  - I2C SCL (OLED)
    GPIO 23  - NRF Radio1 MOSI
    GPIO 32  - SD Card CS
    GPIO 27  - NRF Radio2 CSN

RESERVED/DO NOT MODIFY:
    GPIO 1   - TX (Serial Debug Output)
    GPIO 3   - RX (Serial Input)
    GPIO 6-11, 28-31 - Internal Flash
    GPIO 34-39 - Input Only

AVAILABLE FOR EXPANSION:
    GPIO 0, 8, 9, 10, 24-26, 33 (with provisos)


================================================================================
                          REQUIRED LIBRARIES
================================================================================

All libraries should be installed via Arduino IDE Library Manager:

1. U8g2 by oliver
   - OLED Display driver
   - Supports SSD1306 and many other displays
   - Version: 2.34.0 or later

2. RF24 by TMRh20
   - NRF24L01+ SPI radio library
   - Version: 1.3.11 or later

3. BLEDevice (built-in)
   - Bluetooth Low Energy
   - Part of ESP32 core

4. BluetoothSerial (built-in)
   - Classic Bluetooth Serial communication
   - Part of ESP32 core

5. WiFi (built-in)
   - ESP32 WiFi functionality
   - Part of ESP32 core

Installation Steps:
  - Open Arduino IDE
  - Go to: Sketch → Include Library → Manage Libraries
  - Search and install each library above
  - Select correct version numbers

Board Selection:
  - Go to: Tools → Board → Boards Manager
  - Install: "esp32 by Espressif Systems"
  - Select: "ESP32 Dev Module" (or your variant)


================================================================================
                        BOARD CONFIGURATION
================================================================================

Arduino IDE Settings:

  Board:              ESP32 Dev Module
  CPU Frequency:      240 MHz
  Flash Frequency:    80 MHz
  Flash Mode:         QIO
  Flash Size:         4MB (32Mb)
  Partition Scheme:   Huge APP (3MB No OTA/1MB SPIFFS)
  PSRAM:              OPI PSRAM (optional if available)
  
  Upload Speed:       115200
  Port:               COM[X] (auto-detect)


================================================================================
                        COMPILE & UPLOAD
================================================================================

1. Open the project in Arduino IDE:
   - File → Open → nav_smart_v4.ino

2. Verify Code:
   - Sketch → Verify/Compile
   - Should complete without errors
   - Warnings are OK

3. Connect ESP32:
   - Connect via Micro-USB to computer
   - Arduino will auto-detect and show COM port

4. Upload:
   - Sketch → Upload
   - Monitor output in Serial Monitor (115200 baud)

5. Testing:
   - Press BTN_SELECT to wake from sleep
   - Press BTN_NEXT to navigate menu
   - Observe OLED display for output


================================================================================
                        OPERATION MODES
================================================================================

MAIN MENU (9 Options):
  1. WiFi Scan          - Network discovery & RSSI sorting
  2. WiFi Analyzer      - Channel load & interference graph
  3. WiFi Jammer        - Burst Deauth/Disassoc (Targeted/Sweep)
  4. Evil Twin          - Fake AP with Captive Portal & Credential Logging
  5. Beacon Spam        - SSID flooding/spamming
  6. PMKID Capture      - EAPOL/Handshake sniffing
  7. NRF Jammer         - 2.4GHz ISM Multi-mode disruption
  8. MouseJack          - NRF24 Keystroke Injection (Logitech/MS/Generic)
  9. Settings           - System configuration & Display brightness


NRF JAMMER MODES:
  0. Bluetooth Jammer   - Sweep BT channels (2400-2478 MHz)
  1. Drone Jammer       - Sweep all ISM (2400-2525 MHz)
  2. WiFi NRF Jam       - Targeted noise on WiFi-overlapping frequencies
  3. Multi-Channel      - Coordinated dual-radio strategy (BT + Drone)
  4. Mouse Jammer       - Disruption of wireless HID peripherals
  5. Target Jam         - High-intensity fixed channel carrier


================================================================================
                        TROUBLESHOOTING GUIDE
================================================================================

Problem: OLED NOT DISPLAYING
  □ Check I2C connections (GPIO 21/22)
  □ Verify address 0x3C with I2C scanner
  □ Confirm 3.3V and GND connections
  □ Try different OLED modules (may use different address)

Problem: BUTTONS NOT RESPONDING
  □ Check GPIO 2 and 4 connections
  □ Verify pull-up resistors to 3.3V
  □ Test with Serial Monitor: Serial.println(digitalRead(pin))

Problem: NRF24 NOT INITIALIZING
  □ Verify SPI pin connections (18, 19, 23)
  □ Check CE (GPIO 5) and CSN (GPIO 17)
  □ Add 10µF capacitor between VCC-GND
  □ Power supply must provide ≥200mA
  □ Test with RF24 example sketches first

Problem: CRASH or RANDOM REBOOTS
  □ Check power supply capacity (≥2A)
  □ Reduce number of attack packet bursts
  □ Free up memory: reduce buffers
  □ Monitor Serial output for errors

Problem: WiFi JAMMER NOT WORKING
  □ Ensure Mode is set to WIFI_AP_STA
  □ Check target is on valid channel (1-13)
  □ Verify WiFi.mode() transitions
  □ Check ESP logs for AP init failures

Problem: SEGMENTATION FAULTS
  □ Array bounds checking enabled
  □ Verify all pointer assignments
  □ Check Config.h constants match actual hardware
  □ Enable verbose logging in WiFiLogic.h


================================================================================
                        HARDWARE WIRING DIAGRAM
================================================================================

                              ESP32-WROOM-32
                        ┌─────────────────────────┐
                        │                         │
              USB 5V ──→│ MICRO USB   VIN         │
                        │           GND           │
                        │                         │
    OLED I2C:           │ GND (Common Ground)     │
    GPIO21 (SDA) ───→   │ GPIO 21 (SDA)  GPIO 22  │ ←─ GPIO22 (SCL)
    GPIO22 (SCL) ───→   │ (SCL)          GPIO 19  │
                        │                GPIO 23  │
    SD Card (VSPI):     │                         │
    GPIO18 (SCK)  ──→   │ GPIO 18 (SCK)  GPIO 19  │ ←─ GPIO19 (MISO)
    GPIO23 (MOSI) ──→   │ GPIO 23 (MOSI) GPIO 32  │ ←─ GPIO32 (CS)
                        │                         │
    Buttons:            │ GPIO 2 (BTN_SELECT)     │
    GPIO2   ─────→      │ GPIO 4 (BTN_NEXT)       │
    GPIO4   ─────→      │ GPIO 5 (NRF CE1)        │
                        │ GPIO 16 (NRF CE2)       │
    NRF24 #1:           │ GPIO 17 (NRF CSN1)      │
    GPIO5  (CE) ───→    │ GPIO 18 (NRF SCK)       │
    GPIO17 (CSN) ──→    │ GPIO 19 (NRF MISO)      │
    GPIO18 (SCK) ──→    │ GPIO 23 (NRF MOSI)      │
    GPIO19 (MISO) ──→   │                         │
    GPIO23 (MOSI) ──→   │ GPIO 12 (NRF MISO2)     │ ←─ GPIO12
                        │ GPIO 13 (NRF MOSI2)     │ ←─ GPIO13
    NRF24 #2:           │ GPIO 14 (NRF SCK2)      │ ←─ GPIO14
    GPIO14 (SCK) ───→   │ GPIO 27 (NRF CSN2)      │ ←─ GPIO27
    GPIO12 (MISO) ──→   │                         │
    GPIO13 (MOSI) ──→   │                         │
    GPIO16 (CE) ───→    │                         │
    GPIO27 (CSN) ──→    │                         │
                        └─────────────────────────┘


================================================================================
                        SCHEMATIC CONNECTIONS
================================================================================

I2C OLED (SSD1306):
  ┌─────────────────┐
  │   OLED Module   │
  │   SSD1306       │
  │                 │
  │ VCC  ─────→ 3.3V
  │ GND  ─────→ GND
  │ SCL  ─────→ GPIO22
  │ SDA  ─────→ GPIO21
  └─────────────────┘


MICRO SD CARD (VSPI - Shared with NRF #1):
  ┌──────────────────────┐
  │   MicroSD Adapter    │
  │                      │
  │ VCC  ─────→ 3.3V     │
  │ GND  ─────→ GND      │
  │ CS   ─────→ GPIO32   │
  │ CLK  ─────→ GPIO18   │
  │ DI   ─────→ GPIO23   │
  │ DO   ─────→ GPIO19   │
  └──────────────────────┘


NRF24L01+ MODULE #1 (VSPI):
  ┌──────────────────────┐
  │   NRF24L01+          │    ===== 10µF Cap =====
  │   Module             │    ║  VCC ─ GND  ║
  │                      │    ║             ║
  │ VCC  ─[CAP]─→ 3.3V   │←───╝ (Both modules)
  │ GND  ─────→ GND      │
  │ CE   ─────→ GPIO5    │
  │ CSN  ─────→ GPIO17   │
  │ SCK  ─────→ GPIO18   │
  │ MOSI ─────→ GPIO23   │
  │ MISO ─←─── GPIO19    │
  │ IRQ  ─────→ (N/C)    │
  └──────────────────────┘


PUSH BUTTONS (with pull-up resistors):
  ┌──────────────────────┐
  │   Press to GND       │
  │                      │
  │   ────[Button]────   │
  │   │                  │
  │   ├─→ 10kΩ Resistor─→ 3.3V (Pull-up)
  │   │                  │
  │   └─→ GPIO2/4        │
  │   │                  │
  │   └─→ GND            │
  └──────────────────────┘


================================================================================
                        POWER DISTRIBUTION
================================================================================

USB 5V Input
    │
    ├─→ [Micro-USB Connector]
    │
    ├─→ [ESP32 Onboard 3.3V Regulator]
    │
    ├─→ 3.3V Output (Main Rail)
         │
         ├─→ ESP32 VCC
         ├─→ OLED VCC
         ├─→ NRF24 #1 VCC ─[10µF]─ GND
         ├─→ NRF24 #2 VCC ─[10µF]─ GND
         ├─→ Button Pull-ups (10kΩ each)
         │
         └─→ GND (Common Ground)
              │
              ├─→ ESP32 GND
              ├─→ OLED GND
              ├─→ NRF24 #1 GND
              ├─→ NRF24 #2 GND
              ├─→ Button GND
              │
              └─→ USB GND


================================================================================
                        OPERATION BASICS
================================================================================

Initial Setup:
  1. Connect OLED via I2C (GPIO 21/22)
  2. Connect Buttons (GPIO 2 and 4)
  3. Connect NRF24 via SPI (GPIO 18, 19, 23, 5, 17)
  4. Power on via USB
  5. OLED should display boot animation

Navigation:
  • Press BTN_NEXT (GPIO4) to scroll menu
  • Press BTN_SELECT (GPIO2) to confirm selection
  • Long press to exit/stop attack

Display Modes:
  • Main Display: Cozmo face (indicates device status)
  • Menu View: List of available attacks
  • Active Attack: Real-time statistics
  • Sleep Mode: Low contrast face (5 min inactivity)

Default Settings:
  • Brightness: Maximum
  • Auto-Sleep: 5 minutes idle → 10 min sleep
  • Display Refresh: 30 Hz


================================================================================
                        PERFORMANCE NOTES
================================================================================

Memory Usage:
  • Code: ~400 KB
  • SRAM: ~80% typical (under attack)
  • Flash: ~600 KB available for SPIFFS

Processing:
  • Core 0: Display & UI rendering
  • Core 1: Attack execution (High Priority Task)
  • WiFi Stack: Raw 802.11 Frame Injection with Sanity Bypass
  • Memory: Thread-safe state management using Atomic Operations

Attack Rates:
  • WiFi Deauth: ~500-800 packets/sec (Multi-Reason Burst)
  • Beacon Spam: ~10 beacons/sec
  • NRF24 Sweep: ~100 hops/sec

Bluetooth Serial:
  • Baud Rate: 10 configurable (internal use)
  • Device Name: "ESP32_NavSmart"
  • Pairing Code: None required


================================================================================
                        LEGAL & ETHICAL USE
================================================================================

##  DISCLAIMER:
    This tool is provided FOR EDUCATIONAL PURPOSES ONLY.
    
    Unauthorized access to computer networks is ILLEGAL.
    You may only use this tool on networks you own or have
    explicit permission to test.
    
    The author assumes NO LIABILITY for illegal usage.

Usage Guidelines:
  ✓ Use only on YOUR OWN networks
  ✓ Use only with EXPLICIT written permission on others' networks
  ✓ Follow all local and international laws
  ✓ Respect others' privacy and security
  ✓ Use responsibly for legitimate penetration testing


================================================================================
                        REVISION HISTORY
================================================================================

v10.0 (April 2026)
  • Implemented Burst Deauth Logic (Multi-reason codes 0x02, 0x08, 0x0A)
  • Added Type 0xA0 (Disassociation) to attack cycles
  • Hardened Captive Portal with input sanitization
  • Full Thread-Safety using AtomicUtils and Mutex protection
  • Optimized Radio Performance with Silent AP mode during jamming
  • Documentation and README added

v9.5 (March 2026)
  • Dual NRF24 radio support
  • Multi-mode NRF jamming attacks
  • Evil Twin with captive portal

v9.0 (February 2026)
  • Initial public release
  • WiFi scanning and jamming
  • BLE spam functionality


================================================================================
                        QUICK START COMMANDS
================================================================================

Via Serial Monitor (115200 baud):
  • "NEXT"        → Simulate BTN_NEXT press
  • "SELECT"      → Simulate BTN_SELECT press
  • "NAV:text"    → Display navigation message
  • "WA:sender:message" → Simulate WhatsApp message

Example:
  NAV:Turn right in 500m
  WA:Mom:Are you coming home?


================================================================================
                        RESOURCES & LINKS
================================================================================

Libraries:
  • U8g2: https://github.com/olikraus/u8g2
  • RF24: https://github.com/nRF24/RF24
  • ESP32: https://github.com/espressif/arduino-esp32

Documentation:
  • ESP32 Datasheet: https://www.espressif.com
  • NRF24L01+ Datasheet: Nordic Semiconductor
  • SSD1306 Datasheet: Solomon Systech

Tools:
  • Arduino IDE: https://www.arduino.cc/en/software
  • Serial Monitor: Built into Arduino IDE

Community:
  • r/esp32: https://reddit.com/r/esp32
  • Arduino Forums: https://forum.arduino.cc


================================================================================
                        SUPPORT & DONATION
================================================================================

If you find this project useful, educational, or if it helped you in your
security research, consider supporting its continued development! 

Hardware hacking requires constant purchasing of new modules, antennas, 
and microcontrollers. Your donations will directly fund the research and 
development of future EVIL COZMO updates and features.

You can support the developer by sending crypto to the addresses below:

  • Bitcoin (BTC)  : 181uDqPnzmH15V59E9UKRRLeEfuCsvRGqD
  
  • Binance (BNB)  : 0x78872f62f5870bf5d22e683a213a77f09198b377 
                     *(Please ensure it is sent via BSC / BEP-20 Network)*

Thank you for keeping the open-source hardware hacking community alive!


================================================================================

END OF DOCUMENTATION

For updates, fixes, and additional information, refer to the project
repository or documentation files included with this release.

Questions? Check troubleshooting section or consult library documentation.

================================================================================



