#include <Wire.h>

#define BUTTON_ADDR 0x3E

struct ButtonStates {
  bool A;
  bool B;
  bool C;
};

ButtonStates readButtons() {
  ButtonStates buttons = {false, false, false};

  Wire1.requestFrom(BUTTON_ADDR, 4); // read 4 bytes

  uint8_t raw[4] = {0, 0, 0, 0};
  int i = 0;
  while (Wire1.available() && i < 4) {
    raw[i++] = Wire1.read();
  }

  // Map buttons from bytes 1,2,3 (0-based indexing)
  buttons.A = raw[1] != 0;
  buttons.B = raw[2] != 0;
  buttons.C = raw[3] != 0;

  return buttons;
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Wire1.begin();
}

void loop() {
  ButtonStates btn = readButtons();

  Serial.print("A: "); Serial.print(btn.A);
  Serial.print(" | B: "); Serial.print(btn.B);
  Serial.print(" | C: "); Serial.println(btn.C);

  delay(200);
}