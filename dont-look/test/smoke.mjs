const ctx = new Proxy({}, {
  get: () => () => {},
  set: () => true,
});
globalThis.Image = class {
  set src(_) {}
};
globalThis.document = {
  createElement() {
    return { width: 64, height: 64, getContext: () => ctx };
  },
};
globalThis.window = { matchMedia: () => ({ matches: false }) };

const { Game } = await import('../js/game.mjs');

const audio = new Proxy({}, { get: () => () => {} });
const canvas = { getContext: () => ctx };
const game = new Game(canvas, audio);
game.newGame();
game.input.x = -1;
for (let i = 0; i < 180; i += 1) {
  game.update(1 / 60);
  game.draw();
}
if (!game.state || game.state.player.hp <= 0) {
  console.error('player did not survive the smoke walk');
  process.exit(1);
}
if (!Number.isFinite(game.state.player.x) || !Number.isFinite(game.state.player.y)) {
  console.error('player position is not finite');
  process.exit(1);
}
game.attack();
game.dodge();
game.toggleLight();
game.update(1 / 30);
console.log('smoke ok', game.state.room, Math.round(game.state.player.x));
