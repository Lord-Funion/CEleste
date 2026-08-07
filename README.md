# CEleste for TI-84 Plus CE

## Custom Levels v1.0.0

This branch contains the complete v1.0.0 public half of the CEleste custom-level ecosystem:

- a versioned, checksummed `CELV` level format;
- `.8xv` AppVar compatibility code;
- single-level and ordered level-pack loading;
- a Custom Levels browser opened from the title screen with `MODE`;
- a free calculator-native editor in `tools/calculator-editor`;
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

## Format

The specification is in [`docs/custom-level-format.md`](docs/custom-level-format.md). The format supports multiple 16x16 rooms, metadata, existing CEleste entity IDs, deterministic RLE tile encoding, CRC32, and nested ordered level packs.

## Free calculator editor

The source is in `tools/calculator-editor`. With the CE C/C++ Toolchain installed:

```sh
cd tools/calculator-editor
make
```

The program saves an editable draft in the `CELEDITS` AppVar and exports playable `CELV` AppVars that become `.8xv` files when copied to a computer.

## CEleste Studio 1.0.0

**CEleste Studio 1.0.0** is the separate advanced computer editor. Its planned pricing is **$0.99 at launch** and **$1.99 normally**, as a one-time purchase. The paid editor's full UI source is intentionally stored in the private `Lord-Funion/CEleste-Studio` repository; this public repository contains only the open compatibility layer required for interoperability.

## Verification

For v1.0.0, GitHub Actions successfully:

- generated the graphics with `convimg`;
- compiled the custom-level parser and runtime;
- compiled `CEleste-v1.0.0.8xp` with CEdev v15.0;
- compiled `CELEDIT-v1.0.0.8xp` with CEdev v15.0;
- generated and verified release checksums.

The serializer/parser round-trip, pack, checksum, and corruption tests also pass. CEmu, TI Connect CE, and physical-calculator testing remain useful platform-validation steps, but the release is versioned as **1.0.0**, not alpha or beta.

This is unofficial community software and is not affiliated with or endorsed by Extremely OK Games, Maddy Thorson, Noel Berry, Texas Instruments, or the CE Programming Toolchain developers. Existing repository license and copyright notices remain in effect.
