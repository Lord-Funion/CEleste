# CEleste Custom-Level Format (`CELV`) v2

This document specifies the binary payload stored inside a TI-84 Plus CE AppVar. When transferred to a computer, the AppVar is represented by a standard `.8xv` file. The outer `.8xv` wrapper is not part of the `CELV` payload.

All integers are unsigned and little-endian. All text is printable ASCII. Unless stated otherwise, lengths are byte counts.

**Compatibility:** current CEleste reads both CELV v1 and CELV v2. Current editors export v2. A v1 room is interpreted exactly as before with every tile/entity rotation set to 0°.

## Payload header

The fixed header remains 34 bytes.

| Offset | Size | Field | Description |
|---:|---:|---|---|
| 0 | 4 | magic | ASCII `CELV` |
| 4 | 1 | version | `2` for current exports; runtime also accepts `1` |
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
| 28 | 2 | minimum game version | Current v2 exporters write `0x0101` |
| 30 | 4 | reserved | Write zero |

Immediately after the header are title, author, and description bytes with no terminators.

## Level room records

A level contains 1–32 rooms. Current calculator gameplay requires 16×16 rooms.

Each room begins with a 2-byte `record length` counting the bytes after that length field.

| Field | Size | Description |
|---|---:|---|
| width | 1 | Current runtime requires 16 |
| height | 1 | Current runtime requires 16 |
| spawn X/Y | 2 | Player-spawn cell |
| exit X/Y | 2 | Legacy/suggested metadata; room completion is still crossing the top edge |
| flags | 1 | Reserved |
| rotation encoding | 1 | v2: `1` = packed 2-bits-per-cell rotation plane; `0` = all rotations zero |
| compressed tile length | 2 | Byte length of RLE stream |
| entity count | 2 | Number of entity records |
| room ID | 4 | Stable room identifier |
| tile RLE | variable | Count/value pairs |
| tile rotation plane | 64 bytes when encoding = 1 | Four 2-bit rotation values per byte |
| entities | `count × 4` | Type, X, Y, flags |

### Terrain tile plane

Tiles are flattened row-major. A 16×16 room expands to exactly 256 tile IDs. Each RLE pair is a one-byte run count (1–255) followed by a one-byte PICO-8 atlas tile ID.

Gameplay entities are not written into this plane by current editors.

### Tile rotation plane

CELV v2 stores rotation independently from tile ID. This is what allows **any atlas tile to rotate even when no separate counterpart ID exists**.

Each cell has a 2-bit clockwise quarter-turn value:

| Value | Rotation |
|---:|---:|
| 0 | 0° |
| 1 | 90° clockwise |
| 2 | 180° |
| 3 | 270° clockwise |

Four cells are packed into each byte. For flattened cell index `i`, its two bits are stored at:

`(rotationByte[i >> 2] >> ((i & 3) * 2)) & 3`

A 16×16 room therefore uses exactly 64 bytes. CEleste uses CEdev graphx rotation routines at runtime to transform the original atlas sprite; exporters do not need to manufacture new sprite IDs.

CELV v1 has no rotation plane and decodes as 256 zero rotation values.

## Entities

Each entity remains four bytes: `type`, `x`, `y`, `flags`.

Supported gameplay IDs include:

| ID | Object |
|---:|---|
| 8 | Key |
| 11 | Moving platform |
| 12 | Moving platform/right variant |
| 18 | Spring |
| 20 | Locked chest |
| 22 | Dash balloon |
| 23 | Falling floor |
| 26 | Strawberry |
| 28 | Flying strawberry |
| 64 | Fake wall |
| 86 | Memorial/message |
| 96 | Big/dash-upgrade chest |
| 118 | Summit flag |
| 129 | Climb Chest (unlocks MATH wall-grab/climbing with stamina) |
| 130 | Silver Key (linked gate key) |
| 131 | Silver Gate (solid 8×8 linked gate block) |

Multi-sprite entities remain one logical record. For example, one type-64 record renders the complete fake wall and one type-96 record renders the complete big chest. Silver gates are intentionally single-cell records so multiple type-131 entities with the same link group can be stacked into doors, portcullises, walls, or arbitrary keyed barriers.

### Entity flags and rotation

CELV v2 divides the existing flag byte into gameplay bits and rotation bits:

- bits **0–5** (`0x3F`) = gameplay options;
- bits **6–7** (`0xC0`) = clockwise quarter-turn rotation.

Rotation is decoded as `(flags >> 6) & 3`, using the same 0/90/180/270° values as terrain.

Gameplay options currently used:

| Entity | Gameplay bits | Meaning |
|---|---:|---|
| Locked chest (`20`) | 0 (`0x01`) | When set, the chest is empty and does not spawn a strawberry |
| Fake wall (`64`) | 0 (`0x01`) | When set, the wall is empty and does not spawn a strawberry |
| Big chest (`96`) | 1 (`0x02`) | When set, upgrade to three dashes instead of two |
| Silver Key (`130`) | 0–5 (`0x3F`) | Link group `0–63` unlocked when the key is collected |
| Silver Gate (`131`) | 0–5 (`0x3F`) | Link group `0–63`; disappears when that group is unlocked |

For example, a locked chest rotated 90° with its normal strawberry behavior uses flags `0x40`. An empty locked chest rotated 90° uses `0x41`. A Silver Gate in link group 12 rotated 90° uses flags `0x4C`.

The runtime masks rotation bits away before reading gameplay options, so rotating a chest does not change whether it requires a key or contains a strawberry, and rotating a silver key/gate does not change its link group.

### Silver key/gate linking

Silver linking is level-wide rather than room-local:

- collecting a Silver Key unlocks its six-bit link group for the remainder of the active custom level;
- every Silver Gate with the same link group disappears, including gates in other rooms;
- a collected Silver Key stays collected through room deaths/restarts;
- an unlocked link remains unlocked while changing rooms, but resets when a new custom level is loaded;
- multiple Silver Keys may target the same group, and any number of Silver Gate blocks may share that group.

This system is separate from the original yellow Key (`8`) and Locked Chest (`20`) mechanic.

## Gameplay semantics of rotation

All supported custom graphics can be rendered at the four quarter-turn orientations.

- Directional spike collision is rotated to match the rendered spike direction.
- Locked chests retain key/chest behavior at every orientation.
- Fake-wall strawberry options and big-chest dash-count options remain independent from orientation.
- Silver Keys retain their link group at every orientation. Silver Gate graphics rotate while the same link group continues to control collision/unlocking.
- Other objects keep their ordinary gameplay semantics unless the runtime explicitly defines a directional semantic for that object. Rotation can therefore be graphical for objects whose original mechanics are not orientation-dependent.
- The Climb Chest (`129`) is a custom power-up entity. Touching it unlocks modern-Celeste-style wall grabbing for the remainder of that custom level: hold `MATH` against a non-ice wall, use Up/Down to climb, and manage the 110-point stamina pool. Ground contact restores stamina.

Rooms complete only when Madeline exits through the top edge. The summit flag is not the standard room-completion trigger.

## Pack records

A pack's item count is its number of levels. After pack metadata, each nested level is encoded as a 4-byte payload length followed by one complete `CELV` level payload. Current exports nest v2 level payloads. The parser may also encounter v1 nested levels and handles them as unrotated.

## `.8xv` wrapper

The computer editor writes a standard TI-83/84 variable file with:

- signature `**TI83F*`;
- secondary signature `1A 0A 00`;
- one type-`0x15` AppVar entry;
- an 8-character variable name;
- archive flag `0x80` by default;
- two-byte AppVar payload length followed by the `CELV` payload;
- the standard 16-bit additive TI data-section checksum.

The outer TI checksum and inner CELV CRC32 are validated independently.

## Limits and validation

Current implementations reject or report:

- incorrect magic, unsupported versions/kinds, or truncation;
- total-length or CRC mismatch;
- metadata over documented limits;
- invalid room counts or non-16×16 calculator rooms;
- malformed RLE;
- invalid rotation encoding/data;
- more than 48 gameplay entities per room;
- compound pieces outside room bounds or overlapping another logical gameplay footprint;
- malformed nested pack entries or unexplained trailing bytes.

Studio additionally warns when a Silver Key link has no Silver Gate or a Silver Gate link has no Silver Key in the same level.

The Studio exporter uses a conservative 65,000-byte AppVar payload limit.