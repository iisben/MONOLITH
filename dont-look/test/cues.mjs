import fs from 'fs';
import { ROOM_BED, clipCurve, pickAmbience, scareLayerSum, softClipSample } from '../js/audio.mjs';
import {
  CUE,
  ENEMY_STEP_MARKS,
  FOOT_MARKS,
  HEAVY_ROOMS,
  gaitInterval,
  gaitRate,
  phaseMarksCrossed,
  stepLeaf,
} from '../js/cues.mjs';
import { createDirector } from '../js/director.mjs';
import { createDoorState, stepDoor } from '../js/doors.mjs';
import { Game } from '../js/game.mjs';
import { ROOMS, createWorld } from '../js/world.mjs';

const problems = [];
const fail = (msg) => problems.push(msg);

function assert(cond, msg) {
  if (!cond) fail(msg);
}

assert(softClipSample(0) === 0, 'silence is zero');
assert(softClipSample(Number.POSITIVE_INFINITY) === 0, 'non-finite samples die');
assert(Math.abs(softClipSample(40)) <= 1, 'positive clip stays inside 1');
assert(Math.abs(softClipSample(-40)) <= 1, 'negative clip stays inside 1');
const curve = clipCurve(128);
assert(Math.abs(curve[0] + 1) < 1e-9, 'clip curve starts at -1');
assert(Math.abs(curve[curve.length - 1] - 1) < 1e-9, 'clip curve ends at 1');
assert(scareLayerSum() < 1, 'scare layers sum under the clip');
assert(Math.abs(softClipSample(scareLayerSum())) < 1, 'summed scare layers clip');

for (const [running, sneaking] of [[false, false], [false, true], [true, false]]) {
  const interval = gaitInterval(running, sneaking);
  assert(Math.abs(gaitRate(running, sneaking) * interval - Math.PI) < 1e-9, 'gait rate matches the plant interval');
}

let phase = 0;
let plants = 0;
const walk = CUE.foot.walkInterval;
for (let n = 0; n < 4; n += 1) {
  const next = phase + gaitRate(false, false) * walk;
  plants += phaseMarksCrossed(phase, next, FOOT_MARKS);
  phase = next;
}
assert(plants === 4, `four walk intervals planted ${plants} times`);
const mark = FOOT_MARKS[0];
assert(phaseMarksCrossed(mark - 0.01, mark, FOOT_MARKS) === 1, 'the plant frame itself fires');
assert(phaseMarksCrossed(mark, mark + 0.01, FOOT_MARKS) === 0, 'the same plant does not fire twice');

let leaf = 0;
let crossings = 0;
let latchAt = 0;
for (let t = 0; leaf < 1 && t < 2; t += 0.05) {
  const step = stepLeaf(leaf, true, 0.05);
  if (step.crossed) {
    crossings += 1;
    latchAt = t + 0.05;
    assert(leaf < CUE.door.latchLeaf && step.leaf >= CUE.door.latchLeaf, 'latch step did not skip the mark');
  }
  leaf = step.leaf;
}
assert(crossings === 1, 'the latch crossed once');
assert(Math.abs(latchAt - CUE.door.latchLeaf * CUE.door.duration) <= 0.05, 'latch lands on its frame');

function run(director, seconds, dt = CUE.frame, env = {}) {
  const events = [];
  const views = [];
  const steps = Math.ceil(seconds / dt);
  for (let i = 0; i < steps; i += 1) {
    events.push(...director.update(dt, { time: i * dt, ...env }));
    views.push(director.view);
  }
  return { events, views };
}

function upward(views, read, line) {
  let count = 0;
  let prev = 0;
  let window = 0;
  let inWindow = 0;
  for (const view of views) {
    const value = read(view);
    if (value > line && prev <= line) {
      count += 1;
      inWindow += 1;
    }
    prev = value;
    window += 1;
    if (window === 60) {
      if (inWindow > 1) return Infinity;
      window = 0;
      inWindow = 0;
    }
  }
  return count;
}

const scare = createDirector();
assert(scare.requestScare('once', { story: true, face: true }) === true, 'story scare arms');
assert(scare.requestScare('once', { story: true, face: true }) === false, 'a live scare is not stacked');
const played = run(scare, 4);
const hits = played.events.filter((ev) => ev.type === 'scare-hit');
assert(hits.length === 1, `scare hit ${hits.length} times`);
assert(scare.mode === 'idle', 'scare returns to idle');
assert(scare.controlLock === 0, 'movement lock clears');
assert(Math.max(...played.views.map((view) => view.lunge)) > 0.5, 'the face lunges');
assert(upward(played.views, (view) => view.lunge, 0.5) === 1, 'the lunge peaks once');
assert(upward(played.views, (view) => view.black, 0.45) === 1, 'the blackout is a single pulse');
assert(played.views.every((view) => view.black <= 0.78), 'blackout stays capped');

const quiet = createDirector();
quiet.requestScare('soft', { story: true, face: true, reduce: true });
const softened = run(quiet, 4, CUE.frame, { reduce: true });
assert(Math.max(...softened.views.map((view) => view.lunge)) === 0, 'reduced scares do not lunge');
assert(Math.max(...softened.views.map((view) => view.black)) < 0.4, 'reduced blackout stays dim');

const cool = createDirector();
cool.requestScare('first', { once: false });
run(cool, 4);
assert(cool.requestScare('second', { once: false }) === false, 'cooldown blocks a casual scare');
assert(cool.requestScare('story', { story: true, once: false }) === true, 'a story scare ignores cooldown');

const queue = createDirector();
queue.requestScare('s1', { story: true, face: false });
assert(queue.requestScare('s2', { story: true, face: false }) === false, 'the second story scare waits');
const queued = run(queue, 8);
assert(queued.events.filter((ev) => ev.type === 'scare-hit').length === 2, 'the queued scare still hits');
assert(queue.mode === 'idle', 'the queue drains to idle');

const death = createDirector();
death.requestTransition('hall', { heavy: true });
death.update(CUE.frame, {});
assert(death.requestScare('death', { story: true, preempt: true, face: true }) === true, 'death preempts');
const dying = run(death, 4);
assert(dying.events.filter((ev) => ev.type === 'scare-hit').length === 1, 'death hits once');
assert(dying.events.filter((ev) => ev.type === 'transition-sub').length === 0, 'death drops the room dip');

const long = createDirector();
long.requestScare('held', { story: true, aftermath: 20 });
const held = run(long, 3.5, 0.05);
assert(held.events.some((ev) => ev.type === 'scare-hit'), 'a long scare still hits');
assert(long.mode === 'idle', 'the scare hard-stops');
assert(long.controlLock === 0, 'the hard stop clears the lock');

const room = createDirector();
room.requestTransition('basement', { heavy: true });
const dip = run(room, 1.4);
assert(dip.events.filter((ev) => ev.type === 'transition-sub').length === 1, 'the room dip has one sub hit');
assert(upward(dip.views, (view) => view.black, 0.45) === 1, 'the room blackout pulses once');

for (const id of Object.keys(ROOMS)) {
  assert(ROOM_BED[id], `${id} has a room bed`);
}
for (const id of HEAVY_ROOMS) assert(ROOMS[id], `${id} is a real room`);
assert(pickAmbience('basement', 2) === pickAmbience('basement', 2), 'ambience pick is stable');
assert(ENEMY_STEP_MARKS.length === 2, 'enemies plant twice a cycle');

const world = createWorld();
const door = world.doors.find((item) => item.kind !== 'overhead' && item.id !== 'front');
const swinging = createDoorState({ ...door, open: false });
swinging.open = true;
const doorEvents = [];
for (let i = 0; i < 240 && swinging.angle < 0.99; i += 1) {
  doorEvents.push(...stepDoor(door, swinging, CUE.frame, { reduce: false }));
}
assert(doorEvents.some((ev) => ev.type === 'creak' || ev.type === 'thud' || ev.type === 'latch'), 'a swinging door speaks on a frame');
const snapped = createDoorState({ ...door, open: false });
snapped.open = true;
const snapEvents = stepDoor(door, snapped, CUE.frame, { reduce: true });
assert(snapped.angle === 1, 'reduced motion snaps the leaf');
assert(snapEvents.length === 1, 'the snap is one impact');

const gameSrc = fs.readFileSync(new URL('../js/game.mjs', import.meta.url), 'utf8');
const renderSrc = fs.readFileSync(new URL('../js/render.mjs', import.meta.url), 'utf8');
assert(!gameSrc.includes('audio.stinger'), 'stingers go through the scare clock');
assert(!gameSrc.includes('p.step'), 'footsteps use the gait phase');
assert(gameSrc.includes('stepDoor'), 'door audio follows the leaf');
assert(renderSrc.includes('player.phase'), 'the player swing is the gait phase');
assert(renderSrc.includes('enemy.phase'), 'the enemy bob is the step phase');
assert(!renderSrc.includes('Math.random'), 'the camera no longer shakes at random');

const audio = new Proxy({}, { get: () => () => {} });
const canvas = { getContext() { return {}; } };
const game = new Game(canvas, audio);
game.newGame();
game.input.x = 1;
for (let i = 0; i < 90; i += 1) game.update(CUE.frame);
assert(game.state.player.phase > 0, 'walking advances the gait');
const before = game.state.player.phase;
game.input.x = 0;
game.update(CUE.frame);
assert(game.state.player.phase === before, 'a planted foot holds its phase');
game.state.player.x = (ROOMS.basement.x + 1.5) * 32;
game.state.player.y = (ROOMS.basement.y + 1.5) * 32;
game.update(CUE.frame);
assert(game.director.fired.has('enter-basement') || game.director.mode !== 'idle', 'the basement entrance is directed');

if (problems.length) {
  console.error(problems.join('\n'));
  process.exit(1);
}
console.log('cues ok');
