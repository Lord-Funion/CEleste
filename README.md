# CEleste for TI-84 Plus CE

## Custom Levels v1.0.0

This repository contains the complete v1.0.0 public half of the CEleste custom-level ecosystem:

- a versioned, checksummed `CELV` level format;
- `.8xv` AppVar compatibility code;
- single-level and ordered level-pack loading;
- a Custom Levels browser opened from the title screen with `MODE`;
- a free calculator-native editor in `tools/calculator-editor`;
- complete logical gameplay-piece support rather than loose internal sprite fragments;
- arbitrary 0°/90°/180°/270° rotation for custom terrain and gameplay graphics;
- the complete binary format specification;
- GitHub Actions builds for both calculator executables.

The original built-in Celeste Classic campaign remains compiled into the game. Custom levels are read from separate AppVars whose data begins with `CELV`.

## v1.0.0 calculator binaries

GitHub Actions builds both programs with CEdev v15.0:

- `CEleste-v1.0.0.8xp` — the game with custom-level loading;
- `CELEDIT-v1.0.0.8xp` — the free calculator-native level editor.

The v1 workflow also publishes `SHA256SUMS.txt` with each artifact.

## Install and play custom levels

1. Transfer `CEleste-v1.0.0.8xp` to the calculator.
2. Transfer one or more compatible `.8xv` files with TI Connect CE.
3. Start CEleste and press `MODE` on the title screen.
4. Use Up/Down to choose a level or pack and press `2nd` or `Alpha` to play.

The loader reads AppVars from RAM or archive. Malformed, truncated, unsupported, or checksum-invalid payloads are rejected before gameplay.

Custom rooms complete when Madeline climbs through the top edge, matching Celeste Classic. The summit flag is an optional summit/results object, not the ordinary room-completion trigger.

## Complete gameplay pieces

Custom levels use the original CEleste/Celeste Classic gameplay objects, including:

- player spawn;
- spikes;
- springs and falling floors;
- normal and flying strawberries;
- dash balloons;
- keys and locked chests;
- complete 16×16 fake walls;
- moving platforms;
- the complete memorial/message object;
- complete 16×16 big/dash-upgrade chests;
- the Climb Chest, which unlocks `MATH`-held wall grabbing/climbing with Celeste-style stamina;
- the summit flag.

Multi-sprite objects are represented as one logical entity. Authors do not need to place internal companion quadrants such as fake-wall, memorial, or big-chest sprite fragments manually.

CELV entity flags are consumed by the runtime. A locked chest or fake wall contains a strawberry by default; its option flag can make it empty. Big chests upgrade Madeline to two dashes by default and can be configured for a three-dash upgrade. Collected custom-room fruit state survives room deaths/restarts.

## Arbitrary rotation

Current editors export **CELV v2**. A piece keeps its original PICO-8 tile/entity ID and stores a separate two-bit clockwise quarter-turn value:

- `0` = 0°;
- `1` = 90°;
- `2` = 180°;
- `3` = 270°.

This means rotation does **not** require another sprite ID to exist. CEleste uses CEdev/graphx sprite transforms at runtime, so an ID 20 locked chest can be rendered sideways or upside-down while remaining the same functional locked chest. The key/chest relationship and strawberry option are therefore independent from its visual orientation.

Terrain rotations are stored in a packed per-cell rotation plane. Gameplay-entity rotations use bits 6–7 of the existing entity flag byte; bits 0–5 remain available for gameplay options. Directional spike collision follows the rotated visual direction. Other objects retain their normal gameplay behavior unless that behavior itself has explicit directional handling.

The runtime remains backwards-compatible with CELV v1 files; v1 rooms decode with every rotation set to 0°.

## Format

The specification is in [`docs/custom-level-format.md`](docs/custom-level-format.md). The format supports multiple 16×16 rooms, metadata, CEleste gameplay entity IDs and option flags, deterministic RLE terrain encoding, per-cell/entity rotation, CRC32, and nested ordered level packs.

Terrain and gameplay entities use separate planes when exported by current editors, preventing entity sprites from accidentally acting as terrain.

## Free calculator editor

The source is in `tools/calculator-editor`. With the CE C/C++ Toolchain installed:

```sh
cd tools/calculator-editor
make
```

CELEDIT exposes the original standalone Celeste Classic terrain/background/decor tile families plus complete logical gameplay pieces. `WINDOW` rotates the piece under the cursor another 90°. If the cursor is empty, `WINDOW` changes the 0°/90°/180°/270° orientation that will be used for the next placed piece. It works even when there is no counterpart tile ID because CELEDIT and CEleste use graphx sprite rotation.

Compound pieces such as fake walls, big chests, memorials, flying strawberries, balloons, and moving platforms are drawn as complete objects in the editor.

The program saves an editable draft in the `CELEDITS` AppVar and exports playable `CELV` AppVars that become `.8xv` files when copied to a computer. CELEDIT uses the original/default gameplay properties; the richer desktop Studio inspector can configure optional entity flags such as empty strawberry containers or three-dash big chests.

## CEleste Studio 1.0.0

**CEleste Studio 1.0.0** is the separate advanced computer editor. Its full UI source is intentionally stored in the private `Lord-Funion/CEleste-Studio` repository; this public repository contains only the open compatibility layer required for interoperability.

## Verification

GitHub Actions verify the public runtime by:

- generating the graphics with `convimg`;
- compiling the custom-level parser and runtime;
- compiling `CEleste-v1.0.0.8xp` with CEdev v15.0;
- compiling the complete-piece `CELEDIT-v1.0.0.8xp` with CEdev v15.0;
- generating and verifying release checksums.

The serializer/parser round-trip, pack, checksum, corruption, compound-piece, entity-flag, and rotation paths are also covered by the custom-level ecosystem tests. CEmu, TI Connect CE, and physical-calculator testing remain useful platform-validation steps, but the release is versioned as **1.0.0**, not alpha or beta.

This is unofficial community software and is not affiliated with or endorsed by Extremely OK Games, Maddy Thorson, Noel Berry, Texas Instruments, or the CE Programming Toolchain developers. Existing repository license and copyright notices remain in effect.
