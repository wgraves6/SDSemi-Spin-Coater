#include "Joystick.h"

Joystick js(A0, A1, 12);

void setup() {
    Serial.begin(9600);
    js.begin();
}

void loop() {
    js.update();
    auto s = js.getState();

    Serial.print("X:"); Serial.print(s.normX);
    Serial.print(" Y:"); Serial.print(s.normY);
    Serial.print(" Btn:"); Serial.print(s.pressed);

    if (s.up) Serial.print(" UP");
    if (s.down) Serial.print(" DOWN");
    if (s.left) Serial.print(" LEFT");
    if (s.right) Serial.print(" RIGHT");
    if (s.click) Serial.print(" CLICK");

    Serial.println();
    delay(50);
}