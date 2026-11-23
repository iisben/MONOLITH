import fs from 'fs';
import {
  CaptionQueue,
  FADE_IN,
  FADE_OUT,
  captionFinished,
  captionHold,
  captionOpacity,
} from '../js/captions.mjs';
import { INSPECTIONS, ITEMS, MOM_LINES, MOM_LINES_LATE, NOTES, describeGoal } from '../js/data.mjs';
import { sequenceBeats } from '../js/intro.mjs';
import { DOORS, PROPS, ROOMS } from '../js/world.mjs';

const problems = [];
const fail = (msg) => problems.push(msg);

const base = {
  installed: { fuse: false, keys: false, gas: false, photo: false },
  read: {},
  loc: Object.fromEntries(['fuse', 'gas', 'camera', 'key-car', 'key-her', 'key-base', 'key-shed'].map((id) => [id, 'world'])),
  hasLight: false,
  finale: false,
  room: 'mud',
  tookBaseKey: false,
  herOpen: false,
  shedOpen: false,
};

function goal(patch) {
  return describeGoal({
    ...base,
    ...patch,
    installed: { ...base.installed, ...(patch.installed || {}) },
    read: { ...base.read, ...(patch.read || {}) },
    loc: { ...base.loc, ...(patch.loc || {}) },
  });
}

if (!goal({}).includes('light')) fail('opening objective should ask for a light');
if (!goal({ hasLight: true }).toLowerCase().includes('car')) fail('a light should point at the car');
if (!goal({ hasLight: true, read: { foyer: true } }).toLowerCase().includes('not mom')) fail('foyer note should spoil the voice');

const seen = new Set();
const samples = [
  {},
  { hasLight: true },
  { hasLight: true, read: { foyer: true } },
  { hasLight: true, read: { foyer: true, kitchen: true } },
  { hasLight: true, read: { kitchen: true }, herOpen: true, loc: { 'key-car': 'world' } },
  { room: 'her', loc: { 'key-car': 'world' } },
  { room: 'shed', loc: { gas: 'world' } },
  { room: 'basement', loc: { fuse: 'world' } },
  { loc: { fuse: 'inv' } },
  { loc: { fuse: 'stash' } },
  { loc: { fuse: 'floor' } },
  { loc: { gas: 'inv' } },
  { loc: { gas: 'stash' } },
  { loc: { 'key-car': 'inv' } },
  { loc: { 'key-car': 'stash' } },
  { loc: { 'key-her': 'inv' } },
  { loc: { 'key-her': 'stash' } },
  { loc: { 'key-her': 'floor' } },
  { loc: { 'key-base': 'inv' } },
  { loc: { 'key-base': 'stash' } },
  { tookBaseKey: true, loc: { 'key-base': 'world' } },
  { read: { diary: true }, loc: { 'key-base': 'world' } },
  { read: { ray: true }, loc: { fuse: 'world' } },
  { installed: { fuse: true }, read: { journal: true } },
  { installed: { fuse: true }, shedOpen: true },
  { installed: { fuse: true }, loc: { 'key-shed': 'stash' } },
  { installed: { fuse: true, keys: true, gas: true }, loc: { camera: 'inv' } },
  { installed: { fuse: true, keys: true, gas: true }, loc: { camera: 'stash' } },
  { installed: { fuse: true, keys: true, gas: true }, loc: { camera: 'world' } },
  { finale: true },
  { hasLight: true, read: { foyer: true }, installed: { keys: true }, loc: { 'key-base': 'world' } },
];

for (const sample of samples) {
  const text = goal(sample);
  seen.add(text);
  if (!text || !text.trim()) fail(`blank objective for ${JSON.stringify(sample)}`);
  if (text.length > 68) fail(`objective too long (${text.length}): ${text}`);
}

if (!goal({ loc: { fuse: 'stash' } }).includes('bin')) fail('a fuse in the bin should say so');
if (goal({ loc: { fuse: 'stash' } }).toLowerCase().includes('seat')) fail('a fuse in the bin is not ready to seat');
if (!goal({ read: { kitchen: true }, herOpen: true }).toLowerCase().includes('journal')) fail('an open her door should point at the keys');
if (goal({ read: { kitchen: true }, herOpen: true }).toLowerCase().includes('cabinet')) fail('used her key should not send you back to the cabinet');
if (goal({ room: 'her', loc: { 'key-car': 'world' } }).toLowerCase().includes('desk')) fail('her room has no desk');
if (goal({}).toLowerCase().includes('table')) fail('the foyer has no table');

const banned = [
  'foyer table',
  'upstairs',
  'dresser',
  'nightstand',
  'in the drawer',
  'on the desk',
  'on this bench',
  'on the shelf',
  'ben, on the trail',
  'emergency lights',
  'driver seat',
];

const roots = [
  new URL('../js/data.mjs', import.meta.url),
  new URL('../js/game.mjs', import.meta.url),
  new URL('../js/main.mjs', import.meta.url),
  new URL('../js/intro.mjs', import.meta.url),
  new URL('../js/world.mjs', import.meta.url),
  new URL('../index.html', import.meta.url),
];
for (const url of roots) {
  const text = fs.readFileSync(url, 'utf8').toLowerCase();
  for (const phrase of banned) {
    if (text.includes(phrase)) fail(`${url.pathname.split('/').pop()} still says "${phrase}"`);
  }
}

for (const prop of PROPS) {
  if (prop.note && !NOTES[prop.note]) fail(`missing note ${prop.note}`);
  if (prop.inspect && !INSPECTIONS[prop.inspect]) fail(`missing inspection ${prop.inspect}`);
  if (prop.item && !ITEMS[prop.item]) fail(`missing item ${prop.item}`);
  if (!prop.label) fail(`prop ${prop.id} has no label`);
}
for (const door of DOORS) {
  if (door.lock && door.lock !== 'power' && !ITEMS[door.lock]) fail(`door ${door.id} lock ${door.lock} is not an item`);
}
for (const id of Object.keys(ROOMS)) {
  if (!ROOMS[id].name) fail(`room ${id} has no name`);
}

const noteText = Object.values(NOTES).map((note) => note.body).join('\n').toLowerCase();
for (const clue of ['flashlight', 'foyer', 'sweater', "isn't me", 'six', 'bin', 'bathroom', 'cabinet', 'two rounds', 'by the bed', 'garage bench', 'shed', 'by the shelf', 'tanks', 'yard door']) {
  if (!noteText.includes(clue)) fail(`notes lost the clue "${clue}"`);
}

const short = captionHold('The smile.');
const long = captionHold('The sweater is warm and the bed is breathing in the dark of the room tonight.');
if (!(long > short)) fail('longer captions must last longer');
if (short < 1.7) fail(`hold floor too low: ${short}`);
if (long > 7.2) fail(`hold ceiling too high: ${long}`);
if (captionOpacity(0, 2, true) !== 0) fail('fade should start clear');
if (captionOpacity(FADE_IN + 0.4, 2, true) !== 1) fail('fade should hold at full');
if (captionOpacity(FADE_IN + 2 + FADE_OUT, 2, true) !== 0) fail('fade should end clear');
if (captionOpacity(0, 2, false) !== 1) fail('reduced motion should show the line at once');
if (!captionFinished(FADE_IN + 2 + FADE_OUT, 2, true)) fail('caption should finish after the fade');
if (captionFinished(1, 2, true)) fail('caption finished before it could be read');

const queue = new CaptionQueue();
queue.push({ text: 'First line of the house', kind: 'say', priority: 4 });
queue.push({ text: 'Second line of the house', kind: 'say', priority: 4 });
queue.push({ text: 'A board creaks ahead.', kind: 'ambient', priority: 1 });
if (queue.current.text !== 'First line of the house') fail('queue replaced the line on screen');
queue.advance(0.4, true);
if (queue.current.text !== 'First line of the house') fail('caption vanished too fast');
queue.advance(captionHold('First line of the house') + FADE_IN + FADE_OUT, true);
if (queue.current?.text !== 'Second line of the house') fail('the next caption was dropped');
if (queue.pending[0]?.kind !== 'ambient') fail('a lower priority line should wait');

const quiet = new CaptionQueue();
quiet.push({ text: 'A board creaks behind you.', kind: 'ambient', priority: 1, ambient: false });
if (quiet.current) fail('captions off should skip ambient lines');
quiet.push({ text: 'The lock gives.', kind: 'say', priority: 4, ambient: false });
if (quiet.current?.text !== 'The lock gives.') fail('gameplay lines must stay when captions are off');

for (const line of [...MOM_LINES, ...MOM_LINES_LATE]) {
  if (!line.trim()) fail('blank mom line');
  if (line.toLowerCase().includes('upstairs')) fail('mom sent you upstairs');
}

for (const beat of sequenceBeats(true)) {
  const marks = [...(beat.lines || [])];
  if (beat.aside) marks.push(beat.aside);
  for (const mark of marks) {
    const remain = (1 - Math.min(0.92, mark.at || 0)) * beat.dur;
    if (remain + 0.02 < captionHold(mark.text)) {
      fail(`${beat.id} drops "${mark.text}" after ${remain.toFixed(2)}s`);
    }
  }
}

if (problems.length) {
  console.error(problems.join('\n'));
  console.error(`\n${seen.size} objectives checked`);
  process.exit(1);
}
console.log(`copy ok — ${seen.size} objectives, ${PROPS.length} props, ${Object.keys(NOTES).length} notes`);
