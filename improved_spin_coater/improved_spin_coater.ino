#include <Wire.h>
#include "OLEDLineDisplay.h"
#include "ModulinoKnob.h"
#include "MenuUI.h"
#include "SpinProfile.h"
#include "SpinProfilePage.h"
#include "XY160D.h"
#include "HallSensorRPM.h"
#include "RPMController.h"
#include "MotorMap.h"
#include "MotorCalibrator.h"
#include "Mpu6050.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ---- Defines ----
#define KNOB_ADDR 0x3A
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3D

// ---- Hardware ----
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);
OLEDLineDisplay oled(display, 4);
ModulinoKnob knob;
XY160D motor1(6, 7, 5);
HallSensorRPM sensor(2, 4);
RPMController rpmController;
const unsigned long SPIN_CONTROL_INTERVAL_MS = 20; // fixed PID tick rate (50 Hz)

// MPU6050 vibration monitor. Uses the default Wire bus (Wire1 is taken by
// the OLED + knob) at its default address (AD0 tied to GND).
MPU6050 imu(MPU6050_ADDRESS_AD0_LOW);
const unsigned long MPU_SAMPLE_INTERVAL_MS = 20;   // accel poll rate
const unsigned long MPU_PUBLISH_INTERVAL_MS = 1000; // telemetry publish cadence

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
// MQTT
// ================================================================

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

// Non-blocking MQTT connect/keepalive. A broker that's down or unreachable
// must never stall the rest of loop() - the motor, knob, and spin/calibrate
// state machine all have to keep running with or without MQTT. So instead of
// looping+delay()ing until connected, this makes at most one connect attempt
// per MQTT_RETRY_INTERVAL_MS and always returns immediately either way.
//
// mqttClient.connect() itself can still block for the PubSubClient socket
// timeout (defaults to 15s!) while it waits for a TCP handshake that's never
// coming, which reads as a "huge lag spike" every retry interval. setup()
// caps that via mqttClient.setSocketTimeout(2). The linkStatus() check below
// additionally skips the attempt outright (no blocking call at all) when the
// Ethernet cable is unplugged - the most common "not connected" case.
const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;

void maintainMqtt() {
  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    return;
  }

  static unsigned long lastAttempt = 0;
  unsigned long now = millis();
  if (now - lastAttempt < MQTT_RETRY_INTERVAL_MS) {
    return;
  }
  lastAttempt = now;

  Serial.print("Connecting to MQTT broker...");
  if (mqttClient.connect(mqttClientId, mqttUser, mqttPassword)) {
    Serial.println("connected.");
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" will retry.");
  }
}

// Writes `value` into `doc[key]` as raw JSON text with a fixed number of
// decimal places, guaranteeing the payload always contains a decimal point.
// Without this, Ignition's MQTT/JSON tag-creation can infer an Int tag from
// a whole-number float in the first payload, which then rejects/truncates
// every later fractional value.
void addFloatField(JsonDocument &doc, const char *key, float value, uint8_t decimals, char *buf, size_t bufSize) {
  MPU6050::formatFixed(value, decimals, buf, bufSize);
  doc[key] = serialized(buf);
}

// Publishes one telemetry payload (spin/calibrate profile metrics + vibration
// stats) once per MPU_PUBLISH_INTERVAL_MS window. Only called while a spin
// profile or calibration is actively running - `phase` distinguishes which
// (spin phases from phaseName(), or "Calibrate").
void publishTelemetry(const char *phase, float targetRPM, float actualRPM, unsigned long timeS) {
  MPU6050VibrationStats vibStats;
  if (!imu.updateVibration(vibStats)) {
    return;
  }

  JsonDocument doc;
  doc["phase"] = phase;
  char targetBuf[16], actualBuf[16];
  addFloatField(doc, "targetSpeed", targetRPM, 1, targetBuf, sizeof(targetBuf));
  addFloatField(doc, "actualSpeed", actualRPM, 1, actualBuf, sizeof(actualBuf));
  doc["time"] = timeS;

  char rmsBuf[16], peakBuf[16], crestBuf[16];
  addFloatField(doc, "rmsAccelG", vibStats.rmsAccelG, 4, rmsBuf, sizeof(rmsBuf));
  addFloatField(doc, "peakAccelG", vibStats.peakAccelG, 4, peakBuf, sizeof(peakBuf));
  addFloatField(doc, "crestFactor", vibStats.crestFactor, 4, crestBuf, sizeof(crestBuf));
  doc["sampleCount"] = vibStats.sampleCount;

  char payload[256];
  serializeJson(doc, payload);

  Serial.print("Publishing: ");
  Serial.println(payload);
  mqttClient.publish(mqttTopic, payload);
}

// ================================================================
// SETUP
// ================================================================

void setup() {
  Serial.begin(115200);

  Ethernet.begin(mac, ip, gateway, gateway, subnet);

  delay(1000);

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield not found. Check wiring/SPI connection.");
    return;
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable not connected.");
  }

  Serial.print("Arduino IP address: ");
  Serial.println(Ethernet.localIP());

  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setSocketTimeout(2); // cap connect() blocking time; default is 15s

  Wire1.begin();
  Wire1.setClock(400000); // Fast Mode - OLED full-buffer flushes are the UI's biggest stall
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

  Serial.println("Initializing MPU6050...");
  if (!imu.begin(MPU6050_ACCEL_RANGE_2G, MPU6050_GYRO_RANGE_250DPS, MPU6050_DLPF_44HZ)) {
    Serial.println("MPU6050 not found. Check wiring/address (SDA/SCL, power, AD0).");
  } else {
    Serial.println("MPU6050 found. Calibrating (keep the coater still)...");
    Wire.setClock(400000); // Fast Mode
    imu.calibrateGyro(500);
    imu.calibrateGravity(500);
    imu.setVibrationWindow(MPU_SAMPLE_INTERVAL_MS, MPU_PUBLISH_INTERVAL_MS);
    imu.beginVibrationWindow();
    Serial.println("MPU6050 calibration complete.");
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED failed");
    while (1)
      ;
  }

  oled.begin();
  menu.begin(oled, rootMenu, sizeof(rootMenu) / sizeof(rootMenu[0]));
}

// ================================================================
// LOOP helpers
// ================================================================

// Display constraint: this is a 128px-wide OLED in 4-line mode, which makes
// OLEDLineDisplay auto-select text size 2x (16px glyphs, filling the 64px
// height exactly across 4 lines - see OLEDLineDisplay's constructor). At
// that size, setText() caps each line at 128 / (6*2) = 10 characters -
// anything longer is silently truncated. Keep labels within that budget.
void updateOledSpin(float rpm) {
  // During PHASE_RAMP, lastTargetRPM() changes by ~1 RPM every few ms, which
  // would otherwise mark a line dirty almost every call. Adafruit_SSD1306's
  // display() always flushes the *entire* 1KB framebuffer (no partial
  // update), so that turned into a near-constant full I2C transfer and was
  // the main source of visible lag during a ramp. Throttling the whole OLED
  // refresh to a human-readable rate decouples it from how fast the
  // underlying values are actually changing.
  static unsigned long lastOledUpdate = 0;
  unsigned long now = millis();
  if (now - lastOledUpdate < 150) {
    return;
  }
  lastOledUpdate = now;

  oled.setText(0, "Ph:%s", phaseName(spinRunner.currentPhase()));
  oled.setText(1, "Left:%ds", spinRunner.phaseRemainingS());
  oled.setText(2, "Tgt:%d", spinRunner.lastTargetRPM());
  oled.setText(3, "Act:%d", (int)rpm);
  oled.render();
}

void updateOledCal(float rpm) {
  oled.setText(0, "Calibrate");
  oled.setText(1, "%d%%", MotorCalibrator_progress());
  oled.setText(2, "RPM:%d", (int)rpm);
  oled.setText(3, "PWM:%d", MotorCalibrator_currentPWM());
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

  maintainMqtt();

  switch (appState) {

    case APP_MENU:
      menu.update(delta, rosePressed);

      if (gDoEditProfile) {
        gDoEditProfile = false;
        profilePage.start(oled);
        appState = APP_EDIT_PROFILE;

      } else if (gDoStartSpin) {
        gDoStartSpin = false;
        {
          spinRunner.start(motor1, rpmController);
          imu.beginVibrationWindow();
          oled.clear();
          appState = APP_SPIN;
        }

      } else if (gDoCalibrate) {
        gDoCalibrate = false;
        motor1.Brake();
        rpmController.reset();
        MotorCalibrator_start();
        imu.beginVibrationWindow();
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

    case APP_SPIN: {
      // The PID's I/D terms are computed per-call, not per-second, so they
      // only mean what their tuned gains assume if update() runs at a
      // steady cadence. loop()'s actual period varies a lot (OLED/MQTT/I2C
      // work), so pin the control update to a fixed tick instead of calling
      // it on every raw pass - this keeps Ki/Kd behaving consistently
      // regardless of how much display/network work happens around it.
      static unsigned long lastCtrlTick = 0;
      unsigned long nowMs = millis();
      bool spinDone = false;
      if (nowMs - lastCtrlTick >= SPIN_CONTROL_INTERVAL_MS) {
        lastCtrlTick = nowMs;
        spinDone = !spinRunner.update(rpm);
      }

      if (spinDone) {
        appState = APP_MENU;
        menu.resetToRoot();
      } else {
        updateOledSpin(rpm);
        publishTelemetry(phaseName(spinRunner.currentPhase()),
                          spinRunner.lastTargetRPM(), rpm,
                          spinRunner.elapsedS());
      }
      break;
    }

    case APP_CALIBRATE:
      MotorCalibrator_update(rpm);
      if (!MotorCalibrator_isRunning()) {
        appState = APP_MENU;
        menu.resetToRoot();
      } else {
        updateOledCal(rpm);
        publishTelemetry("Calibrate", 0, rpm, MotorCalibrator_elapsedS());
      }
      break;
  }
}
