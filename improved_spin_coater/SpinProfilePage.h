#pragma once
#include <Arduino.h>
#include "OLEDLineDisplay.h"
#include "TM1637BlinkerDigit.h"
#include "SpinProfile.h"

class SpinProfilePage {
public:
    void start(OLEDLineDisplay& oled, TM1637BlinkerDigit& blinker);

    // Call every loop; returns true when the page is done (return to menu)
    bool update(int delta, bool pressed);

private:
    enum State { STEP_LIST, FIELD_SELECT, DIGIT_EDIT };

    OLEDLineDisplay*    _oled;
    TM1637BlinkerDigit* _blinker;
    State _state;

    // Shared scroll state for list modes
    int _listSel;
    int _listOff;

    // Which step and field are being edited
    int _editStep;
    int _editField;   // 0 = rpm, 1 = duration

    // Digit edit state
    int _digitPos;
    int _digits[4];
    int _numDigits;

    // Label storage for list modes
    char        _labels[OLED_LIST_MAX_ITEMS][OLED_MAX_CHARS];
    const char* _labelPtrs[OLED_LIST_MAX_ITEMS];
    int         _labelCount;

    void enterStepList();
    void enterFieldSelect();
    void enterDigitEdit(int field);
    void saveDigits();
    void renderDigitEdit();
};
