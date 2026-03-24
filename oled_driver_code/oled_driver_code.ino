#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

enum TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

// ---- OLED Line Driver with Auto TextSize ----
class OLEDLineDisplay {
private:
    Adafruit_SSD1306 &disp;
    int lines;
    int lineHeight;
    int textSize; // calculated automatically
    String lastText[20];
    bool lastUnderline[20];
    bool lastInvert[20];
    TextAlign lastAlign[20];

public:
    OLEDLineDisplay(Adafruit_SSD1306 &d, int numLines) : disp(d) {
        lines = numLines;
        lineHeight = SCREEN_HEIGHT / lines;

        // Calculate text size to fit line height (approx)
        textSize = 1;
        while ((textSize + 1) * 8 <= lineHeight) { // each textSize unit ~8 pixels high for normal font
            textSize++;
        }

        for (int i = 0; i < lines; i++) {
            lastText[i] = "";
            lastUnderline[i] = false;
            lastInvert[i] = false;
            lastAlign[i] = LEFT;
        }
    }

    void begin() {
        disp.clearDisplay();
        disp.setTextColor(SSD1306_WHITE);
        disp.setTextSize(textSize);
    }

    void drawLine(int lineNum, const char* text, bool underline = false, bool invert = false, TextAlign align = LEFT) {
        if (lineNum < 0 || lineNum >= lines) return;

        int y = lineNum * lineHeight;

        int prevWidth = lastText[lineNum].length() * 6 * textSize;
        int newWidth = strlen(text) * 6 * textSize;

        if (lastText[lineNum] != text || lastUnderline[lineNum] != underline || lastInvert[lineNum] != invert || lastAlign[lineNum] != align || newWidth < prevWidth) {
            // Clear full line area
            disp.fillRect(0, y, SCREEN_WIDTH, lineHeight, invert ? SSD1306_WHITE : SSD1306_BLACK);

            // Compute X based on alignment
            int x = 0;
            if (align == CENTER) {
                x = (SCREEN_WIDTH - newWidth) / 2;
            } else if (align == RIGHT) {
                x = SCREEN_WIDTH - newWidth;
            }

            disp.setCursor(x, y);
            disp.setTextColor(invert ? SSD1306_BLACK : SSD1306_WHITE);
            disp.setTextSize(textSize);
            disp.print(text);

            if (underline) {
                disp.drawLine(x, y + lineHeight - 2, x + newWidth, y + lineHeight - 2, invert ? SSD1306_BLACK : SSD1306_WHITE);
            }

            lastText[lineNum] = String(text);
            lastUnderline[lineNum] = underline;
            lastInvert[lineNum] = invert;
            lastAlign[lineNum] = align;
        }
    }

    void update() { disp.display(); }

    void clear() {
        disp.clearDisplay();
        for (int i = 0; i < lines; i++) {
            lastText[i] = "";
            lastUnderline[i] = false;
            lastInvert[i] = false;
            lastAlign[i] = LEFT;
        }
    }

    int getLineHeight() { return lineHeight; }
    int getTextSize() { return textSize; }
};

// ---- Global OLEDLineDisplay object ----
OLEDLineDisplay oled(display, 4); // 4 lines for example

void setup() {
    Serial.begin(9600);
    delay(1000);

    Wire1.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED failed");
        while (1);
    }

    oled.begin();
}

void loop() {
    static int counter = 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "Count: %d", counter++);

    oled.drawLine(0, buf, false, false, LEFT);        // left aligned
    oled.drawLine(1, "Button A", true, true, CENTER); // centered, underlined + inverted
    oled.drawLine(2, "Button B", true, false, RIGHT); // right aligned + underlined
    oled.drawLine(3, "Hello OLED", false, true, CENTER); // centered + inverted

    oled.update();
    delay(500);
}