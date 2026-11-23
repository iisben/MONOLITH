// Boot-baked sprites. Each id is its own drawing. A PNG dropped on a floor
// material still shows underneath the wear, via tex.mjs.
import { material } from './tex.mjs';

const cache = new Map();

function hash(x, y = 0) {
  let n = Math.imul((x | 0) ^ 0x9e3779b9, 0x85ebca6b) ^ Math.imul((y | 0) ^ 0xc2b2ae35, 0x27d4eb2f);
  n = Math.imul(n ^ (n >>> 15), 0x165667b1);
  return (n ^ (n >>> 16)) >>> 0;
}

function bake(id, w, h, draw) {
  if (cache.has(id)) return cache.get(id);
  const canvas = document.createElement('canvas');
  canvas.width = w;
  canvas.height = h;
  const g = canvas.getContext('2d');
  if (g) draw(g, w, h);
  cache.set(id, canvas);
  return canvas;
}

function px(g, x, y, color, w = 1, h = 1) {
  if (!color) return;
  g.fillStyle = color;
  g.fillRect(x | 0, y | 0, w, h);
}

function under(g, name, w, h) {
  try {
    const img = material(name);
    if (img) g.drawImage(img, 0, 0, w, h);
  } catch { /* the baked wear still stands */ }
}

export function floorStyle(name, room) {
  if (name === 'carpet') {
    if (room === 'mom') return 'carpet-mom';
    if (room === 'chris') return 'carpet-chris';
    return 'carpet-her';
  }
  if (name === 'tile') {
    if (room === 'bath') return 'tile-bath';
    if (room === 'mud') return 'tile-mud';
    return 'tile-kitchen';
  }
  if (name === 'concrete') return room === 'basement' ? 'concrete-base' : 'concrete-garage';
  if (name === 'grass') return 'grass';
  if (room === 'shed') return 'wood-shed';
  if (room === 'hallN' || room === 'hallS') return 'wood-hall';
  return 'wood-main';
}

export function floorSprite(style, variant) {
  const v = variant & 3;
  return bake(`floor-${style}-${v}`, 64, 64, (g, w, h) => {
    if (style.startsWith('wood')) woodFloor(g, w, h, style, v);
    else if (style.startsWith('carpet')) carpetFloor(g, w, h, style, v);
    else if (style.startsWith('tile')) tileFloor(g, w, h, style, v);
    else if (style === 'grass') grassFloor(g, w, h, v);
    else concreteFloor(g, w, h, style, v);
  });
}

function woodFloor(g, w, h, style, v) {
  under(g, 'wood', w, h);
  const board = style === 'wood-hall' ? 5 : style === 'wood-shed' ? 10 : 8;
  const rows = style === 'wood-shed'
    ? ['#5a4634', '#4a3828', '#6a5340', '#3e3024']
    : style === 'wood-hall'
      ? ['#6e543c', '#5c4632', '#7a6048', '#4e3c2c']
      : ['#6b4b34', '#5a3e2c', '#7a5840', '#4a3426'];
  for (let y = 0; y < h; y += board) {
    px(g, 0, y, rows[(y / board + v) % rows.length], w, board - 1);
    px(g, 0, y + board - 1, '#1c120c', w, 1);
    px(g, 0, y + 1, 'rgba(255,220,180,0.07)', w, 1);
    const knot = (hash(y, v + 4) % (w - 10)) + 2;
    px(g, knot, y + 2, '#3a2618', 5, 3);
    px(g, knot + 1, y + 3, '#8d6a48', 2, 1);
    px(g, hash(v, y) % w, y + board - 2, '#c4a574', 1, 1);
  }
  if (style === 'wood-shed') {
    px(g, 8 + v * 6, 20, '#3a3420', 18, 6);
    px(g, 4, 48, '#6a7a48', 3, 2);
    px(g, 40, 12, '#6a7a48', 2, 2);
  } else if (style === 'wood-hall') {
    px(g, 18, 0, 'rgba(40,28,18,0.35)', 28, h);
    px(g, 22 + v, 8, 'rgba(0,0,0,0.25)', 2, 48);
  } else if (v === 1) px(g, 16, 22, 'rgba(30,18,10,0.4)', 20, 10);
  else if (v === 2) px(g, 6, 40, 'rgba(210,180,120,0.18)', 30, 2);
  px(g, 0, h - 2, 'rgba(0,0,0,0.35)', w, 2);
}

function carpetFloor(g, w, h, style, v) {
  under(g, 'carpet', w, h);
  if (style === 'carpet-her') {
    px(g, 0, 0, '#3e4a46', w, h);
    for (let y = 0; y < h; y += 2) px(g, 0, y, y % 4 ? '#44524c' : '#36423e', w, 1);
    for (let i = 0; i < 6; i += 1) {
      const x = (hash(i, v) % 7) * 8 + 2;
      const y = (hash(v, i) % 7) * 8 + 2;
      px(g, x, y, '#6e8a78', 3, 3);
      px(g, x + 1, y + 1, '#d7e4d4', 1, 1);
    }
    px(g, 24, 30, 'rgba(20,10,10,0.45)', 14, 9);
  } else if (style === 'carpet-mom') {
    px(g, 0, 0, '#4a403c', w, h);
    for (let y = 0; y < h; y += 3) px(g, 0, y, '#564840', w, 1);
    px(g, 22, 0, '#3a3028', 18, h);
    px(g, 24, 0, 'rgba(255,255,255,0.04)', 4, h);
    px(g, 8 + v * 4, 18, 'rgba(80,40,36,0.35)', 10, 16);
  } else {
    px(g, 0, 0, '#4a4034', w, h);
    for (let i = 0; i < 18; i += 1) {
      px(g, hash(i, v) % w, hash(v + 2, i) % h, i % 2 ? '#5c4c38' : '#3a3228', 3, 2);
    }
    px(g, 6, 14, '#c4a06a', 10, 2);
    px(g, 8, 14, '#1c2744', 6, 2);
    px(g, 40, 36, 'rgba(0,0,0,0.28)', 12, 8);
  }
  px(g, 0, h - 2, 'rgba(0,0,0,0.4)', w, 2);
}

function tileFloor(g, w, h, style, v) {
  under(g, 'tile', w, h);
  const size = style === 'tile-bath' ? 16 : style === 'tile-kitchen' ? 32 : 21;
  const base = style === 'tile-bath' ? '#8a9294' : style === 'tile-kitchen' ? '#6a7074' : '#5c584e';
  const alt = style === 'tile-bath' ? '#9aa4a4' : style === 'tile-kitchen' ? '#7c8286' : '#6a6456';
  px(g, 0, 0, '#2a2c2e', w, h);
  for (let y = 0; y < h; y += size) {
    for (let x = 0; x < w; x += size) {
      const crack = hash(x + v * 3, y) % 11 === 0;
      px(g, x + 1, y + 1, ((x + y) / size + v) % 2 ? alt : base, size - 2, size - 2);
      px(g, x + 2, y + 2, 'rgba(255,255,255,0.14)', size - 6, 1);
      if (crack) px(g, x + 4, y + 3, '#2a2420', 1, size - 6);
    }
  }
  if (style === 'tile-bath') px(g, 20, 40, 'rgba(180,200,210,0.35)', 16, 8);
  if (style === 'tile-kitchen') px(g, 8, 8, 'rgba(40,30,10,0.35)', 18, 6);
  if (style === 'tile-mud') {
    px(g, 30, 34, '#2a241c', 8, 4);
    px(g, 34, 38, '#2a241c', 6, 3);
  }
}

function concreteFloor(g, w, h, style, v) {
  under(g, 'concrete', w, h);
  const damp = style === 'concrete-base';
  px(g, 0, 0, damp ? '#3a3e40' : '#6a655c', w, h);
  for (let i = 0; i < 28; i += 1) {
    px(g, hash(i, v) % w, hash(v, i) % h, damp ? '#2e3436' : '#7a756c', 3, 2);
  }
  px(g, 4, 20 + v, 'rgba(255,255,255,0.05)', w - 8, 1);
  if (damp) {
    px(g, 10, 12, 'rgba(20,40,36,0.55)', 22, 14);
    px(g, 18, 8, 'rgba(140,160,150,0.2)', 8, 20);
  } else {
    px(g, 28, 30, 'rgba(20,16,10,0.55)', 24, 16);
    px(g, 8, 8, '#4a4030', 6, 4);
  }
  px(g, 0, h - 3, 'rgba(0,0,0,0.4)', w, 3);
}

function grassFloor(g, w, h, v) {
  under(g, 'grass', w, h);
  px(g, 0, 0, '#2a3a22', w, h);
  for (let i = 0; i < 50; i += 1) {
    const x = hash(i, v) % w;
    const y = hash(v + 5, i) % h;
    px(g, x, y, i % 3 ? '#3e5630' : '#1e2c16', 1, 4);
    if (i % 5 === 0) px(g, x, y, '#8a9a58', 1, 2);
  }
  if (v === 0) px(g, 16, 28, '#4a3828', 14, 8);
  if (v === 2) px(g, 36, 10, '#6a5a38', 8, 3);
  px(g, 4 + v, 50, '#24301c', 10, 4);
}

export function wallStyleAt(neighborRooms) {
  if (neighborRooms.includes('basement')) return 'block';
  if (neighborRooms.includes('yard') || neighborRooms.includes('shed')) return 'brick';
  if (neighborRooms.includes('garage')) return 'cinder';
  return 'plaster';
}

export function wallSprite(style, variant) {
  const v = variant & 3;
  return bake(`wall-${style}-${v}`, 32, 32, (g, w, h) => {
    if (style === 'brick') {
      px(g, 0, 0, '#5a4034', w, h);
      for (let y = 0; y < h; y += 8) {
        const shift = (y / 8) % 2 ? 8 : 0;
        px(g, 0, y, '#6e5040', w, 7);
        px(g, 0, y + 7, '#2a1c16', w, 1);
        for (let x = shift; x < w; x += 16) px(g, x, y, '#2a1c16', 1, 7);
        if (hash(y, v) % 4 === 0) px(g, shift + 4, y + 2, '#4a3428', 6, 2);
      }
    } else if (style === 'block' || style === 'cinder') {
      const face = style === 'block' ? '#3a4044' : '#6a6860';
      const mortar = style === 'block' ? '#1c2022' : '#3a3832';
      px(g, 0, 0, mortar, w, h);
      for (let y = 0; y < h; y += 10) {
        px(g, 1, y + 1, face, 14, 8);
        px(g, 17, y + 1, face, 14, 8);
        px(g, 2, y + 2, 'rgba(255,255,255,0.06)', 8, 1);
        if (style === 'block' && hash(y, v) % 3 === 0) px(g, 4, y + 4, 'rgba(20,40,36,0.45)', 8, 3);
      }
    } else {
      px(g, 0, 0, '#5c5148', w, h);
      for (let i = 0; i < 16; i += 1) px(g, hash(i, v) % w, hash(v, i) % h, i % 2 ? '#675c52' : '#4e453e', 2, 2);
      px(g, 4 + v * 3, 6, '#3a322c', 1, 14 + v);
      px(g, 18, 20, '#6a6056', 6, 1);
      px(g, 0, 28, '#3a322c', w, 4);
      px(g, 0, 28, '#8a7a68', w, 1);
    }
    px(g, 0, 0, 'rgba(255,255,255,0.05)', w, 1);
    px(g, 0, h - 2, 'rgba(0,0,0,0.45)', w, 2);
  });
}

export function trimSprite() {
  return bake('trim', 32, 6, (g, w) => {
    px(g, 0, 0, '#c4a07a', w, 2);
    px(g, 0, 2, '#8a6848', w, 2);
    px(g, 0, 4, '#2a1c14', w, 2);
    px(g, 3, 1, 'rgba(255,255,255,0.25)', 4, 1);
  });
}

export function doorSprite(id, locked) {
  const kind = id === 'yard' ? 'garage' : id === 'base' ? 'slab' : id === 'shed' ? 'shed' : 'panel';
  return bake(`door-${kind}-${locked ? 'lock' : 'shut'}`, 32, 32, (g, w, h) => {
    if (kind === 'garage') {
      px(g, 2, 2, '#4a4e52', w - 4, h - 4);
      for (let y = 4; y < h - 2; y += 6) {
        px(g, 3, y, '#6a7074', w - 6, 4);
        px(g, 3, y + 4, '#2a2e30', w - 6, 1);
      }
      px(g, 8, 14, '#1a1c1e', 16, 3);
    } else if (kind === 'slab') {
      px(g, 4, 2, '#2e3438', w - 8, h - 4);
      px(g, 6, 4, '#3e4648', w - 12, h - 8);
      for (let i = 0; i < 4; i += 1) px(g, 8, 6 + i * 6, '#8a9090', 2, 2);
      px(g, w - 12, 15, '#1a1c1e', 5, 4);
    } else if (kind === 'shed') {
      px(g, 6, 2, '#5a4634', 20, 28);
      for (let x = 8; x < 24; x += 4) px(g, x, 3, '#3a2c20', 1, 26);
      px(g, 8, 4, '#6a5844', 16, 1);
      px(g, 20, 16, '#c4a06a', 3, 2);
    } else {
      px(g, 5, 2, '#5a4030', 22, 28);
      px(g, 8, 4, '#3a281c', 16, 10);
      px(g, 8, 16, '#3a281c', 16, 10);
      px(g, 9, 5, '#6a5040', 14, 8);
      px(g, 9, 17, '#6a5040', 14, 8);
      px(g, 6, 3, '#c4a574', 20, 1);
    }
    if (locked) {
      px(g, 18, 14, '#8a4038', 6, 6);
      px(g, 20, 16, '#1a0c0c', 2, 2);
    } else if (kind !== 'garage') {
      px(g, 20, 16, '#e4c07a', 3, 3);
      px(g, 21, 15, '#fff4d2', 1, 1);
    }
  });
}

export function itemSprite(id) {
  return bake(`item-${id}`, 24, 24, (g) => {
    const painters = {
      stick: drawStick,
      knife: drawKnife,
      pistol: drawPistol,
      ammo: drawAmmo,
      bandage: drawBandage,
      pills: drawPills,
      film: drawFilm,
      tape: drawTape,
      'key-her': (ctx) => drawKey(ctx, '#e0b84e', 0),
      'key-base': (ctx) => drawKey(ctx, '#b08a55', 1),
      'key-shed': (ctx) => drawKey(ctx, '#c4b48a', 2),
      'key-car': drawCarKeys,
      fuse: drawFuse,
      gas: drawGas,
      camera: drawCamera,
      flashlight: drawFlashlight,
      battery: drawBattery,
      note: drawNote,
    };
    (painters[id] || drawNote)(g);
  });
}

function drawStick(g) {
  px(g, 16, 2, '#c4a06a', 3, 16);
  px(g, 15, 4, '#8a6840', 1, 12);
  px(g, 10, 16, '#d7d0c4', 12, 3);
  px(g, 11, 17, '#1c2744', 8, 1);
  px(g, 8, 18, '#2a241c', 14, 2);
}
function drawKnife(g) {
  px(g, 14, 3, '#d5dbe6', 3, 12);
  px(g, 16, 3, '#ffffff', 1, 8);
  px(g, 15, 14, '#1a1c1e', 1, 2);
  px(g, 12, 15, '#5a4030', 7, 4);
  px(g, 13, 16, '#3a281c', 5, 1);
  px(g, 18, 6, '#9aa0a4', 1, 4);
}
function drawPistol(g) {
  px(g, 4, 10, '#1c1e22', 14, 4);
  px(g, 16, 9, '#2a2e32', 5, 3);
  px(g, 6, 14, '#3a2c22', 4, 6);
  px(g, 7, 15, '#5a4030', 2, 4);
  px(g, 10, 8, '#8a9094', 3, 2);
  px(g, 8, 11, '#c4a574', 1, 1);
}
function drawAmmo(g) {
  for (let i = 0; i < 3; i += 1) {
    px(g, 6 + i * 4, 8, '#c4a05a', 3, 8);
    px(g, 6 + i * 4, 7, '#e8d090', 3, 2);
    px(g, 7 + i * 4, 15, '#8a7038', 1, 2);
  }
}
function drawBandage(g) {
  px(g, 6, 8, '#e6e0d4', 12, 8);
  px(g, 8, 6, '#d7d0c2', 8, 12);
  px(g, 11, 4, '#b7a898', 2, 16);
  px(g, 10, 11, '#c45048', 4, 2);
}
function drawPills(g) {
  px(g, 8, 4, '#d7e4ea', 8, 3);
  px(g, 7, 7, '#f2f4f0', 10, 12);
  px(g, 8, 9, '#1c2744', 8, 1);
  px(g, 8, 12, '#8f3a3a', 8, 1);
  px(g, 9, 15, '#c4a574', 6, 1);
}
function drawFilm(g) {
  px(g, 5, 6, '#2a241c', 14, 12);
  px(g, 7, 8, '#0c0c0e', 10, 8);
  px(g, 4, 8, '#1a1814', 2, 2);
  px(g, 4, 14, '#1a1814', 2, 2);
  px(g, 18, 8, '#1a1814', 2, 2);
  px(g, 9, 10, '#6a5848', 3, 4);
}
function drawTape(g) {
  px(g, 6, 6, '#c4b48a', 12, 12);
  px(g, 9, 9, '#5a4030', 6, 6);
  px(g, 11, 11, '#2a2018', 2, 2);
  px(g, 4, 10, '#d7d0c4', 3, 2);
}
function drawKey(g, metal, kind) {
  px(g, 4, 8, metal, 8, 8);
  px(g, 6, 10, '#1a140e', 4, 4);
  px(g, 11, 11, metal, kind === 1 ? 9 : 7, 2);
  px(g, 16, 13, metal, 2, kind === 2 ? 3 : 4);
  if (kind === 2) {
    px(g, 3, 6, '#8a8070', 4, 2);
    px(g, 2, 7, '#8a8070', 2, 6);
  }
  px(g, 5, 9, '#fff4d2', 2, 1);
}
function drawCarKeys(g) {
  drawKey(g, '#d7d0c4', 0);
  px(g, 8, 4, '#8a4038', 6, 4);
  px(g, 14, 16, '#c4a574', 6, 2);
  px(g, 18, 14, '#c4a574', 2, 3);
}
function drawFuse(g) {
  px(g, 6, 9, '#e6e0d4', 12, 6);
  px(g, 4, 10, '#8a9094', 3, 4);
  px(g, 17, 10, '#8a9094', 3, 4);
  px(g, 9, 11, '#c45048', 6, 2);
  px(g, 8, 8, 'rgba(255,255,255,0.4)', 4, 1);
}
function drawGas(g) {
  px(g, 7, 6, '#c45048', 10, 14);
  px(g, 8, 4, '#8a3030', 6, 3);
  px(g, 14, 3, '#2a2e30', 6, 2);
  px(g, 16, 5, '#2a2e30', 2, 4);
  px(g, 9, 8, '#e6e0d4', 6, 4);
  px(g, 9, 14, '#1a140e', 6, 2);
}
function drawCamera(g) {
  px(g, 4, 8, '#2a2e32', 16, 10);
  px(g, 8, 10, '#0e1012', 6, 6);
  px(g, 10, 12, '#9aafbc', 2, 2);
  px(g, 16, 6, '#e6e0d4', 4, 4);
  px(g, 6, 7, '#1a1c1e', 4, 2);
  px(g, 5, 16, '#5a4030', 12, 2);
}
function drawFlashlight(g) {
  px(g, 3, 10, '#c4a574', 14, 5);
  px(g, 16, 9, '#e6d2a4', 5, 7);
  px(g, 17, 11, '#fff4d2', 2, 3);
  px(g, 6, 11, '#2a241c', 2, 3);
  px(g, 8, 9, '#8a6848', 8, 1);
}
function drawBattery(g) {
  px(g, 6, 6, '#3a6a32', 5, 12);
  px(g, 13, 6, '#3a6a32', 5, 12);
  px(g, 7, 4, '#d7d0c4', 3, 3);
  px(g, 14, 4, '#d7d0c4', 3, 3);
  px(g, 7, 10, '#e6e0d4', 3, 1);
  px(g, 14, 12, '#1a140e', 3, 1);
}
function drawNote(g) {
  px(g, 6, 3, '#e6dcc8', 12, 16);
  px(g, 6, 3, '#d7cbb0', 12, 2);
  px(g, 8, 8, '#8a7b68', 8, 1);
  px(g, 8, 11, '#8a7b68', 7, 1);
  px(g, 8, 14, '#8a7b68', 6, 1);
  px(g, 15, 4, 'rgba(0,0,0,0.15)', 2, 14);
}

export function markSprite(prop) {
  if (prop.kind === 'flashlight') return itemSprite('flashlight');
  if (prop.kind === 'battery') return itemSprite('battery');
  if (prop.kind === 'note') return itemSprite('note');
  if (prop.kind === 'pickup' && prop.item) return itemSprite(prop.item);
  if (prop.kind === 'inspect') return bake(`look-${prop.inspect || prop.id}`, 24, 24, (g) => drawInspect(g, prop.inspect || prop.id));
  if (prop.kind === 'container') return bake(`box-${prop.id}`, 24, 24, (g) => drawContainer(g, prop.id));
  if (prop.kind === 'save') return bake('bench-mark', 24, 24, drawVise);
  if (prop.kind === 'stash') return bake('bin-mark', 24, 24, drawBin);
  if (prop.kind === 'board') return bake('board-mark', 24, 24, drawBoard);
  return itemSprite('note');
}

function drawInspect(g, id) {
  if (id === 'tv') {
    px(g, 3, 6, '#1a1c1e', 18, 12);
    px(g, 5, 8, '#3a4a44', 14, 8);
    px(g, 8, 18, '#5a4030', 8, 2);
    px(g, 10, 10, '#d7e4ea', 2, 2);
  } else if (id === 'mirror') {
    px(g, 6, 2, '#8a8078', 12, 18);
    px(g, 8, 4, '#9ab0b4', 8, 12);
    px(g, 9, 8, '#c45048', 6, 1);
    px(g, 5, 20, '#5a4030', 14, 2);
  } else if (id === 'tanks') {
    px(g, 4, 4, '#1e3a34', 6, 16);
    px(g, 14, 6, '#1e3a34', 6, 14);
    px(g, 8, 8, '#8a9090', 8, 2);
    px(g, 6, 6, '#9aafbc', 2, 2);
  } else if (id === 'cloth') {
    px(g, 6, 6, '#3aafb0', 12, 8);
    px(g, 8, 8, '#d7e4ea', 4, 3);
    px(g, 5, 12, '#2a8a88', 6, 4);
  } else if (id === 'front') {
    px(g, 4, 3, '#5a4030', 16, 18);
    px(g, 7, 6, '#3a281c', 10, 6);
    px(g, 14, 14, '#e4c07a', 2, 2);
  } else if (id === 'duffel' || id === 'bag') {
    px(g, 3, 8, '#3a342c', 18, 10);
    px(g, 6, 6, '#5a4030', 12, 3);
    px(g, 8, 11, '#c4a574', 8, 2);
  } else if (id === 'yard') {
    px(g, 2, 14, '#2a3a22', 20, 6);
    px(g, 6, 8, '#6a5a68', 12, 6);
    px(g, 4, 6, '#4a4038', 4, 3);
  } else {
    px(g, 4, 5, '#e6dcc8', 16, 14);
    px(g, 6, 7, '#8a8078', 12, 8);
    px(g, 8, 9, '#c4a574', 3, 3);
  }
}

function drawContainer(g, id) {
  if (id === 'bag') {
    drawInspect(g, 'duffel');
    return;
  }
  px(g, 4, 4, '#d7e4ea', 16, 16);
  px(g, 6, 6, '#9ab0b4', 12, 6);
  px(g, 10, 14, '#8a9094', 4, 2);
  px(g, 7, 8, '#3a4a44', 4, 2);
}

function drawVise(g) {
  px(g, 3, 14, '#5a4634', 18, 5);
  px(g, 6, 8, '#8a9094', 4, 8);
  px(g, 14, 8, '#8a9094', 4, 8);
  px(g, 8, 6, '#c4a574', 8, 3);
}
function drawBin(g) {
  px(g, 4, 8, '#4a4034', 16, 12);
  px(g, 6, 6, '#6a5844', 12, 3);
  px(g, 8, 11, '#c4a06a', 3, 4);
  px(g, 13, 12, '#8a9094', 4, 3);
}
function drawBoard(g) {
  px(g, 6, 3, '#d7cbb0', 12, 16);
  px(g, 8, 2, '#8a9094', 8, 3);
  px(g, 8, 8, '#5a4030', 8, 1);
  px(g, 8, 11, '#5a4030', 6, 1);
  px(g, 8, 14, '#3a6a32', 7, 1);
}

export function furnitureSprite(piece) {
  const w = piece.w * 32;
  const h = piece.h * 32;
  return bake(`furn-${piece.id}-${w}x${h}`, w, h, (g) => {
    const id = piece.id;
    if (id === 'her-bed') bed(g, w, h, '#3a3038', '#2a2428', '#6a5860');
    else if (id === 'chris-bed') bed(g, w, h, '#3a3428', '#1c2744', '#c4a06a');
    else if (id === 'mom-bed') sweaterBed(g, w, h);
    else if (piece.kind === 'tub') tub(g, w, h);
    else if (piece.kind === 'couch') couch(g, w, h);
    else if (piece.kind === 'table') table(g, w, h);
    else if (piece.kind === 'counter') counter(g, w, h);
    else if (piece.kind === 'car') car(g, w, h);
    else if (id === 'gbench') bench(g, w, h, true);
    else if (id === 'bbench') bench(g, w, h, false);
    else if (piece.kind === 'tanks') tanks(g, w, h);
    else if (piece.kind === 'shelf') shelf(g, w, h);
    else bed(g, w, h, '#3a3038', '#2a2428', '#6a5860');
  });
}

function bed(g, w, h, wood, cover, stripe) {
  px(g, 4, 4, wood, w - 8, 8);
  px(g, 6, 2, wood, 6, 6);
  px(g, 8, 12, cover, w - 16, h - 20);
  px(g, 10, 14, stripe, w - 22, 3);
  px(g, 12, 20, 'rgba(255,255,255,0.06)', w - 28, 4);
  px(g, 6, h - 8, wood, 4, 6);
  px(g, w - 10, h - 8, wood, 4, 6);
  px(g, 14, 8, '#e6dcc8', 16, 8);
  px(g, 16, 10, 'rgba(0,0,0,0.15)', 10, 2);
}
function sweaterBed(g, w, h) {
  bed(g, w, h, '#4a4038', '#3a3430', '#2a2624');
  px(g, 18, 16, '#6f9aab', w - 40, 18);
  px(g, 22, 18, '#d7e4ea', w - 52, 4);
  px(g, 26, 24, '#4a7a88', 10, 8);
  px(g, 20, 20, 'rgba(0,0,0,0.2)', 8, 10);
  px(g, w - 36, 18, '#8fb4c0', 6, 12);
}
function tub(g, w, h) {
  px(g, 8, h - 10, '#2a2e30', 6, 6);
  px(g, w - 14, h - 10, '#2a2e30', 6, 6);
  px(g, 6, 10, '#d7e4ea', w - 12, h - 18);
  px(g, 10, 14, '#b7c6c8', w - 20, h - 28);
  px(g, 14, 18, 'rgba(80,100,110,0.35)', w - 36, 8);
  px(g, w - 22, 6, '#8a9094', 4, 10);
  px(g, w - 26, 6, '#8a9094', 10, 3);
  px(g, 16, 16, '#9ab0b0', 4, 2);
}
function couch(g, w, h) {
  px(g, 2, 16, '#3a3028', 8, h - 24);
  px(g, w - 10, 16, '#3a3028', 8, h - 24);
  px(g, 8, 12, '#4a3c32', w - 16, h - 20);
  for (let i = 0; i < 3; i += 1) {
    const x = 12 + i * ((w - 28) / 3);
    px(g, x, 16, i === 1 ? '#3a322c' : '#5a4a3c', (w - 36) / 3, h - 32);
    px(g, x + 2, 18, 'rgba(255,255,255,0.05)', 6, 2);
  }
  px(g, 10, h - 8, '#2a2018', 6, 6);
  px(g, w - 16, h - 8, '#2a2018', 6, 6);
  px(g, 24, h - 14, 'rgba(0,0,0,0.25)', 18, 4);
}
function table(g, w, h) {
  px(g, 4, 8, '#5a4030', w - 8, 10);
  px(g, 6, 9, '#c4a574', w - 14, 2);
  px(g, 8, 18, '#3a281c', 4, h - 22);
  px(g, w - 12, 18, '#3a281c', 4, h - 22);
  px(g, 10, 12, '#e6dcc8', 10, 4);
  px(g, w - 28, 11, '#6a3030', 8, 5);
}
function counter(g, w, h) {
  px(g, 2, 0, '#5a5e60', w - 4, 6);
  px(g, 2, 6, '#3a3e42', w - 4, h - 8);
  for (let y = 10; y < h - 8; y += 24) {
    px(g, 4, y, '#2a2e32', w - 8, 20);
    px(g, w - 8, y + 8, '#c4a574', 2, 3);
  }
  px(g, 2, 4, 'rgba(255,255,255,0.15)', w - 4, 1);
}
function car(g, w, h) {
  px(g, 8, 18, '#2a2e32', w - 16, 8);
  px(g, 16, 22, '#1a1c1e', 16, 10);
  px(g, w - 36, 22, '#1a1c1e', 16, 10);
  px(g, 10, 26, '#14161a', 18, 12);
  px(g, w - 32, 26, '#14161a', 18, 12);
  px(g, 18, 14, '#8aa0a8', 28, 16);
  px(g, w * 0.5, 14, '#6a8088', 34, 16);
  px(g, 6, 20, '#e6d2a4', 6, 4);
  px(g, w - 12, 20, '#8a3030', 6, 4);
  px(g, 4, 24, '#3a3e42', w - 8, 14);
}
function bench(g, w, h, garage) {
  px(g, 2, 8, '#5a4634', w - 4, 8);
  px(g, 4, 9, '#c4a574', w - 10, 1);
  px(g, 4, 16, '#3a2c22', 4, h - 18);
  px(g, w - 8, 16, '#3a2c22', 4, h - 18);
  if (garage) {
    px(g, 8, 2, '#8a9094', 10, 8);
    px(g, 18, 4, '#c4a574', 6, 3);
    px(g, w - 20, 10, '#2a2e30', 8, 3);
  } else {
    px(g, 6, 4, '#6a3030', 8, 5);
    px(g, 18, 6, '#d7d0c4', 10, 3);
    px(g, w - 24, 5, '#1a1c1e', 12, 4);
  }
}
function tanks(g, w, h) {
  px(g, 6, 8, '#16302c', 18, h - 16);
  px(g, 34, 14, '#16302c', 18, h - 24);
  px(g, 8, 12, 'rgba(160,200,190,0.25)', 8, h - 28);
  px(g, 36, 18, 'rgba(160,200,190,0.18)', 8, h - 36);
  px(g, 22, 20, '#8a9094', 14, 3);
  px(g, 10, 6, '#9aafbc', 6, 4);
  px(g, 4, h - 8, '#2a2e30', 8, 4);
  px(g, 40, h - 8, '#2a2e30', 8, 4);
}
function shelf(g, w, h) {
  px(g, 2, 6, '#5a4030', w - 4, 4);
  px(g, 2, 16, '#5a4030', w - 4, 4);
  px(g, 4, 4, '#3a281c', 3, h - 6);
  px(g, w - 7, 4, '#3a281c', 3, h - 6);
  px(g, 10, 2, '#c45048', 6, 4);
  px(g, 22, 8, '#d7e4ea', 4, 3);
  px(g, 28, 12, '#3aafb0', 6, 4);
}

export function actorSprite(id, frame = 0) {
  const painters = {
    chris: drawChris,
    pantry: drawPantry,
    momshape: drawMomShape,
    dining: drawDining,
    base: drawBase,
    yardbug: drawYard,
    grin: drawGrinBody,
    down: drawDown,
    'sleep-mom': drawSleepMom,
    'sleep-lump': drawSleepLump,
  };
  const paint = painters[id] || drawPantry;
  return bake(`actor-${id}-${frame}`, 40, 48, (g) => paint(g, frame));
}

function drawChris(g, frame) {
  const step = frame === 1 ? 2 : frame === 3 ? -2 : 0;
  px(g, 12, 42, 'rgba(0,0,0,0.45)', 16, 3);
  px(g, 12, 30 + (step > 0 ? 0 : 1), '#2a3148', 5, 10);
  px(g, 22, 30 + (step < 0 ? 0 : 1), '#243044', 5, 10 + (step ? 1 : 0));
  px(g, 12, 39, '#1a140e', 5, 3);
  px(g, 22, 39, '#1a140e', 5, 3);
  px(g, 11, 18, '#1c2744', 18, 13);
  px(g, 14, 18, '#d5dbe6', 1, 13);
  px(g, 19, 18, '#d5dbe6', 1, 13);
  px(g, 24, 18, '#d5dbe6', 1, 13);
  px(g, 11, 23, '#8f3a3a', 18, 1);
  px(g, 11, 28, '#d5dbe6', 18, 1);
  px(g, 6, 20 + step, '#c49a78', 5, 9);
  px(g, 29, 20 - step, '#c49a78', 5, 9);
  px(g, 13, 8, '#c49a78', 14, 11);
  px(g, 12, 5, '#241c18', 16, 5);
  px(g, 16, 12, '#2a2118', 2, 2);
  px(g, 22, 12, '#2a2118', 2, 2);
  px(g, 15, 12, '#f2efe8', 1, 1);
  px(g, 21, 12, '#f2efe8', 1, 1);
  px(g, 19, 14, '#a87860', 2, 2);
  px(g, 17, 16, '#8a5048', 6, 1);
  px(g, 13, 10, 'rgba(0,0,0,0.25)', 14, 1);
}
function drawPantry(g, frame) {
  const lean = frame % 2;
  px(g, 8, 40, 'rgba(0,0,0,0.4)', 22, 3);
  px(g, 14, 28, '#3a3430', 8, 12);
  px(g, 10, 18 + lean, '#6a5a48', 20, 14);
  px(g, 12, 20, '#e6dcc8', 16, 8);
  px(g, 14, 22, '#8a4038', 4, 4);
  px(g, 4, 24, '#c4a888', 6, 16);
  px(g, 30, 16, '#3a3430', 4, 8);
  px(g, 16, 8 + lean, '#c4a888', 10, 10);
  px(g, 18, 11, '#1a140e', 6, 2);
  px(g, 17, 6, '#2a241c', 12, 4);
}
function drawMomShape(g, frame) {
  const tilt = frame % 2 ? 1 : -1;
  px(g, 14, 42, 'rgba(0,0,0,0.4)', 12, 3);
  px(g, 15, 30, '#2a3140', 4, 12);
  px(g, 22, 30, '#2a3140', 4, 12);
  px(g, 12, 16, '#6f9aab', 16, 16);
  px(g, 14, 18, '#d7e4ea', 12, 3);
  px(g, 10, 20, '#6f9aab', 3, 12);
  px(g, 27, 20, '#6f9aab', 3, 12);
  px(g, 14 + tilt, 6, '#d7c4b0', 12, 11);
  px(g, 14 + tilt, 4, '#4a4038', 12, 4);
  px(g, 17 + tilt, 10, '#1a140e', 2, 2);
  px(g, 22 + tilt, 10, '#1a140e', 2, 2);
}
function drawDining(g, frame) {
  px(g, 6, 42, 'rgba(0,0,0,0.45)', 28, 3);
  px(g, 10, 28, '#1a1816', 8, 12);
  px(g, 22, 28, '#1a1816', 8, 12);
  px(g, 6, 14, '#242220', 28, 16);
  px(g, 8, 16, '#3a3834', 24, 4);
  px(g, 2, 16 + (frame % 2), '#1a1816', 6, 14);
  px(g, 32, 16, '#1a1816', 6, 14);
  px(g, 14, 4, '#c4b4a4', 12, 11);
  px(g, 16, 8, '#f2efe8', 8, 2);
  px(g, 13, 3, '#1a1816', 14, 4);
}
function drawBase(g, frame) {
  const bob = frame % 2;
  px(g, 4, 40, 'rgba(0,0,0,0.5)', 30, 4);
  px(g, 8, 26, '#3a4a48', 10, 10);
  px(g, 22, 28, '#3a4a48', 8, 8);
  px(g, 6, 20 + bob, '#6a7874', 26, 10);
  px(g, 2, 24, '#d7e4ea', 8, 4);
  px(g, 0, 30, '#c4d0cc', 10, 3);
  px(g, 30, 22, '#d7e4ea', 8, 3);
  px(g, 32, 28, '#c4d0cc', 7, 3);
  px(g, 14, 12 + bob, '#d7e0da', 12, 10);
  px(g, 16, 15, '#1a2422', 8, 2);
  px(g, 12, 10, '#9ab0aa', 14, 4);
}
function drawYard(g, frame) {
  const reach = frame % 2 ? 2 : 0;
  px(g, 4, 36, '#2a3a22', 30, 4);
  px(g, 8, 24, '#3a4038', 22, 10);
  px(g, 6, 28, '#2a3028', 4, 10);
  px(g, 28, 28, '#2a3028', 4, 10);
  px(g, 2 + reach, 20, '#4a4038', 12, 4);
  px(g, 0 + reach, 22, '#c4b4a4', 6, 3);
  px(g, 26, 18, '#3a4038', 10, 8);
  px(g, 30, 14, '#2a241c', 8, 6);
  px(g, 32, 16, '#f2efe8', 3, 2);
  px(g, 10, 22, '#6a7a48', 6, 2);
}
function drawGrinBody(g) {
  px(g, 16, 30, '#0c0b0c', 2, 16);
  px(g, 24, 30, '#0c0b0c', 2, 16);
  px(g, 14, 16, '#141216', 12, 16);
  px(g, 4, 18, '#0c0b0c', 2, 22);
  px(g, 34, 18, '#0c0b0c', 2, 22);
  px(g, 8, 16, '#1a1816', 4, 3);
  px(g, 28, 16, '#1a1816', 4, 3);
  px(g, 17, 10, '#0c0b0c', 6, 7);
  px(g, 6, 38, '#2a2420', 3, 2);
  px(g, 34, 38, '#2a2420', 3, 2);
}
function drawDown(g) {
  px(g, 4, 30, 'rgba(0,0,0,0.4)', 32, 4);
  px(g, 6, 26, '#4a4038', 28, 6);
  px(g, 8, 24, '#6a5a48', 10, 4);
  px(g, 22, 22, '#2a241c', 8, 6);
  px(g, 10, 27, '#8a4038', 4, 2);
}
function drawSleepMom(g) {
  px(g, 6, 28, '#6f9aab', 28, 10);
  px(g, 10, 26, '#d7e4ea', 16, 4);
  px(g, 24, 24, '#d7c4b0', 8, 6);
  px(g, 8, 32, '#4a7a88', 8, 4);
}
function drawSleepLump(g) {
  px(g, 8, 30, '#2a2724', 24, 8);
  px(g, 12, 28, '#3a342c', 12, 4);
  px(g, 14, 32, '#1a1816', 6, 2);
}

export function weaponSprite(id) {
  return bake(`held-${id}`, 28, 28, (g) => {
    if (id === 'stick') drawStick(g);
    else if (id === 'knife') drawKnife(g);
    else if (id === 'pistol') drawPistol(g);
    else if (id === 'camera') drawCamera(g);
    else {
      px(g, 12, 4, '#c4b4a4', 4, 16);
      px(g, 10, 18, '#8a4038', 8, 4);
    }
  });
}

export function puffSprite() {
  return bake('puff', 10, 10, (g) => {
    px(g, 4, 3, 'rgba(255,236,196,0.85)', 2, 2);
    px(g, 2, 5, 'rgba(210,180,130,0.45)', 2, 1);
    px(g, 6, 6, 'rgba(255,244,210,0.4)', 2, 1);
    px(g, 3, 7, 'rgba(160,130,90,0.3)', 3, 1);
    px(g, 5, 2, 'rgba(255,250,230,0.5)', 1, 1);
  });
}

export function streakSprite() {
  return bake('streak', 14, 6, (g) => {
    px(g, 0, 2, 'rgba(228,192,122,0.15)', 14, 2);
    px(g, 4, 1, 'rgba(255,244,210,0.8)', 6, 3);
    px(g, 8, 2, '#fff', 2, 1);
  });
}

export function footSprite() {
  return bake('foot', 10, 6, (g) => {
    px(g, 1, 1, 'rgba(20,14,10,0.55)', 3, 4);
    px(g, 5, 1, 'rgba(20,14,10,0.4)', 3, 3);
  });
}

export function scorchSprite() {
  return bake('scorch', 16, 16, (g) => {
    px(g, 2, 4, 'rgba(40,16,12,0.7)', 12, 8);
    px(g, 5, 6, 'rgba(90,20,16,0.8)', 6, 4);
    px(g, 7, 7, '#2a140e', 2, 2);
  });
}

export function moteSprite() {
  return bake('mote', 7, 7, (g) => {
    px(g, 3, 2, 'rgba(255,246,220,0.95)', 1, 2);
    px(g, 2, 4, 'rgba(232,206,150,0.55)', 2, 1);
    px(g, 5, 3, 'rgba(255,236,190,0.35)', 1, 1);
  });
}

export { hash };
