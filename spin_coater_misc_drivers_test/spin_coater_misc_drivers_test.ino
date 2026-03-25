#include <Wire.h>
#include "OLEDLineDisplay.h"
#include "TM1637BlinkerDigit.h"
#include "ModulinoKnob.h"
#include "FlexibleButtons.h"
#include "XY160D.h"
#include "HallSensorRPM.h"
#include "RPMController.h"
#include "MotorMap.h"
#include "MotorCalibrator.h"

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
XY160D motor1(6, 7, 5);
RPMController rpmController;

// ---- RPM Sensor ----
HallSensorRPM sensor(2, 4);

// ---- Input State ----
struct InputState {
  int knobValue;
  bool buttonA;
  bool buttonB;
  bool buttonC;
};

InputState inputs;

// ---- Motor ----
bool motorEnabled = false;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire1.begin();

  knob.begin(Wire1, KNOB_ADDR);

  buttons.beginI2C(Wire1, BUTTON_ADDR);
  buttons.setDebounceTime(25);

  MotorMap_init();

  rpmController.begin(MotorMap_get(), MotorMap_size());
  rpmController.setGains(0.035f, 0.0010f);
  rpmController.setKd(0.08f);
  rpmController.setRampRate(4);
  rpmController.setDeadband(15);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED failed");
    while (1);
  }

  oled.begin();

  blinker.begin();
  blinker.setNumber(8888);

  motor1.Brake();
  sensor.begin();

  // ---- Hook calibrator to motor ----
  MotorCalibrator_setPWMCallback(applyPWM);
}

// ---------------- LOOP ----------------
void loop() {
  unsigned long now = millis();

  processInputs();

  float rpm = sensor.getRPM();

  // ---- Calibration has priority ----
  if (MotorCalibrator_isRunning()) {
    MotorCalibrator_update(rpm);
  } else {
    handleMotor(rpm);
  }

  handleOledDisplay(now, rpm);
  handle4DigitDisplay(now, rpm);
}

// ---------------- INPUT ----------------
void processInputs() {
  knob.update();
  buttons.update();

  inputs.knobValue = knob.value();
  inputs.buttonA = buttons.A();
  inputs.buttonB = buttons.B();
  inputs.buttonC = buttons.C();

  // ---- Enable / Disable ----
  if (buttons.roseA()) {
    motorEnabled = true;
    Serial.println("Motor ENABLED");
  }

  if (buttons.fellA()) {
    motorEnabled = false;
    motor1.Brake();
    Serial.println("Motor DISABLED");
  }

  // ---- Start calibration (only if idle) ----
  if (buttons.roseB() && !MotorCalibrator_isRunning()) {
    motorEnabled = false;
    motor1.Brake();

    MotorCalibrator_start();
    Serial.println("Calibration START");
  }
}

// ---------------- MOTOR ----------------
void handleMotor(float rpm) {
  if (!motorEnabled) return;

  int targetRPM = constrain(inputs.knobValue, 0, 255) * 100;
  int pwm = rpmController.update(targetRPM, rpm);

  motor1.Forward(pwm);
}

void applyPWM(int pwm) {
  motor1.Forward(pwm);
}

// ---------------- OLED ----------------
void handleOledDisplay(unsigned long now, float rpm) {
  static unsigned long last = 0;
  if (now - last < 80) return;
  last = now;

  if (MotorCalibrator_isRunning()) {
    oled.setText(0, "CALIBRATING");
    oled.setText(1, "RPM: %.0f", rpm);
    oled.setText(2, "%d%%", MotorCalibrator_progress());
    oled.setText(3, "Please wait");
  } else if (MotorCalibrator_isDone()) {
    oled.setText(0, "CAL COMPLETE");
    oled.setText(1, "RPM: %.0f", rpm);
    oled.setText(2, "Press B to rerun");
    oled.setText(3, "");
  } else {
    oled.setText(0, "RPM");
    oled.setText(1, "Act: %.0f", rpm);
    oled.setText(2, "Targ: %d", inputs.knobValue*100);
    oled.setText(3, motorEnabled ? "RUNNING" : "STOPPED");
  }

  oled.render();
}

// ---------------- 4-DIGIT ----------------
void handle4DigitDisplay(unsigned long now, float rpm) {
  static unsigned long last = 0;
  if (now - last < 80) return;
  last = now;

  blinker.setNumber((int)rpm);
}