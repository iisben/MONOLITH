import { prepareDoor } from './doors.mjs';

// House geometry. Tile 0 is solid. 2–6 are floors. 9 is a door.
// Door frames, hinges, and swing pockets are fitted in doors.mjs.
// Run `node dont-look/test/reach.mjs` and `node dont-look/test/doors.mjs`
// after moving a room, door, or prop.

export const TILE = 32;
export const COLS = 32;
export const ROWS = 49;

// brief is the map label when the full name will not fit the room.
export const ROOMS = {
  her:      { x: 2,  y: 2,  w: 8,  h: 6, floor: 'carpet',   name: 'Her Room', brief: 'Her' },
  hallN:    { x: 11, y: 2,  w: 8,  h: 6, floor: 'wood',     name: 'Upper Hall', brief: 'Upper' },
  bath:     { x: 20, y: 2,  w: 8,  h: 6, floor: 'tile',     name: 'Bathroom', brief: 'Bath' },
  chris:    { x: 2,  y: 9,  w: 8,  h: 6, floor: 'carpet',   name: "Chris's Room", brief: 'Chris' },
  hallS:    { x: 11, y: 9,  w: 8,  h: 6, floor: 'wood',     name: 'Lower Hall', brief: 'Lower' },
  mom:      { x: 20, y: 9,  w: 8,  h: 6, floor: 'carpet',   name: "Mom's Room", brief: 'Mom' },
  living:   { x: 2,  y: 16, w: 15, h: 7, floor: 'wood',     name: 'Living Room', brief: 'Living' },
  dining:   { x: 18, y: 16, w: 10, h: 7, floor: 'wood',     name: 'Dining Room', brief: 'Dining' },
  kitchen:  { x: 2,  y: 24, w: 10, h: 6, floor: 'tile',     name: 'Kitchen' },
  foyer:    { x: 13, y: 24, w: 8,  h: 6, floor: 'wood',     name: 'Foyer' },
  mud:      { x: 22, y: 24, w: 6,  h: 6, floor: 'tile',     name: 'Mudroom', brief: 'Mud' },
  garage:   { x: 2,  y: 31, w: 16, h: 7, floor: 'concrete', name: 'Garage' },
  basement: { x: 19, y: 31, w: 11, h: 7, floor: 'concrete', name: 'Basement', brief: 'Basement' },
  yard:     { x: 2,  y: 39, w: 26, h: 8, floor: 'grass',    name: 'Yard' },
  shed:     { x: 21, y: 42, w: 5,  h: 3, floor: 'wood',     name: 'Shed', nest: true },
};

// open: starts open. lock: item id the door consumes, or 'power' for the fuse.
export const DOORS = [
  { id: 'her',   x: 10, y: 4,  lock: 'key-her',  open: false },
  { id: 'bath',  x: 19, y: 4,  lock: null,       open: true },
  { id: 'chris', x: 10, y: 11, lock: null,       open: true },
  { id: 'mom',   x: 19, y: 11, lock: null,       open: false },
  { id: 'hall',  x: 14, y: 8,  lock: null,       open: true },
  { id: 'liv-h', x: 14, y: 15, lock: null,       open: true },
  { id: 'din',   x: 17, y: 19, lock: null,       open: true },
  { id: 'kit-n', x: 6,  y: 23, lock: null,       open: true },
  { id: 'foy-n', x: 15, y: 23, lock: null,       open: true },
  { id: 'kit-e', x: 12, y: 26, lock: null,       open: true },
  { id: 'foy-e', x: 21, y: 26, lock: null,       open: true },
  { id: 'din-s', x: 24, y: 23, lock: null,       open: true },
  { id: 'gar-n', x: 16, y: 30, lock: null,       open: true },
  { id: 'gar-w', x: 6,  y: 30, lock: null,       open: true },
  { id: 'base',  x: 24, y: 30, lock: 'key-base', open: false },
  { id: 'yard',  x: 8,  y: 38, lock: 'power',    open: false, sticky: true },
  { id: 'shed',  x: 20, y: 43, lock: 'key-shed', open: false },
];

export const FURNITURE = [
  { id: 'her-bed',   x: 3,  y: 3,  w: 3, h: 2, kind: 'bed' },
  { id: 'tub',       x: 24, y: 3,  w: 3, h: 2, kind: 'tub' },
  { id: 'chris-bed', x: 3,  y: 10, w: 3, h: 2, kind: 'bed' },
  { id: 'mom-bed',   x: 23, y: 10, w: 4, h: 2, kind: 'sweater' },
  { id: 'couch',     x: 8,  y: 19, w: 5, h: 2, kind: 'couch' },
  { id: 'table',     x: 21, y: 18, w: 3, h: 2, kind: 'table' },
  { id: 'counter',   x: 2,  y: 25, w: 1, h: 4, kind: 'counter' },
  { id: 'car',       x: 8,  y: 33, w: 4, h: 2, kind: 'car' },
  { id: 'gbench',    x: 3,  y: 32, w: 2, h: 1, kind: 'bench' },
  { id: 'bbench',    x: 22, y: 33, w: 4, h: 1, kind: 'bench' },
  { id: 'tanks',     x: 27, y: 32, w: 2, h: 3, kind: 'tanks' },
  { id: 'shelf',     x: 23, y: 42, w: 2, h: 1, kind: 'shelf' },
];

export const SPAWN = { x: 24, y: 26 };

export const PROPS = [
  { id: 'duffel', x: 25, y: 28, kind: 'inspect', inspect: 'duffel', label: 'Your bag' },
  { id: 'front', x: 27, y: 26, kind: 'inspect', inspect: 'front', label: 'Front door' },
  { id: 'light', x: 16, y: 27, kind: 'flashlight', label: 'Flashlight' },
  { id: 'note-foyer', x: 15, y: 25, kind: 'note', note: 'foyer', label: "Mom's note" },
  { id: 'stick', x: 4, y: 21, kind: 'pickup', item: 'stick', label: 'Hockey stick' },
  { id: 'band-liv', x: 9, y: 21, kind: 'pickup', item: 'bandage', label: 'Bandage' },
  { id: 'family', x: 5, y: 16, kind: 'inspect', inspect: 'family', label: 'Family photo' },
  { id: 'tv', x: 4, y: 18, kind: 'inspect', inspect: 'tv', label: 'Television' },
  { id: 'bat-liv', x: 12, y: 17, kind: 'battery', label: 'Batteries' },
  { id: 'ammo-din', x: 20, y: 17, kind: 'pickup', item: 'ammo', count: 2, label: 'Rounds' },
  { id: 'knife', x: 4, y: 26, kind: 'pickup', item: 'knife', label: 'Kitchen knife' },
  { id: 'pills-kit', x: 3, y: 27, kind: 'pickup', item: 'pills', label: 'Painkillers' },
  { id: 'note-kit', x: 8, y: 25, kind: 'note', note: 'kitchen', label: 'Grocery list' },
  { id: 'bat-kit', x: 9, y: 29, kind: 'battery', label: 'Batteries' },
  { id: 'journal', x: 7, y: 6, kind: 'note', note: 'journal', label: 'Her journal' },
  { id: 'keys', x: 8, y: 5, kind: 'pickup', item: 'key-car', label: 'Car keys' },
  { id: 'herpic', x: 4, y: 6, kind: 'inspect', inspect: 'herpic', label: 'Her photo' },
  { id: 'bat-her', x: 6, y: 3, kind: 'battery', label: 'Batteries' },
  { id: 'cabinet', x: 21, y: 3, kind: 'container', label: 'Cabinet', items: [{ item: 'key-her' }, { item: 'bandage' }] },
  { id: 'mirror', x: 23, y: 2, kind: 'inspect', inspect: 'mirror', label: 'Mirror' },
  { id: 'note-chris', x: 7, y: 13, kind: 'note', note: 'hockey', label: 'Thursday note' },
  { id: 'film-chris', x: 8, y: 13, kind: 'pickup', item: 'film', label: 'Film slide' },
  { id: 'band-chris', x: 6, y: 12, kind: 'pickup', item: 'bandage', label: 'Bandage' },
  { id: 'ammo-chris', x: 8, y: 12, kind: 'pickup', item: 'ammo', count: 2, label: 'Rounds' },
  { id: 'bat-chris', x: 4, y: 13, kind: 'battery', label: 'Batteries' },
  { id: 'tape', x: 5, y: 12, kind: 'pickup', item: 'tape', label: 'Stick tape' },
  { id: 'diary', x: 22, y: 13, kind: 'note', note: 'diary', label: "Mom's diary" },
  { id: 'sweater', x: 21, y: 12, kind: 'pickup', item: 'key-base', label: 'Sweater pocket' },
  { id: 'mompic', x: 21, y: 10, kind: 'inspect', inspect: 'mompic', label: "Mom's photo" },
  { id: 'car', x: 12, y: 34, kind: 'car', label: 'The car' },
  { id: 'save', x: 5, y: 32, kind: 'save', label: 'Garage bench' },
  { id: 'stash', x: 3, y: 35, kind: 'stash', label: 'Parts bin' },
  { id: 'bag', x: 14, y: 35, kind: 'container', label: 'Gear bag', items: [{ note: 'bag' }, { item: 'pills' }, { item: 'film' }] },
  { id: 'board', x: 15, y: 32, kind: 'board', label: 'Checklist' },
  { id: 'ray', x: 21, y: 36, kind: 'note', note: 'ray', label: 'A scratched note' },
  { id: 'fuse', x: 21, y: 34, kind: 'pickup', item: 'fuse', label: 'Fuse' },
  { id: 'camera', x: 26, y: 34, kind: 'pickup', item: 'camera', label: "Ben's camera" },
  { id: 'shedkey', x: 23, y: 35, kind: 'pickup', item: 'key-shed', label: 'Shed key' },
  { id: 'gun', x: 26, y: 36, kind: 'pickup', item: 'pistol', label: 'Revolver' },
  { id: 'ammo-base', x: 24, y: 36, kind: 'pickup', item: 'ammo', count: 4, label: 'Rounds' },
  { id: 'film-base', x: 29, y: 36, kind: 'pickup', item: 'film', label: 'Film slide' },
  { id: 'band-base', x: 20, y: 32, kind: 'pickup', item: 'bandage', label: 'Bandage' },
  { id: 'bat-base', x: 29, y: 35, kind: 'battery', label: 'Batteries' },
  { id: 'badge', x: 29, y: 33, kind: 'inspect', inspect: 'badge', label: 'A scratched photo' },
  { id: 'tanks', x: 26, y: 32, kind: 'inspect', inspect: 'tanks', label: 'The tanks' },
  { id: 'yardnote', x: 6, y: 42, kind: 'inspect', inspect: 'yard', label: 'The yard' },
  { id: 'gas', x: 22, y: 43, kind: 'pickup', item: 'gas', label: 'Gas can' },
  { id: 'cloth', x: 24, y: 44, kind: 'inspect', inspect: 'cloth', label: 'Torn cloth' },
];

export const SPAWNS = [
  { id: 'pantry', kind: 'shambler', x: 10, y: 28, wake: 'near', wakeDist: 48, lightWake: true, patrol: [[10, 28], [9, 27]] },
  { id: 'momshape', kind: 'shambler', x: 25, y: 13, wake: 'flag', flag: 'tookBaseKey', silent: true, patrol: [[25, 13], [23, 12], [21, 12]] },
  { id: 'dining', kind: 'shambler', x: 26, y: 21, wake: 'flag', flag: 'tookCarKeys', patrol: [[26, 21], [24, 20], [23, 21]] },
  { id: 'base', kind: 'shambler', x: 20, y: 35, wake: 'now', patrol: [[20, 35], [20, 33], [21, 36]] },
  { id: 'yardbug', kind: 'shambler', x: 15, y: 44, wake: 'flag', flag: 'enteredYard', patrol: [[15, 44], [10, 42], [18, 41]] },
  { id: 'grin', kind: 'grin', x: 28, y: 36, wake: 'flag', flag: 'tookFuse', patrol: [[28, 36], [22, 36]] },
];

const FLOOR_ID = { wood: 2, carpet: 3, tile: 4, concrete: 5, grass: 6 };

export function createWorld() {
  const tiles = Array.from({ length: ROWS }, () => Array(COLS).fill(0));
  for (const room of Object.values(ROOMS)) {
    if (room.nest) continue;
    for (let y = room.y; y < room.y + room.h; y++) {
      for (let x = room.x; x < room.x + room.w; x++) tiles[y][x] = FLOOR_ID[room.floor];
    }
  }
  const shed = ROOMS.shed;
  for (let y = shed.y - 1; y <= shed.y + shed.h; y++) {
    for (let x = shed.x - 1; x <= shed.x + shed.w; x++) {
      const edge = x === shed.x - 1 || x === shed.x + shed.w || y === shed.y - 1 || y === shed.y + shed.h;
      if (edge) tiles[y][x] = 0;
    }
  }
  for (let y = shed.y; y < shed.y + shed.h; y++) {
    for (let x = shed.x; x < shed.x + shed.w; x++) tiles[y][x] = FLOOR_ID.wood;
  }

  const furniture = new Set();
  for (const piece of FURNITURE) {
    for (let y = piece.y; y < piece.y + piece.h; y++) {
      for (let x = piece.x; x < piece.x + piece.w; x++) furniture.add(`${x},${y}`);
    }
  }

  const doors = DOORS.map((d) => ({ ...d }));
  for (const door of doors) {
    tiles[door.y][door.x] = 9;
    for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
      const t = tiles[door.y + dy]?.[door.x + dx];
      if (t && t !== 9 && t !== 0) {
        door.floor = t;
        break;
      }
    }
    door.floor = door.floor || 2;
    prepareDoor(door, tiles, furniture);
  }

  const doorAt = new Map(doors.map((d) => [`${d.x},${d.y}`, d]));
  const pocketAt = new Map();
  for (const door of doors) {
    if (!door.pocket) continue;
    const key = `${door.pocket.x},${door.pocket.y}`;
    if (!pocketAt.has(key)) pocketAt.set(key, []);
    pocketAt.get(key).push(door);
  }
  return { tiles, doors, doorAt, pocketAt, furniture, props: PROPS, spawns: SPAWNS };
}

export function inBounds(tx, ty) {
  return tx >= 0 && ty >= 0 && tx < COLS && ty < ROWS;
}

export function roomAt(tx, ty) {
  for (const [id, room] of Object.entries(ROOMS)) {
    if (!room.nest) continue;
    if (tx >= room.x && ty >= room.y && tx < room.x + room.w && ty < room.y + room.h) return id;
  }
  for (const [id, room] of Object.entries(ROOMS)) {
    if (room.nest) continue;
    if (tx >= room.x && ty >= room.y && tx < room.x + room.w && ty < room.y + room.h) return id;
  }
  return null;
}

export function floorName(tile) {
  return { 2: 'wood', 3: 'carpet', 4: 'tile', 5: 'concrete', 6: 'grass', 9: 'wood' }[tile] || 'concrete';
}

// openDoor(door) => whether a traveler may cross. Closed-but-unlocked still counts:
// the player can open it. Furniture and walls never count.
export function passable(world, tx, ty, openDoor) {
  if (!inBounds(tx, ty)) return false;
  if (world.furniture.has(`${tx},${ty}`)) return false;
  const tile = world.tiles[ty][tx];
  if (tile === 0) return false;
  if (tile === 9) {
    const door = world.doorAt.get(`${tx},${ty}`);
    return openDoor(door);
  }
  return true;
}

export function bfs(world, openDoor, start = SPAWN) {
  const seen = new Set();
  const q = [[start.x, start.y]];
  seen.add(`${start.x},${start.y}`);
  while (q.length) {
    const [x, y] = q.shift();
    for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
      const nx = x + dx;
      const ny = y + dy;
      const key = `${nx},${ny}`;
      if (seen.has(key)) continue;
      if (!passable(world, nx, ny, openDoor)) continue;
      seen.add(key);
      q.push([nx, ny]);
    }
  }
  return seen;
}

export function ascii(world, openDoor) {
  let out = '';
  for (let y = 0; y < ROWS; y++) {
    let row = '';
    for (let x = 0; x < COLS; x++) {
      if (x === SPAWN.x && y === SPAWN.y) row += '@';
      else if (world.furniture.has(`${x},${y}`)) row += 'F';
      else if (world.tiles[y][x] === 9) {
        const door = world.doorAt.get(`${x},${y}`);
        row += openDoor(door) ? 'd' : 'D';
      } else if (world.tiles[y][x] === 0) row += '#';
      else row += '.';
    }
    out += row + '\n';
  }
  return out;
}
