import {
  actorSprite, doorSprite, floorSprite, floorStyle, footSprite, furnitureSprite,
  markSprite, moteSprite, scorchSprite, trimSprite, wallSprite, wallStyleAt, weaponSprite,
} from './atlas.mjs';
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

function paintFloor(ctx, tile, x, y, room, sx, sy) {
  const name = typeof tile === 'string' ? tile : floorName(tile);
  const img = floorSprite(floorStyle(name, room), hash(x, y) % 4);
  ctx.drawImage(img, sx, sy, TILE, TILE);
}

function drawBloom(ctx, game, ox, oy) {
  const p = game.state.player;
  if (p.lightOn && game.state.hasLight && p.battery > 0) {
    const gx = p.x + Math.cos(p.facing) * 36 - ox;
    const gy = p.y + Math.sin(p.facing) * 36 - oy;
    const grad = ctx.createRadialGradient?.(gx, gy, 4, gx, gy, 70);
    if (grad?.addColorStop) {
      grad.addColorStop(0, 'rgba(255, 214, 150, 0.18)');
      grad.addColorStop(1, 'rgba(255, 214, 150, 0)');
      ctx.fillStyle = grad;
      ctx.fillRect(gx - 70, gy - 70, 140, 140);
    }
  }
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
  const shake = game.settings?.shake === false ? 0 : (game.shake || 0);
  const kickX = game.kickX || 0;
  const kickY = game.kickY || 0;
  const ox = cam.x + (shake ? (Math.random() - 0.5) * shake * 5 : 0) + kickX;
  const oy = cam.y + (shake ? (Math.random() - 0.5) * shake * 5 : 0) + kickY;
  const zoom = game.settings?.detail === false ? 1 : (cam.zoom || 1);
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = '#070605';
  ctx.fillRect(0, 0, VIEW_W, VIEW_H);
  ctx.save();
  if (zoom !== 1) {
    ctx.translate(VIEW_W / 2, VIEW_H / 2);
    ctx.scale(zoom, zoom);
    ctx.translate(-VIEW_W / 2, -VIEW_H / 2);
  }

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
        const near = [];
        for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
          const id = roomAt(x + dx, y + dy);
          if (id) near.push(id);
        }
        ctx.drawImage(wallSprite(wallStyleAt(near), hash(x, y) % 4), sx, sy, TILE, TILE);
        const trim = trimSprite();
        const face = (nx, ny, rx, ry, rw, rh) => {
          const neighbor = world.tiles[ny]?.[nx];
          if (!neighbor) return;
          ctx.drawImage(trim, sx + rx, sy + ry, rw, rh);
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
        paintFloor(ctx, door.floor, x, y, roomAt(x, y), sx, sy);
        drawDoor(ctx, sx, sy, door, state, world, x, y);
        continue;
      }
      paintFloor(ctx, tile, x, y, roomAt(x, y), sx, sy);
    }
  }

  for (const trace of state.traces || []) {
    const img = trace.kind === 'foot' ? footSprite() : scorchSprite();
    ctx.globalAlpha = Math.max(0, 1 - trace.age / trace.life) * 0.85;
    ctx.drawImage(img, Math.round(trace.x - ox - img.width / 2), Math.round(trace.y - oy - img.height / 2));
  }
  ctx.globalAlpha = 1;

  for (const piece of FURNITURE) {
    if (piece.x + piece.w < x0 || piece.x > x1 || piece.y + piece.h < y0 || piece.y > y1) continue;
    drawFurniture(ctx, piece, ox, oy, state, game);
  }

  for (const decal of state.decals) {
    const img = scorchSprite();
    ctx.drawImage(img, Math.round(decal.x - ox - 8), Math.round(decal.y - oy - 8));
  }

  for (const enemy of state.enemies) {
    if (!enemy.alive) continue;
    drawEnemy(ctx, enemy, ox, oy, game);
  }

  for (const prop of world.props) drawProp(ctx, prop, ox, oy, state, game.time);

  for (const drop of state.dropped) {
    drawGlint(ctx, drop.x - ox, drop.y - oy, '#e4c07a', game.time);
  }

  if (game.settings?.detail !== false && game.settings?.post !== false) drawBloom(ctx, game, ox, oy);
  drawDarkness(ctx, game, ox, oy);
  drawCloseSilhouettes(ctx, game, ox, oy);
  drawPlayer(ctx, state.player, ox, oy, game);
  game.fx?.draw(ctx, ox, oy);
  drawMotes(ctx, game, ox, oy);
  ctx.restore();
  if (game.veil > 0.02) {
    ctx.fillStyle = `rgba(0, 0, 0, ${Math.min(0.55, game.veil)})`;
    ctx.fillRect(0, 0, VIEW_W, VIEW_H);
  }
  if (game.settings.post !== false) {
    ctx.fillStyle = 'rgba(36, 22, 14, 0.1)';
    ctx.fillRect(0, 0, VIEW_W, VIEW_H);
  }

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
  const sprite = furnitureSprite(piece);
  ctx.drawImage(sprite, sx, sy);
  if (piece.kind !== 'car') return;
  const light = game.lightAt((piece.x + 2.2) * TILE, (piece.y + 0.85) * TILE);
  if (light <= 0.12) return;
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
  ctx.drawImage(face, sx + piece.w * TILE * 0.56, sy + 15, fw, Math.min(fh, 24));
  ctx.globalAlpha = 1;
}

function drawDoor(ctx, sx, sy, door, state, world, x, y) {
  const st = state.doors[door.id];
  const locked = door.lock && !st.unlocked && !(door.lock === 'power' && state.flags.powerOn);
  const swing = st.swing ?? (st.open ? 1 : 0);
  const rattle = st.rattle > 0 ? Math.round(Math.sin(st.rattle * 48) * 1) : 0;
  const leaf = doorSprite(door.id, !!locked);
  const across = (world.tiles[y]?.[x - 1] > 0) && (world.tiles[y]?.[x + 1] > 0);
  if (swing > 0.82) {
    ctx.drawImage(trimSprite(), sx + (across ? 0 : TILE - 5), sy + (across ? TILE - 5 : 0), across ? TILE : 5, across ? 5 : TILE);
    return;
  }
  ctx.save();
  ctx.beginPath();
  ctx.rect(sx, sy, TILE, TILE);
  ctx.clip();
  if (swing < 0.08) {
    ctx.drawImage(leaf, sx + rattle, sy, TILE, TILE);
  } else {
    ctx.translate(across ? sx + 2 : sx + TILE / 2, across ? sy + TILE / 2 : sy + 2);
    ctx.rotate((across ? -1 : 1) * swing * 1.05);
    ctx.drawImage(leaf, 0, across ? -TILE / 2 : 0, TILE - 2, TILE - 2);
  }
  ctx.restore();
}

function drawProp(ctx, prop, ox, oy, state, time = 0) {
  if (state.taken[prop.id]) return;
  if (prop.kind === 'car') return;
  if (prop.kind === 'container' && !(state.containers[prop.id] || []).length) return;
  const x = Math.round((prop.x + 0.5) * TILE - ox);
  const y = Math.round((prop.y + 0.5) * TILE - oy);
  const lift = Math.round(Math.sin(time * 2.2 + prop.x * 0.4) * 1);
  ctx.fillStyle = 'rgba(0,0,0,0.45)';
  ctx.beginPath();
  ctx.ellipse(x, y + 2, 8, 3, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.drawImage(markSprite(prop), x - 12, y - 16 + lift);
}

function drawGlint(ctx, x, y, color, time) {
  if (Math.sin(time * 3.2 + x * 0.2) < -0.2) return;
  ctx.globalAlpha = 0.85;
  ctx.drawImage(moteSprite(), Math.round(x) - 3, Math.round(y) - 3);
  ctx.globalAlpha = 1;
  void color;
}

function drawShadow(ctx, sx, sy, game, wx, wy, isPlayer) {
  ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';
  ctx.beginPath();
  ctx.ellipse(sx, sy + 6, 10, 3.4, 0, 0, Math.PI * 2);
  ctx.fill();
  const p = game.state.player;
  let lx = p.x;
  let ly = p.y;
  if (isPlayer && p.lightOn && game.state.hasLight) {
    lx = p.x + Math.cos(p.facing) * 28;
    ly = p.y + Math.sin(p.facing) * 28;
  }
  const ang = Math.atan2(wy - ly, wx - lx);
  const stretch = Math.min(16, 7 + Math.hypot(wx - lx, wy - ly) * 0.03);
  const flick = 0.78 + 0.22 * Math.abs(Math.sin(game.time * 2.6));
  ctx.save();
  ctx.translate(sx, sy + 4);
  ctx.rotate(ang);
  ctx.fillStyle = `rgba(0, 0, 0, ${0.24 * flick})`;
  ctx.beginPath();
  ctx.ellipse(stretch * 0.55, 0, stretch, 3.2, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

function drawPlayer(ctx, player, ox, oy, game) {
  const time = game.time;
  const x = Math.round(player.x - ox);
  const y = Math.round(player.y - oy);
  if (player.iframes > 0 && Math.sin(time * 40) > 0) return;
  const rate = player.running ? 14 : player.sneaking ? 6 : 9;
  const frame = player.moving ? Math.floor(time * rate) % 4 : 0;
  const bob = player.moving ? (frame % 2) : 0;
  drawShadow(ctx, x, y, game, player.x, player.y, true);
  const body = actorSprite('chris', frame);
  ctx.drawImage(body, x - 20, y - 40 - bob);
  const slot = game.state.inv[game.state.equipped];
  if (!slot) return;
  const gun = weaponSprite(slot.id);
  ctx.save();
  ctx.translate(x, y - 10 - bob);
  ctx.rotate(player.facing);
  const reach = slot.id === 'stick' ? 8 : 4;
  ctx.drawImage(gun, reach, -10);
  if (player.atk > 0.05) {
    ctx.globalAlpha = Math.min(0.65, player.atk * 2);
    ctx.drawImage(gun, reach + 4, -14);
    ctx.globalAlpha = 1;
  }
  ctx.restore();
}

function drawEnemy(ctx, enemy, ox, oy, game) {
  const x = Math.round(enemy.x - ox);
  const y = Math.round(enemy.y - oy);
  const frame = Math.floor(game.time * (enemy.state === 'chase' ? 8 : 3) + enemy.x) % 4;
  if (enemy.wind > 0) {
    const ang = Math.atan2(game.state.player.y - enemy.y, game.state.player.x - enemy.x);
    ctx.strokeStyle = 'rgba(196, 80, 72, 0.85)';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(x, y);
    ctx.lineTo(x + Math.cos(ang) * 18, y + Math.sin(ang) * 10);
    ctx.stroke();
  }
  let id = enemy.id;
  if (enemy.downUntil > game.time) id = 'down';
  else if (!enemy.awake && enemy.silent) id = 'sleep-mom';
  else if (!enemy.awake && enemy.wake === 'flag') id = 'sleep-lump';
  else if (enemy.kind === 'grin') id = 'grin';
  const body = actorSprite(id, frame);
  const light = game.lightAt(enemy.x, enemy.y);
  drawShadow(ctx, x, y, game, enemy.x, enemy.y, false);
  ctx.save();
  ctx.translate(x, y);
  ctx.drawImage(body, -20, -36);
  if (light > 0.4) {
    ctx.globalAlpha = (light - 0.4) * 0.55;
    ctx.drawImage(body, -21, -37);
    ctx.globalAlpha = 1;
  }
  if (enemy.kind === 'grin' && enemy.downUntil <= game.time && enemy.awake) {
    if (light > 0.16) {
      const face = portrait('grin', { crop: 'face', size: 28, dark: 1 - light, corrupt: 0.15 });
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(face, -12, -46, 24, Math.round(24 * (face.height / face.width)));
    } else {
      const smile = 5 + Math.floor(Math.sin(game.time * 6) * 2 + 2);
      ctx.fillStyle = '#f4f1ea';
      ctx.fillRect(-smile / 2, -32, smile, 2);
    }
  }
  ctx.restore();
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
  const mote = moteSprite();
  for (let i = 0; i < 22; i += 1) {
    const a = game.time * 0.22 + i * 0.7;
    const r = 14 + ((i * 19) % 110);
    const wx = p.x + Math.cos(p.facing + Math.sin(a) * 0.4) * r;
    const wy = p.y + Math.sin(p.facing + Math.sin(a + 1) * 0.4) * r;
    if (game.lightAt(wx, wy) < 0.35) continue;
    const drift = Math.sin(game.time * 1.4 + i) * 3;
    ctx.globalAlpha = 0.35 + (i % 5) * 0.1;
    ctx.drawImage(mote, Math.round(wx - ox), Math.round(wy - oy + drift));
  }
  ctx.globalAlpha = 1;
}

function drawDarkness(ctx, game, ox, oy) {
  const cell = game.settings?.detail === false ? 8 : 2;
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
