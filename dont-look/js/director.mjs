// Transition and jump-scare director.
// One scare at a time. Story beats can preempt a transition, never stack a second
// hit on top of a live hit. Movement lock is a short kick on the hit frame and
// is always forced back to zero. Blackout envelopes are single pulses so the
// grade stays under one luminance swing per second.

import { CUE, clamp, smoothstep } from './cues.mjs';

const IDLE_VIEW = {
  clear: '46%',
  ink: '0.58',
  black: 0,
  aberration: 0,
  grain: 0.04,
  distort: 0,
  lunge: 0,
  face: false,
  kickX: 0,
  kickY: 0,
  driftX: 0,
  driftY: 0,
};

export function createDirector() {
  const fired = new Set();
  let mode = 'idle';
  let t = 0;
  let elapsed = 0;
  let cooldown = 0;
  let transitionCooldown = 0;
  let controlLock = 0;
  let spec = null;
  let queue = null;
  let events = [];
  let impulseX = 0;
  let impulseY = 0;
  let view = { ...IDLE_VIEW };
  let reduce = false;
  let danger = 0;
  let time = 0;

  function reset() {
    mode = 'idle';
    t = 0;
    elapsed = 0;
    cooldown = 0;
    transitionCooldown = 0;
    controlLock = 0;
    spec = null;
    queue = null;
    events = [];
    impulseX = 0;
    impulseY = 0;
    fired.clear();
    view = { ...IDLE_VIEW };
  }

  function push(type, extra = {}) {
    events.push({ type, ...extra });
  }

  function scareSpec(options) {
    const quiet = !!options.reduce;
    return {
      kind: 'scare',
      story: !!options.story,
      face: !!options.face && !quiet,
      reduce: quiet,
      weight: clamp(options.weight ?? 1, 0.25, 1),
      buildup: Math.max(0.12, options.buildup ?? CUE.scare.buildup),
      silence: Math.max(0.06, options.silence ?? CUE.scare.silence),
      aftermath: options.aftermath ?? CUE.scare.aftermath,
      angle: options.angle ?? -Math.PI / 2,
      preempt: !!options.preempt,
    };
  }

  function transitionSpec(options) {
    const heavy = !!options.heavy;
    const duration = options.duration ?? (heavy ? CUE.transition.heavyDuration : CUE.transition.duration);
    return {
      kind: 'transition',
      heavy,
      calm: !!options.calm,
      reduce: !!options.reduce,
      weight: clamp(options.weight ?? (heavy ? 1 : 0.4), 0.15, 1),
      duration,
      subAt: duration * 0.42,
      subFired: false,
    };
  }

  function beginScare(id, options) {
    fired.add(id);
    spec = scareSpec(options);
    spec.id = id;
    mode = 'buildup';
    t = 0;
    elapsed = 0;
    push('scare-build', {
      weight: spec.weight,
      reduce: spec.reduce,
      duration: spec.buildup,
      face: spec.face,
    });
  }

  function beginTransition(id, options) {
    if (options.once) fired.add(id);
    spec = transitionSpec(options);
    spec.id = id;
    mode = 'transition';
    t = 0;
    elapsed = 0;
    push('transition-start', {
      weight: spec.weight,
      heavy: spec.heavy,
      calm: spec.calm,
      reduce: spec.reduce,
      duration: spec.duration,
    });
  }

  function finish() {
    const kind = spec?.kind;
    mode = 'idle';
    t = 0;
    elapsed = 0;
    controlLock = 0;
    if (kind === 'scare') cooldown = CUE.scare.cooldown;
    if (kind === 'transition') transitionCooldown = spec?.heavy ? 1.4 : 1.15;
    push(kind === 'scare' ? 'scare-done' : 'transition-done');
    spec = null;
    if (!queue) return;
    const next = queue;
    queue = null;
    if (next.kind === 'scare') beginScare(next.id, next.options);
    else beginTransition(next.id, next.options);
  }

  function requestScare(id, options = {}) {
    if (!id) return false;
    if (options.once !== false && fired.has(id)) return false;
    const active = mode === 'buildup' || mode === 'silence' || mode === 'aftermath';
    if (active) {
      if (options.preempt) {
        beginScare(id, options);
        return true;
      }
      if (options.story && !queue) queue = { kind: 'scare', id, options };
      return false;
    }
    if (mode === 'transition') {
      if (options.preempt || options.story) {
        beginScare(id, options);
        return true;
      }
      return false;
    }
    if (cooldown > 0 && !options.story && !options.preempt) return false;
    beginScare(id, options);
    return true;
  }

  function requestTransition(id, options = {}) {
    if (!id) return false;
    if (options.once && fired.has(id)) return false;
    if (mode !== 'idle') {
      if (options.heavy && !queue) queue = { kind: 'transition', id, options };
      return false;
    }
    if (!options.heavy && transitionCooldown > 0) return false;
    beginTransition(id, options);
    return true;
  }

  function bump(amount, x = 1, y = 0) {
    const scale = reduce ? 0.15 : 1;
    impulseX += x * amount * scale;
    impulseY += y * amount * scale;
    const mag = Math.hypot(impulseX, impulseY);
    const cap = reduce ? 2 : 14;
    if (mag > cap) {
      impulseX = (impulseX / mag) * cap;
      impulseY = (impulseY / mag) * cap;
    }
  }

  function advance(dt) {
    if (mode === 'idle' || !spec) return;
    t += dt;
    elapsed += dt;
    if (spec.kind === 'scare' && elapsed > CUE.scare.max) {
      finish();
      return;
    }
    let guard = 0;
    while (guard++ < 4 && spec) {
      if (mode === 'buildup' && t >= spec.buildup) {
        t -= spec.buildup;
        mode = 'silence';
        push('scare-silence', { reduce: spec.reduce });
        continue;
      }
      if (mode === 'silence' && t >= spec.silence) {
        t -= spec.silence;
        mode = 'aftermath';
        controlLock = spec.reduce ? 0 : CUE.scare.lock;
        const kick = spec.reduce ? 0 : 12 * spec.weight;
        impulseX += Math.cos(spec.angle) * kick;
        impulseY += Math.sin(spec.angle) * kick;
        push('scare-hit', {
          weight: spec.weight,
          reduce: spec.reduce,
          face: spec.face,
        });
        continue;
      }
      if (mode === 'aftermath' && t >= spec.aftermath) {
        finish();
        return;
      }
      if (mode === 'transition' && !spec.subFired && t >= spec.subAt) {
        spec.subFired = true;
        push('transition-sub', { weight: spec.weight, heavy: spec.heavy, calm: spec.calm });
        continue;
      }
      if (mode === 'transition' && t >= spec.duration) {
        finish();
        return;
      }
      break;
    }
  }

  function envelope() {
    if (mode === 'buildup' && spec) {
      const u = smoothstep(t / spec.buildup);
      const quiet = spec.reduce;
      return {
        ab: (quiet ? 0.05 : 0.42) * u * spec.weight,
        grain: 0.04 + (quiet ? 0.03 : 0.16) * u,
        distort: quiet ? 0 : 0.22 * u * spec.weight,
        black: (quiet ? 0.08 : 0.16) * u,
        lunge: 0,
        face: false,
      };
    }
    if (mode === 'silence' && spec) {
      const quiet = spec.reduce;
      return {
        ab: quiet ? 0.04 : 0.28,
        grain: 0.035,
        distort: 0,
        black: quiet ? 0.18 : 0.7,
        lunge: 0,
        face: false,
      };
    }
    if (mode === 'aftermath' && spec) {
      const quiet = spec.reduce;
      const decay = Math.exp(-t * 2.7);
      const faceDecay = Math.exp(-t * 3.3);
      return {
        ab: (quiet ? 0.05 : 0.72) * decay * spec.weight,
        grain: 0.04 + (quiet ? 0.04 : 0.28) * decay,
        distort: quiet ? 0 : 0.55 * Math.exp(-t * 6) * spec.weight,
        black: (quiet ? 0.16 : 0.22) * decay,
        lunge: spec.face ? faceDecay : 0,
        face: spec.face && faceDecay > 0.08,
      };
    }
    if (mode === 'transition' && spec) {
      const u = t / spec.duration;
      const center = 0.42;
      const bump = Math.exp(-((u - center) ** 2) / 0.02);
      const quiet = spec.reduce || spec.calm;
      const amount = (quiet ? 0.28 : 1) * spec.weight;
      return {
        ab: (spec.reduce ? 0.04 : 0.36) * bump * amount,
        grain: 0.04 + (spec.reduce ? 0.02 : 0.14) * bump * amount,
        distort: spec.reduce ? 0 : 0.32 * bump * amount,
        black: (spec.reduce ? 0.1 : spec.heavy ? 0.58 : 0.3) * bump * (spec.calm ? 0.35 : 1),
        lunge: 0,
        face: false,
      };
    }
    return {
      ab: reduce ? 0 : danger * 0.1,
      grain: 0.035 + (reduce ? 0 : danger * 0.035),
      distort: 0,
      black: 0,
      lunge: 0,
      face: false,
    };
  }

  function compose() {
    const env = envelope();
    const tight = clamp(env.ab * 0.65 + env.black * 0.4 + danger * 0.18, 0, 1);
    const clear = Math.round((reduce ? 50 : 46) - tight * (reduce ? 6 : 20));
    const ink = (reduce ? 0.5 : 0.58) + tight * (reduce ? 0.06 : 0.26);
    const driftAmp = reduce || mode === 'silence' ? 0 : 0.35 + danger * 1.8 + (mode === 'buildup' ? 1.6 : 0);
    view = {
      clear: `${clear}%`,
      ink: ink.toFixed(3),
      black: clamp(env.black, 0, 0.78),
      aberration: clamp(env.ab, 0, 1),
      grain: clamp(env.grain, 0, 0.5),
      distort: clamp(env.distort, 0, 1),
      lunge: spec?.reduce ? 0 : clamp(env.lunge, 0, 1),
      face: !!(env.face && !reduce),
      kickX: impulseX,
      kickY: impulseY,
      driftX: Math.sin(time * 0.37) * driftAmp,
      driftY: Math.cos(time * 0.23) * driftAmp * 0.65,
    };
  }

  return {
    reset,
    requestScare,
    requestTransition,
    bump,
    get mode() { return mode; },
    get locked() { return controlLock > 0.001; },
    get controlLock() { return controlLock; },
    get cooldown() { return cooldown; },
    get view() { return view; },
    get fired() { return fired; },
    update(dt, env = {}) {
      const step = Number.isFinite(dt) ? Math.min(0.05, Math.max(0, dt)) : 0;
      reduce = !!env.reduce;
      danger = clamp(env.danger || 0, 0, 1);
      time = env.time || time;
      cooldown = Math.max(0, cooldown - step);
      transitionCooldown = Math.max(0, transitionCooldown - step);
      controlLock = Math.max(0, controlLock - step);
      const decay = Math.exp(-step * 9);
      impulseX *= decay;
      impulseY *= decay;
      if (Math.abs(impulseX) < 0.05) impulseX = 0;
      if (Math.abs(impulseY) < 0.05) impulseY = 0;
      advance(step);
      if (mode === 'idle') controlLock = 0;
      controlLock = Math.min(controlLock, CUE.scare.lock);
      compose();
      const out = events;
      events = [];
      return out;
    },
  };
}
