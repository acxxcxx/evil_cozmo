#ifndef KEYBOARD_DATA_H
#define KEYBOARD_DATA_H

#include <Arduino.h>

// HID Modifier Keys (from USB HID Usage Tables)
#define MOD_NONE    0x00
#define MOD_LCTRL   0x01
#define MOD_LSHIFT  0x02
#define MOD_LALT    0x04
#define MOD_LGUI    0x08 // Left GUI (Windows/Command key)
#define MOD_RCTRL   0x10
#define MOD_RSHIFT  0x20
#define MOD_RALT    0x40
#define MOD_RGUI    0x80

// Structure to hold HID keycode and modifier
struct HIDKey {
    uint8_t modifier;
    uint8_t usage;
};

// Function to convert ASCII character to HID keycode and modifier
// This is a simplified version and might need expansion for full keyboard support
inline HIDKey asciiToHID(char c) {
    HIDKey key = {MOD_NONE, 0x00};

    if (c >= 'a' && c <= 'z') {
        key.usage = 0x04 + (c - 'a'); // HID Usage for 'a' is 0x04
    } else if (c >= 'A' && c <= 'Z') {
        key.modifier = MOD_LSHIFT;
        key.usage = 0x04 + (c - 'A');
    } else if (c >= '1' && c <= '9') {
        key.usage = 0x1E + (c - '1'); // HID Usage for '1' is 0x1E
    } else if (c == '0') {
        key.usage = 0x27; // HID Usage for '0' is 0x27
    } else {
        switch (c) {
            case ' ': key.usage = 0x2C; break; // Space
            case '\n': key.usage = 0x28; break; // Enter
            case '\t': key.usage = 0x2B; break; // Tab
            case '-': key.usage = 0x2D; break; // -
            case '=': key.usage = 0x2E; break; // =
            case '[': key.usage = 0x2F; break; // [
            case ']': key.usage = 0x30; break; // ]
            case '\\': key.usage = 0x31; break; // \
            case ';': key.usage = 0x33; break; // ;
            case '\'': key.usage = 0x34; break; // '
            case '`': key.usage = 0x35; break; // `
            case ',': key.usage = 0x36; break; // ,
            case '.': key.usage = 0x37; break; // .
            case '/': key.usage = 0x38; break; // /
            // Shifted characters
            case '!': key = {MOD_LSHIFT, 0x1E}; break; // ! (Shift + 1)
            case '@': key = {MOD_LSHIFT, 0x1F}; break; // @ (Shift + 2)
            case '#': key = {MOD_LSHIFT, 0x20}; break; // # (Shift + 3)
            case '$': key = {MOD_LSHIFT, 0x21}; break; // $ (Shift + 4)
            case '%': key = {MOD_LSHIFT, 0x22}; break; // % (Shift + 5)
            case '^': key = {MOD_LSHIFT, 0x23}; break; // ^ (Shift + 6)
            case '&': key = {MOD_LSHIFT, 0x24}; break; // & (Shift + 7)
            case '*': key = {MOD_LSHIFT, 0x25}; break; // * (Shift + 8)
            case '(': key = {MOD_LSHIFT, 0x26}; break; // ( (Shift + 9)
            case ')': key = {MOD_LSHIFT, 0x27}; break; // ) (Shift + 0)
            case '_': key = {MOD_LSHIFT, 0x2D}; break; // _ (Shift + -)
            case '+': key = {MOD_LSHIFT, 0x2E}; break; // + (Shift + =)
            case '{': key = {MOD_LSHIFT, 0x2F}; break; // { (Shift + [)
            case '}': key = {MOD_LSHIFT, 0x30}; break; // } (Shift + ])
            case '|': key = {MOD_LSHIFT, 0x31}; break; // | (Shift + \)
            case ':': key = {MOD_LSHIFT, 0x33}; break; // : (Shift + ;)
            case '"': key = {MOD_LSHIFT, 0x34}; break; // " (Shift + ')
            case '~': key = {MOD_LSHIFT, 0x35}; break; // ~ (Shift + `)
            case '<': key = {MOD_LSHIFT, 0x36}; break; // < (Shift + ,)
            case '>': key = {MOD_LSHIFT, 0x37}; break; // > (Shift + .)
            case '?': key = {MOD_LSHIFT, 0x38}; break; // ? (Shift + /)
            default: key.usage = 0x00; break; // Unknown character
        }
    }
    return key;
}

#endif // KEYBOARD_DATA_H