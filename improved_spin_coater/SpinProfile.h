#pragma once
#include <Arduino.h>
#include "XY160D.h"
#include "RPMController.h"

#define SPIN_PROFILE_MAX_STEPS 6

struct SpinStep {
    uint16_t rpm;       // target RPM (0–9999)
    uint16_t durationS; // duration in seconds (0–999)
};

extern SpinStep spinProfile[SPIN_PROFILE_MAX_STEPS];
extern int      spinProfileCount;

class SpinRunner {
public:
    void start(XY160D& motor, RPMController& ctrl);

    bool update(float rpm);  // returns false when finished
    int  currentStep()    const;
    int  stepRemainingS() const;

private:
    bool           _running   = false;
    int            _step      = 0;
    unsigned long  _stepStart = 0;
    XY160D*        _motor     = nullptr;
    RPMController* _ctrl      = nullptr;
};
