#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Arduino.h>

class Joystick {
public:
    struct State {
        int rawX;
        int rawY;
        bool pressed;

        int normX;
        int normY;

        bool up;
        bool down;
        bool left;
        bool right;
        bool click;
    };

    Joystick(uint8_t pinX, uint8_t pinY, uint8_t pinSW);

    void begin();
    void update();

    const State& getState() const;

    void setDeadzone(int dz);
    void setCenter(int cx, int cy);
    void setInvert(bool invX, bool invY);

private:
    uint8_t _pinX, _pinY, _pinSW;

    int _centerX = 512;
    int _centerY = 512;
    int _deadzone = 60;

    bool _invertX = false;
    bool _invertY = false;

    bool _lastRawBtn = false;
    bool _stableBtn = false;
    unsigned long _lastDebounceTime = 0;
    const unsigned long _debounceDelay = 25;

    State _state;

    int normalize(int value, int center);
};

#endif