#include "ModulinoKnob.h"

void ModulinoKnob::begin(TwoWire &wire, uint8_t addr) {
  _wire = &wire;
  _addr = addr;
}

void ModulinoKnob::update() {
  _wire->requestFrom(_addr, (uint8_t)4);

  if (_wire->available() >= 4) {
    _wire->read();
    byte low  = _wire->read();
    byte high = _wire->read();
    byte btn  = _wire->read();

    _value = (high << 8) | low;
    _pressed = (btn != 0);
  }
}

int ModulinoKnob::value() const { return _value; }
bool ModulinoKnob::pressed() const { return _pressed; }