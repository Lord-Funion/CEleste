# CEleste for TI-84 Plus CE

## Custom Levels v1.0.0

This repository contains the complete v1.0.0 public half of the CEleste custom-level ecosystem:

- a versioned, checksummed `CELV` level format;
- `.8xv` AppVar compatibility code;
- single-level and ordered level-pack loading;
- a Custom Levels browser opened from the title screen with `MODE`;
- a free calculator-native editor in `tools/calculator-editor`;
- complete logical gameplay-piece support rather than loose internal sprite fragments;
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
- four spike directions;
- springs and falling floors;
- normal and flying strawberries;
- dash balloons;
- keys and locked chests;
- complete 16×16 fake walls;
- left/right moving platforms;
- the complete memorial/message object;
- complete 16×16 big/dash-upgrade chests;
- the summit flag.

Multi-sprite objects are represented as one logical entity. Authors do not need to place internal companion quadrants such as fake-wall, memorial, or big-chest sprite fragments manually.

CELV entity flags are consumed by the runtime. A locked chest or fake wall contains a strawberry by default; its option flag can make it empty. Big chests upgrade Madeline to two dashes by default and can be configured for a three-dash upgrade. Collected custom-room fruit state survives room deaths/restarts.

## Format

The specification is in [`docs/custom-level-format.md`](docs/custom-level-format.md). The format supports multiple 16×16 rooms, metadata, CEleste gameplay entity IDs and option flags, deterministic RLE terrain encoding, CRC32, and nested ordered level packs.

Terrain and gameplay entities use separate planes when exported by current editors, preventing entity sprites from accidentally acting as terrain.

## Free calculator editor

The source is in `tools/calculator-editor`. With the CE C/C++ Toolchain installed:

```sh
cd tools/calculator-editor
make
```

CELEDIT exposes the original standalone Celeste Classic terrain/background/decor tile families plus complete logical gameplay pieces. `WINDOW` rotates pieces that have genuine in-game rotation/direction counterparts, including all four spike directions and left/right moving platforms. Compound pieces such as fake walls, big chests, memorials, flying strawberries, balloons, and moving platforms are drawn as complete objects in the editor.

The program saves an editable draft in the `CELEDITS` AppVar and exports playable `CELV` AppVars that become `.8xv` files when copied to a computer. CELEDIT uses the original/default gameplay properties; the richer desktop Studio inspector can configure optional entity flags such as empty strawberry containers or three-dash big chests.

## CEleste Studio 1.0.0

**CEleste Studio 1.0.0** is the separate advanced computer editor. Its planned pricing is **$0.99 at launch** and **$1.99 normally**, as a one-time purchase. The paid editor's full UI source is intentionally stored in the private `Lord-Funion/CEleste-Studio` repository; this public repository contains only the open compatibility layer required for interoperability.

## Verification

For v1.0.0, GitHub Actions successfully:

- generated the graphics with `convimg`;
- compiled the custom-level parser and runtime;
- compiled `CEleste-v1.0.0.8xp` with CEdev v15.0;
- compiled the complete-piece `CELEDIT-v1.0.0.8xp` with CEdev v15.0;
- generated and verified release checksums.

The serializer/parser round-trip, pack, checksum, corruption, compound-piece, and entity-flag tests also pass. CEmu, TI Connect CE, and physical-calculator testing remain useful platform-validation steps, but the release is versioned as **1.0.0**, not alpha or beta.

This is unofficial community software and is not affiliated with or endorsed by Extremely OK Games, Maddy Thorson, Noel Berry, Texas Instruments, or the CE Programming Toolchain developers. Existing repository license and copyright notices remain in effect.
