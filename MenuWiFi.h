#ifndef MENU_WIFI_H
#define MENU_WIFI_H

#include <WiFi.h>
#include "Config.h"
#include "DisplayBase.h"
#include <algorithm>

void sortWifiByRSSI() {
    int currentCount = AtomicUtils::safe_load(wifiScanCount);
    if (currentCount <= 0) return;
    for(int i=0; i<currentCount; i++) wifiIndices[i] = i;
    std::sort(wifiIndices, wifiIndices + currentCount, [](int a, int b) {
        return WiFi.RSSI(a) > WiFi.RSSI(b);
    });
}

int updateWifiScanState() {
    static unsigned long scanStartTime = 0;
    int currentCount = AtomicUtils::safe_load(wifiScanCount);
    if (currentCount == -1) {
        if (scanStartTime == 0) scanStartTime = millis(); // Start scan timer
        int8_t res = WiFi.scanComplete();
        if (res >= 0) {
            AtomicUtils::safe_store(wifiScanCount, min((int)res, (int)MAX_WIFI_LIST));
            sortWifiByRSSI();
            scanStartTime = 0; // Reset timer once scan completes
        } else if (millis() - scanStartTime > 10000) { // 10 second scan timeout
            Serial.println("[WIFI_SCAN] Scan timed out.");
            AtomicUtils::safe_store(wifiScanCount, 0); // Treat as no networks found
            scanStartTime = 0; // Reset timer
        }
        return res;
    }
    return currentCount;
}

void drawWiFiScanner() {
    if (AtomicUtils::safe_load(wifiScanCount) == -2) {
        esp_wifi_set_promiscuous(false);
        WiFi.mode(WIFI_STA); 
        WiFi.disconnect();
        WiFi.scanNetworks(true); 
        AtomicUtils::safe_store(wifiScanCount, -1);
    }

    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(36, HEADER_Y, "WIFI LIST");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    
    int scanState = updateWifiScanState();
    int currentCount = AtomicUtils::safe_load(wifiScanCount);
    if (scanState == WIFI_SCAN_RUNNING || currentCount == -1) {
        u8g2.drawStr(10, 45, "Scanning...");
    } else if (currentCount == 0) {
        u8g2.drawStr(10, 45, "No Networks Found!");
    } else {
        // Center-aligned scrolling: keep cursor on line 2 (index 1)
        int start = constrain(wifiScannerCursor - 1, 0, max(0, currentCount - 3));
        for (int i = 0; i < 3; i++) {
            int idx = start + i; if (idx >= currentCount) break;
            int y = 39 + (i * 9);
            if (idx == wifiScannerCursor) {
                u8g2.setDrawColor(1); u8g2.drawBox(4, y - 7, 120, 9); u8g2.setDrawColor(0);
            } else u8g2.setDrawColor(1);
            
            u8g2.setCursor(10, y);
            String ssid = WiFi.SSID(wifiIndices[idx]);
            String auth;
            switch(WiFi.encryptionType(wifiIndices[idx])) {
                case WIFI_AUTH_OPEN: auth = " [O]"; break;
                case WIFI_AUTH_WEP:  auth = " [W]"; break;
                default:             auth = " [*]"; break;
            }

            if (ssid.length() == 0) {
                // If SSID is empty, display BSSID (Hidden)
                u8g2.print("HID:" + WiFi.BSSIDstr(wifiIndices[idx]).substring(0, 8) + auth);
            } 
            else if (idx == wifiScannerCursor && ssid.length() > 14) {
                int scrollIdx = (millis() / 300) % (ssid.length() - 10);
                u8g2.print(ssid.substring(scrollIdx, scrollIdx + 14) + auth);
            } 
            else {
                u8g2.print(ssid.substring(0, 14) + auth);
            }
        }
    }
    u8g2.sendBuffer();
}

void drawWiFiJammerRun() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(30, HEADER_Y, "WIFI JAMMER");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    String ssidDisplay = lockedSSID.substring(0, 15);
    xSemaphoreGive(stringMutex);
    
    u8g2.setCursor(5, 30); u8g2.print("Target: " + ssidDisplay);
    u8g2.setCursor(5, 40); u8g2.print("Sent: "); u8g2.print(AtomicUtils::safe_load(pkts_deauth_sent));
    u8g2.setCursor(5, 50); u8g2.print("Ch: "); u8g2.print(AtomicUtils::safe_load(targetChannel));
    char bssidStr[18];
    snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X", lockedBSSID[0], lockedBSSID[1], lockedBSSID[2], lockedBSSID[3], lockedBSSID[4], lockedBSSID[5]);
    u8g2.setFont(u8g2_font_4x6_tf); u8g2.setCursor(5, 58); u8g2.print(bssidStr);
    u8g2.sendBuffer();
}

void drawWiFiSweepRun() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "WIFI SWEEPING");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(5, 33); u8g2.print("Target: ALL CHANNELS");
    u8g2.setCursor(10, 42); u8g2.print("Sent: "); u8g2.print(AtomicUtils::safe_load(pkts_deauth_sent));
    u8g2.setCursor(10, 51); u8g2.print("Scanning Ch 1-13...");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(5, 62, "HOLD SELECT TO STOP");
    u8g2.sendBuffer();
}

void drawWiFiAnalyzer() {
    int currentCount = AtomicUtils::safe_load(wifiScanCount);
    static unsigned long lastRefresh = 0;
    if (currentCount == -2) {
        esp_wifi_set_promiscuous(false);
        WiFi.mode(WIFI_STA); WiFi.disconnect();
        WiFi.scanNetworks(true); AtomicUtils::safe_store(wifiScanCount, -1);
    }

    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "WIFI ANALYZER");
    u8g2.setDrawColor(1);

    int scanState = updateWifiScanState();

    // Update channelLoad data only when scan just completed
    if (scanState >= 0) {
        for(int i=0; i<14; i++) channelLoad[i] = 0;
        for(int i=0; i<currentCount; i++) {
            int ch = WiFi.channel(wifiIndices[i]);
            if(ch >= 1 && ch <= 13) channelLoad[ch]++;
        }

        // Auto-refresh every 4 seconds (WiFi scan requires time)
        if (millis() - lastRefresh > 4000) {
            AtomicUtils::safe_store(wifiScanCount, -2); 
            lastRefresh = millis();
        }
    }

    // Always draw graph using latest channelLoad data
    int bestCh = 1; int minAPs = 999;
    for(int i=1; i<=13; i++) {
        int x = 10 + (i * 8);
        int h = min(25, channelLoad[i] * 4);
        u8g2.drawBox(x, 55 - h, 6, h);
        
        u8g2.setFont(u8g2_font_4x6_tf);
        if (i % 2 != 0) u8g2.setCursor(x, 63); else u8g2.setCursor(x, 30);
        u8g2.print(i);

        if(channelLoad[i] < minAPs) { minAPs = channelLoad[i]; bestCh = i; }
    }

    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(5, 40); u8g2.print("Best Channel: ");
    u8g2.setDrawColor(1); u8g2.drawBox(80, 32, 20, 10);
    u8g2.setDrawColor(0); u8g2.setCursor(85, 40); u8g2.print(bestCh);
    u8g2.setDrawColor(1);

    // If scanning, show small corner indicator to prevent blinking
    if (scanState == WIFI_SCAN_RUNNING) {
        u8g2.setFont(u8g2_font_4x6_tf); u8g2.drawStr(95, 25, "SCAN...");
    }
    
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(5, 62, "PRESS NEXT TO EXIT");
    u8g2.sendBuffer();
}

void drawEvilTwinRun() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "TWIN ACTIVE");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    String ssidCopy = lockedSSID.substring(0, 14);
    String usrCopy = capturedUsername;
    String pwdCopy = capturedPassword;
    xSemaphoreGive(stringMutex);

    u8g2.setCursor(5, 35); u8g2.print("SSID: " + ssidCopy);
    u8g2.setCursor(5, 44); u8g2.print("Hits: "); u8g2.print(AtomicUtils::safe_load(captivePortalHits));
    
    if (usrCopy != "" || pwdCopy != "") {
        u8g2.setFont(u8g2_font_4x6_tf);
        u8g2.setCursor(5, 54); u8g2.print("USR: " + usrCopy.substring(0, 20));
        u8g2.setCursor(5, 62); u8g2.print("PWD: " + pwdCopy.substring(0, 20));
    } else u8g2.drawStr(5, 61, "WAITING...");
    u8g2.sendBuffer();
}

void drawBeaconSpamRun() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(15, HEADER_Y, "BEACON ACTIVE");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(5, 40); u8g2.print("SSID: "); u8g2.print(lockedSSID.substring(0, 16));
    if (beaconCapturedPassword != "") {
        u8g2.setCursor(5, 60); u8g2.print("PWD: "); u8g2.print(beaconCapturedPassword.substring(0, 16));
    } else u8g2.drawStr(5, 60, "Spamming...");
    u8g2.sendBuffer();
}

void drawPMKIDCapture() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(20, HEADER_Y, "PMKID CAPTURE");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(10, 38); u8g2.print("Status: Sniffing Ch "); u8g2.print(currentChannel);
    u8g2.setCursor(10, 48); u8g2.print("EAPOL Detected: "); u8g2.print(AtomicUtils::safe_load(pkts_pmkid));
    u8g2.drawStr(10, 60, "Wait for handshake...");
    u8g2.sendBuffer();
}

void drawWiFiJammerMenu() {
    if (wifiScanCount == -2) {
        esp_wifi_set_promiscuous(false);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.scanNetworks(true); // Start Async Scan
        wifiScanCount = -1;
    }
    
    if (updateWifiScanState() == WIFI_SCAN_RUNNING) {
        u8g2.clearBuffer(); drawStatusBar();
        u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(20, 40, "SCANNING TARGET...");
        u8g2.sendBuffer();
        return;
    }
    
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "SELECT TARGET");
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x7_tf);

    if (wifiScanCount <= 0) {
        u8g2.drawStr(10, 45, "No Targets Found");
    } else {
        int totalItems = wifiScanCount + 1; // +1 for Sweep option
        // Keep cursor off screen bottom by limiting view to 3 items
        int start = constrain(wifiJammerCursor - 1, 0, max(0, totalItems - 3));
        
        for (int i = 0; i < 3; ++i) {
            int itemIdx = start + i;
            if (itemIdx >= totalItems) break;
            int y = 39 + (i * 9);
            if (itemIdx == wifiJammerCursor) { u8g2.setDrawColor(1); u8g2.drawBox(4, y-7, 120, 9); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1);
            u8g2.setCursor(10, y);
            
            if (itemIdx == 0) {
                u8g2.print("[SWEEP ALL CHANNELS]");
            } else {
                int wifiIdx = wifiIndices[itemIdx - 1];
                String ssid = WiFi.SSID(wifiIdx);
                if (itemIdx == wifiJammerCursor && ssid.length() > 12) {
                    int scrollIdx = (millis() / 300) % (ssid.length() - 8);
                    u8g2.print(ssid.substring(scrollIdx, scrollIdx + 12));
                } else {
                    u8g2.print(ssid.substring(0, 12));
                }
                u8g2.print(" [" + String(WiFi.RSSI(wifiIdx)) + "]");
            }
        }
    }
    u8g2.sendBuffer();
}

void drawEvilTwinMenu() {
    if (wifiScanCount == -2) {
        WiFi.mode(WIFI_STA); WiFi.disconnect();
        WiFi.scanNetworks(true); wifiScanCount = -1;
    }
    if (updateWifiScanState() == WIFI_SCAN_RUNNING) {
        u8g2.clearBuffer(); drawStatusBar(); 
        u8g2.drawStr(20, 40, "SCANNING TARGET..."); u8g2.sendBuffer();
        return;
    }

    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "EVIL TWIN SETUP");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);

    if (wifiScanCount <= 0) {
        u8g2.drawStr(10, 45, "No Targets Found");
    } else {
        int start = constrain(evilTwinCursor - 1, 0, max(0, wifiScanCount - 3));
        for (int i = 0; i < 3; ++i) {
            int itemIdx = start + i;
            if (itemIdx >= wifiScanCount) break;
            int y = 39 + (i * 9);
            if (itemIdx == evilTwinCursor) { u8g2.setDrawColor(1); u8g2.drawBox(4, y-7, 120, 9); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1);
            u8g2.setCursor(10, y); u8g2.print(WiFi.SSID(wifiIndices[itemIdx]).substring(0, 15));
        }
    }
    u8g2.sendBuffer();
}

void drawBeaconSpamMenu() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(20, HEADER_Y, "BEACON SPAM");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);
    if (wifiScanCount <= 0) {
        u8g2.drawStr(10, 45, "No Targets Found");
    } else {
        int start = constrain(beaconSpamCursor - 1, 0, max(0, wifiScanCount - 3));
        for (int i = 0; i < 3; ++i) {
            int itemIdx = start + i;
            if (itemIdx >= wifiScanCount) break;
            int y = 39 + (i * 9);
            if (itemIdx == beaconSpamCursor) { u8g2.setDrawColor(1); u8g2.drawBox(4, y-7, 120, 9); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1);
            u8g2.setCursor(10, y); u8g2.print(WiFi.SSID(wifiIndices[itemIdx]).substring(0, 15));
        }
    }
    u8g2.sendBuffer();
}

void drawWiFiExtenderRun() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "WIFI EXTENDER");
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_5x7_tf);

    xSemaphoreTake(stringMutex, portMAX_DELAY);
    String s = extenderSSID;
    xSemaphoreGive(stringMutex);

    if (s == "") {
        u8g2.drawStr(5, 35, "Input via BT Serial:");
        u8g2.drawStr(5, 45, "EXT_SSID:wifi_name");
        u8g2.drawStr(5, 53, "EXT_PASS:password");
        u8g2.drawStr(5, 61, "Then press SELECT");
    } else if (!AtomicUtils::safe_load(extenderActive)) {
        u8g2.setCursor(5, 35); u8g2.print("Target: " + s.substring(0, 15));
        u8g2.setCursor(5, 45); u8g2.print("Status: READY");
        u8g2.setFont(u8g2_font_4x6_tf); u8g2.drawStr(5, 60, "SSID & Pass OK. Press SELECT");
    } else {
        u8g2.setCursor(5, 35); u8g2.print("Target: " + s.substring(0, 15));
        u8g2.setCursor(5, 45); 
        if (WiFi.status() == WL_CONNECTED) {
            u8g2.print("Status: CONNECTED");
            u8g2.setCursor(5, 55); u8g2.print("IP: " + WiFi.localIP().toString());
        } else {
            u8g2.print("Status: CONNECTING");
            int anim = (millis() / 400) % 4;
            for(int i=0; i<anim; i++) u8g2.print(".");
        }
        u8g2.setFont(u8g2_font_4x6_tf); u8g2.drawStr(5, 63, "NEXT: BACK | SEL: RECONNECT");
    }
    u8g2.sendBuffer();
}
#endif