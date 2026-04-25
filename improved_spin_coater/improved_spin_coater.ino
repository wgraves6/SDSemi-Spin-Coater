#include <Wire.h>
#include "OLEDLineDisplay.h"
#include "TM1637BlinkerDigit.h"
#include "ModulinoKnob.h"
#include "MenuUI.h"
#include "SpinProfile.h"
#include "SpinProfilePage.h"
#include "XY160D.h"
#include "HallSensorRPM.h"
#include "RPMController.h"
#include "MotorMap.h"
#include "MotorCalibrator.h"

// ---- Defines ----
#define CLK 9
#define DIO 10
#define KNOB_ADDR 0x3A
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3D

// ---- Hardware ----
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);
TM1637BlinkerDigit blinker(CLK, DIO);
OLEDLineDisplay oled(display, 4);
ModulinoKnob knob;
XY160D motor1(6, 7, 5);
HallSensorRPM sensor(2, 4);
RPMController rpmController;

// ---- App state machine ----
enum AppState { APP_MENU,
                APP_EDIT_PROFILE,
                APP_SPIN,
                APP_CALIBRATE };
AppState appState = APP_MENU;

MenuUI menu;
SpinProfilePage profilePage;
SpinRunner spinRunner;

// ---- Knob tracking ----
int prevKnobValue = 0;
bool prevPressed = false;

// ---- Deferred flags set by menu callbacks ----
bool gDoEditProfile = false;
bool gDoStartSpin = false;
bool gDoCalibrate = false;

// ================================================================
// Menu callbacks
// ================================================================

void doEditProfile() {
  gDoEditProfile = true;
}
void doStartSpin() {
  gDoStartSpin = true;
}
void doCalibrate() {
  gDoCalibrate = true;
}

void applyPWM(int pwm) {
  motor1.Forward(pwm);
}

// ================================================================
// Menu tree
// ================================================================

const MenuItem confirmSpinMenu[] = {
  MENU_ACTION("Confirm", doStartSpin),
};

const MenuItem confirmCalMenu[] = {
  MENU_ACTION("Confirm", doCalibrate),
};

const MenuItem rootMenu[] = {
  MENU_SUBMENU("Start Spin", confirmSpinMenu),
  MENU_ACTION("Edit", doEditProfile),
  MENU_SUBMENU("Calibrate", confirmCalMenu),
};

// ================================================================
// SETUP
// ================================================================

void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire1.begin();
  knob.begin(Wire1, KNOB_ADDR);

  MotorMap_init();
  rpmController.begin(MotorMap_get(), MotorMap_size());
  rpmController.setGains(0.035f, 0.0010f);
  rpmController.setKd(0.08f);
  rpmController.setRampRate(4);
  rpmController.setDeadband(15);

  sensor.begin();
  motor1.Brake();
  MotorCalibrator_setPWMCallback(applyPWM);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED failed");
    while (1)
      ;
  }

  blinker.begin();
  blinker.setNumber(0);

  oled.begin();
  menu.begin(oled, rootMenu, sizeof(rootMenu) / sizeof(rootMenu[0]));
}

// ================================================================
// LOOP helpers
// ================================================================

void updateOledSpin(float rpm) {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 100) return;
  last = now;

  oled.setText(0, "SPINNING");
  oled.setText(1, "Step:%d/%d", spinRunner.currentStep() + 1, spinProfileCount);
  oled.setText(2, "Left:%ds", spinRunner.stepRemainingS());
  oled.setText(3, "RPM:%d", (int)rpm);
  oled.render();
}

void updateOledCal() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 200) return;
  last = now;

  oled.setText(0, "CALIBRATING");
  oled.setText(1, "%d%%", MotorCalibrator_progress());
  oled.setText(2, "Please wait");
  oled.setText(3, "");
  oled.render();
}

// ================================================================
// LOOP
// ================================================================

void loop() {
  knob.update();
  float rpm = sensor.getRPM();

  int delta = knob.value() - prevKnobValue;
  prevKnobValue = knob.value();
  bool rosePressed = knob.pressed() && !prevPressed;
  prevPressed = knob.pressed();

  switch (appState) {

    case APP_MENU:
      menu.update(delta, rosePressed);

      if (gDoEditProfile) {
        gDoEditProfile = false;
        profilePage.start(oled);
        appState = APP_EDIT_PROFILE;

      } else if (gDoStartSpin) {
        gDoStartSpin = false;
        if (spinProfileCount > 0) {
          spinRunner.start(motor1, rpmController);
          oled.clear();
          appState = APP_SPIN;
        }

      } else if (gDoCalibrate) {
        gDoCalibrate = false;
        motor1.Brake();
        rpmController.reset();
        MotorCalibrator_start();
        oled.clear();
        appState = APP_CALIBRATE;
      }
      break;

    case APP_EDIT_PROFILE:
      if (profilePage.update(delta, rosePressed)) {
        appState = APP_MENU;
        menu.redraw();
      }
      break;

    case APP_SPIN:
      if (!spinRunner.update(rpm)) {
        appState = APP_MENU;
        menu.redraw();
      } else {
        updateOledSpin(rpm);
        blinker.setNumber((int)rpm);
      }
      break;

    case APP_CALIBRATE:
      MotorCalibrator_update(rpm);
      if (!MotorCalibrator_isRunning()) {
        appState = APP_MENU;
        menu.redraw();
      } else {
        updateOledCal();
      }
      break;
  }
}
