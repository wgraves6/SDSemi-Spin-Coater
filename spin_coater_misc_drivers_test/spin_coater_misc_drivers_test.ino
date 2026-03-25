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

bool calibrating = false;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire1.begin();

  // ---- Init Drivers ----
  knob.begin(Wire1, KNOB_ADDR);

  buttons.beginI2C(Wire1, BUTTON_ADDR);
  buttons.setDebounceTime(25);

  // ---- initial guess lookup table ----
  MotorMap_init();

  rpmController.begin(
    MotorMap_get(),
    MotorMap_size());
  rpmController.setGains(0.035f, 0.0010f);
  rpmController.setKd(0.08f);
  rpmController.setRampRate(4);
  rpmController.setDeadband(15);

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

  float rpm = sensor.getRPM();

  // ---- CALIBRATION MODE ----
  if (MotorCalibrator_isRunning()) {

    calibrating = true;

    MotorCalibrator_update(rpm);

    // MotorCalibrator uses applyPWM(), so DO NOT run normal motor control

  } else {

    if (calibrating) {
      // just finished
      calibrating = false;
      Serial.println("Calibration finished, returning to normal mode");
    }

    handleMotor();
  }

  handleOledDisplay(now);
  handle4DigitDisplay(now);
  //handleDebug(now);
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

  // ---- Button B = start calibration ----
  if (buttons.roseB()) {
    calibrating = true;
    motorEnabled = false;  // force disable normal control
    motor1.Brake();

    MotorCalibrator_start();

    Serial.println("Calibration triggered");
  }
}

// ---------------- MOTOR CONTROL ----------------
void handleMotor() {
  if (motorEnabled) {
    int speed = constrain(inputs.knobValue, 0, 255) * 100;
    int pwm = rpmController.update(speed, sensor.getRPM());  //*.94 temp fix for rpm overshoot
    motor1.Forward(pwm);
  }
}

void applyPWM(int pwm) {
  motor1.Forward(pwm);
}

// ---------------- OLED ----------------
void handleOledDisplay(unsigned long now) {
  static unsigned long last = 0;
  if (now - last >= 80) {
    last = now;

    if (MotorCalibrator_isRunning()) {

      oled.setText(0, "CALIBRATING...");
      oled.setText(1, "RPM: %.0f", sensor.getRPM());
      oled.setText(2, "PWM sweep active");
      oled.setText(3, "Wait...");

    } else {

      oled.setText(0, "RPM: %.1f", sensor.getRPM());
      oled.setText(1, "Speed: %d", inputs.knobValue);
      oled.setText(2, motorEnabled ? "RUNNING" : "STOPPED");
      oled.setText(3, calibrating ? "DONE" : "");
    }

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