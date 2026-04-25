#include "OLEDLineDisplay.h"

OLEDLineDisplay::OLEDLineDisplay(Adafruit_SSD1306 &d, int numLines)
    : disp(d)
{
    lines = numLines;
    if (lines > OLED_MAX_LINES) lines = OLED_MAX_LINES;

    lineHeight = disp.height() / lines;

    // Largest text size whose glyph height (8px * scale) fits within one line slot
    textSize = 1;
    while ((textSize + 1) * 8 <= lineHeight) textSize++;

    for (int i = 0; i < lines; i++) {
        state[i].text[0]   = '\0';
        state[i].underline = false;
        state[i].invert    = false;
        state[i].align     = LEFT;
        state[i].dirty     = true;
    }

    _listCount    = 0;
    _listOffset   = 0;
    _listSelected = 0;
    _listDirty    = false;
}

void OLEDLineDisplay::begin() {
    disp.clearDisplay();
    disp.setTextColor(SSD1306_WHITE);
    disp.setTextSize(textSize);
}

int OLEDLineDisplay::computeTextWidth(const char* text) {
    return strlen(text) * 6 * textSize;
}

void OLEDLineDisplay::setText(int line, const char* fmt, ...) {
    if (line < 0 || line >= lines) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, OLED_MAX_CHARS, fmt, args);
    va_end(args);

    int maxChars = disp.width() / (6 * textSize);
    if (maxChars >= OLED_MAX_CHARS) maxChars = OLED_MAX_CHARS - 1;
    buffer[maxChars] = '\0';

    if (strcmp(state[line].text, buffer) != 0) {
        strncpy(state[line].text, buffer, OLED_MAX_CHARS - 1);
        state[line].text[OLED_MAX_CHARS - 1] = '\0';
        state[line].dirty = true;
    }
}

void OLEDLineDisplay::render() {
    bool anyDirty = false;

    for (int i = 0; i < lines; i++) {
        if (!state[i].dirty) continue;
        anyDirty = true;

        int y     = i * lineHeight;
        int width = strlen(state[i].text) * 6 * textSize;

        disp.fillRect(0, y, disp.width(), lineHeight,
                      state[i].invert ? SSD1306_WHITE : SSD1306_BLACK);

        int x = 0;
        if      (state[i].align == CENTER) x = (disp.width() - width) / 2;
        else if (state[i].align == RIGHT)  x = disp.width() - width;

        disp.setCursor(x, y);
        disp.setTextColor(state[i].invert ? SSD1306_BLACK : SSD1306_WHITE);
        disp.setTextSize(textSize);
        disp.print(state[i].text);

        if (state[i].underline) {
            disp.drawLine(x, y + lineHeight - 2, x + width, y + lineHeight - 2,
                          state[i].invert ? SSD1306_BLACK : SSD1306_WHITE);
        }

        state[i].dirty = false;
    }

    if (anyDirty) disp.display();
}

void OLEDLineDisplay::clear() {
    disp.clearDisplay();

    for (int i = 0; i < lines; i++) {
        state[i].text[0]   = '\0';
        state[i].underline = false;
        state[i].invert    = false;
        state[i].align     = LEFT;
        state[i].dirty     = true;
    }
}

int OLEDLineDisplay::getVisibleLines() const { return lines; }

void OLEDLineDisplay::setList(const char** items, int count) {
    _listCount = (count > OLED_LIST_MAX_ITEMS) ? OLED_LIST_MAX_ITEMS : count;
    for (int i = 0; i < _listCount; i++) {
        strncpy(_listItems[i], items[i], OLED_MAX_CHARS - 1);
        _listItems[i][OLED_MAX_CHARS - 1] = '\0';
    }
    _listDirty = true;
}

void OLEDLineDisplay::setListSelected(int index) {
    if (index < 0) index = 0;
    if (_listCount > 0 && index >= _listCount) index = _listCount - 1;
    if (_listSelected != index) {
        _listSelected = index;
        _listDirty    = true;
    }
}

void OLEDLineDisplay::setListOffset(int offset) {
    if (offset < 0) offset = 0;
    int maxOffset = _listCount - lines;
    if (maxOffset < 0) maxOffset = 0;
    if (offset > maxOffset) offset = maxOffset;
    if (_listOffset != offset) {
        _listOffset = offset;
        _listDirty  = true;
    }
}

int OLEDLineDisplay::getListOffset() const { return _listOffset; }

void OLEDLineDisplay::renderList() {
    if (!_listDirty) return;

    bool hasScrollbar = (_listCount > lines);
    int  contentWidth = hasScrollbar ? disp.width() - 3 : disp.width();

    disp.clearDisplay();
    disp.setTextSize(textSize);

    for (int i = 0; i < lines; i++) {
        int  itemIdx  = _listOffset + i;
        if (itemIdx >= _listCount) break;

        int  y        = i * lineHeight;
        bool selected = (itemIdx == _listSelected);

        disp.fillRect(0, y, contentWidth, lineHeight,
                      selected ? SSD1306_WHITE : SSD1306_BLACK);
        disp.setCursor(1, y);
        disp.setTextColor(selected ? SSD1306_BLACK : SSD1306_WHITE);
        disp.print(_listItems[itemIdx]);
    }

    if (hasScrollbar) {
        int barH = max(3, lines * disp.height() / _listCount);
        int barY = (_listCount > lines)
            ? _listOffset * (disp.height() - barH) / (_listCount - lines)
            : 0;
        disp.fillRect(disp.width() - 2, barY, 2, barH, SSD1306_WHITE);
    }

    disp.display();
    _listDirty = false;
}
