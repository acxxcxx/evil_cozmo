#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "Config.h"
#include "DisplayBase.h"

void drawMenu() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, 14); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(36, 26, "MAIN MENU");
    static const char* const mItems[MENU_ITEMS_COUNT] = {
        "1. WiFi Scan", "2. Marauder", "3. WiFi Jammer", "4. Evil Twin", 
        "5. Beacon Spam", "6. BLE Spam", "7. NRF Jammer", "8. PMKID Capture", 
        "9. WiFi Analyzer", "10. MouseJack", "11. Extender", "12. Brightness", 
        "13. Reboot", "14. BACK"
    };
    int start = constrain(menuCursor - 2, 0, max(0, MENU_ITEMS_COUNT - 4)); 
    for (int i = 0; i < 4; i++) {
        int idx = start + i; if (idx >= MENU_ITEMS_COUNT) break;
        int y = 38 + (i * 8);
        if (idx == menuCursor) { u8g2.setDrawColor(1); u8g2.drawBox(4, y-7, 115, 9); u8g2.setDrawColor(0); } else u8g2.setDrawColor(1); 
        u8g2.setFont(u8g2_font_5x7_tf); u8g2.drawStr(10, y, mItems[idx]);
    }
    u8g2.sendBuffer();
}

void drawBrightnessUI() {
    u8g2.clearBuffer(); drawStatusBar();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 16, 128, HEADER_H); u8g2.setDrawColor(0); 
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(24, HEADER_Y, "BRIGHTNESS"); u8g2.setDrawColor(1);
    for(int i=0; i<3; i++) { 
        if (i <= brightnessLevel) u8g2.drawBox(36 + (i*24), 48 - (i*5), 14, 10 + (i*5)); 
        else u8g2.drawFrame(36 + (i*24), 48 - (i*5), 14, 10 + (i*5)); 
    }
    u8g2.sendBuffer();
}

void drawBooting(int progress) {
    u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(18, 26, "EVIL COZMO");
    u8g2.drawFrame(14,57,100,6); u8g2.drawBox(15,58,progress,4); u8g2.sendBuffer();
}

#endif