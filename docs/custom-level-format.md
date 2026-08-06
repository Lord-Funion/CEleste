# CEleste Custom-Level Format (`CELV`) v1

This document specifies the binary payload stored inside a TI-84 Plus CE AppVar. When transferred to a computer, the AppVar is represented by a standard `.8xv` file. The outer `.8xv` wrapper is not part of the `CELV` payload.

All integers are unsigned and little-endian. All text is printable ASCII. Exporters must reject or replace unsupported characters. Unless explicitly stated otherwise, lengths are byte counts.

## Design goals

- Deterministic output: unchanged projects produce byte-identical payloads.
- Bounds-checkable parsing on calculator hardware.
- Single levels and ordered level packs use the same container.
- Normal campaign data remains compiled into CEleste and is never overwritten.
- Unknown versions or malformed records are rejected before gameplay.
- The calculator can read the payload directly from RAM or archive through `fileioc`.

## Payload header

The fixed header is 34 bytes.

| Offset | Size | Field | Description |
|---:|---:|---|---|
| 0 | 4 | magic | ASCII `CELV` |
| 4 | 1 | version | `1` |
| 5 | 1 | kind | `1` = level, `2` = pack |
| 6 | 2 | flags | Reserved; write zero |
| 8 | 4 | total length | Entire payload length, including header |
| 12 | 4 | CRC32 | IEEE CRC32 of every byte after the 34-byte header |
| 16 | 4 | ID | Stable project, level, or pack identifier |
| 20 | 2 | item count | Rooms for a level; nested levels for a pack |
| 22 | 1 | difficulty | 0 unrated, 1 easy, 2 normal, 3 hard, 4 expert, 5 extreme |
| 23 | 1 | reserved | Write zero |
| 24 | 1 | title length | Maximum 63 |
| 25 | 1 | author length | Maximum 31 |
| 26 | 2 | description length | Maximum 255 |
| 28 | 2 | minimum game version | BCD-like major/minor, currently `0x0100` |
| 30 | 4 | reserved | Write zero |

Immediately after the header are the title, author, and description bytes, with no terminators.

## Level records

A level's item count is its room count. Version 1 requires at least one room and supports up to 32 in the portable format. The calculator runtime may enforce a lower limit.

Each room starts with a 2-byte `record length`. This length counts the bytes after the length field and allows a parser to skip a future extended record.

| Field | Size | Description |
|---|---:|---|
| width | 1 | Version 1 runtime requires 16 |
| height | 1 | Version 1 runtime requires 16 |
| spawn X/Y | 2 | Player-spawn cell |
| exit X/Y | 2 | Suggested room-exit cell; gameplay completion still requires leaving through the top |
| flags | 1 | Reserved |
| reserved | 1 | Zero |
| compressed tile length | 2 | Byte length of RLE stream |
| entity count | 2 | Number of entity records |
| room ID | 4 | Stable room identifier |
| tile RLE | variable | Count/value byte pairs |
| entities | `count x 4` | Type, X, Y, flags |

### Tile RLE

Tiles are flattened row-major. A 16x16 room expands to exactly 256 bytes. Each pair is a one-byte run count from 1 through 255 followed by a one-byte tile ID.

A decoder must reject zero-length runs, odd-length streams, overflow beyond the room area, or streams that do not expand to exactly 256 bytes.

### Entities

Entity records are four bytes: `type`, `x`, `y`, and `flags`. Version 1 uses the existing CEleste/PICO-8 tile IDs:

| ID | Object |
|---:|---|
| 1 | Player spawn; normally represented by spawn coordinates instead |
| 8 | Key |
| 11 | Moving platform, left |
| 12 | Moving platform, right |
| 18 | Spring |
| 20 | Chest |
| 22 | Dash balloon |
| 23 | Falling floor |
| 26 | Strawberry |
| 28 | Flying strawberry |
| 64 | Fake wall |
| 86 | Message |
| 96 | Big chest |
| 118 | Summit flag |

The calculator loader overlays the player spawn and entity records on the room's terrain tiles when CEleste requests a tile.

## Pack records

A pack's item count is its number of levels. After pack metadata, each nested level is encoded as a 4-byte nested payload length followed by one complete `CELV` level payload.

Nested pack payloads are invalid. Every nested level carries its own CRC32 and metadata. This permits extraction without rewriting the level.

## `.8xv` wrapper

The computer editor writes a standard TI-83/84 variable file:

- signature `**TI83F*`;
- secondary signature `1A 0A 00`;
- 42-byte comment;
- one variable entry with type `0x15` (AppVar);
- 8-character calculator variable name;
- archived flag `0x80` by default;
- AppVar data consisting of a 2-byte payload length followed by the `CELV` payload;
- 16-bit additive checksum over the TI data section.

The computer implementation validates the outer TI checksum and inner CRC32 independently.

## Variable naming

Recommended names are:

- single level: `CL` plus a stable base-36 ID, truncated to 8 characters;
- pack: `CP` plus a stable base-36 ID, truncated to 8 characters.

Names contain only uppercase ASCII letters and digits. Importers must not rely on the variable name for identity; use the payload ID.

## Limits and security

Parsers must reject:

- payloads shorter than 34 bytes;
- incorrect magic, unsupported version, or unsupported kind;
- total-length or CRC mismatch;
- metadata exceeding the documented limits;
- zero or excessive room counts;
- non-16x16 rooms in the current calculator runtime;
- invalid RLE;
- entities outside room bounds;
- excessive entity counts;
- truncated nested levels;
- trailing bytes not accounted for by records.

The current computer exporter uses a conservative 65,000-byte payload limit. A calculator build may impose a lower practical limit based on available RAM.
