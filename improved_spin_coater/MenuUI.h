#pragma once
#include <Arduino.h>
#include "OLEDLineDisplay.h"

typedef void (*MenuCallback)();

struct MenuItem {
    const char*       label;
    MenuCallback      action;       // non-null for action items
    const MenuItem*   submenu;      // non-null for submenu items
    int               submenuCount;
};

// Convenience macros for defining menu items
#define MENU_ACTION(lbl, fn)   { lbl, fn, nullptr, 0 }
#define MENU_SUBMENU(lbl, sub) { lbl, nullptr, sub, (int)(sizeof(sub)/sizeof((sub)[0])) }

class MenuUI {
public:
    // Attach to display and load root menu
    void begin(OLEDLineDisplay& oled, const MenuItem* items, int count);

    // Call each loop with encoder delta and a rising-edge press signal
    void update(int delta, bool pressed);

private:
    static const int MAX_DEPTH = 5;

    struct Level {
        const MenuItem* items;
        int count;
        int selected; // index in visible list (0 == "< Back" when depth > 0)
        int offset;   // scroll offset in visible list
    };

    Level            _stack[MAX_DEPTH];
    int              _depth;
    OLEDLineDisplay* _oled;

    void push(const MenuItem* items, int count);
    void pop();
    void rebuildList();
    int  visibleCount() const;
};
