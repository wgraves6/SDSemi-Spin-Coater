#include "Joystick.h"

Joystick::Joystick(uint8_t pinX, uint8_t pinY, uint8_t pinSW)
    : _pinX(pinX), _pinY(pinY), _pinSW(pinSW) {}

void Joystick::begin() {
    pinMode(_pinSW, INPUT_PULLUP);
}

void Joystick::setDeadzone(int dz) {
    _deadzone = dz;
}

void Joystick::setCenter(int cx, int cy) {
    _centerX = cx;
    _centerY = cy;
}

void Joystick::setInvert(bool invX, bool invY) {
    _invertX = invX;
    _invertY = invY;
}

int Joystick::normalize(int value, int center) {
    int delta = value - center;

    if (abs(delta) < _deadzone) return 0;

    if (delta > 0) {
        return map(delta, _deadzone, 512, 0, 1000);
    } else {
        return map(delta, -_deadzone, -512, 0, -1000);
    }
}

void Joystick::update() {
    _state.rawX = analogRead(_pinX);
    _state.rawY = analogRead(_pinY);

    bool reading = !digitalRead(_pinSW); // active LOW

    if (reading != _lastRawBtn) {
        _lastDebounceTime = millis();
    }

    if ((millis() - _lastDebounceTime) > _debounceDelay) {
        if (reading != _stableBtn) {
            _stableBtn = reading;
            _state.click = _stableBtn;
        } else {
            _state.click = false;
        }
    }

    _lastRawBtn = reading;
    _state.pressed = _stableBtn;

    int nx = normalize(_state.rawX, _centerX);
    int ny = normalize(_state.rawY, _centerY);

    if (_invertX) nx = -nx;
    if (_invertY) ny = -ny;

    _state.normX = nx;
    _state.normY = ny;

    _state.left  = nx < -400;
    _state.right = nx >  400;
    _state.up    = ny >  400;
    _state.down  = ny < -400;
}

const Joystick::State& Joystick::getState() const {
    return _state;
}