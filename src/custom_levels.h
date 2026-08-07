#pragma once

#include <cstddef>
#include <cstdint>
#include "clevel_format.h"

namespace custom_levels {

constexpr uint8_t MAX_CATALOG_ENTRIES = 24;

struct CatalogEntry {
    char variable_name[9];
    char title[clevel::MAX_TITLE + 1];
    char author[clevel::MAX_AUTHOR + 1];
    uint32_t id;
    uint16_t item_count;
    uint8_t kind;
    bool archived;
};

void initialize();
uint8_t scan();
uint8_t catalog_size();
const CatalogEntry *catalog_entry(uint8_t index);
bool load(uint8_t catalog_index, uint16_t pack_level_index = 0);
void unload();
bool active();
const clevel::Level *level();
uint8_t room_index();
bool set_room(uint8_t index);
bool next_room();
bool previous_room();
bool next_level();
uint16_t pack_level_index();
uint16_t generation();
uint8_t tile(uint8_t room, uint8_t x, uint8_t y);
const char *last_error();

} // namespace custom_levels
