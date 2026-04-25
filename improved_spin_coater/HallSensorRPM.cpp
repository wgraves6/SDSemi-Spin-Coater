#include "HallSensorRPM.h"

HallSensorRPM* HallSensorRPM::_instance = nullptr;

HallSensorRPM::HallSensorRPM(uint8_t pin, uint8_t magnetsPerRev)
  : _pin(pin),
    _magnetsPerRev(magnetsPerRev),
    _lastPulseTime(0),
    _period(0) {}

void HallSensorRPM::begin() {
    pinMode(_pin, INPUT_PULLUP);
    _instance = this;
    attachInterrupt(digitalPinToInterrupt(_pin), isrHandler, FALLING);
}

void HallSensorRPM::isrHandler() {
    if (_instance) _instance->handleInterrupt();
}

void HallSensorRPM::handleInterrupt() {
    unsigned long now = micros();

    // Debounce / ignore pulses that are too close together
    if (now - _lastPulseTime < _minPulseInterval) return;

    _period        = now - _lastPulseTime;
    _lastPulseTime = now;
}

float HallSensorRPM::getRPM() {
    noInterrupts();
    unsigned long period = _period;
    unsigned long lastTime = _lastPulseTime;
    interrupts();

    // Timeout: no pulses → RPM = 0
    if (micros() - lastTime > 500000) return 0;
    if (period == 0) return 0;

    // Convert period (microseconds per pulse) to RPM
    return (60.0f * 1000000.0f) / (period * _magnetsPerRev);
}

