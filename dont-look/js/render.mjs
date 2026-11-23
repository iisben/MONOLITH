import {
  beginShade,
  drawActorShadows,
  drawContactShadow,
  drawEnvironmentShade,
  drawLightMap,
  shadeIllumination,
} from './lighting.mjs';
import { artIdForProp, drawWorldSprite, finderColor, shadowFor } from './art.mjs';
import { portrait } from './photos.mjs';
import { drawDoors, interactPoint } from './doors.mjs';
import { FURNITURE, ROOMS, floorName, roomAt, TILE } from './world.mjs';

export const VIEW_W = 416;
export const VIEW_H = 224;

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
  beginShade(game, ox, oy);

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
        ctx.fillStyle = floorColor(door.floor, x, y, roomAt(x, y));
        ctx.fillRect(sx, sy, TILE, TILE);
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

  drawEnvironmentShade(ctx, game, ox, oy, x0, y0, x1, y1);

  for (const piece of FURNITURE) {
    if (piece.x + piece.w < x0 || piece.x > x1 || piece.y + piece.h < y0 || piece.y > y1) continue;
    drawFurniture(ctx, piece, ox, oy, state, game);
  }

  drawDoors(ctx, game, ox, oy);

  for (const decal of state.decals) {
    const sx = Math.round(decal.x - ox);
    const sy = Math.round(decal.y - oy);
    ctx.fillStyle = 'rgba(70, 16, 14, 0.85)';
    ctx.fillRect(sx - 7, sy - 4, 14, 8);
  }

  for (const enemy of state.enemies) {
    if (!enemy.alive) continue;
    const kind = enemy.downUntil > game.time
      ? 'down'
      : enemy.kind === 'grin'
        ? 'grin'
        : !enemy.awake && enemy.silent
          ? 'sleeper'
          : 'shambler';
    drawActorShadows(ctx, game, enemy.x, enemy.y, kind, { time: game.time });
    drawEnemy(ctx, enemy, ox, oy, game);
  }

  for (const prop of world.props) drawProp(ctx, prop, ox, oy, state, game);

  for (const drop of state.dropped) {
    const broken = drop.slot.dur != null && drop.slot.dur <= 0;
    const shade = shadowFor(broken ? `${drop.slot.id}-cracked` : drop.slot.id);
    drawActorShadows(ctx, game, drop.x, drop.y, 'drop', shade);
    drawWorldSprite(ctx, drop.slot.id, drop.x - ox, drop.y - oy, {
      ground: true,
      foot: 1,
      shadow: false,
      broken,
      time: game.time,
      reduce: game.settings.reduceFlash,
    });
  }

  drawLightMap(ctx, game);
  drawCloseSilhouettes(ctx, game, ox, oy);
  drawActorShadows(ctx, game, state.player.x, state.player.y, 'player', {
    self: true,
    facing: state.player.facing,
    moving: state.player.moving,
    overDark: true,
  });
  drawPlayer(ctx, state.player, ox, oy, game);
  drawPickupFx(ctx, game, ox, oy);
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
  drawContactShadow(ctx, sx + w * 0.5, sy + h - 3, { radius: Math.min(20, Math.max(6, w * 0.16)) }, 1);
  if (piece.kind === 'car') {
    ctx.fillStyle = '#343a42';
    ctx.fillRect(sx + 4, sy + 12, w - 8, h - 20);
    ctx.fillStyle = '#14161a';
    ctx.fillRect(sx + 10, sy + h - 14, 14, 8);
    ctx.fillRect(sx + w - 24, sy + h - 14, 14, 8);
    ctx.fillStyle = '#9aafbc';
    ctx.fillRect(sx + 14, sy + 14, w * 0.3, 26);
    ctx.fillRect(sx + w * 0.52, sy + 14, w * 0.34, 26);
    const light = shadeIllumination(game, (piece.x + 2.2) * TILE, (piece.y + 0.85) * TILE);
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

function drawProp(ctx, prop, ox, oy, state, game) {
  if (state.taken[prop.id]) return;
  if (prop.inspect === 'front') return;
  if (prop.kind === 'container' && !(state.containers[prop.id] || []).length) return;
  const id = artIdForProp(prop);
  if (!id) return;
  const wx = (prop.x + 0.5) * TILE;
  const wy = (prop.y + 0.5) * TILE;
  if (game) drawActorShadows(ctx, game, wx, wy, prop.kind === 'car' ? 'drop' : 'prop', shadowFor(id));
  drawWorldSprite(ctx, id, wx - ox, wy - oy, {
    ground: true,
    foot: prop.kind === 'car' ? 1 : 2,
    shadow: false,
    time: game?.time || 0,
    reduce: !!game?.settings?.reduceFlash,
  });
}

function drawFinder(ctx, x, y, color, time, reduce) {
  const wave = Math.sin(time * 1.15 + x * 0.13);
  if (!reduce && wave < 0.2) return;
  const px = Math.round(x);
  const py = Math.round(y);
  ctx.globalAlpha = reduce ? 0.5 : 0.35 + Math.max(0, wave) * 0.45;
  ctx.fillStyle = color;
  ctx.fillRect(px, py, 1, 1);
  ctx.globalAlpha = 1;
}

function drawPickupFx(ctx, game, ox, oy) {
  const fx = game.state.fx;
  if (!fx?.length) return;
  for (const bit of fx) {
    const k = 1 - bit.age / bit.life;
    if (k <= 0) continue;
    const lift = game.settings.reduceFlash ? 0 : (1 - k) * 8;
    drawWorldSprite(ctx, bit.id, bit.x - ox, bit.y - oy - lift, {
      ground: true,
      foot: 0,
      shadow: false,
      alpha: Math.max(0, k),
      time: game.time,
      reduce: true,
    });
  }
}

function drawPlayer(ctx, player, ox, oy, game) {
  const time = game.time;
  const x = Math.round(player.x - ox);
  const y = Math.round(player.y - oy);
  if (player.iframes > 0 && Math.sin(time * 40) > 0) return;
  const rate = player.running ? 16 : player.sneaking ? 7 : 11;
  const swing = player.moving ? Math.sin(time * rate) : 0;
  const bob = player.moving ? Math.round(Math.abs(swing)) : 0;
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
  drawHeld(ctx, game, x, y - bob, player);
}

function drawHeld(ctx, game, x, y, player) {
  const slot = game.state.inv[game.state.equipped];
  const fx = Math.cos(player.facing);
  const fy = Math.sin(player.facing);
  ctx.strokeStyle = 'rgba(228,192,122,0.55)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(x, y - 4);
  ctx.lineTo(x + fx * 8, y - 4 + fy * 8);
  ctx.stroke();
  if (!slot) return;
  const held = slot.id === 'stick' || slot.id === 'knife' || slot.id === 'pistol' || slot.id === 'camera';
  if (!held) return;
  if (player.atk > 0) {
    ctx.strokeStyle = 'rgba(228, 192, 122, 0.4)';
    ctx.beginPath();
    ctx.arc(x, y, slot.id === 'stick' ? 16 : 12, player.facing - 0.7, player.facing + 0.7);
    ctx.stroke();
  }
  drawWorldSprite(ctx, slot.id, x + fx * 6, y + fy * 6, {
    aim: player.facing,
    jab: player.atk > 0 ? 3 : 0,
    shadow: false,
    broken: slot.dur != null && slot.dur <= 0,
    time: game.time,
    reduce: game.settings.reduceFlash,
  });
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
    const light = shadeIllumination(game, enemy.x, enemy.y);
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
    const light = shadeIllumination(game, enemy.x, enemy.y);
    if (light > 0.22) continue;
    drawActorShadows(ctx, game, enemy.x, enemy.y, enemy.kind === 'grin' ? 'grin' : 'shambler', { contactOnly: true, overDark: true });
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
    if (shadeIllumination(game, x, y) > 0.35) continue;
    if (Math.hypot(x - game.state.player.x, y - game.state.player.y) > 210) continue;
    if (!game.los(game.state.player.x, game.state.player.y, x, y)) continue;
    const id = artIdForProp(prop);
    drawFinder(ctx, x - ox, y - oy, finderColor(id || 'stick'), time + prop.x, game.settings.reduceFlash);
  }
  for (const drop of game.state.dropped) {
    if (shadeIllumination(game, drop.x, drop.y) > 0.35) continue;
    if (Math.hypot(drop.x - game.state.player.x, drop.y - game.state.player.y) > 210) continue;
    if (!game.los(game.state.player.x, game.state.player.y, drop.x, drop.y)) continue;
    drawFinder(ctx, drop.x - ox, drop.y - oy, finderColor(drop.slot.id), time + drop.x, game.settings.reduceFlash);
  }
  const player = game.state.player;
  for (const door of game.world.doors) {
    const st = game.state.doors[door.id];
    const point = interactPoint(door, st);
    const x = point.x;
    const y = point.y;
    const distance = Math.hypot(x - player.x, y - player.y);
    if (distance > 160 || distance < 18) continue;
    if (shadeIllumination(game, x, y) > 0.5) continue;
    if (!game.los(player.x, player.y, x, y)) continue;
    const locked = door.lock && !st.unlocked && !(door.lock === 'power' && game.state.flags.powerOn);
    drawFinder(ctx, x - ox, y - oy, locked ? '#c45048' : '#e4c07a', time + door.y, game.settings.reduceFlash);
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
    if (shadeIllumination(game, p.x + (x + ox - p.x), p.y + (y + oy - p.y)) < 0.4) continue;
    ctx.fillRect(Math.round(x), Math.round(y + Math.sin(game.time * 2 + i) * 2), 1, 1);
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
      const full = room.name;
      const brief = room.brief || full;
      const fits = (label, px) => {
        ctx.font = `${px}px Courier New, monospace`;
        return ctx.measureText(label).width <= rw - 8;
      };
      let label = full;
      let size = 12;
      if (!fits(full, 12)) {
        if (fits(full, 10)) size = 10;
        else if (fits(brief, 11)) {
          label = brief;
          size = 11;
        } else if (fits(brief, 9)) {
          label = brief;
          size = 9;
        } else {
          label = brief;
          size = 8;
        }
      }
      ctx.font = `${size}px Courier New, monospace`;
      ctx.fillStyle = '#e4c07a';
      ctx.save();
      ctx.beginPath();
      ctx.rect(x + 1, y + 1, Math.max(0, rw - 2), Math.max(0, rh - 2));
      ctx.clip();
      ctx.fillText(label, x + 3, y + Math.min(rh - 3, size + 2));
      ctx.restore();
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
    const across = door.axis === 'ew';
    const w = across ? Math.max(5, scale * 0.7) : Math.max(2, scale * 0.22);
    const h = across ? Math.max(2, scale * 0.22) : Math.max(5, scale * 0.7);
    ctx.fillStyle = locked ? '#c45048' : st.angle > 0.72 ? '#8d734c' : '#e4c07a';
    ctx.fillRect(x - w / 2, y - h / 2, w, h);
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
