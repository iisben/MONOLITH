import { ITEMS, INSPECTIONS, NOTES, TUNING } from './data.mjs';
import { createAudio } from './audio.mjs';
import { SAVE_KEY, Game } from './game.mjs';
import { loadCast, portrait } from './photos.mjs';
import { drawMap } from './render.mjs';

const params = new URLSearchParams(location.search);
const audio = createAudio();
const canvas = document.querySelector('#view');
const game = new Game(canvas, audio);
if (params.has('debug')) window.__game = game;

const SETTINGS_KEY = 'dont-look-settings';
const PARTS = [
  { id: 'fuse', item: 'fuse', name: 'Fuse', detail: 'Powers the yard door' },
  { id: 'keys', item: 'key-car', name: 'Keys', detail: 'Ignition' },
  { id: 'gas', item: 'gas', name: 'Gas', detail: 'Tank' },
  { id: 'photo', item: 'camera', name: 'Photo', detail: 'Of the car, not the face' },
];

const title = document.querySelector('#title');
const intro = document.querySelector('#intro');
const overlay = document.querySelector('#overlay');
const card = document.querySelector('#card');
const dock = document.querySelector('#dock');
const gallery = document.querySelector('#gallery');
const beginBtn = document.querySelector('#begin');

const keys = new Set();
const padMem = { down: new Set(), use: false };
const stick = { active: false, x: 0, y: 0, pointer: -1 };
let touchSneak = false;
let touchRun = false;
let mode = 'title';
let loopOn = false;
let lastFrame = 0;
let sig = '';
let screenType = '';
let portraitBand = '';
let introTimers = [];

const slotsEl = document.querySelector('#slots');
for (let i = 0; i < TUNING.slots; i += 1) {
  const cell = document.createElement('button');
  cell.type = 'button';
  cell.className = 'slot';
  cell.addEventListener('click', () => {
    if (!game.state || game.state.dead || game.state.won) return;
    const last = Math.max(0, game.state.inv.length - 1);
    game.state.selected = Math.min(i, last);
    if (game.state.screen?.type !== 'inventory') game.toggleInv();
  });
  slotsEl.appendChild(cell);
}

loadSettings();
applySettings();
bindInput();
refreshTitle();

await loadCast();
beginBtn.disabled = false;
beginBtn.textContent = 'Begin the drive';
refreshTitle();
if (params.has('photos')) buildGallery();
if (params.has('skip')) beginPlay(true);

function loadSettings() {
  try {
    const saved = JSON.parse(localStorage.getItem(SETTINGS_KEY) || 'null');
    if (!saved || typeof saved !== 'object') return;
    for (const key of ['captions', 'reduceFlash', 'shake', 'post', 'touch']) {
      if (typeof saved[key] === 'boolean') game.settings[key] = saved[key];
    }
    for (const key of ['volume', 'music', 'sfx', 'ambience']) {
      if (typeof saved[key] === 'number' && saved[key] >= 0 && saved[key] <= 1) game.settings[key] = saved[key];
    }
  } catch { /* a private window can refuse */ }
}

function writeSettings() {
  try {
    const payload = {
      captions: game.settings.captions,
      reduceFlash: game.settings.reduceFlash,
      shake: game.settings.shake !== false,
      post: game.settings.post !== false,
      volume: game.settings.volume,
      music: game.settings.music,
      sfx: game.settings.sfx,
      ambience: game.settings.ambience,
    };
    if (typeof game.settings.touch === 'boolean') payload.touch = game.settings.touch;
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(payload));
  } catch { /* keep the in-memory choice */ }
}

function touchPreferred() {
  if (typeof game.settings.touch === 'boolean') return game.settings.touch;
  return window.matchMedia('(pointer: coarse)').matches || window.innerWidth < 720;
}

function applySettings() {
  audio.setVolume(game.settings.volume);
  audio.setBuses({
    music: game.settings.music ?? 0.85,
    sfx: game.settings.sfx ?? 0.9,
    ambience: game.settings.ambience ?? 0.8,
  });
  document.body.classList.toggle('reduce', !!game.settings.reduceFlash);
  document.body.classList.toggle('nopost', game.settings.post === false);
  document.body.classList.toggle('touch', touchPreferred());
  for (const node of document.querySelectorAll('[data-setting]')) {
    const key = node.dataset.setting;
    if (node.type === 'checkbox') node.checked = key === 'touch' ? touchPreferred() : game.settings[key] !== false && !!game.settings[key];
    if (node.type === 'range' && document.activeElement !== node && game.settings[key] != null) {
      node.value = String(game.settings[key]);
    }
  }
}

function settingChanged(target) {
  const key = target?.dataset?.setting;
  if (!key) return;
  if (target.type === 'checkbox') game.settings[key] = target.checked;
  if (target.type === 'range') game.settings[key] = Number(target.value);
  writeSettings();
  applySettings();
}

function hasSave() {
  try { return !!localStorage.getItem(SAVE_KEY); } catch { return false; }
}

function refreshTitle() {
  document.querySelector('#continue').hidden = !hasSave();
}

function later(fn, ms) {
  const id = setTimeout(fn, ms);
  introTimers.push(id);
}

function clearIntro() {
  for (const id of introTimers) clearTimeout(id);
  introTimers = [];
}

function showScene(name) {
  for (const id of ['phone', 'glitch', 'arrive']) {
    document.querySelector(`#scene-${id}`).hidden = id !== name;
  }
}

function startIntro() {
  audio.unlock();
  clearIntro();
  title.hidden = true;
  intro.hidden = false;
  gallery.hidden = true;
  const photo = document.querySelector('#glitch-photo');
  photo.src = 'assets/evan.jpg';
  photo.classList.remove('hit');
  showScene('phone');
}

function openPhoto() {
  audio.unlock();
  showScene('glitch');
  const photo = document.querySelector('#glitch-photo');
  const reduce = !!game.settings.reduceFlash;
  later(() => {
    photo.classList.add('hit');
    if (!reduce) photo.src = 'assets/grin.jpg';
  }, reduce ? 80 : 360);
  later(() => showScene('arrive'), reduce ? 420 : 1100);
}

function beginPlay(fresh) {
  audio.unlock();
  clearIntro();
  intro.hidden = true;
  title.hidden = true;
  gallery.hidden = true;
  if (fresh) game.newGame();
  play();
}

function continueGame() {
  audio.unlock();
  try {
    game.hydrate(JSON.parse(localStorage.getItem(SAVE_KEY)));
    beginPlay(false);
  } catch {
    try { localStorage.removeItem(SAVE_KEY); } catch { /* ignore */ }
    refreshTitle();
    startIntro();
  }
}

function backToTitle() {
  mode = 'title';
  document.body.classList.remove('playing');
  dock.hidden = true;
  overlay.hidden = true;
  intro.hidden = true;
  title.hidden = false;
  screenType = '';
  sig = '';
  refreshTitle();
  audio.mix({ danger: 0.02, humVol: 0.006, humPan: 0, distorted: false });
}

function wakeFromBench() {
  audio.unlock();
  if (hasSave()) {
    try {
      game.hydrate(JSON.parse(localStorage.getItem(SAVE_KEY)));
      sig = '';
      return;
    } catch {
      try { localStorage.removeItem(SAVE_KEY); } catch { /* ignore */ }
    }
  }
  game.newGame();
  sig = '';
}

function play() {
  mode = 'play';
  portraitBand = '';
  sig = '';
  screenType = '';
  document.body.classList.add('playing');
  dock.hidden = false;
  fit();
  requestAnimationFrame(fit);
  if (!loopOn) {
    loopOn = true;
    requestAnimationFrame(frame);
  }
}

function fit() {
  const stage = document.querySelector('#stage');
  const world = document.querySelector('#world');
  const availW = stage.clientWidth;
  const availH = stage.clientHeight;
  if (!availW || !availH) return;
  let scale = Math.min(availW / 416, availH / 224);
  if (scale >= 1.5) {
    const steps = [1.5, 2, 2.5, 3, 3.5, 4, 5, 6];
    let best = 1.5;
    for (const step of steps) if (step <= scale + 0.02) best = step;
    scale = best;
  }
  scale = Math.max(0.45, scale);
  world.style.width = `${Math.round(416 * scale)}px`;
  world.style.height = `${Math.round(224 * scale)}px`;
  world.classList.toggle('cramped', scale < 1.2);
}

function frame(now) {
  const dt = lastFrame ? Math.min(0.05, (now - lastFrame) / 1000) : 0;
  lastFrame = now;
  if (mode === 'play' && game.state) {
    gather();
    game.update(dt);
    game.draw();
    sync();
  }
  requestAnimationFrame(frame);
}

function gather() {
  if (!game.state) return;
  if (stick.active) {
    game.input.x = stick.x;
    game.input.y = stick.y;
    game.mouseAim = false;
  } else {
    let x = 0;
    let y = 0;
    if (keys.has('KeyA') || keys.has('ArrowLeft')) x -= 1;
    if (keys.has('KeyD') || keys.has('ArrowRight')) x += 1;
    if (keys.has('KeyW') || keys.has('ArrowUp')) y -= 1;
    if (keys.has('KeyS') || keys.has('ArrowDown')) y += 1;
    game.input.x = x;
    game.input.y = y;
  }
  const keyRun = keys.has('ShiftLeft') || keys.has('ShiftRight');
  const keySneak = keys.has('KeyC');
  game.input.run = keyRun || touchRun;
  game.input.sneak = keySneak || touchSneak;
  applyPad();
}

function applyPad() {
  const list = navigator.getGamepads?.();
  game.input.pad = null;
  if (!list || !game.state || mode !== 'play') {
    padMem.down.clear();
    return;
  }
  let pad = null;
  for (const candidate of list) {
    if (candidate?.connected) pad = candidate;
  }
  if (!pad) {
    padMem.down.clear();
    return;
  }
  game.input.pad = pad;
  const down = new Set();
  pad.buttons.forEach((button, index) => {
    if (button.pressed) down.add(index);
  });
  const fresh = (index) => down.has(index) && !padMem.down.has(index);
  const screen = game.state.screen;
  if (fresh(9)) {
    if (screen?.type !== 'dead' && screen?.type !== 'ending') {
      if (screen) game.closeScreen();
      else game.openPause();
    }
  } else if (fresh(8)) {
    if (screen?.type === 'map') game.closeScreen();
    else if (!screen) game.openMap();
  }
  const reading = screen && (screen.type === 'note' || screen.type === 'inspect' || screen.type === 'board' || screen.type === 'map');
  if (reading && (fresh(0) || fresh(1))) game.closeScreen();
  if (screen?.type === 'inventory' || screen?.type === 'stash') {
    if (fresh(14)) game.nudgeSel(-1);
    if (fresh(15)) game.nudgeSel(1);
    if (fresh(0)) game.useSelected();
    if (fresh(1)) game.closeScreen();
  }
  if (!screen) {
    if (fresh(0)) {
      game.input.use = true;
      padMem.use = true;
      game.use();
    }
    if (!down.has(0) && padMem.use) {
      game.input.use = false;
      padMem.use = false;
    }
    if (fresh(1)) game.dodge();
    if (fresh(2)) game.attack();
    if (fresh(3)) game.toggleLight();
    const rt = (pad.buttons[7]?.value || 0) > 0.45 || (pad.axes[5] || -1) > 0.15;
    const lt = (pad.buttons[6]?.value || 0) > 0.45 || (pad.axes[4] || -1) > 0.15;
    if (down.has(5) || rt) game.input.run = true;
    if (down.has(4) || lt) game.input.sneak = true;
    const rx = pad.axes[2] || 0;
    const ry = pad.axes[3] || 0;
    if (Math.hypot(rx, ry) > 0.35) {
      game.state.player.facing = Math.atan2(ry, rx);
      game.mouseAim = true;
    }
    let x = 0;
    let y = 0;
    if (down.has(14)) x -= 1;
    if (down.has(15)) x += 1;
    if (down.has(12)) y -= 1;
    if (down.has(13)) y += 1;
    const ax = pad.axes[0] || 0;
    const ay = pad.axes[1] || 0;
    if (Math.hypot(ax, ay) > 0.18) {
      x = ax;
      y = ay;
    }
    if ((x || y) && !stick.active && game.input.x === 0 && game.input.y === 0) {
      game.input.x = x;
      game.input.y = y;
      if (Math.hypot(rx, ry) <= 0.35) game.mouseAim = false;
    }
  }
  padMem.down = down;
}

function onPress(event) {
  if (event.target.closest('button, input, textarea') && event.code !== 'Escape') return;
  const screen = game.state?.screen;
  if (event.code === 'Escape') {
    if (!game.state || mode !== 'play') return;
    if (screen?.type === 'dead' || screen?.type === 'ending') return;
    if (screen) game.closeScreen();
    else game.openPause();
    return;
  }
  if (!game.state || mode !== 'play') return;
  if (event.code === 'KeyI' || event.code === 'Tab') {
    if (!screen || screen.type === 'inventory') game.toggleInv();
    return;
  }
  if (event.code === 'KeyM') {
    if (screen?.type === 'map') game.closeScreen();
    else game.openMap();
    return;
  }
  if (screen) {
    const reading = screen.type === 'note' || screen.type === 'inspect' || screen.type === 'board' || screen.type === 'map';
    if ((event.code === 'KeyE' || event.code === 'Enter' || event.code === 'Space') && reading) {
      game.closeScreen();
      return;
    }
    if (screen.type === 'inventory' || screen.type === 'stash') {
      if (event.code === 'ArrowLeft' || event.code === 'KeyA') game.nudgeSel(-1);
      if (event.code === 'ArrowRight' || event.code === 'KeyD') game.nudgeSel(1);
      if (event.code === 'KeyE' || event.code === 'Enter') game.useSelected();
      if (event.code === 'KeyG') game.dropSelected();
    }
    return;
  }
  if (event.code === 'KeyE') {
    game.input.use = true;
    game.use();
  } else if (event.code === 'KeyF') game.toggleLight();
  else if (event.code === 'Space') game.dodge();
  else if (event.code === 'KeyJ' || event.code === 'KeyK') game.attack();
}

function bindInput() {
  window.addEventListener('keydown', (event) => {
    audio.unlock();
    if (event.code === 'Space' || event.code === 'Tab') event.preventDefault();
    if (event.code.startsWith('Arrow') && event.target.tagName !== 'INPUT') event.preventDefault();
    const fresh = !keys.has(event.code);
    keys.add(event.code);
    if (!fresh || event.repeat) return;
    onPress(event);
  });
  window.addEventListener('keyup', (event) => {
    keys.delete(event.code);
    if (event.code === 'KeyE') game.input.use = false;
  });
  window.addEventListener('blur', () => {
    keys.clear();
    game.input.use = false;
    endStick();
  });
  window.addEventListener('resize', () => {
    applySettings();
    fit();
  });
  document.addEventListener('change', (event) => settingChanged(event.target));
  document.addEventListener('input', (event) => {
    if (event.target?.dataset?.setting && event.target.type === 'range') settingChanged(event.target);
  });

  beginBtn.addEventListener('click', startIntro);
  document.querySelector('#continue').addEventListener('click', continueGame);
  document.querySelector('#open-photo').addEventListener('click', openPhoto);
  document.querySelector('#go-inside').addEventListener('click', () => beginPlay(true));
  document.querySelector('#skip').addEventListener('click', () => beginPlay(true));
  document.querySelector('#backdrop').addEventListener('click', () => {
    const type = game.state?.screen?.type;
    if (!type || type === 'dead' || type === 'ending') return;
    game.closeScreen();
  });

  canvas.addEventListener('contextmenu', (event) => event.preventDefault());
  canvas.addEventListener('pointerdown', (event) => {
    audio.unlock();
    if (event.pointerType !== 'mouse') return;
    if (!game.state || game.state.screen || mode !== 'play') return;
    aimFrom(event);
    if (event.button === 0) game.attack();
    if (event.button === 2) game.use();
  });
  canvas.addEventListener('pointermove', (event) => {
    if (event.pointerType !== 'mouse') return;
    if (!game.state || game.state.screen || mode !== 'play') return;
    aimFrom(event);
  });

  const stickEl = document.querySelector('#stick');
  const knob = document.querySelector('#knob');
  stickEl.addEventListener('pointerdown', (event) => {
    audio.unlock();
    stick.active = true;
    stick.pointer = event.pointerId;
    stickEl.setPointerCapture(event.pointerId);
    moveStick(event, knob);
  });
  stickEl.addEventListener('pointermove', (event) => {
    if (!stick.active || event.pointerId !== stick.pointer) return;
    moveStick(event, knob);
  });
  stickEl.addEventListener('pointerup', endStick);
  stickEl.addEventListener('pointercancel', endStick);

  const pad = document.querySelector('#pad');
  pad.addEventListener('pointerdown', (event) => {
    const button = event.target.closest('button');
    if (!button || !game.state) return;
    audio.unlock();
    const act = button.dataset.act;
    if (act === 'use') {
      game.input.use = true;
      game.use();
      button.setPointerCapture(event.pointerId);
    } else if (act === 'hit') game.attack();
    else if (act === 'dodge') game.dodge();
    else if (act === 'light') game.toggleLight();
    else if (act === 'bag') game.toggleInv();
    else if (act === 'sneak') {
      touchSneak = !touchSneak;
      if (touchSneak) touchRun = false;
    } else if (act === 'run') {
      touchRun = !touchRun;
      if (touchRun) touchSneak = false;
    }
  });
  pad.addEventListener('pointerup', (event) => {
    if (event.target.closest('button')?.dataset.act === 'use') game.input.use = false;
  });
  pad.addEventListener('pointercancel', () => {
    game.input.use = false;
  });
}

function endStick() {
  stick.active = false;
  stick.x = 0;
  stick.y = 0;
  stick.pointer = -1;
  document.querySelector('#knob').style.transform = 'translate(-50%, -50%)';
}

function moveStick(event, knob) {
  const rect = document.querySelector('#stick').getBoundingClientRect();
  const dx = event.clientX - (rect.left + rect.width / 2);
  const dy = event.clientY - (rect.top + rect.height / 2);
  const mag = Math.hypot(dx, dy) || 1;
  const max = 34;
  const clamped = Math.min(max, mag);
  const nx = dx / mag;
  const ny = dy / mag;
  knob.style.transform = `translate(calc(-50% + ${nx * clamped}px), calc(-50% + ${ny * clamped}px))`;
  const strength = Math.min(1, mag / max);
  if (strength < 0.18) {
    stick.x = 0;
    stick.y = 0;
  } else {
    stick.x = nx * strength;
    stick.y = ny * strength;
  }
  game.mouseAim = false;
}

function aimFrom(event) {
  const rect = canvas.getBoundingClientRect();
  const x = (event.clientX - rect.left) * (canvas.width / rect.width);
  const y = (event.clientY - rect.top) * (canvas.height / rect.height);
  game.aim(x, y);
}

function sync() {
  const next = signature();
  if (next !== sig) {
    sig = next;
    buildOverlay();
  }
  syncHud();
  const toast = document.querySelector('#menu-toast');
  if (toast) toast.textContent = game.caption?.text || '';
}

function signature() {
  const state = game.state;
  if (!state) return '';
  const screen = state.screen;
  return [
    screen?.type || '',
    screen?.id || '',
    state.selected,
    state.equipped,
    state.flags.cameraLooks || 0,
    state.flags.tookFuse ? 1 : 0,
    state.dead ? 1 : 0,
    state.won ? 1 : 0,
    JSON.stringify(state.installed),
    state.inv.map((item) => `${item.id}:${item.count}:${item.dur}:${item.flashes}`).join(','),
    state.stash.map((item) => `${item.id}:${item.count}:${item.dur}:${item.flashes}`).join(','),
  ].join('|');
}

function syncHud() {
  const state = game.state;
  if (!state) return;
  const player = state.player;
  const hp = player.hp;
  const band = hp > 68 ? 'fine' : hp > 34 ? 'hurt' : 'crit';
  const condition = document.querySelector('#condition');
  condition.textContent = band === 'fine' ? 'FINE' : band === 'hurt' ? 'HURT' : 'CRITICAL';
  condition.className = band;
  document.querySelector('#hp').style.width = `${hp}%`;
  document.querySelector('#stam').style.width = `${player.stamina}%`;
  document.querySelector('#light').style.width = state.hasLight ? `${player.battery}%` : '0%';

  const frameEl = document.querySelector('#portrait');
  frameEl.className = band;
  if (band !== portraitBand) {
    portraitBand = band;
    clear(frameEl);
    const opts = band === 'hurt'
      ? { corrupt: 0.4, dark: 0.08 }
      : band === 'crit'
        ? { corrupt: 0.8, grin: 0.25, dark: 0.18 }
        : {};
    frameEl.appendChild(portrait('chris', { crop: 'face', size: 96, clean: band === 'fine', ...opts }));
  }

  const cells = slotsEl.children;
  for (let i = 0; i < cells.length; i += 1) {
    const slot = state.inv[i];
    const cell = cells[i];
    cell.textContent = slot ? shortName(slot) : '';
    cell.classList.toggle('equipped', i === state.equipped);
    cell.classList.toggle('on', !!state.screen && i === state.selected && !!slot);
  }

  document.querySelector('#weapon').textContent = weaponLine();
  document.querySelector('#bagcount').textContent = `${state.inv.length}/${TUNING.slots}`;
  const stance = document.querySelector('#stance');
  stance.textContent = player.sneaking ? 'QUIET' : player.running ? 'LOUD' : 'WALK';
  stance.className = player.sneaking ? 'quiet' : player.running ? 'loud' : '';
  const film = state.inv.filter((slot) => slot.id === 'film').reduce((sum, slot) => sum + slot.count, 0);
  document.querySelector('#film').textContent = `FILM ${film}`;

  const hint = document.querySelector('#hint');
  if (state.hasLight) hint.textContent = '';
  else if (document.body.classList.contains('touch')) hint.textContent = 'Stick moves · Use acts · Bag holds six';
  else hint.textContent = 'WASD move · mouse aims · E use · I bag — six slots';

  const banner = game.roomBanner;
  document.querySelector('#room-name').textContent = banner && game.uiTime < banner.until ? banner.name : '';
  document.querySelector('#objective').textContent = game.objective();
  for (const node of document.querySelectorAll('#pips span')) {
    node.classList.toggle('on', !!state.installed[node.dataset.part]);
  }

  const prompt = game.prompt();
  const promptEl = document.querySelector('#prompt');
  if (!prompt) promptEl.hidden = true;
  else {
    promptEl.hidden = false;
    let text = prompt.text;
    if (text.startsWith('HOLD E') && document.body.classList.contains('touch')) text = 'HOLD USE TO GO';
    document.querySelector('#prompt-text').textContent = text;
    document.querySelector('#prompt-bar').style.width = prompt.fill ? `${Math.round(prompt.fill * 100)}%` : '0%';
  }

  const subtitle = document.querySelector('#subtitle');
  if (game.caption) {
    subtitle.hidden = false;
    subtitle.dataset.kind = game.caption.kind;
    subtitle.textContent = game.caption.text;
  } else {
    subtitle.hidden = true;
    subtitle.textContent = '';
  }

  document.querySelector('#hurt').style.opacity = String(Math.min(0.65, game.hurtFlash || 0));
  document.querySelector('#whiteflash').style.opacity = String((game.flash || 0) * (game.settings.reduceFlash ? 0.2 : 0.8));
  const grin = document.querySelector('#grinflash');
  if (game.grinFlash > 0.02 && !game.settings.reduceFlash) {
    grin.hidden = false;
    grin.style.opacity = String(Math.min(1, game.grinFlash * 4));
  } else grin.hidden = true;

  const sneakBtn = document.querySelector('[data-act="sneak"]');
  const runBtn = document.querySelector('[data-act="run"]');
  sneakBtn.classList.toggle('on', touchSneak);
  runBtn.classList.toggle('on', touchRun);
  sneakBtn.setAttribute('aria-pressed', touchSneak ? 'true' : 'false');
  runBtn.setAttribute('aria-pressed', touchRun ? 'true' : 'false');
}

function weaponLine() {
  const slot = game.state.inv[game.state.equipped];
  if (!slot) return 'Unarmed';
  const def = ITEMS[slot.id];
  if (!def) return 'Unarmed';
  if (slot.dur != null) return `${def.name} ${Math.max(0, slot.dur)}`;
  if (slot.flashes != null) return `Camera ${slot.flashes}`;
  if (def.ammo) {
    const ammo = game.state.inv.find((item) => item.id === 'ammo');
    return `Revolver ${ammo ? ammo.count : 0}`;
  }
  return def.name;
}

function shortName(slot) {
  const def = ITEMS[slot.id];
  if (!def) return '?';
  const first = def.name.split(' ')[0];
  if (slot.flashes != null) return `Cam ${slot.flashes}`;
  if (slot.dur != null && slot.dur <= 0) return 'Broken';
  if ((slot.count || 1) > 1) return `${first} ${slot.count}`;
  return first;
}

function buildOverlay() {
  const screen = game.state?.screen;
  if (!screen || mode !== 'play') {
    overlay.hidden = true;
    overlay.classList.remove('bleed');
    screenType = '';
    return;
  }
  overlay.hidden = false;
  const type = screen.type;
  const changed = type !== screenType;
  screenType = type;
  overlay.classList.toggle('bleed', type === 'dead' || type === 'ending');
  clear(card);
  if (type === 'note') buildNote(screen.id);
  else if (type === 'inspect') buildInspect(screen.id);
  else if (type === 'inventory') buildInventory();
  else if (type === 'stash') buildStash();
  else if (type === 'car') buildCar(true);
  else if (type === 'board') buildCar(false);
  else if (type === 'map') buildMap();
  else if (type === 'pause') buildPause();
  else if (type === 'dead') buildDead();
  else if (type === 'ending') buildEnding();
  if (type !== 'dead' && type !== 'ending') {
    const toast = el('p', 'toast');
    toast.id = 'menu-toast';
    card.appendChild(toast);
  }
  if (changed) card.focus();
}

function buildNote(id) {
  const note = NOTES[id] || { title: 'Note', body: '' };
  card.appendChild(el('h2', '', note.title));
  if (note.photo) {
    const fig = el('figure', 'photos');
    fig.appendChild(portrait(note.photo, { crop: 'bust', size: 160, clean: true }));
    card.appendChild(fig);
  }
  const paper = el('div', id === 'ray' ? 'paper scratched' : 'paper');
  writeLines(paper, note.body);
  card.appendChild(paper);
  card.appendChild(actions([btn('Close', () => game.closeScreen())]));
}

function buildInspect(id) {
  const info = INSPECTIONS[id] || { title: 'Look', body: '' };
  card.appendChild(el('h2', '', info.title));
  if (info.photos?.length) {
    const row = el('div', 'photos');
    const corruptMom = id === 'family' && game.state.flags.tookFuse;
    for (const spec of info.photos) {
      const fig = el('figure');
      const opts = {
        crop: spec.crop || 'bust',
        size: 140,
        corrupt: spec.corrupt || 0,
        grin: spec.grin || 0,
      };
      if (corruptMom && spec.id === 'mom') {
        opts.corrupt = 0.8;
        opts.grin = 0.55;
      }
      opts.clean = !(opts.corrupt || opts.grin);
      fig.appendChild(portrait(spec.id, opts));
      if (spec.label) fig.appendChild(el('figcaption', '', spec.label));
      row.appendChild(fig);
    }
    card.appendChild(row);
  }
  writeLines(card, info.body);
  card.appendChild(actions([btn('Close', () => game.closeScreen())]));
}

function buildInventory() {
  card.appendChild(el('h2', '', 'Hands'));
  card.appendChild(el('p', 'muted', `${game.state.inv.length} of ${TUNING.slots} slots. Heals are used from here.`));
  if (!game.state.inv.length) card.appendChild(el('p', '', 'Empty. The house is not.'));
  else card.appendChild(itemList(game.state.inv, game.state.selected, (index) => {
    game.state.selected = index;
  }));
  const slot = game.state.inv[game.state.selected];
  if (slot) {
    const def = ITEMS[slot.id];
    card.appendChild(el('h2', '', def?.name || 'Item'));
    for (const line of slotFacts(slot)) card.appendChild(el('p', '', line));
    if (slot.id === 'camera') {
      const face = el('div', 'camera-face');
      face.appendChild(cameraFace());
      card.appendChild(face);
    }
    const buttons = [];
    if (def?.type === 'heal') buttons.push(btn('Use', () => game.useSelected()));
    if (def?.type === 'weapon' || def?.type === 'camera') {
      if (game.state.equipped !== game.state.selected) buttons.push(btn('Equip', () => game.useSelected()));
    }
    if (def?.type === 'tape') buttons.push(btn('Wrap', () => game.useSelected()));
    if (slot.id === 'camera') buttons.push(btn('Look', () => game.lookCamera()));
    buttons.push(btn('Drop', () => game.dropSelected()));
    card.appendChild(actions(buttons));
  }
  card.appendChild(actions([btn('Close', () => game.closeScreen())]));
}

function buildStash() {
  card.appendChild(el('h2', '', 'Parts bin'));
  card.appendChild(el('p', 'muted', 'The bin keeps what your hands cannot. The car takes what you are holding.'));
  const split = el('div', 'split');
  const left = el('div');
  const right = el('div');
  left.appendChild(el('p', 'muted', 'Hands'));
  if (!game.state.inv.length) left.appendChild(el('p', '', 'Empty.'));
  else {
    left.appendChild(itemList(game.state.inv, game.state.selected, (index) => {
      game.state.selected = index;
    }));
    const row = actions([
      btn('Store', () => game.storeSelected()),
      btn('Drop', () => game.dropSelected()),
    ]);
    left.appendChild(row);
  }
  right.appendChild(el('p', 'muted', 'Bin'));
  if (!game.state.stash.length) right.appendChild(el('p', '', 'Empty.'));
  game.state.stash.forEach((slot, index) => {
    const def = ITEMS[slot.id];
    right.appendChild(btn(def?.name || 'Item', () => game.takeStash(index), 'row'));
  });
  split.appendChild(left);
  split.appendChild(right);
  card.appendChild(split);
  card.appendChild(actions([btn('Close', () => game.closeScreen())]));
}

function buildCar(interactive) {
  card.appendChild(el('h2', '', interactive ? 'The car' : 'Checklist'));
  const mom = el('div', 'photos');
  const fig = el('figure');
  fig.appendChild(portrait('mom', {
    crop: 'bust',
    size: 84,
    dark: 0.12,
    corrupt: game.state.flags.tookFuse ? 0.28 : 0.04,
    grin: game.state.flags.tookFuse ? 0.12 : 0,
  }));
  fig.appendChild(el('figcaption', '', 'Through the glass'));
  mom.appendChild(fig);
  card.appendChild(mom);
  card.appendChild(el('p', '', 'Mom is in the passenger seat. She does not look up.'));
  card.appendChild(el('p', 'slip', "Don't answer me"));
  if (interactive) card.appendChild(el('p', 'muted', 'The car takes what is in your hands.'));
  for (const part of PARTS) {
    const row = el('div', game.state.installed[part.id] ? 'part on' : 'part');
    row.appendChild(el('div', 'name', part.name));
    const where = el('div', 'where', `${part.detail}. ${partPlace(part)}`);
    row.appendChild(where);
    if (interactive && !game.state.installed[part.id] && game.itemLoc(part.item) === 'inv') {
      const label = part.id === 'photo' ? 'Photograph' : 'Seat';
      row.appendChild(btn(label, () => game.install(part.id)));
    }
    card.appendChild(row);
  }
  card.appendChild(actions([btn('Close', () => game.closeScreen())]));
}

function partPlace(part) {
  if (game.state.installed[part.id]) return 'In the car';
  const loc = game.itemLoc(part.item);
  if (loc === 'inv') return 'In your hands';
  if (loc === 'stash') return 'In the bin';
  if (loc === 'floor') return 'On the floor';
  return 'Still out there';
}

function buildMap() {
  card.appendChild(el('h2', '', 'The house'));
  const map = document.createElement('canvas');
  map.id = 'map';
  map.width = 420;
  map.height = 280;
  card.appendChild(map);
  drawMap(map.getContext('2d'), game, map.width, map.height);
  card.appendChild(el('p', 'muted', 'Amber is shut. Red is locked. Dim rooms are next door, not yet walked.'));
  card.appendChild(actions([btn('Close', () => game.closeScreen())]));
}

function buildPause() {
  card.appendChild(el('h2', '', 'Paused'));
  const list = el('div', 'controls');
  const lines = [
    ['Move', 'WASD or arrows'],
    ['Aim', 'Mouse'],
    ['Run', 'Hold Shift'],
    ['Sneak', 'Hold C'],
    ['Dodge', 'Space'],
    ['Light', 'F'],
    ['Use', 'E or right click'],
    ['Hit', 'J, K, or left click'],
    ['Bag', 'I or Tab'],
    ['Map', 'M'],
    ['Drop', 'G, from the bag'],
    ['Gamepad', 'Stick move, A use, B dodge, X hit, Y light'],
  ];
  for (const [name, how] of lines) list.appendChild(el('span', '', `${name} ${how}`));
  card.appendChild(list);
  const settings = el('div', 'settings');
  settings.appendChild(check('Less flicker', 'reduceFlash', !!game.settings.reduceFlash));
  settings.appendChild(check('Screen shake', 'shake', game.settings.shake !== false));
  settings.appendChild(check('Grade', 'post', game.settings.post !== false));
  settings.appendChild(check('Captions', 'captions', game.settings.captions !== false));
  settings.appendChild(check('Touch controls', 'touch', touchPreferred()));
  settings.appendChild(slider('Volume', 'volume'));
  settings.appendChild(slider('Music', 'music'));
  settings.appendChild(slider('Effects', 'sfx'));
  settings.appendChild(slider('Room', 'ambience'));
  card.appendChild(settings);
  card.appendChild(actions([
    btn('Resume', () => game.closeScreen(), 'primary'),
    btn('Title screen', backToTitle),
  ]));
  card.appendChild(el('p', 'muted', 'The title keeps a slide, if you took one.'));
}

function buildDead() {
  if (!game.settings.reduceFlash) {
    const img = el('img', 'bleed-photo face-grin');
    img.src = 'assets/grin.jpg';
    img.alt = '';
    card.appendChild(img);
  }
  const copy = el('div', 'end-copy');
  copy.appendChild(el('h2', '', 'You looked too long'));
  copy.appendChild(el('p', 'lead', hasSave()
    ? 'The bench still has you.'
    : 'There was no slide in the bench.'));
  const buttons = [];
  if (hasSave()) buttons.push(btn('Wake at the bench', wakeFromBench, 'primary'));
  else buttons.push(btn('Start the drive again', wakeFromBench, 'primary'));
  buttons.push(btn('Title screen', backToTitle));
  copy.appendChild(actions(buttons));
  card.appendChild(copy);
}

function buildEnding() {
  const img = el('img', 'bleed-photo face-mom');
  img.src = 'assets/mom.jpg';
  img.alt = '';
  card.appendChild(img);
  const copy = el('div', 'end-copy');
  if (!game.settings.reduceFlash) {
    const mirror = el('div', 'rearview tiny');
    const grin = el('img', 'face-grin');
    grin.src = 'assets/grin.jpg';
    grin.alt = '';
    mirror.appendChild(grin);
    copy.appendChild(mirror);
  }
  copy.appendChild(el('h2', '', 'The color comes back'));
  copy.appendChild(el('p', 'lead', 'Mom is in the passenger seat. She doesn’t ask what you saw. The camera is face-down on the dash.'));
  copy.appendChild(el('p', 'slip', 'Don’t look back'));
  copy.appendChild(actions([btn('Leave it', backToTitle, 'primary')]));
  card.appendChild(copy);
}

function cameraFace() {
  const looks = game.state.flags.cameraLooks || 0;
  if (looks >= 3) return portrait('grin', { crop: 'face', size: 160, corrupt: 0.12 });
  if (looks >= 2) return portrait('evan', { crop: 'face', size: 160, grin: 0.55, corrupt: 0.35 });
  if (looks === 1) return portrait('evan', { crop: 'face', size: 160, grin: 0.12 });
  return portrait('evan', { crop: 'face', size: 160, clean: true });
}

function slotFacts(slot) {
  const def = ITEMS[slot.id];
  const lines = [];
  if ((slot.count || 1) > 1) lines.push(`Carrying ${slot.count}.`);
  if (slot.dur != null) lines.push(slot.dur > 0 ? `Durability ${slot.dur}.` : 'Broken.');
  if (slot.flashes != null) lines.push(`Bulbs ${slot.flashes}. The plate is for the car.`);
  if (def?.ammo) {
    const ammo = game.state.inv.find((item) => item.id === 'ammo');
    lines.push(`Rounds ${ammo ? ammo.count : 0}.`);
  }
  if (def?.desc) lines.push(def.desc);
  return lines;
}

function itemList(items, selected, onPick) {
  const list = el('div');
  items.forEach((slot, index) => {
    const def = ITEMS[slot.id];
    const mark = index === game.state.equipped ? ' · held' : '';
    const count = (slot.count || 1) > 1 ? ` ${slot.count}` : '';
    list.appendChild(btn(`${index + 1}  ${def?.name || 'Item'}${count}${mark}`, () => onPick(index), index === selected ? 'row on' : 'row'));
  });
  return list;
}

function slider(label, key) {
  const node = document.createElement('label');
  node.appendChild(document.createTextNode(`${label} `));
  const range = document.createElement('input');
  range.type = 'range';
  range.min = '0';
  range.max = '1';
  range.step = '0.05';
  range.value = String(game.settings[key] ?? 0.8);
  range.dataset.setting = key;
  node.appendChild(range);
  return node;
}

function check(label, key, checked) {
  const node = document.createElement('label');
  const input = document.createElement('input');
  input.type = 'checkbox';
  input.dataset.setting = key;
  input.checked = checked;
  node.appendChild(input);
  node.appendChild(document.createTextNode(` ${label}`));
  return node;
}

function writeLines(parent, text) {
  for (const line of String(text || '').split('\n')) {
    parent.appendChild(el('p', '', line));
  }
}

function actions(buttons) {
  const row = el('div', 'actions');
  for (const button of buttons) row.appendChild(button);
  return row;
}

function btn(label, onClick, className) {
  const node = el('button', className || '', label);
  node.type = 'button';
  node.addEventListener('click', () => {
    audio.unlock();
    audio.ui();
    onClick();
  });
  return node;
}

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text != null) node.textContent = text;
  return node;
}

function clear(node) {
  while (node.firstChild) node.removeChild(node.firstChild);
}

function buildGallery() {
  clear(gallery);
  gallery.hidden = false;
  gallery.appendChild(el('h2', '', 'Faces'));
  const grid = el('div', 'gallery-grid');
  for (const id of ['chris', 'her', 'mom', 'evan', 'basement', 'grin', 'teammate']) {
    for (const crop of ['face', 'bust']) {
      const fig = el('figure');
      fig.appendChild(portrait(id, { crop, size: 140 }));
      fig.appendChild(el('figcaption', '', `${id} ${crop}`));
      grid.appendChild(fig);
    }
  }
  gallery.appendChild(grid);
  gallery.appendChild(btn('Close', () => { gallery.hidden = true; }));
}
