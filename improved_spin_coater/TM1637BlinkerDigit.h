#pragma once

#include <Arduino.h>
#include <TM1637Display.h>

// ============================================================
// TM1637BlinkerDigit
// Wrapper around TM1637Display that adds per-digit blinking
// with non-blocking timing (millis-based).
//
// Supports:
// - Displaying a 4-digit integer
// - Independent blinking control for each digit
// ============================================================
class TM1637BlinkerDigit {
private:
  TM1637Display display;  // underlying TM1637 driver instance

  int digits[4];          // current digit values (0–9 for each position)

  // --- Blinking Control ---
  uint8_t blinkMask;      // bitmask indicating which digits blink
                          // bit 0 = digit[0], bit 3 = digit[3]

  unsigned long blinkDelay[4]; // blink interval per digit (ms)
  unsigned long lastMillis[4]; // last toggle timestamp per digit
  bool digitState[4];          // current visibility state:
                              // true = visible, false = hidden

  unsigned long defaultDelay;  // default blink interval (ms)

  // --- Internal Helpers ---
  void showDigits();           // push current state to display
  uint8_t* getSegmentsArray(); // build segment buffer for display

public:
  // ============================================================
  // Constructor
  // clkPin: clock pin for TM1637
  // dioPin: data pin for TM1637
  // ============================================================
  TM1637BlinkerDigit(uint8_t clkPin, uint8_t dioPin);

  // ============================================================
  // Initialize display hardware
  // brightness: 0x00 (min) → 0x0f (max)
  // ============================================================
  void begin(uint8_t brightness = 0x0f);

  // ============================================================
  // Set a 4-digit integer to display
  // (values outside 0–9999 will wrap/truncate)
  // ============================================================
  void setNumber(int num);

  // ============================================================
  // Start blinking a specific digit
  // index: 0–3 (left → right)
  // delayMs: blink interval in milliseconds
  // ============================================================
  void startBlink(uint8_t index, unsigned long delayMs = 500);

  // ============================================================
  // Stop blinking a specific digit and force it visible
  // ============================================================
  void stopBlink(uint8_t index);

  // ============================================================
  // Disable blinking on all digits
  // ============================================================
  void clearBlink();

  // ============================================================
  // Set default blink delay (used when no custom delay provided)
  // ============================================================
  void setDefaultBlinkDelay(unsigned long ms);

  // ============================================================
  // Update blinking state (call frequently in loop)
  // Handles non-blocking timing and refreshes display
  // ============================================================
  void update();
};