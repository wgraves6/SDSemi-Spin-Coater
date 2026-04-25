#include <Wire.h>
#include "OLEDLineDisplay.h"
#include "ModulinoKnob.h"
#include "MenuUI.h"

// ---- Defines ----
#define KNOB_ADDR 0x3A
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3D

// ---- Display ----
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

// ---- Drivers ----
OLEDLineDisplay oled(display, 4);
ModulinoKnob knob;
MenuUI menu;

// ---- Knob tracking ----
int prevKnobValue = 0;
bool prevPressed = false;

// ================================================================
// Menu actions — replace stub Serial prints with real behaviour
// ================================================================

// Motor Control
void doStart() {
  Serial.println("[Motor] Start spin");
}
void doStop() {
  Serial.println("[Motor] Stop spin");
}
void doRPM1000() {
  Serial.println("[Motor] Set 1000 RPM");
}
void doRPM2000() {
  Serial.println("[Motor] Set 2000 RPM");
}
void doRPM3000() {
  Serial.println("[Motor] Set 3000 RPM");
}

// Calibration
void doCalibrate() {
  Serial.println("[Cal] Run calibration");
}
void doResetMap() {
  Serial.println("[Cal] Reset motor map");
}

// Settings
void doRampRate() {
  Serial.println("[Settings] Ramp rate");
}
void doDeadband() {
  Serial.println("[Settings] Deadband");
}
void doResetPID() {
  Serial.println("[Settings] Reset PID");
}

// About
void doAbout() {
  Serial.println("[About] Spin Coater v1.0");
}

// ================================================================
// Menu tree — defined bottom-up so parent arrays can reference children
// ================================================================

const MenuItem motorMenu[] = {
  MENU_ACTION("Start Spin", doStart),
  MENU_ACTION("Stop Spin", doStop),
  MENU_ACTION("1000 RPM", doRPM1000),
  MENU_ACTION("2000 RPM", doRPM2000),
  MENU_ACTION("3000 RPM", doRPM3000),
};

const MenuItem calMenu[] = {
  MENU_ACTION("Run Cal", doCalibrate),
  MENU_ACTION("Reset Map", doResetMap),
};

const MenuItem settingsMenu[] = {
  MENU_ACTION("Ramp Rate", doRampRate),
  MENU_ACTION("Deadband", doDeadband),
  MENU_ACTION("Reset PID", doResetPID),
};

const MenuItem rootMenu[] = {
  MENU_SUBMENU("Motor Ctrl", motorMenu),
  MENU_SUBMENU("Calibrate", calMenu),
  MENU_SUBMENU("Settings", settingsMenu),
  MENU_ACTION("About", doAbout),
};

// ================================================================

void setup() {
  Serial.begin(9600);
  delay(1000);

  Wire1.begin();
  knob.begin(Wire1, KNOB_ADDR);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED failed");
    while (1)
      ;
  }

  oled.begin();
  menu.begin(oled, rootMenu, sizeof(rootMenu) / sizeof(rootMenu[0]));
}

void loop() {
  knob.update();

  int delta = knob.value() - prevKnobValue;
  prevKnobValue = knob.value();

  bool rosePressed = knob.pressed() && !prevPressed;
  prevPressed = knob.pressed();

  menu.update(delta, rosePressed);
}
