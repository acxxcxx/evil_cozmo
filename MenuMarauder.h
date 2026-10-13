#ifndef MENU_MARAUDER_H
#define MENU_MARAUDER_H

#include "Config.h"
#include "DisplayBase.h"
#include <atomic>
#include <esp_wifi.h>

void drawMarauderMenu() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "MARAUDER TOOLS");
    const char* mItems[] = {"1. Packet Mon", "2. Deauth Detect", "3. Clear Stats", "4. Back"};
    int start = constrain(marauderCursor - 1, 0, max(0, 4 - 3));
    for (int i = 0; i < 3; i++) {
        int idx = start + i;
        if (idx >= 4) break;
        int y = 40 + (i * 9);
        if (idx == marauderCursor) { u8g2.setDrawColor(1); u8g2.drawBox(10, y-7, 108, 9); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1);
        u8g2.drawStr(15, y, mItems[idx]);
    }
    u8g2.sendBuffer();
}

void drawMarauderSniff() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_4x6_tf); u8g2.drawStr(2, 24, "MARAUDER v0.1 - SNIFFING...");
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(10, 33); u8g2.print("TOTAL PKTS: "); u8g2.print(AtomicUtils::safe_load(pkts_total));
    u8g2.setCursor(10, 41); u8g2.print("M: "); u8g2.print(AtomicUtils::safe_load(pkts_mgmt));
    u8g2.print(" DAUTH_RX: "); u8g2.print(AtomicUtils::safe_load(pkts_deauth_rx));
    u8g2.setCursor(10, 49); u8g2.print("DATA: "); u8g2.print(AtomicUtils::safe_load(pkts_data));
    u8g2.print(" APs: "); u8g2.print(AtomicUtils::safe_load(unique_aps));
    u8g2.setCursor(10, 56); u8g2.print("CHANNEL: "); u8g2.print(currentChannel);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(5, 63, "HOLD SEL:STOP | NEXT:BACK");
    u8g2.sendBuffer();
}

#endif