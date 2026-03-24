#ifndef XY160D_H
#define XY160D_H

#include <Arduino.h>

class XY160D {
public:
    // Constructor: pass the pin numbers for the motor
    XY160D(int in1, int in2, int en);

    // Motor control methods
    void Forward(int speed);
    void Backward(int speed);
    void Brake();

private:
    int _in1;
    int _in2;
    int _en;
};

#endif // XY160D_H