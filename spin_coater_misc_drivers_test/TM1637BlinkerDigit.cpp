#include "TM1637BlinkerDigit.h"

// ============================================================
// Constructor
// Initializes display interface and internal state for digits
// ============================================================
TM1637BlinkerDigit::TM1637BlinkerDigit(uint8_t clkPin, uint8_t dioPin)
  : display(clkPin, dioPin) {

  // Initialize per-digit state
  for (int i = 0; i < 4; i++) {
    digits[i] = 0;            // digit value (0–9)
    blinkDelay[i] = 500;      // default blink interval (ms)
    lastMillis[i] = 0;        // last toggle timestamp
    digitState[i] = true;     // true = visible, false = hidden
  }

  blinkMask = 0;              // bitmask: which digits are blinking
  defaultDelay = 500;         // fallback blink delay
}

// ============================================================
// Initialize display hardware
// ============================================================
void TM1637BlinkerDigit::begin(uint8_t brightness) {
  display.setBrightness(brightness); // set LED intensity
  display.clear();                   // clear all segments
}

// ============================================================
// Set a 4-digit number to display
// Breaks integer into individual digits (thousands → ones)
// ============================================================
void TM1637BlinkerDigit::setNumber(int num) {
  digits[0] = (num / 1000) % 10;
  digits[1] = (num / 100) % 10;
  digits[2] = (num / 10) % 10;
  digits[3] = num % 10;

  showDigits(); // immediately update display
}

// ============================================================
// Enable blinking for a specific digit
// index: 0–3 (left → right)
// delayMs: blink interval in milliseconds
// ============================================================
void TM1637BlinkerDigit::startBlink(uint8_t index, unsigned long delayMs) {
  if (index > 3) return;

  blinkMask |= (1 << index);       // set bit for this digit
  blinkDelay[index] = delayMs;     // assign blink speed
  lastMillis[index] = millis();    // reset timer
}

// ============================================================
// Disable blinking for a specific digit
// Ensures digit becomes visible again
// ============================================================
void TM1637BlinkerDigit::stopBlink(uint8_t index) {
  if (index > 3) return;

  blinkMask &= ~(1 << index);  // clear bit
  digitState[index] = true;    // force visible state
  showDigits();
}

// ============================================================
// Disable blinking for all digits
// ============================================================
void TM1637BlinkerDigit::clearBlink() {
  blinkMask = 0;

  // Reset all digits to visible
  for (int i = 0; i < 4; i++) {
    digitState[i] = true;
  }

  showDigits();
}

// ============================================================
// Set default blink delay (not currently auto-applied)
// Useful if you extend logic later
// ============================================================
void TM1637BlinkerDigit::setDefaultBlinkDelay(unsigned long ms) {
  defaultDelay = ms;
}

// ============================================================
// Update function (call repeatedly in loop)
// Handles non-blocking blink timing using millis()
// ============================================================
void TM1637BlinkerDigit::update() {
  unsigned long currentMillis = millis();

  for (int i = 0; i < 4; i++) {
    // Check if this digit is flagged to blink
    if (blinkMask & (1 << i)) {

      // Time to toggle visibility?
      if (currentMillis - lastMillis[i] >= blinkDelay[i]) {
        lastMillis[i] = currentMillis;
        digitState[i] = !digitState[i]; // toggle ON/OFF
      }
    }
  }

  showDigits(); // refresh display each cycle
}

// ============================================================
// Push current digit states to display
// ============================================================
void TM1637BlinkerDigit::showDigits() {
  display.setSegments(getSegmentsArray(), 4);
}

// ============================================================
// Build segment data array for display
// Applies blinking by conditionally blanking digits
// ============================================================
uint8_t* TM1637BlinkerDigit::getSegmentsArray() {
  static uint8_t segs[4]; // static to persist after return

  for (int i = 0; i < 4; i++) {

    // If blinking and currently "off", blank the digit
    if ((blinkMask & (1 << i)) && !digitState[i]) {
      segs[i] = 0; // all segments off
    } else {
      segs[i] = display.encodeDigit(digits[i]); // normal display
    }
  }

  return segs;
}