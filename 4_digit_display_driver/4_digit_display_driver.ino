#include <TM1637Display.h>

#define CLK 9
#define DIO 10

class TM1637BlinkerDigit {
  private:
    TM1637Display display;
    int digits[4];           // Store all 4 digits
    int blinkDigitIndex;     // Which digit to blink (0-3)
    unsigned long delayTime; // Blink speed
    bool isOn;               // Current state of the blinking digit
    unsigned long lastMillis;

  public:
    TM1637BlinkerDigit(uint8_t clkPin, uint8_t dioPin) : display(clkPin, dioPin) {
      for (int i = 0; i < 4; i++) digits[i] = 0;
      blinkDigitIndex = 0;
      delayTime = 500;
      isOn = false;
      lastMillis = 0;
    }

    void begin(uint8_t brightness = 0x0f) {
      display.setBrightness(brightness);
      display.clear();
    }

    void setNumber(int num) {
      // Break number into digits
      digits[0] = (num / 1000) % 10;
      digits[1] = (num / 100) % 10;
      digits[2] = (num / 10) % 10;
      digits[3] = num % 10;
      // Update display immediately
      showDigits();
    }

    // 0-indexed
    void setBlinkDigit(int index) {
      if (index >= 0 && index < 4) blinkDigitIndex = index;
    }

    void setBlinkDelay(unsigned long ms) {
      delayTime = ms;
    }

    void update() {
      unsigned long currentMillis = millis();
      if (currentMillis - lastMillis >= delayTime) {
        lastMillis = currentMillis;
        if (isOn) {
          display.setSegments(getSegmentsArray(), 4); // Show number with blinked digit off
          isOn = false;
        } else {
          showDigits(); // Show full number
          isOn = true;
        }
      }
    }

    void stop() {
      display.clear();
      isOn = false;
    }

  private:
    void showDigits() {
      display.setSegments(getSegmentsArray(), 4);
    }

    uint8_t* getSegmentsArray() {
      static uint8_t segs[4];
      for (int i = 0; i < 4; i++) {
        if (i == blinkDigitIndex && !isOn) {
          segs[i] = 0; // turn off the blinking digit
        } else {
          segs[i] = display.encodeDigit(digits[i]);
        }
      }
      return segs;
    }
};

// Create a blinker instance
TM1637BlinkerDigit blinker(CLK, DIO);

void setup() {
  blinker.begin();
  blinker.setNumber(1234);   // Number to display
  blinker.setBlinkDigit(2);  // Blink the 3rd digit (0-indexed)
  blinker.setBlinkDelay(500); // Blink speed
}

void loop() {
  blinker.update();
}