#include "clevel_format.h"

#include <cstring>

namespace clevel {
namespace {

class Reader {
public:
    Reader(const uint8_t *data, std::size_t size) : data_(data), size_(size), pos_(0) {}
    bool take_u8(uint8_t &value) { if (!require(1)) return false; value = data_[pos_++]; return true; }
    bool take_u16(uint16_t &value) { if (!require(2)) return false; value = static_cast<uint16_t>(data_[pos_] | (data_[pos_ + 1] << 8)); pos_ += 2; return true; }
    bool take_u32(uint32_t &value) { if (!require(4)) return false; value = static_cast<uint32_t>(data_[pos_]) | (static_cast<uint32_t>(data_[pos_+1]) << 8) | (static_cast<uint32_t>(data_[pos_+2]) << 16) | (static_cast<uint32_t>(data_[pos_+3]) << 24); pos_ += 4; return true; }
    bool take_bytes(uint8_t *target, std::size_t count) { if (!require(count)) return false; std::memcpy(target, data_ + pos_, count); pos_ += count; return true; }
    bool skip(std::size_t count) { if (!require(count)) return false; pos_ += count; return true; }
    bool slice(std::size_t count, const uint8_t *&ptr) { if (!require(count)) return false; ptr = data_ + pos_; pos_ += count; return true; }
    std::size_t pos() const { return pos_; }
    std::size_t remaining() const { return size_ - pos_; }
private:
    bool require(std::size_t count) const { return count <= size_ - pos_; }
    const uint8_t *data_;
    std::size_t size_;
    std::size_t pos_;
};

bool read_text(Reader &reader, char *target, std::size_t capacity, uint16_t length, Error &error) {
    if (length >= capacity) { error = Error::MetadataTooLong; return false; }
    if (!reader.take_bytes(reinterpret_cast<uint8_t *>(target), length)) { error = Error::Truncated; return false; }
    target[length] = '\0'; return true;
}

bool read_header(const uint8_t *data, std::size_t size, PayloadInfo &out, Reader &reader, Error &error) {
    if (size < HEADER_SIZE) { error = Error::Truncated; return false; }
    uint8_t magic[4];
    if (!reader.take_bytes(magic, 4)) { error = Error::Truncated; return false; }
    if (std::memcmp(magic, "CELV", 4) != 0) { error = Error::BadMagic; return false; }
    if (!reader.take_u8(out.version)) { error = Error::Truncated; return false; }
    if (out.version != FORMAT_VERSION) { error = Error::UnsupportedVersion; return false; }
    if (!reader.take_u8(out.kind)) { error = Error::Truncated; return false; }
    if (out.kind != KIND_LEVEL && out.kind != KIND_PACK) { error = Error::UnsupportedKind; return false; }
    uint32_t checksum; uint8_t reserved8; uint8_t title_len; uint8_t author_len; uint16_t description_len; uint32_t reserved32;
    if (!reader.take_u16(out.flags) || !reader.take_u32(out.total_length) || !reader.take_u32(checksum) ||
        !reader.take_u32(out.id) || !reader.take_u16(out.item_count) || !reader.take_u8(out.difficulty) ||
        !reader.take_u8(reserved8) || !reader.take_u8(title_len) || !reader.take_u8(author_len) ||
        !reader.take_u16(description_len) || !reader.take_u16(out.min_game_version) || !reader.take_u32(reserved32)) {
        error = Error::Truncated; return false;
    }
    if (out.total_length != size) { error = Error::LengthMismatch; return false; }
    if (crc32(data + HEADER_SIZE, size - HEADER_SIZE) != checksum) { error = Error::ChecksumMismatch; return false; }
    if (!read_text(reader, out.title, sizeof out.title, title_len, error) ||
        !read_text(reader, out.author, sizeof out.author, author_len, error) ||
        !read_text(reader, out.description, sizeof out.description, description_len, error)) return false;
    error = Error::None; return true;
}

bool decode_room(Reader &reader, Room &room, Error &error) {
    uint16_t record_length;
    if (!reader.take_u16(record_length)) { error = Error::Truncated; return false; }
    const std::size_t record_end = reader.pos() + record_length;
    if (record_end < reader.pos() || record_end > reader.pos() + reader.remaining()) { error = Error::Truncated; return false; }
    uint8_t reserved; uint16_t tile_length; uint16_t entity_count;
    if (!reader.take_u8(room.width) || !reader.take_u8(room.height) ||
        !reader.take_u8(room.spawn_x) || !reader.take_u8(room.spawn_y) ||
        !reader.take_u8(room.exit_x) || !reader.take_u8(room.exit_y) ||
        !reader.take_u8(room.flags) || !reader.take_u8(reserved) ||
        !reader.take_u16(tile_length) || !reader.take_u16(entity_count) || !reader.take_u32(room.id)) {
        error = Error::Truncated; return false;
    }
    if (room.width != 16 || room.height != 16) { error = Error::InvalidRoomSize; return false; }
    if (entity_count > MAX_ENTITIES_PER_ROOM) { error = Error::TooManyEntities; return false; }
    const uint8_t *rle;
    if (!reader.slice(tile_length, rle)) { error = Error::Truncated; return false; }
    if (tile_length & 1u) { error = Error::BadRle; return false; }
    std::size_t output = 0;
    for (std::size_t i = 0; i < tile_length; i += 2) {
        const uint8_t count = rle[i], value = rle[i + 1];
        if (!count || output + count > ROOM_TILE_COUNT) { error = Error::BadRle; return false; }
        std::memset(room.tiles + output, value, count); output += count;
    }
    if (output != ROOM_TILE_COUNT) { error = Error::BadRle; return false; }
    room.entity_count = static_cast<uint8_t>(entity_count);
    for (uint16_t i = 0; i < entity_count; ++i) {
        if (!reader.take_u8(room.entities[i].type) || !reader.take_u8(room.entities[i].x) ||
            !reader.take_u8(room.entities[i].y) || !reader.take_u8(room.entities[i].flags)) {
            error = Error::Truncated; return false;
        }
        if (room.entities[i].x >= 16 || room.entities[i].y >= 16) { error = Error::InvalidRoomSize; return false; }
    }
    if (reader.pos() != record_end) { error = Error::RoomLengthMismatch; return false; }
    return true;
}

bool decode_level_after_header(Reader &reader, const PayloadInfo &info, Level &out, Error &error) {
    if (info.kind != KIND_LEVEL) { error = Error::NestedKindMismatch; return false; }
    if (info.item_count == 0 || info.item_count > MAX_ROOMS) { error = Error::TooManyRooms; return false; }
    out.id = info.id; out.difficulty = info.difficulty; out.flags = info.flags; out.min_game_version = info.min_game_version;
    std::strcpy(out.title, info.title); std::strcpy(out.author, info.author); std::strcpy(out.description, info.description);
    out.room_count = static_cast<uint8_t>(info.item_count);
    for (uint16_t i = 0; i < info.item_count; ++i) if (!decode_room(reader, out.rooms[i], error)) return false;
    if (reader.remaining() != 0) { error = Error::TrailingData; return false; }
    error = Error::None; return true;
}

} // namespace

uint32_t crc32(const uint8_t *data, std::size_t size) {
    uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & static_cast<uint32_t>(-static_cast<int32_t>(crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

bool inspect(const uint8_t *data, std::size_t size, PayloadInfo &out, Error &error) {
    std::memset(&out, 0, sizeof out); Reader reader(data, size); return read_header(data, size, out, reader, error);
}

bool decode_level(const uint8_t *data, std::size_t size, Level &out, Error &error) {
    std::memset(&out, 0, sizeof out); PayloadInfo info{}; Reader reader(data, size);
    if (!read_header(data, size, info, reader, error)) return false;
    return decode_level_after_header(reader, info, out, error);
}

bool decode_pack_level(const uint8_t *data, std::size_t size, uint16_t level_index, Level &out, Error &error) {
    std::memset(&out, 0, sizeof out); PayloadInfo pack{}; Reader reader(data, size);
    if (!read_header(data, size, pack, reader, error)) return false;
    if (pack.kind != KIND_PACK) { error = Error::UnsupportedKind; return false; }
    if (level_index >= pack.item_count) { error = Error::PackIndexOutOfRange; return false; }
    for (uint16_t i = 0; i < pack.item_count; ++i) {
        uint32_t length; if (!reader.take_u32(length)) { error = Error::Truncated; return false; }
        const uint8_t *nested; if (!reader.slice(length, nested)) { error = Error::Truncated; return false; }
        if (i == level_index) return decode_level(nested, length, out, error);
    }
    error = Error::PackIndexOutOfRange; return false;
}

const char *error_string(Error error) {
    switch (error) {
        case Error::None: return "no error";
        case Error::Truncated: return "truncated data";
        case Error::BadMagic: return "bad CELV magic";
        case Error::UnsupportedVersion: return "unsupported format version";
        case Error::UnsupportedKind: return "unsupported payload kind";
        case Error::LengthMismatch: return "payload length mismatch";
        case Error::ChecksumMismatch: return "CRC32 mismatch";
        case Error::MetadataTooLong: return "metadata exceeds calculator limits";
        case Error::TooManyRooms: return "invalid room count";
        case Error::InvalidRoomSize: return "invalid room dimensions or coordinates";
        case Error::BadRle: return "invalid room RLE";
        case Error::TooManyEntities: return "too many entities in room";
        case Error::RoomLengthMismatch: return "room record length mismatch";
        case Error::PackIndexOutOfRange: return "pack level index out of range";
        case Error::NestedKindMismatch: return "pack record is not a level";
        case Error::TrailingData: return "unexpected trailing data";
    }
    return "unknown error";
}

} // namespace clevel
