#include "SpinProfile.h"

SpinPhaseProfile spinPhase = {
    500,   // idleSpeed  RPM
    10,    // idleTime   s
    10,    // rampTime   s
    4000,  // finalSpeed RPM
    60,    // finalTime  s
};

const char* phaseName(int phase) {
    switch (phase) {
        case PHASE_IDLE:  return "Idle";
        case PHASE_RAMP:  return "Ramp";
        case PHASE_FINAL: return "Final";
        default:          return "???";
    }
}

// ----------------------------------------------------------------

static uint16_t phaseDuration(int phase) {
    switch (phase) {
        case PHASE_IDLE:  return spinPhase.idleTime;
        case PHASE_RAMP:  return spinPhase.rampTime;
        case PHASE_FINAL: return spinPhase.finalTime;
        default:          return 0;
    }
}

void SpinRunner::start(XY160D& motor, RPMController& ctrl) {
    _motor      = &motor;
    _ctrl       = &ctrl;
    _phase      = PHASE_IDLE;
    _phaseStart = millis();
    _runStart   = _phaseStart;
    _lastTarget = spinPhase.idleSpeed;
    _running    = true;
    _ctrl->reset();
}

bool SpinRunner::update(float rpm) {
    if (!_running) return false;

    unsigned long elapsedMs = millis() - _phaseStart;
    unsigned long totalMs   = (unsigned long)phaseDuration(_phase) * 1000UL;

    if (elapsedMs >= totalMs) {
        _phase++;
        if (_phase >= PHASE_COUNT) {
            _running = false;
            _motor->Brake();
            _ctrl->reset();
            return false;
        }
        _phaseStart = millis();
        _ctrl->reset();
        elapsedMs = 0;
        totalMs   = (unsigned long)phaseDuration(_phase) * 1000UL;
    }

    uint16_t target = 0;
    switch (_phase) {
        case PHASE_IDLE:
            target = spinPhase.idleSpeed;
            break;
        case PHASE_RAMP: {
            float t = (totalMs > 0) ? (float)elapsedMs / (float)totalMs : 1.0f;
            t = constrain(t, 0.0f, 1.0f);
            target = (uint16_t)(spinPhase.idleSpeed
                      + t * (float)((int)spinPhase.finalSpeed - (int)spinPhase.idleSpeed));
            break;
        }
        case PHASE_FINAL:
            target = spinPhase.finalSpeed;
            break;
    }

    _lastTarget = target;
    int pwm = _ctrl->update(target, rpm);
    _motor->Forward(pwm);
    return true;
}

int SpinRunner::currentPhase()    const { return _phase; }
int SpinRunner::lastTargetRPM()   const { return _lastTarget; }

int SpinRunner::phaseRemainingS() const {
    if (!_running) return 0;
    unsigned long elapsedMs = millis() - _phaseStart;
    unsigned long totalMs   = (unsigned long)phaseDuration(_phase) * 1000UL;
    if (elapsedMs >= totalMs) return 0;
    return (int)((totalMs - elapsedMs) / 1000UL);
}

unsigned long SpinRunner::elapsedS() const {
    if (!_running) return 0;
    return (millis() - _runStart) / 1000UL;
}
