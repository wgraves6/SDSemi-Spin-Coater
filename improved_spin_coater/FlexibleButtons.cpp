#include "FlexibleButtons.h"

// ---------- Init ----------

void FlexibleButtons::beginI2C(TwoWire &wire, uint8_t addr) {
  _mode = MODE_I2C;
  _wire = &wire;
  _addr = addr;
}

void FlexibleButtons::beginGPIO(uint8_t pinA, uint8_t pinB, uint8_t pinC, bool usePullups) {
  _mode = MODE_GPIO;

  _pinA = pinA;
  _pinB = pinB;
  _pinC = pinC;

  pinMode(_pinA, usePullups ? INPUT_PULLUP : INPUT);
  pinMode(_pinB, usePullups ? INPUT_PULLUP : INPUT);
  pinMode(_pinC, usePullups ? INPUT_PULLUP : INPUT);

  _invert = usePullups;
}

void FlexibleButtons::setDebounceTime(uint16_t ms) {
  _debounceMs = ms;
}

// ---------- Update ----------

void FlexibleButtons::update() {
  if (_mode == MODE_I2C) {
    updateI2C();
  } else {
    updateGPIO();
  }
}

// ---------- I2C ----------

void FlexibleButtons::updateI2C() {
  _wire->requestFrom(_addr, (uint8_t)4);

  uint8_t raw[4] = {0};
  int i = 0;

  while (_wire->available() && i < 4) {
    raw[i++] = _wire->read();
  }

  processButton(_btnA, raw[1] != 0);
  processButton(_btnB, raw[2] != 0);
  processButton(_btnC, raw[3] != 0);
}

// ---------- GPIO ----------

void FlexibleButtons::updateGPIO() {
  bool a = digitalRead(_pinA) ^ _invert;
  bool b = digitalRead(_pinB) ^ _invert;
  bool c = digitalRead(_pinC) ^ _invert;

  processButton(_btnA, a);
  processButton(_btnB, b);
  processButton(_btnC, c);
}

// ---------- Core Logic ----------

void FlexibleButtons::processButton(ButtonInternal &btn, bool newRaw) {
  unsigned long now = millis();

  // track raw change
  if (newRaw != btn.raw) {
    btn.raw = newRaw;
    btn.lastChangeTime = now;
  }

  btn.edge = NONE;

  // debounce check
  if ((now - btn.lastChangeTime) >= _debounceMs) {
    if (btn.debounced != btn.raw) {
      btn.last = btn.debounced;
      btn.debounced = btn.raw;

      // detect edge
      if (!btn.last && btn.debounced) {
        btn.edge = RISING;
        btn.toggle = !btn.toggle;
      }
      else if (btn.last && !btn.debounced) {
        btn.edge = FALLING;
      }
    }
  }
}

// ---------- Raw State ----------

bool FlexibleButtons::A() const { return _btnA.debounced; }
bool FlexibleButtons::B() const { return _btnB.debounced; }
bool FlexibleButtons::C() const { return _btnC.debounced; }

// ---------- Edge API ----------

bool FlexibleButtons::roseA() { return _btnA.edge == RISING; }
bool FlexibleButtons::fellA() { return _btnA.edge == FALLING; }

bool FlexibleButtons::roseB() { return _btnB.edge == RISING; }
bool FlexibleButtons::fellB() { return _btnB.edge == FALLING; }

bool FlexibleButtons::roseC() { return _btnC.edge == RISING; }
bool FlexibleButtons::fellC() { return _btnC.edge == FALLING; }

// ---------- Toggle API ----------

bool FlexibleButtons::toggleA() { return _btnA.toggle; }
bool FlexibleButtons::toggleB() { return _btnB.toggle; }
bool FlexibleButtons::toggleC() { return _btnC.toggle; }