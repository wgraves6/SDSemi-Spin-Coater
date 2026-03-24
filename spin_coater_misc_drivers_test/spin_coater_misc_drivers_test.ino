#include <Wire.h>
#include "OLEDLineDisplay.h"
#include "TM1637BlinkerDigit.h"
#include "ModulinoKnob.h"
#include "FlexibleButtons.h"
#include "XY160D.h"
#include "HallSensorRPM.h"
#include "RPMController.h"

// ---- Defines ----
#define CLK 9
#define DIO 10
#define KNOB_ADDR 0x3A
#define BUTTON_ADDR 0x3E
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3D

// ---- Display ----
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

// ---- Drivers ----
TM1637BlinkerDigit blinker(CLK, DIO);
OLEDLineDisplay oled(display, 4);
ModulinoKnob knob;
FlexibleButtons buttons;
XY160D motor1(7, 6, 5);
RPMController rpmController;

// ---- RPM Sensor ----
HallSensorRPM sensor(2, 4);  // pin 2, 4 magnets per rev

// ---- Combined Input State ----
struct InputState {
  int knobValue;
  bool knobButton;
  bool buttonA;
  bool buttonB;
  bool buttonC;
};

InputState inputs;

// ---- Motor state ----
bool motorEnabled = false;

// ---- initial guess lookup table ----
PWMRPMPoint motorMap[] = {
  { 0, 0 },
  { 5, 0 },
  { 10, 0 },
  { 15, 0 },
  { 20, 0 },
  { 25, 0 },

  { 30, 250 },
  { 35, 575 },
  { 40, 800 },
  { 45, 1100 },
  { 50, 1350 },
  { 55, 1550 },
  { 60, 1750 },
  { 65, 1950 },
  { 70, 2100 },
  { 75, 2200 },
  { 80, 2350 },
  { 85, 2450 },
  { 90, 2550 },
  { 95, 2650 },
  { 100, 2750 },
  { 105, 2850 },
  { 110, 2950 },
  { 115, 3000 },
  { 120, 3100 },
  { 125, 3150 },
  { 130, 3200 },
  { 135, 3250 },
  { 140, 3300 },
  { 145, 3350 },
  { 150, 3400 },
  { 155, 3450 },
  { 160, 3450 },
  { 165, 3450 },
  { 170, 3500 },
  { 175, 3550 },
  { 180, 3550 },
  { 185, 3550 },
  { 190, 3600 },
  { 195, 3600 },
  { 200, 3650 },
  { 205, 3650 },
  { 210, 3650 },
  { 215, 3700 },
  { 220, 3700 },
  { 225, 3700 },
  { 230, 3700 },
  { 235, 3700 },
  { 240, 3750 },
  { 245, 3750 },
  { 250, 3750 },
  { 255, 3850 }
};

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire1.begin();

  // ---- Init Drivers ----
  knob.begin(Wire1, KNOB_ADDR);

  buttons.beginI2C(Wire1, BUTTON_ADDR);
  buttons.setDebounceTime(25);


  rpmController.begin(motorMap, sizeof(motorMap) / sizeof(motorMap[0]));
  rpmController.setGains(0.04, 0.0015);
  rpmController.setOvershootLimit(1.10);  // never exceed +10%
  rpmController.setRampRate(4);           // smooth PWM changes
  rpmController.setDeadband(25);          // reduce jitter
  rpmController.setOutputLimits(0, 255);
  rpmController.setCorrectiveScalar(.94);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED failed");
    while (1)
      ;
  }

  oled.begin();

  blinker.begin();
  blinker.setNumber(8888);

  // ---- Motor ----
  motor1.Brake();

  // ---- RPM Sensor ----
  sensor.begin();
}

// ---------------- LOOP ----------------
void loop() {
  unsigned long now = millis();

  processInputs();

  handleMotor();  // NEW: continuous knob control
  handleOledDisplay(now);
  handle4DigitDisplay(now);
  handleDebug(now);
}

// ---------------- INPUT PROCESSING ----------------
void processInputs() {
  knob.update();
  buttons.update();

  inputs.knobValue = knob.value();  // assume 0–255
  inputs.knobButton = knob.pressed();

  inputs.buttonA = buttons.A();
  inputs.buttonB = buttons.B();
  inputs.buttonC = buttons.C();

  // ---- Button A = enable/disable motor ----
  if (buttons.roseA()) {
    motorEnabled = true;
    Serial.println("Motor ENABLED");
  }

  if (buttons.fellA()) {
    motorEnabled = false;
    motor1.Brake();
    Serial.println("Motor DISABLED");
  }
}

// ---------------- MOTOR CONTROL ----------------
void handleMotor() {
  if (motorEnabled) {
    int speed = constrain(inputs.knobValue, 0, 255) * 100;
    int pwm = rpmController.update(speed, sensor.getRPM()); //*.94 temp fix for rpm overshoot
    motor1.Forward(pwm);
  }
}

// ---------------- OLED ----------------
void handleOledDisplay(unsigned long now) {
  static unsigned long last = 0;
  if (now - last >= 80) {
    last = now;

    oled.setText(0, "RPM: %.1f", sensor.getRPM());
    oled.setText(1, "Speed: %d", inputs.knobValue);
    oled.setText(2, motorEnabled ? "RUNNING" : "STOPPED");
    oled.render();
  }
}

// ---------------- 4-DIGIT DISPLAY ----------------
void handle4DigitDisplay(unsigned long now) {
  static unsigned long last = 0;
  if (now - last >= 80) {
    last = now;

    blinker.setNumber((int)sensor.getRPM());
  }
}

// ---------------- DEBUG ----------------
void handleDebug(unsigned long now) {
  static unsigned long last = 0;
  if (now - last >= 200) {
    last = now;

    Serial.print("Knob: ");
    Serial.print(inputs.knobValue);
    Serial.print(" | RPM: ");
    Serial.print(sensor.getRPM());
    Serial.print(" | Enabled: ");
    Serial.print(motorEnabled);
    Serial.println();
  }
}