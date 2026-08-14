#include "custom_level_menu.h"

#include <keypadc.h>
#include "classic.h"
#include "custom_levels.h"
#include "emu.h"

namespace custom_level_menu {
namespace {
bool menu_open = false;
uint8_t selected = 0;
bool old_mode = false, old_up = false, old_down = false, old_play = false;

bool edge(bool now, bool &old) { const bool result = now && !old; old = now; return result; }
}

void initialize() {
    menu_open = false; selected = 0;
    old_mode = old_up = old_down = old_play = false;
}

bool open() { return menu_open; }

void show() {
    custom_levels::scan();
    selected = 0;
    menu_open = true;
    old_mode = kb_IsDown(kb_KeyMode);
    old_up = kb_IsDown(kb_KeyUp);
    old_down = kb_IsDown(kb_KeyDown);
    old_play = kb_IsDown(kb_Key2nd) || kb_IsDown(kb_KeyAlpha);
}

bool update() {
    const bool mode = kb_IsDown(kb_KeyMode);
    const bool up = kb_IsDown(kb_KeyUp);
    const bool down = kb_IsDown(kb_KeyDown);
    const bool play = kb_IsDown(kb_Key2nd) || kb_IsDown(kb_KeyAlpha);
    const bool mode_pressed = edge(mode, old_mode);
    const bool up_pressed = edge(up, old_up);
    const bool down_pressed = edge(down, old_down);
    const bool play_pressed = edge(play, old_play);

    if (!is_title()) {
        menu_open = false;
        return false;
    }
    if (!menu_open) {
        if (!mode_pressed) return false;
        show(); return true;
    }
    if (mode_pressed) { menu_open = false; return true; }
    const uint8_t count = custom_levels::catalog_size();
    if (count) {
        if (up_pressed) selected = selected == 0 ? count - 1 : selected - 1;
        if (down_pressed) selected = static_cast<uint8_t>((selected + 1) % count);
        if (play_pressed && custom_levels::load(selected)) {
            menu_open = false; begin_game(); return true;
        }
    }
    return true;
}

void draw() {
    if (!menu_open || !is_title()) return;
    rectfill(7, 7, 121, 121, 0);
    print("CUSTOM LEVELS", 32, 12, 11);
    const uint8_t count = custom_levels::catalog_size();
    if (!count) {
        const char *error = custom_levels::last_error();
        if (error && error[0]) {
            print("CELV FOUND, INVALID", 17, 43, 8);
            print(error, 12, 58, 7);
        } else {
            print("NO CELV APPVARS", 27, 51, 7);
        }
        print("MODE: BACK", 38, 103, 6);
        return;
    }
    const uint8_t first = selected >= 6 ? selected - 5 : 0;
    for (uint8_t row = 0; row < 6 && first + row < count; ++row) {
        const uint8_t index = first + row;
        const auto *entry = custom_levels::catalog_entry(index);
        if (index == selected) rectfill(12, 26 + row * 12, 116, 35 + row * 12, 1);
        print(entry->title, 15, 28 + row * 12, index == selected ? 7 : 6);
        if (entry->kind == clevel::KIND_PACK) print("P", 109, 28 + row * 12, 10);
    }
    print("2ND: PLAY  MODE: BACK", 12, 106, 6);
}

} // namespace custom_level_menu
