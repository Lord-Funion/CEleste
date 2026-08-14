#pragma once

namespace custom_level_menu {
void initialize();
bool update();
void draw();
bool open();
bool results_open();
void close();
void show_browser();
void show_results(int run_deaths, int run_minutes, int run_seconds);
}
