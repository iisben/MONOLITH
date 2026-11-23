// Physical doors. Tile size matches world.mjs TILE.
// A closed leaf is centered in the wall and flush with both jambs.
// It swings on the hinge into a clear floor tile, then rests as a thin
// slab along that wall so a body can pass the opening.

export const TILE = 32;
export const SWING_FRAMES = 8;

const JAMB = 2;
const THICK = 12;
const OPEN_THICK = 3;

const FACE_OF = {
  her: 'bedroom',
  bath: 'bath',
  chris: 'bedroom',
  mom: 'bedroom',
  hall: 'stained',
  'liv-h': 'stained',
  din: 'stained',
  'kit-n': 'kitchen',
  'foy-n': 'stained',
  'kit-e': 'kitchen',
  'foy-e': 'stained',
  'din-s': 'stained',
  'gar-n': 'util',
  'gar-w': 'util',
  base: 'heavy',
  yard: 'overhead',
  shed: 'shed',
  front: 'sealed',
};

const KIND_OF = {
  base: 'heavy',
  yard: 'overhead',
  shed: 'shed',
  front: 'sealed',
};

// Which way the leaf prefers to swing when both rooms are clear.
const PREFER_SWING = {
  her: 'w',
  bath: 'e',
  chris: 'w',
  mom: 'e',
  hall: 's',
  'liv-h': 's',
  din: 'e',
  'kit-n': 'n',
  'foy-n': 's',
  'kit-e': 'w',
  'foy-e': 'e',
  'din-s': 'n',
  'gar-n': 'n',
  'gar-w': 'n',
  base: 's',
  shed: 'w',
};

const DIR = {
  n: { x: 0, y: -1 },
  s: { x: 0, y: 1 },
  e: { x: 1, y: 0 },
  w: { x: -1, y: 0 },
};

const FACE = {
  bedroom: { leaf: [92, 58, 40], plank: [74, 46, 32], panel: [46, 28, 20], edge: [196, 160, 122], metal: [176, 148, 96], grime: [22, 14, 10] },
  bath: { leaf: [108, 114, 112], plank: [90, 98, 96], panel: [70, 78, 76], edge: [214, 210, 200], metal: [186, 192, 196], grime: [36, 48, 46] },
  stained: { leaf: [78, 46, 30], plank: [62, 36, 22], panel: [40, 22, 14], edge: [168, 116, 72], metal: [138, 112, 72], grime: [18, 10, 8] },
  kitchen: { leaf: [112, 86, 56], plank: [96, 72, 46], panel: [64, 46, 30], edge: [210, 176, 128], metal: [196, 190, 176], grime: [40, 30, 18] },
  util: { leaf: [86, 76, 64], plank: [70, 62, 52], panel: [48, 42, 36], edge: [138, 126, 112], metal: [154, 158, 162], grime: [24, 20, 16] },
  heavy: { leaf: [54, 44, 36], plank: [40, 32, 26], panel: [26, 20, 16], edge: [106, 88, 72], metal: [110, 116, 120], grime: [8, 10, 14] },
  shed: { leaf: [102, 92, 74], plank: [84, 74, 58], panel: [58, 50, 40], edge: [138, 126, 104], metal: [74, 78, 82], grime: [36, 34, 26] },
  overhead: { leaf: [92, 98, 104], plank: [70, 76, 82], panel: [52, 58, 64], edge: [168, 174, 180], metal: [196, 200, 204], grime: [32, 28, 24] },
  sealed: { leaf: [104, 58, 42], plank: [82, 44, 32], panel: [52, 26, 20], edge: [196, 138, 106], metal: [90, 88, 86], grime: [36, 16, 12] },
};

// The front door is set into the exterior wall. It does not open.
// Frame occupies the west face of wall tile (28, 26), flush with the
// mudroom boundary and with the top and bottom of that wall cell.
export const SEALED_DOORS = [
  {
    id: 'front',
    x: 28,
    y: 26,
    kind: 'sealed',
    face: 'sealed',
    frame: { x: 0, y: 0, w: 13, h: TILE },
  },
];

const BODY = [[0, 0], [1, 0], [-1, 0], [0, 1], [0, -1], [0.7, 0.7], [0.7, -0.7], [-0.7, 0.7], [-0.7, -0.7]];

function hash(x, y) {
  let n = (x * 374761393 + y * 668265263) | 0;
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return (n ^ (n >>> 16)) >>> 0;
}

function rot(v, ang) {
  const c = Math.cos(ang);
  const s = Math.sin(ang);
  return { x: v.x * c - v.y * s, y: v.x * s + v.y * c };
}

function ease(t) {
  const x = Math.max(0, Math.min(1, t));
  return x * x * (3 - 2 * x);
}

function tileAt(tiles, x, y) {
  return tiles[y]?.[x];
}

function isFloor(tile) {
  return tile && tile !== 0 && tile !== 9;
}

function neighbor(door, dir) {
  return { x: door.x + dir.x, y: door.y + dir.y };
}

export function createDoorState(door) {
  const open = !!door.open;
  const st = {
    open,
    unlocked: !door.lock,
    angle: open ? 1 : 0,
    vel: 0,
    rattle: 0,
    rattleSent: 0,
    slam: false,
    pendingSlam: false,
    latched: !open,
    arrivedOpen: open,
    frame: open ? SWING_FRAMES : 0,
    block: 'tile',
  };
  syncDoorMotion(door, st);
  return st;
}

export function syncDoorMotion(door, st) {
  if (!door || door.kind === 'sealed') return;
  if (door.kind === 'overhead') {
    st.block = 'curtain';
    return;
  }
  const corners = leafCorners(door, st.angle);
  const pocket = door.pocket
    ? { x: (door.pocket.x - door.x + 0.5) * TILE, y: (door.pocket.y - door.y + 0.5) * TILE }
    : null;
  const hit = polyHitsCircle(corners, TILE / 2, TILE / 2, 12)
    || (pocket && polyHitsCircle(corners, pocket.x, pocket.y, 12));
  st.block = hit ? 'tile' : 'leaf';
}

export function prepareDoor(door, tiles, furniture) {
  door.face = FACE_OF[door.id] || 'stained';
  door.kind = KIND_OF[door.id] || 'wood';
  door.jamb = JAMB;
  door.thick = door.kind === 'overhead' ? 14 : THICK;
  door.openThick = OPEN_THICK;
  door.span = TILE - JAMB * 2;

  const sides = ['n', 'e', 's', 'w'].map((name) => {
    const dir = DIR[name];
    const x = door.x + dir.x;
    const y = door.y + dir.y;
    const tile = tileAt(tiles, x, y);
    return { name, dir, x, y, tile: tile ?? 0, floor: isFloor(tile) };
  });
  const floors = sides.filter((side) => side.floor);
  const opposite = floors.length === 2
    && floors[0].dir.x + floors[1].dir.x === 0
    && floors[0].dir.y + floors[1].dir.y === 0;
  door.axis = opposite && floors.some((side) => side.name === 'e') ? 'ns' : 'ew';
  door.passage = floors.map((side) => ({ ...side.dir, x: side.x, y: side.y }));
  door.fitIssues = [];
  if (!opposite) door.fitIssues.push('passage');
  const jambNames = door.axis === 'ns' ? ['n', 's'] : ['e', 'w'];
  for (const name of jambNames) {
    const side = sides.find((item) => item.name === name);
    if (!side || side.tile !== 0) door.fitIssues.push(`jamb-${name}`);
  }

  if (door.kind === 'overhead') {
    door.hinge = 'n';
    door.swing = 'n';
    door.closedDir = { x: 1, y: 0 };
    door.swingSign = -1;
    door.hingeClosed = { x: JAMB, y: TILE / 2 };
    door.hingeOpen = { x: JAMB, y: OPEN_THICK / 2 };
    door.pocket = null;
    if (door.fitIssues.length) {
      throw new Error(`door ${door.id} is not seated in its wall (${door.fitIssues.join(', ')})`);
    }
    return door;
  }

  const hingeEnds = door.axis === 'ns' ? ['n', 's'] : ['w', 'e'];
  const swingDirs = door.axis === 'ns' ? ['w', 'e'] : ['n', 's'];
  let best = null;
  for (const hinge of hingeEnds) {
    for (const swing of swingDirs) {
      const swingSide = sides.find((side) => side.name === swing);
      let score = 0;
      if (!swingSide?.floor) score = -1;
      else if (furniture.has(`${swingSide.x},${swingSide.y}`)) score = -1;
      else {
        score = 100;
        if (PREFER_SWING[door.id] === swing) score += 20;
        if (hinge === 'n' || hinge === 'w') score += 2;
        for (const name of ['n', 'e', 's', 'w']) {
          const dir = DIR[name];
          if (isFloor(tileAt(tiles, swingSide.x + dir.x, swingSide.y + dir.y))) score += 1;
        }
      }
      const option = { hinge, swing, score };
      if (!best || option.score > best.score || (option.score === best.score && `${hinge}${swing}` < `${best.hinge}${best.swing}`)) {
        best = option;
      }
    }
  }
  if (!best || best.score < 0) {
    throw new Error(`door ${door.id} has nowhere to swing`);
  }
  applyHinge(door, best.hinge, best.swing);
  if (door.fitIssues.length) {
    throw new Error(`door ${door.id} is not seated in its wall (${door.fitIssues.join(', ')})`);
  }
  return door;
}

function applyHinge(door, hinge, swing) {
  door.hinge = hinge;
  door.swing = swing;
  const inward = {
    n: { x: TILE / 2, y: JAMB },
    s: { x: TILE / 2, y: TILE - JAMB },
    w: { x: JAMB, y: TILE / 2 },
    e: { x: TILE - JAMB, y: TILE / 2 },
  };
  const along = { n: { x: 0, y: 1 }, s: { x: 0, y: -1 }, w: { x: 1, y: 0 }, e: { x: -1, y: 0 } };
  door.hingeClosed = inward[hinge];
  door.closedDir = along[hinge];
  const turned = rot(door.closedDir, Math.PI / 2);
  const target = DIR[swing];
  const dot = turned.x * target.x + turned.y * target.y;
  door.swingSign = dot >= 0 ? 1 : -1;
  const openHinge = { x: TILE / 2, y: TILE / 2 };
  if (hinge === 'n' || hinge === 's') openHinge.y = hinge === 'n' ? OPEN_THICK / 2 : TILE - OPEN_THICK / 2;
  else openHinge.x = hinge === 'w' ? OPEN_THICK / 2 : TILE - OPEN_THICK / 2;
  if (swing === 'w') openHinge.x = 0;
  else if (swing === 'e') openHinge.x = TILE;
  else if (swing === 'n') openHinge.y = 0;
  else openHinge.y = TILE;
  door.hingeOpen = openHinge;
  const pocketDir = DIR[swing];
  door.pocket = { x: door.x + pocketDir.x, y: door.y + pocketDir.y };
}

export function leafPose(door, angle, wobble = 0) {
  const e = ease(angle);
  const thick = door.thick + (door.openThick - door.thick) * e;
  const hinge = {
    x: door.hingeClosed.x + (door.hingeOpen.x - door.hingeClosed.x) * e,
    y: door.hingeClosed.y + (door.hingeOpen.y - door.hingeClosed.y) * e,
  };
  let dir = rot(door.closedDir, door.swingSign * e * (Math.PI / 2));
  const len = Math.hypot(dir.x, dir.y) || 1;
  dir = { x: dir.x / len, y: dir.y / len };
  const n = { x: -dir.y, y: dir.x };
  hinge.x += n.x * wobble;
  hinge.y += n.y * wobble;
  return { hinge, dir, n, thick, span: door.span, e };
}

export function leafCorners(door, angle, wobble = 0) {
  const pose = leafPose(door, angle, wobble);
  const hw = pose.thick / 2;
  const tipX = pose.hinge.x + pose.dir.x * pose.span;
  const tipY = pose.hinge.y + pose.dir.y * pose.span;
  return [
    { x: pose.hinge.x + pose.n.x * hw, y: pose.hinge.y + pose.n.y * hw },
    { x: pose.hinge.x - pose.n.x * hw, y: pose.hinge.y - pose.n.y * hw },
    { x: tipX - pose.n.x * hw, y: tipY - pose.n.y * hw },
    { x: tipX + pose.n.x * hw, y: tipY + pose.n.y * hw },
  ];
}

export function curtainSouth(door, angle) {
  const housing = 3;
  return housing + (TILE - housing) * (1 - ease(angle));
}

export function doorBlocksPoint(door, st, px, py) {
  if (!door || door.kind === 'sealed') return false;
  const x = px - door.x * TILE;
  const y = py - door.y * TILE;
  if (door.kind === 'overhead') {
    const south = curtainSouth(door, st.angle);
    return x >= door.jamb && x < TILE - door.jamb && y >= 0 && y < south - 0.05;
  }
  if (st.block === 'tile') {
    return x >= 0 && y >= 0 && x < TILE && y < TILE;
  }
  return pointInConvex(leafCorners(door, st.angle), x, y);
}

export function doorSeals(door, st) {
  return bodyBlocked(door, st, (door.x + 0.5) * TILE, (door.y + 0.5) * TILE, 8);
}

export function bodyBlocked(door, st, x, y, radius) {
  for (const [ox, oy] of BODY) {
    if (doorBlocksPoint(door, st, x + ox * radius, y + oy * radius)) return true;
  }
  return false;
}

export function interactPoint(door, st) {
  const ox = door.x * TILE;
  const oy = door.y * TILE;
  if (door.kind === 'overhead' || st.angle >= 0.42) {
    return { x: ox + TILE / 2, y: oy + TILE / 2 };
  }
  const pose = leafPose(door, st.angle);
  const u = door.span - 5;
  return {
    x: ox + pose.hinge.x + pose.dir.x * u,
    y: oy + pose.hinge.y + pose.dir.y * u,
  };
}

export function planDoorUse(door, st, { has, powerOn }) {
  if (!st.open) {
    if (door.lock === 'power' && !powerOn && !st.unlocked) return { action: 'rattle', say: 'no-power' };
    if (door.lock && door.lock !== 'power' && !st.unlocked) {
      if (!has(door.lock)) return { action: 'rattle', say: 'locked' };
      return { action: 'unlock', consume: door.lock };
    }
    return { action: 'open', noise: 60 };
  }
  if (door.sticky) return { action: 'refuse', say: 'sticky' };
  return { action: 'close' };
}

export function stepDoor(door, st, dt, { reduce = false } = {}) {
  const events = [];
  const overhead = door.kind === 'overhead';
  const target = st.open ? 1 : 0;

  if (st.pendingSlam) {
    st.pendingSlam = false;
    st.open = false;
    st.angle = 0;
    st.vel = 0;
    st.slam = false;
    st.latched = true;
    st.arrivedOpen = false;
    st.frame = 0;
    st.rattle = Math.max(st.rattle, 0.32);
    events.push({ type: 'slam', intensity: 1 });
  } else if (reduce) {
    if (Math.abs(st.angle - target) > 0.0001 || st.slam) {
      const closing = target < st.angle || st.slam;
      st.angle = target;
      st.vel = 0;
      st.frame = target >= 1 ? SWING_FRAMES : 0;
      st.latched = target === 0;
      st.arrivedOpen = target === 1;
      events.push({
        type: closing ? (st.slam ? 'slam' : 'latch') : (overhead ? 'motor-stop' : 'thud'),
        intensity: 1,
      });
      st.slam = false;
    }
  } else {
    const k = st.slam && !st.open ? 140 : overhead ? 16 : 48;
    const damp = st.slam && !st.open ? 18 : overhead ? 8 : 12.4;
    st.vel += ((target - st.angle) * k - st.vel * damp) * dt;
    st.vel = Math.max(-8, Math.min(8, st.vel));
    st.angle += st.vel * dt;
    if (Math.abs(target - st.angle) < 0.012 && Math.abs(st.vel) < 0.2) {
      st.angle = target;
      st.vel = target === 0 ? -0.2 : 0.2;
    }

    if (st.angle <= 0) {
      const impact = Math.max(0, -st.vel);
      st.angle = 0;
      if (!st.latched) {
        const hard = st.slam || impact > 3.2;
        events.push({ type: hard ? 'slam' : 'latch', intensity: Math.min(1, 0.5 + impact / 5) });
        st.slam = false;
      }
      st.vel = 0;
      st.latched = true;
      st.arrivedOpen = false;
    } else if (st.angle >= 1) {
      const impact = Math.max(0, st.vel);
      st.angle = 1;
      if (!st.arrivedOpen) {
        events.push({ type: overhead ? 'motor-stop' : 'thud', intensity: Math.min(1, 0.45 + impact / 4) });
      }
      st.vel = 0;
      st.arrivedOpen = true;
      st.latched = false;
      st.slam = false;
    } else {
      if (st.angle > 0.08) st.latched = false;
      if (st.angle < 0.92) st.arrivedOpen = false;
    }

    const next = Math.max(0, Math.min(SWING_FRAMES, Math.round(st.angle * SWING_FRAMES)));
    if (next !== st.frame) {
      const dir = Math.sign(next - st.frame);
      for (let frame = st.frame + dir; dir > 0 ? frame <= next : frame >= next; frame += dir) {
        if (frame <= 0 || frame >= SWING_FRAMES) continue;
        const intensity = Math.min(1, Math.abs(st.vel) / 2.2);
        if (intensity > 0.05) {
          events.push({ type: overhead ? 'motor' : 'creak', intensity, frame });
        }
      }
      st.frame = next;
    }
  }

  if (st.rattle > 0) {
    const prev = st.rattle;
    st.rattle = Math.max(0, st.rattle - dt);
    const marks = [0.34, 0.22, 0.1];
    for (let i = st.rattleSent; i < marks.length; i++) {
      if (prev >= marks[i] && st.rattle < marks[i]) {
        events.push({ type: overhead ? 'motor-rattle' : 'rattle', intensity: 0.75 - i * 0.18 });
        st.rattleSent = i + 1;
      }
    }
    if (st.rattle === 0) st.rattleSent = 0;
  }

  syncDoorMotion(door, st);
  return events;
}

export function auditWorld(doors, tiles, furniture) {
  return doors.map((door) => auditDoor(door, tiles, furniture));
}

export function auditDoor(door, tiles, furniture) {
  const issues = [...(door.fitIssues || [])];
  const row = {
    id: door.id,
    tile: `${door.x},${door.y}`,
    axis: door.axis,
    kind: door.kind,
    hinge: door.hinge || '—',
    swing: door.swing || '—',
    leaf: '',
    center: '',
    flush: '',
    open: '',
    pocket: door.pocket ? `${door.pocket.x},${door.pocket.y}` : '—',
    issues,
  };
  if (door.kind === 'overhead') {
    const south = curtainSouth(door, 0);
    row.leaf = `x ${door.jamb}..${TILE - door.jamb} y 0..${south.toFixed(0)}`;
    row.center = '0.0';
    row.flush = south === TILE ? 'flush' : 'short';
    if (south !== TILE) issues.push('curtain');
    const closed = createDoorState({ ...door, open: false, lock: null });
    const opened = createDoorState({ ...door, open: true, lock: null });
    const cx = (door.x + 0.5) * TILE;
    const cy = (door.y + 0.5) * TILE;
    if (!bodyBlocked(door, closed, cx, cy, 9)) issues.push('closed-clear');
    if (bodyBlocked(door, opened, cx, cy, 12)) issues.push('open-blocks');
    row.open = issues.includes('open-blocks') ? 'blocks' : 'clears';
    return row;
  }
  const corners = leafCorners(door, 0);
  const xs = corners.map((p) => p.x);
  const ys = corners.map((p) => p.y);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...ys);
  const maxY = Math.max(...ys);
  const cx = (minX + maxX) / 2;
  const cy = (minY + maxY) / 2;
  const centerErr = Math.hypot(cx - TILE / 2, cy - TILE / 2);
  row.leaf = `x ${minX.toFixed(1)}..${maxX.toFixed(1)} y ${minY.toFixed(1)}..${maxY.toFixed(1)}`;
  row.center = centerErr.toFixed(2);
  const spanMin = door.axis === 'ns' ? minY : minX;
  const spanMax = door.axis === 'ns' ? maxY : maxX;
  const flush = Math.abs(spanMin - JAMB) < 0.2 && Math.abs(spanMax - (TILE - JAMB)) < 0.2;
  row.flush = flush ? 'flush' : 'gap';
  if (!flush) issues.push('flush');
  if (centerErr > 0.2) issues.push('center');
  const thickMin = door.axis === 'ns' ? minX : minY;
  const thickMax = door.axis === 'ns' ? maxX : maxY;
  if (Math.abs((thickMin + thickMax) / 2 - TILE / 2) > 0.2) issues.push('thickness-center');

  const closed = createDoorState({ ...door, open: false, lock: door.lock });
  const opened = createDoorState({ ...door, open: true, lock: null });
  opened.angle = 1;
  opened.block = 'leaf';
  syncDoorMotion(door, opened);
  const worldCenter = { x: (door.x + 0.5) * TILE, y: (door.y + 0.5) * TILE };
  if (!bodyBlocked(door, closed, worldCenter.x, worldCenter.y, 9)) issues.push('closed-clear');
  if (bodyBlocked(door, opened, worldCenter.x, worldCenter.y, 12)) issues.push('open-blocks');
  if (door.pocket) {
    const px = (door.pocket.x + 0.5) * TILE;
    const py = (door.pocket.y + 0.5) * TILE;
    if (bodyBlocked(door, opened, px, py, 12)) issues.push('pocket-blocks');
    if (!isFloor(tileAt(tiles, door.pocket.x, door.pocket.y))) issues.push('pocket-floor');
    if (furniture.has(`${door.pocket.x},${door.pocket.y}`)) issues.push('pocket-furniture');
  } else issues.push('pocket-missing');
  for (const side of door.passage || []) {
    const x = (side.x + 0.5) * TILE;
    const y = (side.y + 0.5) * TILE;
    if (bodyBlocked(door, opened, x, y, 12)) issues.push('side-blocks');
    if (!passageWalk(door, opened, worldCenter, { x, y }, 9)) issues.push('soft-lock');
  }
  row.open = issues.some((item) => item.includes('block') || item === 'soft-lock') ? 'blocks' : 'clears';
  return row;
}

function passageWalk(door, st, a, b, radius) {
  for (let i = 0; i <= 8; i++) {
    const t = i / 8;
    const x = a.x + (b.x - a.x) * t;
    const y = a.y + (b.y - a.y) * t;
    if (bodyBlocked(door, st, x, y, radius)) return false;
  }
  return true;
}

export function formatAudit(rows) {
  const head = ['id', 'tile', 'axis', 'kind', 'hinge', 'swing', 'closed leaf (local px)', 'center', 'jambs', 'open r12', 'pocket', 'issues'];
  const body = rows.map((row) => [
    row.id,
    row.tile,
    row.axis,
    row.kind,
    row.hinge,
    row.swing,
    row.leaf,
    row.center,
    row.flush,
    row.open,
    row.pocket,
    row.issues.length ? row.issues.join(',') : '—',
  ]);
  const widths = head.map((cell, i) => Math.max(cell.length, ...body.map((line) => line[i].length)));
  const line = (cells) => cells.map((cell, i) => cell.padEnd(widths[i])).join('  ');
  return [line(head), line(widths.map((w) => '-'.repeat(w))), ...body.map(line)].join('\n');
}

export function auditSealed(spec, tiles) {
  const issues = [];
  const tile = tileAt(tiles, spec.x, spec.y);
  const west = tileAt(tiles, spec.x - 1, spec.y);
  if (tile !== 0) issues.push('not-wall');
  if (!isFloor(west)) issues.push('no-approach');
  if (spec.frame.x !== 0) issues.push('west-gap');
  if (spec.frame.y !== 0 || spec.frame.y + spec.frame.h !== TILE) issues.push('vertical-gap');
  if (spec.frame.x < 0 || spec.frame.x + spec.frame.w > TILE) issues.push('leaves-wall');
  return {
    id: spec.id,
    tile: `${spec.x},${spec.y}`,
    axis: 'face',
    kind: 'sealed',
    hinge: 'n',
    swing: '—',
    leaf: `wall x ${spec.frame.x}..${spec.frame.x + spec.frame.w} y ${spec.frame.y}..${spec.frame.y + spec.frame.h}`,
    center: '0.00',
    flush: issues.length ? 'gap' : 'flush',
    open: 'sealed',
    pocket: '—',
    issues,
  };
}

function pointInConvex(corners, x, y) {
  let sign = 0;
  for (let i = 0; i < corners.length; i++) {
    const a = corners[i];
    const b = corners[(i + 1) % corners.length];
    const cross = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
    if (Math.abs(cross) < 1e-6) continue;
    const s = Math.sign(cross);
    if (!sign) sign = s;
    else if (s !== sign) return false;
  }
  return true;
}

function polyHitsCircle(corners, cx, cy, radius) {
  if (pointInConvex(corners, cx, cy)) return true;
  for (let i = 0; i < 20; i++) {
    const a = (i / 20) * Math.PI * 2;
    if (pointInConvex(corners, cx + Math.cos(a) * (radius - 0.05), cy + Math.sin(a) * (radius - 0.05))) return true;
  }
  for (let i = 0; i < corners.length; i++) {
    const a = corners[i];
    const b = corners[(i + 1) % corners.length];
    if (Math.hypot(a.x - cx, a.y - cy) < radius) return true;
    for (let s = 1; s < 8; s++) {
      const t = s / 8;
      const x = a.x + (b.x - a.x) * t;
      const y = a.y + (b.y - a.y) * t;
      if (Math.hypot(x - cx, y - cy) < radius) return true;
    }
  }
  return false;
}

function rgb(r, g, b) {
  return `rgb(${r | 0},${g | 0},${b | 0})`;
}

function clampByte(v) {
  return Math.max(0, Math.min(255, Math.round(v)));
}

function tone(base, mul, add = 0) {
  return rgb(clampByte(base[0] * mul + add), clampByte(base[1] * mul + add), clampByte(base[2] * mul + add));
}

export function drawDoors(ctx, game, ox, oy) {
  const { world, state } = game;
  for (const door of world.doors) {
    const st = state.doors[door.id];
    if (!st) continue;
    const light = game.lightAt((door.x + 0.5) * TILE, (door.y + 0.5) * TILE);
    const sx = Math.round(door.x * TILE - ox);
    const sy = Math.round(door.y * TILE - oy);
    if (door.kind === 'overhead') drawOverhead(ctx, door, st, sx, sy, light, game.time, game);
    else drawSwingDoor(ctx, door, st, sx, sy, light, game.time, game);
  }
  for (const spec of SEALED_DOORS) {
    const light = game.lightAt((spec.x + 0.15) * TILE, (spec.y + 0.5) * TILE);
    drawSealed(ctx, spec, Math.round(spec.x * TILE - ox), Math.round(spec.y * TILE - oy), light, game.time);
  }
}

function leafShadowNudge(game, wx, wy) {
  const lights = game._shade?.lights;
  let x = 1.4;
  let y = 2.2;
  if (!lights) return { x, y, alpha: 0.34 };
  let best = 0;
  for (const light of lights) {
    if (light.kind === 'dir') {
      x += light.dirX * 1.1;
      y += light.dirY * 1.1;
      continue;
    }
    const dx = wx - light.x;
    const dy = wy - light.y;
    const dist = Math.hypot(dx, dy);
    if (!(dist > 0) || dist > (light.radius || 0) || !(light.intensity > 0)) continue;
    if (light.kind === 'cone') {
      const aimX = Math.cos(light.aim || 0);
      const aimY = Math.sin(light.aim || 0);
      const facing = (dx / dist) * aimX + (dy / dist) * aimY;
      if (facing < Math.cos(light.cone || 1)) continue;
    }
    const fall = light.intensity * (1 - dist / light.radius);
    if (fall <= best) continue;
    best = fall;
    const reach = 2.2 + Math.min(3.2, (dist / (light.z || 40)) * 6);
    x = (dx / dist) * reach;
    y = (dy / dist) * reach;
  }
  return { x, y, alpha: Math.max(0.16, Math.min(0.42, 0.2 + best * 0.45)) };
}

function drawSwingDoor(ctx, door, st, sx, sy, light, time, game) {
  drawFrame(ctx, door, sx, sy);
  drawSill(ctx, door, sx, sy);
  if (door.kind === 'heavy' && st.angle < 0.35) {
    ctx.fillStyle = '#07080c';
    ctx.fillRect(sx + door.jamb, sy + TILE - door.jamb - 1, TILE - door.jamb * 2, 1);
  }
  const wobble = st.rattle > 0 ? Math.sin(time * 48) * st.rattle * 3 : 0;
  const pose = leafPose(door, st.angle, wobble);
  const corners = leafCorners(door, st.angle, wobble).map((p) => ({ x: p.x + sx, y: p.y + sy }));
  const nudge = leafShadowNudge(game, (door.x + 0.5) * TILE, (door.y + 0.5) * TILE);
  const shadow = corners.map((p) => ({ x: p.x + nudge.x, y: p.y + nudge.y }));
  fillConvex(ctx, shadow, () => `rgba(0,0,0,${nudge.alpha})`);
  const face = FACE[door.face] || FACE.stained;
  const locked = !!(door.lock && !st.unlocked);
  fillConvex(ctx, corners, (x, y) => leafPixel(door, face, pose, x - sx, y - sy, locked, light));
}

function drawFrame(ctx, door, sx, sy) {
  const wood = hash(door.x, door.y) % 2 ? '#5c4e44' : '#4a3e36';
  const lip = '#c4a07a';
  ctx.fillStyle = wood;
  if (door.axis === 'ns') {
    ctx.fillRect(sx, sy, TILE, door.jamb);
    ctx.fillRect(sx, sy + TILE - door.jamb, TILE, door.jamb);
    ctx.fillStyle = lip;
    ctx.fillRect(sx, sy + door.jamb - 1, TILE, 1);
    ctx.fillRect(sx, sy + TILE - door.jamb, TILE, 1);
  } else {
    ctx.fillRect(sx, sy, door.jamb, TILE);
    ctx.fillRect(sx + TILE - door.jamb, sy, door.jamb, TILE);
    ctx.fillStyle = lip;
    ctx.fillRect(sx + door.jamb - 1, sy, 1, TILE);
    ctx.fillRect(sx + TILE - door.jamb, sy, 1, TILE);
  }
}

function drawSill(ctx, door, sx, sy) {
  ctx.fillStyle = 'rgba(0,0,0,0.28)';
  if (door.axis === 'ns') ctx.fillRect(sx + TILE / 2, sy + door.jamb, 1, TILE - door.jamb * 2);
  else ctx.fillRect(sx + door.jamb, sy + TILE / 2, TILE - door.jamb * 2, 1);
}

function leafPixel(door, face, pose, x, y, locked, light) {
  const rx = x - pose.hinge.x;
  const ry = y - pose.hinge.y;
  const u = (rx * pose.dir.x + ry * pose.dir.y) / pose.span;
  const v = (rx * pose.n.x + ry * pose.n.y) / pose.thick + 0.5;
  if (u < -0.02 || u > 1.02 || v < -0.02 || v > 1.02) return null;
  const faceBand = v < 0.22 || v > 0.78;
  let base = faceBand ? face.panel : face.plank;
  const grain = hash(door.x * 13 + Math.floor(u * 18), door.y * 9 + Math.floor(v * 6));
  let mul = 0.86 + (grain % 5) * 0.035;
  if (!faceBand && grain % 4 === 0) base = face.leaf;
  if (face.panel && faceBand) {
    const seam = Math.floor(u * (door.face === 'bath' ? 1 : 2));
    const local = u * (door.face === 'bath' ? 1 : 2) - seam;
    if (local > 0.14 && local < 0.86 && v > 0.08 && v < 0.92 && (v < 0.18 || v > 0.82 || local < 0.22 || local > 0.78)) {
      base = face.edge;
      mul = 0.55;
    }
  }
  if (!faceBand && (door.face === 'stained' || door.face === 'util' || door.face === 'shed' || door.face === 'heavy')) {
    if (Math.floor(v * 10) % 2 === 0) mul *= 0.9;
  }
  if (door.face === 'heavy' && !faceBand && Math.abs(v - 0.5) < 0.08 && u > 0.15 && u < 0.85) {
    base = face.edge;
    mul = 0.7;
  }
  if (u < 0.05 || u > 0.96) mul *= 0.72;
  if (v < 0.08) {
    base = face.edge;
    mul = 1;
  }
  if (v > 0.9) mul *= 0.62;
  if (u > 0.84) mul *= 1 - (u - 0.84) * 0.85;
  if (grain % 19 === 0) mul *= 0.78;
  if (u < 0.16 && v > 0.18 && v < 0.82) {
    base = face.metal;
    mul = v > 0.42 && v < 0.58 ? 0.55 : 0.95;
  }
  if (u > 0.78 && u < 0.9 && v > 0.34 && v < 0.66) {
    base = face.metal;
    mul = 1.05;
  }
  if (locked && door.lock !== 'power' && u > 0.68 && u < 0.76 && v > 0.4 && v < 0.6) {
    base = door.kind === 'shed' ? [40, 42, 46] : [70, 48, 40];
    mul = 0.9;
  }
  if (door.kind === 'shed' && locked && u > 0.9 && v > 0.3 && v < 0.7) {
    base = [48, 52, 58];
    mul = 1;
  }
  let add = 0;
  if (light > 0.42 && v < 0.2) add = 28 * Math.min(1, (light - 0.42) / 0.4);
  return tone(base, mul, add);
}

function drawOverhead(ctx, door, st, sx, sy, light, time, game) {
  drawFrame(ctx, door, sx, sy);
  const face = FACE.overhead;
  const south = curtainSouth(door, st.angle);
  const wobble = st.rattle > 0 ? Math.round(Math.sin(time * 40) * st.rattle * 2) : 0;
  const x0 = sx + door.jamb;
  const w = TILE - door.jamb * 2;
  const h = Math.max(3, Math.round(south));
  const nudge = leafShadowNudge(game, (door.x + 0.5) * TILE, (door.y + 0.5) * TILE);
  ctx.fillStyle = `rgba(0,0,0,${nudge.alpha})`;
  ctx.fillRect(x0 + nudge.x, sy + nudge.y, w, h);
  for (let y = 0; y < h; y++) {
    const v = y / h;
    const corrugation = y % 3 === 0;
    const mul = corrugation ? 0.72 : 0.95 - v * 0.18;
    const add = light > 0.45 && y < 2 ? 24 : 0;
    ctx.fillStyle = tone(corrugation ? face.panel : face.leaf, mul, add);
    ctx.fillRect(x0, sy + y + wobble, w, 1);
    if (y % 8 === 4) {
      ctx.fillStyle = tone(face.metal, 0.8);
      ctx.fillRect(x0 + 2, sy + y + wobble, 2, 2);
      ctx.fillRect(x0 + w - 4, sy + y + wobble, 2, 2);
    }
  }
  ctx.fillStyle = tone(face.edge, 0.8);
  ctx.fillRect(x0, sy, w, 3);
  ctx.fillStyle = '#1a1c20';
  ctx.fillRect(x0 + w / 2 - 5, sy + 1, 10, 6);
  const hot = door.lock === 'power' && !st.unlocked;
  ctx.fillStyle = hot ? '#8a3030' : st.angle > 0.85 ? '#6a8a48' : '#a07838';
  ctx.fillRect(x0 + w / 2 - 2, sy + 3, 3, 2);
  if (hash(door.x, Math.floor(time * 2)) % 5 === 0 && h > 8) {
    ctx.fillStyle = 'rgba(20,16,12,0.45)';
    ctx.fillRect(x0 + (hash(door.y, 3) % (w - 6)), sy + h - 4, 5, 2);
  }
}

function drawSealed(ctx, spec, sx, sy, light, time) {
  const face = FACE.sealed;
  const frame = spec.frame;
  ctx.fillStyle = '#3a3028';
  ctx.fillRect(sx + frame.x, sy + frame.y, frame.w, frame.h);
  ctx.fillStyle = tone(face.edge, 0.75);
  ctx.fillRect(sx + frame.x, sy + frame.y, frame.w, 2);
  ctx.fillRect(sx + frame.x, sy + frame.y + frame.h - 2, frame.w, 2);
  ctx.fillRect(sx + frame.x, sy, 2, TILE);
  const breath = 0.5 + 0.5 * Math.sin(time * 1.15);
  const leafX = sx + 3;
  const leafY = sy + 3;
  const leafW = 8;
  const leafH = TILE - 6;
  for (let y = 0; y < leafH; y++) {
    for (let x = 0; x < leafW; x++) {
      const u = y / leafH;
      const v = x / leafW;
      const grain = hash(spec.x + x, spec.y + y);
      let base = v > 0.25 && v < 0.75 ? face.plank : face.panel;
      let mul = 0.84 + (grain % 5) * 0.04;
      if (u > 0.12 && u < 0.46 && v > 0.2 && v < 0.8) base = face.panel;
      if (u > 0.54 && u < 0.88 && v > 0.2 && v < 0.8) base = face.panel;
      if (u < 0.08 || u > 0.93) mul *= 0.7;
      if (u > 0.9) mul *= 0.75;
      if (x === 0) {
        base = face.edge;
        mul = 0.9 + breath * 0.12;
      }
      if (u < 0.16 && v > 0.2 && v < 0.8) base = face.metal;
      if (u > 0.72 && u < 0.84 && v > 0.35 && v < 0.7) base = face.metal;
      if (u > 0.62 && u < 0.7 && v > 0.4 && v < 0.62) base = [64, 40, 36];
      const add = light > 0.4 && x === 0 ? 22 : breath * 6;
      ctx.fillStyle = tone(base, mul, add);
      ctx.fillRect(leafX + x, leafY + y, 1, 1);
    }
  }
  ctx.fillStyle = `rgba(40, 16, 12, ${0.25 + breath * 0.2})`;
  ctx.fillRect(sx, sy + TILE - 3, 2, 2);
}

function fillConvex(ctx, corners, paint) {
  let minY = Infinity;
  let maxY = -Infinity;
  for (const p of corners) {
    minY = Math.min(minY, p.y);
    maxY = Math.max(maxY, p.y);
  }
  for (let y = Math.floor(minY); y <= Math.ceil(maxY); y++) {
    const xs = [];
    for (let i = 0; i < corners.length; i++) {
      const a = corners[i];
      const b = corners[(i + 1) % corners.length];
      if (a.y === b.y) continue;
      const lo = Math.min(a.y, b.y);
      const hi = Math.max(a.y, b.y);
      const scan = y + 0.5;
      if (scan < lo || scan >= hi) continue;
      const t = (scan - a.y) / (b.y - a.y);
      xs.push(a.x + (b.x - a.x) * t);
    }
    if (xs.length < 2) continue;
    xs.sort((a, b) => a - b);
    let runX = 0;
    let runW = 0;
    let runC = null;
    const flush = () => {
      if (runW > 0 && runC) {
        ctx.fillStyle = runC;
        ctx.fillRect(runX, y, runW, 1);
      }
      runW = 0;
    };
    for (let x = Math.floor(xs[0]); x <= Math.ceil(xs[xs.length - 1]); x++) {
      const color = paint(x + 0.5, y + 0.5);
      if (!color) {
        flush();
        runC = null;
        continue;
      }
      if (color !== runC) {
        flush();
        runC = color;
        runX = x;
        runW = 1;
      } else runW += 1;
    }
    flush();
  }
}
