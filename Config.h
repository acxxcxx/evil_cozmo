#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ── Hardware Configuration ────────────────────────────────────
#define SCREEN_WTH            128
#define SCREEN_HEIGHT         64
#define HEADER_H              11
#define HEADER_Y              25
#define ITEM_H                9
#define MAX_VISIBLE           4

// ── Pin Configuration ─────────────────────────────────────────
#define BTN_NEXT              4
#define BTN_SELECT            2
#define I2C_SDA               21
#define I2C_SCL               22
#define NRF_RADIO1_CE         5
#define NRF_RADIO1_CSN        17
#define NRF_RADIO2_CE         16
#define NRF_RADIO2_CSN        27
#define NRF_SPI1_SCK          18
#define NRF_SPI1_MISO         19
#define NRF_SPI1_MOSI         23
#define SD_CS                 32
#define NRF_SPI2_SCK          14
#define NRF_SPI2_MISO         12
#define NRF_SPI2_MOSI         13

// ── Constants & Configuration ─────────────────────────────────
#define MAX_WIFI_LIST         20
#define MAX_WA_MSG_LEN        80
#define NOTIF_TIMEOUT         15000UL
#define SCAN_RETRY_MS         2000UL
#define BT_TIMEOUT_MS         50
#define MENU_ITEMS_COUNT      14
#define DISPLAY_REFRESH_HZ    20 // Reduced for better performance on busy tasks
#define MAX_MJ_TARGETS        8
#define RSSI_HISTORY_SIZE     22
#define BUTTON_DEBOUNCE_MS    200
#define RESTART_HOLD_MS       5000
#define DEAUTH_SEND_INTERVAL  20U
#define EVILTWIN_DEAUTH_INT   80U
#define BEACON_SEND_INTERVAL  100U
#define NRF_INIT_DELAY_MS     500
#define TASK_STACK_SIZE       8192
#define TASK_PRIORITY         1

// ── Sleep & Power Configuration ────────────────────────────────
#define TIME_TO_SLEEP         (5UL * 60UL * 1000UL)
#define SLEEP_DURATION        (10UL * 60UL * 1000UL)
#define SLEEP_CONTRAST        5

// ── WiFi Channel Configuration ─────────────────────────────────
#define WIFI_CHANNEL_MIN      1
#define WIFI_CHANNEL_MAX      13
#define NRF_CHANNEL_DEFAULT   45

// ── UI State Enum ──────────────────────────────────────────────
enum UiMode { 
    UI_MAIN, UI_MENU, UI_BRIGHTNESS, UI_WIFI_SCAN, 
    UI_MARAUDER_MENU, UI_MARAUDER_SNIFF, 
    UI_WIFIJAMMER_MENU, UI_WIFIJAMMER_RUN, 
    UI_EVILTWIN_MENU, UI_EVILTWIN_RUN, 
    UI_BEACON_SPAM_MENU, UI_BEACON_SPAM_RUN, 
    UI_BLE_SPAM, UI_NRF_MENU, UI_NRF_RUN, UI_NRF_SPECTRUM, UI_WIFI_SWEEP_RUN, UI_NRF_WIFI_TARGET, UI_WIFI_EXTENDER,
    UI_NRF_TARGET_SCAN, UI_NRF_TARGET_SELECT, UI_NRF_LISTEN_MOUSE, UI_NRF_LISTEN_KEYBOARD,
    UI_PMKID_CAPTURE, UI_MOUSEJACK_RUN, UI_WIFI_ANALYZER, UI_REBOOT
};
// Ensure UI_REBOOT order is consistent with menu index (11)

// ── Brightness ─────────────────────────────────────────────────
const int contrastValues[3] = {10, 90, 230}; 
const int SLEEP_CONTRAST_VAL = 5; 

// ── Thread-Safe Utility Functions for Atomic Operations ────────
namespace AtomicUtils {
  template<typename T>
  inline T safe_load(std::atomic<T>& var) { return var.load(std::memory_order_seq_cst); }
  
  template<typename T>
  inline void safe_store(std::atomic<T>& var, T val) { var.store(val, std::memory_order_seq_cst); }
  
  template<typename T>
  inline T safe_exchange(std::atomic<T>& var, T val) { return var.exchange(val, std::memory_order_seq_cst); }
}

// Forward declarations for externs used in modules
#include <U8g2lib.h>
#include <BluetoothSerial.h>
#include <SPI.h>
#include <atomic>
#include <Preferences.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <RF24.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern BluetoothSerial SerialBT;
extern Preferences preferences;
extern RF24 radio;
extern RF24 radio2;
extern int brightnessLevel;
extern std::atomic<UiMode> currentUi;
extern SemaphoreHandle_t stringMutex; // Mutex for cross-core String protection
extern int menuCursor;
extern std::atomic<bool> nrfRunning;
extern std::atomic<bool> radio1_ok;
extern std::atomic<bool> radio2_ok;
extern unsigned long lastNavUpdate;
extern bool sd_ok;
extern unsigned long lastWaUpdate;
extern String navInstruction;
extern String waSender;
extern String waMessage;
extern bool isShowingWA;
extern bool isSleeping;
extern std::atomic<int> wifiScanCount;
extern int wifiIndices[MAX_WIFI_LIST];
extern String lockedSSID;
extern std::atomic<long> blePacketCount;
extern String resolvedHiddenSSID; // Stores captured hidden SSID
extern uint8_t lockedBSSID[6];
extern int currentRSSI;
extern int rssiHistory[RSSI_HISTORY_SIZE];
extern unsigned long lastBtnPress;
extern unsigned long lastActiveTime;
extern unsigned long sleepStartTime;
extern unsigned long lastDisplayUpdate;
extern unsigned long selectHoldStartTime;

// Marauder specific
extern int marauderCursor;
extern std::atomic<long> pkts_total;
extern std::atomic<long> pkts_mgmt;
extern std::atomic<long> pkts_data;
extern std::atomic<long> pkts_ctrl;
extern std::atomic<long> pkts_prob;
extern std::atomic<long> pkts_deauth_rx;
extern std::atomic<long> pkts_deauth_sent;
extern std::atomic<long> pkts_pmkid;
extern std::atomic<int> unique_aps;
extern uint8_t currentChannel;
extern unsigned long channelTimer;

// WiFi Attack specific
extern int wifiScannerCursor;
extern int wifiJammerCursor;
extern int evilTwinCursor;
extern int beaconSpamCursor;
extern std::atomic<int> targetChannel;
extern uint16_t deauthSeq;
extern std::atomic<int> captivePortalHits;
extern String capturedUsername;
extern String capturedPassword;
extern String extenderSSID;
extern String extenderPassword;
extern std::atomic<int> beaconPortalHits;
extern std::atomic<bool> extenderActive;
extern String beaconCapturedPassword;
extern std::atomic<bool> beaconStarted;
extern DNSServer dnsServer;
extern WebServer server;
extern int channelLoad[14];
extern String webTemplate;

// NRF specific
extern int nrfCursor;
extern std::atomic<int> nrfJamMode;
extern SPIClass vspi;
extern SPIClass hspi;
extern std::atomic<bool> mouseJackActive;
extern uint8_t mjTargetAddresses[MAX_MJ_TARGETS][5];
extern std::atomic<int> mjTargetCount;
extern int mjChannel;
extern int mjPayloadCursor;
extern String mjStatus;
extern uint8_t nrfSpectrum[128];

extern int nrfTargetCursor;

extern uint8_t mjTargetChannels[MAX_MJ_TARGETS];


#endif