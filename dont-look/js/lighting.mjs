// Grounded shadows and the screen light map.
// Contact shadows stay on the feet. Casts are rebuilt from the same light
// list that paints the pools, so a flicker moves the shadow in that frame.
// VIEW size matches render.mjs (416×224). Keeping it local avoids a cycle.

import { TUNING } from './data.mjs';
import { doorSeals } from './doors.mjs';
import { COLS, ROWS, TILE, roomAt } from './world.mjs';

export const VIEW_W = 416;
export const VIEW_H = 224;

const CELL = 4;
const GW = VIEW_W / CELL;
const GH = VIEW_H / CELL;
const STRIDE = 6;

const MOON_X = 0.62;
const MOON_Y = 0.78;
const MOON_LEN = Math.hypot(MOON_X, MOON_Y);
export const MOON_DIR = { x: MOON_X / MOON_LEN, y: MOON_Y / MOON_LEN };
const MOON_RGB = [148, 168, 198];

// Practicals sit on open floor. `mains` bulbs are dark until the fuse is in.
// Always-on sources are the ones the house already has: static, tanks, the
// warm front door, a bathroom nightlight, a window. None of them are a plot beat.
const PRACTICALS = [
  { id: 'foyer', tx: 17, ty: 25, z: 48, radius: 84, intensity: 0.36, ember: 0, flicker: 11, phase: 0.2, power: 'mains', rgb: [232, 196, 140] },
  { id: 'kitchen', tx: 6, ty: 27, z: 42, radius: 96, intensity: 0.32, ember: 0, flicker: 17, phase: 1.4, power: 'mains', rgb: [188, 214, 196] },
  { id: 'bath', tx: 22, ty: 5, z: 34, radius: 58, intensity: 0.24, ember: 0.24, flicker: 7.5, phase: 2.2, power: 'always', rgb: [176, 206, 214] },
  { id: 'hall-n', tx: 14, ty: 5, z: 46, radius: 74, intensity: 0.22, ember: 0, flicker: 13, phase: 0.6, power: 'mains', rgb: [220, 186, 130] },
  { id: 'hall-s', tx: 15, ty: 12, z: 46, radius: 74, intensity: 0.2, ember: 0, flicker: 15, phase: 2.8, power: 'mains', rgb: [220, 186, 130] },
  { id: 'tv', tx: 5, ty: 19, z: 20, radius: 52, intensity: 0.18, ember: 0.18, flicker: 22, phase: 0.15, power: 'always', rgb: [154, 176, 198] },
  { id: 'dining', tx: 23, ty: 21, z: 50, radius: 68, intensity: 0.18, ember: 0, flicker: 8.5, phase: 1.1, power: 'mains', rgb: [214, 176, 124] },
  { id: 'mom-window', tx: 21, ty: 13, z: 40, radius: 48, intensity: 0.1, ember: 0.1, flicker: 0.7, phase: 0.4, power: 'always', rgb: [142, 164, 190] },
  { id: 'garage', tx: 10, ty: 32, z: 36, radius: 100, intensity: 0.4, ember: 0, flicker: 9.5, phase: 1.7, power: 'mains', rgb: [232, 204, 156] },
  { id: 'tanks', tx: 25, ty: 34, z: 26, radius: 62, intensity: 0.28, ember: 0.28, flicker: 6.2, phase: 0.9, power: 'always', rgb: [96, 176, 162] },
  { id: 'front', tx: 27, ty: 26, z: 16, radius: 40, intensity: 0.16, ember: 0.16, flicker: 1.2, phase: 0.5, power: 'always', rgb: [196, 124, 96] },
];

export const ACTORS = {
  player: { h: 18, radius: 5.2, foot: 7, cast: 1 },
  shambler: { h: 15, radius: 4.8, foot: 6, cast: 0.95 },
  grin: { h: 22, radius: 3.5, foot: 8, cast: 0.9 },
  sleeper: { h: 6, radius: 7.2, foot: 4, cast: 0.5 },
  down: { h: 4, radius: 7.5, foot: 1, cast: 0.35 },
  prop: { h: 6, radius: 3.1, foot: 2, cast: 0.45 },
  drop: { h: 4, radius: 2.5, foot: 1, cast: 0.4 },
};

const DIRS = [[1, 0], [-1, 0], [0, 1], [0, -1]];

let lightCanvas = null;
let lightImage = null;
let lightCtx = null;

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

export function angDiff(a, b) {
  let d = a - b;
  while (d > Math.PI) d -= Math.PI * 2;
  while (d < -Math.PI) d += Math.PI * 2;
  return d;
}

// Shared with Game.lightAt so the beam and the self-shadow pulse together.
export function flashlightFlicker(time, battery, reduceFlash) {
  if (battery >= 16) return 1;
  if (reduceFlash) return 0.72;
  return 0.45 + 0.55 * Math.abs(Math.sin(time * 26));
}

export function lampFlicker(time, lamp, reduceFlash) {
  if (!lamp.flicker) return 1;
  const phase = lamp.phase || 0;
  if (reduceFlash) return 0.84 + 0.1 * Math.sin(time * 0.8 + phase);
  const hz = lamp.flicker;
  const a = Math.sin(time * hz + phase);
  const b = Math.sin(time * hz * 0.37 + phase * 2.13);
  const c = Math.sin(time * hz * 2.17 + phase * 0.37);
  const gate = b * 0.7 + c * 0.3;
  if (gate > 0.62) return 0.16 + 0.14 * (0.5 + 0.5 * a);
  if (gate < -0.78) return 0.88 + 0.12 * (0.5 + 0.5 * a);
  return 0.48 + 0.44 * (0.5 + 0.5 * a);
}

// `intensity` lifts the floor. Indoors it stays at 0 so the flashlight is still
// the only thing that opens a room. `cast` is the directional shadow, which
// can exist without brightening the floor.
export function moonStrength(room) {
  if (room === 'yard') return { intensity: 0.46, cast: 0.5, virtualDist: 86 };
  if (room === 'shed') return { intensity: 0.2, cast: 0.3, virtualDist: 42 };
  if (room === 'garage') return { intensity: 0.04, cast: 0.16, virtualDist: 30 };
  return { intensity: 0, cast: 0.2, virtualDist: 22 };
}

// 0 in a buried corner, 1 in open floor. Applied as a light multiplier.
export function aoMultiplier(blockers, x, y) {
  const tx = Math.floor(x / TILE);
  const ty = Math.floor(y / TILE);
  let nearest = 64;
  let closeCards = 0;
  for (let oy = -1; oy <= 1; oy++) {
    for (let ox = -1; ox <= 1; ox++) {
      if (!ox && !oy) continue;
      if (!isOccluder(blockers, tx + ox, ty + oy)) continue;
      const left = (tx + ox) * TILE;
      const top = (ty + oy) * TILE;
      const right = left + TILE;
      const bottom = top + TILE;
      const dx = x < left ? left - x : x > right ? x - right : 0;
      const dy = y < top ? top - y : y > bottom ? y - bottom : 0;
      const dist = Math.hypot(dx, dy);
      if (dist < nearest) nearest = dist;
      if ((ox === 0 || oy === 0) && dist < 14) closeCards += 1;
    }
  }
  const edge = nearest < 20 ? (1 - nearest / 20) * 0.2 : 0;
  const corner = closeCards >= 2 ? 0.12 : 0;
  return clamp(1 - edge - corner, 0.68, 1);
}

export function wallOcclusion(game, x, y) {
  const blockers = game._shade?.blockers;
  if (!blockers) return 1;
  return aoMultiplier(blockers, x, y);
}

// Pulls midtones up enough that a lamp pool is readable, and leaves unlit
// floor buried. 0 → black, 1 → the painted floor, unchanged.
export function shadeFactor(light) {
  const l = clamp(light, 0, 1);
  return l ** 0.72;
}

export function spriteAnchor(worldX, worldY, ox, oy, foot) {
  return {
    x: Math.round(worldX - ox),
    y: Math.round(worldY - oy) + foot,
  };
}

// 0 open floor, 1 furniture (occludes neighbors, still receives light), 2 wall.
function tileClass(game, tx, ty) {
  if (tx < 0 || ty < 0 || tx >= COLS || ty >= ROWS) return 2;
  const tile = game.world.tiles[ty][tx];
  if (tile === 0) return 2;
  if (tile === 9) {
    const door = game.world.doorAt.get(`${tx},${ty}`);
    const st = door && game.state.doors[door.id];
    return door && st && doorSeals(door, st) ? 1 : 0;
  }
  if (game.world.furniture.has(`${tx},${ty}`)) return 1;
  return 0;
}

function isWall(blockers, x, y) {
  if (x < 0 || y < 0 || x >= COLS || y >= ROWS) return true;
  return blockers[y * COLS + x] === 2;
}

function isOccluder(blockers, x, y) {
  if (x < 0 || y < 0 || x >= COLS || y >= ROWS) return true;
  return blockers[y * COLS + x] !== 0;
}

const scratch = {
  blockers: new Uint8Array(COLS * ROWS),
  field: new Float32Array(GW * GH * STRIDE),
  exposure: new Map(),
  stats: { contacts: 0, casts: 0, aoBands: 0, wallCasts: 0, worstGap: -Infinity, lights: 0, fieldMs: 0 },
};

function buildBlockers(game) {
  const grid = scratch.blockers;
  for (let y = 0; y < ROWS; y++) {
    for (let x = 0; x < COLS; x++) grid[y * COLS + x] = tileClass(game, x, y);
  }
  return grid;
}

function rayExposure(game, light, x, y, rays) {
  if (rays < 3) return game.los(light.x, light.y, x, y) ? 1 : 0;
  const dx = x - light.x;
  const dy = y - light.y;
  const dist = Math.hypot(dx, dy) || 1;
  const px = (-dy / dist) * 6;
  const py = (dx / dist) * 6;
  let ok = game.los(light.x, light.y, x, y) ? 1 : 0;
  ok += game.los(light.x + px, light.y + py, x, y) ? 1 : 0;
  ok += game.los(light.x - px, light.y - py, x, y) ? 1 : 0;
  return ok / 3;
}

function sampleExposure(game, light, x, y, rays, quantize) {
  if (!light || light.kind === 'dir') return 1;
  const dist = Math.hypot(x - light.x, y - light.y);
  if (dist < 14) return 1;
  const cache = game._shade?.exposure;
  let key = '';
  if (quantize && cache) {
    key = `${light.id}:${rays}:${x >> 3}:${y >> 3}`;
    const hit = cache.get(key);
    if (hit !== undefined) return hit;
  }
  const value = rayExposure(game, light, x, y, rays);
  if (key) cache.set(key, value);
  return value;
}

function pointFalloff(light, x, y) {
  const dx = x - light.x;
  const dy = y - light.y;
  const dist = Math.hypot(dx, dy);
  if (dist >= light.radius || light.intensity <= 0) return 0;
  let edge = 1;
  if (light.kind === 'cone') {
    const diff = Math.abs(angDiff(Math.atan2(dy, dx), light.aim));
    if (diff > light.cone) return 0;
    edge = 1 - diff / light.cone;
    edge *= edge;
  }
  const fall = (1 - dist / light.radius) ** 1.45;
  return light.intensity * fall * edge;
}

export function collectLights(game) {
  const lights = [];
  const reduce = !!game.settings?.reduceFlash;
  const time = game.time || 0;
  const powered = !!game.state.flags.powerOn;
  const roomLamp = new Map();

  for (const lamp of PRACTICALS) {
    const flicker = lampFlicker(time, lamp, reduce);
    const base = lamp.power === 'mains' && !powered ? lamp.ember : lamp.intensity;
    const intensity = base * flicker;
    if (intensity < 0.02) continue;
    const light = {
      id: lamp.id,
      kind: 'point',
      x: (lamp.tx + 0.5) * TILE,
      y: (lamp.ty + 0.5) * TILE,
      z: lamp.z,
      radius: lamp.radius,
      intensity,
      flicker,
      base,
      rgb: lamp.rgb,
      tx: lamp.tx,
      ty: lamp.ty,
    };
    lights.push(light);
    const room = roomAt(lamp.tx, lamp.ty);
    const prev = roomLamp.get(room);
    if (!prev || intensity > prev.intensity) roomLamp.set(room, light);
  }

  for (const door of game.world.doors) {
    const motion = game.state.doors[door.id];
    const open = !(motion && doorSeals(door, motion));
    let spill = 0;
    let rgb = MOON_RGB;
    let flicker = 1;
    let source = null;
    for (const [dx, dy] of DIRS) {
      const tx = door.x + dx;
      const ty = door.y + dy;
      const tile = game.world.tiles[ty]?.[tx];
      if (!tile || tile === 0 || tile === 9) continue;
      const room = roomAt(tx, ty);
      if (room === 'yard') {
        const moon = 0.4;
        if (moon > spill) {
          spill = moon;
          rgb = MOON_RGB;
          flicker = 1;
          source = null;
        }
      }
      const lamp = roomLamp.get(room);
      if (lamp && lamp.intensity > spill) {
        spill = lamp.intensity;
        rgb = lamp.rgb;
        flicker = lamp.flicker;
        source = lamp.id;
      }
    }
    const scale = open ? 0.48 : 0.14;
    const intensity = spill * scale;
    if (intensity < 0.035) continue;
    lights.push({
      id: `door:${door.id}`,
      kind: 'point',
      x: (door.x + 0.5) * TILE,
      y: (door.y + 0.5) * TILE,
      z: open ? 42 : 20,
      radius: open ? (door.id === 'yard' ? 92 : 60) : 24,
      intensity,
      flicker,
      base: spill,
      rgb,
      source,
      open,
    });
  }

  const player = game.state.player;
  if (game.state.hasLight && player.lightOn && player.battery > 0) {
    const fx = Math.cos(player.facing);
    const fy = Math.sin(player.facing);
    let x = player.x + fx * 12;
    let y = player.y + fy * 12;
    if (game.tileBlocks?.(x, y, false)) {
      x = player.x + fx * 4;
      y = player.y + fy * 4;
    }
    lights.push({
      id: 'flash',
      kind: 'cone',
      x,
      y,
      z: 24,
      radius: TUNING.lightRange,
      intensity: flashlightFlicker(time, player.battery, reduce),
      flicker: flashlightFlicker(time, player.battery, reduce),
      aim: player.facing,
      cone: TUNING.cone,
      rgb: [232, 214, 176],
    });
  }

  lights.push({
    id: 'moon',
    kind: 'dir',
    dirX: MOON_DIR.x,
    dirY: MOON_DIR.y,
    z: 170,
    radius: Infinity,
    intensity: 1,
    flicker: 1,
    rgb: MOON_RGB,
  });
  return lights;
}

function combineIllumination(beam, pool, ao) {
  const capped = Math.min(0.82, pool);
  return clamp((1 - (1 - clamp(beam, 0, 1)) * (1 - capped)) * ao, 0, 1);
}

export function shadeIllumination(game, x, y) {
  const shade = game._shade;
  if (!shade) return game.lightAt(x, y);
  const tx = Math.floor(x / TILE);
  const ty = Math.floor(y / TILE);
  if (isWall(shade.blockers, tx, ty)) return 0;
  const beam = game.lightAt(x, y);
  const room = roomAt(tx, ty);
  let pool = moonStrength(room).intensity;
  let cr = MOON_RGB[0] * pool;
  let cg = MOON_RGB[1] * pool;
  let cb = MOON_RGB[2] * pool;
  for (const light of shade.lights) {
    if (light.kind === 'dir' || light.kind === 'cone') continue;
    const fall = pointFalloff(light, x, y);
    if (fall <= 0) continue;
    const exposed = sampleExposure(game, light, x, y, 1, true);
    if (exposed <= 0) continue;
    const add = fall * exposed;
    pool += add;
    cr += light.rgb[0] * add;
    cg += light.rgb[1] * add;
    cb += light.rgb[2] * add;
  }
  const ao = aoMultiplier(shade.blockers, x, y);
  return combineIllumination(beam, pool, ao);
}

function buildField(game, shade) {
  const { ox, oy, blockers, lights } = shade;
  const field = shade.field;
  for (let gy = 0; gy < GH; gy++) {
    const wy = oy + (gy + 0.5) * CELL;
    const ty = Math.floor(wy / TILE);
    for (let gx = 0; gx < GW; gx++) {
      const wx = ox + (gx + 0.5) * CELL;
      const i = (gy * GW + gx) * STRIDE;
      const tx = Math.floor(wx / TILE);
      if (isWall(blockers, tx, ty)) {
        field[i + 5] = -1;
        continue;
      }
      const beam = game.lightAt(wx, wy);
      const room = roomAt(tx, ty);
      const moon = moonStrength(room).intensity;
      field[i] = beam;
      field[i + 1] = moon;
      field[i + 2] = MOON_RGB[0] * moon;
      field[i + 3] = MOON_RGB[1] * moon;
      field[i + 4] = MOON_RGB[2] * moon;
      field[i + 5] = aoMultiplier(blockers, wx, wy);
    }
  }
  for (const light of lights) {
    if (light.kind !== 'point' || light.intensity < 0.02) continue;
    const x0 = Math.max(0, Math.floor((light.x - light.radius - ox) / CELL));
    const y0 = Math.max(0, Math.floor((light.y - light.radius - oy) / CELL));
    const x1 = Math.min(GW - 1, Math.floor((light.x + light.radius - ox) / CELL));
    const y1 = Math.min(GH - 1, Math.floor((light.y + light.radius - oy) / CELL));
    for (let gy = y0; gy <= y1; gy++) {
      const wy = oy + (gy + 0.5) * CELL;
      for (let gx = x0; gx <= x1; gx++) {
        const i = (gy * GW + gx) * STRIDE;
        if (field[i + 5] < 0) continue;
        const wx = ox + (gx + 0.5) * CELL;
        const fall = pointFalloff(light, wx, wy);
        if (fall <= 0) continue;
        const exposed = sampleExposure(game, light, wx, wy, 1, true);
        if (exposed <= 0) continue;
        const add = fall * exposed;
        field[i + 1] += add;
        field[i + 2] += light.rgb[0] * add;
        field[i + 3] += light.rgb[1] * add;
        field[i + 4] += light.rgb[2] * add;
      }
    }
  }
}

export function beginShade(game, ox, oy) {
  const started = performance.now();
  const blockers = buildBlockers(game);
  const lights = collectLights(game);
  const field = scratch.field;
  field.fill(0);
  const exposure = scratch.exposure;
  exposure.clear();
  const stats = scratch.stats;
  stats.contacts = 0;
  stats.casts = 0;
  stats.aoBands = 0;
  stats.wallCasts = 0;
  stats.worstGap = -Infinity;
  stats.lights = lights.length;
  stats.fieldMs = 0;
  game._shade = { ox, oy, blockers, lights, field, exposure, stats };
  buildField(game, game._shade);
  stats.fieldMs = performance.now() - started;
  return game._shade;
}

export function shadeStats(game) {
  return game._shade?.stats || null;
}

function keyStrength(game, lights, x, y) {
  let key = 0;
  for (const light of lights) {
    if (light.kind === 'dir') continue;
    const fall = pointFalloff(light, x, y);
    if (fall < 0.06) continue;
    const exposed = sampleExposure(game, light, x, y, 1, false);
    if (exposed <= 0) continue;
    const value = fall * exposed;
    if (value > key) key = value;
  }
  return key;
}

export function projectCasts(feetX, feetY, actor, lights, game, opts = {}) {
  const room = roomAt(Math.floor(feetX / TILE), Math.floor(feetY / TILE));
  const key = keyStrength(game, lights, feetX, feetY);
  const casts = [];
  for (const light of lights) {
    let dirX = 0;
    let dirY = 0;
    let length = 0;
    let alpha = 0;
    let soft = 0.4;
    if (light.kind === 'dir') {
      const moon = moonStrength(room);
      const cast = moon.cast * (key > 0.18 ? 0.2 : 1);
      if (cast < 0.04) continue;
      dirX = light.dirX;
      dirY = light.dirY;
      length = clamp((actor.h * moon.virtualDist) / light.z, 4, 34);
      alpha = cast * 0.7 * actor.cast;
      soft = 0.78;
    } else if (light.id === 'flash' && opts.self) {
      const facing = opts.facing || 0;
      dirX = -Math.cos(facing);
      dirY = -Math.sin(facing);
      length = clamp(20 + actor.h * 0.5, 18, 34);
      alpha = light.intensity * 0.56 * actor.cast;
      soft = 0.46;
    } else if (light.kind === 'point' || light.kind === 'cone') {
      const dx = feetX - light.x;
      const dy = feetY - light.y;
      const dist = Math.hypot(dx, dy);
      if (dist < 1 || dist > light.radius || light.intensity <= 0.02) continue;
      if (light.kind === 'cone') {
        const diff = Math.abs(angDiff(Math.atan2(dy, dx), light.aim));
        if (diff > light.cone) continue;
        alpha = (1 - diff / light.cone) ** 2;
      } else alpha = 1;
      const exposed = sampleExposure(game, light, feetX, feetY, 3, false);
      if (exposed < 0.08) continue;
      dirX = dx / dist;
      dirY = dy / dist;
      length = clamp((actor.h * dist) / light.z * 1.4, 10, 72);
      const fall = (1 - dist / light.radius) ** 0.7;
      alpha *= Math.min(0.72, light.intensity * 2.1) * fall * exposed * actor.cast;
      soft = clamp(0.18 + (length / 72) * 0.78, 0.18, 0.95);
    } else continue;
    if (alpha < 0.025) continue;
    const along = length * 0.36;
    const len = length * 0.58;
    casts.push({
      id: light.id,
      dirX,
      dirY,
      along,
      len,
      wid: Math.max(1.2, actor.radius * (0.76 + soft * 0.58)),
      alpha: Math.min(0.58, alpha),
      soft,
      length,
    });
  }
  casts.sort((a, b) => b.alpha - a.alpha || (a.id < b.id ? -1 : 1));
  let budget = 0.8;
  const kept = [];
  for (const cast of casts) {
    if (budget < 0.03) break;
    const alpha = Math.min(cast.alpha, budget);
    budget -= alpha * 0.7;
    kept.push({ ...cast, alpha });
  }
  return kept.slice(0, 2);
}

function fillPixelOval(ctx, sx, sy, rx, ry, color) {
  const cx = Math.round(sx);
  const cy = Math.round(sy);
  const radX = Math.max(1, rx);
  const radY = Math.max(1, ry);
  ctx.fillStyle = color;
  const y0 = Math.ceil(-radY);
  const y1 = Math.floor(radY);
  for (let y = y0; y <= y1; y++) {
    const span = radX * Math.sqrt(Math.max(0, 1 - (y / radY) ** 2));
    const half = Math.max(0, Math.round(span));
    ctx.fillRect(cx - half, cy + y, half * 2 + 1, 1);
  }
}

export function drawContactShadow(ctx, sx, sy, actor, press = 1) {
  const rx = Math.max(3.4, actor.radius * press);
  const ry = Math.max(1.8, actor.radius * 0.46);
  const layers = [
    [rx * 2.55, ry * 2.5, 0.42],
    [rx * 1.6, ry * 1.6, 0.58],
    [rx * 0.95, ry * 1.05, 0.76],
  ];
  for (const [w, h, alpha] of layers) {
    fillPixelOval(ctx, sx, sy, w, h, `rgba(2, 3, 6, ${alpha})`);
  }
}

export function drawCastShadow(ctx, sx, sy, cast, visibility = 1) {
  const alpha = cast.alpha * visibility;
  if (alpha < 0.02) return;
  const reach = Math.max(4, cast.length);
  const step = Math.max(3, reach / 8);
  for (let d = 0; d <= reach + 0.01; d += step) {
    const t = d / reach;
    const fade = (1 - t) ** 1.2;
    const rx = Math.max(1.4, cast.wid * (1.25 - t * 0.7));
    const ry = Math.max(1.2, cast.wid * (0.58 - t * 0.2));
    const ink = Math.min(0.78, alpha * (0.62 + 0.85 * fade));
    fillPixelOval(
      ctx,
      sx + cast.dirX * d,
      sy + cast.dirY * d,
      rx,
      ry,
      `rgba(1, 2, 4, ${ink.toFixed(3)})`,
    );
  }
}

export function drawActorShadows(ctx, game, worldX, worldY, name, opts = {}) {
  const shade = game._shade;
  const base = ACTORS[name] || ACTORS.prop;
  if (!shade || !base) return;
  const actor = opts.radius || opts.h
    ? { ...base, radius: opts.radius || base.radius, h: opts.h || base.h }
    : base;
  const anchor = spriteAnchor(worldX, worldY, shade.ox, shade.oy, actor.foot);
  if (anchor.x < -64 || anchor.y < -64 || anchor.x > VIEW_W + 64 || anchor.y > VIEW_H + 64) return;
  if (!opts.castOnly) {
    drawContactShadow(ctx, anchor.x, anchor.y, actor, opts.moving ? 1.06 : 1);
    shade.stats.contacts += 1;
  }
  if (opts.contactOnly) return;
  const feetX = worldX;
  const feetY = worldY + actor.foot;
  const casts = projectCasts(feetX, feetY, actor, shade.lights, game, opts);
  const visibility = opts.overDark ? Math.max(0.42, shadeIllumination(game, feetX, feetY)) : 1;
  for (const cast of casts) {
    const gap = cast.along - cast.len;
    if (gap > shade.stats.worstGap) shade.stats.worstGap = gap;
    drawCastShadow(ctx, anchor.x, anchor.y, cast, visibility);
    shade.stats.casts += 1;
  }
}

function paintBands(ctx, sx, sy, inwardX, inwardY, depth, alphas) {
  const step = Math.max(2, Math.round(depth / alphas.length));
  for (let i = 0; i < alphas.length; i++) {
    const offset = i * step;
    ctx.fillStyle = `rgba(0, 0, 0, ${alphas[i]})`;
    if (inwardY === 1) ctx.fillRect(sx, sy + offset, TILE, step);
    else if (inwardY === -1) ctx.fillRect(sx, sy + TILE - step - offset, TILE, step);
    else if (inwardX === 1) ctx.fillRect(sx + offset, sy, step, TILE);
    else ctx.fillRect(sx + TILE - step - offset, sy, step, TILE);
  }
}

export function drawEnvironmentShade(ctx, game, ox, oy, x0, y0, x1, y1) {
  const shade = game._shade;
  if (!shade) return;
  const { blockers, lights } = shade;
  const aoAlpha = [0.32, 0.16, 0.07, 0.03];
  for (let ty = y0; ty <= y1; ty++) {
    for (let tx = x0; tx <= x1; tx++) {
      if (isOccluder(blockers, tx, ty)) continue;
      const tile = game.world.tiles[ty]?.[tx];
      if (!tile) continue;
      const sx = Math.round(tx * TILE - ox);
      const sy = Math.round(ty * TILE - oy);
      const north = isOccluder(blockers, tx, ty - 1);
      const south = isOccluder(blockers, tx, ty + 1);
      const west = isOccluder(blockers, tx - 1, ty);
      const east = isOccluder(blockers, tx + 1, ty);
      if (north) {
        paintBands(ctx, sx, sy, 0, 1, 8, aoAlpha);
        shade.stats.aoBands += 1;
      }
      if (south) {
        paintBands(ctx, sx, sy, 0, -1, 8, aoAlpha);
        shade.stats.aoBands += 1;
      }
      if (west) {
        paintBands(ctx, sx, sy, 1, 0, 8, aoAlpha);
        shade.stats.aoBands += 1;
      }
      if (east) {
        paintBands(ctx, sx, sy, -1, 0, 8, aoAlpha);
        shade.stats.aoBands += 1;
      }
      if (north && west) {
        ctx.fillStyle = 'rgba(0, 0, 0, 0.22)';
        ctx.fillRect(sx, sy, 10, 10);
      }
      if (north && east) {
        ctx.fillStyle = 'rgba(0, 0, 0, 0.22)';
        ctx.fillRect(sx + TILE - 10, sy, 10, 10);
      }
      if (south && west) {
        ctx.fillStyle = 'rgba(0, 0, 0, 0.22)';
        ctx.fillRect(sx, sy + TILE - 10, 10, 10);
      }
      if (south && east) {
        ctx.fillStyle = 'rgba(0, 0, 0, 0.22)';
        ctx.fillRect(sx + TILE - 10, sy + TILE - 10, 10, 10);
      }

      const cx = (tx + 0.5) * TILE;
      const cy = (ty + 0.5) * TILE;
      let first = null;
      let second = null;
      for (const light of lights) {
        if (light.kind === 'dir') continue;
        const fall = pointFalloff(light, cx, cy);
        if (fall < 0.05) continue;
        if (!first || fall > first.fall) {
          second = first;
          first = { light, fall };
        } else if (!second || fall > second.fall) second = { light, fall };
      }
      const keys = first ? (second ? [first, second] : [first]) : [];
      for (const key of keys) {
        const exposed = sampleExposure(game, key.light, cx, cy, 1, true);
        if (exposed < 0.15) continue;
        const dx = cx - key.light.x;
        const dy = cy - key.light.y;
        const dist = Math.hypot(dx, dy) || 1;
        stampWallCast(ctx, shade, sx, sy, dx / dist, dy / dist, key.fall * exposed, north, south, west, east);
      }
      const room = roomAt(tx, ty);
      const moon = moonStrength(room);
      if (moon.intensity >= 0.12) {
        stampWallCast(ctx, shade, sx, sy, MOON_DIR.x, MOON_DIR.y, moon.intensity * 0.85, north, south, west, east);
      }
    }
  }
}

function stampWallCast(ctx, shade, sx, sy, dirX, dirY, influence, north, south, west, east) {
  const sides = [
    north ? { x: 0, y: 1 } : null,
    south ? { x: 0, y: -1 } : null,
    west ? { x: 1, y: 0 } : null,
    east ? { x: -1, y: 0 } : null,
  ];
  for (const side of sides) {
    if (!side) continue;
    const dot = dirX * side.x + dirY * side.y;
    if (dot <= 0.08) continue;
    const fade = Math.min(1, (dot - 0.08) / 0.45) * Math.min(1, influence * 1.6);
    if (fade < 0.06) continue;
    const depth = Math.round(8 + 16 * fade);
    paintBands(ctx, sx, sy, side.x, side.y, depth, [0.14 * fade, 0.07 * fade, 0.03 * fade]);
    shade.stats.wallCasts += 1;
  }
}

const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];

function ensureLightBuffer() {
  if (lightImage) return true;
  let canvas = null;
  if (typeof OffscreenCanvas !== 'undefined') canvas = new OffscreenCanvas(GW, GH);
  else if (typeof document !== 'undefined' && document.createElement) {
    canvas = document.createElement('canvas');
    canvas.width = GW;
    canvas.height = GH;
  }
  if (!canvas) return false;
  const ctx = canvas.getContext('2d');
  if (!ctx) return false;
  lightCanvas = canvas;
  lightCtx = ctx;
  lightImage = ctx.createImageData(GW, GH);
  return true;
}

function lightChannels(illum, beam, pool, cr, cg, cb, gx, gy) {
  let light = illum;
  if (light > 0.025 && light < 0.94) {
    const b = BAYER[(gy & 3) * 4 + (gx & 3)] / 16;
    light = clamp(light + (b - 0.5) * 0.028, 0, 1);
  }
  const factor = shadeFactor(light);
  const cool = 1 - factor;
  const weight = pool > 0.001 ? Math.min(1, pool / (pool + beam + 0.001)) : 0;
  const tint = (channel) => (1 - weight) + weight * (0.55 + 0.45 * (channel / 255));
  return [
    Math.round(255 * factor * (1 - cool * 0.2) * tint(cr)),
    Math.round(255 * factor * (1 - cool * 0.1) * tint(cg)),
    Math.round(255 * factor * tint(cb)),
  ];
}

export function drawLightMap(ctx, game) {
  const shade = game._shade;
  if (!shade) return;
  const prev = ctx.globalCompositeOperation;
  ctx.globalCompositeOperation = 'multiply';
  if (ensureLightBuffer()) {
    const data = lightImage.data;
    const field = shade.field;
    for (let gy = 0; gy < GH; gy++) {
      for (let gx = 0; gx < GW; gx++) {
        const i = (gy * GW + gx) * STRIDE;
        const di = (gy * GW + gx) * 4;
        if (field[i + 5] < 0) {
          data[di] = 0;
          data[di + 1] = 0;
          data[di + 2] = 0;
          data[di + 3] = 255;
          continue;
        }
        const beam = field[i];
        const pool = field[i + 1];
        const weight = pool > 0.0001 ? pool : 1;
        const illum = combineIllumination(beam, pool, field[i + 5]);
        const channels = lightChannels(illum, beam, pool, field[i + 2] / weight, field[i + 3] / weight, field[i + 4] / weight, gx, gy);
        data[di] = channels[0];
        data[di + 1] = channels[1];
        data[di + 2] = channels[2];
        data[di + 3] = 255;
      }
    }
    lightCtx.putImageData(lightImage, 0, 0);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(lightCanvas, 0, 0, VIEW_W, VIEW_H);
  } else {
    const field = shade.field;
    for (let gy = 0; gy < GH; gy++) {
      for (let gx = 0; gx < GW; gx++) {
        const i = (gy * GW + gx) * STRIDE;
        let r = 0;
        let g = 0;
        let b = 0;
        if (field[i + 5] >= 0) {
          const beam = field[i];
          const pool = field[i + 1];
          const weight = pool > 0.0001 ? pool : 1;
          const illum = combineIllumination(beam, pool, field[i + 5]);
          const channels = lightChannels(illum, beam, pool, field[i + 2] / weight, field[i + 3] / weight, field[i + 4] / weight, gx, gy);
          r = channels[0];
          g = channels[1];
          b = channels[2];
        }
        ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
        ctx.fillRect(gx * CELL, gy * CELL, CELL, CELL);
      }
    }
  }
  ctx.globalCompositeOperation = prev;
}

export const PRACTICAL_LIST = PRACTICALS;
