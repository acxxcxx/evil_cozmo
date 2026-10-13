#ifndef DISPLAY_BASE_H
#define DISPLAY_BASE_H

#include "Config.h"

void drawStatusBar() {
    u8g2.setDrawColor(1);
    u8g2.drawHLine(0, 15, 128); 
    u8g2.setFont(u8g2_font_5x7_tf);
    if (SerialBT.hasClient()) u8g2.drawStr(2, 11, "BT: CONNECTED");
    else u8g2.drawStr(2, 11, "BT: WAIT...");
    String brtText = (brightnessLevel == 2) ? "MAX" : (brightnessLevel == 1) ? "MED" : "MIN";
    u8g2.drawStr(108, 11, brtText.c_str());
}

void drawCozmoFace(int cx, int cy, int eyeW, int eyeH, int lookX, int lookY, int emotion) {
    int gap = eyeW + 4, radius = eyeW / 4;
    if (radius > eyeH / 2) radius = max(1, eyeH / 2);
    int lx = cx - gap/2 - eyeW/2 + lookX, rx = cx + gap/2 - eyeW/2 + lookX, y = cy - eyeH/2 + lookY;
    u8g2.setDrawColor(1);
    if (emotion == 6) { // Sleep
        u8g2.drawRBox(lx, cy + 8, eyeW, 6, 2); u8g2.drawRBox(rx, cy + 8, eyeW, 6, 2); return;
    }
    u8g2.drawRBox(lx, y, eyeW, max(1, eyeH), radius); u8g2.drawRBox(rx, y, eyeW, max(1, eyeH), radius);
    if (emotion == 1) { // Happy
        u8g2.setDrawColor(0); u8g2.drawDisc(lx + eyeW/2, y + eyeH + 10, eyeW*2); u8g2.drawDisc(rx + eyeW/2, y + eyeH + 10, eyeW*2);
    } else if (emotion == 2) { // Angry
        u8g2.setDrawColor(0); u8g2.drawTriangle(lx, y-1, lx+eyeW+2, y-1, lx+eyeW+2, y+eyeH/2); u8g2.drawTriangle(rx-2, y-1, rx+eyeW, y-1, rx-2, y+eyeH/2);
    }
    u8g2.setDrawColor(1);
}

void drawNavOverlay() {
    u8g2.setDrawColor(1); u8g2.drawBox(0, 52, 128, 12); u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x7_tf);
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    String nav = navInstruction;
    xSemaphoreGive(stringMutex);
    
    if(nav.length() > 20) nav = nav.substring(0,20);
    u8g2.drawStr(2, 60, nav.c_str()); u8g2.setDrawColor(1);
    int barW = map(constrain(millis()-lastNavUpdate,0,15000),0,15000,126,0);
    u8g2.drawHLine(1, 51, barW);
}

void drawMainDisplay(unsigned long t) {
    u8g2.clearBuffer(); drawStatusBar();
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    bool hasNav = (navInstruction != "");
    xSemaphoreGive(stringMutex);

    if (hasNav) {
        u8g2.setDrawColor(1);
        u8g2.drawTriangle(64, 20, 48, 36, 80, 36); u8g2.drawBox(58, 36, 12, 12); // Arrow Symbol
        drawNavOverlay(); 
    } else if (isSleeping) {
        drawCozmoFace(64, 34, 26, 32, 0, 0, 6);
    } else {
        int floatY = (int)(sin(t / 400.0) * 2);
        drawCozmoFace(64, 34 + floatY, 26, 32, 0, 0, 0);
    }
    u8g2.sendBuffer();
}

void drawWADisplay(unsigned long t) {
    u8g2.clearBuffer(); drawStatusBar();
    
    xSemaphoreTake(stringMutex, portMAX_DELAY);
    String sender = waSender;
    String msg = waMessage;
    xSemaphoreGive(stringMutex);

    u8g2.setFont(u8g2_font_5x7_tf); u8g2.drawStr(36, 26, "WHATSAPP");
    u8g2.setFont(u8g2_font_6x10_tf); u8g2.drawStr(36, 38, sender.substring(0,14).c_str());
    int scrollX = 128 - ((t - lastWaUpdate) % 5000 * 256 / 5000);
    u8g2.drawStr(scrollX, 60, msg.c_str()); 
    u8g2.sendBuffer();
}

#endif