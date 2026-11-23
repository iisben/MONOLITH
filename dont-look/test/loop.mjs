import { ITEMS } from '../js/data.mjs';
import { ITEM_CUES } from '../js/item-cues.mjs';
import { INK, SPRITES, artIdForProp, artIds, maskOf } from '../js/art.mjs';
import { PROPS } from '../js/world.mjs';
import { Game } from '../js/game.mjs';

const problems = [];
const fail = (msg) => problems.push(msg);
const TILE = 32;

function lum(hex) {
  const n = Number.parseInt(hex.slice(1), 16);
  const ch = [(n >> 16) & 255, (n >> 8) & 255, n & 255].map((v) => {
    const s = v / 255;
    return s <= 0.04045 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
  });
  return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
}

const bg = lum('#100e0c');
const masks = new Map();
for (const id of artIds()) {
  const def = SPRITES[id];
  const inks = new Set();
  let filled = 0;
  let brightest = 0;
  for (const row of def.rows) {
    for (const ch of row) {
      if (ch === ' ') continue;
      filled += 1;
      inks.add(ch);
      brightest = Math.max(brightest, lum(INK[ch]));
    }
  }
  if (filled < 12) fail(`${id} has only ${filled} pixels`);
  if (inks.size < 2) fail(`${id} uses a single ink`);
  if ((brightest + 0.05) / (bg + 0.05) < 3) fail(`${id} brightest ink is under 3:1 on #100e0c`);
  if (def.rows[def.gy]?.[def.gx] === ' ' || def.rows[def.gy]?.[def.gx] == null) fail(`${id} glint is not on a filled pixel`);
  if (!def.material) fail(`${id} has no material`);
  if (!def.glint) fail(`${id} has no glint`);
  const mask = maskOf(id);
  if (masks.has(mask)) fail(`${id} shares a silhouette with ${masks.get(mask)}`);
  else masks.set(mask, id);
  if (id !== 'stick-cracked' && !ITEM_CUES[`${id}:take`] && !ITEM_CUES[`${id}:use`]) {
    fail(`${id} has no pickup or use sound`);
  }
}

const cueSeen = new Map();
for (const [key, spec] of Object.entries(ITEM_CUES)) {
  const body = JSON.stringify(spec);
  if (cueSeen.has(body)) fail(`${key} repeats the sound of ${cueSeen.get(body)}`);
  else cueSeen.set(body, key);
  for (const step of spec) {
    if (step.peak > 0.05) fail(`${key} peaks at ${step.peak}`);
  }
}

const placed = new Set();
for (const prop of PROPS) {
  if (!artIdForProp(prop) || !SPRITES[artIdForProp(prop)]) fail(`prop ${prop.id} has no sprite`);
  if (prop.kind === 'pickup') placed.add(prop.item);
  for (const item of prop.items || []) if (item.item) placed.add(item.item);
}
for (const id of Object.keys(ITEMS)) {
  if (!placed.has(id)) fail(`${id} is never placed in the house`);
  if (!SPRITES[id]) fail(`${id} has no inventory sprite`);
}

const store = new Map();
globalThis.localStorage = {
  getItem: (key) => (store.has(key) ? store.get(key) : null),
  setItem: (key, value) => store.set(key, String(value)),
  removeItem: (key) => store.delete(key),
};
const calls = [];
const audio = new Proxy({}, {
  get: (_target, key) => (...args) => {
    calls.push(key);
    return args;
  },
});
const game = new Game({ getContext: () => ({}) }, audio);

function at(id) {
  const prop = game.world.props.find((item) => item.id === id)
    || game.world.doors.find((item) => item.id === id);
  game.state.player.x = (prop.x + 0.5) * TILE;
  game.state.player.y = (prop.y + 0.5) * TILE;
  game.state.screen = null;
}

function useAt(id) {
  at(id);
  game.use();
  if (game.state.screen?.type === 'note' || game.state.screen?.type === 'inspect') game.closeScreen();
}

function count(id) {
  return game.state.inv.filter((slot) => slot.id === id).reduce((sum, slot) => sum + (slot.count || 1), 0);
}

function has(id) {
  return game.state.inv.some((slot) => slot.id === id);
}

game.newGame();
useAt('her');
if (game.state.doors.her.open || game.state.doors.her.unlocked) fail('her door opened without the key');
useAt('yard');
if (game.state.doors.yard.open || game.state.flags.powerOn) fail('yard door opened before the fuse');

at('bath');
game.use();
if (game.state.doors.bath.open) fail('an open door refused to close');
const bath = game.world.doors.find((door) => door.id === 'bath');
const onBath = Math.floor(game.state.player.x / TILE) === bath.x && Math.floor(game.state.player.y / TILE) === bath.y;
if (onBath) fail('closing the bath door left you inside it');
if (game.hitsSolid(game.state.player, 9)) fail('closing a door shoved you into a wall');

game.newGame();
game.give('ammo', 10);
game.give('ammo', 4);
if (count('ammo') !== 14) fail(`ammo 10+4 became ${count('ammo')}`);
game.newGame();
for (let i = 0; i < 6; i += 1) {
  if (!game.give('stick')) fail('a free slot refused a stick');
}
if (game.give('knife')) fail('a full bag accepted another item');
if (count('stick') !== 6) fail('a refused item still changed the bag');
game.newGame();
game.give('film', 1);
const remember = localStorage.setItem;
localStorage.setItem = () => { throw new Error('quota'); };
game.useSave();
localStorage.setItem = remember;
if (count('film') !== 1) fail('a failed save ate the film');

game.newGame();
const snap = JSON.stringify(PROPS);
useAt('light');
if (!game.state.hasLight) fail('flashlight did not turn on');
useAt('note-foyer');
if (!game.state.flags.read.foyer) fail('foyer note was not read');
useAt('cabinet');
useAt('cabinet');
if (!has('key-her') || !has('bandage')) fail('the cabinet did not give both items');
useAt('her');
if (!game.state.doors.her.open || has('key-her')) fail('her key did not open her door');
useAt('keys');
if (!has('key-car') || game.state.doors.her.open) fail('taking the car keys did not slam her door');
useAt('sweater');
if (!has('key-base')) fail('the sweater pocket was empty');
useAt('base');
if (!game.state.doors.base.open || has('key-base')) fail('the basement key did not open the basement');
useAt('fuse');
useAt('camera');
useAt('shedkey');
if (!has('fuse') || !has('camera') || !has('key-shed')) fail('the basement bench did not give its tools');
game.state.player.x = 80;
game.state.player.y = 80;
game.install('fuse');
if (game.state.installed.fuse) fail('the fuse seated itself away from the car');
at('car');
game.install('fuse');
if (!game.state.flags.powerOn || !game.state.doors.yard.open) fail('the fuse did not open the yard');
game.install('keys');
useAt('gas');
if (!has('gas')) fail('the gas can was unreachable');
at('car');
game.install('gas');
if (!game.state.installed.gas || has('gas')) fail('the gas did not go into the tank');
game.install('photo');
if (!game.state.flags.finale || has('camera')) fail('photographing the car did not start the drive');
const grin = game.state.enemies.find((enemy) => enemy.kind === 'grin');
if (!grin?.awake) fail('the grin did not wake for the drive');
game.state.player.iframes = 30;
game.input.use = true;
game.input.x = 0;
game.input.y = 0;
for (let i = 0; i < 50 && !game.state.won; i += 1) game.update(0.05);
if (!game.state.won || game.state.screen?.type !== 'ending') fail('holding at the car did not end the night');
game.newGame();
if (game.state.flags.finale || game.state.inv.length || game.state.doors.her.unlocked) fail('a new night kept the last night');
if (game.state.containers.cabinet.length !== 2) fail('the cabinet did not refill');
if (game.state.enemies.some((enemy) => enemy.kind === 'grin' && enemy.awake)) fail('the grin stayed awake into a new night');
if (JSON.stringify(PROPS) !== snap) fail('picking things up rewrote the world catalog');

if (problems.length) {
  console.error(problems.join('\n'));
  process.exit(1);
}
console.log(`loop ok — ${artIds().length} unique sprites, ${Object.keys(ITEM_CUES).length} cues, drive home closed`);
