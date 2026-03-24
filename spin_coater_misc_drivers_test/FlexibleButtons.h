#pragma once
#include <Arduino.h>
#include <Wire.h>
/******************************************************************************
 * FLEXIBLE BUTTONS CONFIGURATION
 *
 * Instructions:
 * -------------
 * The FlexibleButtons driver supports two modes:
 *
 * 1️ I2C Mode (buttons connected via Qwiic/I2C bus)
 *    - Call beginI2C(TwoWire &wire, uint8_t addr) in setup():
 *
 *        buttons.beginI2C(Wire1, BUTTON_ADDR);
 *        buttons.setDebounceTime(25); // optional, default is 20 ms
 *
 *    - Make sure the buttons are wired to the correct I2C bus
 *      and have the proper I2C address.
 *
 * 2️ GPIO Mode (buttons connected to Arduino digital pins)
 *    - Call beginGPIO(uint8_t pinA, uint8_t pinB, uint8_t pinC, bool usePullups)
 *      in setup():
 *
 *        buttons.beginGPIO(BTN_A_PIN, BTN_B_PIN, BTN_C_PIN, true);
 *        buttons.setDebounceTime(25); // optional
 *
 *      - Set 'usePullups' to true if using INPUT_PULLUP wiring (active-low buttons)
 *      - Set 'usePullups' to false if using active-high buttons with external pull-downs
 *
 * Public API:
 * -----------
 *  - Raw button states:
 *        buttons.A(), buttons.B(), buttons.C()
 *
 *  - Edge detection:
 *        buttons.roseA(), buttons.fellA()
 *        buttons.roseB(), buttons.fellB()
 *        buttons.roseC(), buttons.fellC()
 *
 *  - Toggle (ON/OFF latch):
 *        buttons.toggleA(), buttons.toggleB(), buttons.toggleC()
 *
 * Notes:
 * ------
 *  - Always call buttons.update() in your main loop to update debounced
 *    states, edges, and toggles.
 *  - You can switch between I2C and GPIO modes by changing the initialization
 *    method in setup() without changing the rest of your code.
 ******************************************************************************/
class FlexibleButtons {
public:
  enum Mode {
    MODE_I2C,
    MODE_GPIO
  };

  enum Edge {
    NONE,
    RISING,
    FALLING
  };

  // ---- Init ----
  void beginI2C(TwoWire &wire, uint8_t addr);
  void beginGPIO(uint8_t pinA, uint8_t pinB, uint8_t pinC, bool usePullups = true);

  void setDebounceTime(uint16_t ms); // default ~20ms
  void update();

  // ---- Raw State ----
  bool A() const;
  bool B() const;
  bool C() const;

  // ---- Edge Detection ----
  bool roseA();
  bool fellA();

  bool roseB();
  bool fellB();

  bool roseC();
  bool fellC();

  // ---- Toggle ----
  bool toggleA();
  bool toggleB();
  bool toggleC();

private:
  struct ButtonInternal {
    bool raw = false;        // immediate read
    bool debounced = false;  // stable state
    bool last = false;       // previous debounced
    bool toggle = false;

    unsigned long lastChangeTime = 0;

    Edge edge = NONE;
  };

  void updateI2C();
  void updateGPIO();
  void processButton(ButtonInternal &btn, bool newRaw);

  // ---- Config ----
  Mode _mode = MODE_GPIO;
  uint16_t _debounceMs = 20;

  // ---- I2C ----
  TwoWire* _wire = nullptr;
  uint8_t _addr = 0;

  // ---- GPIO ----
  uint8_t _pinA = 255;
  uint8_t _pinB = 255;
  uint8_t _pinC = 255;
  bool _invert = false;

  // ---- Buttons ----
  ButtonInternal _btnA;
  ButtonInternal _btnB;
  ButtonInternal _btnC;
};