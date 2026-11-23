import { CaptionQueue } from './captions.mjs';
import {
  CUE,
  ENEMY_STEP_MARKS,
  FOOT_MARKS,
  HEAVY_ROOMS,
  gaitRate,
  hashPhase,
  phaseMarksCrossed,
} from './cues.mjs';
import { ENEMY, ITEMS, MOM_LINES, MOM_LINES_LATE, TUNING, describeGoal } from './data.mjs';
import { createDirector } from './director.mjs';
import {
  SWING_FRAMES,
  bodyBlocked,
  createDoorState,
  doorBlocksPoint,
  interactPoint,
  planDoorUse,
  stepDoor,
  syncDoorMotion,
} from './doors.mjs';
import { flashlightFlicker } from './lighting.mjs';
import { render, VIEW_H, VIEW_W } from './render.mjs';
import {
  COLS,
  FURNITURE,
  ROOMS,
  ROWS,
  SPAWN,
  TILE,
  createWorld,
  floorName,
  passable,
  roomAt,
} from './world.mjs';

export const SAVE_KEY = 'dont-look-save-v1';

const DIRS = [[1, 0], [-1, 0], [0, 1], [0, -1]];
const CAR_LEAVE = 26;

function angDiff(a, b) {
  let d = a - b;
  while (d > Math.PI) d -= Math.PI * 2;
  while (d < -Math.PI) d += Math.PI * 2;
  return d;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

export class Game {
  constructor(canvas, audio) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d', { alpha: false });
    this.audio = audio;
    this.world = createWorld();
    this.input = { x: 0, y: 0, sneak: false, run: false, use: false };
    this.settings = {
      captions: true,
      reduceFlash: (typeof window !== 'undefined' && window.matchMedia?.('(prefers-reduced-motion: reduce)').matches) || false,
      volume: 0.85,
    };
    this.mouseAim = false;
    this.state = null;
    this.time = 0;
    this.uiTime = 0;
    this.captions = new CaptionQueue();
    this.caption = null;
    this.roomBanner = null;
    this.shake = 0;
    this.flash = 0;
    this.grinFlash = 0;
    this.hurtFlash = 0;
    this.cam = { x: 0, y: 0, ready: false };
    this.dropN = 1;
    this.momTimer = 3.4;
    this.momIndex = 0;
    this.creak = 6;
    this.director = createDirector();
    this.ambienceN = 0;
    this.danger = 0;
  }

  newGame() {
    const doors = {};
    for (const door of this.world.doors) doors[door.id] = createDoorState(door);
    const containers = {};
    for (const prop of this.world.props) {
      if (prop.kind === 'container') containers[prop.id] = prop.items.map((item) => ({ ...item }));
    }
    this.state = {
      v: 1,
      player: {
        x: (SPAWN.x + 0.5) * TILE,
        y: (SPAWN.y + 0.5) * TILE,
        facing: Math.PI,
        hp: 100,
        stamina: 100,
        battery: 0,
        lightOn: false,
        iframes: 0,
        dodge: 0,
        dodgeX: 0,
        dodgeY: 0,
        atk: 0,
        step: 0.2,
        phase: 0,
        sneaking: false,
        running: false,
      },
      hasLight: false,
      inv: [],
      stash: [],
      equipped: -1,
      selected: 0,
      doors,
      taken: {},
      containers,
      dropped: [],
      enemies: this.world.spawns.map((spawn) => this.makeEnemy(spawn)),
      flags: {
        read: {},
        tookBaseKey: false,
        tookCarKeys: false,
        tookFuse: false,
        tookGas: false,
        powerOn: false,
        finale: false,
        darker: false,
        enteredYard: false,
        cameraLooks: 0,
        tv: 0,
        mirror: false,
      },
      installed: { fuse: false, keys: false, gas: false, photo: false },
      seen: { mud: true },
      room: 'mud',
      decals: [],
      noises: [],
      screen: null,
      hold: 0,
      dead: false,
      won: false,
      fx: [],
    };
    this.dropN = 1;
    this.time = 0;
    this.uiTime = 0;
    this.momTimer = 3.2;
    this.momIndex = 0;
    this.creak = 6.5;
    this.captions.clear();
    this.caption = null;
    this.showRoom(ROOMS.mud.name);
    this.shake = 0;
    this.flash = 0;
    this.grinFlash = 0;
    this.hurtFlash = 0;
    this.mouseAim = false;
    this.director.reset();
    this.ambienceN = 0;
    this.danger = 0;
    this.snapCamera();
    this.say("She's calling. The flashlight is in the foyer.");
  }

  makeEnemy(spawn) {
    const stats = ENEMY[spawn.kind];
    return {
      id: spawn.id,
      kind: spawn.kind,
      x: (spawn.x + 0.5) * TILE,
      y: (spawn.y + 0.5) * TILE,
      hp: stats.hp,
      speed: stats.speed,
      dmg: stats.dmg,
      reach: stats.reach,
      radius: stats.radius,
      alive: true,
      awake: spawn.wake === 'now',
      wake: spawn.wake,
      wakeDist: spawn.wakeDist ?? 40,
      lightWake: !!spawn.lightWake,
      silent: !!spawn.silent,
      flag: spawn.flag || null,
      patrol: (spawn.patrol || [[spawn.x, spawn.y]]).map(([x, y]) => ({
        x: (x + 0.5) * TILE,
        y: (y + 0.5) * TILE,
      })),
      pi: 0,
      state: 'patrol',
      cool: 0,
      lost: 0,
      path: null,
      nextPath: 0,
      pathIndex: 0,
      openingDoor: null,
      opening: 0,
      stunUntil: 0,
      downUntil: 0,
      phase: hashPhase(spawn.id),
      groan: 2 + Math.random() * 3,
      risen: 0,
      revealed: false,
      stuck: 0,
      goalX: 0,
      goalY: 0,
    };
  }

  serialize() {
    const s = this.state;
    const enemies = s.enemies.map((enemy) => ({
      id: enemy.id,
      x: enemy.x,
      y: enemy.y,
      hp: enemy.hp,
      speed: enemy.speed,
      alive: enemy.alive,
      awake: enemy.awake,
      state: enemy.state === 'chase' ? 'chase' : 'patrol',
      pi: enemy.pi,
      risen: enemy.risen,
      lost: 0,
    }));
    return {
      v: 1,
      player: {
        x: s.player.x,
        y: s.player.y,
        facing: s.player.facing,
        hp: s.player.hp,
        stamina: s.player.stamina,
        battery: s.player.battery,
        lightOn: s.player.lightOn,
      },
      hasLight: s.hasLight,
      inv: s.inv,
      stash: s.stash,
      equipped: s.equipped,
      doors: s.doors,
      taken: s.taken,
      containers: s.containers,
      dropped: s.dropped,
      enemies,
      flags: s.flags,
      installed: s.installed,
      seen: s.seen,
      room: s.room,
      decals: s.decals,
    };
  }

  save(quiet) {
    try {
      localStorage.setItem(SAVE_KEY, JSON.stringify(this.serialize()));
      if (!quiet) this.say('The slide holds this room. For now.');
      return true;
    } catch {
      if (!quiet) this.say("The slide won't stick in this browser.");
      return false;
    }
  }

  hydrate(data) {
    this.newGame();
    if (!data || data.v !== 1) throw new Error('bad save');
    const s = this.state;
    Object.assign(s.player, data.player);
    s.player.iframes = 0;
    s.player.dodge = 0;
    s.player.atk = 0;
    s.hasLight = !!data.hasLight;
    s.inv = data.inv || [];
    s.stash = data.stash || [];
    s.equipped = data.equipped ?? -1;
    if (s.equipped >= s.inv.length) s.equipped = -1;
    s.taken = data.taken || {};
    s.containers = data.containers || s.containers;
    s.dropped = data.dropped || [];
    s.installed = { ...s.installed, ...data.installed };
    s.flags = {
      ...s.flags,
      ...data.flags,
      read: { ...s.flags.read, ...(data.flags?.read || {}) },
    };
    s.seen = { ...s.seen, ...data.seen };
    s.room = data.room || s.room;
    s.decals = data.decals || [];
    for (const [id, saved] of Object.entries(data.doors || {})) {
      const st = s.doors[id];
      if (!st) continue;
      Object.assign(st, saved);
      if (typeof saved.angle !== 'number') st.angle = st.open ? 1 : 0;
    }
    this.normalizeDoors();
    for (const saved of data.enemies || []) {
      const enemy = s.enemies.find((item) => item.id === saved.id);
      if (!enemy) continue;
      Object.assign(enemy, saved, { path: null, openingDoor: null, stunUntil: 0, downUntil: 0 });
    }
    if (this.hitsSolid(s.player, 8)) {
      s.player.x = (SPAWN.x + 0.5) * TILE;
      s.player.y = (SPAWN.y + 0.5) * TILE;
    }
    if (s.flags.finale) {
      s.player.iframes = 1.4;
      const grin = s.enemies.find((enemy) => enemy.kind === 'grin');
      if (grin) grin.stunUntil = 1.4;
    }
    s.screen = null;
    s.dead = false;
    s.won = false;
    this.captions.clear();
    this.caption = null;
    this.showRoom(ROOMS[s.room]?.name || '');
    this.snapCamera();
    this.say('The slide lets go. You are still here.');
  }

  snapCamera() {
    const p = this.state.player;
    this.cam.x = clamp(p.x - VIEW_W / 2, 0, COLS * TILE - VIEW_W);
    this.cam.y = clamp(p.y - VIEW_H / 2, 0, ROWS * TILE - VIEW_H);
    this.cam.ready = true;
  }

  armArrival() {
    if (!this.state) return;
    this.snapCamera();
    if (!this.captionMotion()) return;
    this.cam.x += 34;
    this.cam.y += 16;
  }

  update(dt) {
    if (!this.state) return;
    this.uiTime += dt;
    this.flash = Math.max(0, this.flash - dt * 1.4);
    this.grinFlash = Math.max(0, this.grinFlash - dt * 1.15);
    this.hurtFlash = Math.max(0, this.hurtFlash - dt * 1.6);
    this.shake = Math.max(0, this.shake - dt * 2.2);
    if (this.state.fx?.length) {
      const age = Math.min(0.05, dt);
      for (const bit of this.state.fx) bit.age += age;
      this.state.fx = this.state.fx.filter((bit) => bit.age < bit.life);
    }
    const modal = this.state.screen
      && this.state.screen.type !== 'dead'
      && this.state.screen.type !== 'ending';
    if (!this.state.dead && !this.state.won && !modal) {
      this.caption = this.captions.advance(Math.min(0.05, dt), this.captionMotion());
    }
    const step = Math.min(0.033, dt);
    const halted = !!(this.state.screen || this.state.dead || this.state.won);
    this.playDirector(this.director.update(step, {
      reduce: !!this.settings.reduceFlash,
      danger: this.danger || 0,
      time: this.time,
    }));
    if (halted) {
      this.updateDoors(step);
      this.audio.mix({
        danger: 0.05,
        humVol: 0.004,
        humPan: 0,
        distorted: !!this.state.flags.tookBaseKey,
        room: this.state.room,
        paused: true,
        reduce: !!this.settings.reduceFlash,
        time: this.time,
      });
      return;
    }
    this.time += step;
    this.updateDoors(step);
    this.updatePlayer(step);
    this.state.noises = this.state.noises.filter((noise) => noise.until > this.time);
    this.updateRoom();
    this.updateMom(step);
    this.updateCreak(step);
    for (const enemy of this.state.enemies) this.updateEnemy(enemy, step);
    if (!this.state.dead && this.state.flags.finale && this.nearCar(CAR_LEAVE) && this.input.use) {
      this.state.hold += step;
      if (this.state.hold >= TUNING.holdTime) this.win();
    } else this.state.hold = Math.max(0, this.state.hold - step * 1.7);
    this.updateCamera(step);
    this.mixAudio();
  }

  updatePlayer(dt) {
    const p = this.state.player;
    if (p.iframes > 0) p.iframes -= dt;
    if (p.atk > 0) p.atk -= dt;
    if (this.director.locked) {
      p.moving = false;
      if (p.dodge > 0) p.dodge -= dt;
      return;
    }
    if (p.dodge > 0) {
      p.dodge -= dt;
      this.tryMove(p, p.dodgeX * dt, p.dodgeY * dt, 9);
      return;
    }
    let x = this.input.x;
    let y = this.input.y;
    const mag = Math.hypot(x, y);
    const moving = mag > 0.12;
    if (mag > 1) {
      x /= mag;
      y /= mag;
    }
    const sneak = this.input.sneak && !this.input.run;
    const run = this.input.run && !this.input.sneak && p.stamina > 1 && moving;
    p.sneaking = sneak && moving;
    p.running = !!run;
    let speed = TUNING.walk;
    if (sneak && moving) speed = TUNING.sneak;
    else if (run) speed = TUNING.run;
    if (p.hp < 36) speed *= 0.9;
    p.moving = moving;
    if (moving) {
      if (!this.mouseAim) p.facing = Math.atan2(y, x);
      this.tryMove(p, x * speed * dt, y * speed * dt, 9);
      const prev = p.phase || 0;
      const next = prev + dt * gaitRate(!!run, sneak && moving);
      p.phase = next;
      if (phaseMarksCrossed(prev, next, FOOT_MARKS) > 0) {
        this.audio.foot(floorName(this.tileAt(p.x, p.y)), sneak ? 0.25 : run ? 1 : 0.55);
        this.makeNoise(sneak ? 14 : run ? 100 : 46, 0.28);
      }
      if (run) p.stamina = Math.max(0, p.stamina - TUNING.runDrain * dt);
    }
    if (!run) {
      const regen = moving ? TUNING.stamRegen * 0.45 : TUNING.stamRegen;
      p.stamina = Math.min(100, p.stamina + regen * dt);
    }
    if (p.lightOn && this.state.hasLight) {
      p.battery -= TUNING.lightDrain * dt;
      if (p.battery <= 0) {
        p.battery = 0;
        p.lightOn = false;
        this.say('The light dies.');
      }
    }
  }

  updateRoom() {
    const p = this.state.player;
    const id = roomAt(Math.floor(p.x / TILE), Math.floor(p.y / TILE));
    if (!id || id === this.state.room) return;
    const previous = this.state.room && ROOMS[this.state.room] ? ROOMS[this.state.room].name : '';
    this.state.room = id;
    this.state.seen[id] = true;
    if (ROOMS[id].name !== previous) this.showRoom(ROOMS[id].name);
    const heavy = HEAVY_ROOMS.has(id);
    this.director.requestTransition(`enter-${id}`, {
      heavy,
      once: heavy,
      reduce: !!this.settings.reduceFlash,
    });
    if (id === 'yard') this.state.flags.enteredYard = true;
    if (id === 'mom' && !this.state.flags.sawMom) {
      this.state.flags.sawMom = true;
      this.ambient('Breathing. Close. Not yours.');
    }
  }

  updateMom(dt) {
    this.momTimer -= dt;
    if (this.momTimer > 0 || this.state.flags.finale) return;
    this.momTimer = 17 + Math.random() * 8;
    const lines = this.state.flags.tookBaseKey ? MOM_LINES_LATE : MOM_LINES;
    const line = lines[this.momIndex % lines.length];
    this.momIndex += 1;
    this.voice(line);
  }

  updateCreak(dt) {
    this.creak -= dt;
    if (this.creak > 0) return;
    const room = this.state.room || 'default';
    const heavy = HEAVY_ROOMS.has(room);
    this.ambienceN += 1;
    this.creak = (heavy ? 5.2 : 7.4) + (this.ambienceN % 5) * 0.7;
    const p = this.state.player;
    const angle = (this.ambienceN * 2.399963) % (Math.PI * 2);
    const dist = 110 + (this.ambienceN % 4) * 28;
    const x = p.x + Math.cos(angle) * dist;
    const y = p.y + Math.sin(angle) * dist;
    const kind = this.audio.ambience?.({
      room,
      pan: this.pan(x, y),
      danger: this.danger || 0,
      index: this.ambienceN,
    }) || 'creak';
    if (kind === 'creak' && this.ambienceN % 2 === 0) {
      this.ambient(`A board creaks ${this.dirPhrase(x, y)}.`);
    }
  }

  updateEnemy(enemy, dt) {
    if (!enemy.alive) return;
    if (!enemy.awake) {
      this.considerWake(enemy);
      if (!enemy.awake) return;
    }
    if (enemy.cool > 0) enemy.cool -= dt;
    if (enemy.downUntil > this.time || enemy.stunUntil > this.time) return;
    const player = this.state.player;
    const distance = Math.hypot(enemy.x - player.x, enemy.y - player.y);
    const detected = this.detection(enemy, distance);
    if (detected) {
      enemy.state = 'chase';
      enemy.lost = 0;
      enemy.goalX = player.x;
      enemy.goalY = player.y;
      if (enemy.kind === 'grin' && this.beamTouches(enemy) && !enemy.revealed) {
        enemy.revealed = true;
        this.scare('grin-reveal', { story: true, face: true, weight: 1 });
        this.say('The smile.');
      }
    } else if (enemy.state === 'chase') {
      enemy.lost += dt;
      if (enemy.lost > 2) enemy.revealed = false;
      if (enemy.lost > (enemy.kind === 'grin' ? 7 : 4)) enemy.state = 'patrol';
    }
    if (distance < enemy.reach + 8 && player.iframes <= 0 && player.dodge <= 0 && enemy.cool <= 0) {
      enemy.cool = TUNING.enemyCool;
      this.hurt(enemy.dmg, enemy);
      if (this.state.dead) return;
    }
    enemy.groan -= dt;
    if (enemy.groan <= 0) {
      enemy.groan = (enemy.kind === 'grin' ? 3.5 : 5) + Math.random() * 4;
      const heard = this.distVol(enemy, enemy.kind === 'grin' ? 460 : 300);
      if (heard > 0.04) {
        this.audio.moan(this.pan(enemy.x, enemy.y), heard, enemy.kind);
        const visible = this.beamTouches(enemy) || distance < 36;
        if (!visible) {
          const word = enemy.kind === 'grin' ? 'Breathing' : 'Something shifts';
          this.ambient(`${word} ${this.dirPhrase(enemy.x, enemy.y)}.`);
        }
      }
    }
    if (this.tryOpenDoor(enemy, dt)) return;
    const goal = enemy.state === 'chase'
      ? { x: enemy.goalX, y: enemy.goalY }
      : enemy.patrol[enemy.pi % enemy.patrol.length];
    if (enemy.state !== 'chase' && Math.hypot(goal.x - enemy.x, goal.y - enemy.y) < 8) {
      enemy.pi += 1;
      return;
    }
    const before = enemy.x + enemy.y;
    this.steer(enemy, goal, dt);
    if (Math.abs(enemy.x + enemy.y - before) < 0.35) enemy.stuck += dt;
    else {
      enemy.stuck = 0;
      this.plantEnemy(enemy, dt);
    }
    if (enemy.stuck > 0.7) {
      enemy.stuck = 0;
      enemy.path = null;
      const angle = Math.random() * Math.PI * 2;
      this.tryMove(enemy, Math.cos(angle) * 18, Math.sin(angle) * 18, enemy.radius);
    }
  }

  considerWake(enemy) {
    const distance = Math.hypot(enemy.x - this.state.player.x, enemy.y - this.state.player.y);
    let wake = false;
    if (enemy.wake === 'flag' && enemy.flag && this.state.flags[enemy.flag]) wake = true;
    if (enemy.wake === 'near' && distance < enemy.wakeDist) wake = true;
    if (enemy.lightWake && distance < 110 && this.beamTouches(enemy)) wake = true;
    if (!enemy.silent && enemy.wake === 'flag' && distance < 34) wake = true;
    if (!enemy.silent) {
      for (const noise of this.state.noises) {
        if (noise.radius >= 160 && Math.hypot(enemy.x - noise.x, enemy.y - noise.y) < noise.radius) wake = true;
      }
    }
    if (!wake) return;
    enemy.awake = true;
    enemy.state = 'chase';
    if (enemy.lightWake) this.ambient('Something in the kitchen moves.');
    if (enemy.id === 'yardbug') this.ambient('Something in the yard is with you.');
  }

  detection(enemy, distance) {
    const sneak = this.state.player.sneaking;
    if (distance < (sneak ? 18 : 30)) return 'touch';
    if (this.beamTouches(enemy) && distance < (sneak ? 78 : 158)) return 'light';
    const hear = enemy.kind === 'grin' ? 1.3 : 1;
    for (const noise of this.state.noises) {
      if (noise.until < this.time) continue;
      if (Math.hypot(enemy.x - noise.x, enemy.y - noise.y) < noise.radius * hear) return 'noise';
    }
    return null;
  }

  tryOpenDoor(enemy, dt) {
    const angle = Math.atan2(enemy.goalY - enemy.y, enemy.goalX - enemy.x);
    const tx = Math.floor((enemy.x + Math.cos(angle) * 14) / TILE);
    const ty = Math.floor((enemy.y + Math.sin(angle) * 14) / TILE);
    const door = this.world.doorAt.get(`${tx},${ty}`);
    const st = door && this.state.doors[door.id];
    const blocked = door && st && bodyBlocked(door, st, (door.x + 0.5) * TILE, (door.y + 0.5) * TILE, 10);
    if (!door || !st || !blocked || !this.doorMayCross(door)) {
      enemy.openingDoor = null;
      return false;
    }
    if (enemy.openingDoor !== door.id) {
      enemy.openingDoor = door.id;
      enemy.opening = enemy.kind === 'grin' ? 0.22 : 0.4;
      this.audio.rattle(this.pan(tx * TILE, ty * TILE));
      if (Math.hypot(enemy.x - this.state.player.x, enemy.y - this.state.player.y) < 260) {
        this.ambient(`A handle turns ${this.dirPhrase(tx * TILE, ty * TILE)}.`);
      }
    }
    enemy.opening -= dt;
    if (enemy.opening <= 0) st.open = true;
    return true;
  }

  steer(enemy, goal, dt) {
    const dx = goal.x - enemy.x;
    const dy = goal.y - enemy.y;
    const distance = Math.hypot(dx, dy) || 1;
    const direct = this.los(enemy.x, enemy.y, goal.x, goal.y) && distance < 190;
    if (!direct && (!enemy.path || this.time >= enemy.nextPath)) {
      enemy.path = this.astar(enemy, goal);
      enemy.nextPath = this.time + 0.45;
      enemy.pathIndex = 0;
    }
    let mx = dx;
    let my = dy;
    if (!direct && enemy.path?.length) {
      while (enemy.pathIndex < enemy.path.length - 1) {
        const node = enemy.path[enemy.pathIndex];
        if (Math.hypot(node.x - enemy.x, node.y - enemy.y) < 8) enemy.pathIndex += 1;
        else break;
      }
      const node = enemy.path[Math.min(enemy.pathIndex, enemy.path.length - 1)];
      mx = node.x - enemy.x;
      my = node.y - enemy.y;
    }
    const mag = Math.hypot(mx, my) || 1;
    this.tryMove(enemy, (mx / mag) * enemy.speed * dt, (my / mag) * enemy.speed * dt, enemy.radius);
  }

  astar(enemy, goal) {
    const start = { x: Math.floor(enemy.x / TILE), y: Math.floor(enemy.y / TILE) };
    const end = { x: Math.floor(goal.x / TILE), y: Math.floor(goal.y / TILE) };
    const ok = (tx, ty) => this.enemyTileOk(enemy, tx, ty);
    if (!ok(end.x, end.y)) return null;
    const key = (x, y) => x + y * COLS;
    const open = [start];
    const cost = new Map([[key(start.x, start.y), 0]]);
    const prev = new Map();
    const closed = new Set();
    let guard = 0;
    while (open.length && guard++ < 2500) {
      open.sort((a, b) => {
        const ca = cost.get(key(a.x, a.y)) + Math.abs(a.x - end.x) + Math.abs(a.y - end.y);
        const cb = cost.get(key(b.x, b.y)) + Math.abs(b.x - end.x) + Math.abs(b.y - end.y);
        return ca - cb;
      });
      const current = open.shift();
      const ck = key(current.x, current.y);
      if (closed.has(ck)) continue;
      closed.add(ck);
      if (current.x === end.x && current.y === end.y) {
        const path = [];
        let cursor = current;
        let ck2 = ck;
        path.push(cursor);
        while (prev.has(ck2)) {
          cursor = prev.get(ck2);
          ck2 = key(cursor.x, cursor.y);
          path.push(cursor);
        }
        path.reverse();
        return path.map((node) => ({ x: (node.x + 0.5) * TILE, y: (node.y + 0.5) * TILE }));
      }
      for (const [dx, dy] of DIRS) {
        const nx = current.x + dx;
        const ny = current.y + dy;
        if (!ok(nx, ny)) continue;
        const nk = key(nx, ny);
        const next = cost.get(ck) + 1;
        if (next < (cost.get(nk) ?? Infinity)) {
          cost.set(nk, next);
          prev.set(nk, current);
          open.push({ x: nx, y: ny });
        }
      }
    }
    return null;
  }

  enemyTileOk(enemy, tx, ty) {
    if (!passable(this.world, tx, ty, (door) => this.doorMayCross(door))) return false;
    if (enemy.kind === 'grin' && !this.state.flags.finale && roomAt(tx, ty) === 'garage') return false;
    return true;
  }

  doorMayCross(door) {
    if (!door.lock) return true;
    if (door.lock === 'power') return this.state.flags.powerOn || this.state.doors[door.id].unlocked;
    return this.state.doors[door.id].unlocked;
  }

  updateCamera(dt) {
    const p = this.state.player;
    const look = 26;
    const tx = clamp(p.x + Math.cos(p.facing) * look - VIEW_W / 2, 0, COLS * TILE - VIEW_W);
    const ty = clamp(p.y + Math.sin(p.facing) * look - VIEW_H / 2, 0, ROWS * TILE - VIEW_H);
    const k = Math.min(1, dt * 10);
    this.cam.x += (tx - this.cam.x) * k;
    this.cam.y += (ty - this.cam.y) * k;
  }

  tryMove(ent, dx, dy, radius) {
    const steps = Math.max(1, Math.ceil(Math.hypot(dx, dy) / 6));
    for (let i = 0; i < steps; i++) {
      ent.x += dx / steps;
      if (this.hitsSolid(ent, radius)) ent.x -= dx / steps;
      ent.y += dy / steps;
      if (this.hitsSolid(ent, radius)) ent.y -= dy / steps;
    }
  }

  hitsSolid(ent, radius) {
    for (const [ox, oy] of [[0, 0], [radius, 0], [-radius, 0], [0, radius], [0, -radius],
      [radius * 0.7, radius * 0.7], [radius * 0.7, -radius * 0.7], [-radius * 0.7, radius * 0.7], [-radius * 0.7, -radius * 0.7]]) {
      if (this.blocks(ent.x + ox, ent.y + oy, ent.kind ? ent : null)) return true;
    }
    return false;
  }

  blocks(px, py, ent) {
    if (this.tileBlocks(px, py)) return true;
    if (ent?.kind === 'grin' && !this.state.flags.finale) {
      if (roomAt(Math.floor(px / TILE), Math.floor(py / TILE)) === 'garage') return true;
    }
    return false;
  }

  tileBlocks(px, py, forMove = true) {
    const tx = Math.floor(px / TILE);
    const ty = Math.floor(py / TILE);
    if (tx < 0 || ty < 0 || tx >= COLS || ty >= ROWS) return true;
    if (forMove && this.world.furniture.has(`${tx},${ty}`)) return true;
    const tile = this.world.tiles[ty][tx];
    if (tile === 0) return true;
    if (tile === 9) {
      const door = this.world.doorAt.get(`${tx},${ty}`);
      if (!door) return true;
      const st = this.state.doors[door.id];
      if (st && doorBlocksPoint(door, st, px, py)) return true;
    }
    const pocket = this.world.pocketAt?.get(`${tx},${ty}`);
    if (pocket) {
      for (const door of pocket) {
        const st = this.state.doors[door.id];
        if (st && doorBlocksPoint(door, st, px, py)) return true;
      }
    }
    return false;
  }

  tileAt(px, py) {
    const tx = Math.floor(px / TILE);
    const ty = Math.floor(py / TILE);
    return this.world.tiles[ty]?.[tx] ?? 0;
  }

  los(x0, y0, x1, y1) {
    const dx = x1 - x0;
    const dy = y1 - y0;
    const distance = Math.hypot(dx, dy);
    if (distance < 10) return true;
    const steps = Math.ceil(distance / 10);
    for (let i = 1; i < steps; i++) {
      const t = i / steps;
      if (this.tileBlocks(x0 + dx * t, y0 + dy * t, false)) return false;
    }
    return true;
  }

  lightAt(wx, wy) {
    const p = this.state.player;
    const dx = wx - p.x;
    const dy = wy - p.y;
    const distance = Math.hypot(dx, dy);
    const feet = this.state.flags.darker ? TUNING.feetDark : TUNING.feet;
    let ambient = distance < feet ? (1 - distance / feet) ** 1.4 * 0.92 : 0;
    const blocked = !this.los(p.x, p.y, wx, wy);
    if (blocked) return distance < 16 ? ambient : 0;
    if (!this.state.hasLight || !p.lightOn || p.battery <= 0) return ambient;
    const diff = Math.abs(angDiff(Math.atan2(dy, dx), p.facing));
    if (diff > TUNING.cone && distance > feet) return ambient;
    const fall = Math.max(0, 1 - distance / TUNING.lightRange);
    const cone = diff < TUNING.cone ? 1 - diff / TUNING.cone : 0;
    let beam = fall * (0.22 + 0.78 * cone);
    beam *= flashlightFlicker(this.time, p.battery, this.settings.reduceFlash);
    return Math.min(1, Math.max(ambient, beam));
  }

  beamTouches(enemy) {
    const p = this.state.player;
    if (!this.state.hasLight || !p.lightOn || p.battery <= 0) return false;
    const distance = Math.hypot(enemy.x - p.x, enemy.y - p.y);
    if (distance > TUNING.lightRange) return false;
    const angle = Math.atan2(enemy.y - p.y, enemy.x - p.x);
    if (Math.abs(angDiff(angle, p.facing)) > TUNING.cone) return false;
    return this.los(p.x, p.y, enemy.x, enemy.y);
  }

  aim(sx, sy) {
    if (!this.state) return;
    const wx = this.cam.x + sx;
    const wy = this.cam.y + sy;
    const p = this.state.player;
    p.facing = Math.atan2(wy - p.y, wx - p.x);
    this.mouseAim = true;
  }

  toggleLight() {
    if (!this.state || this.state.screen) return;
    if (!this.state.hasLight) {
      this.say('No light yet. The foyer has one.');
      return;
    }
    const p = this.state.player;
    if (p.battery <= 0 && !p.lightOn) {
      this.say('The batteries are dead.');
      return;
    }
    p.lightOn = !p.lightOn;
  }

  dodge() {
    if (!this.state || this.state.screen || this.state.dead) return;
    const p = this.state.player;
    if (p.dodge > 0 || p.stamina < TUNING.dodgeCost) return;
    let x = this.input.x;
    let y = this.input.y;
    if (Math.hypot(x, y) < 0.2) {
      x = Math.cos(p.facing);
      y = Math.sin(p.facing);
    }
    const mag = Math.hypot(x, y) || 1;
    p.stamina -= TUNING.dodgeCost;
    p.dodge = TUNING.dodgeTime;
    p.dodgeX = (x / mag) * TUNING.dodge;
    p.dodgeY = (y / mag) * TUNING.dodge;
    p.iframes = Math.max(p.iframes, TUNING.dodgeTime + 0.08);
    this.audio.swing();
  }

  attack() {
    if (!this.state || this.state.screen || this.state.dead) return;
    const p = this.state.player;
    if (p.atk > 0) return;
    const index = this.state.equipped;
    const slot = index >= 0 ? this.state.inv[index] : null;
    const def = slot ? ITEMS[slot.id] : null;
    if (slot?.id === 'camera') {
      this.flashBulb(slot);
      return;
    }
    if (def?.ammo) {
      this.shoot(def);
      return;
    }
    if (def?.type === 'weapon') {
      this.melee(slot, def, index);
      return;
    }
    this.melee(null, { dmg: 7, range: 22, arc: 1.1, cool: 0.48, noise: 18 }, -1);
  }

  melee(slot, def, index) {
    const p = this.state.player;
    let damage = def.dmg;
    if (slot && def.dur != null && slot.dur <= 0) {
      damage = 6;
      this.say(slot.id === 'stick' ? 'The shaft is cracked. It needs the tape.' : 'Nothing left of it.');
    }
    p.atk = def.cool;
    this.audio.swing();
    this.makeNoise(def.noise || 20, 0.25);
    for (const enemy of this.state.enemies) {
      if (!enemy.alive || enemy.downUntil > this.time) continue;
      if (!this.inArc(enemy, def.range, def.arc)) continue;
      if (!this.los(p.x, p.y, enemy.x, enemy.y)) continue;
      this.damageEnemy(enemy, damage);
    }
    if (slot && def.dur != null && slot.dur > 0) {
      slot.dur -= 1;
      if (slot.dur <= 0 && slot.id === 'knife') {
        this.say('The blade snaps.');
        this.audio.cue?.('knife', 'break');
        this.removeAt(index);
      } else if (slot.dur <= 0) {
        this.say('The stick cracks.');
        this.audio.cue?.('stick', 'break');
      }
    }
  }

  shoot(def) {
    const ammoIndex = this.state.inv.findIndex((slot) => slot.id === 'ammo' && slot.count > 0);
      if (ammoIndex < 0) {
        this.say('The cylinder is empty.');
        this.audio.cue?.('pistol', 'empty');
        return;
      }
    this.state.inv[ammoIndex].count -= 1;
    if (this.state.inv[ammoIndex].count <= 0) this.removeAt(ammoIndex);
    const p = this.state.player;
    p.atk = def.cool;
    this.audio.shot();
    this.makeNoise(def.noise, 1.05);
    this.director.bump(9, Math.cos(p.facing), Math.sin(p.facing));
    let best = null;
    let bestD = Infinity;
    for (const enemy of this.state.enemies) {
      if (!enemy.alive || enemy.downUntil > this.time) continue;
      if (!this.inArc(enemy, def.range, def.arc)) continue;
      if (!this.los(p.x, p.y, enemy.x, enemy.y)) continue;
      const distance = Math.hypot(enemy.x - p.x, enemy.y - p.y);
      if (distance < bestD) {
        best = enemy;
        bestD = distance;
      }
    }
    if (best) this.damageEnemy(best, def.dmg);
    else this.say('The shot goes into the dark.');
  }

  flashBulb(slot) {
    if (!slot.flashes) {
      this.say('No bulbs left. The plate is for the car.');
      return;
    }
    slot.flashes -= 1;
    this.state.player.atk = 0.8;
    this.spook('white');
    this.audio.flash();
    this.makeNoise(120, 0.55);
    let stunned = 0;
    for (const enemy of this.state.enemies) {
      if (!enemy.alive || !enemy.awake || enemy.downUntil > this.time) continue;
      if (Math.hypot(enemy.x - this.state.player.x, enemy.y - this.state.player.y) > 140) continue;
      if (!this.los(this.state.player.x, this.state.player.y, enemy.x, enemy.y)) continue;
      enemy.stunUntil = this.time + (enemy.kind === 'grin' ? 3.2 : 2.2);
      stunned += 1;
    }
    this.say(stunned ? 'The bulb pops. It flinches.' : 'The bulb pops. The room is empty.');
  }

  inArc(enemy, range, arc) {
    const p = this.state.player;
    const distance = Math.hypot(enemy.x - p.x, enemy.y - p.y);
    if (distance > range) return false;
    const angle = Math.atan2(enemy.y - p.y, enemy.x - p.x);
    return Math.abs(angDiff(angle, p.facing)) <= arc / 2;
  }

  damageEnemy(enemy, damage) {
    if (!enemy.awake) {
      enemy.awake = true;
      enemy.state = 'chase';
    }
    enemy.hp -= damage;
    const p = this.state.player;
    const dx = enemy.x - p.x;
    const dy = enemy.y - p.y;
    const mag = Math.hypot(dx, dy) || 1;
    this.tryMove(enemy, (dx / mag) * 14, (dy / mag) * 14, enemy.radius);
    this.audio.hit();
    if (enemy.hp > 0) return;
    if (enemy.kind === 'grin') {
      enemy.hp = ENEMY.grin.hp;
      enemy.downUntil = this.time + ENEMY.grin.revive;
      enemy.risen += 1;
      enemy.speed = Math.min(150, ENEMY.grin.speed + enemy.risen * 8);
      enemy.state = 'chase';
      this.say('It falls. The smile stays.');
      if (!this.scare('grin-down', { story: true, face: false, weight: 0.7 }) && this.director.fired.has('grin-down')) {
        this.audio.dread?.();
      }
      return;
    }
    enemy.alive = false;
    this.state.decals.push({ x: enemy.x, y: enemy.y });
    this.say('It goes still.');
  }

  hurt(amount, from) {
    const p = this.state.player;
    if (p.iframes > 0 || p.dodge > 0 || this.state.dead) return;
    p.hp = Math.max(0, p.hp - amount);
    p.iframes = TUNING.meleeIFrames;
    p.stamina = Math.max(0, p.stamina - 18);
    this.hurtFlash = 0.28;
    this.audio.hurt();
    this.rumble(0.7, 140);
    const dx = p.x - from.x;
    const dy = p.y - from.y;
    const mag = Math.hypot(dx, dy) || 1;
    this.director.bump(11, dx / mag, dy / mag);
    this.tryMove(p, (dx / mag) * 16, (dy / mag) * 16, 9);
    if (p.hp <= 0) this.die();
    else this.say(from.kind === 'grin' ? 'The smile is on you.' : 'Something touches you.', 4);
  }

  die() {
    this.state.dead = true;
    this.state.screen = { type: 'dead' };
    this.captions.clear();
    this.caption = null;
    this.scare('death', {
      story: true,
      face: true,
      preempt: true,
      weight: 1,
      buildup: 0.18,
      silence: 0.08,
      aftermath: 1.2,
    });
  }

  win() {
    if (this.state.dead) return;
    this.state.won = true;
    this.state.screen = { type: 'ending' };
    this.captions.clear();
    this.caption = null;
    this.director.requestTransition('ending', {
      calm: true,
      once: true,
      reduce: !!this.settings.reduceFlash,
    });
    this.audio.resolve?.();
    try { localStorage.removeItem(SAVE_KEY); } catch { /* a private window can refuse */ }
  }

  use() {
    if (!this.state || this.state.dead || this.state.won) return;
    if (this.state.screen) return;
    if (this.state.flags.finale && this.nearCar(CAR_LEAVE)) return;
    const hit = this.nearest();
    if (!hit) return;
    hit.apply();
  }

  nearest() {
    const p = this.state.player;
    let best = null;
    const consider = (x, y, label, apply) => {
      const distance = Math.hypot(p.x - x, p.y - y);
      if (distance > TUNING.interact) return;
      const angle = Math.atan2(y - p.y, x - p.x);
      const facingOk = Math.abs(angDiff(angle, p.facing)) < 1.2;
      if (distance > 24 && !facingOk) return;
      if (!best || distance < best.distance) best = { distance, label, apply };
    };
    for (const prop of this.world.props) {
      if (this.state.taken[prop.id]) continue;
      if (prop.kind === 'container' && !(this.state.containers[prop.id] || []).length) continue;
      consider((prop.x + 0.5) * TILE, (prop.y + 0.5) * TILE, prop.label, () => this.useProp(prop));
    }
    for (const drop of this.state.dropped) {
      const name = ITEMS[drop.slot.id]?.name || 'Item';
      consider(drop.x, drop.y, name, () => this.pickupDrop(drop));
    }
    for (const door of this.world.doors) {
      const st = this.state.doors[door.id];
      const point = interactPoint(door, st);
      consider(point.x, point.y, this.doorLabel(door, st), () => this.useDoor(door));
    }
    if (this.nearCar(18)) {
      const label = this.state.flags.finale ? 'Get in' : 'The car';
      if (!best || best.distance > 10) best = { distance: 10, label, apply: () => this.openCar() };
    }
    return best;
  }

  prompt() {
    if (!this.state || this.state.screen || this.state.dead) return null;
    if (this.state.flags.finale && this.nearCar(CAR_LEAVE)) {
      return { text: 'HOLD E TO GO', fill: this.state.hold / TUNING.holdTime };
    }
    const hit = this.nearest();
    if (!hit) return null;
    return { text: `E  ${hit.label}`, fill: 0 };
  }

  doorOther(door) {
    const rooms = [];
    for (const [dx, dy] of DIRS) {
      const id = roomAt(door.x + dx, door.y + dy);
      if (id && !rooms.includes(id)) rooms.push(id);
    }
    const other = rooms.find((id) => id !== this.state.room) || rooms[0];
    return other && ROOMS[other] ? ROOMS[other].name : 'Door';
  }

  doorLabel(door, st) {
    const name = this.doorOther(door);
    if (!st.open) {
      if (door.lock === 'power' && !this.state.flags.powerOn && !st.unlocked) return 'Yard door, dead';
      if (door.lock && door.lock !== 'power' && !st.unlocked) {
        return this.has(door.lock) ? `Unlock ${name}` : `${name}, locked`;
      }
      return name;
    }
    if (door.sticky) return 'Yard door, open';
    return 'Close door';
  }

  nearCar(pad) {
    const car = FURNITURE.find((piece) => piece.id === 'car');
    const p = this.state.player;
    const x0 = car.x * TILE - pad;
    const y0 = car.y * TILE - pad;
    const x1 = (car.x + car.w) * TILE + pad;
    const y1 = (car.y + car.h) * TILE + pad;
    return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1;
  }

  useDoor(door) {
    const st = this.state.doors[door.id];
    const plan = planDoorUse(door, st, {
      has: (id) => this.has(id),
      powerOn: !!this.state.flags.powerOn,
    });
    if (plan.action === 'rattle') {
      st.rattle = Math.max(st.rattle, 0.42);
      st.rattleSent = 0;
      if (plan.say === 'no-power') {
        this.say('The motor is dead. Seat the fuse in the car.');
        return;
      }
      const lines = {
        'key-her': 'Her door. Locked.',
        'key-base': 'The basement. Locked. Cold air under the door.',
        'key-shed': 'The shed is locked.',
      };
      this.say(lines[door.lock] || 'Locked.');
      return;
    }
    if (plan.action === 'refuse') {
      this.say('The yard door is up. Leave it.');
      return;
    }
    if (plan.action === 'unlock') {
      this.consume(plan.consume);
      st.unlocked = true;
      st.open = true;
      this.audio.cue?.(plan.consume, 'use');
      this.makeNoise(72, 0.35);
      this.say('The lock gives.');
      return;
    }
    if (plan.action === 'open') {
      st.open = true;
      this.makeNoise(plan.noise || 60, 0.3);
      return;
    }
    if (!this.clearOf(door)) {
      this.say('The door meets you.');
      return;
    }
    this.nudgeDrops(door);
    st.open = false;
  }

  normalizeDoors() {
    for (const door of this.world.doors) {
      const st = this.state.doors[door.id];
      if (!st) continue;
      if (typeof st.angle !== 'number' || Number.isNaN(st.angle)) st.angle = st.open ? 1 : 0;
      st.angle = Math.max(0, Math.min(1, st.angle));
      st.vel = 0;
      st.rattle = 0;
      st.rattleSent = 0;
      st.slam = false;
      st.pendingSlam = false;
      st.latched = st.angle < 0.05;
      st.arrivedOpen = st.angle > 0.95;
      st.frame = Math.round(st.angle * SWING_FRAMES);
      syncDoorMotion(door, st);
    }
  }

  updateDoors(dt) {
    const reduce = !!this.settings.reduceFlash;
    for (const door of this.world.doors) {
      const st = this.state.doors[door.id];
      if (!st) continue;
      const wasShut = st.block === 'tile' || (door.kind === 'overhead' && bodyBlocked(door, st, (door.x + 0.5) * TILE, (door.y + 0.5) * TILE, 8));
      const events = stepDoor(door, st, dt, { reduce });
      const shut = st.block === 'tile' || (door.kind === 'overhead' && bodyBlocked(door, st, (door.x + 0.5) * TILE, (door.y + 0.5) * TILE, 8));
      if (!wasShut && shut) this.ejectDoor(door);
      if (!events.length) continue;
      const pan = this.pan((door.x + 0.5) * TILE, (door.y + 0.5) * TILE);
      const dist = Math.hypot((door.x + 0.5) * TILE - this.state.player.x, (door.y + 0.5) * TILE - this.state.player.y);
      const gain = dist < 48 ? 1 : Math.max(0.18, 1 - (dist - 48) / 380);
      for (const ev of events) this.playDoorEvent(ev, pan, gain);
    }
  }

  playDoorEvent(ev, pan, gain) {
    const audio = this.audio;
    const strength = (ev.intensity ?? 1) * gain;
    if (ev.type === 'creak') audio.doorCreak?.(pan, strength);
    else if (ev.type === 'latch') audio.latch?.(pan);
    else if (ev.type === 'slam') {
      audio.slam?.(pan);
      if (gain > 0.45) this.director.bump(8 * gain, pan, 0.15);
    } else if (ev.type === 'thud') audio.thud?.(pan);
    else if (ev.type === 'rattle' || ev.type === 'motor-rattle') audio.rattle?.(pan);
    else if (ev.type === 'motor') audio.motorTick?.(pan, strength);
    else if (ev.type === 'motor-stop') audio.motor?.(pan);
  }

  ejectDoor(door) {
    const ents = [this.state.player, ...this.state.enemies];
    for (const ent of ents) {
      if (Math.floor(ent.x / TILE) !== door.x || Math.floor(ent.y / TILE) !== door.y) continue;
      let best = null;
      for (const side of door.passage || []) {
        const x = (side.x + 0.5) * TILE;
        const y = (side.y + 0.5) * TILE;
        const dist = Math.hypot(ent.x - x, ent.y - y);
        if (!best || dist < best.dist) best = { x, y, dist };
      }
      if (!best) continue;
      ent.x = best.x;
      ent.y = best.y;
    }
  }

  clearOf(door) {
    const p = this.state.player;
    if (Math.floor(p.x / TILE) !== door.x || Math.floor(p.y / TILE) !== door.y) return true;
    let best = null;
    for (const [dx, dy] of DIRS) {
      const x = (door.x + dx + 0.5) * TILE;
      const y = (door.y + dy + 0.5) * TILE;
      if (this.tileBlocks(x, y)) continue;
      const dist = Math.hypot(x - p.x, y - p.y);
      if (!best || dist < best.dist) best = { x, y, dist };
    }
    if (!best) return false;
    p.x = best.x;
    p.y = best.y;
    return true;
  }

  nudgeDrops(door) {
    for (const drop of this.state.dropped) {
      if (Math.floor(drop.x / TILE) !== door.x || Math.floor(drop.y / TILE) !== door.y) continue;
      drop.x = this.state.player.x;
      drop.y = this.state.player.y;
    }
  }

  dropSpot() {
    const p = this.state.player;
    const tries = [
      [Math.cos(p.facing) * 18, Math.sin(p.facing) * 18],
      [0, 0],
      [16, 0],
      [-16, 0],
      [0, 16],
      [0, -16],
    ];
    for (const [dx, dy] of tries) {
      const x = p.x + dx;
      const y = p.y + dy;
      if (!this.tileBlocks(x, y)) return { x, y };
    }
    return { x: p.x, y: p.y };
  }

  feedback(id, x, y, verb) {
    if (!id) return;
    this.audio.cue?.(id, verb);
    if (!this.state.fx) this.state.fx = [];
    this.state.fx.push({ id, x, y, age: 0, life: verb === 'take' ? 0.34 : 0.26 });
    if (this.state.fx.length > 6) this.state.fx.shift();
  }

  useProp(prop) {
    if (prop.kind === 'flashlight') {
      this.state.hasLight = true;
      this.state.player.lightOn = true;
      this.state.player.battery = Math.max(this.state.player.battery, 82);
      this.state.taken[prop.id] = true;
      this.feedback('flashlight', (prop.x + 0.5) * TILE, (prop.y + 0.5) * TILE, 'take');
      this.say('Flashlight. It will not last.');
      return;
    }
    if (prop.kind === 'battery') {
      if (this.state.player.battery >= 99) {
        this.say(this.state.hasLight ? 'The flashlight is already full.' : 'Those can wait until you have the light.');
        return;
      }
      this.state.player.battery = Math.min(100, this.state.player.battery + 55);
      this.state.taken[prop.id] = true;
      this.feedback('battery', (prop.x + 0.5) * TILE, (prop.y + 0.5) * TILE, 'take');
      this.say(this.state.hasLight ? 'Fresh batteries. The beam steadies.' : 'Batteries. They wait for the flashlight.');
      return;
    }
    if (prop.kind === 'pickup') {
      if (!this.give(prop.item, prop.count || 1)) {
        this.handsFull();
        return;
      }
      this.state.taken[prop.id] = true;
      this.feedback(prop.item, (prop.x + 0.5) * TILE, (prop.y + 0.5) * TILE, 'take');
      this.say(`${ITEMS[prop.item].name}.`);
      this.storyItem(prop.item);
      return;
    }
    if (prop.kind === 'note') {
      this.openNote(prop.note);
      return;
    }
    if (prop.kind === 'inspect') {
      this.openInspect(prop.inspect);
      return;
    }
    if (prop.kind === 'container') {
      this.search(prop);
      return;
    }
    if (prop.kind === 'save') {
      this.useSave();
      return;
    }
    if (prop.kind === 'stash') {
      this.state.selected = 0;
      this.state.screen = { type: 'stash' };
      this.audio.cue?.('bin', 'use');
      return;
    }
    if (prop.kind === 'car') this.openCar();
    if (prop.kind === 'board') {
      this.state.screen = { type: 'board' };
      this.audio.cue?.('clipboard', 'use');
    }
  }

  search(prop) {
    const items = this.state.containers[prop.id];
    if (!items?.length) {
      this.say('Nothing left in it.');
      return;
    }
    const next = items[0];
    if (next.note) {
      items.shift();
      this.audio.cue?.(prop.id === 'cabinet' ? 'cabinet' : 'gearbag', 'use');
      this.openNote(next.note);
      return;
    }
    if (!this.give(next.item, next.count || 1)) {
      this.handsFull();
      return;
    }
    items.shift();
    this.audio.cue?.(prop.id === 'cabinet' ? 'cabinet' : 'gearbag', 'use');
    this.feedback(next.item, (prop.x + 0.5) * TILE, (prop.y + 0.5) * TILE, 'take');
    this.say(`${ITEMS[next.item].name}.`);
    this.storyItem(next.item);
  }

  openNote(id) {
    this.state.flags.read[id] = true;
    this.state.screen = { type: 'note', id };
    this.audio.cue?.(`note-${id}`, 'use');
  }

  openInspect(id) {
    if (id === 'tv') {
      this.state.flags.tv += 1;
      if (this.state.flags.tv === 1) {
        this.scare('tv', { story: true, face: true, weight: 1 });
        this.say('The smile is in the glass.');
      } else {
        this.hurt(8, { kind: 'grin', x: this.state.player.x + 8, y: this.state.player.y });
        if (this.state.dead) return;
        if (this.state.flags.tv === 2) this.scare('tv-again', { story: true, face: true, weight: 0.85 });
        this.say('You looked again.');
      }
    }
    if (id === 'mirror' && !this.state.flags.mirror) {
      this.state.flags.mirror = true;
      this.scare('mirror', { story: true, face: true, weight: 0.9 });
    }
    this.state.screen = { type: 'inspect', id };
    this.audio.cue?.(`look-${id}`, 'use');
  }

  openCar() {
    if (this.state.flags.finale) return;
    this.state.screen = { type: 'car' };
    this.audio.cue?.('handle', 'use');
  }

  storyItem(id) {
    const flags = this.state.flags;
    if (id === 'key-base' && !flags.tookBaseKey) {
      flags.tookBaseKey = true;
      this.say('The sweater is warm. The bed is breathing.');
      this.scare('base-key', { story: true, face: false, weight: 0.55, buildup: 0.45 });
    }
    if (id === 'key-car' && !flags.tookCarKeys) {
      flags.tookCarKeys = true;
      if (this.state.doors.her) {
        const door = this.world.doors.find((item) => item.id === 'her');
        const st = this.state.doors.her;
        if (!door || this.clearOf(door)) {
          st.open = false;
          st.slam = true;
          if (st.angle <= 0.02) st.pendingSlam = true;
        }
      }
      this.say('Her door slams. The dining room answers.');
    }
    if (id === 'fuse' && !flags.tookFuse) {
      flags.tookFuse = true;
      flags.darker = true;
      this.say('The house goes dark. Somewhere, a smile opens.');
      this.scare('fuse-dark', { story: true, face: true, weight: 1 });
    }
    if (id === 'gas' && !flags.tookGas) {
      flags.tookGas = true;
      this.makeNoise(420, 1.3);
      this.say('The can sloshes. It hears you.');
      this.scare('gas', { story: true, face: false, weight: 0.65 });
    }
  }

  install(part) {
    if (!this.state || this.state.flags.finale) return;
    if (!this.nearCar(18)) {
      this.say('That goes in the car.');
      return;
    }
    const installed = this.state.installed;
    if (part === 'photo') {
      if (!installed.fuse || !installed.keys || !installed.gas) {
        this.say('The car is still dead. A picture will not drive it.');
        return;
      }
      const index = this.state.inv.findIndex((slot) => slot.id === 'camera');
      if (index < 0) {
        this.say("Ben's camera has to be in your hands.");
        return;
      }
      this.removeAt(index);
      installed.photo = true;
      this.beginFinale();
      return;
    }
    const need = { fuse: 'fuse', keys: 'key-car', gas: 'gas' };
    if (installed[part]) {
      this.say('Already in the car.');
      return;
    }
    if (!this.has(need[part])) {
      this.say('It is not in your hands.');
      return;
    }
    this.consume(need[part]);
    installed[part] = true;
    this.feedback(need[part], this.state.player.x, this.state.player.y, 'use');
    if (part === 'fuse') {
      this.state.flags.powerOn = true;
      this.state.doors.yard.open = true;
      this.state.doors.yard.unlocked = true;
      this.makeNoise(150, 0.7);
      this.say('The motor grinds. The yard door stays up.');
    } else if (part === 'keys') this.say('The key turns. The dash stays dark.');
    else this.say('The can empties into the tank. The needle climbs off E.');
  }

  beginFinale() {
    this.state.flags.finale = true;
    this.state.screen = null;
    const grin = this.state.enemies.find((enemy) => enemy.kind === 'grin');
    if (grin) {
      grin.awake = true;
      grin.alive = true;
      grin.state = 'chase';
      grin.downUntil = 0;
      grin.hp = Math.max(grin.hp, 40);
      grin.stunUntil = this.time + 1.5;
    }
    this.state.player.iframes = 1.5;
    this.save(true);
    this.say('The shutter sounds like a door. Get in.');
    this.audio.shutter();
    this.director.requestTransition('finale', {
      heavy: true,
      once: true,
      reduce: !!this.settings.reduceFlash,
    });
  }

  useSave() {
    const index = this.state.inv.findIndex((slot) => slot.id === 'film' && slot.count > 0);
    if (index < 0) {
      this.say('No film. The bench takes one slide.');
      this.audio.cue?.('film', 'empty');
      return;
    }
    const backup = this.state.inv.map((slot) => ({ ...slot }));
    const equipped = this.state.equipped;
    const selected = this.state.selected;
    const slot = this.state.inv[index];
    slot.count -= 1;
    if (slot.count <= 0) this.removeAt(index);
    if (!this.save(false)) {
      this.state.inv = backup;
      this.state.equipped = equipped;
      this.state.selected = selected;
      return;
    }
    this.audio.cue?.('film', 'use');
    this.spook('white');
  }

  spaceFor(id) {
    const def = ITEMS[id];
    if (!def) return 0;
    if (!def.stack) return this.state.inv.length < TUNING.slots ? 1 : 0;
    let room = 0;
    for (const slot of this.state.inv) {
      if (slot.id === id) room += Math.max(0, def.stack - (slot.count || 0));
    }
    room += (TUNING.slots - this.state.inv.length) * def.stack;
    return room;
  }

  commit(id, count, extra = null) {
    const def = ITEMS[id];
    let left = count;
    if (def.stack) {
      for (const slot of this.state.inv) {
        if (slot.id !== id || (slot.count || 0) >= def.stack) continue;
        const moved = Math.min(def.stack - slot.count, left);
        slot.count += moved;
        left -= moved;
        if (left <= 0) return;
      }
    }
    while (left > 0 && this.state.inv.length < TUNING.slots) {
      const n = def.stack ? Math.min(def.stack, left) : left;
      this.state.inv.push({
        id,
        count: def.stack ? n : extra?.count || n,
        dur: extra?.dur ?? def.dur ?? null,
        flashes: extra?.flashes ?? def.flashes ?? null,
      });
      if (this.state.equipped < 0 && (def.type === 'weapon' || def.type === 'camera')) {
        this.state.equipped = this.state.inv.length - 1;
      }
      left -= def.stack ? n : left;
      if (!def.stack) break;
    }
  }

  give(id, count = 1) {
    if (this.spaceFor(id) < count) return false;
    this.commit(id, count);
    return true;
  }

  giveSlot(slot) {
    const def = ITEMS[slot.id];
    const count = slot.count || 1;
    if (!def || this.spaceFor(slot.id) < count) return false;
    if (!def.stack) {
      this.state.inv.push(slot);
      if (this.state.equipped < 0 && (def.type === 'weapon' || def.type === 'camera')) {
        this.state.equipped = this.state.inv.length - 1;
      }
      return true;
    }
    this.commit(slot.id, count, slot);
    return true;
  }

  pickupDrop(drop) {
    const slot = drop.slot;
    const def = ITEMS[slot.id];
    const count = slot.count || 1;
    if (this.spaceFor(slot.id) <= 0) {
      this.handsFull();
      return;
    }
    if (!def?.stack) {
      this.state.inv.push(slot);
      this.state.dropped = this.state.dropped.filter((item) => item !== drop);
      if (this.state.equipped < 0 && (def?.type === 'weapon' || def?.type === 'camera')) {
        this.state.equipped = this.state.inv.length - 1;
      }
    } else {
      const moved = Math.min(count, this.spaceFor(slot.id));
      this.commit(slot.id, moved, slot);
      slot.count -= moved;
      if (slot.count <= 0) this.state.dropped = this.state.dropped.filter((item) => item !== drop);
    }
    this.feedback(slot.id, drop.x, drop.y, 'take');
    this.say(`${def?.name || 'Item'}.`);
  }

  handsFull() {
    if (this.state.flags.toldFull) this.say('Your hands are full.');
    else {
      this.state.flags.toldFull = true;
      this.say('Your hands are full. The bin in the garage can take the rest.');
    }
  }

  has(id) {
    return this.state.inv.some((slot) => slot.id === id && (slot.count || 1) > 0);
  }

  consume(id) {
    const index = this.state.inv.findIndex((slot) => slot.id === id);
    if (index < 0) return false;
    const slot = this.state.inv[index];
    if ((slot.count || 1) > 1) slot.count -= 1;
    else this.removeAt(index);
    return true;
  }

  removeAt(index) {
    this.state.inv.splice(index, 1);
    this.fixIndex(index);
  }

  fixIndex(removed) {
    if (this.state.equipped === removed) this.state.equipped = -1;
    else if (this.state.equipped > removed) this.state.equipped -= 1;
    if (this.state.selected === removed) this.state.selected = Math.max(0, Math.min(removed, this.state.inv.length - 1));
    else if (this.state.selected > removed) this.state.selected -= 1;
  }

  itemLoc(id) {
    if (this.state.inv.some((slot) => slot.id === id)) return 'inv';
    if (this.state.stash.some((slot) => slot.id === id)) return 'stash';
    if (this.state.dropped.some((drop) => drop.slot.id === id)) return 'floor';
    return 'world';
  }

  objective() {
    if (!this.state) return '';
    const ids = ['fuse', 'gas', 'camera', 'key-car', 'key-her', 'key-base', 'key-shed'];
    const loc = {};
    for (const id of ids) loc[id] = this.itemLoc(id);
    return describeGoal({
      installed: this.state.installed,
      read: this.state.flags.read,
      loc,
      hasLight: this.state.hasLight,
      finale: this.state.flags.finale,
      room: this.state.room,
      tookBaseKey: this.state.flags.tookBaseKey,
      herOpen: !!this.state.doors.her?.unlocked,
      shedOpen: !!this.state.doors.shed?.unlocked,
    });
  }

  toggleInv() {
    if (!this.state || this.state.dead || this.state.won) return;
    if (this.state.screen?.type === 'inventory') {
      this.state.screen = null;
      this.audio.ui?.('close');
    } else {
      this.state.selected = Math.min(this.state.selected, Math.max(0, this.state.inv.length - 1));
      this.state.screen = { type: 'inventory' };
      this.audio.ui?.('open');
    }
  }

  openMap() {
    if (!this.state || this.state.screen) return;
    this.state.screen = { type: 'map' };
    this.audio.ui?.('open');
  }

  openPause() {
    if (!this.state || this.state.dead || this.state.won) return;
    if (this.state.screen?.type === 'pause') {
      this.state.screen = null;
      this.audio.ui?.('close');
    } else {
      this.state.screen = { type: 'pause' };
      this.audio.ui?.('heavy');
    }
  }

  closeScreen() {
    if (!this.state?.screen) return;
    if (this.state.screen.type === 'dead' || this.state.screen.type === 'ending') return;
    this.state.screen = null;
    this.audio.ui?.('close');
  }

  nudgeSel(dir) {
    if (!this.state.inv.length) return;
    const next = (this.state.selected + dir + this.state.inv.length) % this.state.inv.length;
    this.state.selected = next;
  }

  useSelected() {
    const index = this.state.selected;
    const slot = this.state.inv[index];
    if (!slot) return;
    const def = ITEMS[slot.id];
    if (def.type === 'heal') {
      this.state.player.hp = Math.min(100, this.state.player.hp + def.heal);
      slot.count -= 1;
      if (slot.count <= 0) this.removeAt(index);
      this.feedback(slot.id, this.state.player.x, this.state.player.y, 'use');
      this.say('The shaking eases. A little.');
      return;
    }
    if (def.type === 'weapon' || def.type === 'camera') {
      this.state.equipped = index;
      this.audio.cue?.(slot.id, 'use');
      this.say(`${def.name}, in your hands.`);
      return;
    }
    if (def.type === 'tape') {
      const stick = this.state.inv.find((item) => item.id === 'stick');
      if (!stick) {
        this.say('Nothing to wrap.');
        return;
      }
      stick.dur = ITEMS.stick.dur;
      this.removeAt(index);
      this.feedback('tape', this.state.player.x, this.state.player.y, 'use');
      this.say('The stick is solid again.');
      return;
    }
    this.say(def.desc);
  }

  lookCamera() {
    const flags = this.state.flags;
    flags.cameraLooks = (flags.cameraLooks || 0) + 1;
    if (flags.cameraLooks >= 3) {
      const grin = this.state.enemies.find((enemy) => enemy.kind === 'grin' && enemy.awake && enemy.alive && enemy.downUntil <= this.time);
      if (grin) {
        const dx = this.state.player.x - grin.x;
        const dy = this.state.player.y - grin.y;
        const distance = Math.hypot(dx, dy) || 1;
        if (distance > 90) this.tryMove(grin, (dx / distance) * 64, (dy / distance) * 64, grin.radius);
        this.say('It is closer now.');
        this.scare('grin-closer', {
          once: false,
          face: true,
          weight: 0.85,
          buildup: 0.28,
          silence: 0.08,
          aftermath: 0.8,
        });
      } else this.say('In the picture, the face is closer than the trees.');
    } else if (flags.cameraLooks === 2) this.say('The smile is closer than it was.');
    else this.say("A hat. A trail. That isn't Ben. The trees behind him are wrong.");
  }

  dropSelected() {
    const index = this.state.selected;
    const slot = this.state.inv[index];
    if (!slot) return;
    this.state.inv.splice(index, 1);
    this.fixIndex(index);
    const spot = this.dropSpot();
    this.state.dropped.push({
      id: `d${this.dropN++}`,
      x: spot.x,
      y: spot.y,
      slot,
    });
    this.say('You set it down.');
  }

  storeSelected() {
    const index = this.state.selected;
    const slot = this.state.inv[index];
    if (!slot) return;
    this.state.inv.splice(index, 1);
    this.fixIndex(index);
    this.state.stash.push(slot);
  }

  takeStash(index) {
    const slot = this.state.stash[index];
    if (!slot) return;
    if (!this.giveSlot(slot)) {
      this.handsFull();
      return;
    }
    this.state.stash.splice(index, 1);
  }

  makeNoise(radius, life = 0.35) {
    const p = this.state.player;
    this.state.noises.push({ x: p.x, y: p.y, radius, until: this.time + life });
    if (this.state.noises.length > 16) this.state.noises.shift();
  }

  pan(x, y) {
    const p = this.state.player;
    return Math.sin(Math.atan2(y - p.y, x - p.x) - p.facing);
  }

  dirPhrase(x, y) {
    const p = this.state.player;
    let angle = Math.atan2(y - p.y, x - p.x) - p.facing;
    while (angle > Math.PI) angle -= Math.PI * 2;
    while (angle < -Math.PI) angle += Math.PI * 2;
    const mag = Math.abs(angle);
    if (mag < 0.55) return 'ahead';
    if (mag > 2.5) return 'behind you';
    return angle > 0 ? 'to the right' : 'to the left';
  }

  distVol(ent, max) {
    const d = Math.hypot(ent.x - this.state.player.x, ent.y - this.state.player.y);
    if (d > max) return 0;
    const muffled = this.los(this.state.player.x, this.state.player.y, ent.x, ent.y) ? 1 : 0.45;
    return (1 - d / max) ** 1.35 * muffled;
  }

  mixAudio() {
    let danger = 0;
    let humX = (ROOMS.mom.x + ROOMS.mom.w / 2) * TILE;
    let humY = (ROOMS.mom.y + ROOMS.mom.h / 2) * TILE;
    const grin = this.state.enemies.find((enemy) => enemy.kind === 'grin' && enemy.awake && enemy.alive);
    if (grin && grin.downUntil <= this.time) {
      humX = grin.x;
      humY = grin.y;
      danger = Math.max(danger, this.distVol(grin, 380));
    }
    for (const enemy of this.state.enemies) {
      if (!enemy.alive || !enemy.awake || enemy.kind === 'grin') continue;
      danger = Math.max(danger, this.distVol(enemy, 220) * 0.6);
    }
    if (this.state.player.hp < 36) danger = Math.max(danger, 0.35);
    const hum = this.distVol({ x: humX, y: humY }, 520);
    this.audio.mix({
      danger,
      humVol: this.state.flags.finale ? 0.02 : 0.012 * (0.35 + hum),
      humPan: this.pan(humX, humY),
      distorted: !!this.state.flags.tookBaseKey,
      room: this.state.room,
      reduce: !!this.settings.reduceFlash,
      time: this.time,
    });
    this.danger = danger;
    if (danger > 0.22 && this.director.mode === 'idle') this.audio.heart(danger);
  }

  captionMotion() {
    if (this.settings.reduceFlash) return false;
    try {
      if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) return false;
    } catch { /* tests and private windows can lack matchMedia */ }
    return true;
  }

  showRoom(name) {
    if (!name) {
      this.roomBanner = null;
      return;
    }
    const words = String(name).trim().split(/\s+/).filter(Boolean).length || 1;
    const hold = Math.max(1.7, 1.05 + words * 0.45);
    this.roomBanner = { name, from: this.uiTime, until: this.uiTime + hold };
  }

  pushCaption(text, kind, priority) {
    this.captions.push({
      text,
      kind,
      priority,
      ambient: this.settings.captions !== false,
    });
    const current = this.captions.current;
    if (current && current.age === 0) {
      current.opacity = this.captionMotion() ? 0 : 1;
    }
    this.caption = current;
  }

  say(text, priority = 4) {
    this.pushCaption(text, 'say', priority);
  }

  ambient(text) {
    this.pushCaption(text, 'ambient', 1);
  }

  voice(text) {
    this.pushCaption(text, 'voice', 3);
    this.audio.voice();
  }

  rumble(magnitude, ms) {
    const pad = this.input.pad;
    const actuator = pad?.vibrationActuator || pad?.hapticActuators?.[0];
    if (!actuator?.playEffect) return;
    actuator.playEffect('dual-rumble', {
      startDelay: 0,
      duration: ms,
      weakMagnitude: magnitude * 0.65,
      strongMagnitude: magnitude,
    }).catch(() => {});
  }

  plantEnemy(enemy, dt) {
    const prev = enemy.phase || 0;
    const next = prev + dt * CUE.enemy.rate;
    enemy.phase = next;
    if (!phaseMarksCrossed(prev, next, ENEMY_STEP_MARKS)) return;
    const heard = this.distVol(enemy, 220);
    if (heard <= 0.05) return;
    this.audio.foot(floorName(this.tileAt(enemy.x, enemy.y)), 0.22 * heard, this.pan(enemy.x, enemy.y));
  }

  scare(id, options = {}) {
    return this.director.requestScare(id, {
      reduce: !!this.settings.reduceFlash,
      ...options,
    });
  }

  playDirector(events) {
    for (const ev of events) {
      if (ev.type === 'scare-build') {
        this.audio.scareBuild?.({
          duration: ev.duration,
          weight: ev.weight,
          reduce: ev.reduce,
        });
      } else if (ev.type === 'scare-silence') {
        this.audio.scareSilence?.();
      } else if (ev.type === 'scare-hit') {
        this.audio.scareHit?.({
          weight: ev.weight,
          reduce: ev.reduce,
          face: ev.face,
          delay: 0,
        });
      } else if (ev.type === 'transition-start' && !ev.calm) {
        this.audio.scareBuild?.({
          duration: (ev.duration || CUE.transition.duration) * 0.42,
          weight: (ev.weight || 0.4) * 0.55,
          reduce: ev.reduce,
        });
      } else if (ev.type === 'transition-sub' && !ev.calm) {
        this.audio.cut?.({ weight: ev.heavy ? ev.weight : (ev.weight || 0.4) * 0.65 });
      }
    }
  }

  spook(kind) {
    if (this.settings.reduceFlash) {
      this.flash = 0.08;
      return;
    }
    if (kind === 'grin') this.grinFlash = 0.2;
    else this.flash = 0.16;
  }

  draw() {
    render(this);
  }

  debugGoto(roomId) {
    const room = ROOMS[roomId];
    if (!room || !this.state) return;
    this.state.player.x = (room.x + 1.2) * TILE;
    this.state.player.y = (room.y + room.h - 1.4) * TILE;
    if (this.hitsSolid(this.state.player, 8)) {
      this.state.player.x = (room.x + room.w - 1.4) * TILE;
      this.state.player.y = (room.y + room.h - 1.4) * TILE;
    }
    this.snapCamera();
    this.state.room = '';
    this.updateRoom();
  }
}
