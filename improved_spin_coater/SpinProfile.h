#pragma once
#include <Arduino.h>
#include "XY160D.h"
#include "RPMController.h"

// Fixed 3-phase spin profile
struct SpinPhaseProfile {
    uint16_t idleSpeed;   // RPM during idle
    uint16_t idleTime;    // seconds
    uint16_t rampTime;    // seconds to ramp from idle to final speed
    uint16_t finalSpeed;  // RPM during final spin
    uint16_t finalTime;   // seconds
};

extern SpinPhaseProfile spinPhase;

#define PHASE_IDLE  0
#define PHASE_RAMP  1
#define PHASE_FINAL 2
#define PHASE_COUNT 3

const char* phaseName(int phase);

class SpinRunner {
public:
    void start(XY160D& motor, RPMController& ctrl);
    bool update(float rpm);   // returns false when finished
    int  currentPhase()    const;
    int  phaseRemainingS() const;
    int  lastTargetRPM()   const;
    unsigned long elapsedS() const;

private:
    bool           _running     = false;
    int            _phase       = 0;
    unsigned long  _phaseStart  = 0;
    unsigned long  _runStart    = 0;
    int            _lastTarget  = 0;
    XY160D*        _motor       = nullptr;
    RPMController* _ctrl        = nullptr;
};
