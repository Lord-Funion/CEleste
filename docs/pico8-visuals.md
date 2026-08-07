# PICO-8 visual parity

CEleste v1 uses the Celeste Classic/PICO-8 sprite atlas in both calculator-facing interfaces:

- CEleste gameplay renders from `src/gfx/atlas.png` through CEdev `convimg`.
- CELEDIT generates its own sprite table from the same atlas and displays the real 8x8 sprites for terrain, entities, player spawn, finish flag, and the currently selected palette item.
- CEleste Studio synchronizes the same atlas into its private repository and uses it for the desktop grid, palette, entities, and playable preview.

The Studio preview also mirrors the original Celeste Classic player constants and rules for 30 Hz updates, acceleration/deceleration, gravity, coyote time, jump buffering, wall sliding and wall jumps, eight-direction dashes, PICO-8 tile flags, spike collision, and room completion through the top edge.
