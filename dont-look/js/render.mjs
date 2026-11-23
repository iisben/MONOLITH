import { portrait } from './photos.mjs';
import { FURNITURE, ROOMS, floorName, roomAt, TILE } from './world.mjs';

export const VIEW_W = 416;
export const VIEW_H = 224;

const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];

const FLOOR = {
  wood: ['#6b4e3a', '#5a4030', '#7d5c44'],
  carpet: ['#4c565e', '#404a50', '#5a646c'],
  tile: ['#5e686f', '#50585e', '#6c767e'],
  concrete: ['#4e4a45', '#403c38', '#5c5852'],
  grass: ['#314228', '#28361f', '#3c4e2e'],
};

function hash(x, y) {
  let n = (x * 374761393 + y * 668265263) | 0;
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return (n ^ (n >>> 16)) >>> 0;
}

function mixHex(hex, other, t) {
  const p = (h, i) => parseInt(h.slice(1 + i * 2, 3 + i * 2), 16);
  const ch = (i) => Math.round(p(hex, i) + (p(other, i) - p(hex, i)) * t);
  return `rgb(${ch(0)}, ${ch(1)}, ${ch(2)})`;
}

function floorColor(tile, x, y, room) {
  const name = floorName(tile);
  const row = FLOOR[name] || FLOOR.concrete;
  let color = row[hash(x, y) % row.length];
  if (room === 'her') color = mixHex(color, '#163430', 0.22);
  else if (room === 'mom') color = mixHex(color, '#1c3344', 0.24);
  else if (room === 'chris') color = mixHex(color, '#3a2616', 0.14);
  else if (room === 'basement') color = mixHex(color, '#10141a', 0.28);
  return color;
}

export function render(game) {
  const ctx = game.ctx;
  const { state, world, cam } = game;
  if (!state) return;
  const shake = game.shake || 0;
  const ox = cam.x + (shake ? (Math.random() - 0.5) * shake * 5 : 0);
  const oy = cam.y + (shake ? (Math.random() - 0.5) * shake * 5 : 0);
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = '#070605';
  ctx.fillRect(0, 0, VIEW_W, VIEW_H);

  const x0 = Math.max(0, Math.floor(ox / TILE) - 1);
  const y0 = Math.max(0, Math.floor(oy / TILE) - 1);
  const x1 = Math.min(world.tiles[0].length - 1, Math.floor((ox + VIEW_W) / TILE) + 1);
  const y1 = Math.min(world.tiles.length - 1, Math.floor((oy + VIEW_H) / TILE) + 1);

  for (let y = y0; y <= y1; y++) {
    for (let x = x0; x <= x1; x++) {
      const tile = world.tiles[y][x];
      const sx = Math.round(x * TILE - ox);
      const sy = Math.round(y * TILE - oy);
      if (tile === 0) {
        ctx.fillStyle = hash(x, y) % 2 ? '#5c4e44' : '#4a3e36';
        ctx.fillRect(sx, sy, TILE, TILE);
        ctx.fillStyle = '#1a1512';
        ctx.fillRect(sx + 4, sy, TILE - 4, 1);
        ctx.fillRect(sx, sy + 8, TILE, 1);
        ctx.fillRect(sx, sy + 16, TILE, 1);
        ctx.fillRect(sx, sy + 24, TILE, 1);
        const lip = '#c4a07a';
        const face = (nx, ny, rx, ry, rw, rh) => {
          const neighbor = world.tiles[ny]?.[nx];
          if (!neighbor) return;
          ctx.fillStyle = lip;
          ctx.fillRect(sx + rx, sy + ry, rw, rh);
        };
        face(x, y - 1, 0, 0, TILE, 5);
        face(x, y + 1, 0, TILE - 6, TILE, 6);
        face(x - 1, y, 0, 0, 5, TILE);
        face(x + 1, y, TILE - 5, 0, 5, TILE);
        continue;
      }
      if (tile === 9) {
        const door = world.doorAt.get(`${x},${y}`);
        const open = state.doors[door.id].open;
        ctx.fillStyle = floorColor(door.floor, x, y, roomAt(x, y));
        ctx.fillRect(sx, sy, TILE, TILE);
        if (!open) drawDoor(ctx, sx, sy, door, state);
        continue;
      }
      ctx.fillStyle = floorColor(tile, x, y, roomAt(x, y));
      ctx.fillRect(sx, sy, TILE, TILE);
      ctx.fillStyle = 'rgba(0,0,0,0.22)';
      ctx.fillRect(sx, sy + TILE - 1, TILE, 1);
      ctx.fillRect(sx + TILE - 1, sy, 1, TILE);
      if (hash(x, y) % 11 === 0) {
        ctx.fillStyle = 'rgba(0,0,0,0.18)';
        ctx.fillRect(sx + (hash(y, x) % 20), sy + (hash(x + 3, y) % 20), 3, 2);
      }
    }
  }

  for (const piece of FURNITURE) {
    if (piece.x + piece.w < x0 || piece.x > x1 || piece.y + piece.h < y0 || piece.y > y1) continue;
    drawFurniture(ctx, piece, ox, oy, state, game);
  }

  for (const decal of state.decals) {
    const sx = Math.round(decal.x - ox);
    const sy = Math.round(decal.y - oy);
    ctx.fillStyle = 'rgba(70, 16, 14, 0.85)';
    ctx.fillRect(sx - 7, sy - 4, 14, 8);
  }

  for (const enemy of state.enemies) {
    if (!enemy.alive) continue;
    drawEnemy(ctx, enemy, ox, oy, game);
  }

  for (const prop of world.props) drawProp(ctx, prop, ox, oy, state);

  for (const drop of state.dropped) {
    drawGlint(ctx, drop.x - ox, drop.y - oy, '#e4c07a', game.time);
  }

  drawDarkness(ctx, game, ox, oy);
  drawCloseSilhouettes(ctx, game, ox, oy);
  drawPlayer(ctx, state.player, ox, oy, game);
  drawMotes(ctx, game, ox, oy);

  if (game.hurtFlash > 0) {
    ctx.fillStyle = `rgba(120, 24, 18, ${game.hurtFlash * 0.55})`;
    ctx.fillRect(0, 0, VIEW_W, VIEW_H);
  }
  if (game.flash > 0) {
    ctx.fillStyle = `rgba(232, 220, 190, ${game.flash * (game.settings.reduceFlash ? 0.25 : 0.7)})`;
    ctx.fillRect(0, 0, VIEW_W, VIEW_H);
  }
}

function drawFurniture(ctx, piece, ox, oy, state, game) {
  const sx = Math.round(piece.x * TILE - ox);
  const sy = Math.round(piece.y * TILE - oy);
  const w = piece.w * TILE;
  const h = piece.h * TILE;
  if (piece.kind === 'car') {
    ctx.fillStyle = '#343a42';
    ctx.fillRect(sx + 4, sy + 12, w - 8, h - 20);
    ctx.fillStyle = '#14161a';
    ctx.fillRect(sx + 10, sy + h - 14, 14, 8);
    ctx.fillRect(sx + w - 24, sy + h - 14, 14, 8);
    ctx.fillStyle = '#9aafbc';
    ctx.fillRect(sx + 14, sy + 14, w * 0.3, 26);
    ctx.fillRect(sx + w * 0.52, sy + 14, w * 0.34, 26);
    const light = game.lightAt((piece.x + 2.2) * TILE, (piece.y + 0.85) * TILE);
    if (light > 0.12) {
      const face = portrait('mom', {
        crop: 'face',
        size: 32,
        dark: 0.12,
        corrupt: state.flags.tookFuse ? 0.55 : 0.06,
        grin: state.flags.tookFuse ? 0.28 : 0,
      });
      const fw = 22;
      const fh = Math.round(fw * (face.height / face.width));
      ctx.imageSmoothingEnabled = false;
      ctx.globalAlpha = 0.95;
      ctx.drawImage(face, sx + w * 0.56, sy + 15, fw, Math.min(fh, 24));
      ctx.globalAlpha = 1;
    }
    return;
  }
  const colors = {
    bed: '#3a3338',
    sweater: '#6f9aab',
    couch: '#3d342c',
    table: '#4a3828',
    counter: '#3e4348',
    tub: '#4a5558',
    bench: '#3a342c',
    tanks: '#1e3a34',
    shelf: '#4a3828',
  };
  ctx.fillStyle = colors[piece.kind] || '#333';
  ctx.fillRect(sx + 1, sy + 1, w - 2, h - 2);
  ctx.fillStyle = 'rgba(255,255,255,0.06)';
  ctx.fillRect(sx + 2, sy + 2, w - 4, 3);
  if (piece.kind === 'tanks') {
    ctx.fillStyle = 'rgba(120, 220, 190, 0.25)';
    for (let i = 0; i < piece.w; i++) ctx.fillRect(sx + i * TILE + 6, sy + 8, 16, h - 16);
    if (Math.sin(game.time * 3) > 0.65 && !game.settings.reduceFlash) {
      ctx.fillStyle = 'rgba(220,220,220,0.35)';
      ctx.fillRect(sx + 10, sy + 14, 8, 3);
    }
  }
  if (piece.kind === 'sweater') {
    ctx.fillStyle = '#d7e4ea';
    ctx.fillRect(sx + 8, sy + 6, w - 16, 4);
  }
}

function drawDoor(ctx, sx, sy, door, state) {
  const locked = door.lock && !state.doors[door.id].unlocked && !(door.lock === 'power' && state.flags.powerOn);
  ctx.fillStyle = locked ? '#4a2a28' : '#5a4632';
  ctx.fillRect(sx + 6, sy + 4, TILE - 12, TILE - 8);
  ctx.fillStyle = locked ? '#e07068' : '#e4c07a';
  ctx.fillRect(sx + TILE - 12, sy + 14, 3, 3);
}

function drawProp(ctx, prop, ox, oy, state) {
  if (state.taken[prop.id]) return;
  if (prop.kind === 'container' && !(state.containers[prop.id] || []).length) return;
  const x = Math.round((prop.x + 0.5) * TILE - ox);
  const y = Math.round((prop.y + 0.5) * TILE - oy);
  if (prop.kind === 'flashlight') {
    ctx.fillStyle = '#d7c27a';
    ctx.fillRect(x - 5, y - 2, 10, 4);
    return;
  }
  if (prop.kind === 'battery') {
    ctx.fillStyle = '#6f8f62';
    ctx.fillRect(x - 4, y - 5, 8, 10);
    return;
  }
  if (prop.kind === 'note') {
    ctx.fillStyle = '#d9d0c0';
    ctx.fillRect(x - 5, y - 6, 10, 12);
    ctx.fillStyle = '#8a7b68';
    ctx.fillRect(x - 3, y - 3, 6, 1);
    return;
  }
  if (prop.kind === 'inspect') {
    ctx.fillStyle = '#c4b48a';
    ctx.fillRect(x - 6, y - 7, 12, 10);
    ctx.fillStyle = '#1a1612';
    ctx.fillRect(x - 4, y - 5, 8, 6);
    return;
  }
  if (prop.kind === 'pickup') {
    const color = {
      weapon: '#c8c2b4',
      ammo: '#c2a15a',
      heal: '#d7d2c8',
      film: '#222',
      tape: '#b7b0a4',
      key: '#e0b84e',
      quest: '#d07a3a',
      camera: '#22262c',
    };
    // Color is chosen by the caller via item type in draw; keep a warm default and a key glint.
    ctx.fillStyle = prop.item?.startsWith('key') ? '#e0b84e' : color.quest;
    if (prop.item === 'bandage' || prop.item === 'pills') ctx.fillStyle = '#e6e0d4';
    if (prop.item === 'ammo') ctx.fillStyle = '#c2a15a';
    if (prop.item === 'stick' || prop.item === 'knife' || prop.item === 'pistol') ctx.fillStyle = '#c8c2b4';
    if (prop.item === 'film') ctx.fillStyle = '#1c1c1c';
    if (prop.item === 'camera') ctx.fillStyle = '#2a2e34';
    ctx.fillRect(x - 4, y - 4, 8, 8);
    return;
  }
  if (prop.kind === 'save' || prop.kind === 'stash' || prop.kind === 'board') {
    ctx.fillStyle = prop.kind === 'board' ? '#d9d0c0' : '#6a5a44';
    ctx.fillRect(x - 6, y - 5, 12, 8);
  }
}

function drawGlint(ctx, x, y, color, time) {
  if (Math.sin(time * 3.2 + x * 0.2) < -0.35) return;
  const px = Math.round(x);
  const py = Math.round(y);
  ctx.fillStyle = color;
  ctx.fillRect(px - 1, py - 1, 3, 3);
  ctx.fillStyle = '#fff4d2';
  ctx.fillRect(px, py, 1, 1);
}

function drawPlayer(ctx, player, ox, oy, game) {
  const time = game.time;
  const x = Math.round(player.x - ox);
  const y = Math.round(player.y - oy);
  if (player.iframes > 0 && Math.sin(time * 40) > 0) return;
  const rate = player.running ? 16 : player.sneaking ? 7 : 11;
  const swing = player.moving ? Math.sin(time * rate) : 0;
  const bob = player.moving ? Math.round(Math.abs(swing)) : 0;
  ctx.fillStyle = 'rgba(0,0,0,0.4)';
  ctx.beginPath();
  ctx.ellipse(x, y + 7, 7, 3, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = '#8a8172';
  ctx.fillRect(x - 5, y + 1 - bob, 3, 6 + Math.round(swing * 2));
  ctx.fillRect(x + 2, y + 1 - bob, 3, 6 - Math.round(swing * 2));
  ctx.fillStyle = '#1c2744';
  ctx.fillRect(x - 6, y - 7 - bob, 12, 8);
  ctx.fillStyle = '#d5dbe6';
  ctx.fillRect(x - 6, y - 7 - bob, 3, 8);
  ctx.fillRect(x - 1, y - 5 - bob, 2, 6);
  ctx.fillRect(x + 3, y - 6 - bob, 2, 5);
  ctx.fillStyle = '#c49a78';
  ctx.fillRect(x - 3, y - 11 - bob, 6, 5);
  ctx.fillStyle = '#241c18';
  ctx.fillRect(x - 4, y - 13 - bob, 8, 3);
  const aim = 12;
  ctx.strokeStyle = 'rgba(228,192,122,0.85)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(x, y - 4);
  ctx.lineTo(x + Math.cos(player.facing) * aim, y - 4 + Math.sin(player.facing) * aim);
  ctx.stroke();
  drawHeld(ctx, game, x, y - bob, player);
}

function drawHeld(ctx, game, x, y, player) {
  const slot = game.state.inv[game.state.equipped];
  if (!slot) return;
  const reach = slot.id === 'pistol' ? 16 : slot.id === 'stick' ? 18 : 12;
  const fx = Math.cos(player.facing);
  const fy = Math.sin(player.facing);
  const tipX = x + fx * reach;
  const tipY = y + fy * reach;
  if (player.atk > 0) {
    ctx.strokeStyle = 'rgba(228, 192, 122, 0.55)';
    ctx.beginPath();
    ctx.arc(x, y, reach - 2, player.facing - 0.8, player.facing + 0.8);
    ctx.stroke();
  }
  if (slot.id === 'stick') {
    ctx.strokeStyle = '#c4a06a';
    ctx.beginPath();
    ctx.moveTo(x + fx * 4, y + fy * 4);
    ctx.lineTo(tipX, tipY);
    ctx.stroke();
  } else if (slot.id === 'knife') {
    ctx.strokeStyle = '#d5dbe6';
    ctx.beginPath();
    ctx.moveTo(x + fx * 5, y + fy * 5);
    ctx.lineTo(tipX, tipY);
    ctx.stroke();
  } else if (slot.id === 'pistol') {
    ctx.fillStyle = '#1a1c20';
    ctx.fillRect(Math.round(tipX) - 2, Math.round(tipY) - 2, 4, 3);
  } else if (slot.id === 'camera') {
    ctx.fillStyle = '#2a2e34';
    ctx.fillRect(Math.round(x + fx * 8) - 3, Math.round(y + fy * 8) - 3, 6, 5);
    ctx.fillStyle = '#9aafbc';
    ctx.fillRect(Math.round(x + fx * 10), Math.round(y + fy * 10), 2, 2);
  }
}

function drawEnemy(ctx, enemy, ox, oy, game) {
  const x = Math.round(enemy.x - ox);
  const y = Math.round(enemy.y - oy);
  if (enemy.downUntil > game.time) {
    ctx.fillStyle = '#e8e4dc';
    ctx.fillRect(x - 6, y, 12, 2);
    return;
  }
  if (!enemy.awake && enemy.silent) {
    ctx.fillStyle = '#7eafc0';
    ctx.fillRect(x - 7, y - 3, 14, 8);
    ctx.fillStyle = '#d5e4ea';
    ctx.fillRect(x - 4, y - 1, 8, 2);
    return;
  }
  if (!enemy.awake && enemy.wake === 'flag') {
    ctx.fillStyle = '#2a2724';
    ctx.fillRect(x - 5, y - 2, 10, 6);
    return;
  }
  const bob = Math.sin(game.time * 8 + enemy.x) > 0 ? 0 : 1;
  if (enemy.kind === 'grin') {
    const light = game.lightAt(enemy.x, enemy.y);
    ctx.fillStyle = '#0c0b0c';
    ctx.fillRect(x - 3, y - 6, 6, 14);
    if (light > 0.16) {
      const face = portrait('grin', { crop: 'face', size: 28, dark: 1 - light, corrupt: 0.15 });
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(face, x - 12, y - 26, 24, Math.round(24 * (face.height / face.width)));
    } else {
      ctx.fillStyle = '#f4f1ea';
      const smile = 4 + Math.floor(Math.sin(game.time * 6) * 2 + 2);
      ctx.fillRect(x - smile + 2, y - 10 + bob, smile, 2);
    }
    return;
  }
  const stun = enemy.stunUntil > game.time;
  ctx.fillStyle = stun ? '#6a6a62' : '#4e4c46';
  ctx.fillRect(x - 4, y - 8, 8, 9);
  ctx.fillStyle = '#2a2926';
  ctx.fillRect(x - 5, y + bob, 3, 5);
  ctx.fillRect(x + 2, y + (1 - bob), 3, 5);
  if (enemy.state === 'chase') {
    ctx.fillStyle = '#f2efe8';
    ctx.fillRect(x - 2, y - 2, 4, 1);
  }
}

function drawCloseSilhouettes(ctx, game, ox, oy) {
  for (const enemy of game.state.enemies) {
    if (!enemy.alive || !enemy.awake || enemy.downUntil > game.time) continue;
    const d = Math.hypot(enemy.x - game.state.player.x, enemy.y - game.state.player.y);
    if (d > 40) continue;
    const light = game.lightAt(enemy.x, enemy.y);
    if (light > 0.22) continue;
    ctx.globalAlpha = 0.45;
    drawEnemy(ctx, enemy, ox, oy, game);
    ctx.globalAlpha = 1;
  }
  // Item glints punch through the dark so a careful sweep can find them.
  const time = game.time;
  for (const prop of game.world.props) {
    if (game.state.taken[prop.id]) continue;
    if (prop.kind === 'container' && !(game.state.containers[prop.id] || []).length) continue;
    if (prop.kind === 'board' || prop.kind === 'save' || prop.kind === 'stash' || prop.kind === 'car') continue;
    const x = (prop.x + 0.5) * TILE;
    const y = (prop.y + 0.5) * TILE;
    if (game.lightAt(x, y) > 0.35) continue;
    if (Math.hypot(x - game.state.player.x, y - game.state.player.y) > 210) continue;
    if (!game.los(game.state.player.x, game.state.player.y, x, y)) continue;
    drawGlint(ctx, x - ox, y - oy, '#e4c07a', time + prop.x);
  }
  const player = game.state.player;
  for (const door of game.world.doors) {
    const x = (door.x + 0.5) * TILE;
    const y = (door.y + 0.5) * TILE;
    const distance = Math.hypot(x - player.x, y - player.y);
    if (distance > 160 || distance < 18) continue;
    if (game.lightAt(x, y) > 0.5) continue;
    if (!game.los(player.x, player.y, x, y)) continue;
    const st = game.state.doors[door.id];
    const locked = door.lock && !st.unlocked && !(door.lock === 'power' && game.state.flags.powerOn);
    drawGlint(ctx, x - ox, y - oy, locked ? '#c45048' : '#e4c07a', time + door.y);
  }
}

function drawMotes(ctx, game, ox, oy) {
  const p = game.state.player;
  if (!game.state.hasLight || !p.lightOn || p.battery <= 0) return;
  ctx.fillStyle = 'rgba(232, 214, 170, 0.45)';
  for (let i = 0; i < 10; i++) {
    const a = game.time * 0.35 + i;
    const r = 18 + ((i * 17) % 80);
    const x = p.x + Math.cos(p.facing + Math.sin(a) * 0.35) * r - ox;
    const y = p.y + Math.sin(p.facing + Math.sin(a + 1) * 0.35) * r - oy;
    if (game.lightAt(p.x + (x + ox - p.x), p.y + (y + oy - p.y)) < 0.4) continue;
    ctx.fillRect(Math.round(x), Math.round(y + Math.sin(game.time * 2 + i) * 2), 1, 1);
  }
}

function drawDarkness(ctx, game, ox, oy) {
  const cell = 4;
  for (let y = 0; y < VIEW_H; y += cell) {
    for (let x = 0; x < VIEW_W; x += cell) {
      const wx = ox + x + cell / 2;
      const wy = oy + y + cell / 2;
      let light = game.lightAt(wx, wy);
      const b = BAYER[((y >> 2) & 3) * 4 + ((x >> 2) & 3)] / 16;
      if (light < 0.92) light = light + (b - 0.5) * 0.035;
      const clamped = Math.max(0, Math.min(1, light));
      const dark = (1 - clamped) ** 1.15;
      if (dark < 0.04) continue;
      const cool = Math.round(8 + (1 - clamped) * 6);
      ctx.fillStyle = `rgba(4, 5, ${cool}, ${dark})`;
      ctx.fillRect(x, y, cell, cell);
    }
  }
}

export function drawMap(ctx, game, w, h) {
  const { state } = game;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#120f0c';
  ctx.fillRect(0, 0, w, h);
  const rooms = ROOMS;
  let minX = 99;
  let minY = 99;
  let maxX = 0;
  let maxY = 0;
  for (const room of Object.values(rooms)) {
    minX = Math.min(minX, room.x);
    minY = Math.min(minY, room.y);
    maxX = Math.max(maxX, room.x + room.w);
    maxY = Math.max(maxY, room.y + room.h);
  }
  const pad = 18;
  const scale = Math.min((w - pad * 2) / (maxX - minX), (h - pad * 2) / (maxY - minY));
  const show = new Set(Object.keys(state.seen));
  for (const door of game.world.doors) {
    const ids = [];
    for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
      const id = roomAt(door.x + dx, door.y + dy);
      if (id) ids.push(id);
    }
    if (ids.some((id) => state.seen[id])) ids.forEach((id) => show.add(id));
  }
  ctx.font = '12px Courier New, monospace';
  ctx.lineWidth = 1;
  for (const [id, room] of Object.entries(rooms)) {
    if (!show.has(id)) continue;
    const x = pad + (room.x - minX) * scale;
    const y = pad + (room.y - minY) * scale;
    const rw = room.w * scale;
    const rh = room.h * scale;
    const seen = Boolean(state.seen[id]);
    ctx.globalAlpha = seen ? 1 : 0.45;
    ctx.fillStyle = seen ? '#2a241e' : 'transparent';
    ctx.strokeStyle = '#8d734c';
    ctx.fillRect(x, y, rw, rh);
    ctx.strokeRect(x + 0.5, y + 0.5, rw, rh);
    if (seen) {
      ctx.fillStyle = '#e4c07a';
      ctx.fillText(room.name, x + 4, y + 13);
    }
    ctx.globalAlpha = 1;
  }
  for (const door of game.world.doors) {
    const ids = [[1, 0], [-1, 0], [0, 1], [0, -1]]
      .map(([dx, dy]) => roomAt(door.x + dx, door.y + dy))
      .filter(Boolean);
    if (!ids.some((id) => state.seen[id])) continue;
    const x = pad + (door.x + 0.5 - minX) * scale;
    const y = pad + (door.y + 0.5 - minY) * scale;
    const st = state.doors[door.id];
    const locked = door.lock && !st.unlocked && !(door.lock === 'power' && state.flags.powerOn);
    ctx.fillStyle = locked ? '#c45048' : st.open ? '#8d734c' : '#e4c07a';
    ctx.fillRect(x - 2, y - 2, 4, 4);
  }
  const px = pad + (state.player.x / TILE - minX) * scale;
  const py = pad + (state.player.y / TILE - minY) * scale;
  ctx.fillStyle = '#f0d7a0';
  ctx.beginPath();
  ctx.arc(px, py, 4, 0, Math.PI * 2);
  ctx.fill();
  ctx.beginPath();
  ctx.moveTo(px, py);
  ctx.lineTo(px + Math.cos(state.player.facing) * 12, py + Math.sin(state.player.facing) * 12);
  ctx.strokeStyle = '#f0d7a0';
  ctx.stroke();
}
