from pathlib import Path


def edit(path, replacements):
    p = Path(path)
    text = p.read_text()
    original = text
    for old, new in replacements:
        if new in text:
            continue
        if old not in text:
            raise SystemExit(f"anchor not found in {path}: {old[:100]!r}")
        text = text.replace(old, new, 1)
    if text != original:
        p.write_text(text)


edit("src/classic.cpp", [
    (
'''void Key::update() {
    int was = sprite;
    // todo: figure out why this only shows two sprites
    sprite = 9 + (sin(frames * (UINT24_MAX / 30)) + TRIG_SCALE / 2) / TRIG_SCALE;
    int is = sprite;
    if(is == 10 and is != was) {
        flip.x = not flip.x;
    }
    if(check_player(0, 0)) {
        //sfx(23)
        sfx_timer = 10;
        has_key = true;
        delete this;
    }
}

Chest::Chest''',
'''void Key::update() {
    int was = sprite;
    // todo: figure out why this only shows two sprites
    sprite = 9 + (sin(frames * (UINT24_MAX / 30)) + TRIG_SCALE / 2) / TRIG_SCALE;
    int is = sprite;
    if(is == 10 and is != was) {
        flip.x = not flip.x;
    }
    if(check_player(0, 0)) {
        //sfx(23)
        sfx_timer = 10;
        has_key = true;
        delete this;
    }
}

SilverKey::SilverKey(int x, int y) : Object(x, y) {
    type = SILVER_KEY;
    sprite = KEY;
    solids = false;
}

void SilverKey::update() {
    int was = sprite;
    sprite = 9 + (sin(frames * (UINT24_MAX / 30)) + TRIG_SCALE / 2) / TRIG_SCALE;
    int is = sprite;
    if(is == 10 and is != was) flip.x = not flip.x;
    if(check_player(0, 0)) {
        sfx_timer = 10;
        if(custom_levels::active()) {
            custom_levels::unlock_gate_link(custom_flags);
            custom_levels::collect_source(custom_levels::room_index(), custom_source);
        }
        new Smoke(x, y);
        delete this;
    }
}

void SilverKey::draw() {
    // The silver key deliberately reuses the original animated chest-key art.
    pal(9, 6);
    pal(10, 7);
    spr_rot(sprite, x, y, custom_rotation, flip.x, flip.y);
    pal(9, 9);
    pal(10, 10);
}

Chest::Chest'''
    ),
    (
'''void Chest::update() {
    if(has_key) {
        timer -= 1;
        x = start - 1 + rnd(3);
        if(timer <= 0) {
            sfx_timer = 20;
            //sfx(16);
            // CELV entity flag bit 0 makes a locked chest empty. Flags=0
            // preserves the original key -> chest -> strawberry puzzle.
            if((custom_flags & 0x01u) == 0) {
                if(custom_levels::active()) init_object(FRUIT, x, y - 4, 0, custom_source);
                else new Fruit(x, y - 4);
            }
            delete this;
        }
    }
}

ClimbChest::ClimbChest''',
'''void Chest::update() {
    if(has_key) {
        timer -= 1;
        x = start - 1 + rnd(3);
        if(timer <= 0) {
            sfx_timer = 20;
            //sfx(16);
            // CELV entity flag bit 0 makes a locked chest empty. Flags=0
            // preserves the original key -> chest -> strawberry puzzle.
            if((custom_flags & 0x01u) == 0) {
                if(custom_levels::active()) init_object(FRUIT, x, y - 4, 0, custom_source);
                else new Fruit(x, y - 4);
            }
            delete this;
        }
    }
}

SilverGate::SilverGate(int x, int y) : Object(x, y) {
    type = SILVER_GATE;
    sprite = 0;
}

void SilverGate::update() {
    if(custom_levels::active() && custom_levels::gate_link_unlocked(custom_flags)) {
        new Smoke(x, y);
        delete this;
    }
}

void SilverGate::draw() {
    // One 8x8 linked block. Authors can stack any number with the same link
    // group to build a door, portcullis, wall, or arbitrary gate shape.
    rectfill(x, y, x + 7, y + 7, 5);
    if(custom_rotation & 1u) {
        rectfill(x, y + 1, x + 7, y + 2, 6);
        rectfill(x, y + 5, x + 7, y + 6, 6);
    } else {
        rectfill(x + 1, y, x + 2, y + 7, 6);
        rectfill(x + 5, y, x + 6, y + 7, 6);
    }
    rectfill(x + 3, y + 3, x + 4, y + 4, 7);
}

ClimbChest::ClimbChest'''
    ),
    (
'''        case KEY:
            object = custom
                ? (custom_levels::key_needed(custom_levels::room_index()) ? new Key(x, y) : nullptr)
                : (source_done ? nullptr : new Key(x, y));
            break;
        case CHEST:''',
'''        case KEY:
            object = custom
                ? (custom_levels::key_needed(custom_levels::room_index()) ? new Key(x, y) : nullptr)
                : (source_done ? nullptr : new Key(x, y));
            break;
        case SILVER_KEY:
            object = custom && !source_done ? new SilverKey(x, y) : nullptr;
            break;
        case SILVER_GATE:
            object = custom && !custom_levels::gate_link_unlocked(gameplay_flags) ? new SilverGate(x, y) : nullptr;
            break;
        case CHEST:'''
    ),
    (
'''    return solid_at(x + hitbox.x + ox, y + hitbox.y + oy, hitbox.w, hitbox.h)
           or check(FALL_FLOOR, ox, oy)
           or check(FAKE_WALL, ox, oy);''',
'''    return solid_at(x + hitbox.x + ox, y + hitbox.y + oy, hitbox.w, hitbox.h)
           or check(FALL_FLOOR, ox, oy)
           or check(FAKE_WALL, ox, oy)
           or check(SILVER_GATE, ox, oy);'''
    ),
])

edit("tools/calculator-editor/src/main.cpp", [
    (
'''    121,122,123,124,125,126,127,
    8,11,12,18,20,22,23,26,28,64,86,96,118,129
};

constexpr uint8_t ENTITY_IDS[] = {8,11,12,18,20,22,23,26,28,64,86,96,118,129};''',
'''    121,122,123,124,125,126,127,
    8,11,12,18,20,22,23,26,28,64,86,96,118,129,130,131
};

constexpr uint8_t ENTITY_IDS[] = {8,11,12,18,20,22,23,26,28,64,86,96,118,129,130,131};'''
    ),
    (
'''        case 118: return "Summit flag";
        case 129: return "Climb chest";''',
'''        case 118: return "Summit flag";
        case 129: return "Climb chest";
        case 130: return "Silver key";
        case 131: return "Silver gate";'''
    ),
    (
'''    if(id == 18) return 192;
    if(id == 8) return 231;
    if(is_entity(id)) return 164;''',
'''    if(id == 18) return 192;
    if(id == 8) return 231;
    if(id == 130) return 6;
    if(id == 131) return 5;
    if(is_entity(id)) return 164;'''
    ),
    (
'''void draw_piece(uint8_t id, int x, int y, uint8_t rotation = 0) {
    if(id == 129) {''',
'''void draw_piece(uint8_t id, int x, int y, uint8_t rotation = 0) {
    if(id == 130) {
        gfx_SetColor(6);
        gfx_FillRectangle(x, y, 4, 4);
        gfx_FillRectangle(x + 3, y + 1, 5, 2);
        gfx_FillRectangle(x + 6, y + 3, 2, 2);
        gfx_SetColor(7);
        gfx_SetPixel(x + 1, y + 1);
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
    if(id == 129) {'''
    ),
    (
'''void draw_panel(int x,int y,int w,int h,uint8_t fill=1,uint8_t border=13) {''',
'''void adjust_property_link(int delta) {
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
    } else {
        placement_flags=static_cast<uint8_t>((int(placement_flags)+delta+64)&63);
    }
    set_notice("Silver link changed");
}

void draw_panel(int x,int y,int w,int h,uint8_t fill=1,uint8_t border=13) {'''
    ),
    (
'''    } else if(selected_id==96) {
        gfx_PrintStringXY("Dashes",190,108);
        gfx_PrintStringXY((placement_flags&2)?"3":"2",238,108);
    }
    gfx_SetTextFGColor(13);''',
'''    } else if(selected_id==96) {
        gfx_PrintStringXY("Dashes",190,108);
        gfx_PrintStringXY((placement_flags&2)?"3":"2",238,108);
    } else if(selected_id==130||selected_id==131) {
        gfx_PrintStringXY("Link",190,108);
        gfx_SetTextXY(232,108); gfx_PrintUInt(placement_flags&63,2);
    }
    gfx_SetTextFGColor(13);'''
    ),
    (
'''    } else if(id==96) {
        gfx_PrintStringXY("Dash upgrade:",80,110);
        gfx_PrintStringXY((flags&2)?"3 dashes":"2 dashes",184,110);
        gfx_PrintStringXY("2ND toggles 2 / 3",80,128);
    } else {
        gfx_PrintStringXY("No extra gameplay options",80,110);
    }''',
'''    } else if(id==96) {
        gfx_PrintStringXY("Dash upgrade:",80,110);
        gfx_PrintStringXY((flags&2)?"3 dashes":"2 dashes",184,110);
        gfx_PrintStringXY("2ND toggles 2 / 3",80,128);
    } else if(id==130||id==131) {
        gfx_PrintStringXY("Link group:",80,110);
        gfx_SetTextXY(170,110); gfx_PrintUInt(flags&63,2);
        gfx_PrintStringXY("+/- changes linked gate group",80,128);
    } else {
        gfx_PrintStringXY("No extra gameplay options",80,110);
    }'''
    ),
    (
'''    gfx_PrintStringXY("Chest/fake wall berry options are editable.",8,146);
    gfx_PrintStringXY("Big chests can grant 2 or 3 dashes.",8,160);
    gfx_PrintStringXY("All pieces keep real 0/90/180/270 rotation.",8,174);
    gfx_PrintStringXY("Rooms complete by climbing through the top.",8,188);''',
'''    gfx_PrintStringXY("Chest/fake wall berry options are editable.",8,146);
    gfx_PrintStringXY("Big chests can grant 2 or 3 dashes.",8,160);
    gfx_PrintStringXY("Silver keys/gates link by group 0-63.",8,174);
    gfx_PrintStringXY("All pieces keep real 0/90/180/270 rotation.",8,188);
    gfx_PrintStringXY("Rooms complete by climbing through the top.",8,202);'''
    ),
    (
'''            if(p(7,kb_Left))set_property_rotation((rot+3)&3);
            if(p(7,kb_Right)||p(3,kb_Window))set_property_rotation((rot+1)&3);
            if(p(1,kb_2nd))toggle_property_option();
            if(p(1,kb_Mode)||p(4,kb_Stat))view=EDITOR;''',
'''            if(p(7,kb_Left))set_property_rotation((rot+3)&3);
            if(p(7,kb_Right)||p(3,kb_Window))set_property_rotation((rot+1)&3);
            if(p(1,kb_2nd))toggle_property_option();
            if(p(6,kb_Add))adjust_property_link(1);
            if(p(6,kb_Sub))adjust_property_link(-1);
            if(p(1,kb_Mode)||p(4,kb_Stat))view=EDITOR;'''
    ),
])

print("silver key/gate source update complete")
