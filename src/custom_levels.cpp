#include "custom_levels.h"

#include <cstring>
#include <fileioc.h>

namespace custom_levels {
namespace {
CatalogEntry catalog[MAX_CATALOG_ENTRIES]{};
uint8_t count = 0;
clevel::Level loaded{};
bool is_active = false;
uint8_t current_room = 0;
uint8_t active_catalog = 0;
uint16_t active_pack_level = 0;
uint16_t content_generation = 1;
bool collected_fruit[clevel::MAX_ROOMS]{}; // legacy v1 tile-plane compatibility
constexpr uint8_t SOURCE_BYTES = (clevel::MAX_ENTITIES_PER_ROOM + 7) / 8;
uint8_t collected_sources[clevel::MAX_ROOMS][SOURCE_BYTES]{};
char error_text[64] = "";

void set_error(const char *text) {
    std::strncpy(error_text, text ? text : "unknown error", sizeof error_text - 1);
    error_text[sizeof error_text - 1] = '\0';
}

bool inspect_variable(const char *name, CatalogEntry &entry) {
    const uint8_t handle = ti_OpenVar(name, "r", TI_APPVAR_TYPE);
    if (!handle) return false;
    const uint16_t size = ti_GetSize(handle);
    const auto *data = static_cast<const uint8_t *>(ti_GetDataPtr(handle));
    clevel::PayloadInfo info{}; clevel::Error error;
    const bool ok = data && clevel::inspect(data, size, info, error);
    if (ok) {
        std::strncpy(entry.variable_name, name, 8); entry.variable_name[8] = '\0';
        std::strncpy(entry.title, info.title, clevel::MAX_TITLE); entry.title[clevel::MAX_TITLE] = '\0';
        std::strncpy(entry.author, info.author, clevel::MAX_AUTHOR); entry.author[clevel::MAX_AUTHOR] = '\0';
        entry.id = info.id; entry.item_count = info.item_count; entry.kind = info.kind;
        entry.archived = ti_IsArchived(handle);
    }
    ti_Close(handle); return ok;
}
}

void initialize() { count = 0; is_active = false; current_room = 0; active_catalog = 0; active_pack_level = 0; content_generation = 1; std::memset(collected_fruit, 0, sizeof collected_fruit); std::memset(collected_sources, 0, sizeof collected_sources); error_text[0] = '\0'; }

uint8_t scan() {
    count = 0; void *vat = nullptr; char *name;
    while (count < MAX_CATALOG_ENTRIES && (name = ti_DetectVar(&vat, "CELV", TI_APPVAR_TYPE)) != nullptr) {
        if (inspect_variable(name, catalog[count])) ++count;
    }
    return count;
}

uint8_t catalog_size() { return count; }
const CatalogEntry *catalog_entry(uint8_t index) { return index < count ? &catalog[index] : nullptr; }

bool load(uint8_t catalog_index, uint16_t pack_level_index) {
    if (catalog_index >= count) { set_error("catalog index out of range"); return false; }
    const uint8_t handle = ti_OpenVar(catalog[catalog_index].variable_name, "r", TI_APPVAR_TYPE);
    if (!handle) { set_error("could not open AppVar"); return false; }
    const uint16_t size = ti_GetSize(handle); const auto *data = static_cast<const uint8_t *>(ti_GetDataPtr(handle));
    clevel::Error error; bool ok = false;
    if (data) ok = catalog[catalog_index].kind == clevel::KIND_PACK
        ? clevel::decode_pack_level(data, size, pack_level_index, loaded, error)
        : clevel::decode_level(data, size, loaded, error);
    else { error = clevel::Error::Truncated; }
    ti_Close(handle);
    if (!ok) { set_error(clevel::error_string(error)); is_active = false; return false; }
    current_room = 0; active_catalog = catalog_index; active_pack_level = pack_level_index; is_active = true;
    std::memset(collected_fruit, 0, sizeof collected_fruit);
    std::memset(collected_sources, 0, sizeof collected_sources);
    ++content_generation; if (!content_generation) content_generation = 1;
    error_text[0] = '\0'; return true;
}

void unload() { if (is_active) { ++content_generation; if (!content_generation) content_generation = 1; } is_active = false; current_room = 0; active_pack_level = 0; }
bool active() { return is_active; }
const clevel::Level *level() { return is_active ? &loaded : nullptr; }
uint8_t room_index() { return current_room; }
bool set_room(uint8_t index) { if (!is_active || index >= loaded.room_count) return false; current_room = index; return true; }
bool next_room() { return set_room(static_cast<uint8_t>(current_room + 1)); }
bool previous_room() { return current_room > 0 && set_room(static_cast<uint8_t>(current_room - 1)); }

bool next_level() {
    if (!is_active || active_catalog >= count || catalog[active_catalog].kind != clevel::KIND_PACK) return false;
    if (active_pack_level + 1 >= catalog[active_catalog].item_count) return false;
    return load(active_catalog, static_cast<uint16_t>(active_pack_level + 1));
}

uint16_t pack_level_index() { return active_pack_level; }
uint16_t generation() { return content_generation; }

uint8_t tile(uint8_t room, uint8_t x, uint8_t y) {
    if (!is_active || room >= loaded.room_count || x >= 16 || y >= 16) return 0;
    return loaded.rooms[room].tiles[y * 16 + x];
}

bool fruit_collected(uint8_t room) { return is_active && room < loaded.room_count && collected_fruit[room]; }
void collect_fruit(uint8_t room) { if (is_active && room < loaded.room_count) collected_fruit[room] = true; }

bool source_collected(uint8_t room, uint8_t source) {
    if (!is_active || room >= loaded.room_count) return false;
    if (source >= clevel::MAX_ENTITIES_PER_ROOM) return collected_fruit[room];
    return (collected_sources[room][source >> 3] & static_cast<uint8_t>(1u << (source & 7))) != 0;
}

void collect_source(uint8_t room, uint8_t source) {
    if (!is_active || room >= loaded.room_count) return;
    if (source >= clevel::MAX_ENTITIES_PER_ROOM) { collected_fruit[room] = true; return; }
    collected_sources[room][source >> 3] |= static_cast<uint8_t>(1u << (source & 7));
}

bool key_needed(uint8_t room) {
    if (!is_active || room >= loaded.room_count) return false;
    const clevel::Room &r = loaded.rooms[room];
    bool saw_chest = false;
    for (uint8_t i = 0; i < r.entity_count; ++i) {
        const clevel::Entity &entity = r.entities[i];
        if (entity.type != 20) continue;
        saw_chest = true;
        // Empty chests do not mark a collectible source, so they require a key
        // again after each death/restart. Strawberry chests disappear once
        // their own source strawberry has been collected.
        if ((entity.flags & 0x01u) != 0 || !source_collected(room, i)) return true;
    }
    // A deliberately placed key with no chest is still allowed to appear.
    return !saw_chest;
}

const char *last_error() { return error_text; }

} // namespace custom_levels
