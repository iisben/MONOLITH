# Sequel upgrade plan

The story is frozen. Notes, Mom's lines, inspections, item descriptions, on-screen sentences, quest gates, and endings stay word for word. This pass changes the machine around them.

## Art

Warm soot, amber light, and cold hall shadows. Floors and walls are generated 64px materials (wood, carpet, tile, concrete, grass, plaster) so the beam reads as a place. Real photographs stay the faces. Dust rides the flashlight. A light grade and grain sit on top and can be turned off.

## Audio

One graph with Master, Music, SFX, and Ambience buses, a compressor so stacks do not clip, and a short reverb send. Footsteps, hits, and UI clicks vary slightly in pitch. Each room leans the drone. Danger still raises the bed and the heartbeat.

## Play

Fixed 60Hz steps, a little acceleration, a short attack buffer, a visible enemy windup before the same damage, a brief hit-stop, and pooled sparks. Wake flags, keys, the four car parts, and the finale hold are unchanged.

## Interface

The same words, with bus sliders, a grade toggle, and a shake toggle. Reduced flicker still shortens flashes. The dock does not shrink off the screen.

## Swap hook

`assets/tiles/<name>.png` replaces a generated material when the file loads. See `ASSET_SWAP_MANIFEST.md`.
