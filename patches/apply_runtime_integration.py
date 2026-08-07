#!/usr/bin/env python3
"""Apply guarded custom-level hooks to the existing CEleste runtime."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text()
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"Expected integration point not found in {path}: {old[:80]!r}")
    target.write_text(text.replace(old, new, 1))

def insert_after(path: str, marker: str, addition: str) -> None:
    replace(path, marker, marker + addition)

replace("src/emu.h", "extern const uint8_t tilemap[64 * 128];\n\n#define mget(x, y) tilemap[(x) + (y) * 128]\n", "extern const uint8_t tilemap[64 * 128];\n\nuint8_t mget(int x, int y);\n")
insert_after("src/emu.cpp", '#include "practice.h"\n', '#include "custom_levels.h"\n')
insert_after("src/emu.cpp", "#define SAVE_NAME \"CelesteS\"\n", "\nuint8_t mget(int x, int y) {\n    if(custom_levels::active()) {\n        if(x < 0 || y < 0) return 0;\n        const uint8_t room_index = static_cast<uint8_t>((x / 16) + (y / 16) * 8);\n        return custom_levels::tile(room_index, static_cast<uint8_t>(x % 16), static_cast<uint8_t>(y % 16));\n    }\n    if(x < 0 || x >= 128 || y < 0 || y >= 64) return 0;\n    return tilemap[x + y * 128];\n}\n")
replace("src/emu.cpp", "            uint8_t tile = tilemap[x + cell_x + (y + cell_y) * 128];\n", "            uint8_t tile = mget(x + cell_x, y + cell_y);\n")
replace("src/emu.cpp", "        int y;\n        gfx_rletsprite_t *sprite;\n    } cache[3] = {};\n    auto entry = &cache[layers - 1];\n    if(entry->x != cell_x || entry->y != cell_y || !entry->sprite) {\n        entry->x = cell_x;\n        entry->y = cell_y;\n", "        int y;\n        uint16_t generation;\n        gfx_rletsprite_t *sprite;\n    } cache[3] = {};\n    auto entry = &cache[layers - 1];\n    const uint16_t generation = custom_levels::generation();\n    if(entry->x != cell_x || entry->y != cell_y || entry->generation != generation || !entry->sprite) {\n        entry->x = cell_x;\n        entry->y = cell_y;\n        entry->generation = generation;\n")

insert_after("src/classic.cpp", '#include "practice.h"\n', '#include "custom_levels.h"\n#include "custom_level_menu.h"\n')
insert_after("src/classic.cpp", "void _init(FILE *save) {\n", "    custom_levels::initialize();\n    custom_level_menu::initialize();\n")
insert_after("src/classic.cpp", "void title_screen() {\n", "    custom_levels::unload();\n")
replace("src/classic.cpp", "bool is_title() {\n    return level_index() == 31;\n}\n", "bool is_title() {\n    return !custom_levels::active() && level_index() == 31;\n}\n")
replace("src/classic.cpp", "    if(y < -4 and level_index() < 30) {\n", "    if(y < -4 and (custom_levels::active() or level_index() < 30)) {\n")
replace("src/classic.cpp", "void next_room() {\n    if(room.x == 2 and room.y == 1) {\n", "void next_room() {\n    if(custom_levels::active()) {\n        if(custom_levels::next_room()) {\n            const uint8_t index = custom_levels::room_index();\n            load_room(index % 8, index / 8);\n        } else if(custom_levels::next_level()) {\n            load_room(0, 0);\n        } else {\n            title_screen();\n        }\n        return;\n    }\n    if(room.x == 2 and room.y == 1) {\n")
replace("src/classic.cpp", "void prev_room() {\n    if(level_index() < 1) return;\n", "void prev_room() {\n    if(custom_levels::active()) {\n        if(custom_levels::previous_room()) {\n            const uint8_t index = custom_levels::room_index();\n            load_room(index % 8, index / 8);\n        }\n        return;\n    }\n    if(level_index() < 1) return;\n")
replace("src/classic.cpp", "    if(practice_mode && level_index() != 30) {\n", "    if(!custom_levels::active() && practice_mode && level_index() != 30) {\n")
# Earlier revisions used a simple custom-level fruit guard here. Newer revisions
# deliberately keep per-room collection state so strawberries/keys/chests/fake
# walls stay consumed after a death/restart. Accept either integrated form.
classic = (ROOT / "src/classic.cpp").read_text()
if "bool has_fruit = custom_levels::active()" not in classic:
    replace("src/classic.cpp", "    bool has_fruit = got_fruit[level_index()];\n", "    bool has_fruit = custom_levels::active() ? false : got_fruit[level_index()];\n")
replace("src/classic.cpp", "    if(frames == 0 and level_index() < 30) {\n", "    if(frames == 0 and (custom_levels::active() or level_index() < 30)) {\n")
insert_after("src/classic.cpp", "    update_title_sequences();\n", "\n    if(custom_level_menu::update()) {\n        profiler_end(update);\n        return;\n    }\n")
replace("src/classic.cpp", "    profiler_end(draw);\n}\n\nvoid Object::draw() {\n", "    custom_level_menu::draw();\n    profiler_end(draw);\n}\n\nvoid Object::draw() {\n")
replace("src/classic.cpp", "    return !practice_mode && !new_game_plus && level_index() == 30 &&\n", "    return !custom_levels::active() && !practice_mode && !new_game_plus && level_index() == 30 &&\n")
replace("src/classic.cpp", "bool new_game_plus_available() {\n    if(practice_mode || new_game_plus || level_index() != 30) return false;\n", "bool new_game_plus_available() {\n    if(custom_levels::active() || practice_mode || new_game_plus || level_index() != 30) return false;\n")
replace("src/classic.cpp", "    return !test_mode && !is_title() && level_index() != 30;\n", "    return !custom_levels::active() && !test_mode && !is_title() && level_index() != 30;\n")
print("Custom-level runtime integration applied.")
