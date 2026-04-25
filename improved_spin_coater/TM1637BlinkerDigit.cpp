#include "TM1637BlinkerDigit.h"

TM1637BlinkerDigit::TM1637BlinkerDigit(uint8_t clkPin, uint8_t dioPin)
    : display(clkPin, dioPin)
{
    for (int i = 0; i < 4; i++) {
        digits[i]      = 0;
        blinkDelay[i]  = 500;
        lastMillis[i]  = 0;
        digitState[i]  = true;
    }
    blinkMask = 0;
}

void TM1637BlinkerDigit::begin(uint8_t brightness) {
    display.setBrightness(brightness);
    display.clear();
}

void TM1637BlinkerDigit::setNumber(int num) {
    digits[0] = (num / 1000) % 10;
    digits[1] = (num /  100) % 10;
    digits[2] = (num /   10) % 10;
    digits[3] =  num         % 10;
    showDigits();
}

void TM1637BlinkerDigit::startBlink(uint8_t index, unsigned long delayMs) {
    if (index > 3) return;
    blinkMask        |= (1 << index);
    blinkDelay[index] = delayMs;
    lastMillis[index] = millis();
}

void TM1637BlinkerDigit::stopBlink(uint8_t index) {
    if (index > 3) return;
    blinkMask       &= ~(1 << index);
    digitState[index] = true;
    showDigits();
}

void TM1637BlinkerDigit::clearBlink() {
    blinkMask = 0;
    for (int i = 0; i < 4; i++) digitState[i] = true;
    showDigits();
}

void TM1637BlinkerDigit::update() {
    unsigned long now = millis();
    bool changed = false;

    for (int i = 0; i < 4; i++) {
        if ((blinkMask & (1 << i)) && (now - lastMillis[i] >= blinkDelay[i])) {
            lastMillis[i]  = now;
            digitState[i]  = !digitState[i];
            changed        = true;
        }
    }

    if (changed) showDigits();
}

void TM1637BlinkerDigit::showDigits() {
    display.setSegments(getSegmentsArray(), 4);
}

uint8_t* TM1637BlinkerDigit::getSegmentsArray() {
    static uint8_t segs[4]; // static: persists past return
    for (int i = 0; i < 4; i++) {
        segs[i] = ((blinkMask & (1 << i)) && !digitState[i])
            ? 0
            : display.encodeDigit(digits[i]);
    }
    return segs;
}
