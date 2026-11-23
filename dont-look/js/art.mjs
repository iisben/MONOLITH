// Unique pixel still-lifes for every pickup, key, tool, note, and inspectable.
// Space is empty. Any other character is a palette ink. Masks must not match.

export const INK = {
  K: '#100e0c',
  k: '#2c261f',
  W: '#f4efe4',
  w: '#d9cbb6',
  q: '#a89880',
  B: '#1c2744',
  b: '#3d4e6e',
  R: '#8e2c28',
  r: '#c45048',
  G: '#2f5a38',
  g: '#8fb58a',
  C: '#b87333',
  c: '#e0b84e',
  y: '#f6e2a0',
  S: '#e7eef6',
  s: '#9aa6b2',
  H: '#f7f4ee',
  T: '#3cb8b0',
  t: '#145e58',
  N: '#6b3e28',
  n: '#3a2418',
  O: '#d2b07a',
  o: '#8a5a32',
  A: '#d0893a',
  a: '#f3eadc',
  P: '#f0e6d4',
  p: '#cfc3b0',
  L: '#c5ced6',
  M: '#8d7d58',
  m: '#4a4030',
  F: '#aeb6c0',
  f: '#5c636c',
  U: '#8a4030',
  u: '#5a382e',
  E: '#163044',
  e: '#b7d4e0',
  D: '#12161c',
  d: '#2c343e',
  Z: '#f7f4ee',
  z: '#b7aea0',
  X: '#07080a',
  V: '#6e4a8a',
  Q: '#243028',
};

const PLATE = {
  wood: '#3a2c22',
  steel: '#1a1f26',
  cloth: '#262420',
  paper: '#4a3b2c',
  brass: '#3a3018',
  glass: '#12181e',
  plastic: '#241416',
  dirt: '#1c2218',
  ceramic: '#2a2826',
  canvas: '#2a2618',
};

const FINDER = {
  wood: '#d2b07a',
  steel: '#e7eef6',
  cloth: '#d9cbb6',
  paper: '#f4efe4',
  brass: '#f6e2a0',
  glass: '#b7d4e0',
  plastic: '#c45048',
  dirt: '#8fb58a',
  ceramic: '#f0e6d4',
  canvas: '#d2b07a',
};

export const SPRITES = {};

function pic(id, meta, text) {
  const body = text.replace(/^\n/, '').replace(/\n$/, '');
  const rows = body.split('\n');
  const w = Math.max(...rows.map((row) => row.length));
  const padded = rows.map((row) => row.padEnd(w, ' '));
  for (const row of padded) {
    for (const ch of row) {
      if (ch !== ' ' && !INK[ch]) throw new Error(`${id} uses unknown ink "${ch}"`);
    }
  }
  SPRITES[id] = {
    id,
    w,
    h: padded.length,
    rows: padded,
    plate: 'wood',
    material: 'Worn object',
    glint: null,
    gx: 0,
    gy: 0,
    point: 'none',
    ...meta,
  };
  const def = SPRITES[id];
  const filled = (x, y) => def.rows[y]?.[x] && def.rows[y][x] !== ' ';
  if (!filled(def.gx, def.gy)) {
    const prefer = ['H', 'y', 'S', 'e', 'W', 'T', 'C', 'O', 'a', 'Z', 'c', 'F', 'g', 'r'];
    let found = null;
    for (const ch of prefer) {
      for (let y = 0; y < def.h && !found; y += 1) {
        const x = def.rows[y].indexOf(ch);
        if (x >= 0) found = [x, y];
      }
    }
    if (!found) {
      for (let y = 0; y < def.h && !found; y += 1) {
        for (let x = 0; x < def.w; x += 1) {
          if (def.rows[y][x] !== ' ') {
            found = [x, y];
            break;
          }
        }
      }
    }
    def.gx = found[0];
    def.gy = found[1];
  }
}

pic('stick', {
  plate: 'wood',
  material: 'Taped ash, scuffed blade',
  glint: '#e7eef6',
  gx: 3,
  gy: 15,
  point: 'up',
}, `
   oo
   OO
   KK
   oo
   OO
   KK
   oo
   OO
   nn
   NN
  sss
 sSSSs
sSSHSSs
 sSSSs
  sss
   S
`);

pic('stick-cracked', {
  plate: 'wood',
  material: 'Split ash, tape gone',
  glint: '#d2b07a',
  gx: 1,
  gy: 1,
  point: 'up',
}, `
  oo
 oU Uo
  oo
 o
  OO
   nn
  sS
 sHS
  ss
`);

pic('knife', {
  plate: 'steel',
  material: 'Kitchen steel, riveted handle',
  glint: '#f7f4ee',
  gx: 3,
  gy: 1,
  point: 'up',
}, `
   ss
  sHHs
  sSSs
   ss
   KK
  nNNn
  NCN
  nNNn
   nn
`);

pic('pistol', {
  plate: 'steel',
  material: 'Blued revolver, walnut grip',
  glint: '#f6e2a0',
  gx: 8,
  gy: 2,
  point: 'right',
}, `
      ssssssS
dddddDFcFcFcd
 DDDDDcycdNN
  DDDD fNNKn
      nNN
       n
`);

pic('ammo', {
  plate: 'brass',
  material: 'Brass rounds in a torn card',
  glint: '#f6e2a0',
  gx: 2,
  gy: 2,
  point: 'none',
}, `
 qqqqqqqqqqqq
qCcy Ccy Ccyq
qCc  Cc  Cc q
qCc  Cc  Cc q
 qqqqqqqqqqq
`);

pic('bandage', {
  plate: 'cloth',
  material: 'Gauze roll, one rust stain',
  glint: '#f4efe4',
  gx: 4,
  gy: 3,
  point: 'none',
}, `
   aaaaaa
  aZaZaZa
 aaZaRaZaa
aZaZaZaZaZa
 aaZaZaZaa
  aaaaaaa
   zzzz
`);

pic('pills', {
  plate: 'plastic',
  material: 'Amber bottle, white cap',
  glint: '#f6e2a0',
  gx: 4,
  gy: 1,
  point: 'none',
}, `
  HHHH
  HWW H
  zzzz
  AAAA
  AWWA
  AUUA
  AAAA
  AAAA
   AA
`);

pic('film', {
  plate: 'plastic',
  material: 'Cardboard mount, one dark frame',
  glint: '#f4efe4',
  gx: 1,
  gy: 1,
  point: 'none',
}, `
pppppppppp
pWWWWWWWWp
pWEXXXEWWp
pWEXXXEWWp
pWEXXXEWWp
pWWWWWWWWp
pppppppppp
  p    p
`);

pic('tape', {
  plate: 'cloth',
  material: 'Athletic tape, cardboard core',
  glint: '#f7f4ee',
  gx: 3,
  gy: 2,
  point: 'none',
}, `
  ZZZZZZ
 ZZaZaZZ
ZZa   aZZ
Za  o  aZ
Za oOo aZ
Za  o  aZ
ZZa   aZZ
 ZZZZZZZ
`);

pic('key-her', {
  plate: 'brass',
  material: 'One brass key, round bow',
  glint: '#f6e2a0',
  gx: 3,
  gy: 2,
  point: 'up',
}, `
  ccc
 cyyyc
 cy yc
 cyyyc
  ccc
   c
   c
   c
   cy
   c
`);

pic('key-base', {
  plate: 'steel',
  material: 'Long iron key, rust on the bit',
  glint: '#e7eef6',
  gx: 3,
  gy: 1,
  point: 'up',
}, `
  fFf
 fUFUf
 fFFFf
   f
   f
   f
   f
   fU
   ff
   FU
`);

pic('key-shed', {
  plate: 'brass',
  material: 'Split ring, name scratched off',
  glint: '#f6e2a0',
  gx: 4,
  gy: 2,
  point: 'none',
}, `
  ccccc
 c yyc c
c  c c  F
 c cyc fU
  ccccc f
   c    f
   cy   U
`);

pic('key-car', {
  plate: 'plastic',
  material: 'Black fob, cut metal blade',
  glint: '#e7eef6',
  gx: 3,
  gy: 7,
  point: 'up',
}, `
  DDDD
 DRRRRD
 D WW D
  DDDD
   ss
   SH
   ss
   sS
   ss
`);

pic('fuse', {
  plate: 'ceramic',
  material: 'Ceramic cartridge, tarnished caps',
  glint: '#f7f4ee',
  gx: 8,
  gy: 1,
  point: 'none',
}, `
  ss     ss
sSFFPPPPPFFs
sFFFPUUPPFFs
  ss     ss
`);

pic('gas', {
  plate: 'plastic',
  material: 'Dented jerry can',
  glint: '#f4efe4',
  gx: 3,
  gy: 5,
  point: 'none',
}, `
  kk k
 krrrk
  rrr
 rUUUr
rU r Ur
rU r Ur
 rUUUr
 rrrrr
  RRR
`);

pic('camera', {
  plate: 'steel',
  material: "Ben's body, glass lens",
  glint: '#b7d4e0',
  gx: 11,
  gy: 4,
  point: 'right',
}, `
 nnnnnnn
nDDDDDDn
nDLdLD eee
nDDDDDDeEEe
 nLLLL n s
  nnn   s
   zz
`);

pic('flashlight', {
  plate: 'steel',
  material: 'Pocket light, yellow lens',
  glint: '#f6e2a0',
  gx: 5,
  gy: 1,
  point: 'right',
}, `
  DDDDcc
 DDkkDDcC
DDk  kDDc
 DDDDDDc
  kkkkk
   kKk
`);

pic('battery', {
  plate: 'plastic',
  material: 'Two cells, copper caps',
  glint: '#f6e2a0',
  gx: 2,
  gy: 1,
  point: 'none',
}, `
 CC   CC
CgGg CgGg
CgGg CgGg
CgGg CgGg
CkKk CkKk
`);

pic('note-foyer', {
  plate: 'paper',
  material: 'Lined notepad, folded corner',
  glint: '#f4efe4',
  gx: 2,
  gy: 2,
  point: 'none',
}, `
wwwwwwww
wqwqwqwq
wwwwwwww
wBwwwwww
wwwwwww
wqwqwqw
wwwwwww
wqwwwww
wwwwwww
`);

pic('note-kitchen', {
  plate: 'paper',
  material: 'Torn grocery scrap',
  glint: '#f4efe4',
  gx: 1,
  gy: 3,
  point: 'none',
}, `
ww wwww w
wwwwwwwww
wkw wkw w
wwwwwwwww
wkwkwkwkw
wwwwwwwww
 wwww www
  wwwww
`);

pic('note-hockey', {
  plate: 'paper',
  material: 'Desk sheet, punched holes',
  glint: '#f4efe4',
  gx: 6,
  gy: 1,
  point: 'none',
}, `
Kk wwwwwwww
Kk wOwwwwww
Kk wwwwwwww
Kk wkwkwkww
Kk wwwwwwww
Kk wwwwwBww
 wwwwwwwwww
`);

pic('note-journal', {
  plate: 'cloth',
  material: 'Cloth journal, turquoise spine',
  glint: '#3cb8b0',
  gx: 2,
  gy: 2,
  point: 'none',
}, `
TTTTTT
TwwwwwT
TwqwqwT
TwwwwwT
TwqwqwT
TwwwwwT
TTTTTT
  tt
`);

pic('note-diary', {
  plate: 'wood',
  material: 'Leather diary, ribbon marker',
  glint: '#d2b07a',
  gx: 3,
  gy: 2,
  point: 'none',
}, `
 nnnnnn
nNNWNNn
nNwwwNn
nNNWNNn
nNwwwNn
 nnnnnn
   R
   R
`);

pic('note-ray', {
  plate: 'dirt',
  material: 'Basement scrap, gouged',
  glint: '#d9cbb6',
  gx: 2,
  gy: 2,
  point: 'none',
}, `
  qqqq
 qKqqwq
qwwqwwqq
 qwwKqq
  qqqq
   q
`);

pic('note-bag', {
  plate: 'glass',
  material: 'Lock screen, cracked glass',
  glint: '#b7d4e0',
  gx: 3,
  gy: 1,
  point: 'none',
}, `
 kkkkkkk
kDDDDDDk
kD e e Dk
kD  W  Dk
kDDDDDDk
k  eee k
 kkkkkkk
`);

pic('look-duffel', {
  plate: 'canvas',
  material: 'Navy team duffel',
  glint: '#e7eef6',
  gx: 4,
  gy: 2,
  point: 'none',
}, `
  bbbbbbb
 bBBBBBBb
bBSBSBSBBb
 bBBBBBB
  zzzzz
`);

pic('look-front', {
  plate: 'wood',
  material: 'Front door, warm peephole',
  glint: '#f6e2a0',
  gx: 5,
  gy: 4,
  point: 'none',
}, `
 nnnnnnnn
nOOOyyOOn
nOOooooOn
nOOOyyOOn
nOOOOOOOn
nOOOcOOOn
nOOOOOOOn
 nnnnnnn
  kk kk
`);

pic('look-family', {
  plate: 'wood',
  material: 'Yard frame, three people',
  glint: '#f4efe4',
  gx: 2,
  gy: 1,
  point: 'none',
}, `
nnnnnnnnnnnn
nWWWWwWWWWn
nWOWWTWOWWn
nWqqWWqqWWn
nWWWWWWWWWn
 nnnnnnnnnn
`);

pic('look-tv', {
  plate: 'glass',
  material: 'CRT, curved glass',
  glint: '#b7d4e0',
  gx: 4,
  gy: 3,
  point: 'none',
}, `
 ddddddddd
dEEEEEEEEd
dEeEeEeEed
dEEEEEEEEd
 ddddddddd
  kk   kk
`);

pic('look-herpic', {
  plate: 'cloth',
  material: 'Small frame, turquoise mat',
  glint: '#3cb8b0',
  gx: 3,
  gy: 2,
  point: 'none',
}, `
 nnnnn
nTTTTT n
nTWWWT n
nTWWWT
nTTTTT
 nnnnn
`);

pic('look-mirror', {
  plate: 'glass',
  material: 'Mirror, lipstick on the glass',
  glint: '#b7d4e0',
  gx: 4,
  gy: 3,
  point: 'none',
}, `
  eeee
 eEEEEe
eEEEREE e
eEEEEEE
eEEEeEE
 eEEEEe
  eeee
   R
`);

pic('look-mompic', {
  plate: 'cloth',
  material: 'Nightstand frame on a stand',
  glint: '#b7d4e0',
  gx: 3,
  gy: 2,
  point: 'none',
}, `
 nnnnnn
nBBBBBn
nBeeeBn
nBBBBBn
 nnnnnn
   nn
  nnnn
`);

pic('look-badge', {
  plate: 'paper',
  material: 'Scratched polaroid',
  glint: '#f4efe4',
  gx: 2,
  gy: 1,
  point: 'none',
}, `
 wwwwww
wKKKKw
wKeEKw
wKKKKw
 wwwwww
 wqqqqw
 wwwwww
`);

pic('look-tanks', {
  plate: 'steel',
  material: 'Pressure gauge, green glass',
  glint: '#8fb58a',
  gx: 5,
  gy: 3,
  point: 'none',
}, `
  ffff
 fGGGGf
fGGggGGf
fGGGgGGf
 fGGGGf
  ffff
   ff
`);

pic('look-yard', {
  plate: 'dirt',
  material: 'Garden stake, wrong-color bloom',
  glint: '#c45048',
  gx: 4,
  gy: 1,
  point: 'none',
}, `
   r r
  rWWWr
   QQQ
   Q Q
   ooo
   ooo
  kkkkk
`);

pic('look-cloth', {
  plate: 'cloth',
  material: 'Torn turquoise, caught on a nail',
  glint: '#3cb8b0',
  gx: 3,
  gy: 3,
  point: 'none',
}, `
  fff
 f T f
  TTT
 TTTTTT
T T  T T
 TTTT
  TT
   T
`);

pic('cabinet', {
  plate: 'steel',
  material: 'Medicine cabinet, left door',
  glint: '#e7eef6',
  gx: 2,
  gy: 2,
  point: 'none',
}, `
ssssssssss
sLLs  sLLs
sLLs  sLLs
sLLs  sccs
ssssssssss
  kk  kk
`);

pic('gearbag', {
  plate: 'canvas',
  material: 'Olive gear bag, strap up',
  glint: '#d2b07a',
  gx: 4,
  gy: 3,
  point: 'none',
}, `
  M     M
 MMMMMMMMM
MQggQggQggM
 MMMMMMMMM
  zzzzzzz
`);

pic('bench', {
  plate: 'wood',
  material: 'Film jig on the bench',
  glint: '#f4efe4',
  gx: 7,
  gy: 2,
  point: 'none',
}, `
nnnnnnnnnnnn
nOOoo ppOOOn
nOOOOppppOOn
 nnnnnnnnnn
  N      N
`);

pic('bin', {
  plate: 'plastic',
  material: 'Parts bin, four wells',
  glint: '#e7eef6',
  gx: 2,
  gy: 1,
  point: 'none',
}, `
 fffffffffff
f s s s s sf
f k k k k kf
f s s s s sf
 fffffffffff
`);

pic('handle', {
  plate: 'steel',
  material: 'Driver door, lock under the handle',
  glint: '#e7eef6',
  gx: 2,
  gy: 1,
  point: 'none',
}, `
 sssss
sSSSSs
 sssss
   c
  cyc
   c
`);

pic('clipboard', {
  plate: 'paper',
  material: 'Garage checklist, steel clip',
  glint: '#e7eef6',
  gx: 4,
  gy: 1,
  point: 'none',
}, `
  sss
 nnnnnn
nwwwwwn
nwkwwwn
nwwwwwn
nwkwwwn
nwwwwwn
 nnnnnn
`);

const rasterCache = new Map();

export function artIds() {
  return Object.keys(SPRITES);
}

export function maskOf(id) {
  const def = SPRITES[id];
  if (!def) return '';
  let bits = `${def.w}x${def.h}:`;
  for (const row of def.rows) {
    for (const ch of row) bits += ch === ' ' ? '.' : '#';
  }
  return bits;
}

export function resolveArt(id, broken = false) {
  if (broken && SPRITES[`${id}-cracked`]) return `${id}-cracked`;
  return SPRITES[id] ? id : null;
}

export function artIdForProp(prop) {
  if (!prop) return null;
  if (prop.kind === 'pickup') return prop.item;
  if (prop.kind === 'flashlight') return 'flashlight';
  if (prop.kind === 'battery') return 'battery';
  if (prop.kind === 'note') return `note-${prop.note}`;
  if (prop.kind === 'inspect') return `look-${prop.inspect}`;
  if (prop.kind === 'container') return prop.id === 'cabinet' ? 'cabinet' : 'gearbag';
  if (prop.kind === 'save') return 'bench';
  if (prop.kind === 'stash') return 'bin';
  if (prop.kind === 'car') return 'handle';
  if (prop.kind === 'board') return 'clipboard';
  return null;
}

export function shadowFor(id) {
  const art = resolveArt(id) || id;
  const def = SPRITES[art];
  if (!def) return { radius: 3.1, h: 6 };
  return {
    radius: Math.max(2.4, Math.min(8, def.w * 0.42)),
    h: Math.max(4, Math.min(16, def.h * 0.55)),
  };
}

export function finderColor(id) {
  const def = SPRITES[resolveArt(id) || id];
  if (!def) return '#e4c07a';
  return def.glint || FINDER[def.plate] || '#e4c07a';
}

export function materialOf(id, broken = false) {
  const art = resolveArt(id, broken);
  return SPRITES[art]?.material || '';
}

const PAD = 1;

function paintRows(ctx, def, ox, oy) {
  ctx.fillStyle = '#070605';
  for (let y = 0; y < def.h; y += 1) {
    for (let x = 0; x < def.w; x += 1) {
      if (def.rows[y][x] === ' ') continue;
      for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const nx = x + dx;
        const ny = y + dy;
        const outside = nx < 0 || ny < 0 || nx >= def.w || ny >= def.h;
        if (outside || def.rows[ny][nx] === ' ') ctx.fillRect(ox + nx, oy + ny, 1, 1);
      }
    }
  }
  for (let y = 0; y < def.h; y += 1) {
    const row = def.rows[y];
    for (let x = 0; x < def.w; x += 1) {
      const ch = row[x];
      if (ch === ' ') continue;
      ctx.fillStyle = INK[ch];
      ctx.fillRect(ox + x, oy + y, 1, 1);
    }
  }
}

function raster(id, plate) {
  const key = plate ? `${id}:plate` : id;
  const hit = rasterCache.get(key);
  if (hit) return hit;
  const def = SPRITES[id];
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');
  if (plate) {
    const pad = 10;
    canvas.width = def.w + pad * 2;
    canvas.height = def.h + pad * 2 + 4;
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = PLATE[def.plate] || PLATE.wood;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.beginPath();
    ctx.ellipse(canvas.width / 2, pad + def.h + 2, Math.max(5, def.w * 0.46), 2.4, 0, 0, Math.PI * 2);
    ctx.fill();
    paintRows(ctx, def, pad, pad);
    if (def.glint) {
      ctx.fillStyle = def.glint;
      ctx.fillRect(pad + def.gx, pad + def.gy, 1, 1);
    }
  } else {
    canvas.width = def.w + PAD * 2;
    canvas.height = def.h + PAD * 2;
    ctx.imageSmoothingEnabled = false;
    paintRows(ctx, def, PAD, PAD);
  }
  rasterCache.set(key, canvas);
  return canvas;
}

function phase(id) {
  let n = 0;
  for (let i = 0; i < id.length; i += 1) n += id.charCodeAt(i) * (i + 2);
  return n % 17;
}

function drawGlintPixel(ctx, def, localX, localY, opt) {
  if (def.gx == null || !def.glint) return;
  const wave = Math.sin((opt.time || 0) * 1.25 + phase(def.id));
  let alpha = opt.reduce ? 0.55 : wave > 0.35 ? 0.95 : 0.18;
  if (opt.alpha != null) alpha *= opt.alpha;
  ctx.globalAlpha = alpha;
  ctx.fillStyle = def.glint;
  ctx.fillRect(Math.round(localX + def.gx), Math.round(localY + def.gy), 1, 1);
  ctx.globalAlpha = opt.alpha ?? 1;
}

export function drawWorldSprite(ctx, id, x, y, opt = {}) {
  const artId = resolveArt(id, opt.broken);
  const def = artId && SPRITES[artId];
  if (!def || typeof document === 'undefined') return;
  const src = raster(artId, false);
  ctx.save();
  ctx.imageSmoothingEnabled = false;
  ctx.translate(Math.round(x), Math.round(y));
  if (opt.alpha != null) ctx.globalAlpha = opt.alpha;
  const left = Math.round(-(def.w + PAD * 2) / 2);
  const top = Math.round(-(def.h + PAD * 2) / 2);
  if (opt.aim != null) {
    const offset = def.point === 'up' ? -Math.PI / 2 : 0;
    ctx.rotate(opt.aim + offset);
    const jab = opt.jab || 0;
    ctx.drawImage(src, left + jab, top);
    drawGlintPixel(ctx, def, left + jab + PAD, top + PAD, opt);
  } else if (opt.ground) {
    const foot = opt.foot ?? 2;
    ctx.translate(0, foot);
    if (opt.shadow !== false) {
      ctx.fillStyle = 'rgba(0,0,0,0.55)';
      ctx.beginPath();
      ctx.ellipse(1, 1, Math.max(4, def.w * 0.48), 2.3, 0, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.drawImage(src, left, -(def.h + PAD * 2));
    drawGlintPixel(ctx, def, left + PAD, -(def.h + PAD * 2) + PAD, opt);
  } else {
    ctx.drawImage(src, left, top);
    drawGlintPixel(ctx, def, left + PAD, top + PAD, opt);
  }
  ctx.restore();
}

export function spriteCanvas(id, { broken = false, plate = false } = {}) {
  const artId = resolveArt(id, broken);
  if (!artId || typeof document === 'undefined') return null;
  const src = raster(artId, plate);
  const copy = document.createElement('canvas');
  copy.width = src.width;
  copy.height = src.height;
  copy.className = 'sprite';
  const ctx = copy.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(src, 0, 0);
  return copy;
}
