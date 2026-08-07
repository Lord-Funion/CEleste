#!/usr/bin/env bash
set -euo pipefail

EDITOR_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$EDITOR_DIR/../.." && pwd)"

make -C "$ROOT_DIR" gfx
cp "$ROOT_DIR/src/gfx/atlas.c" "$EDITOR_DIR/src/pico8_atlas.c"
cp "$ROOT_DIR/src/gfx/mypalette.c" "$EDITOR_DIR/src/pico8_palette.c"

test -s "$EDITOR_DIR/src/pico8_atlas.c"
test -s "$EDITOR_DIR/src/pico8_palette.c"
