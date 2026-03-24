#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3D
#define KNOB_ADDR 0x3A

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

int knobValue = 0;
bool buttonPressed = false;

void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire1.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  readKnob();

  // Serial debug
  Serial.print("Value: ");
  Serial.print(knobValue);
  Serial.print(" | Button: ");
  Serial.println(buttonPressed ? "PRESSED" : "released");

  // OLED display
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Knob + Button");

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(knobValue);

  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Btn: ");
  display.println(buttonPressed ? "ON" : "OFF");

  display.display();

  delay(100);
}

// ---- arduino knob ----
void readKnob() {
  Wire1.requestFrom(KNOB_ADDR, 4);  // FIX: request 4 bytes

  if (Wire1.available() >= 4) {
    Wire1.read(); // discard first byte (116 / ID)

    byte low  = Wire1.read();  // position low
    byte high = Wire1.read();  // position high
    byte button = Wire1.read(); // ACTUAL button byte

    knobValue = (high << 8) | low;

    // FIX: explicit check
    buttonPressed = (button == 1);
  }
}