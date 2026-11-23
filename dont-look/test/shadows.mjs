import fs from 'fs';
import { syncDoorMotion } from '../js/doors.mjs';
import { Game } from '../js/game.mjs';
import {
  ACTORS,
  MOON_DIR,
  PRACTICAL_LIST,
  VIEW_H,
  VIEW_W,
  beginShade,
  collectLights,
  flashlightFlicker,
  lampFlicker,
  projectCasts,
  shadeFactor,
  shadeIllumination,
  shadeStats,
  spriteAnchor,
  wallOcclusion,
} from '../js/lighting.mjs';
import { render, VIEW_H as RENDER_H, VIEW_W as RENDER_W } from '../js/render.mjs';
import { ROOMS, TILE, passable, roomAt } from '../js/world.mjs';

const problems = [];
const fail = (msg) => problems.push(msg);
const near = (a, b, eps = 1e-6) => Math.abs(a - b) <= eps;

function contactCovers(calls, ax, ay) {
  let style = '';
  for (const call of calls) {
    if (call[0] === 'set' && call[1] === 'fillStyle') style = String(call[2]);
    if (call[0] !== 'fillRect' || !style.startsWith('rgba(2, 3, 6')) continue;
    const x = call[1];
    const y = call[2];
    const w = call[3];
    const h = call[4];
    if (ax >= x && ax < x + w && ay >= y && ay < y + h) return true;
  }
  return false;
}

function mockCtx() {
  const calls = [];
  return new Proxy({ calls }, {
    get(target, prop) {
      if (prop in target) return target[prop];
      return (...args) => { calls.push([prop, ...args]); };
    },
    set(target, prop, value) {
      target[prop] = value;
      calls.push(['set', prop, value]);
      return true;
    },
  });
}

function fakeCanvas() {
  const ctx = new Proxy({}, {
    get(target, prop) {
      if (prop in target) return target[prop];
      if (prop === 'createImageData') {
        return (w, h) => ({ data: new Uint8ClampedArray(w * h * 4), width: w, height: h });
      }
      return () => {};
    },
    set(target, prop, value) {
      target[prop] = value;
      return true;
    },
  });
  return { width: 8, height: 8, getContext: () => ctx };
}

if (typeof document === 'undefined') {
  globalThis.document = { createElement: () => fakeCanvas() };
}

const audio = new Proxy({}, { get: () => () => {} });
const game = new Game({ getContext: () => ({}) }, audio);
game.newGame();
const clear = { los: () => true };

function swingDoor(id, open) {
  const door = game.world.doors.find((item) => item.id === id);
  const motion = game.state.doors[id];
  motion.open = open;
  motion.angle = open ? 1 : 0;
  motion.latched = !open;
  motion.arrivedOpen = open;
  syncDoorMotion(door, motion);
}

if (VIEW_W !== RENDER_W || VIEW_H !== RENDER_H) fail('light map size drifted from the view');
if (fs.readFileSync(new URL('../js/lighting.mjs', import.meta.url), 'utf8').includes('Math.random')) {
  fail('shadows use Math.random and will flicker between frames');
}

const lamp = PRACTICAL_LIST.find((item) => item.id === 'kitchen');
if (lampFlicker(1.25, lamp, false) !== lampFlicker(1.25, lamp, false)) fail('lamp flicker is not stable');
if (flashlightFlicker(0.4, 80, false) !== 1) fail('a healthy battery must not flicker');
if (flashlightFlicker(0.4, 10, true) !== 0.72) fail('reduced flash changed the beam flicker');

const noisy = [];
const calm = [];
for (let i = 0; i < 90; i++) {
  const t = i * 0.037;
  noisy.push(lampFlicker(t, lamp, false));
  calm.push(lampFlicker(t, lamp, true));
}
const variance = (values) => {
  const mean = values.reduce((sum, value) => sum + value, 0) / values.length;
  return values.reduce((sum, value) => sum + (value - mean) ** 2, 0) / values.length;
};
if (!(Math.min(...calm) > 0.7)) fail('reduced flash still strobes the lamps');
if (!(variance(calm) < variance(noisy) * 0.35)) fail('reduced flash does not calm the lamps');

for (const practical of PRACTICAL_LIST) {
  if (!passable(game.world, practical.tx, practical.ty, () => true)) {
    fail(`practical ${practical.id} is not on open floor`);
  }
  if (roomAt(practical.tx, practical.ty) == null) fail(`practical ${practical.id} is outside the house`);
}

if (shadeFactor(0) !== 0) fail('unlit floor is not fully dark');
if (shadeFactor(1) !== 1) fail('a full beam no longer shows the painted floor');
if (!(shadeFactor(0.05) < 0.16)) fail('a trace of light lifts the black too far');
if (!(shadeFactor(0.25) > shadeFactor(0.05) && shadeFactor(0.25) < 0.55)) {
  fail('lamp pools are either buried or blown out');
}

const px = game.state.player.x;
const py = game.state.player.y;
for (let ox = 0; ox < 4; ox += 0.2) {
  for (let oy = 0; oy < 4; oy += 0.35) {
    const anchor = spriteAnchor(px, py, ox, oy, ACTORS.player.foot);
    if (anchor.x !== Math.round(px - ox) || anchor.y !== Math.round(py - oy) + ACTORS.player.foot) {
      fail('contact shadow anchor does not use the sprite feet');
    }
  }
}

game.state.hasLight = true;
game.state.player.lightOn = true;
game.state.player.battery = 100;
game.state.player.facing = 0;
game.time = 0.4;
const ahead = game.lightAt(px + 100, py);
game.state.player.battery = 10;
const flickered = game.lightAt(px + 100, py);
const beamFlicker = flashlightFlicker(0.4, 10, false);
if (!near(flickered / ahead, beamFlicker, 1e-5)) fail('the beam and the shared flicker diverged');

const selfLit = {
  id: 'flash',
  kind: 'cone',
  x: px + 12,
  y: py,
  z: 24,
  radius: 188,
  intensity: beamFlicker,
  aim: 0,
  cone: 0.82,
  rgb: [232, 214, 176],
};
const selfFull = { ...selfLit, intensity: 1 };
const dimCast = projectCasts(px, py + 7, ACTORS.player, [selfLit], game, { self: true, facing: 0 })
  .find((cast) => cast.id === 'flash');
const fullCast = projectCasts(px, py + 7, ACTORS.player, [selfFull], game, { self: true, facing: 0 })
  .find((cast) => cast.id === 'flash');
if (!dimCast || !fullCast) fail('the flashlight does not cast from the player');
if (!near(dimCast.alpha / fullCast.alpha, beamFlicker, 1e-5)) fail('self-shadow does not follow the beam flicker');
if (!(dimCast.dirX < -0.9 && Math.abs(dimCast.dirY) < 0.1)) fail('flashlight shadow does not fall behind the aim');
const turned = projectCasts(px, py + 7, ACTORS.player, [selfLit], game, { self: true, facing: Math.PI / 2 })
  .find((cast) => cast.id === 'flash');
if (!(turned.dirY < -0.9)) fail('flashlight shadow does not rotate with facing');

const feet = { x: (6.5) * TILE, y: (5.5) * TILE };
let previous = null;
for (const dist of [18, 40, 78, 120]) {
  const light = {
    id: 'sweep',
    kind: 'point',
    x: feet.x - dist,
    y: feet.y,
    z: 42,
    radius: 200,
    intensity: 0.85,
    rgb: [230, 190, 140],
  };
  const cast = projectCasts(feet.x, feet.y, ACTORS.shambler, [light], game, {}).find((item) => item.id === 'sweep');
  if (!cast) fail(`lamp at ${dist}px cast nothing`);
  else {
    if (!(cast.along - cast.len < -0.5)) fail('cast shadow detached from the feet');
    if (!(cast.dirX > 0.98)) fail('cast shadow did not point away from the lamp');
    if (!(Math.abs(cast.dirX ** 2 + cast.dirY ** 2 - 1) < 1e-6)) fail('cast direction is not a unit vector');
    if (previous && !(cast.length > previous.length && cast.soft >= previous.soft - 1e-6)) {
      fail('shadows do not stretch and soften as the light gets farther');
    }
    previous = cast;
  }
}

for (let angle = 0; angle < Math.PI * 2; angle += 0.45) {
  for (const dist of [22, 64, 100]) {
    const light = {
      id: 'arc',
      kind: 'point',
      x: feet.x - Math.cos(angle) * dist,
      y: feet.y - Math.sin(angle) * dist,
      z: 36,
      radius: 180,
      intensity: 0.7 + (angle % 0.2),
      rgb: [200, 180, 140],
    };
    const cast = projectCasts(feet.x, feet.y, ACTORS.player, [light], clear, {}).find((item) => item.id === 'arc');
    if (!cast) fail('a light inside range produced no shadow');
    else {
      const dot = cast.dirX * Math.cos(angle) + cast.dirY * Math.sin(angle);
      if (!(dot > 0.98)) fail('shadow direction popped off the light vector');
      if (!(cast.along - cast.len < 0)) fail('sweeping the light detached the shadow');
      if (!Number.isFinite(cast.alpha) || !Number.isFinite(cast.length)) fail('shadow math produced NaN');
    }
  }
}

const nudgedA = projectCasts(feet.x, feet.y, ACTORS.player, [{
  id: 'nudge', kind: 'point', x: feet.x - 60, y: feet.y - 20, z: 40, radius: 160, intensity: 0.6, rgb: [220, 180, 120],
}], game, {})[0];
const nudgedB = projectCasts(feet.x + 1, feet.y, ACTORS.player, [{
  id: 'nudge', kind: 'point', x: feet.x - 60, y: feet.y - 20, z: 40, radius: 160, intensity: 0.6, rgb: [220, 180, 120],
}], game, {})[0];
if (nudgedA && nudgedB && nudgedA.dirX * nudgedB.dirX + nudgedA.dirY * nudgedB.dirY < 0.99) {
  fail('a one-pixel step flipped the shadow');
}

const blockedLight = {
  id: 'across',
  kind: 'point',
  x: (9.5) * TILE,
  y: (4.5) * TILE,
  z: 40,
  radius: 180,
  intensity: 0.9,
  rgb: [230, 200, 150],
};
const beyond = { x: (11.5) * TILE, y: (4.5) * TILE };
swingDoor('her', false);
const closed = projectCasts(beyond.x, beyond.y, ACTORS.player, [blockedLight], game, {});
if (closed.some((cast) => cast.id === 'across')) fail('a closed door let a shadow through');
swingDoor('her', true);
const opened = projectCasts(beyond.x, beyond.y, ACTORS.player, [blockedLight], game, {});
if (!opened.some((cast) => cast.id === 'across')) fail('an open door still blocked the cast');
swingDoor('her', false);

game.state.flags.powerOn = true;
game.time = 0.35;
let lights = collectLights(game);
const kitchen = lights.find((light) => light.id === 'kitchen');
const kitchenDoor = lights.find((light) => light.id === 'door:kit-n');
if (!kitchen || !kitchenDoor) fail('the kitchen lamp or its doorway leak is missing');
else if (!near(kitchenDoor.intensity, kitchen.intensity * 0.48, 1e-5)) {
  fail('doorway light does not use the lamp that is actually on');
}
const kitchenAt = kitchen.intensity / kitchen.flicker;
game.time = 1.7;
lights = collectLights(game);
const kitchenLater = lights.find((light) => light.id === 'kitchen');
const doorLater = lights.find((light) => light.id === 'door:kit-n');
if (!near(kitchenLater.intensity / kitchen.intensity, lampFlicker(1.7, lamp, false) / lampFlicker(0.35, lamp, false), 1e-5)) {
  fail('lamp intensity left the flicker curve');
}
if (doorLater.source === 'kitchen' && !near(doorLater.intensity / kitchenLater.intensity, 0.48, 1e-5)) {
  fail('doorway leak drifted off the kitchen flicker');
}
if (!near(kitchenLater.intensity / kitchenLater.flicker, kitchenAt, 1e-5)) fail('flicker was baked into the lamp base');

const yardClosed = collectLights(game).find((light) => light.id === 'door:yard');
swingDoor('yard', true);
const yardOpen = collectLights(game).find((light) => light.id === 'door:yard');
if (!yardClosed || !yardOpen) fail('the yard door does not leak moonlight');
if (!(yardOpen.radius > yardClosed.radius && yardOpen.intensity > yardClosed.intensity)) {
  fail('opening the yard door does not widen the moon leak');
}

game.time = 0.2;
game.state.player.lightOn = false;
game.state.player.x = (24.5) * TILE;
game.state.player.y = (26.5) * TILE;
beginShade(game, 0, 0);
const herDark = shadeIllumination(game, 6.5 * TILE, 5.5 * TILE);
if (!(herDark < 0.08)) fail(`her room is lit without a source (${herDark.toFixed(3)})`);
const corner = wallOcclusion(game, 2 * TILE + 2, 2 * TILE + 2);
const edge = wallOcclusion(game, 6 * TILE + 16, 2 * TILE + 2);
const openFloor = wallOcclusion(game, 6 * TILE + 16, 5 * TILE + 16);
if (!(corner < edge && edge < openFloor)) fail('corners are not darker than edges and open floor');
if (!(openFloor > 0.98 && corner >= 0.68)) fail('occlusion either misses the corner or blacks out the room');

game.state.flags.powerOn = true;
beginShade(game, 0, 0);
const pool = shadeIllumination(game, 6.5 * TILE, 27.5 * TILE);
if (!(pool > 0.12 && pool < 0.72)) fail(`kitchen pool is outside a readable horror range (${pool.toFixed(3)})`);

game.state.player.lightOn = true;
game.state.player.battery = 90;
game.state.player.facing = Math.PI;
beginShade(game, game.cam.x, game.cam.y);
const toes = shadeIllumination(game, game.state.player.x, game.state.player.y + 7);
if (!(toes > 0.45)) fail('the floor under the player goes black');

const moonCast = projectCasts(3 * TILE, 42 * TILE, ACTORS.player, [{
  id: 'moon', kind: 'dir', dirX: MOON_DIR.x, dirY: MOON_DIR.y, z: 170, intensity: 1, rgb: [148, 168, 198],
}], game, {})[0];
if (!moonCast || moonCast.dirX * MOON_DIR.x + moonCast.dirY * MOON_DIR.y < 0.99) {
  fail('yard moonlight does not cast along the ambient direction');
}

let worst = -Infinity;
let rooms = 0;
for (const id of Object.keys(ROOMS)) {
  game.debugGoto(id);
  game.state.hasLight = true;
  game.state.player.lightOn = true;
  game.state.player.battery = 70;
  game.state.flags.powerOn = true;
  game.time = 0.4 + rooms * 0.17;
  const shade = beginShade(game, game.cam.x, game.cam.y);
  const casts = projectCasts(
    game.state.player.x,
    game.state.player.y + ACTORS.player.foot,
    ACTORS.player,
    shade.lights,
    game,
    { self: true, facing: game.state.player.facing },
  );
  if (!casts.length) fail(`no cast shadow in ${id}`);
  for (const cast of casts) {
    const gap = cast.along - cast.len;
    if (gap > worst) worst = gap;
    if (!(gap < 0)) fail(`shadow detached in ${id} from ${cast.id}`);
  }
  for (const enemy of game.state.enemies) {
    if (!enemy.alive) continue;
    const kind = enemy.kind === 'grin' ? 'grin' : 'shambler';
    const enemyCasts = projectCasts(enemy.x, enemy.y + ACTORS[kind].foot, ACTORS[kind], shade.lights, game, {});
    for (const cast of enemyCasts) {
      if (!(cast.along - cast.len < 0)) fail(`${enemy.id} shadow detached in ${id}`);
    }
  }
  rooms += 1;
}
if (!(worst < 0)) fail('at least one room left a gap under a foot');

const started = performance.now();
let samples = 0;
for (let i = 0; i < 4; i++) {
  game.time = i * 0.016;
  beginShade(game, game.cam.x, game.cam.y);
  samples += shadeStats(game).fieldMs;
}
const frameMs = samples / 4;
if (frameMs > 45) fail(`light field is too slow for a frame (${frameMs.toFixed(1)}ms)`);

game.ctx = mockCtx();
game.shake = 0;
game.state.player.iframes = 0;
game.time = 0.2;
render(game);
const stats = shadeStats(game);
if (!(stats.contacts >= 1 && stats.casts >= 1)) fail('a rendered frame drew no character shadows');
if (!(stats.aoBands > 0)) fail('rooms have no wall or corner occlusion');
if (!(stats.worstGap < 0)) fail('rendered casts leave a gap at the feet');
const player = game.state.player;
const anchor = spriteAnchor(player.x, player.y, game.cam.x, game.cam.y, ACTORS.player.foot);
if (!contactCovers(game.ctx.calls, anchor.x, anchor.y)) {
  fail('the player contact shadow is not on the sprite feet');
}
if (game.ctx.globalCompositeOperation === 'multiply') fail('the light map left the frame in multiply mode');

game.ctx = mockCtx();
game.state.player.iframes = 1;
game.time = 0.02;
render(game);
const blinked = game.ctx.calls;
if (blinked.some((call) => call[0] === 'set' && call[2] === '#1c2744')) {
  fail('iframe blink still drew the body, so the shadow test is invalid');
}
const blinkAnchor = spriteAnchor(player.x, player.y, game.cam.x, game.cam.y, ACTORS.player.foot);
if (!contactCovers(blinked, blinkAnchor.x, blinkAnchor.y)) {
  fail('the contact shadow disappears when the sprite blinks');
}

if (problems.length) {
  console.error(problems.join('\n'));
  process.exit(1);
}
console.log(`shadows ok — light field ${frameMs.toFixed(1)}ms, ${rooms} rooms, gap ${worst.toFixed(2)}px`);
