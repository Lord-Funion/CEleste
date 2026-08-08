#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace_once(path, old, new):
    p = ROOT / path
    text = p.read_text()
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"missing patch marker in {path}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1))

# emu.cpp: rotate custom tile graphics at draw time with graphx.
replace_once("src/emu.cpp",
'''            uint8_t tile = mget(x + cell_x, y + cell_y);
            // I don't think this is how the PICO-8 actually handles the layers argument but whatevs
            if(tile && fget(tile, layers)) {
                gfx_TransparentSprite_NoClip(atlas_tiles[tile], LCD_WIDTH / 2 + x * 8, y * 8);
            }
''',
'''            const int map_x = x + cell_x;
            const int map_y = y + cell_y;
            uint8_t tile = mget(map_x, map_y);
            uint8_t rotation = 0;
            if(custom_levels::active() && map_x >= 0 && map_y >= 0) {
                const uint8_t room_index = static_cast<uint8_t>((map_x / 16) + (map_y / 16) * 8);
                rotation = custom_levels::tile_rotation(room_index,
                    static_cast<uint8_t>(map_x % 16), static_cast<uint8_t>(map_y % 16));
            }
            // I don't think this is how the PICO-8 actually handles the layers argument but whatevs
            if(tile && fget(tile, layers)) {
                if(rotation) {
                    gfx_TempSprite(rotated_tile, 8, 8);
                    if(rotation == 1) gfx_RotateSpriteC(atlas_tiles[tile], rotated_tile);
                    else if(rotation == 2) gfx_RotateSpriteHalf(atlas_tiles[tile], rotated_tile);
                    else gfx_RotateSpriteCC(atlas_tiles[tile], rotated_tile);
                    gfx_TransparentSprite_NoClip(rotated_tile, LCD_WIDTH / 2 + x * 8, y * 8);
                } else {
                    gfx_TransparentSprite_NoClip(atlas_tiles[tile], LCD_WIDTH / 2 + x * 8, y * 8);
                }
            }
''')

replace_once("src/emu.cpp",
'''    gfx_TransparentSprite(temp, SCREEN_X(x), SCREEN_Y(y));
    profiler_end(spr);
}

void pal() {
''',
'''    gfx_TransparentSprite(temp, SCREEN_X(x), SCREEN_Y(y));
    profiler_end(spr);
}

void spr_rot(uint8_t n, int x, int y, uint8_t rotation, bool flip_x, bool flip_y) {
    rotation &= 0x03u;
    if(rotation == 0) {
        spr(n, x, y, 1, 1, flip_x, flip_y);
        return;
    }
    profiler_add(spr);
    gfx_sprite_t *source = atlas_tiles[n];
    gfx_TempSprite(temp_a, 8, 8);
    gfx_TempSprite(temp_b, 8, 8);
    if(rotation == 1) gfx_RotateSpriteC(source, temp_a);
    else if(rotation == 2) gfx_RotateSpriteHalf(source, temp_a);
    else gfx_RotateSpriteCC(source, temp_a);
    gfx_sprite_t *current = temp_a;
    if(flip_x) {
        gfx_FlipSpriteX(current, temp_b);
        current = temp_b;
    }
    if(flip_y) {
        gfx_sprite_t *target = current == temp_a ? temp_b : temp_a;
        gfx_FlipSpriteY(current, target);
        current = target;
    }
    if(!default_pal) {
        for(uint8_t i = 0; i < 64; i++) current->data[i] = pal_map[current->data[i] & 0xF];
    }
    gfx_TransparentSprite(current, SCREEN_X(x), SCREEN_Y(y));
    profiler_end(spr);
}

void pal() {
''')

# Object state holds rotation independently from gameplay option flags.
replace_once("src/classic.h",
'''    uint8_t type;
    uint8_t custom_flags;
    uint8_t custom_source;
''',
'''    uint8_t type;
    uint8_t custom_flags;
    uint8_t custom_rotation;
    uint8_t custom_source;
''')

# Helpers used by every custom object renderer.
replace_once("src/classic.cpp",
'''void Object::draw() {
    spr(sprite, x, y, 1, 1, flip.x, flip.y);
}
''',
'''static void rotate_child_offset(int dx, int dy, uint8_t rotation, int &rx, int &ry) {
    switch(rotation & 0x03u) {
        case 1: rx = -dy; ry = dx; break;
        case 2: rx = -dx; ry = -dy; break;
        case 3: rx = dy; ry = -dx; break;
        default: rx = dx; ry = dy; break;
    }
}

static void spr_child_rot(uint8_t sprite, int x, int y, int dx, int dy,
                          uint8_t rotation, bool flip_x = false, bool flip_y = false) {
    int rx, ry;
    rotate_child_offset(dx, dy, rotation, rx, ry);
    spr_rot(sprite, x + rx, y + ry, rotation, flip_x, flip_y);
}

static void draw_rotated_2x2(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                             int x, int y, uint8_t rotation) {
    const uint8_t ids[4] = {a, b, c, d};
    for(uint8_t sy = 0; sy < 2; ++sy) for(uint8_t sx = 0; sx < 2; ++sx) {
        uint8_t dx = sx, dy = sy;
        switch(rotation & 0x03u) {
            case 1: dx = static_cast<uint8_t>(1 - sy); dy = sx; break;
            case 2: dx = static_cast<uint8_t>(1 - sx); dy = static_cast<uint8_t>(1 - sy); break;
            case 3: dx = sy; dy = static_cast<uint8_t>(1 - sx); break;
            default: break;
        }
        spr_rot(ids[sy * 2 + sx], x + dx * 8, y + dy * 8, rotation);
    }
}

void Object::draw() {
    spr_rot(sprite, x, y, custom_rotation, flip.x, flip.y);
}
''')

replace_once("src/classic.cpp",
'''void Balloon::draw() {
    if(sprite == 22) {
        spr(13 + sprite_tmr / 8, x, y + 6);
        spr(sprite, x, y);
    }
}
''',
'''void Balloon::draw() {
    if(sprite == 22) {
        spr_child_rot(13 + sprite_tmr / 8, x, y, 0, 6, custom_rotation);
        spr_rot(sprite, x, y, custom_rotation);
    }
}
''')

replace_once("src/classic.cpp",
'''void FallFloor::draw() {
    if(state != 2) {
        if(state != 1) {
            spr(23, x, y);
        } else {
            spr(23 + (15 - delay) / 5, x, y);
        }
    }
}
''',
'''void FallFloor::draw() {
    if(state != 2) {
        if(state != 1) {
            spr_rot(23, x, y, custom_rotation);
        } else {
            spr_rot(23 + (15 - delay) / 5, x, y, custom_rotation);
        }
    }
}
''')

replace_once("src/classic.cpp",
'''    spr(45 + off / 4, x - 6, y - 2, 1, 1, true, false);
    spr(sprite, x, y);
    spr(45 + off / 4, x + 6, y - 2);
''',
'''    spr_child_rot(45 + off / 4, x, y, -6, -2, custom_rotation, true, false);
    spr_rot(sprite, x, y, custom_rotation);
    spr_child_rot(45 + off / 4, x, y, 6, -2, custom_rotation);
''')

replace_once("src/classic.cpp",
'''void FakeWall::draw() {
    spr(64, x, y);
    spr(65, x + 8, y);
    spr(80, x, y + 8);
    spr(81, x + 8, y + 8);
}
''',
'''void FakeWall::draw() {
    draw_rotated_2x2(64, 65, 80, 81, x, y, custom_rotation);
}
''')

replace_once("src/classic.cpp",
'''void Platform::draw() {
    spr(11, x, y - 1);
    spr(12, x + 8, y - 1);
}
''',
'''void Platform::draw() {
    spr_child_rot(11, x, y - 1, 0, 0, custom_rotation);
    spr_child_rot(12, x, y - 1, 8, 0, custom_rotation);
}
''')

replace_once("src/classic.cpp",
'''    if(custom_levels::active()) {
        spr(70, x, y - 8);
        spr(71, x + 8, y - 8);
        spr(86, x, y);
        spr(87, x + 8, y);
    }
''',
'''    if(custom_levels::active()) {
        draw_rotated_2x2(70, 71, 86, 87, x, y - 8, custom_rotation);
    }
''')

replace_once("src/classic.cpp",
'''        spr(96, x, y);
        spr(97, x + 8, y);
''',
'''        spr_child_rot(96, x, y, 0, 0, custom_rotation);
        spr_child_rot(97, x, y, 8, 0, custom_rotation);
''')
replace_once("src/classic.cpp",
'''    spr(112, x, y + 8);
    spr(113, x + 8, y + 8);
}
''',
'''    spr_child_rot(112, x, y, 0, 8, custom_rotation);
    spr_child_rot(113, x, y, 8, 8, custom_rotation);
}
''')
replace_once("src/classic.cpp",
'''    spr(sprite, x, y);
    if(show) {
''',
'''    spr_rot(sprite, x, y, custom_rotation);
    if(show) {
''')

# Preserve high bits 6-7 for quarter-turns; expose only low bits to gameplay.
replace_once("src/classic.cpp",
'''Object *init_object(type type, int x, int y, uint8_t flags, uint8_t source) {
    const bool custom = custom_levels::active();
    const bool source_done = custom
''',
'''Object *init_object(type type, int x, int y, uint8_t flags, uint8_t source) {
    const bool custom = custom_levels::active();
    const uint8_t gameplay_flags = static_cast<uint8_t>(flags & clevel::ENTITY_FLAG_MASK);
    const uint8_t rotation = static_cast<uint8_t>((flags & clevel::ENTITY_ROTATION_MASK) >> clevel::ENTITY_ROTATION_SHIFT);
    const bool source_done = custom
''')
replace_once("src/classic.cpp", "object = (custom && (flags & 0x01u)) || !source_done ? new FakeWall(x, y) : nullptr;",
             "object = (custom && (gameplay_flags & 0x01u)) || !source_done ? new FakeWall(x, y) : nullptr;")
replace_once("src/classic.cpp", "object = (custom && (flags & 0x01u)) || !source_done ? new Chest(x, y) : nullptr;",
             "object = (custom && (gameplay_flags & 0x01u)) || !source_done ? new Chest(x, y) : nullptr;")
replace_once("src/classic.cpp",
'''    if(object) { object->custom_flags = flags; object->custom_source = source; }
''',
'''    if(object) {
        object->custom_flags = gameplay_flags;
        object->custom_rotation = rotation;
        object->custom_source = source;
    }
''')
replace_once("src/classic.cpp",
'''    custom_flags = 0;
    custom_source = 0xFF;
''',
'''    custom_flags = 0;
    custom_rotation = 0;
    custom_source = 0xFF;
''')

# Directional spike collision follows graphical tile rotation.
replace_once("src/classic.cpp",
'''            uint8_t tile = tile_at(i, j);
            if(tile == 17 and ((y + h - 1) % 8 >= 6 or y + h == j * 8 + 8) and yspd >= 0) {
''',
'''            uint8_t tile = tile_at(i, j);
            if(custom_levels::active()) {
                const uint8_t rot = custom_levels::tile_rotation(custom_levels::room_index(), i, j);
                int dir = tile == 17 ? 0 : tile == 59 ? 1 : tile == 27 ? 2 : tile == 43 ? 3 : -1;
                if(dir >= 0) {
                    dir = (dir + rot) & 3;
                    tile = dir == 0 ? 17 : dir == 1 ? 59 : dir == 2 ? 27 : 43;
                }
            }
            if(tile == 17 and ((y + h - 1) % 8 >= 6 or y + h == j * 8 + 8) and yspd >= 0) {
''')

# Calculator editor: store quarter-turns, render with graphx, export CELV v2.
p = ROOT / "tools/calculator-editor/src/main.cpp"
t = p.read_text()
t = t.replace('struct EditRoom { uint8_t tiles[256]; uint8_t spawn_x,spawn_y,exit_x,exit_y; };',
'''struct EditRoom { uint8_t tiles[256]; uint8_t rotations[256]; uint8_t spawn_x,spawn_y,exit_x,exit_y; };''')
t = t.replace('struct Change { uint8_t room,index,before,after; } history[HISTORY_SIZE], redo_stack[HISTORY_SIZE];',
'''struct Change { uint8_t room,index,before,after,before_rotation,after_rotation; } history[HISTORY_SIZE], redo_stack[HISTORY_SIZE];''')
t = t.replace('uint8_t history_count=0,redo_count=0,room_index=0,cursor_x=2,cursor_y=13,palette_index=2;',
'''uint8_t history_count=0,redo_count=0,room_index=0,cursor_x=2,cursor_y=13,palette_index=2,placement_rotation=0;''')
old = '''void draw_sprite(uint8_t id,int x,int y){
  if(id<128&&atlas_tiles[id]) gfx_TransparentSprite(atlas_tiles[id],x,y);
  else {gfx_SetColor(tile_color(id));gfx_FillRectangle(x,y,8,8);}
}
void draw_piece(uint8_t id,int x,int y){'''
new = '''void draw_sprite(uint8_t id,int x,int y,uint8_t rotation=0){
  if(id<128&&atlas_tiles[id]) {
    rotation&=3;
    if(!rotation) gfx_TransparentSprite(atlas_tiles[id],x,y);
    else {gfx_TempSprite(tmp,8,8);if(rotation==1)gfx_RotateSpriteC(atlas_tiles[id],tmp);else if(rotation==2)gfx_RotateSpriteHalf(atlas_tiles[id],tmp);else gfx_RotateSpriteCC(atlas_tiles[id],tmp);gfx_TransparentSprite(tmp,x,y);}
  } else {gfx_SetColor(tile_color(id));gfx_FillRectangle(x,y,8,8);}
}
void rotate_offset(int dx,int dy,uint8_t rotation,int &rx,int &ry){switch(rotation&3){case 1:rx=-dy;ry=dx;break;case 2:rx=-dx;ry=-dy;break;case 3:rx=dy;ry=-dx;break;default:rx=dx;ry=dy;break;}}
void draw_child(uint8_t id,int x,int y,int dx,int dy,uint8_t rotation){int rx,ry;rotate_offset(dx,dy,rotation,rx,ry);draw_sprite(id,x+rx,y+ry,rotation);}
void draw_piece(uint8_t id,int x,int y,uint8_t rotation=0){'''
if old not in t: raise SystemExit('calculator draw marker missing')
t=t.replace(old,new,1)
t=t.replace('if(id==64){draw_sprite(64,x,y);draw_sprite(65,x+8,y);draw_sprite(80,x,y+8);draw_sprite(81,x+8,y+8);return;}',
'''if(id==64){draw_child(64,x,y,0,0,rotation);draw_child(65,x,y,8,0,rotation);draw_child(80,x,y,0,8,rotation);draw_child(81,x,y,8,8,rotation);return;}''')
t=t.replace('if(id==96){draw_sprite(96,x,y);draw_sprite(97,x+8,y);draw_sprite(112,x,y+8);draw_sprite(113,x+8,y+8);return;}',
'''if(id==96){draw_child(96,x,y,0,0,rotation);draw_child(97,x,y,8,0,rotation);draw_child(112,x,y,0,8,rotation);draw_child(113,x,y,8,8,rotation);return;}''')
t=t.replace('if(id==86){draw_sprite(70,x,y-8);draw_sprite(71,x+8,y-8);draw_sprite(86,x,y);draw_sprite(87,x+8,y);return;}',
'''if(id==86){draw_child(70,x,y,0,-8,rotation);draw_child(71,x,y,8,-8,rotation);draw_child(86,x,y,0,0,rotation);draw_child(87,x,y,8,0,rotation);return;}''')
t=t.replace('if(id==11||id==12){draw_sprite(11,x-4,y-1);draw_sprite(12,x+4,y-1);return;}',
'''if(id==11||id==12){draw_child(11,x-4,y-1,0,0,rotation);draw_child(12,x-4,y-1,8,0,rotation);return;}''')
t=t.replace('if(id==28){draw_sprite(45,x-6,y-2);draw_sprite(28,x,y);draw_sprite(45,x+6,y-2);return;}',
'''if(id==28){draw_child(45,x,y,-6,-2,rotation);draw_sprite(28,x,y,rotation);draw_child(45,x,y,6,-2,rotation);return;}''')
t=t.replace('if(id==22){draw_sprite(13,x,y+6);draw_sprite(22,x,y);return;}',
'''if(id==22){draw_child(13,x,y,0,6,rotation);draw_sprite(22,x,y,rotation);return;}''')
t=t.replace('  draw_sprite(id,x,y);\n}', '  draw_sprite(id,x,y,rotation);\n}',1)
# Delete old atlas-counterpart rotation function entirely.
start=t.index('uint8_t rotate_id(uint8_t id){')
end=t.index('int palette_index_for',start)
t=t[:start]+t[end:]
t=t.replace('if(id)draw_piece(id,8+x*8,48+y*8);', 'if(id)draw_piece(id,8+x*8,48+y*8,r.rotations[y*16+x]);')
t=t.replace('draw_piece(PALETTE[palette_index],236,48);', 'draw_piece(PALETTE[palette_index],236,48,placement_rotation);')
t=t.replace('gfx_PrintStringXY("WINDOW: rotate",160,212);', 'gfx_PrintStringXY("WINDOW: rotate 90",160,212);')
t=t.replace('void record_change(uint8_t index,uint8_t before,uint8_t after){if(before==after)return;',
'''void record_change(uint8_t index,uint8_t before,uint8_t after,uint8_t before_rotation,uint8_t after_rotation){if(before==after&&before_rotation==after_rotation)return;''')
t=t.replace('history[history_count++]={room_index,index,before,after};', 'history[history_count++]={room_index,index,before,after,before_rotation,after_rotation};')
t=t.replace('void paint_cell(uint8_t x,uint8_t y,uint8_t value){if(x>=16||y>=16)return;EditRoom &r=project.rooms[room_index];const uint8_t index=y*16+x,before=r.tiles[index];record_change(index,before,value);r.tiles[index]=value;}',
'''void paint_cell(uint8_t x,uint8_t y,uint8_t value,uint8_t rotation=0){if(x>=16||y>=16)return;EditRoom &r=project.rooms[room_index];const uint8_t index=y*16+x,before=r.tiles[index],before_rotation=r.rotations[index];record_change(index,before,value,before_rotation,rotation&3);r.tiles[index]=value;r.rotations[index]=rotation&3;}''')
t=t.replace('void paint(uint8_t value){paint_cell(cursor_x,cursor_y,value);}', 'void paint(uint8_t value){paint_cell(cursor_x,cursor_y,value,placement_rotation);}')
t=t.replace('paint_cell(cursor_x+dx,cursor_y+dy,0);', 'paint_cell(cursor_x+dx,cursor_y+dy,0,0);')
t=t.replace('paint_cell(cursor_x+dx,cursor_y-1+dy,0);', 'paint_cell(cursor_x+dx,cursor_y-1+dy,0,0);')
t=t.replace('paint_cell(ax,ay,0);', 'paint_cell(ax,ay,0,0);')
t=t.replace('project.rooms[c.room].tiles[c.index]=c.before;', 'project.rooms[c.room].tiles[c.index]=c.before;project.rooms[c.room].rotations[c.index]=c.before_rotation;')
t=t.replace('project.rooms[c.room].tiles[c.index]=c.after;', 'project.rooms[c.room].tiles[c.index]=c.after;project.rooms[c.room].rotations[c.index]=c.after_rotation;')
t=t.replace('gfx_PrintStringXY("WINDOW rotates directions.",8,70);', 'gfx_PrintStringXY("WINDOW rotates ANY piece.",8,70);')
# v2 encoder: packed rotation plane and entity rotation in high flag bits.
t=t.replace('w.bytes("CELV",4);w.u8(1);w.u8(1);', 'w.bytes("CELV",4);w.u8(2);w.u8(1);')
t=t.replace('w.u16(0x0100);', 'w.u16(0x0101);')
t=t.replace('const std::size_t record_len=16+rle_len+entity_count*4;w.u16(record_len);w.u8(16);w.u8(16);w.u8(r.spawn_x);w.u8(r.spawn_y);w.u8(r.exit_x);w.u8(r.exit_y);w.u8(0);w.u8(0);w.u16(rle_len);w.u16(entity_count);w.u32(hash_id(project.title)+ri);w.bytes(rle,rle_len);',
'''uint8_t packed_rot[clevel::ROTATION_PLANE_BYTES]={0};for(uint16_t i=0;i<256;i++)if(!is_entity(r.tiles[i]))packed_rot[i>>2]|=(r.rotations[i]&3)<<((i&3)*2);const std::size_t record_len=16+rle_len+clevel::ROTATION_PLANE_BYTES+entity_count*4;w.u16(record_len);w.u8(16);w.u8(16);w.u8(r.spawn_x);w.u8(r.spawn_y);w.u8(r.exit_x);w.u8(r.exit_y);w.u8(0);w.u8(clevel::ROTATION_ENCODING_2BPP);w.u16(rle_len);w.u16(entity_count);w.u32(hash_id(project.title)+ri);w.bytes(rle,rle_len);w.bytes(packed_rot,sizeof packed_rot);''')
t=t.replace('for(uint16_t i=0;i<256;i++)if(is_entity(r.tiles[i])){w.u8(r.tiles[i]);w.u8(i%16);w.u8(i/16);w.u8(0);}',
'''for(uint16_t i=0;i<256;i++)if(is_entity(r.tiles[i])){w.u8(r.tiles[i]);w.u8(i%16);w.u8(i/16);w.u8(static_cast<uint8_t>((r.rotations[i]&3)<<clevel::ENTITY_ROTATION_SHIFT));}''')
# Rotation key no longer swaps IDs; it increments visual quarter-turns.
t=t.replace('uint8_t rotated=rotate_id(PALETTE[palette_index]);int pi=palette_index_for(rotated);if(pi>=0){palette_index=static_cast<uint8_t>(pi);set_notice("ROTATED");}else set_notice("NO ROTATION");',
'''placement_rotation=static_cast<uint8_t>((placement_rotation+1)&3);set_notice(placement_rotation==0?"ROTATION 0":placement_rotation==1?"ROTATION 90":placement_rotation==2?"ROTATION 180":"ROTATION 270");''')
p.write_text(t)

print('Applied arbitrary rotation v2 source patches.')
