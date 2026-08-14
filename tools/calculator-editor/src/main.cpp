#include <cstdint>
#include <cstring>
#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

#include "clevel_format.h"
#include "gfx/gfx.h"

namespace {

constexpr uint8_t MAX_ROOMS = 16;
constexpr uint8_t MAX_ENTITIES = 48;
constexpr uint8_t HISTORY_SIZE = 12;
constexpr std::size_t MAX_METADATA_SIZE = 63 + 31 + 127;
constexpr std::size_t MAX_ENCODED_LEVEL_SIZE = clevel::HEADER_SIZE + MAX_METADATA_SIZE +
    MAX_ROOMS * (16 + 512 + clevel::ROTATION_PLANE_BYTES + MAX_ENTITIES * 4);
constexpr uint8_t ROT_SHIFT = clevel::ENTITY_ROTATION_SHIFT;
constexpr uint8_t ROT_MASK = clevel::ENTITY_ROTATION_MASK;
constexpr uint8_t GAME_MASK = clevel::ENTITY_FLAG_MASK;

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

constexpr uint8_t PALETTE[] = {
    0,1,
    32,33,34,35,36,37,38,39,48,49,50,51,52,53,54,55,72,
    66,67,68,69,82,83,84,85,98,99,100,101,114,115,116,117,
    17,59,27,43,
    16,40,41,42,44,56,57,58,60,61,62,63,73,74,75,76,77,78,79,
    88,89,90,91,92,93,94,95,103,104,105,106,107,108,109,110,111,
    121,122,123,124,125,126,127,
    8,11,12,18,20,22,23,26,28,64,86,96,118,129,130,131
};

constexpr uint8_t ENTITY_IDS[] = {8,11,12,18,20,22,23,26,28,64,86,96,118,129,130,131};

constexpr uint8_t TILE_MASK[128] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  4,2,0,0,0,0,0,0,0,0,0,2,0,0,0,0,
  3,3,3,3,3,3,3,3,4,4,4,2,2,0,0,0,
  3,3,3,3,3,3,3,3,4,4,4,2,2,2,2,2,
  0,0,19,19,19,19,2,2,3,2,2,2,2,2,2,2,
  0,0,19,19,19,19,2,2,4,2,2,2,2,2,2,2,
  0,0,19,19,19,19,0,4,4,2,2,2,2,2,2,2,
  0,0,19,19,19,19,0,0,0,2,2,2,2,2,2,2
};

enum Tool : uint8_t { PENCIL, ERASER, FILL, PICKER };
enum View : uint8_t { EDITOR, PALETTE_VIEW, ROOMS_VIEW, ACTIONS_VIEW, PROPERTIES_VIEW, HELP_VIEW };
enum Category : uint8_t { ALL, TERRAIN, ICE, HAZARDS, GAMEPLAY, BACKGROUND, DECORATION, CATEGORY_COUNT };

struct EditEntity {
    uint8_t type, x, y, flags;
};

struct EditRoom {
    uint8_t tiles[256];
    uint8_t rotations[256];
    uint8_t spawn_x, spawn_y, exit_x, exit_y;
    uint8_t entity_count;
    EditEntity entities[MAX_ENTITIES];
};

struct Project {
    char title[64];
    char author[32];
    char description[128];
    uint8_t difficulty;
    uint8_t room_count;
    EditRoom rooms[MAX_ROOMS];
} project;

struct LegacyEditRoom {
    uint8_t tiles[256];
    uint8_t rotations[256];
    uint8_t spawn_x, spawn_y, exit_x, exit_y;
};

struct LegacyProject {
    char title[64];
    char author[32];
    uint8_t room_count;
    LegacyEditRoom rooms[8];
};

struct RoomSnapshot {
    uint8_t room;
    EditRoom state;
};

RoomSnapshot undo_stack[HISTORY_SIZE], redo_stack[HISTORY_SIZE];
uint8_t undo_count = 0, redo_count = 0;

uint8_t room_index = 0;
uint8_t cursor_x = 2, cursor_y = 13;
uint8_t selected_id = 37;
uint8_t placement_rotation = 0;
uint8_t placement_flags = 0;
Tool tool = PENCIL;
View view = EDITOR;
Category palette_category = ALL;
uint8_t palette_cursor = 0;
uint8_t rooms_cursor = 0;
uint8_t action_cursor = 0;
bool new_project_armed = false;
bool delete_room_armed = false;
bool dirty = false;

enum PropertyTarget : uint8_t { PROP_PLACEMENT, PROP_TILE, PROP_ENTITY };
PropertyTarget property_target = PROP_PLACEMENT;
int property_entity = -1;
uint8_t property_x = 0, property_y = 0;

char notice[56] = "ZOOM opens palette";

bool is_entity(uint8_t id) {
    for(uint8_t v : ENTITY_IDS) if(v == id) return true;
    return false;
}

bool tile_flag(uint8_t id, uint8_t flag) {
    return id < 128 && (TILE_MASK[id] & (1u << flag)) != 0;
}

uint8_t rotation_from_flags(uint8_t flags) {
    return static_cast<uint8_t>((flags & ROT_MASK) >> ROT_SHIFT);
}

uint8_t with_rotation(uint8_t flags, uint8_t rotation) {
    return static_cast<uint8_t>((flags & GAME_MASK) | ((rotation & 3u) << ROT_SHIFT));
}

uint8_t gameplay_flags(uint8_t flags) {
    return static_cast<uint8_t>(flags & GAME_MASK);
}

Category category_of(uint8_t id) {
    if(id == 0 || id == 1 || is_entity(id)) return GAMEPLAY;
    if(id == 17 || id == 27 || id == 43 || id == 59) return HAZARDS;
    if(tile_flag(id, 4)) return ICE;
    if(tile_flag(id, 0)) return TERRAIN;
    if(tile_flag(id, 2)) return BACKGROUND;
    return DECORATION;
}

const char *category_name(Category c) {
    switch(c) {
        case ALL: return "ALL";
        case TERRAIN: return "TERRAIN";
        case ICE: return "ICE";
        case HAZARDS: return "HAZARDS";
        case GAMEPLAY: return "GAMEPLAY";
        case BACKGROUND: return "BACKGROUND";
        case DECORATION: return "DECOR";
        default: return "";
    }
}

const char *logical_name(uint8_t id) {
    switch(id) {
        case 0: return "Empty";
        case 1: return "Player spawn";
        case 8: return "Key";
        case 11: return "Platform left";
        case 12: return "Platform right";
        case 18: return "Spring";
        case 20: return "Locked chest";
        case 22: return "Dash balloon";
        case 23: return "Falling floor";
        case 26: return "Strawberry";
        case 28: return "Flying berry";
        case 64: return "Fake wall";
        case 86: return "Memorial";
        case 96: return "Dash chest";
        case 118: return "Summit flag";
        case 129: return "Climb chest";
        case 130: return "Silver key";
        case 131: return "Silver gate";
        case 17: return "Spikes up";
        case 59: return "Spikes right";
        case 27: return "Spikes down";
        case 43: return "Spikes left";
        default: return nullptr;
    }
}

const char *tool_name(Tool t) {
    switch(t) {
        case PENCIL: return "Pencil";
        case ERASER: return "Eraser";
        case FILL: return "Fill";
        case PICKER: return "Picker";
        default: return "";
    }
}

void set_notice(const char *text) {
    std::strncpy(notice, text ? text : "", sizeof notice - 1);
    notice[sizeof notice - 1] = '\0';
}

void init_room(EditRoom &r) {
    std::memset(&r, 0, sizeof r);
    r.spawn_x = 2;
    r.spawn_y = 13;
    r.exit_x = 13;
    r.exit_y = 1;
    for(uint8_t x = 0; x < 16; ++x) r.tiles[15 * 16 + x] = 37;
    for(uint8_t y = 0; y < 16; ++y) {
        r.tiles[y * 16] = 37;
        r.tiles[y * 16 + 15] = 37;
    }
}

void new_project() {
    std::memset(&project, 0, sizeof project);
    std::strcpy(project.title, "CALCULATOR LEVEL");
    std::strcpy(project.author, "LORD FUNION");
    project.difficulty = 2;
    project.room_count = 1;
    init_room(project.rooms[0]);
    room_index = rooms_cursor = 0;
    undo_count = redo_count = 0;
    selected_id = 37;
    placement_rotation = placement_flags = 0;
    tool = PENCIL;
    delete_room_armed = false;
    dirty = true;
}

void add_entity(EditRoom &r, uint8_t type, uint8_t x, uint8_t y, uint8_t flags) {
    if(r.entity_count >= MAX_ENTITIES) return;
    r.entities[r.entity_count++] = {type, x, y, flags};
}

uint8_t migrate_legacy_tile(uint8_t id) {
    if(id == 2) return 32;
    if(id == 3) return 33;
    if(id == 4) return 66;
    if(id == 5) return 67;
    return id;
}

void migrate_legacy(const LegacyProject &old) {
    new_project();
    std::strncpy(project.title, old.title, sizeof project.title - 1);
    std::strncpy(project.author, old.author, sizeof project.author - 1);
    project.room_count = old.room_count > MAX_ROOMS ? MAX_ROOMS : old.room_count;
    if(!project.room_count) project.room_count = 1;
    for(uint8_t ri = 0; ri < project.room_count; ++ri) {
        EditRoom &dst = project.rooms[ri];
        std::memset(&dst, 0, sizeof dst);
        dst.spawn_x = old.rooms[ri].spawn_x;
        dst.spawn_y = old.rooms[ri].spawn_y;
        dst.exit_x = old.rooms[ri].exit_x;
        dst.exit_y = old.rooms[ri].exit_y;
        for(uint16_t i = 0; i < 256; ++i) {
            const uint8_t id = migrate_legacy_tile(old.rooms[ri].tiles[i]);
            if(id == 1) {
                dst.spawn_x = i % 16;
                dst.spawn_y = i / 16;
            } else if(is_entity(id)) {
                add_entity(dst, id, i % 16, i / 16,
                    static_cast<uint8_t>((old.rooms[ri].rotations[i] & 3u) << ROT_SHIFT));
            } else {
                dst.tiles[i] = id;
                dst.rotations[i] = old.rooms[ri].rotations[i] & 3u;
            }
        }
    }
    set_notice("Old CELEDIT draft upgraded");
}

void save_draft() {
    uint8_t h = ti_Open("CELEDITS", "w");
    if(!h) {
        set_notice("Draft save failed");
        return;
    }
    const bool ok = ti_Write(&project, sizeof project, 1, h) == 1;
    ti_Close(h);
    if(!ok) {
        set_notice("Draft save failed");
        return;
    }
    dirty = false;
    set_notice("Draft saved");
}

void load_draft() {
    uint8_t h = ti_Open("CELEDITS", "r");
    if(!h) {
        new_project();
        return;
    }
    const uint24_t size = ti_GetSize(h);
    if(size == sizeof project && ti_Read(&project, sizeof project, 1, h) == 1 &&
       project.room_count > 0 && project.room_count <= MAX_ROOMS) {
        ti_Close(h);
        dirty = false;
        return;
    }
    if(size == sizeof(LegacyProject)) {
        LegacyProject old{};
        const bool ok = ti_Read(&old, sizeof old, 1, h) == 1;
        ti_Close(h);
        if(ok && old.room_count > 0 && old.room_count <= 8) {
            migrate_legacy(old);
            return;
        }
    } else {
        ti_Close(h);
    }
    new_project();
    set_notice("Draft reset: incompatible save");
}

void push_undo() {
    dirty = true;
    if(undo_count == HISTORY_SIZE) {
        std::memmove(undo_stack, undo_stack + 1, sizeof(RoomSnapshot) * (HISTORY_SIZE - 1));
        --undo_count;
    }
    undo_stack[undo_count].room = room_index;
    undo_stack[undo_count].state = project.rooms[room_index];
    ++undo_count;
    redo_count = 0;
}

void undo() {
    if(!undo_count) {
        set_notice("Nothing to undo");
        return;
    }
    if(redo_count == HISTORY_SIZE) {
        std::memmove(redo_stack, redo_stack + 1, sizeof(RoomSnapshot) * (HISTORY_SIZE - 1));
        --redo_count;
    }
    redo_stack[redo_count].room = room_index;
    redo_stack[redo_count].state = project.rooms[room_index];
    ++redo_count;
    const RoomSnapshot snap = undo_stack[--undo_count];
    room_index = snap.room;
    project.rooms[room_index] = snap.state;
    rooms_cursor = room_index;
    dirty = true;
    set_notice("Undo");
}

void redo() {
    if(!redo_count) {
        set_notice("Nothing to redo");
        return;
    }
    if(undo_count == HISTORY_SIZE) {
        std::memmove(undo_stack, undo_stack + 1, sizeof(RoomSnapshot) * (HISTORY_SIZE - 1));
        --undo_count;
    }
    undo_stack[undo_count].room = room_index;
    undo_stack[undo_count].state = project.rooms[room_index];
    ++undo_count;
    const RoomSnapshot snap = redo_stack[--redo_count];
    room_index = snap.room;
    project.rooms[room_index] = snap.state;
    rooms_cursor = room_index;
    dirty = true;
    set_notice("Redo");
}

uint8_t tile_color(uint8_t id) {
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

void draw_sprite(uint8_t id, int x, int y, uint8_t rotation = 0) {
    if(id < 128 && atlas_tiles[id]) {
        rotation &= 3;
        if(!rotation) {
            gfx_TransparentSprite(atlas_tiles[id], x, y);
        } else {
            gfx_TempSprite(tmp, 8, 8);
            if(rotation == 1) gfx_RotateSpriteC(atlas_tiles[id], tmp);
            else if(rotation == 2) gfx_RotateSpriteHalf(atlas_tiles[id], tmp);
            else gfx_RotateSpriteCC(atlas_tiles[id], tmp);
            gfx_TransparentSprite(tmp, x, y);
        }
    } else {
        gfx_SetColor(tile_color(id));
        gfx_FillRectangle(x, y, 8, 8);
    }
}

void draw_silver_key_texture(int x, int y, uint8_t rotation = 0) {
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

void rotate_offset(int dx, int dy, uint8_t rotation, int &rx, int &ry) {
    switch(rotation & 3) {
        case 1: rx = -dy; ry = dx; break;
        case 2: rx = -dx; ry = -dy; break;
        case 3: rx = dy; ry = -dx; break;
        default: rx = dx; ry = dy; break;
    }
}

void draw_child(uint8_t id, int x, int y, int dx, int dy, uint8_t rotation) {
    int rx, ry;
    rotate_offset(dx, dy, rotation, rx, ry);
    draw_sprite(id, x + rx, y + ry, rotation);
}

void draw_2x2(uint8_t a, uint8_t b, uint8_t c, uint8_t d, int x, int y, uint8_t rotation) {
    const uint8_t ids[4] = {a,b,c,d};
    for(uint8_t sy = 0; sy < 2; ++sy) for(uint8_t sx = 0; sx < 2; ++sx) {
        uint8_t dx = sx, dy = sy;
        switch(rotation & 3) {
            case 1: dx = static_cast<uint8_t>(1 - sy); dy = sx; break;
            case 2: dx = static_cast<uint8_t>(1 - sx); dy = static_cast<uint8_t>(1 - sy); break;
            case 3: dx = sy; dy = static_cast<uint8_t>(1 - sx); break;
            default: break;
        }
        draw_sprite(ids[sy * 2 + sx], x + dx * 8, y + dy * 8, rotation);
    }
}

void draw_piece(uint8_t id, int x, int y, uint8_t rotation = 0) {
    if(id == 130) {
        draw_silver_key_texture(x, y, rotation);
        return;
    }
    if(id == 131) {
        gfx_SetColor(5); gfx_FillRectangle(x, y, 8, 8);
        gfx_SetColor(6);
        if(rotation & 1) {
            gfx_FillRectangle(x, y + 1, 8, 2); gfx_FillRectangle(x, y + 5, 8, 2);
        } else {
            gfx_FillRectangle(x + 1, y, 2, 8); gfx_FillRectangle(x + 5, y, 2, 8);
        }
        gfx_SetColor(7); gfx_FillRectangle(x + 3, y + 3, 2, 2);
        return;
    }
    if(id == 129) {
        draw_sprite(20, x, y, rotation);
        gfx_SetColor(11);
        gfx_SetPixel(x + 4, y + 3);
        gfx_SetPixel(x + 3, y + 4);
        gfx_SetPixel(x + 4, y + 4);
        gfx_SetPixel(x + 5, y + 4);
        gfx_SetPixel(x + 4, y + 5);
        return;
    }
    if(id == 64) {
        draw_2x2(64,65,80,81,x,y,rotation);
        return;
    }
    if(id == 96) {
        draw_2x2(96,97,112,113,x,y,rotation);
        return;
    }
    if(id == 86) {
        draw_2x2(70,71,86,87,x,y-8,rotation);
        return;
    }
    if(id == 11 || id == 12) {
        draw_child(11,x-4,y-1,0,0,rotation);
        draw_child(12,x-4,y-1,8,0,rotation);
        return;
    }
    if(id == 28) {
        draw_child(45,x,y,-6,-2,rotation);
        draw_sprite(28,x,y,rotation);
        draw_child(45,x,y,6,-2,rotation);
        return;
    }
    if(id == 22) {
        draw_child(13,x,y,0,6,rotation);
        draw_sprite(22,x,y,rotation);
        return;
    }
    draw_sprite(id,x,y,rotation);
}

uint8_t entity_footprint(const EditEntity &e, int8_t *xs, int8_t *ys) {
    const uint8_t rot = rotation_from_flags(e.flags);
    if(e.type == 64 || e.type == 96) {
        xs[0]=e.x; ys[0]=e.y;
        xs[1]=e.x+1; ys[1]=e.y;
        xs[2]=e.x; ys[2]=e.y+1;
        xs[3]=e.x+1; ys[3]=e.y+1;
        return 4;
    }
    if(e.type == 86) {
        xs[0]=e.x; ys[0]=e.y-1;
        xs[1]=e.x+1; ys[1]=e.y-1;
        xs[2]=e.x; ys[2]=e.y;
        xs[3]=e.x+1; ys[3]=e.y;
        return 4;
    }
    if(e.type == 11 || e.type == 12) {
        xs[0]=e.x; ys[0]=e.y;
        if(rot & 1) { xs[1]=e.x; ys[1]=e.y+1; }
        else { xs[1]=e.x+1; ys[1]=e.y; }
        return 2;
    }
    xs[0]=e.x; ys[0]=e.y;
    return 1;
}

bool footprint_in_bounds(const EditEntity &e) {
    int8_t xs[4], ys[4];
    const uint8_t n = entity_footprint(e,xs,ys);
    for(uint8_t i=0;i<n;++i) if(xs[i] < 0 || xs[i] >= 16 || ys[i] < 0 || ys[i] >= 16) return false;
    return true;
}

bool entity_contains(const EditEntity &e, uint8_t x, uint8_t y) {
    int8_t xs[4], ys[4];
    const uint8_t n = entity_footprint(e,xs,ys);
    for(uint8_t i=0;i<n;++i) if(xs[i] == x && ys[i] == y) return true;
    return false;
}

int entity_at(const EditRoom &r, uint8_t x, uint8_t y) {
    for(uint8_t i=0;i<r.entity_count;++i) if(entity_contains(r.entities[i],x,y)) return i;
    return -1;
}

bool entities_overlap(const EditEntity &a, const EditEntity &b) {
    int8_t ax[4], ay[4], bx[4], by[4];
    const uint8_t an=entity_footprint(a,ax,ay), bn=entity_footprint(b,bx,by);
    for(uint8_t i=0;i<an;++i) for(uint8_t j=0;j<bn;++j)
        if(ax[i]==bx[j] && ay[i]==by[j]) return true;
    return false;
}

void remove_entity(EditRoom &r, uint8_t index) {
    if(index >= r.entity_count) return;
    for(uint8_t i=index;i+1<r.entity_count;++i) r.entities[i]=r.entities[i+1];
    --r.entity_count;
}

void clear_terrain_under(EditRoom &r, const EditEntity &e) {
    int8_t xs[4], ys[4];
    const uint8_t n=entity_footprint(e,xs,ys);
    for(uint8_t i=0;i<n;++i) {
        if(xs[i]>=0&&xs[i]<16&&ys[i]>=0&&ys[i]<16) {
            const uint16_t p=static_cast<uint16_t>(ys[i])*16+xs[i];
            r.tiles[p]=0;
            r.rotations[p]=0;
        }
    }
}

bool place_entity(uint8_t id) {
    EditRoom &r=project.rooms[room_index];
    EditEntity next{id,cursor_x,cursor_y,with_rotation(placement_flags,placement_rotation)};
    if(!footprint_in_bounds(next)) {
        set_notice("Piece does not fit here");
        return false;
    }
    if(r.entity_count >= MAX_ENTITIES) {
        set_notice("48 entity room limit");
        return false;
    }
    for(uint8_t i=0;i<r.entity_count;) {
        if(entities_overlap(r.entities[i],next)) remove_entity(r,i);
        else ++i;
    }
    clear_terrain_under(r,next);
    r.entities[r.entity_count++]=next;
    return true;
}

bool erase_at(uint8_t x,uint8_t y) {
    EditRoom &r=project.rooms[room_index];
    const int ei=entity_at(r,x,y);
    if(ei>=0) {
        remove_entity(r,static_cast<uint8_t>(ei));
        return true;
    }
    const uint16_t p=y*16+x;
    if(r.tiles[p]) {
        r.tiles[p]=0;
        r.rotations[p]=0;
        return true;
    }
    if(r.spawn_x==x&&r.spawn_y==y) {
        set_notice("Spawn cannot be erased");
    }
    return false;
}

bool pick_at(uint8_t x,uint8_t y) {
    EditRoom &r=project.rooms[room_index];
    const int ei=entity_at(r,x,y);
    if(ei>=0) {
        const EditEntity &e=r.entities[ei];
        selected_id=e.type;
        placement_flags=gameplay_flags(e.flags);
        placement_rotation=rotation_from_flags(e.flags);
        tool=PENCIL;
        set_notice("Picked entity + properties");
        return true;
    }
    const uint16_t p=y*16+x;
    if(r.tiles[p]) {
        selected_id=r.tiles[p];
        placement_rotation=r.rotations[p]&3;
        placement_flags=0;
        tool=PENCIL;
        set_notice("Picked tile + rotation");
        return true;
    }
    selected_id=0;
    placement_rotation=placement_flags=0;
    tool=PENCIL;
    set_notice("Picked Empty");
    return true;
}

bool flood_fill(uint8_t x,uint8_t y) {
    if(is_entity(selected_id) || selected_id==1) {
        set_notice("Fill works on terrain");
        return false;
    }
    EditRoom &r=project.rooms[room_index];
    const uint16_t start=y*16+x;
    const uint8_t from=r.tiles[start], from_rot=r.rotations[start]&3;
    if(from==selected_id && from_rot==placement_rotation) return false;
    uint16_t queue[256];
    uint8_t seen[256]{};
    uint16_t head=0,tail=0;
    queue[tail++]=start;
    seen[start]=1;
    while(head<tail) {
        const uint16_t p=queue[head++];
        r.tiles[p]=selected_id;
        r.rotations[p]=placement_rotation;
        const uint8_t px=p%16,py=p/16;
        const int nx[4]={int(px)-1,int(px)+1,int(px),int(px)};
        const int ny[4]={int(py),int(py),int(py)-1,int(py)+1};
        for(uint8_t i=0;i<4;++i) if(nx[i]>=0&&nx[i]<16&&ny[i]>=0&&ny[i]<16) {
            const uint16_t q=ny[i]*16+nx[i];
            if(!seen[q] && r.tiles[q]==from && (r.rotations[q]&3)==from_rot && entity_at(r,nx[i],ny[i])<0) {
                seen[q]=1;
                queue[tail++]=q;
            }
        }
    }
    return true;
}

bool apply_tool() {
    EditRoom &r=project.rooms[room_index];
    if(tool==PICKER) return pick_at(cursor_x,cursor_y);
    if(tool==ERASER) {
        push_undo();
        if(!erase_at(cursor_x,cursor_y)) --undo_count;
        else dirty = true;
        return true;
    }
    if(tool==FILL) {
        push_undo();
        if(!flood_fill(cursor_x,cursor_y)) --undo_count;
        else dirty = true;
        return true;
    }
    push_undo();
    if(selected_id==0) {
        if(!erase_at(cursor_x,cursor_y)) --undo_count;
        else dirty = true;
        return true;
    }
    if(selected_id==1) {
        const int ei=entity_at(r,cursor_x,cursor_y);
        if(ei>=0) remove_entity(r,static_cast<uint8_t>(ei));
        const uint16_t p=cursor_y*16+cursor_x;
        r.tiles[p]=0;r.rotations[p]=0;
        r.spawn_x=cursor_x;r.spawn_y=cursor_y;
        dirty = true;
        set_notice("Spawn moved");
        return true;
    }
    if(is_entity(selected_id)) {
        if(!place_entity(selected_id)) --undo_count;
        else dirty = true;
        return true;
    }
    const uint16_t p=cursor_y*16+cursor_x;
    const int ei=entity_at(r,cursor_x,cursor_y);
    if(ei>=0) remove_entity(r,static_cast<uint8_t>(ei));
    r.tiles[p]=selected_id;
    r.rotations[p]=placement_rotation;
    dirty = true;
    return true;
}

void cycle_tool() {
    tool=static_cast<Tool>((static_cast<uint8_t>(tool)+1)%4);
    set_notice(tool_name(tool));
}

void rotate_placement(bool clockwise=true) {
    placement_rotation=static_cast<uint8_t>((placement_rotation+(clockwise?1:3))&3);
    set_notice(placement_rotation==0?"Rotation 0":placement_rotation==1?"Rotation 90":placement_rotation==2?"Rotation 180":"Rotation 270");
}

void open_properties() {
    EditRoom &r=project.rooms[room_index];
    const int ei=entity_at(r,cursor_x,cursor_y);
    if(ei>=0) {
        property_target=PROP_ENTITY;
        property_entity=ei;
        property_x=cursor_x;property_y=cursor_y;
    } else {
        const uint16_t p=cursor_y*16+cursor_x;
        if(r.tiles[p]) {
            property_target=PROP_TILE;
            property_entity=-1;
            property_x=cursor_x;property_y=cursor_y;
        } else {
            property_target=PROP_PLACEMENT;
            property_entity=-1;
        }
    }
    view=PROPERTIES_VIEW;
}

uint8_t property_id() {
    if(property_target==PROP_ENTITY && property_entity>=0)
        return project.rooms[room_index].entities[property_entity].type;
    if(property_target==PROP_TILE)
        return project.rooms[room_index].tiles[property_y*16+property_x];
    return selected_id;
}

uint8_t property_rotation() {
    if(property_target==PROP_ENTITY && property_entity>=0)
        return rotation_from_flags(project.rooms[room_index].entities[property_entity].flags);
    if(property_target==PROP_TILE)
        return project.rooms[room_index].rotations[property_y*16+property_x]&3;
    return placement_rotation;
}

uint8_t property_game_flags() {
    if(property_target==PROP_ENTITY && property_entity>=0)
        return gameplay_flags(project.rooms[room_index].entities[property_entity].flags);
    return placement_flags;
}

void set_property_rotation(uint8_t rot) {
    rot&=3;
    if(property_target==PROP_ENTITY && property_entity>=0) {
        EditRoom &r=project.rooms[room_index];
        EditEntity before=r.entities[property_entity];
        EditEntity after=before;
        after.flags=with_rotation(before.flags,rot);
        if(!footprint_in_bounds(after)) {set_notice("Rotation would leave room");return;}
        for(uint8_t i=0;i<r.entity_count;++i)
            if(i!=static_cast<uint8_t>(property_entity)&&entities_overlap(r.entities[i],after)) {
                set_notice("Rotation would overlap piece");return;
            }
        push_undo();
        r.entities[property_entity]=after;
        clear_terrain_under(r,after);
        dirty = true;
    } else if(property_target==PROP_TILE) {
        push_undo();
        project.rooms[room_index].rotations[property_y*16+property_x]=rot;
        dirty = true;
    } else {
        placement_rotation=rot;
    }
}

void toggle_property_option() {
    const uint8_t id=property_id();
    if(id!=20&&id!=64&&id!=96) {
        set_notice("No gameplay option for this piece");
        return;
    }
    if(property_target==PROP_TILE) return;
    if(property_target==PROP_ENTITY&&property_entity>=0) {
        push_undo();
        EditEntity &e=project.rooms[room_index].entities[property_entity];
        uint8_t g=gameplay_flags(e.flags);
        if(id==20||id==64) g^=1;
        else g^=2;
        e.flags=with_rotation(g,rotation_from_flags(e.flags));
        dirty = true;
    } else {
        if(id==20||id==64) placement_flags^=1;
        else placement_flags^=2;
    }
}

void adjust_property_link(int delta) {
    const uint8_t id=property_id();
    if(id!=130&&id!=131) {
        set_notice("Only silver keys/gates have links");
        return;
    }
    if(property_target==PROP_TILE) return;
    if(property_target==PROP_ENTITY&&property_entity>=0) {
        push_undo();
        EditEntity &e=project.rooms[room_index].entities[property_entity];
        const uint8_t link=static_cast<uint8_t>((int(gameplay_flags(e.flags))+delta+64)&63);
        e.flags=with_rotation(link,rotation_from_flags(e.flags));
        dirty = true;
    } else {
        placement_flags=static_cast<uint8_t>((int(placement_flags)+delta+64)&63);
    }
    set_notice("Silver link changed");
}

void draw_panel(int x,int y,int w,int h,uint8_t fill=1,uint8_t border=13) {
    gfx_SetColor(fill);gfx_FillRectangle(x,y,w,h);
    gfx_SetColor(border);gfx_Rectangle(x,y,w,h);
}

void draw_softkey(int x,const char *top,const char *bottom,bool active=false) {
    gfx_SetColor(active?13:1);gfx_FillRectangle(x,181,60,30);
    gfx_SetColor(5);gfx_Rectangle(x,181,60,30);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY(top,x+4,185);
    gfx_SetTextFGColor(13);
    gfx_PrintStringXY(bottom,x+4,198);
}

void draw_selected_info() {
    draw_panel(144,42,168,132,1,5);
    const char *name=logical_name(selected_id);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("SELECTED",152,50);
    if(name) gfx_PrintStringXY(name,152,64);
    else gfx_PrintStringXY(category_name(category_of(selected_id)),152,64);
    gfx_SetTextFGColor(13);
    gfx_PrintStringXY("ID",152,78);gfx_SetTextXY(170,78);gfx_PrintUInt(selected_id,3);
    gfx_PrintStringXY("ROT",210,78);gfx_SetTextXY(236,78);gfx_PrintUInt(placement_rotation*90,3);
    if(selected_id) draw_piece(selected_id,154,94,placement_rotation);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("Tool",190,94);gfx_PrintStringXY(tool_name(tool),224,94);
    if(selected_id==20||selected_id==64) {
        gfx_PrintStringXY("Berry",190,108);
        gfx_PrintStringXY((placement_flags&1)?"NO":"YES",232,108);
    } else if(selected_id==96) {
        gfx_PrintStringXY("Dashes",190,108);
        gfx_PrintStringXY((placement_flags&2)?"3":"2",238,108);
    } else if(selected_id==130||selected_id==131) {
        gfx_PrintStringXY("Link",190,108);
        gfx_SetTextXY(232,108); gfx_PrintUInt(placement_flags&63,2);
    }
    gfx_SetTextFGColor(13);
    gfx_PrintStringXY("STAT: properties",152,132);
    gfx_PrintStringXY("2ND: use tool",152,144);
    gfx_PrintStringXY("ALPHA: quick erase",152,156);
}

void draw_editor() {
    gfx_FillScreen(0);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("CELEDIT",8,5);
    if(dirty) { gfx_SetTextFGColor(PICO_YELLOW); gfx_PrintStringXY("*",54,5); }
    gfx_SetTextFGColor(13);
    gfx_PrintStringXY("Studio-style",62,5);
    gfx_SetTextFGColor(dirty ? PICO_YELLOW : PICO_GREEN);
    gfx_PrintStringXY(dirty ? "UNSAVED" : "SAVED", 250, 5);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY(project.title,8,18);
    gfx_SetTextXY(255,18);gfx_PrintString("R");gfx_PrintUInt(room_index+1,2);gfx_PrintChar('/');gfx_PrintUInt(project.room_count,2);

    draw_panel(6,40,132,132,0,5);
    const EditRoom &r=project.rooms[room_index];
    for(uint8_t y=0;y<16;++y) for(uint8_t x=0;x<16;++x) {
        const uint16_t p=y*16+x;
        if(r.tiles[p]) draw_sprite(r.tiles[p],8+x*8,42+y*8,r.rotations[p]);
    }
    for(uint8_t i=0;i<r.entity_count;++i) {
        const EditEntity &e=r.entities[i];
        draw_piece(e.type,8+e.x*8,42+e.y*8,rotation_from_flags(e.flags));
    }
    draw_piece(1,8+r.spawn_x*8,42+r.spawn_y*8);
    gfx_SetColor(tool==ERASER?PICO_RED:tool==FILL?PICO_YELLOW:tool==PICKER?PICO_BLUE:PICO_WHITE);
    gfx_Rectangle(8+cursor_x*8,42+cursor_y*8,8,8);

    draw_selected_info();
    draw_softkey(6,"Y=","Tool",false);
    draw_softkey(68,"WIN","Rotate",false);
    draw_softkey(130,"ZOOM","Palette",false);
    draw_softkey(192,"TRACE","Rooms",false);
    draw_softkey(254,"GRAPH","Menu",false);
    gfx_SetTextFGColor(PICO_YELLOW);
    gfx_PrintStringXY(notice,8,218);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("MODE save  DEL undo  ENTER redo",8,230);
    gfx_SwapDraw();
}

uint8_t build_filtered(uint8_t *out, Category c) {
    uint8_t n=0;
    for(uint8_t id : PALETTE)
        if(c==ALL||category_of(id)==c) out[n++]=id;
    return n;
}

void draw_palette() {
    gfx_FillScreen(0);
    uint8_t filtered[sizeof PALETTE];
    const uint8_t count=build_filtered(filtered,palette_category);
    if(!count) palette_cursor=0;
    else if(palette_cursor>=count) palette_cursor=count-1;
    const uint8_t page=palette_cursor/24;
    const uint8_t start=page*24;
    gfx_SetTextFGColor(PICO_WHITE);gfx_PrintStringXY("PALETTE",8,6);
    gfx_SetTextFGColor(13);gfx_PrintStringXY(category_name(palette_category),72,6);
    gfx_SetTextFGColor(PICO_WHITE);gfx_PrintStringXY("+/- category",210,6);
    for(uint8_t slot=0;slot<24;++slot) {
        const uint8_t idx=start+slot;
        if(idx>=count) break;
        const uint8_t col=slot%6,row=slot/6;
        const int x=4+col*52,y=26+row*44;
        const bool active=idx==palette_cursor;
        draw_panel(x,y,48,40,active?2:1,active?13:5);
        const uint8_t id=filtered[idx];
        draw_piece(id,x+5,y+5, id==selected_id?placement_rotation:0);
        gfx_SetTextFGColor(active?PICO_WHITE:PICO_LAVENDER);
        const char *name=logical_name(id);
        if(name && std::strlen(name)<=7) gfx_PrintStringXY(name,x+4,y+24);
        else {gfx_PrintStringXY("ID",x+4,y+24);gfx_SetTextXY(x+20,y+24);gfx_PrintUInt(id,3);}
    }
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("Arrows browse  2ND select  WIN rotate",8,210);
    gfx_PrintStringXY("MODE/ZOOM back",8,224);
    gfx_SwapDraw();
}

void draw_room_thumbnail(const EditRoom &r,int x,int y,bool active) {
    draw_panel(x,y,70,48,active?2:1,active?13:5);
    for(uint8_t ty=0;ty<16;++ty) for(uint8_t tx=0;tx<16;++tx) {
        const uint8_t id=r.tiles[ty*16+tx];
        if(id) {gfx_SetColor(tile_color(id));gfx_FillRectangle(x+3+tx*2,y+3+ty*2,2,2);}
    }
    for(uint8_t i=0;i<r.entity_count;++i) {
        gfx_SetColor(tile_color(r.entities[i].type));
        gfx_FillRectangle(x+3+r.entities[i].x*2,y+3+r.entities[i].y*2,2,2);
    }
    gfx_SetColor(PICO_RED);gfx_FillRectangle(x+3+r.spawn_x*2,y+3+r.spawn_y*2,2,2);
}

void draw_rooms() {
    gfx_FillScreen(0);
    if(rooms_cursor>=project.room_count) rooms_cursor=project.room_count-1;
    const uint8_t page=rooms_cursor/8,start=page*8;
    gfx_SetTextFGColor(PICO_WHITE);gfx_PrintStringXY("ROOMS",8,6);
    gfx_SetTextFGColor(13);gfx_SetTextXY(60,6);gfx_PrintUInt(project.room_count,2);gfx_PrintString(" total");
    for(uint8_t slot=0;slot<8;++slot) {
        const uint8_t idx=start+slot;
        if(idx>=project.room_count) break;
        const uint8_t col=slot%4,row=slot/4;
        const int x=5+col*78,y=30+row*82;
        draw_room_thumbnail(project.rooms[idx],x,y,idx==rooms_cursor);
        gfx_SetTextFGColor(idx==rooms_cursor?PICO_WHITE:PICO_LAVENDER);
        gfx_PrintStringXY("Room",x+4,y+52);gfx_SetTextXY(x+38,y+52);gfx_PrintUInt(idx+1,2);
    }
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("2ND open  ENTER add  ALPHA duplicate",8,198);
    if(delete_room_armed) {
        gfx_SetTextFGColor(PICO_RED);
        gfx_PrintStringXY("Press DEL again to delete this room",8,214);
    } else {
        gfx_PrintStringXY("DEL delete  +/- move  MODE/TRACE back",8,214);
    }
    gfx_SwapDraw();
}

const char *ACTIONS[]={"Undo","Redo","Project details","Save draft","Export level","New project","Help","Exit editor"};
constexpr uint8_t ACTION_COUNT=sizeof ACTIONS/sizeof ACTIONS[0];

void draw_actions() {
    gfx_FillScreen(0);
    gfx_SetTextFGColor(PICO_WHITE);gfx_PrintStringXY("MENU",8,7);
    for(uint8_t i=0;i<ACTION_COUNT;++i) {
        const int y=30+i*23;
        if(i==action_cursor){gfx_SetColor(2);gfx_FillRectangle(12,y-4,296,20);gfx_SetColor(13);gfx_Rectangle(12,y-4,296,20);}
        gfx_SetTextFGColor(i==action_cursor?PICO_WHITE:PICO_LAVENDER);
        gfx_PrintStringXY(ACTIONS[i],22,y);
    }
    if(new_project_armed) {
        gfx_SetTextFGColor(PICO_RED);gfx_PrintStringXY("Press 2ND again to confirm NEW PROJECT",12,218);
    } else {
        gfx_SetTextFGColor(PICO_WHITE);gfx_PrintStringXY("Up/Down  2ND choose  MODE/GRAPH back",12,218);
    }
    gfx_SwapDraw();
}

void draw_properties() {
    gfx_FillScreen(0);
    const uint8_t id=property_id(),rot=property_rotation(),flags=property_game_flags();
    gfx_SetTextFGColor(PICO_WHITE);gfx_PrintStringXY("PROPERTIES",8,7);
    gfx_SetTextFGColor(13);
    gfx_PrintStringXY(property_target==PROP_ENTITY?"Placed entity":property_target==PROP_TILE?"Placed tile":"Next placement",8,22);
    draw_panel(16,45,288,126,1,5);
    if(id) draw_piece(id,30,72,rot);
    gfx_SetTextFGColor(PICO_WHITE);
    const char *name=logical_name(id);
    if(name) gfx_PrintStringXY(name,80,55);
    else gfx_PrintStringXY(category_name(category_of(id)),80,55);
    gfx_SetTextFGColor(13);gfx_PrintStringXY("ID",80,72);gfx_SetTextXY(100,72);gfx_PrintUInt(id,3);
    gfx_PrintStringXY("Rotation",80,90);gfx_SetTextXY(145,90);gfx_PrintUInt(rot*90,3);gfx_PrintString(" deg");
    gfx_SetTextFGColor(PICO_WHITE);
    if(id==20||id==64) {
        gfx_PrintStringXY("Contains strawberry:",80,110);
        gfx_PrintStringXY((flags&1)?"NO":"YES",232,110);
        gfx_PrintStringXY("2ND toggles strawberry",80,128);
    } else if(id==96) {
        gfx_PrintStringXY("Dash upgrade:",80,110);
        gfx_PrintStringXY((flags&2)?"3 dashes":"2 dashes",184,110);
        gfx_PrintStringXY("2ND toggles 2 / 3",80,128);
    } else if(id==130||id==131) {
        gfx_PrintStringXY("Link group:",80,110);
        gfx_SetTextXY(170,110); gfx_PrintUInt(flags&63,2);
        gfx_PrintStringXY("+/- changes linked gate group",80,128);
    } else {
        gfx_PrintStringXY("No extra gameplay options",80,110);
    }
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("Left/Right or WINDOW rotate",16,188);
    gfx_PrintStringXY("MODE/STAT back",16,204);
    gfx_SwapDraw();
}

void draw_help() {
    gfx_FillScreen(0);
    gfx_SetTextFGColor(PICO_WHITE);gfx_PrintStringXY("CELEDIT HELP",8,7);
    gfx_SetTextFGColor(13);
    gfx_PrintStringXY("Main editor",8,26);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("Arrows move cursor. 2ND uses current tool.",8,40);
    gfx_PrintStringXY("ALPHA quick-erases. Y= cycles tools.",8,54);
    gfx_PrintStringXY("WINDOW rotates. ZOOM opens full palette.",8,68);
    gfx_PrintStringXY("TRACE opens room manager. STAT properties.",8,82);
    gfx_PrintStringXY("GRAPH opens undo/export/project menu.",8,96);
    gfx_PrintStringXY("+/- changes rooms. CLEAR saves + exits.",8,110);
    gfx_SetTextFGColor(13);gfx_PrintStringXY("Gameplay",8,132);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_PrintStringXY("Chest/fake wall berry options are editable.",8,146);
    gfx_PrintStringXY("Big chests can grant 2 or 3 dashes.",8,160);
    gfx_PrintStringXY("Silver keys/gates link by group 0-63.",8,174);
    gfx_PrintStringXY("All pieces keep real 0/90/180/270 rotation.",8,188);
    gfx_PrintStringXY("Rooms complete by climbing through the top.",8,202);
    gfx_PrintStringXY("MODE or Y= returns",8,216);
    gfx_SwapDraw();
}

void details() {
    gfx_End();
    os_ClrHomeFull();
    os_GetStringInput("LEVEL NAME",project.title,sizeof project.title);
    os_GetStringInput("AUTHOR",project.author,sizeof project.author);
    os_GetStringInput("DESCRIPTION",project.description,sizeof project.description);
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(imgpalette,sizeof imgpalette,0);
    gfx_SetPalette(mypalette,sizeof mypalette,0);
    gfx_SetTransparentColor(PICO_BLACK);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_SetTextBGColor(PICO_BLACK);
    gfx_SetTextTransparentColor(PICO_BLACK);
    dirty = true;
    set_notice("Project details updated");
}

void add_room() {
    if(project.room_count>=MAX_ROOMS){set_notice("16 room calculator limit");return;}
    init_room(project.rooms[project.room_count]);
    room_index=rooms_cursor=project.room_count++;
    delete_room_armed = false;
    dirty = true;
    set_notice("Room added");
}

void duplicate_room() {
    if(project.room_count>=MAX_ROOMS){set_notice("16 room calculator limit");return;}
    const uint8_t src=rooms_cursor;
    for(uint8_t i=project.room_count;i>src+1;--i) project.rooms[i]=project.rooms[i-1];
    project.rooms[src+1]=project.rooms[src];
    ++project.room_count;
    room_index=rooms_cursor=src+1;
    delete_room_armed = false;
    dirty = true;
    set_notice("Room duplicated");
}

void delete_room() {
    if(project.room_count==1){set_notice("A level needs one room");return;}
    if(!delete_room_armed) {
        delete_room_armed = true;
        set_notice("Press DEL again to delete room");
        return;
    }
    for(uint8_t i=rooms_cursor;i+1<project.room_count;++i) project.rooms[i]=project.rooms[i+1];
    --project.room_count;
    if(rooms_cursor>=project.room_count) rooms_cursor=project.room_count-1;
    room_index=rooms_cursor;
    delete_room_armed = false;
    dirty = true;
    set_notice("Room deleted");
}

void move_room(int direction) {
    const int next=int(rooms_cursor)+direction;
    if(next<0||next>=project.room_count)return;
    EditRoom tmp=project.rooms[rooms_cursor];
    project.rooms[rooms_cursor]=project.rooms[next];
    project.rooms[next]=tmp;
    rooms_cursor=room_index=next;
    delete_room_armed = false;
    dirty = true;
    set_notice("Room reordered");
}

struct Writer {
    uint8_t *p; std::size_t cap,pos; bool ok;
    void u8(uint8_t v){if(pos<cap)p[pos++]=v;else ok=false;}
    void u16(uint16_t v){u8(v);u8(v>>8);}
    void u32(uint32_t v){u8(v);u8(v>>8);u8(v>>16);u8(v>>24);}
    void bytes(const void *src,std::size_t n){if(n<=cap-pos){std::memcpy(p+pos,src,n);pos+=n;}else ok=false;}
};

void patch_u32(uint8_t *p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}

uint32_t hash_id(const char *s) {
    uint32_t h=0x811c9dc5u;
    while(*s){h^=static_cast<uint8_t>(*s++);h*=0x01000193u;}
    return h;
}

std::size_t rle_room(const EditRoom &r,uint8_t *out,std::size_t cap) {
    std::size_t pos=0;
    for(uint16_t i=0;i<256;) {
        const uint8_t v=r.tiles[i];
        uint8_t count=1;
        while(i+count<256&&r.tiles[i+count]==v&&count<255)++count;
        if(pos+2>cap)return 0;
        out[pos++]=count;out[pos++]=v;i+=count;
    }
    return pos;
}

std::size_t encode(uint8_t *out,std::size_t cap) {
    const uint8_t title_len=std::strlen(project.title);
    const uint8_t author_len=std::strlen(project.author);
    const uint16_t desc_len=std::strlen(project.description);
    Writer w{out,cap,0,true};
    w.bytes("CELV",4);w.u8(2);w.u8(1);w.u16(0);
    const std::size_t length_pos=w.pos;w.u32(0);
    const std::size_t crc_pos=w.pos;w.u32(0);
    w.u32(hash_id(project.title));w.u16(project.room_count);
    w.u8(project.difficulty);w.u8(0);w.u8(title_len);w.u8(author_len);w.u16(desc_len);w.u16(0x0101);w.u32(0);
    w.bytes(project.title,title_len);w.bytes(project.author,author_len);w.bytes(project.description,desc_len);
    for(uint8_t ri=0;ri<project.room_count;++ri) {
        const EditRoom &r=project.rooms[ri];
        uint8_t rle[512];
        const std::size_t rle_len=rle_room(r,rle,sizeof rle);
        if(!rle_len)return 0;
        uint8_t packed_rot[clevel::ROTATION_PLANE_BYTES]={0};
        for(uint16_t i=0;i<256;++i)
            packed_rot[i>>2]|=static_cast<uint8_t>((r.rotations[i]&3)<<((i&3)*2));
        const std::size_t record_len=16+rle_len+clevel::ROTATION_PLANE_BYTES+r.entity_count*4;
        w.u16(record_len);w.u8(16);w.u8(16);
        w.u8(r.spawn_x);w.u8(r.spawn_y);w.u8(r.exit_x);w.u8(r.exit_y);
        w.u8(0);w.u8(clevel::ROTATION_ENCODING_2BPP);
        w.u16(rle_len);w.u16(r.entity_count);w.u32(hash_id(project.title)+ri);
        w.bytes(rle,rle_len);w.bytes(packed_rot,sizeof packed_rot);
        for(uint8_t i=0;i<r.entity_count;++i) {
            const EditEntity &e=r.entities[i];
            w.u8(e.type);w.u8(e.x);w.u8(e.y);w.u8(e.flags);
        }
    }
    if(!w.ok)return 0;
    patch_u32(out+length_pos,w.pos);
    patch_u32(out+crc_pos,clevel::crc32(out+34,w.pos-34));
    return w.pos;
}

void export_level() {
    // The exact worst case is much smaller than the old 26 KB scratch buffer.
    // Keeping this tight leaves more RAM available for the draft and AppVar.
    static uint8_t payload[MAX_ENCODED_LEVEL_SIZE];
    const std::size_t size=encode(payload,sizeof payload);
    if(!size){set_notice("Level too large to export");return;}
    char name[9]="CL000000";
    const char hex[]="0123456789ABCDEF";
    const uint32_t id=hash_id(project.title);
    for(uint8_t i=0;i<6;++i)name[7-i]=hex[(id>>(i*4))&15];
    uint8_t h=ti_Open(name,"w");
    if(!h||ti_Write(payload,1,size,h)!=size) {
        if(h) {
            ti_Close(h);
            ti_Delete(name);
        }
        set_notice("Export failed");
        return;
    }
    ti_Close(h);
    // Moving a variable to archive can start an OS garbage-collection cycle.
    // CELEDIT draws with graphx and older builds did not install the required
    // GC callbacks, so that prompt could reset the program and clear RAM.
    // CEleste and TI Connect both accept a RAM AppVar; users may archive it
    // from Memory Management after safely leaving the editor.
    char message[32]="Exported ";
    std::strcat(message,name);
    std::strcat(message," in RAM");
    set_notice(message);
}

void activate_action() {
    new_project_armed = action_cursor==5 ? new_project_armed : false;
    switch(action_cursor) {
        case 0: undo(); view=EDITOR; break;
        case 1: redo(); view=EDITOR; break;
        case 2: details(); view=EDITOR; break;
        case 3: save_draft(); view=EDITOR; break;
        case 4: export_level(); save_draft(); view=EDITOR; break;
        case 5:
            if(!new_project_armed) {new_project_armed=true;set_notice("Press 2ND again for new project");}
            else {new_project();new_project_armed=false;view=EDITOR;set_notice("New project");}
            break;
        case 6: view=HELP_VIEW; break;
        case 7: save_draft(); view=EDITOR; break;
    }
}

void draw_current() {
    switch(view) {
        case EDITOR: draw_editor(); break;
        case PALETTE_VIEW: draw_palette(); break;
        case ROOMS_VIEW: draw_rooms(); break;
        case ACTIONS_VIEW: draw_actions(); break;
        case PROPERTIES_VIEW: draw_properties(); break;
        case HELP_VIEW: draw_help(); break;
    }
}

} // namespace

int main() {
    load_draft();
    kb_SetMode(MODE_3_CONTINUOUS);
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(imgpalette,sizeof imgpalette,0);
    gfx_SetPalette(mypalette,sizeof mypalette,0);
    gfx_SetTransparentColor(PICO_BLACK);
    gfx_SetTextFGColor(PICO_WHITE);
    gfx_SetTextBGColor(PICO_BLACK);
    gfx_SetTextTransparentColor(PICO_BLACK);
    uint8_t old[8]={};
    bool running=true;
    while(running) {
        draw_current();
        kb_Scan();
        auto p = [&](uint8_t group,uint8_t mask)->bool{return (kb_Data[group]&mask)&&!(old[group]&mask);};
        if(view==EDITOR) {
            if(p(7,kb_Left)&&cursor_x)--cursor_x;
            if(p(7,kb_Right)&&cursor_x<15)++cursor_x;
            if(p(7,kb_Up)&&cursor_y)--cursor_y;
            if(p(7,kb_Down)&&cursor_y<15)++cursor_y;
            if(p(1,kb_2nd))apply_tool();
            if(p(2,kb_Alpha)){push_undo();if(!erase_at(cursor_x,cursor_y))--undo_count;else dirty=true;}
            if(p(1,kb_Yequ))cycle_tool();
            if(p(1,kb_Window))rotate_placement();
            if(p(1,kb_Zoom)){view=PALETTE_VIEW;palette_cursor=0;}
            if(p(1,kb_Trace)){view=ROOMS_VIEW;rooms_cursor=room_index;delete_room_armed=false;}
            if(p(1,kb_Graph)){view=ACTIONS_VIEW;action_cursor=0;new_project_armed=false;}
            if(p(4,kb_Stat))open_properties();
            if(p(6,kb_Add)){
                if(project.room_count>1){room_index=static_cast<uint8_t>((room_index+1)%project.room_count);rooms_cursor=room_index;set_notice("Next room");}
                else set_notice("Only one room");
            }
            if(p(6,kb_Sub)){
                if(project.room_count>1){room_index=static_cast<uint8_t>((room_index+project.room_count-1)%project.room_count);rooms_cursor=room_index;set_notice("Previous room");}
                else set_notice("Only one room");
            }
            if(p(1,kb_Mode))save_draft();
            if(p(1,kb_Del))undo();
            if(p(6,kb_Enter))redo();
            if(p(6,kb_Clear)){save_draft();running=false;}
        } else if(view==PALETTE_VIEW) {
            uint8_t filtered[sizeof PALETTE];
            const uint8_t count=build_filtered(filtered,palette_category);
            if(p(6,kb_Add)){palette_category=static_cast<Category>((palette_category+1)%CATEGORY_COUNT);palette_cursor=0;}
            if(p(6,kb_Sub)){palette_category=static_cast<Category>((palette_category+CATEGORY_COUNT-1)%CATEGORY_COUNT);palette_cursor=0;}
            if(count) {
                if(p(7,kb_Left)&&palette_cursor)--palette_cursor;
                if(p(7,kb_Right)&&palette_cursor+1<count)++palette_cursor;
                if(p(7,kb_Up)&&palette_cursor>=6)palette_cursor-=6;
                if(p(7,kb_Down)&&palette_cursor+6<count)palette_cursor+=6;
                if(p(1,kb_Window))rotate_placement();
                if(p(1,kb_2nd)){selected_id=filtered[palette_cursor];placement_flags=0;tool=PENCIL;view=EDITOR;set_notice("Palette selection ready");}
            }
            if(p(1,kb_Mode)||p(1,kb_Zoom))view=EDITOR;
        } else if(view==ROOMS_VIEW) {
            if(p(7,kb_Left)&&rooms_cursor){--rooms_cursor;delete_room_armed=false;}
            if(p(7,kb_Right)&&rooms_cursor+1<project.room_count){++rooms_cursor;delete_room_armed=false;}
            if(p(7,kb_Up)&&rooms_cursor>=4){rooms_cursor-=4;delete_room_armed=false;}
            if(p(7,kb_Down)&&rooms_cursor+4<project.room_count){rooms_cursor+=4;delete_room_armed=false;}
            if(p(1,kb_2nd)){room_index=rooms_cursor;view=EDITOR;delete_room_armed=false;set_notice("Room opened");}
            if(p(6,kb_Enter))add_room();
            if(p(2,kb_Alpha))duplicate_room();
            if(p(1,kb_Del))delete_room();
            if(p(6,kb_Add))move_room(1);
            if(p(6,kb_Sub))move_room(-1);
            if(p(1,kb_Mode)||p(1,kb_Trace)){view=EDITOR;delete_room_armed=false;}
        } else if(view==ACTIONS_VIEW) {
            if(p(7,kb_Up)&&action_cursor){--action_cursor;new_project_armed=false;}
            if(p(7,kb_Down)&&action_cursor+1<ACTION_COUNT){++action_cursor;new_project_armed=false;}
            if(p(1,kb_2nd)){
                if(action_cursor==7){save_draft();running=false;}
                else activate_action();
            }
            if(p(1,kb_Mode)||p(1,kb_Graph)){view=EDITOR;new_project_armed=false;}
        } else if(view==PROPERTIES_VIEW) {
            const uint8_t rot=property_rotation();
            if(p(7,kb_Left))set_property_rotation((rot+3)&3);
            if(p(7,kb_Right)||p(1,kb_Window))set_property_rotation((rot+1)&3);
            if(p(1,kb_2nd))toggle_property_option();
            if(p(6,kb_Add))adjust_property_link(1);
            if(p(6,kb_Sub))adjust_property_link(-1);
            if(p(1,kb_Mode)||p(4,kb_Stat))view=EDITOR;
        } else if(view==HELP_VIEW) {
            if(p(1,kb_Mode)||p(1,kb_Yequ)||p(1,kb_2nd))view=EDITOR;
        }
        for(uint8_t group=1;group<=7;++group)old[group]=kb_Data[group];
    }
    gfx_End();
    return 0;
}
