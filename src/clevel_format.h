#pragma once

#include <cstddef>
#include <cstdint>

namespace clevel {

constexpr uint8_t FORMAT_VERSION = 1;
constexpr uint8_t KIND_LEVEL = 1;
constexpr uint8_t KIND_PACK = 2;
constexpr std::size_t HEADER_SIZE = 34;
constexpr std::size_t ROOM_TILE_COUNT = 256;
constexpr std::size_t MAX_ROOMS = 32;
constexpr std::size_t MAX_ENTITIES_PER_ROOM = 48;
constexpr std::size_t MAX_TITLE = 63;
constexpr std::size_t MAX_AUTHOR = 31;
constexpr std::size_t MAX_DESCRIPTION = 255;

struct Entity {
    uint8_t type;
    uint8_t x;
    uint8_t y;
    uint8_t flags;
};

struct Room {
    uint32_t id;
    uint8_t width;
    uint8_t height;
    uint8_t spawn_x;
    uint8_t spawn_y;
    uint8_t exit_x;
    uint8_t exit_y;
    uint8_t flags;
    uint8_t tiles[ROOM_TILE_COUNT];
    uint8_t entity_count;
    Entity entities[MAX_ENTITIES_PER_ROOM];
};

struct Level {
    uint32_t id;
    uint8_t difficulty;
    uint16_t flags;
    uint16_t min_game_version;
    char title[MAX_TITLE + 1];
    char author[MAX_AUTHOR + 1];
    char description[MAX_DESCRIPTION + 1];
    uint8_t room_count;
    Room rooms[MAX_ROOMS];
};

struct PayloadInfo {
    uint8_t kind;
    uint8_t version;
    uint16_t flags;
    uint32_t total_length;
    uint32_t id;
    uint16_t item_count;
    uint8_t difficulty;
    uint16_t min_game_version;
    char title[MAX_TITLE + 1];
    char author[MAX_AUTHOR + 1];
    char description[MAX_DESCRIPTION + 1];
};

enum class Error : uint8_t {
    None,
    Truncated,
    BadMagic,
    UnsupportedVersion,
    UnsupportedKind,
    LengthMismatch,
    ChecksumMismatch,
    MetadataTooLong,
    TooManyRooms,
    InvalidRoomSize,
    BadRle,
    TooManyEntities,
    RoomLengthMismatch,
    PackIndexOutOfRange,
    NestedKindMismatch,
    TrailingData
};

const char *error_string(Error error);
uint32_t crc32(const uint8_t *data, std::size_t size);
bool inspect(const uint8_t *data, std::size_t size, PayloadInfo &out, Error &error);
bool decode_level(const uint8_t *data, std::size_t size, Level &out, Error &error);
bool decode_pack_level(const uint8_t *data, std::size_t size, uint16_t level_index, Level &out, Error &error);

} // namespace clevel
