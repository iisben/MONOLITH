// Procedural materials. A dropped-in PNG at assets/tiles/<name>.png replaces a material.
const ready = new Map();
const pending = new Map();

const PALETTE = {
  wood: ['#6b4e3a', '#5a4030', '#7d5c44', '#4e3828'],
  carpet: ['#4c565e', '#404a50', '#5a646c', '#363f45'],
  tile: ['#6a747b', '#586268', '#7c868e', '#4c555b'],
  concrete: ['#5a564f', '#48443e', '#6c675f', '#3a3632'],
  grass: ['#3a4e30', '#2c3c22', '#4a6238', '#24321c'],
  wall: ['#6a5a4c', '#56483e', '#3e342e', '#7a6858'],
};

function hash(x, y) {
  let n = (x * 374761393 + y * 668265263) | 0;
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return (n ^ (n >>> 16)) >>> 0;
}

function paint(name) {
  const canvas = document.createElement('canvas');
  canvas.width = 64;
  canvas.height = 64;
  const ctx = canvas.getContext('2d');
  const row = PALETTE[name] || PALETTE.concrete;
  if (name === 'wood') {
    for (let y = 0; y < 64; y += 8) {
      ctx.fillStyle = row[(y / 8) % row.length];
      ctx.fillRect(0, y, 64, 8);
      ctx.fillStyle = 'rgba(0,0,0,0.28)';
      ctx.fillRect(0, y + 7, 64, 1);
    }
  } else if (name === 'tile') {
    for (let y = 0; y < 64; y += 32) {
      for (let x = 0; x < 64; x += 32) {
        ctx.fillStyle = row[((x + y) / 32) % row.length];
        ctx.fillRect(x, y, 32, 32);
        ctx.strokeStyle = '#242a2e';
        ctx.strokeRect(x + 1, y + 1, 30, 30);
      }
    }
  } else if (name === 'wall') {
    ctx.fillStyle = row[1];
    ctx.fillRect(0, 0, 64, 64);
    for (let y = 0; y < 64; y += 8) {
      const shift = (y / 8) % 2 ? 8 : 0;
      ctx.fillStyle = row[0];
      ctx.fillRect(0, y, 64, 7);
      ctx.fillStyle = '#1a1512';
      ctx.fillRect(0, y + 7, 64, 1);
      for (let x = shift; x < 64; x += 16) ctx.fillRect(x, y, 1, 7);
    }
  } else if (name === 'grass') {
    ctx.fillStyle = row[1];
    ctx.fillRect(0, 0, 64, 64);
    for (let i = 0; i < 36; i += 1) {
      ctx.fillStyle = row[i % row.length];
      ctx.fillRect(hash(i, 1) % 64, hash(i, 3) % 64, 2, 5);
    }
  } else {
    for (let y = 0; y < 64; y += 4) {
      for (let x = 0; x < 64; x += 4) {
        ctx.fillStyle = row[hash(x, y) % row.length];
        ctx.fillRect(x, y, 4, 4);
      }
    }
  }
  return canvas;
}

export function material(name) {
  if (ready.has(name)) return ready.get(name);
  const painted = paint(name);
  ready.set(name, painted);
  if (!pending.has(name)) {
    pending.set(name, true);
    const img = new Image();
    img.onload = () => ready.set(name, img);
    img.onerror = () => {};
    img.src = new URL(`../assets/tiles/${name}.png`, import.meta.url).href;
  }
  return painted;
}
