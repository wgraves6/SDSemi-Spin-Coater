#include "SpinProfilePage.h"


void SpinProfilePage::start(OLEDLineDisplay& oled, TM1637BlinkerDigit& blinker) {
    _oled    = &oled;
    _blinker = &blinker;
    _listSel = 0;
    _listOff = 0;
    enterStepList();
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
                _listSel = (_editField == 0) ? 1 : 2; // return focus to edited field
                _listOff = 0;
                enterFieldSelect();
            } else {
                renderDigitEdit();
            }
        }
        return false;
    }

    // ---- List modes (STEP_LIST / FIELD_SELECT) ----
    if (delta != 0) {
        _listSel = constrain(_listSel + delta, 0, _labelCount - 1);
        int vis = _oled->getVisibleLines();
        if (_listSel < _listOff)            _listOff = _listSel;
        if (_listSel >= _listOff + vis)     _listOff = _listSel - vis + 1;
        _oled->setListSelected(_listSel);
        _oled->setListOffset(_listOff);
        _oled->renderList();
    }

    if (pressed) {
        if (_state == STEP_LIST) {
            if (_listSel == 0) {
                return true; // "< Done" — exit page
            } else if (_listSel <= spinProfileCount) {
                _editStep = _listSel - 1;
                _listSel  = 0;
                _listOff  = 0;
                enterFieldSelect();
            } else if (_listSel == spinProfileCount + 1) {
                // "+ Add Step"
                if (spinProfileCount < SPIN_PROFILE_MAX_STEPS) {
                    spinProfile[spinProfileCount] = { 1000, 10 };
                    spinProfileCount++;
                    _listSel = spinProfileCount; // point at new step
                }
                enterStepList();
            } else {
                // "- Rem Last"
                if (spinProfileCount > 1) {
                    spinProfileCount--;
                    if (_listSel > spinProfileCount) _listSel = spinProfileCount;
                }
                enterStepList();
            }
        } else { // FIELD_SELECT
            if (_listSel == 0) {
                // "< Back" — restore cursor to the step we came from
                _listSel = _editStep + 1;
                _listOff = 0;
                enterStepList();
            } else if (_listSel == 1) {
                enterDigitEdit(0); // edit RPM
            } else if (_listSel == 2) {
                enterDigitEdit(1); // edit duration
            }
        }
    }

    return false;
}


void SpinProfilePage::enterStepList() {
    _state      = STEP_LIST;
    _labelCount = 0;

    strncpy(_labels[_labelCount++], "< Done", OLED_MAX_CHARS - 1);
    for (int i = 0; i < spinProfileCount; i++) {
        snprintf(_labels[_labelCount++], OLED_MAX_CHARS,
                 "%d:%u/%us", i + 1,
                 (unsigned int)spinProfile[i].rpm,
                 (unsigned int)spinProfile[i].durationS);
    }
    strncpy(_labels[_labelCount++], "+ Add Step", OLED_MAX_CHARS - 1);
    strncpy(_labels[_labelCount++], "- Rem Last", OLED_MAX_CHARS - 1);

    for (int i = 0; i < _labelCount; i++) {
        _labels[i][OLED_MAX_CHARS - 1] = '\0';
        _labelPtrs[i] = _labels[i];
    }

    _oled->setList(_labelPtrs, _labelCount);
    _oled->setListSelected(_listSel);
    _oled->setListOffset(_listOff);
    _oled->renderList();
}

void SpinProfilePage::enterFieldSelect() {
    _state      = FIELD_SELECT;
    _labelCount = 0;

    strncpy(_labels[_labelCount++], "< Back", OLED_MAX_CHARS - 1);
    snprintf(_labels[_labelCount++], OLED_MAX_CHARS, "RPM: %u",
             (unsigned int)spinProfile[_editStep].rpm);
    snprintf(_labels[_labelCount++], OLED_MAX_CHARS, "Dur: %us",
             (unsigned int)spinProfile[_editStep].durationS);

    for (int i = 0; i < _labelCount; i++) {
        _labels[i][OLED_MAX_CHARS - 1] = '\0';
        _labelPtrs[i] = _labels[i];
    }

    _oled->setList(_labelPtrs, _labelCount);
    _oled->setListSelected(_listSel);
    _oled->setListOffset(_listOff);
    _oled->renderList();
}

void SpinProfilePage::enterDigitEdit(int field) {
    _state     = DIGIT_EDIT;
    _editField = field;
    _digitPos  = 0;

    if (field == 0) { // RPM — 4 digits
        _numDigits = 4;
        uint16_t v = spinProfile[_editStep].rpm;
        _digits[0] = (v / 1000) % 10;
        _digits[1] = (v /  100) % 10;
        _digits[2] = (v /   10) % 10;
        _digits[3] =  v         % 10;
    } else { // Duration — 3 digits
        _numDigits = 3;
        uint16_t v = spinProfile[_editStep].durationS;
        _digits[0] = (v / 100) % 10;
        _digits[1] = (v /  10) % 10;
        _digits[2] =  v        % 10;
    }

    _oled->clear();
    renderDigitEdit();
}

void SpinProfilePage::saveDigits() {
    if (_editField == 0) {
        spinProfile[_editStep].rpm =
            _digits[0] * 1000 + _digits[1] * 100 + _digits[2] * 10 + _digits[3];
    } else {
        spinProfile[_editStep].durationS =
            _digits[0] * 100 + _digits[1] * 10 + _digits[2];
    }
}

void SpinProfilePage::renderDigitEdit() {
    char valLine[11];
    char arrowLine[11];
    int val, blinkPos;

    if (_editField == 0) {
        _oled->setText(0, "Edit RPM");
        snprintf(valLine, 11, "RPM:%d%d%d%d",
                 _digits[0], _digits[1], _digits[2], _digits[3]);
        val      = _digits[0]*1000 + _digits[1]*100 + _digits[2]*10 + _digits[3];
        blinkPos = _digitPos;
    } else {
        _oled->setText(0, "Edit Dur");
        snprintf(valLine, 11, "Dur:%d%d%ds",
                 _digits[0], _digits[1], _digits[2]);
        val      = _digits[0]*100 + _digits[1]*10 + _digits[2];
        blinkPos = _digitPos + 1; // leading zero occupies position 0 on 4-digit display
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
