import re
from pathlib import Path

path = Path('tools/calculator-editor/src/main.cpp')
s = path.read_text()

anchor = 'constexpr uint8_t GAME_MASK = clevel::ENTITY_FLAG_MASK;\n'
colors = '''constexpr uint8_t GAME_MASK = clevel::ENTITY_FLAG_MASK;

// CELEDIT uses the 16-color PICO-8 palette for its UI. The atlas itself is
// encoded with repeated-nibble indices (0x00, 0x11, ... 0xFF), so both
// palettes are loaded at startup: imgpalette first, then these UI slots.
constexpr uint8_t PICO_BLACK = 0;
constexpr uint8_t PICO_DARK_BLUE = 1;
constexpr uint8_t PICO_DARK_PURPLE = 2;
constexpr uint8_t PICO_DARK_GREEN = 3;
constexpr uint8_t PICO_BROWN = 4;
constexpr uint8_t PICO_DARK_GRAY = 5;
constexpr uint8_t PICO_LIGHT_GRAY = 6;
constexpr uint8_t PICO_WHITE = 7;
constexpr uint8_t PICO_RED = 8;
constexpr uint8_t PICO_ORANGE = 9;
constexpr uint8_t PICO_YELLOW = 10;
constexpr uint8_t PICO_GREEN = 11;
constexpr uint8_t PICO_BLUE = 12;
constexpr uint8_t PICO_LAVENDER = 13;
constexpr uint8_t PICO_PINK = 14;
constexpr uint8_t PICO_PEACH = 15;
'''
if 'constexpr uint8_t PICO_WHITE = 7;' not in s:
    if anchor not in s:
        raise SystemExit('color constant anchor not found')
    s = s.replace(anchor, colors, 1)

old_tile = '''uint8_t tile_color(uint8_t id) {
    if(id == 0) return 0;
    if(category_of(id) == ICE) return 11;
    if(category_of(id) == HAZARDS) return 7;
    if(id == 26 || id == 28) return 224;
    if(id == 22) return 47;
    if(id == 18) return 192;
    if(id == 8) return 231;
    if(id == 130) return 6;
    if(id == 131) return 5;
    if(is_entity(id)) return 164;
    return static_cast<uint8_t>(80 + (id * 13) % 80);
}
'''
new_tile = '''uint8_t tile_color(uint8_t id) {
    if(id == 0) return PICO_BLACK;
    if(id == 26 || id == 28 || id == 18) return PICO_RED;
    if(id == 22) return PICO_BLUE;
    if(id == 8) return PICO_YELLOW;
    if(id == 130) return PICO_LIGHT_GRAY;
    if(id == 131) return PICO_DARK_GRAY;
    if(category_of(id) == ICE) return PICO_BLUE;
    if(category_of(id) == HAZARDS) return PICO_RED;
    if(category_of(id) == TERRAIN) return PICO_DARK_GRAY;
    if(category_of(id) == BACKGROUND) return PICO_DARK_BLUE;
    if(is_entity(id)) return PICO_LAVENDER;
    return PICO_DARK_PURPLE;
}
'''
if old_tile in s:
    s = s.replace(old_tile, new_tile, 1)
elif new_tile not in s:
    raise SystemExit('tile_color block not found')

replacements = {
    'gfx_SetTextFGColor(255)': 'gfx_SetTextFGColor(PICO_WHITE)',
    'gfx_SetTextFGColor(231)': 'gfx_SetTextFGColor(PICO_YELLOW)',
    'gfx_SetTextFGColor(224)': 'gfx_SetTextFGColor(PICO_RED)',
    'gfx_SetTextFGColor(active?255:13)': 'gfx_SetTextFGColor(active?PICO_WHITE:PICO_LAVENDER)',
    'gfx_SetTextFGColor(idx==rooms_cursor?255:13)': 'gfx_SetTextFGColor(idx==rooms_cursor?PICO_WHITE:PICO_LAVENDER)',
    'gfx_SetTextFGColor(i==action_cursor?255:13)': 'gfx_SetTextFGColor(i==action_cursor?PICO_WHITE:PICO_LAVENDER)',
    'gfx_SetColor(tool==ERASER?224:tool==FILL?231:tool==PICKER?47:255)': 'gfx_SetColor(tool==ERASER?PICO_RED:tool==FILL?PICO_YELLOW:tool==PICKER?PICO_BLUE:PICO_WHITE)',
    'gfx_SetColor(224);gfx_FillRectangle(x+3+r.spawn_x*2,y+3+r.spawn_y*2,2,2);': 'gfx_SetColor(PICO_RED);gfx_FillRectangle(x+3+r.spawn_x*2,y+3+r.spawn_y*2,2,2);',
    'gfx_PrintUInt(rot*90,3);gfx_PrintChar(176);': 'gfx_PrintUInt(rot*90,3);gfx_PrintString(" deg");',
}
for old, new in replacements.items():
    s = s.replace(old, new)

setup_old = '''    gfx_SetTextTransparentColor(0);
    gfx_SetPalette(mypalette,sizeof mypalette,0);'''
setup_new = '''    gfx_SetPalette(imgpalette,sizeof imgpalette,0);
    gfx_SetPalette(mypalette,sizeof mypalette,0);
    gfx_SetTransparentColor(PICO_BLACK);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_SetTextBGColor(PICO_BLACK);
    gfx_SetTextTransparentColor(PICO_BLACK);'''
if setup_old in s:
    s = s.replace(setup_old, setup_new)
elif setup_new not in s:
    raise SystemExit('graphics setup block not found')

# Catch the exact bug class that caused the unreadable UI: direct graphx color
# indices outside the defined 0-15 UI palette.
bad = []
for m in re.finditer(r'gfx_Set(?:TextFG)?Color\((\d+)\)', s):
    if int(m.group(1)) > 15:
        bad.append(m.group(0))
if bad:
    raise SystemExit('high direct palette indices remain: ' + ', '.join(sorted(set(bad))))

path.write_text(s)
print('CELEDIT palette/text fixes applied')
