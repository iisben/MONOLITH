import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { MOM_LINES, NOTES, INSPECTIONS, ITEMS, describeGoal } from '../js/data.mjs';

const lock = JSON.parse(readFileSync(new URL('./narrative-lock.json', import.meta.url), 'utf8'));
const narrative = { MOM_LINES, NOTES, INSPECTIONS };
const dataHash = createHash('sha256').update(JSON.stringify(narrative)).digest('hex');
if (dataHash !== lock.dataHash) {
  console.error('narrative data changed');
  process.exit(1);
}
const itemDesc = Object.fromEntries(Object.entries(ITEMS).map(([id, item]) => [id, item.desc]));
if (JSON.stringify(itemDesc) !== JSON.stringify(lock.itemDesc)) {
  console.error('item descriptions changed');
  process.exit(1);
}
const base = {
  installed: { fuse: false, keys: false, gas: false, photo: false },
  read: {},
  loc: {},
  finale: false,
  room: 'mud',
  hasLight: false,
  tookBaseKey: false,
};
const goalProbe = [
  describeGoal(base),
  describeGoal({ ...base, hasLight: true, read: { foyer: true } }),
];
if (JSON.stringify(goalProbe) !== JSON.stringify(lock.goalProbe)) {
  console.error('goal text changed', goalProbe);
  process.exit(1);
}
const game = readFileSync(new URL('../js/game.mjs', import.meta.url), 'utf8');
const calls = [];
const re = /this\.(say|ambient|voice)\(([\s\S]*?)\)/g;
let match;
while ((match = re.exec(game))) calls.push(`${match[1]}:${match[2].replace(/\s+/g, ' ').trim()}`);
if (JSON.stringify(calls) !== JSON.stringify(lock.calls)) {
  console.error('spoken lines changed', calls.length, lock.calls.length);
  process.exit(1);
}
const pages = [
  readFileSync(new URL('../index.html', import.meta.url), 'utf8'),
  readFileSync(new URL('../js/main.mjs', import.meta.url), 'utf8'),
  readFileSync(new URL('../js/data.mjs', import.meta.url), 'utf8'),
  game,
].join('\n');
for (const line of lock.lines) {
  if (!pages.includes(line)) {
    console.error('missing line', line);
    process.exit(1);
  }
}
console.log('narrative ok');
