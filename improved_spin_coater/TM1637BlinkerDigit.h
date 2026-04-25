#pragma once

#include <Arduino.h>
#include <TM1637Display.h>

class TM1637BlinkerDigit {
private:
    TM1637Display  display;

    int            digits[4];
    uint8_t        blinkMask;
    unsigned long  blinkDelay[4];
    unsigned long  lastMillis[4];
    bool           digitState[4];

    void     showDigits();
    uint8_t* getSegmentsArray();

public:
    TM1637BlinkerDigit(uint8_t clkPin, uint8_t dioPin);

    void begin(uint8_t brightness = 0x0f);
    void setNumber(int num);

    void startBlink(uint8_t index, unsigned long delayMs = 500);
    void stopBlink(uint8_t index);
    void clearBlink();

    void update(); // call in loop to animate blinking
};
