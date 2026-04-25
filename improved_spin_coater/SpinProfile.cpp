#include "SpinProfile.h"

// Default three-step profile
SpinStep spinProfile[SPIN_PROFILE_MAX_STEPS] = {
    { 500,  30 },
    { 4000, 60 },
};
int spinProfileCount = 2;

// ----------------------------------------------------------------

void SpinRunner::start(XY160D& motor, RPMController& ctrl) {
    _motor    = &motor;
    _ctrl     = &ctrl;
    _step     = 0;
    _running  = (spinProfileCount > 0);
    if (_running) {
        _stepStart = millis();
        _ctrl->reset();
    }
}

bool SpinRunner::update(float rpm) {
    if (!_running) return false;

    SpinStep& s = spinProfile[_step];

    // Advance to next step when duration elapses
    if (millis() - _stepStart >= (unsigned long)s.durationS * 1000UL) {
        _step++;
        if (_step >= spinProfileCount) {
            _running = false;
            _motor->Brake();
            _ctrl->reset();
            return false;
        }
        _stepStart = millis();
        _ctrl->reset();
    }

    int pwm = _ctrl->update(spinProfile[_step].rpm, rpm);
    _motor->Forward(pwm);
    return true;
}

bool SpinRunner::isRunning()   const { return _running; }
int  SpinRunner::currentStep() const { return _step; }

int SpinRunner::stepRemainingS() const {
    if (!_running || _step >= spinProfileCount) return 0;
    unsigned long elapsedMs = millis() - _stepStart;
    unsigned long totalMs   = (unsigned long)spinProfile[_step].durationS * 1000UL;
    if (elapsedMs >= totalMs) return 0;
    return (int)((totalMs - elapsedMs) / 1000UL);
}
