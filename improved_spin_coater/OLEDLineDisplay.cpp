#include "OLEDLineDisplay.h"

// Constructor: sets up line layout and initial state
OLEDLineDisplay::OLEDLineDisplay(Adafruit_SSD1306 &d, int numLines)
    : disp(d)
{
    // Limit number of lines to max allowed
    lines = numLines;
    if (lines > OLED_MAX_LINES) {
        lines = OLED_MAX_LINES;
    }

    // Compute height of each line based on display height
    lineHeight = disp.height() / lines;

    // Automatically choose largest text size that fits in a line
    textSize = 1;
    while ((textSize + 1) * 8 <= lineHeight) {
        textSize++;
    }

    // Initialize all line states
    for (int i = 0; i < lines; i++) {
        state[i].text = "";
        state[i].underline = false;
        state[i].invert = false;
        state[i].align = LEFT;
        state[i].dirty = true; // force initial draw
    }

    _listCount = 0;
    _listOffset = 0;
    _listSelected = 0;
    _listDirty = false;
}

// Initialize display settings (call after display.begin())
void OLEDLineDisplay::begin() {
    disp.clearDisplay();
    disp.setTextColor(SSD1306_WHITE);
    disp.setTextSize(textSize);
}

// Estimate pixel width of text (fixed-width font assumption)
int OLEDLineDisplay::computeTextWidth(const char* text) {
    return strlen(text) * 6 * textSize;
}

// Set text for a specific line using printf-style formatting
void OLEDLineDisplay::setText(int line, const char* fmt, ...) {
    // Ignore invalid line index
    if (line < 0 || line >= lines) {
        return;
    }

    // Format text into buffer
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, OLED_MAX_CHARS, fmt, args);
    va_end(args);

    // Limit text length so it fits on screen
    int maxChars = disp.width() / (6 * textSize);
    if (maxChars >= OLED_MAX_CHARS) {
        maxChars = OLED_MAX_CHARS - 1; // prevent overflow
    }
    buffer[maxChars] = '\0';

    // Only update if text actually changed (reduces redraws)
    if (state[line].text != buffer) {
        state[line].text = String(buffer);
        state[line].dirty = true;
    }
}

// Enable/disable underline for a line
void OLEDLineDisplay::setUnderline(int line, bool enable) {
    if (line < 0 || line >= lines) {
        return;
    }

    // Mark dirty only if state changes
    if (state[line].underline != enable) {
        state[line].underline = enable;
        state[line].dirty = true;
    }
}

// Enable/disable inverted colors for a line
void OLEDLineDisplay::setInvert(int line, bool enable) {
    if (line < 0 || line >= lines) {
        return;
    }

    if (state[line].invert != enable) {
        state[line].invert = enable;
        state[line].dirty = true;
    }
}

// Set alignment for a line
void OLEDLineDisplay::setAlign(int line, TextAlign align) {
    if (line < 0 || line >= lines) {
        return;
    }

    if (state[line].align != align) {
        state[line].align = align;
        state[line].dirty = true;
    }
}

// Render only lines that have changed
void OLEDLineDisplay::render() {
    for (int i = 0; i < lines; i++) {
        // Skip lines that haven't changed
        if (!state[i].dirty) {
            continue;
        }

        int y = i * lineHeight;

        // Calculate pixel width of text for alignment
        int width = state[i].text.length() * 6 * textSize;

        // Clear the line area (handle inverted background)
        disp.fillRect(
            0,
            y,
            disp.width(),
            lineHeight,
            state[i].invert ? SSD1306_WHITE : SSD1306_BLACK
        );

        // Determine X position based on alignment
        int x = 0;
        if (state[i].align == CENTER) {
            x = (disp.width() - width) / 2;
        } else if (state[i].align == RIGHT) {
            x = disp.width() - width;
        }

        // Draw text
        disp.setCursor(x, y);
        disp.setTextColor(
            state[i].invert ? SSD1306_BLACK : SSD1306_WHITE
        );
        disp.setTextSize(textSize);
        disp.print(state[i].text);

        // Draw underline if enabled
        if (state[i].underline) {
            disp.drawLine(
                x,
                y + lineHeight - 2,
                x + width,
                y + lineHeight - 2,
                state[i].invert ? SSD1306_BLACK : SSD1306_WHITE
            );
        }

        // Mark line as clean (no longer needs redraw)
        state[i].dirty = false;
    }

    // Push buffer to the physical display
    disp.display();
}

// Clear entire display and reset all line states
void OLEDLineDisplay::clear() {
    disp.clearDisplay();

    for (int i = 0; i < lines; i++) {
        state[i].text = "";
        state[i].underline = false;
        state[i].invert = false;
        state[i].align = LEFT;
        state[i].dirty = true; // force redraw
    }
}

// Return height of each line in pixels
int OLEDLineDisplay::getLineHeight() {
    return lineHeight;
}

// Return current text size scaling factor
int OLEDLineDisplay::getTextSize() {
    return textSize;
}

// Return number of visible lines
int OLEDLineDisplay::getVisibleLines() const {
    return lines;
}

// Copy items into internal storage
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
        _listDirty = true;
    }
}

void OLEDLineDisplay::setListOffset(int offset) {
    if (offset < 0) offset = 0;
    int maxOffset = _listCount - lines;
    if (maxOffset < 0) maxOffset = 0;
    if (offset > maxOffset) offset = maxOffset;
    if (_listOffset != offset) {
        _listOffset = offset;
        _listDirty = true;
    }
}

int OLEDLineDisplay::getListCount()    const { return _listCount; }
int OLEDLineDisplay::getListSelected() const { return _listSelected; }
int OLEDLineDisplay::getListOffset()   const { return _listOffset; }

void OLEDLineDisplay::renderList() {
    if (!_listDirty) return;

    bool hasScrollbar = (_listCount > lines);
    int contentWidth = hasScrollbar ? disp.width() - 3 : disp.width();

    disp.clearDisplay();
    disp.setTextSize(textSize);

    for (int i = 0; i < lines; i++) {
        int itemIdx = _listOffset + i;
        if (itemIdx >= _listCount) break;

        int y = i * lineHeight;
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
        int barX = disp.width() - 2;
        disp.fillRect(barX, barY, 2, barH, SSD1306_WHITE);
    }

    disp.display();
    _listDirty = false;
}