# Asset swap manifest

The game runs with no extra downloads. Photographs in `dont-look/assets/` are the faces and must keep their names.

| File | Spec | Hook |
| --- | --- | --- |
| `assets/tiles/wood.png` | 64×64 PNG, seamless | `material('wood')` in `js/tex.mjs` |
| `assets/tiles/carpet.png` | 64×64 PNG, seamless | `material('carpet')` |
| `assets/tiles/tile.png` | 64×64 PNG, seamless | `material('tile')` |
| `assets/tiles/concrete.png` | 64×64 PNG, seamless | `material('concrete')` |
| `assets/tiles/grass.png` | 64×64 PNG, seamless | `material('grass')` |
| `assets/tiles/wall.png` | 64×64 PNG, seamless | `material('wall')` |

Until a PNG loads, `js/tex.mjs` draws the material. A 404 leaves the generated texture in place.

Faces, not swappable without changing who people are:

- `assets/chris.jpg` player
- `assets/mom.jpg` in the car
- `assets/her.jpg` turquoise shirt
- `assets/evan.jpg` the hat, the trail photo
- `assets/teammate.jpg` the bag photo
- `assets/basement.jpg` name scratched out
- `assets/grin.jpg` the only face the thing wears

Sound is synthesized in `js/audio.mjs`. There is no sample folder.
