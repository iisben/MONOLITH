import fs from 'fs';
import { describeGoal } from '../js/data.mjs';
import { COLS, PROPS, ROOMS, SPAWN, SPAWNS, ascii, bfs, createWorld, inBounds, passable, roomAt } from '../js/world.mjs';

const world = createWorld();
const problems = [];

function fail(msg) {
  problems.push(msg);
}

const always = () => true;
const locked = (door) => !door.lock;
const withKeys = (ids) => (door) => !door.lock || ids.has(door.lock) || (door.lock === 'power' && ids.has('power'));

for (const door of world.doors) {
  const floors = [];
  for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
    const x = door.x + dx;
    const y = door.y + dy;
    if (!inBounds(x, y)) continue;
    const tile = world.tiles[y][x];
    if (tile && tile !== 0 && tile !== 9) floors.push({ x, y, dx, dy });
  }
  if (floors.length !== 2) fail(`door ${door.id} has ${floors.length} floor sides`);
  else if (floors[0].dx + floors[1].dx !== 0 || floors[0].dy + floors[1].dy !== 0) {
    fail(`door ${door.id} sides are not opposite`);
  }
  for (const side of floors) {
    if (world.furniture.has(`${side.x},${side.y}`)) fail(`furniture blocks door ${door.id}`);
  }
  if (door.x < 0 || door.y < 0 || door.x >= COLS) fail(`door ${door.id} out of bounds`);
}

for (const prop of PROPS) {
  if (!passable(world, prop.x, prop.y, always)) fail(`prop ${prop.id} is not walkable`);
  if (!roomAt(prop.x, prop.y)) fail(`prop ${prop.id} is outside every room`);
}

for (const spawn of SPAWNS) {
  if (!passable(world, spawn.x, spawn.y, always)) fail(`spawn ${spawn.id} is not walkable`);
  for (const [x, y] of spawn.patrol) {
    if (!passable(world, x, y, always)) fail(`patrol ${spawn.id} ${x},${y} is blocked`);
  }
}

for (const piece of world.furniture ? [] : []) {
  /* furniture set has no kinds; checked via props */
}

const early = bfs(world, locked);
const mustEarly = ['light', 'stick', 'note-foyer', 'car', 'save', 'bag', 'cabinet', 'sweater', 'diary', 'knife', 'family'];
const mustNot = ['keys', 'journal', 'fuse', 'camera', 'gas', 'cloth', 'gun'];
for (const id of mustEarly) {
  const prop = PROPS.find((p) => p.id === id);
  if (!early.has(`${prop.x},${prop.y}`)) fail(`early path cannot reach ${id}`);
}
for (const id of mustNot) {
  const prop = PROPS.find((p) => p.id === id);
  if (early.has(`${prop.x},${prop.y}`)) fail(`early path should not reach ${id}`);
}

const her = bfs(world, withKeys(new Set(['key-her'])));
if (!her.has('8,5')) fail('her key does not open her room');
const base = bfs(world, withKeys(new Set(['key-base'])));
if (!base.has('21,34')) fail('basement key does not open the fuse');
const yard = bfs(world, withKeys(new Set(['power'])));
if (!yard.has('6,42')) fail('power does not open the yard');
if (yard.has('22,43')) fail('yard should not include the locked shed');
const all = bfs(world, withKeys(new Set(['key-her', 'key-base', 'key-shed', 'power'])));
if (!all.has('22,43')) fail('shed key does not reach the gas');

for (let y = 0; y < world.tiles.length; y++) {
  for (let x = 0; x < COLS; x++) {
    if (!passable(world, x, y, always)) continue;
    if (!all.has(`${x},${y}`)) fail(`sealed floor at ${x},${y} room ${roomAt(x, y)}`);
  }
}

if (!early.has(`${SPAWN.x},${SPAWN.y}`)) fail('spawn is not in the early graph');

const ids = PROPS.map((p) => p.id);
if (new Set(ids).size !== ids.length) fail('duplicate prop id');

const goal = (patch) => describeGoal({
  installed: { fuse: false, keys: false, gas: false, photo: false },
  read: {},
  loc: Object.fromEntries(['fuse', 'gas', 'camera', 'key-car', 'key-her', 'key-base', 'key-shed'].map((id) => [id, 'world'])),
  hasLight: false,
  finale: false,
  room: 'mud',
  tookBaseKey: false,
  ...patch,
});
if (!goal({}).includes('light')) fail('hint should ask for a light');
if (!goal({ hasLight: true }).toLowerCase().includes('car')) fail('hint should mention the car once lit');
if (!goal({ hasLight: true, read: { foyer: true } }).toLowerCase().includes('not mom')) fail('foyer note should spoil the voice');

fs.writeFileSync('/tmp/dontlook-map.txt', ascii(world, locked));

if (problems.length) {
  console.error(problems.join('\n'));
  console.error('\n' + ascii(world, locked));
  process.exit(1);
}
console.log(`world ok — ${PROPS.length} props, ${world.doors.length} doors, ${all.size} walkable tiles`);
