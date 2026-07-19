#pragma once
#include <Arduino.h>
#include "OLEDLineDisplay.h"
#include "SpinProfile.h"

class SpinProfilePage {
public:
    void start(OLEDLineDisplay& oled);

    // Call every loop; returns true when done (return to menu)
    bool update(int delta, bool pressed);

private:
    enum State { PHASE_LIST, FIELD_SELECT, DIGIT_EDIT };

    OLEDLineDisplay*    _oled;
    State _state;

    int _listSel;
    int _listOff;

    int _editPhase;  // 0=idle 1=ramp 2=final
    int _editField;  // field index within the phase

    // Digit edit state
    int  _digitPos;
    int  _digits[4];
    int  _numDigits;
    bool _fieldIsRPM;  // true = 4-digit RPM, false = 3-digit time

    char        _labels[OLED_LIST_MAX_ITEMS][OLED_MAX_CHARS];
    const char* _labelPtrs[OLED_LIST_MAX_ITEMS];
    int         _labelCount;

    void enterPhaseList();
    void enterFieldSelect(int phase);
    void enterDigitEdit(int phase, int field);
    void saveDigits();
    void renderDigitEdit();

    // Helpers
    int      phaseFieldCount(int phase);
    uint16_t getFieldValue(int phase, int field);
    void     setFieldValue(int phase, int field, uint16_t val);
    bool     fieldIsRPM(int phase, int field);
    void     fieldLabel(int phase, int field, char* buf, int bufLen);
};
