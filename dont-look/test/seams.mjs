import { createAudio } from '../js/audio.mjs';
import { Game } from '../js/game.mjs';
import { SPAWN, TILE } from '../js/world.mjs';

const problems = [];
const fail = (msg) => problems.push(msg);

function mockCanvas() {
  const ctx = new Proxy({}, {
    get(_target, prop) {
      if (prop === 'canvas') return { width: 416, height: 224 };
      if (typeof prop !== 'string') return undefined;
      return () => ctx;
    },
  });
  return {
    width: 416,
    height: 224,
    getContext: () => ctx,
  };
}

function mockAudio() {
  const calls = [];
  const audio = new Proxy({}, {
    get(_target, prop) {
      if (typeof prop !== 'string') return undefined;
      return () => { calls.push(prop); };
    },
  });
  return { audio, calls };
}

const { audio, calls } = mockAudio();
const game = new Game(mockCanvas(), audio);
game.newGame();
if (!calls.includes('settle')) fail('a new game left the previous bus ducked');
if (game.director.mode !== 'idle') fail('a new game kept the last scare');

const spawned = game.state.player.x;
game.holdInput = true;
game.input.x = 1;
game.input.y = 0;
for (let i = 0; i < 20; i += 1) game.update(0.05);
if (game.state.player.x !== spawned) fail('the house took input before the intro finished');

game.holdInput = false;
game.state.player.x = Number.NaN;
game.state.player.y = Number.NaN;
game.update(0.016);
if (!Number.isFinite(game.state.player.x) || !Number.isFinite(game.state.player.y)) {
  fail('a broken position stayed NaN');
}
if (game.state.player.x !== (SPAWN.x + 0.5) * TILE) fail('NaN position did not return to the mudroom');

game.newGame();
const door = game.state.doors.hall;
door.open = true;
door.angle = 0.4;
door.pendingSlam = true;
game.state.screen = { type: 'pause' };
for (let i = 0; i < 12; i += 1) game.update(0.05);
if (!door.pendingSlam || door.angle !== 0.4) fail('a paused door kept swinging');
if (calls.includes('slam')) fail('a paused door slammed');
game.scare('seam-pause', { buildup: 0.8, silence: 0.2, aftermath: 0.4 });
for (let i = 0; i < 20; i += 1) game.update(0.05);
if (game.director.mode !== 'buildup') fail('a scare kept playing behind the pause menu');

game.state.screen = null;
game.update(0.05);
if (door.pendingSlam) fail('the door never finished after pause');
if (!calls.includes('slam')) fail('the door slam was lost');

let leftBuild = false;
for (let i = 0; i < 40; i += 1) {
  game.update(0.05);
  if (game.director.mode !== 'buildup') leftBuild = true;
}
if (!leftBuild) fail('the scare never resumed');

game.newGame();
game.die();
if (game.director.mode !== 'buildup') fail('death did not start its scare');
for (let i = 0; i < 16; i += 1) game.update(0.05);
if (game.director.mode === 'buildup') fail('the death scare froze on the end card');

const live = createAudio();
live.settle();
live.endCinematic(0.05);

if (problems.length) {
  console.error(problems.join('\n'));
  process.exit(1);
}
console.log('seams ok — pause holds, handoff ignores input, death scare still plays');
