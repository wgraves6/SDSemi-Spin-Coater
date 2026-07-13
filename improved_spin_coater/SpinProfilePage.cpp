#include "SpinProfilePage.h"

// ---- Phase field layout ----------------------------------------
//
//  Phase 0 (Idle):  field 0 = idleSpeed (RPM, 4 dig)
//                   field 1 = idleTime  (sec, 3 dig)
//  Phase 1 (Ramp):  field 0 = rampTime  (sec, 3 dig)
//  Phase 2 (Final): field 0 = finalSpeed (RPM, 4 dig)
//                   field 1 = finalTime  (sec, 3 dig)
//
// ----------------------------------------------------------------

int SpinProfilePage::phaseFieldCount(int phase) {
    return (phase == PHASE_RAMP) ? 1 : 2;
}

bool SpinProfilePage::fieldIsRPM(int phase, int field) {
    if (phase == PHASE_RAMP) return false;  // rampTime is always a time field
    return (field == 0);                    // first field of idle/final is speed
}

uint16_t SpinProfilePage::getFieldValue(int phase, int field) {
    switch (phase) {
        case PHASE_IDLE:  return (field == 0) ? spinPhase.idleSpeed  : spinPhase.idleTime;
        case PHASE_RAMP:  return spinPhase.rampTime;
        case PHASE_FINAL: return (field == 0) ? spinPhase.finalSpeed : spinPhase.finalTime;
        default:          return 0;
    }
}

void SpinProfilePage::setFieldValue(int phase, int field, uint16_t val) {
    switch (phase) {
        case PHASE_IDLE:
            if (field == 0) spinPhase.idleSpeed = val;
            else            spinPhase.idleTime  = val;
            break;
        case PHASE_RAMP:
            spinPhase.rampTime = val;
            break;
        case PHASE_FINAL:
            if (field == 0) spinPhase.finalSpeed = val;
            else            spinPhase.finalTime  = val;
            break;
    }
}

void SpinProfilePage::fieldLabel(int phase, int field, char* buf, int bufLen) {
    if (fieldIsRPM(phase, field)) {
        snprintf(buf, bufLen, "Speed: %u", (unsigned)getFieldValue(phase, field));
    } else {
        snprintf(buf, bufLen, "Time:  %us", (unsigned)getFieldValue(phase, field));
    }
}

// ----------------------------------------------------------------

void SpinProfilePage::start(OLEDLineDisplay& oled, TM1637BlinkerDigit& blinker) {
    _oled    = &oled;
    _blinker = &blinker;
    _listSel = 0;
    _listOff = 0;
    enterPhaseList();
}

bool SpinProfilePage::update(int delta, bool pressed) {

    // ---- Digit edit mode ----
    if (_state == DIGIT_EDIT) {
        if (delta != 0) {
            _digits[_digitPos] = ((_digits[_digitPos] + delta) % 10 + 10) % 10;
            renderDigitEdit();
        }
        if (pressed) {
            _digitPos++;
            if (_digitPos >= _numDigits) {
                saveDigits();
                _blinker->clearBlink();
                _blinker->setNumber(0);
                _listSel = _editField + 1; // return focus to edited field row
                _listOff = 0;
                enterFieldSelect(_editPhase);
            } else {
                renderDigitEdit();
            }
        }
        return false;
    }

    // ---- List scroll (PHASE_LIST / FIELD_SELECT) ----
    if (delta != 0) {
        _listSel = constrain(_listSel + delta, 0, _labelCount - 1);
        int vis = _oled->getVisibleLines();
        if (_listSel < _listOff)          _listOff = _listSel;
        if (_listSel >= _listOff + vis)   _listOff = _listSel - vis + 1;
        _oled->setListSelected(_listSel);
        _oled->setListOffset(_listOff);
        _oled->renderList();
    }

    if (pressed) {
        if (_state == PHASE_LIST) {
            if (_listSel == 0) {
                return true; // "< Done"
            } else if (_listSel >= 1 && _listSel <= PHASE_COUNT) {
                _editPhase = _listSel - 1;
                _listSel   = 0;
                _listOff   = 0;
                enterFieldSelect(_editPhase);
            }
        } else { // FIELD_SELECT
            if (_listSel == 0) {
                // "< Back" — restore cursor to the phase we came from
                _listSel = _editPhase + 1;
                _listOff = 0;
                enterPhaseList();
            } else {
                int fieldIdx = _listSel - 1;
                if (fieldIdx < phaseFieldCount(_editPhase)) {
                    enterDigitEdit(_editPhase, fieldIdx);
                }
            }
        }
    }

    return false;
}

// ----------------------------------------------------------------

void SpinProfilePage::enterPhaseList() {
    _state      = PHASE_LIST;
    _labelCount = 0;

    strncpy(_labels[_labelCount++], "< Done", OLED_MAX_CHARS - 1);

    // Idle
    snprintf(_labels[_labelCount++], OLED_MAX_CHARS,
             "Idle:%u/%us",
             (unsigned)spinPhase.idleSpeed, (unsigned)spinPhase.idleTime);

    // Ramp
    snprintf(_labels[_labelCount++], OLED_MAX_CHARS,
             "Ramp:%us", (unsigned)spinPhase.rampTime);

    // Final
    snprintf(_labels[_labelCount++], OLED_MAX_CHARS,
             "Final:%u/%us",
             (unsigned)spinPhase.finalSpeed, (unsigned)spinPhase.finalTime);

    for (int i = 0; i < _labelCount; i++) {
        _labels[i][OLED_MAX_CHARS - 1] = '\0';
        _labelPtrs[i] = _labels[i];
    }

    _oled->setList(_labelPtrs, _labelCount);
    _oled->setListSelected(_listSel);
    _oled->setListOffset(_listOff);
    _oled->renderList();
}

void SpinProfilePage::enterFieldSelect(int phase) {
    _state      = FIELD_SELECT;
    _editPhase  = phase;
    _labelCount = 0;

    snprintf(_labels[_labelCount++], OLED_MAX_CHARS, "< %s", phaseName(phase));

    int fc = phaseFieldCount(phase);
    for (int f = 0; f < fc; f++) {
        fieldLabel(phase, f, _labels[_labelCount], OLED_MAX_CHARS);
        _labelCount++;
    }

    for (int i = 0; i < _labelCount; i++) {
        _labels[i][OLED_MAX_CHARS - 1] = '\0';
        _labelPtrs[i] = _labels[i];
    }

    _oled->setList(_labelPtrs, _labelCount);
    _oled->setListSelected(_listSel);
    _oled->setListOffset(_listOff);
    _oled->renderList();
}

void SpinProfilePage::enterDigitEdit(int phase, int field) {
    _state      = DIGIT_EDIT;
    _editPhase  = phase;
    _editField  = field;
    _digitPos   = 0;
    _fieldIsRPM = fieldIsRPM(phase, field);

    uint16_t v = getFieldValue(phase, field);

    if (_fieldIsRPM) {
        _numDigits  = 4;
        _digits[0]  = (v / 1000) % 10;
        _digits[1]  = (v /  100) % 10;
        _digits[2]  = (v /   10) % 10;
        _digits[3]  =  v         % 10;
    } else {
        _numDigits  = 3;
        _digits[0]  = (v / 100) % 10;
        _digits[1]  = (v /  10) % 10;
        _digits[2]  =  v        % 10;
    }

    _oled->clear();
    renderDigitEdit();
}

void SpinProfilePage::saveDigits() {
    uint16_t val;
    if (_fieldIsRPM) {
        val = _digits[0]*1000 + _digits[1]*100 + _digits[2]*10 + _digits[3];
    } else {
        val = _digits[0]*100 + _digits[1]*10 + _digits[2];
    }
    setFieldValue(_editPhase, _editField, val);
}

void SpinProfilePage::renderDigitEdit() {
    // Header: "Idle Speed", "Idle Time", "Ramp Time", "Final Speed", "Final Time"
    const char* phaseStr = phaseName(_editPhase);
    const char* kindStr  = _fieldIsRPM ? "Speed" : "Time";

    char headerLine[OLED_MAX_CHARS];
    snprintf(headerLine, sizeof(headerLine), "%s %s", phaseStr, kindStr);
    _oled->setText(0, headerLine);

    char valLine[11];
    char arrowLine[11];
    int  val;
    int  blinkPos;

    if (_fieldIsRPM) {
        snprintf(valLine, 11, "RPM:%d%d%d%d",
                 _digits[0], _digits[1], _digits[2], _digits[3]);
        val      = _digits[0]*1000 + _digits[1]*100 + _digits[2]*10 + _digits[3];
        blinkPos = _digitPos;
    } else {
        snprintf(valLine, 11, "Sec:%d%d%d ",
                 _digits[0], _digits[1], _digits[2]);
        val      = _digits[0]*100 + _digits[1]*10 + _digits[2];
        blinkPos = _digitPos + 1; // leading zero at TM1637 position 0
    }

    memset(arrowLine, ' ', 10);
    arrowLine[4 + _digitPos] = '^';
    arrowLine[10] = '\0';

    _oled->setText(1, valLine);
    _oled->setText(2, arrowLine);
    _oled->setText(3, (_digitPos == _numDigits - 1) ? "Btn:save" : "Btn:next");
    _oled->render();

    _blinker->setNumber(val);
    _blinker->clearBlink();
    _blinker->startBlink(blinkPos);
}
