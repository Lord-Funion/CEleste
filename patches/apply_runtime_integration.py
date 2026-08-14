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
emu = (ROOT / "src/emu.cpp").read_text()
if "uint8_t tile = mget(" not in emu:
    replace("src/emu.cpp", "            uint8_t tile = tilemap[x + cell_x + (y + cell_y) * 128];\n", "            uint8_t tile = mget(x + cell_x, y + cell_y);\n")
replace("src/emu.cpp", "        int y;\n        gfx_rletsprite_t *sprite;\n    } cache[3] = {};\n    auto entry = &cache[layers - 1];\n    if(entry->x != cell_x || entry->y != cell_y || !entry->sprite) {\n        entry->x = cell_x;\n        entry->y = cell_y;\n", "        int y;\n        uint16_t generation;\n        gfx_rletsprite_t *sprite;\n    } cache[3] = {};\n    auto entry = &cache[layers - 1];\n    const uint16_t generation = custom_levels::generation();\n    if(entry->x != cell_x || entry->y != cell_y || entry->generation != generation || !entry->sprite) {\n        entry->x = cell_x;\n        entry->y = cell_y;\n        entry->generation = generation;\n")

insert_after("src/classic.cpp", '#include "practice.h"\n', '#include "custom_levels.h"\n#include "custom_level_menu.h"\n')
insert_after("src/classic.cpp", "void _init(FILE *save) {\n", "    custom_levels::initialize();\n    custom_level_menu::initialize();\n")
insert_after("src/classic.cpp", "void title_screen() {\n", "    custom_levels::unload();\n")
replace("src/classic.cpp", "bool is_title() {\n    return level_index() == 31;\n}\n", "bool is_title() {\n    return !custom_levels::active() && level_index() == 31;\n}\n")
replace("src/classic.cpp", "    if(y < -4 and level_index() < 30) {\n", "    if(y < -4 and (custom_levels::active() or level_index() < 30)) {\n")
classic = (ROOT / "src/classic.cpp").read_text()
# The custom-level next-room block may contain additional behavior (for example,
# resetting per-level power-ups when advancing to the next level in a pack).
# Presence of the custom-level branch is sufficient to prove this hook is integrated.
if "void next_room() {\n    if(custom_levels::active()) {" not in classic:
    replace("src/classic.cpp", "void next_room() {\n    if(room.x == 2 and room.y == 1) {\n", "void next_room() {\n    if(custom_levels::active()) {\n        if(custom_levels::next_room()) {\n            const uint8_t index = custom_levels::room_index();\n            load_room(index % 8, index / 8);\n        } else if(custom_levels::next_level()) {\n            load_room(0, 0);\n        } else {\n            title_screen();\n        }\n        return;\n    }\n    if(room.x == 2 and room.y == 1) {\n")
replace("src/classic.cpp", "void prev_room() {\n    if(level_index() < 1) return;\n", "void prev_room() {\n    if(custom_levels::active()) {\n        if(custom_levels::previous_room()) {\n            const uint8_t index = custom_levels::room_index();\n            load_room(index % 8, index / 8);\n        }\n        return;\n    }\n    if(level_index() < 1) return;\n")
replace("src/classic.cpp", "    if(practice_mode && level_index() != 30) {\n", "    if(!custom_levels::active() && practice_mode && level_index() != 30) {\n")
classic = (ROOT / "src/classic.cpp").read_text()
if "source_collected(" not in classic and "bool has_fruit = custom_levels::active()" not in classic:
    replace("src/classic.cpp", "    bool has_fruit = got_fruit[level_index()];\n", "    bool has_fruit = custom_levels::active() ? false : got_fruit[level_index()];\n")
classic = (ROOT / "src/classic.cpp").read_text()
if "    if(frames == 0 and !custom_results and (custom_levels::active() or level_index() < 30)) {\n" not in classic:
    replace("src/classic.cpp", "    if(frames == 0 and level_index() < 30) {\n", "    if(frames == 0 and (custom_levels::active() or level_index() < 30)) {\n")
insert_after("src/classic.cpp", "    update_title_sequences();\n", "\n    if(custom_level_menu::update()) {\n        profiler_end(update);\n        return;\n    }\n")
classic = (ROOT / "src/classic.cpp").read_text()
if "custom_level_menu::draw();" not in classic:
    replace("src/classic.cpp", "    profiler_end(draw);\n}\n\nvoid Object::draw() {\n", "    custom_level_menu::draw();\n    profiler_end(draw);\n}\n\nvoid Object::draw() {\n")
replace("src/classic.cpp", "    return !practice_mode && !new_game_plus && level_index() == 30 &&\n", "    return !custom_levels::active() && !practice_mode && !new_game_plus && level_index() == 30 &&\n")
replace("src/classic.cpp", "bool new_game_plus_available() {\n    if(practice_mode || new_game_plus || level_index() != 30) return false;\n", "bool new_game_plus_available() {\n    if(custom_levels::active() || practice_mode || new_game_plus || level_index() != 30) return false;\n")
replace("src/classic.cpp", "    return !test_mode && !is_title() && level_index() != 30;\n", "    return !custom_levels::active() && !test_mode && !is_title() && level_index() != 30;\n")

# Keep the Silver Key visually identical to the user's hand-recolored versions
# of the normal key frames: yellow -> light gray, orange -> dark gray.
replace(
    "src/classic.cpp",
    """void SilverKey::draw() {
    // The silver key deliberately reuses the original animated chest-key art.
    pal(9, 6);
    pal(10, 7);
    spr_rot(sprite, x, y, custom_rotation, flip.x, flip.y);
    pal(9, 9);
    pal(10, 10);
}
""",
    """void SilverKey::draw() {
    // Reuse the normal key's three animation frames with the user's exact
    // silver recolor: orange -> dark gray and yellow -> light gray.
    pal(9, 5);
    pal(10, 6);
    spr_rot(sprite, x, y, custom_rotation, flip.x, flip.y);
    pal(9, 9);
    pal(10, 10);
}
"""
)

editor = (ROOT / "tools/calculator-editor/src/main.cpp").read_text()
silver_helper = """void draw_silver_key_texture(int x, int y, uint8_t rotation = 0) {
    // Copy normal-key frame 8 and apply the same recolor used by the runtime.
    // Atlas graphics use repeated-nibble palette slots: 0x99=orange,
    // 0xAA=yellow, 0x55=dark gray, 0x66=light gray.
    gfx_TempSprite(silver, 8, 8);
    silver->width = 8;
    silver->height = 8;
    std::memcpy(silver->data, atlas_tiles[8]->data, 64);
    for(uint8_t i = 0; i < 64; ++i) {
        if(silver->data[i] == 0x99) silver->data[i] = 0x55;
        else if(silver->data[i] == 0xAA) silver->data[i] = 0x66;
    }

    rotation &= 3;
    if(!rotation) {
        gfx_TransparentSprite(silver, x, y);
        return;
    }

    gfx_TempSprite(rotated, 8, 8);
    if(rotation == 1) gfx_RotateSpriteC(silver, rotated);
    else if(rotation == 2) gfx_RotateSpriteHalf(silver, rotated);
    else gfx_RotateSpriteCC(silver, rotated);
    gfx_TransparentSprite(rotated, x, y);
}

"""
if "void draw_silver_key_texture(" not in editor:
    marker = "void rotate_offset(int dx, int dy, uint8_t rotation, int &rx, int &ry) {\n"
    if marker not in editor:
        raise SystemExit("Expected CELEDIT draw helper insertion point not found")
    editor = editor.replace(marker, silver_helper + marker, 1)

old_silver_placeholder = """    if(id == 130) {
        gfx_SetColor(6);
        gfx_FillRectangle(x, y, 4, 4);
        gfx_FillRectangle(x + 3, y + 1, 5, 2);
        gfx_FillRectangle(x + 6, y + 3, 2, 2);
        gfx_SetColor(7);
        gfx_SetPixel(x + 1, y + 1);
        return;
    }
"""
new_silver_preview = """    if(id == 130) {
        draw_silver_key_texture(x, y, rotation);
        return;
    }
"""
if old_silver_placeholder in editor:
    editor = editor.replace(old_silver_placeholder, new_silver_preview, 1)
elif new_silver_preview not in editor:
    raise SystemExit("Expected CELEDIT Silver Key draw block not found")
(ROOT / "tools/calculator-editor/src/main.cpp").write_text(editor)

print("Custom-level runtime integration applied.")
