#include "XY160D.h"

XY160D::XY160D(int in1, int in2, int en) {
    _in1 = in1;
    _in2 = in2;
    _en = en;

    pinMode(_in1, OUTPUT);
    pinMode(_in2, OUTPUT);
    pinMode(_en, OUTPUT);
}

void XY160D::Forward(int speed) {
    digitalWrite(_in1, HIGH);
    digitalWrite(_in2, LOW);
    analogWrite(_en, speed);
}

void XY160D::Backward(int speed) {
    digitalWrite(_in1, LOW);
    digitalWrite(_in2, HIGH);
    analogWrite(_en, speed);
}

void XY160D::Brake() {
    digitalWrite(_in1, LOW);
    digitalWrite(_in2, LOW);
}