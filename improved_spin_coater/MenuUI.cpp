#include "MenuUI.h"

void MenuUI::begin(OLEDLineDisplay& oled, const MenuItem* items, int count) {
    _oled  = &oled;
    _depth = 0;
    _stack[0] = { items, count, 0, 0 };
    rebuildList();
}

int MenuUI::visibleCount() const {
    return _stack[_depth].count + (_depth > 0 ? 1 : 0);
}

void MenuUI::update(int delta, bool pressed) {
    Level& lvl = _stack[_depth];
    int total  = visibleCount();

    if (delta != 0) {
        lvl.selected = constrain(lvl.selected + delta, 0, total - 1);

        int vis = _oled->getVisibleLines();
        if (lvl.selected < lvl.offset)        lvl.offset = lvl.selected;
        if (lvl.selected >= lvl.offset + vis) lvl.offset = lvl.selected - vis + 1;

        _oled->setListSelected(lvl.selected);
        _oled->setListOffset(lvl.offset);
        _oled->renderList();
    }

    if (pressed) {
        bool hasBack = (_depth > 0);
        if (hasBack && lvl.selected == 0) {
            pop();
            return;
        }
        int idx = lvl.selected - (hasBack ? 1 : 0);
        const MenuItem& item = lvl.items[idx];
        if (item.action) {
            item.action();
        } else if (item.submenu) {
            push(item.submenu, item.submenuCount);
        }
    }
}

void MenuUI::push(const MenuItem* items, int count) {
    if (_depth + 1 >= MAX_DEPTH) return;
    _depth++;
    _stack[_depth] = { items, count, 0, 0 };
    rebuildList();
}

void MenuUI::pop() {
    if (_depth == 0) return;
    _depth--;
    rebuildList();
}

void MenuUI::redraw() {
    rebuildList();
}

void MenuUI::rebuildList() {
    Level& lvl = _stack[_depth];
    const char* labels[OLED_LIST_MAX_ITEMS];
    int idx = 0;

    if (_depth > 0) labels[idx++] = "< Back";
    for (int i = 0; i < lvl.count && idx < OLED_LIST_MAX_ITEMS; i++) {
        labels[idx++] = lvl.items[i].label;
    }

    _oled->setList(labels, idx);
    _oled->setListSelected(lvl.selected);
    _oled->setListOffset(lvl.offset);
    _oled->renderList();
}
