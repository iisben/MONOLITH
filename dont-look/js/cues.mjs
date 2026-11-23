// Shared cue clock for Don't Look.
// Animation phase, door leaf, and audio offsets all read these numbers.
// A "plant" is the frame a foot is fully down. Door latch is the leaf
// position where the door becomes passable and the hardware ticks.

export const FRAME = 1 / 60;

export const CUE = {
  frame: FRAME,
  foot: {
    walkInterval: 0.44,
    sneakInterval: 0.62,
    runInterval: 0.28,
    reflect: 0.04,
  },
  enemy: { rate: 8 },
  door: {
    duration: 0.28,
    slamDuration: 0.1,
    latchLeaf: 0.86,
  },
  melee: { whoosh: 0, impact: 0 },
  shot: { report: 0, tail: 0.05, flash: 2 * FRAME },
  dodge: { cloth: 0 },
  pickup: { tick: 0, settle: FRAME },
  ui: { click: 0 },
  shutter: { clap: 0, slap: FRAME },
  flash: { pop: 0, whine: 0.02 },
  hurt: { impact: 0 },
  transition: {
    duration: 0.7,
    heavyDuration: 1.05,
    sub: 0.36,
  },
  scare: {
    buildup: 0.62,
    silence: 0.1,
    ring: FRAME,
    heart: 0.14,
    heartGap: 0.74,
    lub: 0.16,
    aftermath: 1.65,
    lock: 0.12,
    cooldown: 7.5,
    max: 3.35,
  },
  intro: {
    hit: 0.36,
    hitReduced: 0.08,
    silence: 0.09,
  },
};

export const FOOT_MARKS = [Math.PI / 2, (3 * Math.PI) / 2];
export const ENEMY_STEP_MARKS = [0, Math.PI];

export const HEAVY_ROOMS = new Set(['basement', 'mom', 'her', 'shed', 'yard']);

export function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

export function smoothstep(value) {
  const t = clamp(value, 0, 1);
  return t * t * (3 - 2 * t);
}

export function gaitInterval(running, sneaking) {
  if (running) return CUE.foot.runInterval;
  if (sneaking) return CUE.foot.sneakInterval;
  return CUE.foot.walkInterval;
}

export function gaitRate(running, sneaking) {
  return Math.PI / gaitInterval(running, sneaking);
}

export function hashPhase(id) {
  let n = 2166136261;
  const text = String(id || '');
  for (let i = 0; i < text.length; i += 1) {
    n ^= text.charCodeAt(i);
    n = Math.imul(n, 16777619);
  }
  return ((n >>> 0) % 6283) / 1000;
}

// How many phase marks were crossed in (prev, next].
export function phaseMarksCrossed(prev, next, marks) {
  if (!(next > prev) || !marks?.length) return 0;
  const period = Math.PI * 2;
  let count = 0;
  for (const mark of marks) {
    let k = Math.ceil((prev - mark) / period);
    let phase = k * period + mark;
    while (phase <= next + 1e-8) {
      if (phase > prev + 1e-8) count += 1;
      k += 1;
      phase = k * period + mark;
      if (count > 8) return count;
    }
  }
  return count;
}

// Linear leaf, 0 shut → 1 open. Latch fires on the crossing of latchLeaf.
export function stepLeaf(leaf, open, dt, slam = false) {
  const target = open ? 1 : 0;
  const duration = slam ? CUE.door.slamDuration : CUE.door.duration;
  const prev = leaf;
  if (prev === target || !(dt > 0)) return { leaf: prev, crossed: false };
  const delta = (dt / duration) * Math.sign(target - prev);
  const next = delta > 0 ? Math.min(target, prev + delta) : Math.max(target, prev + delta);
  const mark = open ? CUE.door.latchLeaf : 1 - CUE.door.latchLeaf;
  const crossed = open ? prev < mark && next >= mark : prev > mark && next <= mark;
  return { leaf: next, crossed };
}

export function doorPassable(leaf) {
  return leaf >= CUE.door.latchLeaf;
}
