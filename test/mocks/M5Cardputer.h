#ifndef MOCK_M5CARDPUTER_H
#define MOCK_M5CARDPUTER_H

#include "Arduino.h"

struct MockDisplay {
    void println(const char* s) {}
};

struct MockBtn {
    bool isPressed() { return false; }
};

struct MockKeyboard {
    bool isPressed() { return false; }
    bool isChange() { return false; }
};

struct MockM5 {
    MockDisplay Display;
    MockBtn BtnA;
    MockKeyboard Keyboard;
    void update() {}
    void begin() {}
};

extern MockM5 M5Cardputer;

#endif
