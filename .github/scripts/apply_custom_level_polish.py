from pathlib import Path


def replace(path, old, new):
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"Expected text not found in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))

# ---- custom level runtime helpers ----
replace(
    "src/custom_levels.h",
    "const CatalogEntry *catalog_entry(uint8_t index);\nbool load(uint8_t catalog_index, uint16_t pack_level_index = 0);",
    "const CatalogEntry *catalog_entry(uint8_t index);\nconst CatalogEntry *current_catalog_entry();\nbool load(uint8_t catalog_index, uint16_t pack_level_index = 0);\nbool reload();"
)

replace(
    "src/custom_levels.cpp",
    "uint8_t catalog_size() { return count; }\nconst CatalogEntry *catalog_entry(uint8_t index) { return index < count ? &catalog[index] : nullptr; }\n\nbool load(uint8_t catalog_index, uint16_t pack_level_index) {",
    "uint8_t catalog_size() { return count; }\nconst CatalogEntry *catalog_entry(uint8_t index) { return index < count ? &catalog[index] : nullptr; }\nconst CatalogEntry *current_catalog_entry() { return is_active && active_catalog < count ? &catalog[active_catalog] : nullptr; }\n\nbool load(uint8_t catalog_index, uint16_t pack_level_index) {"
)
replace(
    "src/custom_levels.cpp",
    "void unload() { if (is_active) { ++content_generation; if (!content_generation) content_generation = 1; } is_active = false; current_room = 0; active_pack_level = 0; std::memset(unlocked_gate_links, 0, sizeof unlocked_gate_links); }",
    "bool reload() {\n    if (!is_active || active_catalog >= count) return false;\n    return load(active_catalog, active_pack_level);\n}\n\nvoid unload() { if (is_active) { ++content_generation; if (!content_generation) content_generation = 1; } is_active = false; current_room = 0; active_pack_level = 0; std::memset(unlocked_gate_links, 0, sizeof unlocked_gate_links); }"
)

# ---- custom level browser can be opened programmatically from results ----
replace(
    "src/custom_level_menu.h",
    "void initialize();\nbool update();",
    "void initialize();\nvoid show();\nbool update();"
)
replace(
    "src/custom_level_menu.cpp",
    "bool open() { return menu_open; }\n\nbool update() {",
    "bool open() { return menu_open; }\n\nvoid show() {\n    custom_levels::scan();\n    selected = 0;\n    menu_open = true;\n    old_mode = kb_IsDown(kb_KeyMode);\n    old_up = kb_IsDown(kb_KeyUp);\n    old_down = kb_IsDown(kb_KeyDown);\n    old_play = kb_IsDown(kb_Key2nd) || kb_IsDown(kb_KeyAlpha);\n}\n\nbool update() {"
)
replace(
    "src/custom_level_menu.cpp",
    "        custom_levels::scan(); selected = 0; menu_open = true; return true;",
    "        show(); return true;"
)

# ---- game completion state ----
replace(
    "src/classic.cpp",
    "int covenant_notice_timer = 0;\n\nconstexpr int TOTAL_STRAWBERRIES = 18;",
    "int covenant_notice_timer = 0;\nbool custom_results = false;\nbool custom_results_input_ready = false;\nint custom_results_delay = 0;\n\nconstexpr int TOTAL_STRAWBERRIES = 18;"
)
replace(
    "src/classic.cpp",
    "    start_game = false;\n    start_game_flash = 0;\n    //music(40,0,7);",
    "    start_game = false;\n    start_game_flash = 0;\n    custom_results = false;\n    custom_results_input_ready = false;\n    custom_results_delay = 0;\n    pause_player = false;\n    will_restart = false;\n    delay_restart = 0;\n    //music(40,0,7);"
)
replace(
    "src/classic.cpp",
    "    music_timer = 0;\n    start_game = false;\n    //music(0,0,7);",
    "    music_timer = 0;\n    start_game = false;\n    custom_results = false;\n    custom_results_input_ready = false;\n    custom_results_delay = 0;\n    pause_player = false;\n    will_restart = false;\n    delay_restart = 0;\n    //music(0,0,7);"
)
replace(
    "src/classic.cpp",
    "void restart_room() {\n    will_restart = true;\n    delay_restart = 15;\n}\n\n\nvoid next_room() {",
    "void restart_room() {\n    will_restart = true;\n    delay_restart = 15;\n}\n\nvoid show_custom_results() {\n    custom_results = true;\n    custom_results_input_ready = false;\n    custom_results_delay = 12;\n    pause_player = true;\n    will_restart = false;\n    delay_restart = 0;\n    shake = 0;\n}\n\nvoid replay_custom_level() {\n    custom_results = false;\n    custom_results_input_ready = false;\n    pause_player = false;\n    deaths = 0;\n    climb_enabled = false;\n    climb_stamina = CLIMB_STAMINA_MAX;\n    if(custom_levels::reload()) {\n        begin_game();\n    } else {\n        title_screen();\n        custom_level_menu::show();\n    }\n}\n\nvoid return_to_custom_browser() {\n    custom_results = false;\n    custom_results_input_ready = false;\n    pause_player = false;\n    title_screen();\n    custom_level_menu::show();\n}\n\nvoid next_room() {"
)
replace(
    "src/classic.cpp",
    "        } else {\n            title_screen();\n        }\n        return;\n    }",
    "        } else {\n            show_custom_results();\n        }\n        return;\n    }"
)
replace(
    "src/classic.cpp",
    "    if(frames == 0 and (custom_levels::active() or level_index() < 30)) {",
    "    if(frames == 0 and !custom_results and (custom_levels::active() or level_index() < 30)) {"
)
replace(
    "src/classic.cpp",
    "    if(music_timer > 0) {",
    "    if(custom_results) {\n        const bool second = kb_IsDown(kb_Key2nd);\n        const bool alpha = kb_IsDown(kb_KeyAlpha);\n        const bool mode = kb_IsDown(kb_KeyMode);\n        if(custom_results_delay > 0) {\n            --custom_results_delay;\n        } else if(!custom_results_input_ready) {\n            if(!second && !alpha && !mode) custom_results_input_ready = true;\n        } else if(second) {\n            return_to_custom_browser();\n        } else if(alpha) {\n            replay_custom_level();\n        } else if(mode) {\n            custom_results = false;\n            custom_results_input_ready = false;\n            pause_player = false;\n            title_screen();\n        }\n        profiler_end(update);\n        return;\n    }\n\n    if(music_timer > 0) {"
)

# Custom rooms should not display fake built-in mountain meter numbers.
replace(
    "src/classic.cpp",
    "        if(room.x == 3 and room.y == 1) {\n            print(\"old site\", 48, 62, 7);",
    "        if(custom_levels::active()) {\n            const clevel::Level *level = custom_levels::level();\n            print(\"room\", 42, 62, 7);\n            print_int(custom_levels::room_index() + 1);\n            print(\"/\");\n            print_int(level ? level->room_count : 1);\n        } else if(room.x == 3 and room.y == 1) {\n            print(\"old site\", 48, 62, 7);"
)

# Draw a summit-style completion card over the final room.
replace(
    "src/classic.cpp",
    "    custom_level_menu::draw();\n    profiler_end(draw);",
    "    if(custom_results) {\n        const custom_levels::CatalogEntry *entry = custom_levels::current_catalog_entry();\n        const clevel::Level *level = custom_levels::level();\n        char title[23] = \"CUSTOM LEVEL\";\n        const char *source_title = (entry && entry->kind == clevel::KIND_PACK) ? entry->title : (level ? level->title : nullptr);\n        if(source_title && source_title[0]) {\n            std::strncpy(title, source_title, sizeof title - 1);\n            title[sizeof title - 1] = '\\0';\n        }\n        rectfill(17, 12, 111, 114, 0);\n        print(entry && entry->kind == clevel::KIND_PACK ? \"PACK COMPLETE\" : \"LEVEL COMPLETE\", 34, 18, 11);\n        print(title, 21, 29, 7);\n        spr(118, 60, 40);\n        print(\"time\", 31, 58, 6);\n        draw_time(49, 56);\n        print(\"deaths:\", 38, 69, 6);\n        print_int(deaths);\n        print(\"2nd: levels\", 37, 87, 7);\n        print(\"alpha: replay\", 33, 96, 6);\n        print(\"mode: title\", 37, 105, 6);\n    }\n\n    custom_level_menu::draw();\n    profiler_end(draw);"
)

# ---- CELEDIT QoL: visible dirty state + quick save ----
replace(
    "tools/calculator-editor/src/main.cpp",
    "bool new_project_armed = false;\n\nenum PropertyTarget",
    "bool new_project_armed = false;\nbool dirty = false;\n\nenum PropertyTarget"
)
replace(
    "tools/calculator-editor/src/main.cpp",
    "    ti_Close(h);\n    set_notice(\"Draft saved\");",
    "    ti_Close(h);\n    dirty = false;\n    set_notice(\"Draft saved\");"
)
replace(
    "tools/calculator-editor/src/main.cpp",
    "void push_undo() {\n    if(undo_count == HISTORY_SIZE) {",
    "void push_undo() {\n    dirty = true;\n    if(undo_count == HISTORY_SIZE) {"
)
replace(
    "tools/calculator-editor/src/main.cpp",
    "    rooms_cursor = room_index;\n    set_notice(\"Undo\");",
    "    rooms_cursor = room_index;\n    dirty = true;\n    set_notice(\"Undo\");"
)
replace(
    "tools/calculator-editor/src/main.cpp",
    "    rooms_cursor = room_index;\n    set_notice(\"Redo\");",
    "    rooms_cursor = room_index;\n    dirty = true;\n    set_notice(\"Redo\");"
)
replace(
    "tools/calculator-editor/src/main.cpp",
    "    gfx_PrintStringXY(\"CELEDIT\",8,5);\n    gfx_SetTextFGColor(13);",
    "    gfx_PrintStringXY(\"CELEDIT\",8,5);\n    if(dirty) { gfx_SetTextFGColor(PICO_YELLOW); gfx_PrintStringXY(\"*\",54,5); }\n    gfx_SetTextFGColor(13);"
)
replace(
    "tools/calculator-editor/src/main.cpp",
    "    gfx_PrintStringXY(\"+/- room  CLEAR saves+quits\",8,230);",
    "    gfx_PrintStringXY(\"MODE save  DEL undo  ENTER redo\",8,230);"
)
replace(
    "tools/calculator-editor/src/main.cpp",
    "            if(p(1,kb_Del))undo();\n            if(p(6,kb_Enter))redo();",
    "            if(p(1,kb_Mode))save_draft();\n            if(p(1,kb_Del))undo();\n            if(p(6,kb_Enter))redo();"
)

print("Applied CEleste custom-level and CELEDIT QoL polish")
