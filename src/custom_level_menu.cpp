#include "custom_level_menu.h"

#include <cstring>
#include <keypadc.h>
#include "classic.h"
#include "custom_levels.h"
#include "emu.h"

namespace custom_level_menu {
namespace {
enum class Screen : uint8_t { Closed, Browser, Results };

Screen screen = Screen::Closed;
uint8_t selected = 0;
uint8_t result_selected = 0;
int result_deaths = 0;
int result_minutes = 0;
int result_seconds = 0;
char result_title[22] = "CUSTOM LEVEL";
char result_author[18] = "";
bool result_is_pack = false;
bool old_mode = false, old_up = false, old_down = false, old_play = false;

bool edge(bool now, bool &old) { const bool result = now && !old; old = now; return result; }

int centered_x(const char *value) {
    const int width = static_cast<int>(std::strlen(value)) * 5;
    return width < 128 ? (128 - width) / 2 : 2;
}

void copy_short(char *destination, std::size_t capacity, const char *source) {
    if(!capacity) return;
    std::strncpy(destination, source ? source : "", capacity - 1);
    destination[capacity - 1] = '\0';
}
}

void initialize() {
    close();
    old_mode = old_up = old_down = old_play = false;
}

bool open() { return screen != Screen::Closed; }
bool results_open() { return screen == Screen::Results; }

void close() {
    screen = Screen::Closed;
    selected = result_selected = 0;
}

void show_browser() {
    custom_levels::scan();
    selected = 0;
    screen = Screen::Browser;
}

void show_results(int run_deaths, int run_minutes, int run_seconds) {
    result_deaths = run_deaths;
    result_minutes = run_minutes;
    result_seconds = run_seconds;
    result_selected = 0;
    result_is_pack = false;
    copy_short(result_title, sizeof result_title, "CUSTOM LEVEL");
    result_author[0] = '\0';
    if(const auto *entry = custom_levels::active_entry()) {
        copy_short(result_title, sizeof result_title, entry->title);
        copy_short(result_author, sizeof result_author, entry->author);
        result_is_pack = entry->kind == clevel::KIND_PACK;
    }
    screen = Screen::Results;
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

    if(screen == Screen::Results) {
        constexpr uint8_t RESULT_OPTIONS = 3;
        if(up_pressed) result_selected = result_selected == 0 ? RESULT_OPTIONS - 1 : result_selected - 1;
        if(down_pressed) result_selected = static_cast<uint8_t>((result_selected + 1) % RESULT_OPTIONS);
        if(mode_pressed) {
            title_screen();
            show_browser();
            return true;
        }
        if(play_pressed) {
            if(result_selected == 0) {
                if(custom_levels::restart_entry()) {
                    screen = Screen::Closed;
                    begin_game();
                } else {
                    title_screen();
                    show_browser();
                }
            } else if(result_selected == 1) {
                title_screen();
                show_browser();
            } else {
                title_screen();
            }
        }
        return true;
    }

    if (!is_title()) {
        screen = Screen::Closed;
        return false;
    }
    if (screen == Screen::Closed) {
        if (!mode_pressed) return false;
        show_browser(); return true;
    }
    if (mode_pressed) { screen = Screen::Closed; return true; }
    const uint8_t count = custom_levels::catalog_size();
    if (count) {
        if (up_pressed) selected = selected == 0 ? count - 1 : selected - 1;
        if (down_pressed) selected = static_cast<uint8_t>((selected + 1) % count);
        if (play_pressed && custom_levels::load(selected)) {
            screen = Screen::Closed; begin_game(); return true;
        }
    }
    return true;
}

void draw() {
    if(screen == Screen::Results) {
        rectfill(0, 0, 127, 127, 0);

        // A compact, stepped summit silhouette keeps the completion screen in
        // the visual language of Celeste Classic without requiring new assets.
        for(int step = 0; step < 7; ++step) {
            const uint8_t color = step < 2 ? 6 : (step < 4 ? 5 : 1);
            rectfill(60 - step * 9, 38 + step * 10, 68 + step * 9, 47 + step * 10, color);
        }
        rectfill(8, 8, 9, 9, 7); rectfill(116, 17, 117, 18, 7);
        rectfill(20, 29, 21, 30, 6); rectfill(104, 35, 105, 36, 6);

        rectfill(13, 10, 114, 79, 0);
        print(result_is_pack ? "PACK COMPLETE" : "LEVEL COMPLETE", result_is_pack ? 32 : 29, 15, 10);
        print(result_title, centered_x(result_title), 28, 7);
        if(result_author[0]) {
            print("by", 22, 38, 6);
            print(result_author, 34, 38, 6);
        }
        print("time", 30, 52, 6);
        print_int(result_minutes, 58, 52, 7, 2);
        print(":");
        print_int(result_seconds, 2);
        print("deaths", 30, 63, 6);
        print_int(result_deaths, 68, 63, 7);

        const char *options[] = {"REPLAY", "LEVEL SELECT", "TITLE SCREEN"};
        for(uint8_t i = 0; i < 3; ++i) {
            const int y = 87 + i * 11;
            if(i == result_selected) rectfill(25, y - 2, 103, y + 7, 1);
            print(options[i], centered_x(options[i]), y, i == result_selected ? 7 : 6);
        }
        print("2ND: CHOOSE  MODE: LEVELS", 4, 120, 5);
        return;
    }

    if(screen != Screen::Browser || !is_title()) return;
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
