// Fixed pool. Bursts reuse dead slots instead of allocating.
import { puffSprite, streakSprite } from './atlas.mjs';

const SIZE = 160;

export function createFx() {
  const parts = Array.from({ length: SIZE }, () => ({ alive: false }));
  let cursor = 0;
  return {
    parts,
    burst(x, y, color, count = 6, speed = 36) {
      const n = Math.min(count, 16);
      for (let i = 0; i < n; i += 1) {
        const part = parts[cursor];
        cursor = (cursor + 1) % SIZE;
        const angle = Math.random() * Math.PI * 2;
        const mag = speed * (0.35 + Math.random() * 0.65);
        part.alive = true;
        part.x = x;
        part.y = y;
        part.vx = Math.cos(angle) * mag;
        part.vy = Math.sin(angle) * mag;
        part.life = 0.28 + Math.random() * 0.35;
        part.age = 0;
        part.color = color;
        part.size = 1 + (hash(i) % 2);
        part.streak = speed > 30;
      }
    },
    update(dt) {
      for (const part of parts) {
        if (!part.alive) continue;
        part.age += dt;
        if (part.age >= part.life) {
          part.alive = false;
          continue;
        }
        part.x += part.vx * dt;
        part.y += part.vy * dt;
        part.vx *= 0.9;
        part.vy *= 0.9;
      }
    },
    draw(ctx, ox, oy) {
      const puff = puffSprite();
      const streak = streakSprite();
      for (const part of parts) {
        if (!part.alive) continue;
        ctx.globalAlpha = Math.max(0, 1 - part.age / part.life);
        const img = part.streak ? streak : puff;
        ctx.drawImage(img, Math.round(part.x - ox - img.width / 2), Math.round(part.y - oy - img.height / 2));
      }
      ctx.globalAlpha = 1;
    },
  };
}

function hash(n) {
  return Math.imul(n ^ 0x9e3779b9, 0x85ebca6b) >>> 0;
}
