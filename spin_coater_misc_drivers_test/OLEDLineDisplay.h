#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdarg.h>

// Maximum number of lines the display can handle (software limit)
#define OLED_MAX_LINES 20

// Maximum number of characters per line (includes null terminator)
#define OLED_MAX_CHARS 32

// Text alignment options
enum TextAlign {
    LEFT,   // left aligned text
    CENTER, // centered text
    RIGHT   // right aligned text
};

// Stores all rendering properties for a single line
struct LineState {
    String text;      // text content
    bool underline;   // draw underline under text
    bool invert;      // invert colors (white background, black text)
    TextAlign align;  // alignment setting
    bool dirty;       // true if line needs to be redrawn
};

// Main display driver class for line-based text rendering
class OLEDLineDisplay {
private:
    Adafruit_SSD1306 &disp; // reference to the actual display object

    int lines;       // number of lines used
    int lineHeight;  // height of each line in pixels
    int textSize;    // text scaling factor

    char buffer[OLED_MAX_CHARS]; // temporary buffer for formatted text
    LineState state[OLED_MAX_LINES]; // state for each line

    // Calculate pixel width of a string based on current text size
    int computeTextWidth(const char* text);

public:
    // Constructor: pass in display object and number of lines to use
    OLEDLineDisplay(Adafruit_SSD1306 &d, int numLines);

    // Initialize display settings (call after display.begin())
    void begin();

    // Set text for a line using printf-style formatting
    void setText(int line, const char* fmt, ...);

    // Enable or disable underline for a line
    void setUnderline(int line, bool enable);

    // Enable or disable inverted colors for a line
    void setInvert(int line, bool enable);

    // Set alignment (LEFT, CENTER, RIGHT) for a line
    void setAlign(int line, TextAlign align);

    // Draw all lines that have changed (dirty lines only)
    void render();

    // Clear the display and reset all line states
    void clear();

    // Get height of each line in pixels
    int getLineHeight();

    // Get current text size multiplier
    int getTextSize();
};