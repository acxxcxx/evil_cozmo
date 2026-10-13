#ifndef MENU_MARAUDER_H
#define MENU_MARAUDER_H

#include "Config.h"
#include "DisplayBase.h"
#include <atomic>
#include <esp_wifi.h>

void drawMarauderMenu() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(25, HEADER_Y, "MARAUDER TOOLS");
    const char* mItems[] = {"[Packet Monitor]", "[Back]"};
    for (int i=0; i<2; i++) {
        int y = 45 + (i * 12);
        if (i == marauderCursor) { u8g2.setDrawColor(1); u8g2.drawBox(10, y-9, 108, 11); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1);
        u8g2.drawStr(15, y, mItems[i]);
    }
    u8g2.sendBuffer();
}

void drawMarauderSniff() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_4x6_tf); u8g2.drawStr(2, 24, "MARAUDER v0.1 - SNIFFING...");
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(10, 34); u8g2.print("TOTAL PKTS: "); u8g2.print(AtomicUtils::safe_load(pkts_total));
    u8g2.setCursor(10, 42); u8g2.print("M: "); u8g2.print(AtomicUtils::safe_load(pkts_mgmt));
    u8g2.print(" DAUTH_RX: "); u8g2.print(AtomicUtils::safe_load(pkts_deauth_rx));
    u8g2.setCursor(10, 50); u8g2.print("DATA: "); u8g2.print(AtomicUtils::safe_load(pkts_data));
    u8g2.print(" APs: "); u8g2.print(AtomicUtils::safe_load(unique_aps));
    u8g2.setCursor(10, 58); u8g2.print("CHANNEL: "); u8g2.print(currentChannel);
    u8g2.drawStr(5, 62, "HOLD SEL:STOP | NEXT:BACK");
    u8g2.sendBuffer();
}

#endif