#pragma once
#include <Arduino.h>
#include <Wire.h>

class ModulinoKnob {
public:
  void begin(TwoWire &wire, uint8_t addr);
  void update();

  int value() const;
  bool pressed() const;

private:
  TwoWire* _wire;
  uint8_t _addr;

  int _value;
  bool _pressed;
};