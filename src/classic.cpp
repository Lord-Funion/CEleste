#include "classic.h"

#include <TINYSTL/vector.h>
#include <debug.h>
#include <cstdlib>
#include <cstring>
#include <keypadc.h>
#include "emu.h"
#include "profiler.h"
#include "practice.h"
#include "custom_levels.h"
#include "custom_level_menu.h"

// ~celeste~
// maddy thorson + noel berry
// ce port by john cesarz

// globals //
/////////////

struct vec2i room = {0, 0};

tinystl::vector<Object *> objects = {};
Cloud clouds[NUM_CLOUDS];
Particle particles[25];
DeadParticle dead_particles[8];
int dead_particle_timer;

Player *player = nullptr;

int freeze = 0;
int shake = 0;
bool will_restart = false;
int delay_restart = 0;
bool got_fruit[NUM_FRUITS];
bool has_dashed = false;
int sfx_timer = 0;
bool has_key = false;
bool pause_player = false;
bool flash_bg = false;
int music_timer = 0;
bool new_bg = false;

uint8_t k_left = 0;
uint8_t k_right = 1;
uint8_t k_up = 2;
uint8_t k_down = 3;
uint8_t k_jump = 4;
uint8_t k_dash = 5;

int frames;
int deaths;
int max_dash;
bool start_game;
int start_game_flash;
int seconds;
int minutes;
uint8_t new_game_plus = 0;
bool test_mode = false; // Test runs never overwrite normal progress.
bool unlimited_dashes = false; // Session-only: deliberately excluded from saves.
bool climb_enabled = false;       // Custom-level Climb Chest power-up.
int climb_stamina = 1100;          // 110.0 stamina, stored in tenths.
constexpr int CLIMB_STAMINA_MAX = 1100;
constexpr int CLIMB_HANG_COST = 4;
constexpr int CLIMB_UP_COST = 15;
constexpr int CLIMB_JUMP_COST = 275;

constexpr kb_lkey_t TEST_MODE_SEQUENCE[] = {
        kb_Key8, kb_KeySin, kb_KeyLog, kb_KeySquare, kb_KeyLn, kb_Key2nd
};
constexpr kb_lkey_t UNLIMITED_DASH_SEQUENCE[] = {
        kb_KeySto, kb_KeyMath, kb_Key2nd
};
constexpr int TEST_MODE_SEQUENCE_FRAMES = 3 * 30;
constexpr int UNLIMITED_DASH_SEQUENCE_FRAMES = 2 * 30;
constexpr int TEST_MODE_NOTICE_FRAMES = 45;
constexpr int COVENANT_NOTICE_FRAMES = 90;

uint8_t previous_keypad[8] = {};
int test_mode_sequence_step = 0;
int test_mode_sequence_timer = 0;
int unlimited_dash_sequence_step = 0;
int unlimited_dash_sequence_timer = 0;
int test_mode_notice_timer = 0;
int covenant_notice_timer = 0;

constexpr int TOTAL_STRAWBERRIES = 18;

static void spr_child_rot(uint8_t sprite, int x, int y, int dx, int dy,
                          uint8_t rotation, bool flip_x = false, bool flip_y = false);
static void draw_rotated_2x2(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                             int x, int y, uint8_t rotation);

void _init(FILE *save) {
    custom_levels::initialize();
    custom_level_menu::initialize();
    for(auto &cloud: clouds) {
        cloud.x = rnd(128);
        cloud.y = rnd(128);
        cloud.spd = 1 + rnd(4);
        cloud.w = 32 + rnd(32);
        cloud.h = 4 + (12 - cloud.w * 3 / 16);
    }

    for(Particle &p: particles) {
        p.x = rnd(SP(128));
        p.y = rnd(SP(128));
        p.s = 0 + rnd(SP(5)) / 4;
        p.spd = SP(0.25) + rnd(SP(5));
        p.off = rand() * 2;
        p.c = 6 + rnd(1);
    }

    dead_particle_timer = 0;

    if(save) {
        load_save(save);
    } else {
        title_screen();
    }
}

void title_screen() {
    custom_levels::unload();
    for(bool &i: got_fruit) {
        i = false;
    }
    frames = 0;
    deaths = 0;
    max_dash = 1;
    climb_enabled = false;
    climb_stamina = CLIMB_STAMINA_MAX;
    new_game_plus = 0;
    test_mode = false;
    start_game = false;
    start_game_flash = 0;
    //music(40,0,7);

    load_room(7, 3);
}

void begin_game() {
    frames = 0;
    seconds = 0;
    minutes = 0;
    music_timer = 0;
    start_game = false;
    //music(0,0,7);
    load_room(0, 0);
}

void begin_new_game_plus(bool from_test_mode) {
    frames = 0;
    seconds = 0;
    minutes = 0;
    deaths = 0;
    max_dash = 2;
    climb_enabled = false;
    climb_stamina = CLIMB_STAMINA_MAX;
    new_game_plus = 1;
    test_mode = from_test_mode;
    new_bg = false;
    flash_bg = false;
    music_timer = 0;
    will_restart = false;
    delay_restart = 0;
    pause_player = false;
    load_room(0, 0);
}

kb_lkey_t newly_pressed_title_key() {
    kb_lkey_t pressed_key = 0;
    bool multiple_keys = false;

    for(uint8_t group = 1; group <= 7; group++) {
        uint8_t current = kb_Data[group];
        uint8_t pressed = current & ~previous_keypad[group];
        previous_keypad[group] = current;
        for(uint8_t mask = 1; mask != 0; mask <<= 1) {
            if(pressed & mask) {
                if(pressed_key != 0) multiple_keys = true;
                pressed_key = static_cast<kb_lkey_t>((group << 8) | mask);
            }
        }
    }

    return multiple_keys ? UINT16_MAX : pressed_key;
}

bool advance_title_sequence(kb_lkey_t pressed_key, const kb_lkey_t *sequence,
                            int sequence_length, int time_limit,
                            int &step, int &timer) {
    if(step > 0 && --timer <= 0) {
        step = 0;
        timer = 0;
    }

    if(pressed_key == 0) return false;

    if(pressed_key == sequence[step]) {
        if(step == 0) timer = time_limit;
        step += 1;
        if(step == sequence_length) {
            step = 0;
            timer = 0;
            return true;
        }
    } else if(pressed_key == sequence[0]) {
        step = 1;
        timer = time_limit;
    } else {
        step = 0;
        timer = 0;
    }

    return false;
}

void update_title_sequences() {
    kb_lkey_t pressed_key = newly_pressed_title_key();
    if(!is_title()) {
        test_mode_sequence_step = 0;
        test_mode_sequence_timer = 0;
        unlimited_dash_sequence_step = 0;
        unlimited_dash_sequence_timer = 0;
        return;
    }

    if(test_mode_notice_timer > 0) return;

    if(advance_title_sequence(pressed_key, TEST_MODE_SEQUENCE,
                              sizeof(TEST_MODE_SEQUENCE) / sizeof(TEST_MODE_SEQUENCE[0]),
                              TEST_MODE_SEQUENCE_FRAMES,
                              test_mode_sequence_step, test_mode_sequence_timer)) {
        test_mode_notice_timer = TEST_MODE_NOTICE_FRAMES;
    }

    if(advance_title_sequence(pressed_key, UNLIMITED_DASH_SEQUENCE,
                              sizeof(UNLIMITED_DASH_SEQUENCE) / sizeof(UNLIMITED_DASH_SEQUENCE[0]),
                              UNLIMITED_DASH_SEQUENCE_FRAMES,
                              unlimited_dash_sequence_step, unlimited_dash_sequence_timer)) {
        unlimited_dashes = true;
        covenant_notice_timer = COVENANT_NOTICE_FRAMES;
    }
}

int level_index() {
    return room.x % 8 + room.y * 8;
}

bool is_title() {
    return !custom_levels::active() && level_index() == 31;
}

// player entity //
///////////////////

Player::Player(int x, int y) : Object(x, y) {
    dbg_printf("Player initialized at %i %i\n", x, y);
    p_jump = false;
    p_dash = false;
    grace = 0;
    jbuffer = 0;
    djump = max_dash;
    dash_time = 0;
    dash_effect_time = 0;
    dash_target = {.x=0, .y=0};
    dash_accel = {.x=0, .y=0};
    hitbox = {.x=1, .y=3, .w=6, .h=5};
    spr_off = 0;
    was_on_ground = false;
    type = PLAYER;
    create_hair(this);
    player = this;
}

Player::~Player() {
    player = nullptr;
}

void Player::update() {
    if(pause_player) return;
    profiler_add(player_update);

    int input = btn(k_right) ? 1 : (btn(k_left) ? -1 : 0);

    // spikes collide
    if(spikes_at(x + hitbox.x, y + hitbox.y, hitbox.w, hitbox.h, spd.x, spd.y)) {
        kill();
        return;
    }

    // bottom death
    if(y > 128) {
        kill();
        return;
    }

    bool on_ground = is_solid(0, 1);
    bool on_ice = is_ice(0, 1);

    // smoke particles
    if(on_ground and not was_on_ground) {
        new Smoke(x, y + 4);
    }

    bool jump = btn(k_jump) and not p_jump;
    p_jump = btn(k_jump);
    if(jump) {
        jbuffer = 4;
    } else if(jbuffer > 0) {
        jbuffer -= 1;
    }

    bool dash = btn(k_dash) and not p_dash;

    p_dash = btn(k_dash);

    if(on_ground) {
        grace = 6;
        if(climb_enabled) climb_stamina = CLIMB_STAMINA_MAX;
        if(djump < max_dash) {
            //psfx(54);
            djump = max_dash;
        }
    } else if(grace > 0) {
        grace -= 1;
    }

    dash_effect_time -= 1;
    if(dash_time > 0) {
        new Smoke(x, y);
        dash_time -= 1;
        spd.x = appr(spd.x, dash_target.x, dash_accel.x);
        spd.y = appr(spd.y, dash_target.y, dash_accel.y);
    } else {

        // Optional modern-Celeste-style climbing, unlocked by the custom Climb Chest.
        // MATH is the grab button. Ice walls deliberately cannot be climbed.
        int climb_wall = 0;
        if(climb_enabled && !on_ground && !dash && kb_IsDown(kb_KeyMath) && climb_stamina > 0) {
            if(is_solid(-3, 0) && !is_ice(-3, 0)) climb_wall = -1;
            else if(is_solid(3, 0) && !is_ice(3, 0)) climb_wall = 1;
        }
        bool climbing = climb_wall != 0;

        if(climbing && jbuffer > 0) {
            jbuffer = 0;
            climb_stamina = max(0, climb_stamina - CLIMB_JUMP_COST);
            spd.y = SP(-2);
            spd.x = -climb_wall * SP(2);
            new Smoke(x + climb_wall * 6, y);
            climbing = false;
        }

        if(climbing) {
            spd.x = 0;
            flip.x = climb_wall < 0;
            if(btn(k_up)) {
                spd.y = SP(-0.8);
                climb_stamina = max(0, climb_stamina - CLIMB_UP_COST);
            } else if(btn(k_down)) {
                spd.y = SP(0.8);
            } else {
                spd.y = 0;
                climb_stamina = max(0, climb_stamina - CLIMB_HANG_COST);
            }
            if(climb_stamina == 0) climbing = false;
        }

        if(!climbing) {
        // move
        subpixel maxrun = SP(1);
        subpixel accel = SP(0.6);
        subpixel deccel = SP(0.15);

        if(not on_ground) {
            accel = SP(0.4);
        } else if(on_ice) {
            accel = SP(0.05);
            if(input == (flip.x ? -1 : 1)) {
                accel = SP(0.05);
            }
        }

        if(abs(spd.x) > maxrun) {
            spd.x = appr(spd.x, sign(spd.x) * maxrun, deccel);
        } else {
            spd.x = appr(spd.x, input * maxrun, accel);
        }

        //facing
        if(spd.x != 0) {
            flip.x = (spd.x < 0);
        }

        // gravity
        subpixel maxfall = SP(2);
        subpixel gravity = SP(0.21);

        if(abs(spd.y) <= SP(0.15)) {
            gravity = SP(0.21 * 0.5);
        }

        // wall slide
        if(input != 0 and is_solid(input, 0) and not is_ice(input, 0)) {
            maxfall = SP(0.4);
            if(rnd(10) < 2) {
                new Smoke(x + input * 6, y);
            }
        }

        if(not on_ground) {
            spd.y = appr(spd.y, maxfall, gravity);
        }

        // jump
        if(jbuffer > 0) {
            if(grace > 0) {
                // normal jump
                //psfx(1);
                jbuffer = 0;
                grace = 0;
                spd.y = SP(-2);
                new Smoke(x, y + 4);
            } else {
                // wall jump
                int wall_dir = (is_solid(-3, 0) ? -1 : is_solid(3, 0) ? 1 : 0);
                if(wall_dir != 0) {
                    //psfx(2);
                    jbuffer = 0;
                    spd.y = SP(-2);
                    spd.x = -wall_dir * (maxrun + SP(1));
                    if(not is_ice(wall_dir * 3, 0)) {
                        new Smoke(x + wall_dir * 6, y);
                    }
                }
            }
        }

        // dash
        subpixel d_full = SP(5);
        subpixel d_half = SP(5 * 0.70710678118);

        if((djump > 0 or unlimited_dashes) and dash) {
            new Smoke(x, y);
            if(not unlimited_dashes) djump -= 1;
            dash_time = 4;
            has_dashed = true;
            dash_effect_time = 10;
            int v_input = (btn(k_up) ? -1 : (btn(k_down) ? 1 : 0));
            if(input != 0) {
                if(v_input != 0) {
                    spd.x = input * d_half;
                    spd.y = v_input * d_half;
                } else {
                    spd.x = input * d_full;
                    spd.y = 0;
                }
            } else if(v_input != 0) {
                spd.x = 0;
                spd.y = v_input * d_full;
            } else {
                spd.x = SP(flip.x ? -1 : 1);
                spd.y = 0;
            }

            //psfx(3);
            freeze = 2;
            shake = 6;
            dash_target.x = SP(2) * sign(spd.x);
            dash_target.y = SP(2) * sign(spd.y);
            dash_accel.x = SP(1.5);
            dash_accel.y = SP(1.5);

            if(spd.y < 0) {
                dash_target.y = dash_target.y * 3 / 4;
            }

            if(spd.y != 0) {
                dash_accel.x = SP(1.5 * 0.70710678118);
            }
            if(spd.x != 0) {
                dash_accel.y = SP(15 * 0.70710678118);
            }
        } else if(dash and djump <= 0 and not unlimited_dashes) {
            //psfx(9);
            new Smoke(x, y);
        }
        } // !climbing

    }

    // animation
    spr_off += 1;
    if(not on_ground) {
        if(is_solid(input, 0)) {
            sprite = 5;
        } else {
            sprite = 3;
        }
    } else if(btn(k_down)) {
        sprite = 6;
    } else if(btn(k_up)) {
        sprite = 7;
    } else if((spd.x == 0) or (not btn(k_left) and not btn(k_right))) {
        sprite = 1;
    } else {
        sprite = 1 + (spr_off / 4) % 4;
    }

    // was on the ground
    was_on_ground = on_ground;

    // next level
    if(y < -4 and (custom_levels::active() or level_index() < 30)) {
        if(practice_mode) {
            practice_on_complete();
        } else {
            next_room();
        }
    }
    profiler_end(player_update);
}

void Player::draw() {
    profiler_add(player_draw);
    // clamp in screen
    if(x < -1 or x > 121) {
        x = clamp(x, -1, 121);
        spd.x = 0;
    }

    set_hair_color(djump);
    draw_hair(this, flip.x ? -1 : 1);
    spr(sprite, x, y, 1, 1, flip.x, flip.y);
    unset_hair_color();
    profiler_end(player_draw);
}

//void psfx(int num) {
//    if(sfx_timer <= 0) {
//        //sfx(num)
//    }
//}

void create_hair(Object *obj) {
    for(int i = 0; i < HAIR_SEGMENTS; i++) {
        obj->hair[i].x = SP(obj->x);
        obj->hair[i].y = SP(obj->y);
        obj->hair[i].size = max(1, min(2, 3 - i));
    }
}

void set_hair_color(int djump) {
    // todo: floor?
    pal(8, (djump == 1 ? 8 : djump == 2 ? (7 + ((frames / 3) % 2) * 4) : 12));
}

void draw_hair(Object *obj, int facing) {
    static const uint8_t rainbow_colors[HAIR_SEGMENTS] = {8, 9, 10, 11, 12, 2};
    struct vec2i last = {.x=SP(obj->x + 4 - facing * 2), .y=SP(obj->y + (btn(k_down) ? 4 : 3))};
    int segments = unlimited_dashes ? HAIR_SEGMENTS : 5;
    for(int i = 0; i < segments; i++) {
        obj->hair[i].x += (last.x - obj->hair[i].x) * 2 / 3;
        obj->hair[i].y += ((last.y - obj->hair[i].y) * 2 + 1) / 3;
        if(unlimited_dashes) pal(8, rainbow_colors[i]);
        circfill(PIX(obj->hair[i].x), PIX(obj->hair[i].y), obj->hair[i].size, 8);
        last.x = obj->hair[i].x;
        last.y = obj->hair[i].y;
    }
}

void unset_hair_color() {
    pal(8, 8);
}

PlayerSpawn::PlayerSpawn(int x, int y) : Object(x, y) {
    //sfx(4);
    sprite = 3;
    target = {.x=x, .y=y};
    this->y = 128;
    spd.y = SP(-4);
    state = 0;
    delay = 0;
    solids = false;
    type = PLAYER_SPAWN;
    create_hair(this);
}

void PlayerSpawn::update() {
    // jumping up
    if(state == 0) {
        if(y < target.y + 16) {
            state = 1;
            delay = 3;
        }
        // falling
    } else if(state == 1) {
        spd.y += SP(0.5);
        if(spd.y > 0 and delay > 0) {
            spd.y = 0;
            delay -= 1;
        }
        if(spd.y > 0 and y > target.y) {
            y = target.y;
            spd = {.x=0, .y=0};
            state = 2;
            delay = 5;
            shake = 5;
            new Smoke(x, y + 4);
            //sfx(5);
        }
        // landing
    } else if(state == 2) {
        delay -= 1;
        sprite = 6;
        if(delay < 0) {
            new Player(x, y);
            delete this;
        }
    }
}

void PlayerSpawn::draw() {
    set_hair_color(max_dash);
    draw_hair(this, 1);
    spr(sprite, x, y, 1, 1, flip.x, flip.y);
    unset_hair_color();
}

Spring::Spring(int x, int y) : Object(x, y) {
    hide_in = 0;
    hide_for = 0;
    type = SPRING;
    sprite = SPRING;
}

void Spring::update() {
    if(hide_for > 0) {
        hide_for -= 1;
        if(hide_for <= 0) {
            sprite = 18;
            delay = 0;
        }
    } else if(sprite == 18) {
        Player *hit = collide_player(0, 0);
        if(hit != nullptr and hit->spd.y >= 0) {
            sprite = 19;
            hit->y = y - 4;
            hit->spd.x /= 5;
            hit->spd.y = SP(-3);
            hit->djump = max_dash;
            delay = 10;
            new Smoke(x, y);

            // breakable below us
            auto below = (FallFloor *) collide(FALL_FLOOR, 0, 1);
            if(below != nullptr) {
                below->break_floor();
            }

            //psfx(8);
        }
    } else if(delay > 0) {
        delay -= 1;
        if(delay <= 0) {
            sprite = 18;
        }
    }
    // begin hiding
    if(hide_in > 0) {
        hide_in -= 1;
        if(hide_in <= 0) {
            hide_for = 60;
            sprite = 0;
        }
    }
}

void Spring::break_spring() {
    hide_in = 15;
}

Balloon::Balloon(int x, int y) : Object(x, y) {
    offset = rand() * 2;
    sprite_tmr = rnd(3);
    start = y;
    timer = 0;
    hitbox = {.x=-1, .y=-1, .w=10, .h=10};
    type = BALLOON;
    sprite = BALLOON;
    offset = 0;
}

void Balloon::update() {
    if(sprite == 22) {
        offset += UINT24_MAX / 100;
        y = start + sin(offset) * 2 / TRIG_SCALE;
        Player *hit = collide_player(0, 0);
        if(hit != nullptr and hit->djump < max_dash) {
            //psfx(6);
            new Smoke(x, y);
            hit->djump = max_dash;
            sprite = 0;
            timer = 60;
        }
    } else if(timer > 0) {
        timer -= 1;
    } else {
        //psfx(7)
        new Smoke(x, y);
        sprite = 22;
    }
    if(++sprite_tmr == 24) sprite_tmr = 0;
}

void Balloon::draw() {
    if(sprite == 22) {
        spr_child_rot(13 + sprite_tmr / 8, x, y, 0, 6, custom_rotation);
        spr_rot(sprite, x, y, custom_rotation);
    }
}

FallFloor::FallFloor(int x, int y) : Object(x, y) {
    state = 0;
    solid = true;
    type = FALL_FLOOR;
}

void FallFloor::update() {
    // idling
    if(state == 0) {
        if(check_player(0, -1) or check_player(-1, 0) or check_player(1, 0)) {
            break_floor();
        }
        // shaking
    } else if(state == 1) {
        delay -= 1;
        if(delay <= 0) {
            state = 2;
            delay = 60;//how long it hides for
            collideable = false;
        }
        // invisible, waiting to reset
    } else if(state == 2) {
        delay -= 1;
        if(delay <= 0 and not check_player(0, 0)) {
            //psfx(7);
            state = 0;
            collideable = true;
            new Smoke(x, y);
        }
    }
}

void FallFloor::draw() {
    if(state != 2) {
        if(state != 1) {
            spr_rot(23, x, y, custom_rotation);
        } else {
            spr_rot(23 + (15 - delay) / 5, x, y, custom_rotation);
        }
    }
}

void FallFloor::break_floor() {
    if(state == 0) {
        //psfx(15)
        state = 1;
        delay = 15;//how long until it falls
        new Smoke(x, y);
        auto hit = (Spring *) collide(SPRING, 0, -1);
        if(hit != nullptr) {
            hit->break_spring();
        }
    }
}

Smoke::Smoke(int x, int y) : Object(x, y) {
    sprite = 29;
    spd.y = SP(-0.1);
    spd.x = SP(0.3) + rnd(SP(0.2));
    this->x += -1 + rnd(2);
    this->y += -1 + rnd(2);
    flip.x = maybe();
    flip.y = maybe();
    solids = false;
    type = SMOKE;
}

void Smoke::update() {
    if(++sprite_timer == 5) {
        sprite_timer = 0;
        sprite++;
    }
    if(sprite >= 32) {
        delete this;
    }
}

Fruit::Fruit(int x, int y) : Object(x, y) {
    start = y;
    off = 0;
    sprite = 26;
    type = FRUIT;
}

void Fruit::update() {
    Player *hit = collide_player(0, 0);
    if(hit != nullptr) {
        hit->djump = max_dash;
        sfx_timer = 20;
        //sfx(13);
        if(custom_levels::active()) custom_levels::collect_source(custom_levels::room_index(), custom_source);
        else got_fruit[level_index()] = true;
        new LifeUp(x, y);
        delete this;
    } else {
        off += UINT24_MAX / 40;
        y = start + sin(off) * 5 / 2 / TRIG_SCALE;
    }
}

FlyFruit::FlyFruit(int x, int y) : Object(x, y) {
    start = y;
    fly = false;
    step = UINT24_MAX / 4;
    solids = false;
    sfx_delay = 8;
    type = FLY_FRUIT;
    sprite = FLY_FRUIT;
    off = 0;
}

void FlyFruit::update() {
    //fly away
    if(fly) {
        if(sfx_delay > 0) {
            sfx_delay -= 1;
            if(sfx_delay <= 0) {
                sfx_timer = 20;
                //sfx(14);
            }
        }
        spd.y = appr(spd.y, SP(-3.5), SP(0.25));
        if(y < -16) {
            delete this;
        }
        // wait
    } else {
        if(has_dashed) {
            fly = true;
        }
        step += UINT24_MAX / 20;
        spd.y = sin(step) / 2;
    }
    // collect
    Player *hit = collide_player(0, 0);
    if(hit != nullptr) {
        hit->djump = max_dash;
        sfx_timer = 20;
        //sfx(13)
        if(custom_levels::active()) custom_levels::collect_source(custom_levels::room_index(), custom_source);
        else got_fruit[level_index()] = true;
        new LifeUp(x, y);
        delete this;
    }
}

void FlyFruit::draw() {
    if(not fly) {
        int dir = sin(step);
        if(dir < 0) {
            off = (1 + max(0, sign(y - start))) * 4;
        }
    } else {
        off = (off + 1) % 12;
    }
    spr_child_rot(45 + off / 4, x, y, -6, -2, custom_rotation, true, false);
    spr_rot(sprite, x, y, custom_rotation);
    spr_child_rot(45 + off / 4, x, y, 6, -2, custom_rotation);
}

LifeUp::LifeUp(int x, int y) : Object(x, y) {
    spd.y = SP(-0.25);
    duration = 30;
    x -= 2;
    y -= 4;
    flash = 0;
    solids = false;
}

void LifeUp::update() {
    duration -= 1;
    if(duration <= 0) {
        delete this;
    }
}

void LifeUp::draw() {
    flash += 1;

    print("1000", x - 2, y, 7 + (flash / 2) % 2);
}

FakeWall::FakeWall(int x, int y) : Object(x, y) {
    type = FAKE_WALL;
};

void FakeWall::update() {
    hitbox = {.x=-1, .y=-1, .w=18, .h=18};
    Player *hit = collide_player(0, 0);
    if(hit != nullptr and hit->dash_effect_time > 0) {
        hit->spd.x = -sign(hit->spd.x) * 3 / 2;
        hit->spd.y = SP(-1.5);
        hit->dash_time = -1;
        sfx_timer = 20;
        //sfx(16);
        new Smoke(x, y);
        new Smoke(x + 8, y);
        new Smoke(x, y + 8);
        new Smoke(x + 8, y + 8);
        // CELV entity flag bit 0 makes a fake wall empty. Flags=0 keeps
        // original Celeste Classic behavior: a strawberry is hidden inside.
        if((custom_flags & 0x01u) == 0) {
            if(custom_levels::active()) init_object(FRUIT, x + 4, y + 4, 0, custom_source);
            else new Fruit(x + 4, y + 4);
        }
        delete this;
    }
    hitbox = {.x=0, .y=0, .w=16, .h=16};
}

void FakeWall::draw() {
    draw_rotated_2x2(64, 65, 80, 81, x, y, custom_rotation);
}

Key::Key(int x, int y) : Object(x, y) {
    type = KEY;
    sprite = KEY;
}

void Key::update() {
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

Chest::Chest(int x, int y) : Object(x, y) {
    x -= 4;
    start = x;
    timer = 20;
    type = CHEST;
    sprite = CHEST;
}

void Chest::update() {
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

ClimbChest::ClimbChest(int x, int y) : Object(x, y) {
    type = CLIMB_CHEST;
    sprite = CHEST;
    solids = false;
}

void ClimbChest::update() {
    Player *hit = collide_player(0, 0);
    if(hit == nullptr) return;
    climb_enabled = true;
    climb_stamina = CLIMB_STAMINA_MAX;
    if(custom_levels::active()) custom_levels::collect_source(custom_levels::room_index(), custom_source);
    new Smoke(x, y);
    freeze = 4;
    shake = 6;
    delete this;
}

void ClimbChest::draw() {
    spr_rot(CHEST, x, y, custom_rotation);
    draw_plus(x + 4, y + 4, 11);
}

Platform::Platform(int x, int y, int dir) : Object(x, y) {
    x -= 4;
    solids = false;
    hitbox.w = 16;
    last = x;
    this->dir = dir;
    type = PLATFORM;
}

void Platform::update() {
    spd.x = dir * SP(0.65);
    if(x < -16) {
        x = 128;
    } else if(x > 128) {
        x = -16;
    }
    if(not check_player(0, 0)) {
        Player *hit = collide_player(0, -1);
        if(hit != nullptr) {
            hit->move_x(x - last, 1);
        }
    }
    last = x;
}

void Platform::draw() {
    spr_child_rot(11, x, y - 1, 0, 0, custom_rotation);
    spr_child_rot(12, x, y - 1, 8, 0, custom_rotation);
}

Message::Message(int x, int y) : Object(x, y) {
    index = 0;
    type = MESSAGE;
}

void Message::draw() {
    // In the original cartridge the memorial artwork is four map sprites
    // (70/71 over 86/87). Custom levels store the message as one logical
    // entity, so draw the complete sign here instead of requiring authors
    // to assemble companion sprite fragments by hand.
    if(custom_levels::active()) {
        draw_rotated_2x2(70, 71, 86, 87, x, y - 8, custom_rotation);
    }
    const char *text = "-- celeste mountain --#this memorial to those# perished on the climb";
    if(check_player(4, 0)) {
        if(index / 2 < strlen(text)) {
            index += 1;
            // sfx deleted here
        }
        vec2i off = {.x = 8, .y = 96};
        for(unsigned i = 0; i < index / 2; i++) {
            if(text[i] != '#') {
                // todo: only one rectangle per line?
                rectfill(off.x - 2, off.y - 2, off.x + 7, off.y + 6, 7);
                print(text[i], off.x, off.y, 0);
                off.x += 5;
            } else {
                off.x = 8;
                off.y += 7;
            }
        }
    } else {
        index = 0;
    }
}

BigChest::BigChest(int x, int y) : Object(x, y) {
    state = 0;
    hitbox.w = 16;
    type = BIG_CHEST;
}

uint8_t chest_particle_count = 0;

void BigChest::draw() {
    static ChestParticle particles[50];
    if(state == 0) {
        Player *hit = collide_player(0, 8);
        if(hit != nullptr and hit->is_solid(0, 1)) {
            //music(-1,500,7);
            //sfx(37);
            pause_player = true;
            hit->spd.x = 0;
            hit->spd.y = 0;
            state = 1;
            new Smoke(x, y);
            new Smoke(x + 8, y);
            timer = 60;
            chest_particle_count = 0;
        }
        spr_child_rot(96, x, y, 0, 0, custom_rotation);
        spr_child_rot(97, x, y, 8, 0, custom_rotation);
    } else if(state == 1) {
        timer -= 1;
        shake = 5;
        flash_bg = true;
        if(timer <= 45 and chest_particle_count < 50) {
            ChestParticle &p = particles[chest_particle_count++];
            p.x = SP(1) + rnd(SP(14));
            p.y = 0;
            p.h = 32 + rnd(32);
            p.spd = SP(8) + rnd(SP(8));
        }
        if(timer < 0) {
            state = 2;
            chest_particle_count = 0;
            flash_bg = false;
            new_bg = true;
            // Custom big chests can opt into a three-dash upgrade with
            // entity flag bit 1; ordinary/custom flags=0 gives two dashes.
            const uint8_t target = custom_levels::active()
                ? ((custom_flags & 0x02u) ? 3 : 2)
                : (new_game_plus ? 3 : 2);
            new Orb(x + 4, y + 4, target);
            pause_player = false;
        }
        for(int i = 0; i < chest_particle_count; i++) {
            ChestParticle &p = particles[i];
            p.y += p.spd;
            vert_line(x + PIX(p.x), y + 8 - PIX(p.y), min(y + 8 - PIX(p.y) + p.h, y + 8), 7);
        }
    }
    spr_child_rot(112, x, y, 0, 8, custom_rotation);
    spr_child_rot(113, x, y, 8, 8, custom_rotation);
}

Orb::Orb(int x, int y, uint8_t target_dashes) : Object(x, y) {
    spd.y = SP(-4);
    solids = false;
    chest_particle_count = 0;
    this->target_dashes = target_dashes ? target_dashes : (new_game_plus ? 3 : 2);
}

void Orb::draw() {
    spd.y = appr(spd.y, 0, SP(0.5));
    Player *hit = collide_player(0, 0);
    if(spd.y == 0 and hit != nullptr) {
        music_timer = 45;
        //sfx(51);
        freeze = 10;
        shake = 10;
        max_dash = target_dashes;
        hit->djump = max_dash;
        delete this;
    } else {
        spr(102, x, y);
        int off = frames * (1 << 19);
        for(int i = 0; i <= 7; i++) {
            draw_plus(x + 4 + cos(off + i * (1 << 21)) * 8 / TRIG_SCALE,
                      y + 4 + sin(off + i * (1 << 21)) * 8 / TRIG_SCALE, 7);
        }
    }
}

Flag::Flag(int x, int y) : Object(x, y) {
    this->x += 5;
    score = 0;
    show = false;
    for(bool i: got_fruit) {
        if(i) {
            score += 1;
        }
    }
    type = FLAG;
}

bool show_new_game_plus_prompt(int score) {
    return !custom_levels::active() && !practice_mode && !new_game_plus && level_index() == 30 &&
           score == TOTAL_STRAWBERRIES;
}

void Flag::draw() {
    sprite = 118 + (frames / 5) % 3;
    spr_rot(sprite, x, y, custom_rotation);
    if(show) {
        bool show_prompt = show_new_game_plus_prompt(score);
        int results_bottom = show_prompt ? 39 : 31;
        rectfill(32, 2, 96, results_bottom, 0);
        if(practice_mode) {
            int total = practice_get_total_time();
            if(total == 0) {
                print("incomplete", 44, 16, 7);
            } else {
                int subsecond = (total % 30) * 10 / 3;
                int seconds = total / 30;
                int minutes = seconds / (30 * 60);
                print_int(minutes, 49, 17, 7, 2);
                print(":");
                print_int(seconds, 2);
                print(".");
                print_int(subsecond, 2);
            }
        } else {
            spr(26, 55, 6);
            print("x", 64, 9, 7);
            print_int(score);
            draw_time(49, 16);
            print("deaths:", 48, 24, 7);
            print_int(deaths);
            if(show_prompt) {
                print("2nd: new+", 38, 33, 7);
            }
        }
    } else if(check_player(0, 0)) {
        //sfx(55);
        sfx_timer = 30;
        show = true;
    }
}

bool new_game_plus_available() {
    if(custom_levels::active() || practice_mode || new_game_plus || level_index() != 30) return false;
    for(Object *object : objects) {
        if(object != nullptr && object->type == FLAG && static_cast<Flag *>(object)->show) {
            return static_cast<Flag *>(object)->score == TOTAL_STRAWBERRIES;
        }
    }
    return false;
}

RoomTitle::RoomTitle(int x, int y) : Object(x, y) {
    delay = 5;
}

void RoomTitle::draw() {
    delay -= 1;
    if(delay < -30) {
        delete this;
    } else if(delay < 0) {

        rectfill(24, 58, 104, 70, 0);

        if(room.x == 3 and room.y == 1) {
            print("old site", 48, 62, 7);
        } else if(level_index() == 30) {
            print("summit", 52, 62, 7);
        } else {
            int level = (1 + level_index()) * 100;
            print_int(level, ((level < 1000) ? 54 : 52), 62, 7);
            print(" m");
        }
        //print("---",86,64-2,13)

        if(!practice_mode) {
            draw_time(4, 4);
        }
    }
}

// object functions //
///////////////////////

Object *init_object(type type, int x, int y, uint8_t flags, uint8_t source) {
    const bool custom = custom_levels::active();
    const uint8_t gameplay_flags = static_cast<uint8_t>(flags & clevel::ENTITY_FLAG_MASK);
    const uint8_t rotation = static_cast<uint8_t>((flags & clevel::ENTITY_ROTATION_MASK) >> clevel::ENTITY_ROTATION_SHIFT);
    const bool source_done = custom
        ? custom_levels::source_collected(custom_levels::room_index(), source)
        : got_fruit[level_index()];
    Object *object = nullptr;
    switch(type) {
        case PLAYER_SPAWN: object = new PlayerSpawn(x, y); break;
        case SPRING: object = new Spring(x, y); break;
        case BALLOON: object = new Balloon(x, y); break;
        case FALL_FLOOR: object = new FallFloor(x, y); break;
        case SMOKE: object = new Smoke(x, y); break;
        case FRUIT: object = source_done ? nullptr : new Fruit(x, y); break;
        case FLY_FRUIT: object = source_done ? nullptr : new FlyFruit(x, y); break;
        case FAKE_WALL:
            object = (custom && (gameplay_flags & 0x01u)) || !source_done ? new FakeWall(x, y) : nullptr;
            break;
        case KEY:
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
        case CHEST:
            object = (custom && (gameplay_flags & 0x01u)) || !source_done ? new Chest(x, y) : nullptr;
            break;
        case CLIMB_CHEST:
            object = custom && !source_done ? new ClimbChest(x, y) : nullptr;
            break;
        case PLATFORM: object = new Platform(x, y, -1); break;
        case PLATFORM_RIGHT: object = new Platform(x, y, 1); break;
        case MESSAGE: object = new Message(x, y); break;
        case BIG_CHEST: object = new BigChest(x, y); break;
        case FLAG: object = new Flag(x, y); break;
        default: return nullptr;
    }
    if(object) {
        object->custom_flags = gameplay_flags;
        object->custom_rotation = rotation;
        object->custom_source = source;
    }
    return object;
}

Object::Object(int x, int y) {
    collideable = true;
    solids = true;

    sprite = 0;
    flip = {.x=false, .y=false};

    this->x = x;
    this->y = y;
    hitbox = {.x=0, .y=0, .w=8, .h=8};
    custom_flags = 0;
    custom_rotation = 0;
    custom_source = 0xFF;

    spd = {.x=0, .y=0};
    rem = {.x=0, .y=0};

    objects.push_back(this);
}


bool Object::is_solid(int ox, int oy) {
    if(oy > 0 and not check(PLATFORM, ox, 0) and check(PLATFORM, ox, oy)) {
        return true;
    }
    return solid_at(x + hitbox.x + ox, y + hitbox.y + oy, hitbox.w, hitbox.h)
           or check(FALL_FLOOR, ox, oy)
           or check(FAKE_WALL, ox, oy)
           or check(SILVER_GATE, ox, oy);
}


bool Object::is_ice(int ox, int oy) {
    return ice_at(x + hitbox.x + ox, y + hitbox.y + oy, hitbox.w, hitbox.h);
}

Object *Object::collide(enum type type, int ox, int oy) {
    profiler_add(collide_other);
    for(Object *other: objects) {
        if(other != nullptr and other->type == type and other != this and other->collideable and
           other->x + other->hitbox.x + other->hitbox.w > x + hitbox.x + ox and
           other->y + other->hitbox.y + other->hitbox.h > y + hitbox.y + oy and
           other->x + other->hitbox.x < x + hitbox.x + hitbox.w + ox and
           other->y + other->hitbox.y < y + hitbox.y + hitbox.h + oy) {
            profiler_end(collide_other);
            return other;
        }
    }
    profiler_end(collide_other);
    return nullptr;
}

Player *Object::collide_player(int ox, int oy) {
    profiler_add(collide_player);
    if(player != nullptr and player != this and
       player->x + 1 + 6 > x + hitbox.x + ox and
       player->y + 3 + 5 > y + hitbox.y + oy and
       player->x + 1 < x + hitbox.x + hitbox.w + ox and
       player->y + 3 < y + hitbox.y + hitbox.h + oy) {
        profiler_end(collide_player);
        return player;
    }
    profiler_end(collide_player);
    return nullptr;
}

bool Object::check(enum type type, int ox, int oy) {
    return collide(type, ox, oy) != nullptr;
}

bool Object::check_player(int ox, int oy) {
    return collide_player(ox, oy) != nullptr;
}

void Object::move(subpixel ox, subpixel oy) {
    profiler_add(move);
    if(ox) {
        // [x] get move amount
        rem.x += ox;
        int amount = PIX(rem.x + SP(0.5));
        rem.x -= SP(amount);
        move_x(amount, 0);
    }

    if(oy) {
        // [y] get move amount
        rem.y += oy;
        int amount = PIX(rem.y + SP(0.5));
        rem.y -= SP(amount);
        move_y(amount);
    }
    profiler_end(move);
}

void Object::move_x(int amount, int start) {
    if(!amount) return;
    profiler_add(move_x);
    if(solids) {
        int step = sign(amount);
        for(int i = start; i <= abs(amount); i++) {
            if(not is_solid(step, 0)) {
                x += step;
            } else {
                spd.x = 0;
                rem.x = 0;
                break;
            }
        }
    } else {
        x += amount;
    }
    profiler_end(move_x);
}

void Object::move_y(int amount) {
    if(!amount) return;
    profiler_add(move_y);
    if(solids) {
        int step = sign(amount);
        for(int i = 0; i <= abs(amount); i++) {
            if(not is_solid(0, step)) {
                y += step;
            } else {
                spd.y = 0;
                rem.y = 0;
                break;
            }
        }
    } else {
        y += amount;
    }
    profiler_end(move_y);
}

Object::~Object() {
    for(auto i = objects.begin(); i != objects.end(); i++) {
        if(*i == this) {
            objects.erase(i);
            return;
        }
    }
}

void Player::kill() {
    sfx_timer = 12;
    //sfx(0);
    deaths += 1;
    shake = 10;
    dead_particle_timer = 10;
    for(int dir = 0; dir <= 7; dir++) {
        DeadParticle &p = dead_particles[dir];
        p.x = SP(x + 4);
        p.y = SP(y + 4);
        p.spd.x = sin(dir * (1 << 21)) * 3;
        p.spd.y = cos(dir * (1 << 21)) * 3;
    }
    restart_room();
    delete this;
}

// room functions //
////////////////////


void restart_room() {
    will_restart = true;
    delay_restart = 15;
}


void next_room() {
    if(custom_levels::active()) {
        if(custom_levels::next_room()) {
            const uint8_t index = custom_levels::room_index();
            load_room(index % 8, index / 8);
        } else if(custom_levels::next_level()) {
            climb_enabled = false;
            climb_stamina = CLIMB_STAMINA_MAX;
            load_room(0, 0);
        } else {
            title_screen();
        }
        return;
    }
    if(room.x == 2 and room.y == 1) {
        //music(30,500,7)
    } else if(room.x == 3 and room.y == 1) {
        //music(20,500,7)
    } else if(room.x == 4 and room.y == 2) {
        //music(30,500,7)
    } else if(room.x == 5 and room.y == 3) {
        //music(30,500,7)
    }

    if(level_index() == 30) return;

    if(room.x == 7) {
        load_room(0, room.y + 1);
    } else {
        load_room(room.x + 1, room.y);
    }
}

void prev_room() {
    if(custom_levels::active()) {
        if(custom_levels::previous_room()) {
            const uint8_t index = custom_levels::room_index();
            load_room(index % 8, index / 8);
        }
        return;
    }
    if(level_index() < 1) return;
    if(room.x == 0) {
        load_room(7, room.y - 1);
    } else {
        load_room(room.x - 1, room.y);
    }
}

void load_room(uint8_t x, uint8_t y) {
    dbg_printf("loading room %u, %u\n", x, y);
    has_dashed = false;
    has_key = false;

    //remove existing objects
    while(!objects.empty()) {
        delete objects[0];
    }

    //current room
    room.x = x;
    room.y = y;

    if(!custom_levels::active() && practice_mode && level_index() != 30) {
        got_fruit[level_index()] = false;
    }

    // entities
    if(custom_levels::active()) {
        const clevel::Level *level = custom_levels::level();
        const uint8_t ri = custom_levels::room_index();
        if(level && ri < level->room_count) {
            const clevel::Room &custom_room = level->rooms[ri];
            init_object(PLAYER_SPAWN, custom_room.spawn_x * 8, custom_room.spawn_y * 8);
            for(uint8_t i = 0; i < custom_room.entity_count; ++i) {
                const clevel::Entity &entity = custom_room.entities[i];
                init_object((type)entity.type, entity.x * 8, entity.y * 8, entity.flags, i);
            }
            // Backward compatibility with early CELV v1 writers that also
            // left entity IDs in the RLE tile plane. Only instantiate one if
            // no explicit entity record occupies that coordinate.
            for(uint8_t ty = 0; ty < 16; ++ty) for(uint8_t tx = 0; tx < 16; ++tx) {
                const uint8_t raw = custom_room.tiles[ty * 16 + tx];
                bool recorded = false;
                for(uint8_t i = 0; i < custom_room.entity_count; ++i) {
                    if(custom_room.entities[i].x == tx && custom_room.entities[i].y == ty) { recorded = true; break; }
                }
                if(!recorded) init_object((type)raw, tx * 8, ty * 8);
            }
        }
    } else {
        for(uint8_t tx = 0; tx <= 15; tx++) {
            for(uint8_t ty = 0; ty <= 15; ty++) {
                uint8_t tile = mget(room.x * 16 + tx, room.y * 16 + ty);
                init_object((type) (tile), tx * 8, ty * 8);
            }
        }
    }

    if(!is_title()) {
        new RoomTitle(0, 0);
    }

    practice_on_load();
}

// update function //
///////////////////////

void _update() {
    profiler_add(update);
    frames = ((frames + 1) % 30);
    if(frames == 0 and (custom_levels::active() or level_index() < 30)) {
        seconds = ((seconds + 1) % 60);
        if(seconds == 0) {
            minutes += 1;
        }
    }

    if(music_timer > 0) {
        music_timer -= 1;
        if(music_timer <= 0) {
            //music(10, 0, 7);
        }
    }

    if(sfx_timer > 0) {
        sfx_timer -= 1;
    }

    update_title_sequences();

    if(custom_level_menu::update()) {
        profiler_end(update);
        return;
    }

    if(covenant_notice_timer > 0) {
        covenant_notice_timer -= 1;
    }

    if(is_title() && test_mode_notice_timer > 0) {
        test_mode_notice_timer -= 1;
        if(test_mode_notice_timer == 0) {
            begin_new_game_plus(true);
        }
        profiler_end(update);
        return;
    }

    // cancel if freeze
    if(freeze > 0) {
        freeze -= 1;
        profiler_end(update);
        return;
    }

    if(new_game_plus_available() && btn(k_jump)) {
        begin_new_game_plus(false);
        profiler_end(update);
        return;
    }

    // screenshake
    if(shake > 0) {
        shake -= 1;
        camera();
        if(shake > 0) {
            camera(-2 + rnd(5), -2 + rnd(5));
        }
    }

    // restart (soon)
    if(will_restart and delay_restart > 0) {
        delay_restart -= 1;
        if(delay_restart <= 0) {
            will_restart = false;
            load_room(room.x, room.y);
        }
    }

    profiler_add(obj_update);
    // update each object
    for(size_t i = 0; i < objects.size();) {
        Object *obj = objects[i];
        obj->move(obj->spd.x, obj->spd.y);
        obj->update();
        if(i < objects.size() && objects[i] == obj) i++;
    }
    profiler_end(obj_update);

    // start game
    if(is_title()) {
        if(not start_game and (btn(k_jump) or btn(k_dash))) {
            //music(-1);
            start_game_flash = 50;
            start_game = true;
            //sfx(38);
        }
        if(start_game) {
            start_game_flash -= 1;
            if(start_game_flash <= -30) {
                begin_game();
            }
        }
    }
    profiler_end(update);
}

// drawing functions //
///////////////////////
void _draw() {
    if(freeze > 0) return;
    profiler_add(draw);

    // reset all palette values
    pal();

    // start game flash
    if(start_game) {
        int c = 10;
        if(start_game_flash > 10) {
            if(frames % 10 < 5) {
                c = 7;
            }
        } else if(start_game_flash > 5) {
            c = 2;
        } else if(start_game_flash > 0) {
            c = 1;
        } else {
            c = 0;
        }
        if(c < 10) {
            pal(6, c, 1);
            pal(12, c, 1);
            pal(13, c, 1);
            pal(5, c, 1);
            pal(1, c, 1);
            pal(7, c, 1);
        }
    }

    // clear screen
    int bg_col = 0;
    if(flash_bg) {
        bg_col = frames / 5;
    } else if(new_bg) {
        bg_col = 2;
    }
    rectfill(0, 0, 128, 128, bg_col);

    profiler_add(clouds);
    // clouds
    if(not is_title()) {
        for(Cloud &c: clouds) {
            c.x += c.spd;
            rectfill(c.x, c.y, c.x + c.w, c.y + c.h, new_bg ? 14 : 1);
            if(c.x > 128) {
                c.x = -c.w;
                c.y = rnd(128 - 8);
            }
        }
    }
    profiler_end(clouds);

    profiler_add(tilemap_1);
    // draw bg terrain
    map(room.x * 16, room.y * 16, 0, 0, 2);
    profiler_end(tilemap_1);

    profiler_add(obj_draw);
    // platforms/big chest
    for(auto o: objects) {
        if(o->type == PLATFORM or o->type == BIG_CHEST) {
            o->draw();
        }
    }
    profiler_end(obj_draw);

    profiler_add(tilemap_2);
    // draw terrain
    int off = is_title() ? -4 : 0;
    map(room.x * 16, room.y * 16, off, 0, 1);
    profiler_end(tilemap_2);

    profiler_add(obj_draw);
    // draw objects
    for(auto o: objects) {
        if(o->type != PLATFORM and o->type != BIG_CHEST) {
            o->draw();
        }
    };
    profiler_end(obj_draw);

    profiler_add(tilemap_3);
    // draw fg terrain
    map(room.x * 16, room.y * 16, 0, 0, 3);
    profiler_end(tilemap_3);

    profiler_add(particles);
    // particles
    for(Particle &p : particles) {
        p.x += p.spd;
        p.y += sin(p.off);
        p.off += min(UINT24_MAX / 20, p.spd * (1 << 11));
        rectfill(div256_24(p.x), div256_24(p.y), div256_24(p.x + p.s), div256_24(p.y + p.s), p.c);
        if(p.x > SP(128 + 4)) {
            p.x = SP(-4);
            p.y = rnd(SP(128));
        }
    }

    // dead particles
    if(dead_particle_timer) {
        for(DeadParticle &p: dead_particles) {
            p.x += p.spd.x;
            p.y += p.spd.y;
            rectfill(PIX(p.x) - dead_particle_timer / 5, PIX(p.y) - dead_particle_timer / 5,
                     PIX(p.x) + dead_particle_timer / 5, PIX(p.y) + dead_particle_timer / 5,
                     14 + dead_particle_timer % 2);
        }
        dead_particle_timer--;
    }
    profiler_end(particles);

    // credits
    if(is_title()) {
        print("2nd+alpha", 46, 72, 5);
        print("mode: custom levels", 29, 78, 5);
        print("maddy thorson", 40, 90, 5);
        print("noel berry", 46, 96, 5);
        print("ce port:", 48, 108, 5);
        print("john cesarz", 42, 114, 5);
    }

    if(climb_enabled && !is_title() && (kb_IsDown(kb_KeyMath) || climb_stamina < CLIMB_STAMINA_MAX)) {
        rectfill(104, 4, 124, 8, 0);
        const int width = climb_stamina * 19 / CLIMB_STAMINA_MAX;
        if(width > 0) rectfill(105, 5, 105 + width - 1, 7, climb_stamina < 275 ? 8 : 11);
    }

    if(test_mode_notice_timer > 0) {
        rectfill(25, 56, 103, 65, 0);
        print("TEST MODE ENABLED", 29, 58, 11);
    }

    if(covenant_notice_timer > 0) {
        rectfill(16, 104, 111, 121, 0);
        print("The rainbow represents", 20, 107, 7);
        print("God's covenant promise.", 18, 113, 7);
    }

    if(level_index() == 30) {
        Object *p = nullptr;
        for(auto o: objects) {
            if(o->type == PLAYER) {
                p = o;
                break;
            }
        }
        if(p != nullptr) {
            int diff = min(24, 40 - abs(p->x + 4 - 64));
            rectfill(0, 0, diff, 128, 0);
            rectfill(128 - diff, 0, 128, 128, 0);
        }
    }
    custom_level_menu::draw();
    profiler_end(draw);
}

static void rotate_child_offset(int dx, int dy, uint8_t rotation, int &rx, int &ry) {
    switch(rotation & 0x03u) {
        case 1: rx = -dy; ry = dx; break;
        case 2: rx = -dx; ry = -dy; break;
        case 3: rx = dy; ry = -dx; break;
        default: rx = dx; ry = dy; break;
    }
}

static void spr_child_rot(uint8_t sprite, int x, int y, int dx, int dy,
                          uint8_t rotation, bool flip_x, bool flip_y) {
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

void draw_time(int x, int y) {
    int s = seconds;
    int m = minutes % 60;
    int h = minutes / 60;

    rectfill(x, y, x + 32, y + 6, 0);
    print_int(h, x + 1, y + 1, 7, 2);
    print(":");
    print_int(m, 2);
    print(":");
    print_int(s, 2);
}

// helper functions //
//////////////////////

int clamp(int val, int a, int b) {
    return max(a, min(b, val));
}

int appr(int val, int target, int amount) {
    return val > target
           ? max(val - amount, target)
           : min(val + amount, target);
}

int sign(int v) {
    return v > 0 ? 1 : v < 0 ? -1 : 0;
}

bool maybe() {
    return rand() & 1;
}

bool solid_at(int x, int y, int w, int h) {
    return tile_flag_at(x, y, w, h, 0);
}

bool ice_at(int x, int y, int w, int h) {
    return tile_flag_at(x, y, w, h, 4);
}

bool tile_flag_at(int x, int y, int w, int h, uint8_t flag) {
    for(int i = max(0, x / 8); i <= min(15, (x + w - 1) / 8); i++) {
        for(int j = max(0, y / 8); j <= min(15, (y + h - 1) / 8); j++) {
            if(fget(tile_at(i, j), flag)) {
                return true;
            }
        }
    }
    return false;
}

uint8_t tile_at(int x, int y) {
    return mget(room.x * 16 + x, room.y * 16 + y);
}

bool spikes_at(int x, int y, int w, int h, subpixel xspd, subpixel yspd) {
    for(int i = max(0, x / 8); i <= min(15, (x + w - 1) / 8); i++) {
        for(int j = max(0, y / 8); j <= min(15, (y + h - 1) / 8); j++) {
            uint8_t tile = tile_at(i, j);
            if(custom_levels::active()) {
                const uint8_t rot = custom_levels::tile_rotation(custom_levels::room_index(), i, j);
                int dir = tile == 17 ? 0 : tile == 59 ? 1 : tile == 27 ? 2 : tile == 43 ? 3 : -1;
                if(dir >= 0) {
                    dir = (dir + rot) & 3;
                    tile = dir == 0 ? 17 : dir == 1 ? 59 : dir == 2 ? 27 : 43;
                }
            }
            if(tile == 17 and ((y + h - 1) % 8 >= 6 or y + h == j * 8 + 8) and yspd >= 0) {
                return true;
            } else if(tile == 27 and y % 8 <= 2 and yspd <= 0) {
                return true;
            } else if(tile == 43 and x % 8 <= 2 and xspd <= 0) {
                return true;
            } else if(tile == 59 and ((x + w - 1) % 8 >= 6 or x + w == i * 8 + 8) and xspd >= 0) {
                return true;
            }
        }
    }
    return false;
}

bool needs_save() {
    return !custom_levels::active() && !test_mode && !is_title() && level_index() != 30;
}

#define TO_SERIALIZE(F) \
    F(frames)           \
    F(deaths)           \
    F(max_dash)         \
    F(new_bg)           \
    F(seconds)          \
    F(minutes)          \
    F(got_fruit)        \
    F(room.x)           \
    F(room.y)           \
    F(practice_mode)    \
    F(new_game_plus)    \

#define SERIALIZE(var) fread(&(var), sizeof(var), 1, f);
void load_save(FILE *f) __attribute__ ((optnone)) {
    TO_SERIALIZE(SERIALIZE)
    if(new_game_plus > 1) new_game_plus = 1;
    if(level_index() > 31) {
        title_screen();
        return;
    }
    load_room(room.x, room.y);
}

#define DESERIALIZE(var) fwrite(&(var), sizeof(var), 1, f);
void store_save(FILE *f) __attribute__ ((optnone)) {
    TO_SERIALIZE(DESERIALIZE)
}

uint8_t div256_24_buf[4];
