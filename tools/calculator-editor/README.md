# CELEDIT — calculator editor

`CELEDIT` is the free TI-84 Plus CE editor companion.

Controls:

- arrows: move cursor
- Mode: cycle the complete Celeste Classic terrain/gameplay palette
- 2nd: place
- Alpha: erase
- Window: rotate a directional piece (or the selected palette piece)
- `+` / `-`: next/previous room
- Enter: add room
- Del: delete room
- Trace: undo
- Zoom: redo
- Stat: edit title and author through TI-OS input
- Graph: save draft and export an archived `CELV` AppVar
- Y=: help
- Clear: save draft and exit

The palette includes every standalone terrain/background/decoration family used by the original Celeste Classic map plus all gameplay entities. Multi-sprite gameplay objects are edited as complete logical pieces: fake walls render as their complete 16x16 block, big chests as the complete 16x16 chest, memorials as the complete 2x2 sign, moving platforms as both platform halves, and flying strawberries/balloons include their companion art.

Directional spikes and the original terrain variants that have genuine PICO-8 rotated counterparts can be rotated with Window. Moving platforms rotate between left- and right-moving versions. CELEDIT does not invent rotated art states that CEleste cannot reproduce.

Gameplay behavior is the same as CEleste/Celeste Classic: keys unlock locked chests, locked chests and fake walls contain strawberries by default, big chests grant the dash upgrade, strawberries stay collected across room deaths/restarts, and rooms complete by climbing through the top edge. The summit flag is optional and is not a room exit.

The draft is stored in `CELEDITS`. Exported playable data uses a `CL......` AppVar; transferring that AppVar to a computer with TI Connect CE creates an `.8xv` file.

The editor supports up to eight 16x16 rooms and a 64-change history. CEleste Studio remains the intended tool for level packs, bulk import, deeper validation, category filtering, compound-piece properties, and richer editing.
