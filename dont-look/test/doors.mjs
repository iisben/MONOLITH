import { TILE as WORLD_TILE, createWorld } from '../js/world.mjs';
import {
  SEALED_DOORS,
  SWING_FRAMES,
  TILE,
  auditDoor,
  auditSealed,
  auditWorld,
  bodyBlocked,
  createDoorState,
  doorSeals,
  drawDoors,
  formatAudit,
  interactPoint,
  planDoorUse,
  stepDoor,
} from '../js/doors.mjs';

const world = createWorld();
const problems = [];
const fail = (msg) => problems.push(msg);

if (TILE !== WORLD_TILE) fail(`door tile ${TILE} does not match world tile ${WORLD_TILE}`);
if (world.doors.length !== 17) fail(`expected 17 doors, found ${world.doors.length}`);

const rows = auditWorld(world.doors, world.tiles, world.furniture);
for (const spec of SEALED_DOORS) rows.push(auditSealed(spec, world.tiles));
console.log(formatAudit(rows));

for (const row of rows) {
  if (row.issues.length) fail(`${row.id}: ${row.issues.join(', ')}`);
  if (row.flush !== 'flush') fail(`${row.id} is not flush`);
}

const ids = new Set();
for (const door of world.doors) {
  if (ids.has(door.id)) fail(`duplicate door ${door.id}`);
  ids.add(door.id);
  if (!door.axis || !door.hinge) fail(`${door.id} has no fitted frame`);
  const closed = createDoorState({ ...door, open: false });
  const opened = createDoorState({ ...door, open: true });
  const cx = (door.x + 0.5) * TILE;
  const cy = (door.y + 0.5) * TILE;
  if (!doorSeals(door, closed)) fail(`${door.id} closed leaf does not seal the opening`);
  if (doorSeals(door, opened)) fail(`${door.id} stays sealed when open`);
  if (bodyBlocked(door, opened, cx, cy, 12)) fail(`${door.id} blocks a body in the open doorway`);

  const shut = createDoorState(door);
  shut.open = false;
  shut.angle = shut.open ? 1 : 0;
  const opening = run(door, createDoorState({ ...door, open: false }), (st) => { st.open = true; }, 3);
  if (opening.st.angle < 0.99) fail(`${door.id} did not finish opening (${opening.st.angle})`);
  if (door.kind === 'overhead') {
    if (!opening.types.includes('motor')) fail(`${door.id} motor never ran`);
    if (!opening.types.includes('motor-stop')) fail(`${door.id} motor did not seat`);
    const motorAt = opening.events.findIndex((ev) => ev.type === 'motor');
    const stopAt = opening.events.findIndex((ev) => ev.type === 'motor-stop');
    if (motorAt > stopAt) fail(`${door.id} motor stop fired before the grind`);
  } else {
    if (!opening.types.includes('creak')) fail(`${door.id} swing has no creak`);
    if (!opening.types.includes('thud')) fail(`${door.id} swing has no stop`);
    const creakAt = opening.events.findIndex((ev) => ev.type === 'creak');
    const thudAt = opening.events.findIndex((ev) => ev.type === 'thud');
    if (creakAt > thudAt) fail(`${door.id} thud fired before the creak`);
    const closing = run(door, opening.st, (st) => { st.open = false; }, 3);
    if (closing.st.angle > 0.01) fail(`${door.id} did not finish closing`);
    if (!closing.types.includes('latch') && !closing.types.includes('slam')) fail(`${door.id} did not latch`);
    const closeCreak = closing.events.findIndex((ev) => ev.type === 'creak');
    const latchAt = closing.events.findIndex((ev) => ev.type === 'latch' || ev.type === 'slam');
    if (closeCreak === -1 || closeCreak > latchAt) fail(`${door.id} latch is not on the closing frame`);
  }

  const handle = interactPoint(door, createDoorState({ ...door, open: false }));
  const sides = door.passage || [];
  const reachable = sides.some((side) => Math.hypot(handle.x - (side.x + 0.5) * TILE, handle.y - (side.y + 0.5) * TILE) <= 44);
  if (!reachable) fail(`${door.id} handle is out of reach`);
  const center = interactPoint(door, createDoorState({ ...door, open: true }));
  for (const side of sides) {
    const dist = Math.hypot(center.x - (side.x + 0.5) * TILE, center.y - (side.y + 0.5) * TILE);
    if (dist > 44) fail(`${door.id} open prompt is ${dist.toFixed(1)}px from ${side.x},${side.y}`);
  }
}

const her = world.doors.find((door) => door.id === 'her');
const locked = createDoorState(her);
const denied = planDoorUse(her, locked, { has: () => false, powerOn: false });
if (denied.action !== 'rattle') fail('locked door did not rattle');
locked.rattle = 0.42;
const rattled = run(her, locked, () => {}, 0.5);
if (rattled.st.angle !== 0) fail('rattle opened the door');
if (rattled.types.filter((type) => type === 'rattle').length < 3) fail('locked rattle did not click');
const granted = planDoorUse(her, createDoorState(her), { has: (id) => id === 'key-her', powerOn: false });
if (granted.action !== 'unlock' || granted.consume !== 'key-her') fail('her key did not unlock');

const yard = world.doors.find((door) => door.id === 'yard');
const dead = planDoorUse(yard, createDoorState(yard), { has: () => false, powerOn: false });
if (dead.action !== 'rattle' || dead.say !== 'no-power') fail('dead motor did not rattle');
const powered = createDoorState(yard);
powered.unlocked = true;
powered.open = true;
powered.angle = 1;
if (planDoorUse(yard, powered, { has: () => false, powerOn: true }).action !== 'refuse') fail('yard door can be closed');

const slam = createDoorState({ ...her, open: true });
slam.open = false;
slam.slam = true;
const slammed = run(her, slam, () => {}, 2);
if (!slammed.types.includes('slam')) fail('her door did not slam');
if (slammed.st.angle > 0.02) fail('slam did not seat');

const already = createDoorState(her);
already.pendingSlam = true;
const seated = run(her, already, () => {}, 0.05);
if (!seated.types.includes('slam')) fail('a shut door did not report the slam');

const reduced = createDoorState({ ...her, open: false });
reduced.open = true;
const snapped = run(her, reduced, () => {}, 0.05, true);
if (snapped.st.angle !== 1) fail('reduced motion did not settle open');
if (snapped.types.includes('creak')) fail('reduced motion still swung');
if (!snapped.types.includes('thud')) fail('reduced motion skipped the stop');
if (snapped.events.length !== 1) fail(`reduced motion emitted ${snapped.events.length} events`);

const frames = new Set(slammed.events.filter((ev) => ev.type === 'creak').map((ev) => ev.frame));
if (frames.size && [...frames].some((frame) => frame <= 0 || frame >= SWING_FRAMES)) {
  fail('creak fired on the latch frame');
}

let rects = 0;
const ctx = {
  fillStyle: '',
  fillRect() { rects += 1; },
};
const state = { doors: {}, flags: { powerOn: false } };
for (const door of world.doors) state.doors[door.id] = createDoorState(door);
drawDoors(ctx, {
  world,
  state,
  time: 1.2,
  lightAt: () => 0.66,
}, 0, 0);
if (rects < 200) fail(`door draw produced only ${rects} pixels`);

function run(door, st, prep, seconds, reduce = false) {
  prep(st);
  const events = [];
  const dt = 1 / 60;
  for (let t = 0; t < seconds; t += dt) events.push(...stepDoor(door, st, dt, { reduce }));
  return { st, events, types: events.map((ev) => ev.type) };
}

if (problems.length) {
  console.error(problems.join('\n'));
  process.exit(1);
}
console.log(`doors ok — ${world.doors.length} swinging, ${SEALED_DOORS.length} sealed`);
