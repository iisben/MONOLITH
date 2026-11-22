// Face crops are fractions of the source photo: x, y, w, h.
export const CAST = {
  chris: {
    src: '../assets/chris.jpg',
    face: { x: 0.28, y: 0.0, w: 0.44, h: 0.2 },
    bust: { x: 0.14, y: 0.0, w: 0.72, h: 0.48 },
  },
  her: {
    src: '../assets/her.jpg',
    face: { x: 0.06, y: 0.0, w: 0.88, h: 0.24 },
    bust: { x: 0.0, y: 0.0, w: 1, h: 0.46 },
  },
  mom: {
    src: '../assets/mom.jpg',
    face: { x: 0.14, y: 0.0, w: 0.7, h: 0.24 },
    bust: { x: 0.04, y: 0.0, w: 0.92, h: 0.42 },
  },
  ben: {
    src: '../assets/ben.jpg',
    face: { x: 0.14, y: 0.1, w: 0.72, h: 0.38 },
    bust: { x: 0.04, y: 0.06, w: 0.92, h: 0.55 },
  },
  basement: {
    src: '../assets/basement.jpg',
    face: { x: 0.14, y: 0.06, w: 0.72, h: 0.72 },
    bust: { x: 0.04, y: 0.04, w: 0.92, h: 0.72 },
  },
  grin: {
    src: '../assets/grin.jpg',
    face: { x: 0.04, y: 0.0, w: 0.92, h: 0.82 },
    bust: { x: 0.0, y: 0.0, w: 1, h: 0.92 },
  },
  teammate: {
    src: '../assets/teammate.jpg',
    face: { x: 0.18, y: 0.24, w: 0.64, h: 0.42 },
    bust: { x: 0.04, y: 0.14, w: 0.92, h: 0.55 },
  },
};

const images = {};
const cache = new Map();
const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];

export async function loadCast() {
  await Promise.all(Object.entries(CAST).map(async ([id, def]) => {
    const img = new Image();
    img.decoding = 'async';
    img.src = new URL(def.src, import.meta.url).href;
    try {
      await img.decode();
      images[id] = img;
    } catch {
      images[id] = null;
    }
  }));
}

export function ready(id) {
  return Boolean(images[id]);
}

function quant(value, threshold) {
  const jittered = value + (threshold - 7.5) * 2.4;
  return Math.max(0, Math.min(248, Math.round(jittered / 8) * 8));
}

export function portrait(id, { crop = 'face', size = 96, corrupt = 0, grin = 0, dark = 0 } = {}) {
  const key = [id, crop, size, corrupt.toFixed(2), grin.toFixed(2), dark.toFixed(2), images[id] ? 1 : 0].join('|');
  const hit = cache.get(key);
  if (hit) return hit;
  const def = CAST[id] || CAST.grin;
  const rect = def[crop] || def.face;
  const img = images[id];
  const aspect = img && rect.w > 0 ? (rect.h * img.height) / (rect.w * img.width) : rect.h / rect.w;
  const w = size;
  const h = Math.max(8, Math.round(size * aspect));
  const canvas = document.createElement('canvas');
  canvas.width = w;
  canvas.height = h;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  if (!img) {
    ctx.fillStyle = '#241c18';
    ctx.fillRect(0, 0, w, h);
    cache.set(key, canvas);
    return canvas;
  }
  const sx = rect.x * img.width;
  const sy = rect.y * img.height;
  const sw = rect.w * img.width;
  const sh = rect.h * img.height;
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(img, sx, sy, sw, sh, 0, 0, w, h);
  if (grin > 0 && images.grin) {
    ctx.globalAlpha = grin;
    ctx.drawImage(images.grin, 0, 0, w, h);
    ctx.globalAlpha = 1;
  }
  const imageData = ctx.getImageData(0, 0, w, h);
  const data = imageData.data;
  const copy = new Uint8ClampedArray(data);
  for (let y = 0; y < h; y++) {
    const rowShift = corrupt > 0 && y % 3 === 0 ? Math.round((y % 5) - 2) : 0;
    for (let x = 0; x < w; x++) {
      const sxPx = Math.max(0, Math.min(w - 1, x + rowShift));
      const si = (y * w + sxPx) * 4;
      const di = (y * w + x) * 4;
      const t = BAYER[(y & 3) * 4 + (x & 3)];
      const shade = 1 - dark;
      data[di] = quant(copy[si] * shade, t);
      data[di + 1] = quant(copy[si + 1] * shade, t);
      data[di + 2] = quant(copy[si + 2] * shade * (corrupt > 0.5 ? 0.85 : 1), t);
      data[di + 3] = 255;
    }
  }
  ctx.putImageData(imageData, 0, 0);
  canvas.className = 'portrait';
  cache.set(key, canvas);
  return canvas;
}
