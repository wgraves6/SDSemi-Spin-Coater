#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdarg.h>

#define OLED_MAX_LINES      20
#define OLED_MAX_CHARS      32
#define OLED_LIST_MAX_ITEMS 20

enum TextAlign { LEFT, CENTER, RIGHT };

struct LineState {
    char      text[OLED_MAX_CHARS];
    bool      underline;
    bool      invert;
    TextAlign align;
    bool      dirty;
};

class OLEDLineDisplay {
private:
    Adafruit_SSD1306 &disp;

    int lines;
    int lineHeight;
    int textSize;

    char      buffer[OLED_MAX_CHARS];
    LineState state[OLED_MAX_LINES];

    char        _listItems[OLED_LIST_MAX_ITEMS][OLED_MAX_CHARS];
    int         _listCount;
    int         _listOffset;
    int         _listSelected;
    bool        _listDirty;

    int computeTextWidth(const char* text);

public:
    OLEDLineDisplay(Adafruit_SSD1306 &d, int numLines);

    void begin();
    void setText(int line, const char* fmt, ...);
    void render();
    void clear();
    int  getVisibleLines() const;

    void setList(const char** items, int count);
    void setListSelected(int index);
    void setListOffset(int offset);
    int  getListOffset() const;
    void renderList();
};
