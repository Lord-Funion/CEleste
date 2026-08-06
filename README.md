# CEleste for TI-84 Plus CE

This branch adds the public half of the CEleste custom-level ecosystem:

- a versioned, checksummed `CELV` level format;
- `.8xv` AppVar compatibility code;
- single-level and ordered level-pack loading;
- a Custom Levels browser opened from the title screen with `MODE`;
- a free calculator-native editor in `tools/calculator-editor`;
- the complete binary format specification.

The original built-in Celeste Classic campaign remains compiled into the game. Custom levels are read from separate AppVars whose data begins with `CELV`.

## Install and play custom levels

1. Build and transfer CEleste normally.
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

## CEleste Studio

**CEleste Studio** is the separate advanced computer editor. Its planned pricing is **$0.99 at launch** and **$1.99 normally**, as a one-time purchase. The paid editor's full UI source is intentionally not stored in this public repository; this repository contains the open compatibility layer required for interoperability.

## Status

The JavaScript serializer and portable C++ parser pass round-trip, checksum, pack, and corruption tests. The calculator-facing sources also pass host syntax compilation against API stubs. A real CEdev build, CEmu run, TI Connect CE transfer, and physical-calculator verification are still required before calling the feature hardware-validated.

This is unofficial community software and is not affiliated with or endorsed by Extremely OK Games, Maddy Thorson, Noel Berry, Texas Instruments, or the CE Programming Toolchain developers. Existing repository license and copyright notices remain in effect.
