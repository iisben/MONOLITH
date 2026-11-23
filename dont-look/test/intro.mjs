import fs from 'fs';
import { createAudio } from '../js/audio.mjs';
import {
  STORY,
  activeBeat,
  bootOpacity,
  flickerLevel,
  handoffStart,
  linesAt,
  sequenceBeats,
  storyScript,
  totalDuration,
} from '../js/intro.mjs';

const problems = [];
const fail = (msg) => problems.push(msg);

const ids = ['drive', 'phone', 'fine', 'photo', 'void', 'arrive', 'house', 'garage', 'handoff'];
const full = sequenceBeats(false);
const still = sequenceBeats(true);

if (full.map((beat) => beat.id).join() !== ids.join()) {
  fail(`beat order ${full.map((beat) => beat.id).join(',')}`);
}
if (still.map((beat) => beat.id).join() !== ids.join()) fail('reduced beat order changed');

const fullTime = totalDuration(full);
const stillTime = totalDuration(still);
if (fullTime < 40 || fullTime > 90) fail(`full duration ${fullTime}`);
if (stillTime > fullTime + 0.01) fail(`reduced run is longer (${stillTime} > ${fullTime})`);
if (Math.abs(handoffStart(full) - (fullTime - full[full.length - 1].dur)) > 0.001) {
  fail('handoff start is not the last beat');
}

const script = storyScript();
const page = fs.readFileSync(new URL('../index.html', import.meta.url), 'utf8');
for (const line of [STORY.kicker, STORY.title, STORY.tag, STORY.parts]) {
  if (!page.includes(line)) fail(`title dropped: ${line}`);
}
if (page.includes('id="intro"') || page.includes('id="open-photo"') || page.includes('id="go-inside"')) {
  fail('old slideshow markup is still in the page');
}

for (const line of [
  STORY.tag,
  STORY.chris,
  STORY.void,
  STORY.doNot,
  STORY.img,
  STORY.dont,
  STORY.looked,
  STORY.wake,
  STORY.house,
  STORY.garage,
  STORY.smile,
  STORY.same,
]) {
  if (!script.includes(line)) fail(`missing story line: ${line}`);
}
if (!STORY.title.includes('\u2019')) fail('title apostrophe changed');
if (!STORY.wake.includes('\u2019') || !STORY.house.includes('\u2019') || !STORY.garage.includes('\u2019')) {
  fail('arrival apostrophes changed');
}
if (!STORY.dont.includes('\u2019') || !STORY.looked.includes('\u2019')) fail('message apostrophes changed');

const quietVoid = still.find((beat) => beat.id === 'void');
let quietStrength = null;
quietVoid.cues[0].play({ voidHit(value) { quietStrength = value; } }, true);
if (quietStrength !== 0.4) fail(`quiet void hit ${quietStrength}`);
const loudVoid = full.find((beat) => beat.id === 'void');
let loudStrength = null;
loudVoid.cues[0].play({ voidHit(value) { loudStrength = value; } }, false);
if (loudStrength !== 1) fail(`loud void hit ${loudStrength}`);
if (storyScript().includes(STORY.smile) && still.find((beat) => beat.id === 'void').aside.text !== STORY.smile) {
  fail('smile aside missing from the quiet script source');
}

if (activeBeat(full, 0).beat.id !== 'drive') fail('clock 0 is not the drive');
if (activeBeat(full, fullTime + 4).beat.id !== 'handoff') fail('past the end is not the handoff');

const phone = full.find((beat) => beat.id === 'phone');
if (linesAt(phone, 0).length !== 0) fail('phone speaks before its first mark');
const revealed = [];
for (const line of phone.lines) {
  const shown = linesAt(phone, line.at * phone.dur + 0.01);
  if (shown[shown.length - 1] !== line.text) fail(`phone line late: ${line.text}`);
  if (shown.length !== revealed.length + 1) fail('phone lines skipped or repeated');
  revealed.push(line.text);
}

if (bootOpacity(0) !== 1) fail('boot starts opaque');
if (bootOpacity(0.3) !== 1) fail('boot fades too early');
if (bootOpacity(1) !== 0) fail('boot does not finish clear');

let worst = 0;
for (let seed = 0; seed < 8; seed += 1) {
  const samples = [];
  for (let t = 0; t < 8; t += 0.02) samples.push(flickerLevel(t, seed));
  for (let start = 0; start < samples.length; start += 1) {
    const end = Math.min(samples.length, start + 50);
    let rises = 0;
    for (let i = start + 1; i < end; i += 1) {
      if (samples[i - 1] < 0.5 && samples[i] >= 0.5) rises += 1;
    }
    if (rises > worst) worst = rises;
  }
}
if (worst > 3) fail(`flicker crosses 0.5 ${worst} times in one second`);

const mutated = sequenceBeats(false);
mutated[0].dur = 1;
const fresh = sequenceBeats(false);
if (fresh[0].dur === 1) fail('beat durations are shared across calls');

const audio = createAudio();
for (const name of [
  'unlock', 'cinematic', 'fadeCinematic', 'endCinematic', 'hasCinematic',
  'duckGame', 'phoneBuzz', 'voidHit', 'engineDie', 'bulbTick', 'rise', 'mix',
]) {
  if (typeof audio[name] !== 'function') fail(`audio.${name} missing`);
}
audio.cinematic('title');
audio.phoneBuzz();
audio.rise(true);
audio.voidHit(0.4);
audio.engineDie();
audio.bulbTick();
audio.fadeCinematic(0.05);
audio.endCinematic(0.01);
audio.duckGame();
audio.hasCinematic();
audio.cinematic('off');

if (problems.length) {
  console.error(problems.join('\n'));
  process.exit(1);
}
console.log(`intro ok (${ids.length} beats, ${fullTime.toFixed(1)}s)`);
