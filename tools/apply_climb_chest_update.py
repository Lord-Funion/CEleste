#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace(path, old, new):
    p = ROOT / path
    s = p.read_text()
    if new in s:
        return False
    if old not in s:
        raise SystemExit(f'patch point not found in {path}: {old[:100]!r}')
    p.write_text(s.replace(old, new, 1))
    return True

changed = False

changed |= replace('src/classic.h',
'''    FLAG = 118,\n    PLAYER\n};''',
'''    FLAG = 118,\n    CLIMB_CHEST = 129,\n    PLAYER\n};''')
changed |= replace('src/classic.h',
'''class Platform : public Object {''',
'''class ClimbChest : public Object {\npublic:\n    ClimbChest(int x, int y);\n    void update() override;\n    void draw() override;\n};\n\nclass Platform : public Object {''')

changed |= replace('src/classic.cpp',
'''bool unlimited_dashes = false; // Session-only: deliberately excluded from saves.\n''',
'''bool unlimited_dashes = false; // Session-only: deliberately excluded from saves.\nbool climb_enabled = false;       // Custom-level Climb Chest power-up.\nint climb_stamina = 1100;          // 110.0 stamina, stored in tenths.\nconstexpr int CLIMB_STAMINA_MAX = 1100;\nconstexpr int CLIMB_HANG_COST = 4;\nconstexpr int CLIMB_UP_COST = 15;\nconstexpr int CLIMB_JUMP_COST = 275;\n''')

changed |= replace('src/classic.cpp',
'''    max_dash = 1;\n    new_game_plus = 0;''',
'''    max_dash = 1;\n    climb_enabled = false;\n    climb_stamina = CLIMB_STAMINA_MAX;\n    new_game_plus = 0;''')
changed |= replace('src/classic.cpp',
'''    max_dash = 2;\n    new_game_plus = 1;''',
'''    max_dash = 2;\n    climb_enabled = false;\n    climb_stamina = CLIMB_STAMINA_MAX;\n    new_game_plus = 1;''')

changed |= replace('src/classic.cpp',
'''    if(on_ground) {\n        grace = 6;\n        if(djump < max_dash) {''',
'''    if(on_ground) {\n        grace = 6;\n        if(climb_enabled) climb_stamina = CLIMB_STAMINA_MAX;\n        if(djump < max_dash) {''')

changed |= replace('src/classic.cpp',
'''    } else {\n\n        // move\n        subpixel maxrun = SP(1);''',
'''    } else {\n\n        // Optional modern-Celeste-style climbing, unlocked by the custom Climb Chest.\n        // MATH is the grab button. Ice walls deliberately cannot be climbed.\n        int climb_wall = 0;\n        if(climb_enabled && !on_ground && !dash && kb_IsDown(kb_KeyMath) && climb_stamina > 0) {\n            if(is_solid(-3, 0) && !is_ice(-3, 0)) climb_wall = -1;\n            else if(is_solid(3, 0) && !is_ice(3, 0)) climb_wall = 1;\n        }\n        bool climbing = climb_wall != 0;\n\n        if(climbing && jbuffer > 0) {\n            jbuffer = 0;\n            climb_stamina = max(0, climb_stamina - CLIMB_JUMP_COST);\n            spd.y = SP(-2);\n            spd.x = -climb_wall * SP(2);\n            new Smoke(x + climb_wall * 6, y);\n            climbing = false;\n        }\n\n        if(climbing) {\n            spd.x = 0;\n            flip.x = climb_wall < 0;\n            if(btn(k_up)) {\n                spd.y = SP(-0.8);\n                climb_stamina = max(0, climb_stamina - CLIMB_UP_COST);\n            } else if(btn(k_down)) {\n                spd.y = SP(0.8);\n            } else {\n                spd.y = 0;\n                climb_stamina = max(0, climb_stamina - CLIMB_HANG_COST);\n            }\n            if(climb_stamina == 0) climbing = false;\n        }\n\n        if(!climbing) {\n        // move\n        subpixel maxrun = SP(1);''')

changed |= replace('src/classic.cpp',
'''        } else if(dash and djump <= 0 and not unlimited_dashes) {\n            //psfx(9);\n            new Smoke(x, y);\n        }\n\n    }\n\n    // animation''',
'''        } else if(dash and djump <= 0 and not unlimited_dashes) {\n            //psfx(9);\n            new Smoke(x, y);\n        }\n        } // !climbing\n\n    }\n\n    // animation''')

changed |= replace('src/classic.cpp',
'''Platform::Platform(int x, int y, int dir) : Object(x, y) {''',
'''ClimbChest::ClimbChest(int x, int y) : Object(x, y) {\n    type = CLIMB_CHEST;\n    sprite = CHEST;\n    solids = false;\n}\n\nvoid ClimbChest::update() {\n    Player *hit = collide_player(0, 0);\n    if(hit == nullptr) return;\n    climb_enabled = true;\n    climb_stamina = CLIMB_STAMINA_MAX;\n    if(custom_levels::active()) custom_levels::collect_source(custom_levels::room_index(), custom_source);\n    new Smoke(x, y);\n    freeze = 4;\n    shake = 6;\n    delete this;\n}\n\nvoid ClimbChest::draw() {\n    spr_rot(CHEST, x, y, custom_rotation);\n    draw_plus(x + 4, y + 4, 11);\n}\n\nPlatform::Platform(int x, int y, int dir) : Object(x, y) {''')

changed |= replace('src/classic.cpp',
'''        case CHEST:\n            object = (custom && (gameplay_flags & 0x01u)) || !source_done ? new Chest(x, y) : nullptr;\n            break;\n        case PLATFORM:''',
'''        case CHEST:\n            object = (custom && (gameplay_flags & 0x01u)) || !source_done ? new Chest(x, y) : nullptr;\n            break;\n        case CLIMB_CHEST:\n            object = custom && !source_done ? new ClimbChest(x, y) : nullptr;\n            break;\n        case PLATFORM:''')

changed |= replace('src/classic.cpp',
'''        } else if(custom_levels::next_level()) {\n            load_room(0, 0);''',
'''        } else if(custom_levels::next_level()) {\n            climb_enabled = false;\n            climb_stamina = CLIMB_STAMINA_MAX;\n            load_room(0, 0);''')

changed |= replace('src/classic.cpp',
'''        print("2nd+alpha", 46, 72, 5);\n        print("maddy thorson", 40, 84, 5);\n        print("noel berry", 46, 90, 5);\n        print("ce port:", 48, 102, 5);\n        print("john cesarz", 42, 108, 5);''',
'''        print("2nd+alpha", 46, 72, 5);\n        print("mode: custom levels", 29, 78, 5);\n        print("maddy thorson", 40, 90, 5);\n        print("noel berry", 46, 96, 5);\n        print("ce port:", 48, 108, 5);\n        print("john cesarz", 42, 114, 5);''')

changed |= replace('src/classic.cpp',
'''    if(test_mode_notice_timer > 0) {''',
'''    if(climb_enabled && !is_title() && (kb_IsDown(kb_KeyMath) || climb_stamina < CLIMB_STAMINA_MAX)) {\n        rectfill(104, 4, 124, 8, 0);\n        const int width = climb_stamina * 19 / CLIMB_STAMINA_MAX;\n        if(width > 0) rectfill(105, 5, 105 + width - 1, 7, climb_stamina < 275 ? 8 : 11);\n    }\n\n    if(test_mode_notice_timer > 0) {''')

changed |= replace('tools/calculator-editor/src/main.cpp',
'''    8,11,12,18,20,22,23,26,28,64,86,96,118\n};\n\nconstexpr uint8_t ENTITY_IDS[] = {8,11,12,18,20,22,23,26,28,64,86,96,118};''',
'''    8,11,12,18,20,22,23,26,28,64,86,96,118,129\n};\n\nconstexpr uint8_t ENTITY_IDS[] = {8,11,12,18,20,22,23,26,28,64,86,96,118,129};''')
changed |= replace('tools/calculator-editor/src/main.cpp',
'''        case 118: return "Summit flag";''',
'''        case 118: return "Summit flag";\n        case 129: return "Climb chest";''')
changed |= replace('tools/calculator-editor/src/main.cpp',
'''void draw_piece(uint8_t id, int x, int y, uint8_t rotation = 0) {\n    if(id == 64) {''',
'''void draw_piece(uint8_t id, int x, int y, uint8_t rotation = 0) {\n    if(id == 129) {\n        draw_sprite(20, x, y, rotation);\n        gfx_SetColor(11);\n        gfx_SetPixel(x + 4, y + 3);\n        gfx_SetPixel(x + 3, y + 4);\n        gfx_SetPixel(x + 4, y + 4);\n        gfx_SetPixel(x + 5, y + 4);\n        gfx_SetPixel(x + 4, y + 5);\n        return;\n    }\n    if(id == 64) {''')

changed |= replace('docs/custom-level-format.md',
'''| 118 | Summit flag |\n''',
'''| 118 | Summit flag |\n| 129 | Climb Chest (unlocks MATH wall-grab/climbing with stamina) |\n''')
changed |= replace('docs/custom-level-format.md',
'''- Other objects keep their ordinary gameplay semantics unless the runtime explicitly defines a directional semantic for that object. Rotation can therefore be graphical for objects whose original mechanics are not orientation-dependent.\n''',
'''- Other objects keep their ordinary gameplay semantics unless the runtime explicitly defines a directional semantic for that object. Rotation can therefore be graphical for objects whose original mechanics are not orientation-dependent.\n- The Climb Chest (`129`) is a custom power-up entity. Touching it unlocks modern-Celeste-style wall grabbing for the remainder of that custom level: hold `MATH` against a non-ice wall, use Up/Down to climb, and manage the 110-point stamina pool. Ground contact restores stamina.\n''')
changed |= replace('README.md',
'''- complete 16×16 big/dash-upgrade chests;\n- the summit flag.''',
'''- complete 16×16 big/dash-upgrade chests;\n- the Climb Chest, which unlocks `MATH`-held wall grabbing/climbing with Celeste-style stamina;\n- the summit flag.''')

print('Applied climb chest/title hint update.' if changed else 'Already applied.')
