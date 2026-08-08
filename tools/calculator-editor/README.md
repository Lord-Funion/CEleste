# CELEDIT — calculator-native Studio-style editor

`CELEDIT` is the free TI-84 Plus CE editor companion for CEleste. The current editor uses the same core authoring model as CEleste Studio instead of the old one-tile-at-a-time debug-style interface.

## Main editor

The main screen keeps the 16×16 room visible while showing the currently selected piece, tool, rotation, and gameplay options in a right-side inspector. The five graph-row keys act like a toolbar:

- `Y=` — cycle Pencil, Eraser, Flood Fill, and Eyedropper
- `Window` — rotate the next placement by 90°
- `Zoom` — open the categorized visual palette
- `Trace` — open the room browser/manager
- `Graph` — open the project/actions menu

Other main controls:

- arrows — move the map cursor
- `2nd` — use the current tool
- `Alpha` — quick erase regardless of the active tool
- `Stat` — open properties for the piece under the cursor, or the next placement when the cursor is empty
- `+` / `-` — next/previous room
- `Del` — undo
- `Enter` — redo
- `Clear` — save the draft and exit

## Palette

`Zoom` opens a full-screen visual palette instead of forcing authors to cycle blindly through IDs. The palette contains the original Celeste Classic standalone map pieces plus complete logical gameplay objects.

Categories are:

- All
- Terrain
- Ice
- Hazards
- Gameplay
- Background
- Decoration

Use the arrows to browse, `+` / `-` to change category, `2nd` to select a piece, `Window` to rotate the placement preview, and `Mode`/`Zoom` to return to the map.

Multi-sprite gameplay objects are represented as complete pieces: fake walls are complete 16×16 blocks, big chests are complete 16×16 chests, memorials are complete signs, moving platforms include both halves, and flying strawberries/balloons include their companion graphics.

## Tools

CELEDIT now has the same basic room-authoring tools expected from Studio:

- **Pencil** — place terrain, spawn, or gameplay entities
- **Eraser** — remove terrain or a complete logical entity
- **Flood Fill** — fill connected terrain while preserving rotation boundaries and gameplay entities
- **Eyedropper** — copy a tile/entity together with its rotation and gameplay options

Gameplay entities are stored separately from the terrain plane. They are no longer smuggled into the map as special tile IDs.

## Properties and gameplay options

`Stat` opens the properties screen. If the cursor is over an existing piece, the placed piece is edited directly; otherwise it edits the next placement.

Every graphical piece can use real CELV v2 rotation metadata at 0°, 90°, 180°, or 270°. CEleste renders that rotation with CEdev graphx on the calculator instead of requiring a different PICO-8 counterpart ID.

The calculator editor also exposes the gameplay properties that matter for complete puzzles:

- locked chest — contains a strawberry or is empty
- fake wall — contains a strawberry or is empty
- big/dash chest — upgrades to 2 or 3 dashes

Keys, locked chests, strawberries, fake walls, big chests, balloons, springs, falling floors, moving platforms, flying strawberries, memorials, and the summit flag remain real CEleste gameplay entities after export.

## Room manager

`Trace` opens a visual room browser with mini room previews. It supports:

- opening any room
- adding rooms
- duplicating rooms
- deleting rooms
- reordering rooms

The calculator editor supports up to **16 rooms** per level and **48 gameplay entities per room**. Rooms complete by climbing through the top edge; the summit flag is optional and is not the normal exit.

## Project menu

`Graph` opens a proper actions menu containing undo, redo, project details, save, export, new-project, help, and exit actions. Project details include level title, author, and description. CELV v2 export preserves the project metadata, terrain rotation plane, entity rotation/options, and room order.

The editable draft is stored in the `CELEDITS` AppVar. Existing drafts from the previous 8-room tile/entity layout are migrated into the new entity-based project model when possible. Exported playable data uses a `CL......` AppVar; transferring it to a computer with TI Connect CE creates an `.8xv` file.

## Studio vs. CELEDIT

CELEDIT is no longer intentionally missing basic level-building mechanics. CEleste Studio is still substantially more convenient for mouse editing, the larger desktop workspace, pack management, `.celproj` files, `.8xv` import, browser preview/playtesting, deeper validation, and bulk editing. The calculator editor exists so a complete playable level can be authored directly on TI-84 Plus CE hardware without being reduced to a primitive tile cycler.
