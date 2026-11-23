// Procedural horror bus. No samples — the house is the instrument.
// Graph: beds → duck → pre, punches → pre, then highpass → compressor → tanh clip → master.
// The clip curve is bounded to [-1, 1]. Stingers sit after the silence duck so a
// dead cut can still be hit by the transient. Every one-shot takes a delay from CUE.

import { CUE } from './cues.mjs';
import { ITEM_CUES } from './item-cues.mjs';

const CLIP_K = 2.4;
const CLIP_NORM = Math.tanh(CLIP_K);

export function softClipSample(x) {
  if (!Number.isFinite(x)) return 0;
  return Math.tanh(CLIP_K * x) / CLIP_NORM;
}

export function clipCurve(length = 2048) {
  const curve = new Float32Array(length);
  for (let i = 0; i < length; i += 1) {
    const x = (i / (length - 1)) * 2 - 1;
    curve[i] = softClipSample(x);
  }
  return curve;
}

// Peaks are linear gains into the clipper, not simultaneous sums of every layer.
export const PEAKS = {
  foot: 0.07,
  footBody: 0.035,
  creak: 0.06,
  latch: 0.11,
  slam: 0.3,
  shot: 0.22,
  shotTail: 0.08,
  scareTransient: 0.18,
  scareShriek: 0.14,
  scareSub: 0.26,
  ring: 0.018,
  heart: 0.1,
  bed: 0.045,
  ui: 0.035,
};

export const ROOM_BED = {
  her: { noise: 0.02, hp: 110, lp: 620, hum: 0.006 },
  hallN: { noise: 0.014, hp: 150, lp: 680, hum: 0.002 },
  bath: { noise: 0.012, hp: 240, lp: 1500, hum: 0.01 },
  chris: { noise: 0.015, hp: 140, lp: 740, hum: 0.002 },
  hallS: { noise: 0.014, hp: 150, lp: 680, hum: 0.002 },
  mom: { noise: 0.026, hp: 90, lp: 520, hum: 0.012 },
  living: { noise: 0.018, hp: 120, lp: 700, hum: 0.002 },
  dining: { noise: 0.018, hp: 120, lp: 680, hum: 0.004 },
  kitchen: { noise: 0.015, hp: 200, lp: 1400, hum: 0.02 },
  foyer: { noise: 0.016, hp: 160, lp: 820, hum: 0.003 },
  mud: { noise: 0.018, hp: 180, lp: 900, hum: 0.004 },
  garage: { noise: 0.02, hp: 80, lp: 520, hum: 0.02 },
  basement: { noise: 0.028, hp: 70, lp: 420, hum: 0.04 },
  yard: { noise: 0.024, hp: 220, lp: 1400, hum: 0.001 },
  shed: { noise: 0.02, hp: 100, lp: 500, hum: 0.012 },
  default: { noise: 0.016, hp: 140, lp: 760, hum: 0.003 },
};

const AMBIENCE = {
  basement: ['hum', 'metal', 'drip', 'breath', 'whisper'],
  kitchen: ['hum', 'metal', 'creak', 'drip'],
  garage: ['hum', 'metal', 'creak'],
  yard: ['wind', 'creak', 'metal'],
  shed: ['creak', 'whisper', 'metal'],
  mom: ['breath', 'whisper', 'creak', 'hum'],
  her: ['whisper', 'creak', 'breath'],
  bath: ['drip', 'hum', 'creak'],
  default: ['creak', 'creak', 'metal', 'breath', 'hum'],
};

const SURFACE = {
  wood: { freq: 190, q: 0.85, body: 72 },
  carpet: { freq: 130, q: 0.6, body: 54 },
  tile: { freq: 460, q: 1.15, body: 96 },
  concrete: { freq: 100, q: 0.55, body: 46 },
  grass: { freq: 86, q: 0.45, body: 40 },
};

export function pickAmbience(room, n) {
  const bank = AMBIENCE[room] || AMBIENCE.default;
  const i = ((n % bank.length) + bank.length) % bank.length;
  return bank[i];
}

export function scareLayerSum() {
  return PEAKS.scareTransient + PEAKS.scareShriek + PEAKS.scareSub;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

export function createAudio() {
  let ctx = null;
  let master = null;
  let bed = null;
  let duck = null;
  let punch = null;
  let volume = 0.85;
  let noiseBuf = null;
  let failed = false;
  let drone = null;
  let elec = null;
  let cluster = null;
  let room = null;
  let currentRoom = '';
  let heartAt = 0;
  let patternUntil = 0;
  let scareUntil = 0;
  let riser = null;
  let hitQuiet = false;

  function ensure() {
    if (ctx) return true;
    if (failed) return false;
    const AC = globalThis.AudioContext || globalThis.webkitAudioContext;
    if (!AC) {
      failed = true;
      return false;
    }
    try {
      ctx = new AC();
      noiseBuf = makeNoise(ctx, 2);
      bed = ctx.createGain();
      bed.gain.value = 1;
      duck = ctx.createGain();
      duck.gain.value = 1;
      punch = ctx.createGain();
      punch.gain.value = 1;
      const pre = ctx.createGain();
      const hp = ctx.createBiquadFilter();
      hp.type = 'highpass';
      hp.frequency.value = 20;
      hp.Q.value = 0.7;
      const comp = ctx.createDynamicsCompressor();
      comp.threshold.value = -10;
      comp.knee.value = 8;
      comp.ratio.value = 12;
      comp.attack.value = 0.004;
      comp.release.value = 0.16;
      const clip = ctx.createWaveShaper();
      clip.curve = clipCurve();
      clip.oversample = '2x';
      master = ctx.createGain();
      master.gain.value = volume * 0.9;
      bed.connect(duck);
      duck.connect(pre);
      punch.connect(pre);
      pre.connect(hp);
      hp.connect(comp);
      comp.connect(clip);
      clip.connect(master);
      master.connect(ctx.destination);
      drone = makeDrone();
      elec = makeElec();
      cluster = makeCluster();
      room = makeRoom();
      return true;
    } catch {
      ctx = null;
      master = null;
      failed = true;
      return false;
    }
  }

  function now() {
    return ctx.currentTime;
  }

  function busTime(delay = 0) {
    return now() + Math.max(0, delay);
  }

  function drop(nodes) {
    for (const node of nodes) {
      try { node.disconnect(); } catch { /* already gone */ }
    }
  }

  function envGain(peak, dur, delay, attack = 0.008) {
    const gain = ctx.createGain();
    const t0 = busTime(delay);
    const safePeak = Math.max(0.0002, peak);
    const safeDur = Math.max(attack + 0.02, dur);
    gain.gain.setValueAtTime(0.0001, t0);
    gain.gain.linearRampToValueAtTime(safePeak, t0 + attack);
    gain.gain.exponentialRampToValueAtTime(0.0001, t0 + safeDur);
    return { gain, t0, stop: t0 + safeDur + 0.03 };
  }

  function panTo(node, pan, when) {
    const panner = ctx.createStereoPanner();
    panner.pan.setValueAtTime(clamp(pan || 0, -1, 1), when);
    node.connect(panner);
    return panner;
  }

  function noiseHit({
    dur = 0.12,
    freq = 180,
    q = 0.8,
    type = 'bandpass',
    peak = 0.05,
    pan = 0,
    rate = 1,
    delay = 0,
    target = punch,
    attack = 0.005,
  }) {
    if (!ensure()) return;
    const src = ctx.createBufferSource();
    src.buffer = noiseBuf;
    src.playbackRate.value = rate;
    const filter = ctx.createBiquadFilter();
    filter.type = type;
    filter.frequency.value = Math.max(40, freq);
    filter.Q.value = q;
    const shaped = envGain(peak, dur, delay, attack);
    const panner = panTo(filter, pan, shaped.t0);
    src.connect(filter);
    panner.connect(shaped.gain);
    shaped.gain.connect(target);
    src.start(shaped.t0);
    src.stop(shaped.stop);
    src.onended = () => drop([src, filter, panner, shaped.gain]);
  }

  function tone({
    freq = 220,
    dur = 0.2,
    type = 'sine',
    peak = 0.04,
    pan = 0,
    slide = 0,
    delay = 0,
    target = punch,
    attack = 0.012,
  }) {
    if (!ensure()) return;
    const osc = ctx.createOscillator();
    osc.type = type;
    const shaped = envGain(peak, dur, delay, attack);
    const endFreq = Math.max(28, freq + slide);
    osc.frequency.setValueAtTime(Math.max(28, freq), shaped.t0);
    if (slide) osc.frequency.exponentialRampToValueAtTime(endFreq, shaped.t0 + Math.max(0.02, dur));
    const panner = panTo(osc, pan, shaped.t0);
    panner.connect(shaped.gain);
    shaped.gain.connect(target);
    osc.start(shaped.t0);
    osc.stop(shaped.stop);
    osc.onended = () => drop([osc, panner, shaped.gain]);
  }

  function makeDrone() {
    const a = ctx.createOscillator();
    const b = ctx.createOscillator();
    a.type = 'sine';
    b.type = 'sine';
    a.frequency.value = 52;
    b.frequency.value = 78;
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 220;
    const gain = ctx.createGain();
    gain.gain.value = 0.018;
    a.connect(filter);
    b.connect(filter);
    filter.connect(gain);
    gain.connect(bed);
    a.start();
    b.start();
    return { a, b, gain, filter };
  }

  function makeElec() {
    const freqs = [60, 120, 180];
    const oscs = freqs.map((freq, i) => {
      const osc = ctx.createOscillator();
      osc.type = 'sine';
      osc.frequency.value = freq;
      osc.detune.value = (i - 1) * 4;
      return osc;
    });
    const gain = ctx.createGain();
    gain.gain.value = 0.0001;
    for (const osc of oscs) {
      osc.connect(gain);
      osc.start();
    }
    gain.connect(bed);
    return { gain };
  }

  function makeCluster() {
    const freqs = [196, 233, 174, 146];
    const oscs = freqs.map((freq, i) => {
      const osc = ctx.createOscillator();
      osc.type = 'triangle';
      osc.frequency.value = freq;
      osc.detune.value = (i - 1.5) * 7;
      return osc;
    });
    const filter = ctx.createBiquadFilter();
    filter.type = 'bandpass';
    filter.frequency.value = 740;
    filter.Q.value = 3.2;
    const gain = ctx.createGain();
    gain.gain.value = 0.0001;
    const panner = ctx.createStereoPanner();
    for (const osc of oscs) {
      osc.connect(filter);
      osc.start();
    }
    filter.connect(panner);
    panner.connect(gain);
    gain.connect(bed);
    return { gain, panner, filter, oscs };
  }

  function makeRoom() {
    const src = ctx.createBufferSource();
    src.buffer = noiseBuf;
    src.loop = true;
    const hp = ctx.createBiquadFilter();
    hp.type = 'highpass';
    hp.frequency.value = 140;
    const lp = ctx.createBiquadFilter();
    lp.type = 'lowpass';
    lp.frequency.value = 760;
    const gain = ctx.createGain();
    gain.gain.value = 0.0001;
    src.connect(hp);
    hp.connect(lp);
    lp.connect(gain);
    gain.connect(bed);
    src.start();
    return { gain, hp, lp };
  }

  function stopRiser() {
    if (!riser || !ctx) return;
    const t = now();
    try {
      riser.gain.gain.cancelScheduledValues(t);
      riser.gain.gain.setValueAtTime(Math.max(0.0001, riser.gain.gain.value), t);
      riser.gain.gain.linearRampToValueAtTime(0.0001, t + 0.03);
      riser.src.stop(t + 0.04);
    } catch { /* already stopped */ }
    riser = null;
  }

  function scale(peak) {
    return peak * (hitQuiet ? 0.62 : 1);
  }

  function scareHit(opts = {}) {
    if (!ensure()) return;
    const t0 = now();
    if (t0 + (opts.delay || 0) < scareUntil) return;
    const delay = opts.delay || 0;
    hitQuiet = !!opts.reduce;
    scareUntil = t0 + delay + 0.4;
    patternUntil = t0 + delay + CUE.scare.heart + CUE.scare.heartGap + 0.5;
    const weight = clamp(opts.weight ?? 1, 0.2, 1);
    const face = !!opts.face && !opts.reduce;
    noiseHit({
      dur: 0.045,
      freq: 2200,
      q: 0.45,
      type: 'highpass',
      peak: scale(PEAKS.scareTransient) * weight,
      delay,
      attack: 0.002,
    });
    tone({
      freq: 48,
      dur: 0.42,
      type: 'sine',
      peak: scale(PEAKS.scareSub) * weight,
      slide: -18,
      delay,
      attack: 0.004,
    });
    if (face) {
      noiseHit({
        dur: 0.34,
        freq: 1800,
        q: 1.4,
        peak: scale(PEAKS.scareShriek) * weight,
        delay: delay + 0.008,
        rate: 0.7,
        attack: 0.006,
      });
      tone({
        freq: 740,
        dur: 0.28,
        type: 'sawtooth',
        peak: scale(0.05) * weight,
        slide: -420,
        delay: delay + 0.008,
        attack: 0.006,
      });
    } else {
      noiseHit({
        dur: 0.16,
        freq: 240,
        q: 0.7,
        type: 'lowpass',
        peak: scale(0.12) * weight,
        delay,
        attack: 0.003,
      });
    }
    tone({
      freq: 3120,
      dur: 1.5,
      type: 'sine',
      peak: scale(PEAKS.ring),
      slide: -80,
      delay: delay + CUE.scare.ring,
      attack: 0.01,
    });
    tone({
      freq: 3185,
      dur: 1.45,
      type: 'sine',
      peak: scale(PEAKS.ring) * 0.7,
      slide: -60,
      delay: delay + CUE.scare.ring,
      attack: 0.01,
    });
    const heartDelay = delay + CUE.scare.heart;
    thump(heartDelay, weight);
    thump(heartDelay + CUE.scare.heartGap, weight * 0.9);
    if (!opts.keepDuck) {
      const at = t0 + delay;
      duck.gain.cancelScheduledValues(at);
      duck.gain.setValueAtTime(0.0001, at);
      duck.gain.linearRampToValueAtTime(1, at + 0.62);
    }
    hitQuiet = false;
  }

  function thump(delay, weight) {
    tone({
      freq: 58,
      dur: 0.07,
      type: 'sine',
      peak: scale(PEAKS.heart) * weight,
      delay,
      attack: 0.004,
    });
    tone({
      freq: 42,
      dur: 0.1,
      type: 'sine',
      peak: scale(0.06) * weight,
      slide: -8,
      delay: delay + CUE.scare.lub,
      attack: 0.004,
    });
  }

  function startRiser(duration, peak) {
    if (!ensure()) return;
    stopRiser();
    const t0 = now();
    const src = ctx.createBufferSource();
    src.buffer = noiseBuf;
    src.loop = true;
    const filter = ctx.createBiquadFilter();
    filter.type = 'bandpass';
    filter.Q.value = 2.4;
    filter.frequency.setValueAtTime(160, t0);
    filter.frequency.exponentialRampToValueAtTime(1500, t0 + Math.max(0.12, duration));
    const gain = ctx.createGain();
    gain.gain.setValueAtTime(0.0001, t0);
    gain.gain.linearRampToValueAtTime(Math.max(0.0002, peak), t0 + Math.max(0.12, duration));
    src.connect(filter);
    filter.connect(gain);
    gain.connect(bed);
    src.start(t0);
    src.stop(t0 + duration + 0.08);
    src.onended = () => drop([src, filter, gain]);
    riser = { src, gain };
  }

  let cine = null;

  function holdGameBed(level) {
    if (!ctx || !drone) return;
    const t = now();
    const floor = Math.max(0.0001, level);
    drone.gain.gain.setTargetAtTime(floor, t, 0.2);
    if (room) room.gain.gain.setTargetAtTime(floor, t, 0.25);
    if (elec) elec.gain.gain.setTargetAtTime(floor, t, 0.2);
    if (cluster) cluster.gain.gain.setTargetAtTime(floor, t, 0.2);
  }

  function ensureCine() {
    if (!ensure()) return null;
    if (cine) return cine;
    const bus = ctx.createGain();
    bus.gain.value = 0.0001;
    bus.connect(bed);
    const wind = ctx.createBufferSource();
    wind.buffer = noiseBuf;
    wind.loop = true;
    const windFilter = ctx.createBiquadFilter();
    windFilter.type = 'lowpass';
    windFilter.frequency.value = 480;
    const windGain = ctx.createGain();
    windGain.gain.value = 0.0001;
    wind.connect(windFilter);
    windFilter.connect(windGain);
    windGain.connect(bus);
    wind.start();
    const lowA = ctx.createOscillator();
    const lowB = ctx.createOscillator();
    lowA.type = 'sine';
    lowB.type = 'sine';
    lowA.frequency.value = 46;
    lowB.frequency.value = 69;
    const body = ctx.createGain();
    body.gain.value = 0.0001;
    lowA.connect(body);
    lowB.connect(body);
    body.connect(bus);
    lowA.start();
    lowB.start();
    const airOsc = ctx.createOscillator();
    airOsc.type = 'triangle';
    airOsc.frequency.value = 311;
    const airFilter = ctx.createBiquadFilter();
    airFilter.type = 'bandpass';
    airFilter.frequency.value = 320;
    airFilter.Q.value = 6;
    const air = ctx.createGain();
    air.gain.value = 0.0001;
    airOsc.connect(airFilter);
    airFilter.connect(air);
    air.connect(bus);
    airOsc.start();
    cine = {
      bus,
      windFilter,
      windGain,
      body,
      air,
      nodes: [wind, lowA, lowB, airOsc],
    };
    return cine;
  }

  function duckTo(value, attack, hold = 0, restore = null) {
    if (!duck) return;
    const t = now();
    duck.gain.cancelScheduledValues(t);
    duck.gain.setValueAtTime(Math.max(0.0001, duck.gain.value), t);
    duck.gain.linearRampToValueAtTime(value, t + attack);
    if (restore != null) duck.gain.linearRampToValueAtTime(1, t + attack + hold + restore);
  }

  return {
    unlock() {
      if (!ensure()) return;
      if (ctx.state === 'suspended') ctx.resume();
    },
    setVolume(value) {
      volume = clamp(value, 0, 1);
      if (!master || !ctx) return;
      master.gain.setTargetAtTime(volume * 0.9, now(), 0.02);
    },
    foot(surface, intensity = 0.55, pan = 0) {
      const spec = SURFACE[surface] || SURFACE.wood;
      const gain = clamp(intensity, 0, 1.2);
      const side = clamp(pan, -1, 1);
      noiseHit({
        dur: 0.07,
        freq: spec.freq,
        q: spec.q,
        peak: PEAKS.foot * gain,
        rate: 0.55 + gain * 0.4,
        attack: 0.003,
        pan: side,
        target: punch,
      });
      tone({
        freq: spec.body,
        dur: 0.06,
        type: 'sine',
        peak: PEAKS.footBody * gain,
        attack: 0.003,
        pan: side,
      });
      noiseHit({
        dur: 0.09,
        freq: spec.freq * 0.65,
        q: 0.6,
        peak: PEAKS.foot * gain * 0.32,
        delay: CUE.foot.reflect,
        rate: 0.4,
        pan: side,
        target: bed,
      });
    },
    moan(pan, volumeMul = 1, kind = 'shambler') {
      const grin = kind === 'grin';
      const peak = 0.04 * clamp(volumeMul, 0, 1.4);
      tone({
        freq: grin ? 92 : 68,
        dur: grin ? 0.72 : 0.48,
        type: 'sawtooth',
        peak,
        pan,
        slide: grin ? -28 : 10,
        target: bed,
      });
      noiseHit({
        dur: grin ? 0.5 : 0.36,
        freq: grin ? 520 : 280,
        q: 1.3,
        peak: peak * 0.8,
        pan,
        rate: 0.32,
        target: bed,
      });
    },
    creak(pan = 0) {
      noiseHit({
        dur: 0.38,
        freq: 260,
        q: 6,
        peak: PEAKS.creak,
        pan,
        rate: 0.22,
        target: bed,
      });
      tone({
        freq: 320,
        dur: 0.42,
        type: 'sine',
        peak: 0.018,
        pan,
        slide: -150,
        target: bed,
      });
    },
    door() {
      this.creak(0);
    },
    doorCreak(pan = 0, intensity = 0.7) {
      const gain = 0.4 + Math.max(0, Math.min(1, intensity)) * 0.7;
      noiseHit({
        dur: 0.14 + intensity * 0.08,
        freq: 230 + intensity * 70,
        q: 7.5,
        peak: PEAKS.creak * gain,
        pan,
        rate: 0.18,
        target: bed,
      });
      tone({
        freq: 200 + intensity * 40,
        dur: 0.18,
        type: 'sine',
        peak: 0.016 * gain,
        pan,
        slide: -70,
        target: bed,
      });
    },
    rattle(pan = 0) {
      noiseHit({
        dur: 0.04,
        freq: 1100,
        q: 4.5,
        peak: 0.05,
        pan,
        rate: 1.8,
        attack: 0.002,
      });
      noiseHit({
        dur: 0.06,
        freq: 240,
        q: 1.6,
        peak: 0.04,
        pan,
        rate: 0.55,
        delay: 0.028,
        attack: 0.002,
      });
    },
    motorTick(pan = 0, intensity = 0.6) {
      const gain = 0.45 + Math.max(0, Math.min(1, intensity)) * 0.7;
      noiseHit({
        dur: 0.11,
        freq: 86,
        q: 1.15,
        type: 'lowpass',
        peak: 0.055 * gain,
        pan,
        rate: 0.3,
        target: bed,
        attack: 0.01,
      });
      tone({
        freq: 46,
        dur: 0.1,
        type: 'sawtooth',
        peak: 0.016 * gain,
        pan,
        slide: 10,
        target: bed,
        attack: 0.01,
      });
    },
    thud(pan = 0) {
      noiseHit({
        dur: 0.08,
        freq: 110,
        q: 0.55,
        type: 'lowpass',
        peak: PEAKS.latch * 0.5,
        pan,
        rate: 0.42,
        attack: 0.002,
      });
    },
    latch(pan = 0) {
      noiseHit({
        dur: 0.09,
        freq: 140,
        q: 0.7,
        type: 'lowpass',
        peak: PEAKS.latch,
        pan,
        attack: 0.002,
      });
      tone({ freq: 88, dur: 0.08, type: 'sine', peak: 0.05, pan, attack: 0.002 });
    },
    slam(pan = 0) {
      noiseHit({
        dur: 0.12,
        freq: 90,
        q: 0.55,
        type: 'lowpass',
        peak: PEAKS.slam,
        pan,
        rate: 0.4,
        attack: 0.002,
      });
      tone({
        freq: 52,
        dur: 0.28,
        type: 'sine',
        peak: 0.16,
        pan,
        slide: -16,
        attack: 0.003,
      });
      noiseHit({
        dur: 0.2,
        freq: 640,
        q: 2.5,
        peak: 0.06,
        pan,
        delay: CUE.frame,
      });
    },
    motor(pan = 0) {
      noiseHit({
        dur: 0.55,
        freq: 140,
        q: 1.8,
        peak: 0.07,
        pan,
        rate: 0.35,
        target: bed,
        attack: 0.03,
      });
      tone({
        freq: 58,
        dur: 0.5,
        type: 'sawtooth',
        peak: 0.03,
        pan,
        slide: 12,
        target: bed,
        attack: 0.04,
      });
    },
    swing() {
      noiseHit({
        dur: 0.09,
        freq: 980,
        q: 0.55,
        peak: 0.045,
        rate: 1.35,
        attack: 0.003,
        delay: CUE.melee.whoosh,
      });
    },
    shot() {
      noiseHit({
        dur: 0.05,
        freq: 1800,
        q: 0.4,
        type: 'highpass',
        peak: 0.16,
        attack: 0.001,
        delay: CUE.shot.report,
      });
      noiseHit({
        dur: 0.16,
        freq: 90,
        q: 0.5,
        type: 'lowpass',
        peak: PEAKS.shot,
        rate: 0.4,
        attack: 0.002,
      });
      noiseHit({
        dur: 0.4,
        freq: 160,
        q: 0.4,
        type: 'lowpass',
        peak: PEAKS.shotTail,
        rate: 0.22,
        delay: CUE.shot.tail,
        target: bed,
      });
      tone({ freq: 130, dur: 0.1, type: 'square', peak: 0.04, slide: -70, attack: 0.002 });
    },
    hit() {
      noiseHit({ dur: 0.08, freq: 210, q: 0.8, peak: 0.08, attack: 0.002, delay: CUE.melee.impact });
    },
    hurt() {
      tone({
        freq: 96,
        dur: 0.22,
        type: 'sawtooth',
        peak: 0.055,
        slide: -36,
        delay: CUE.hurt.impact,
        attack: 0.004,
      });
      noiseHit({ dur: 0.1, freq: 180, q: 0.6, type: 'lowpass', peak: 0.06, delay: CUE.hurt.impact });
    },
    pickup() {
      tone({ freq: 410, dur: 0.05, type: 'sine', peak: 0.028, attack: 0.003, delay: CUE.pickup.tick });
      tone({ freq: 620, dur: 0.04, type: 'sine', peak: 0.016, attack: 0.003, delay: CUE.pickup.settle });
    },
    cue(id, verb = 'take') {
      const base = String(id || '').endsWith('-cracked') ? String(id).replace(/-cracked$/, '') : '';
      const spec = ITEM_CUES[`${id}:${verb}`] || (base ? ITEM_CUES[`${base}:${verb}`] : null);
      if (!spec) return;
      for (const step of spec) {
        if (step.noise) noiseHit(step);
        else tone(step);
      }
    },
    shutter() {
      noiseHit({ dur: 0.04, freq: 1900, q: 0.45, peak: 0.06, delay: CUE.shutter.clap, attack: 0.001 });
      noiseHit({
        dur: 0.07,
        freq: 220,
        q: 0.6,
        type: 'lowpass',
        peak: 0.05,
        delay: CUE.shutter.slap,
        attack: 0.002,
      });
    },
    flash() {
      noiseHit({ dur: 0.05, freq: 1600, q: 0.35, peak: 0.07, delay: CUE.flash.pop, attack: 0.001 });
      tone({
        freq: 2400,
        dur: 0.18,
        type: 'sine',
        peak: 0.02,
        slide: -1800,
        delay: CUE.flash.whine,
        attack: 0.004,
      });
    },
    stinger() {
      scareHit({ weight: 0.55, face: false });
    },
    heart(strength) {
      if (!ensure()) return;
      const t = now();
      if (heartAt > t || patternUntil > t) return;
      const amount = clamp(strength, 0, 1);
      const gap = 0.98 - Math.min(0.62, amount) * 0.7;
      heartAt = t + gap;
      tone({ freq: 62, dur: 0.07, type: 'sine', peak: 0.04 * amount, attack: 0.004, target: bed });
      tone({
        freq: 44,
        dur: 0.1,
        type: 'sine',
        peak: 0.028 * amount,
        slide: -8,
        delay: CUE.scare.lub,
        attack: 0.004,
        target: bed,
      });
    },
    voice() {
      tone({ freq: 196, dur: 0.32, type: 'triangle', peak: 0.018, target: bed });
      tone({ freq: 247, dur: 0.38, type: 'triangle', peak: 0.012, slide: -18, target: bed });
      noiseHit({
        dur: 0.36,
        freq: 900,
        q: 1.6,
        peak: 0.012,
        rate: 0.5,
        target: bed,
      });
    },
    ui(kind = 'open') {
      const freq = kind === 'close' ? 280 : kind === 'heavy' ? 140 : 520;
      noiseHit({
        dur: 0.03,
        freq,
        q: 0.7,
        peak: PEAKS.ui,
        delay: CUE.ui.click,
        attack: 0.002,
      });
    },
    paper() {
      noiseHit({ dur: 0.08, freq: 1400, q: 0.5, peak: 0.03, rate: 0.8, attack: 0.004, target: bed });
    },
    metal(pan = 0, gain = 1) {
      tone({
        freq: 118,
        dur: 1.1,
        type: 'sine',
        peak: 0.03 * gain,
        pan,
        slide: -40,
        target: bed,
        attack: 0.05,
      });
      noiseHit({
        dur: 0.8,
        freq: 860,
        q: 4,
        peak: 0.025 * gain,
        pan,
        rate: 0.28,
        target: bed,
        attack: 0.04,
      });
    },
    whisper(pan = 0, gain = 1) {
      noiseHit({
        dur: 0.7,
        freq: 1400,
        q: 2.2,
        peak: 0.02 * gain,
        pan,
        rate: 0.45,
        target: bed,
        attack: 0.08,
      });
      tone({
        freq: 460,
        dur: 0.55,
        type: 'sine',
        peak: 0.008 * gain,
        pan,
        slide: -30,
        target: bed,
        attack: 0.06,
      });
    },
    breath(pan = 0, gain = 1) {
      noiseHit({
        dur: 0.42,
        freq: 520,
        q: 1.1,
        peak: 0.03 * gain,
        pan,
        rate: 0.55,
        target: bed,
        attack: 0.06,
      });
      noiseHit({
        dur: 0.36,
        freq: 480,
        q: 0.9,
        peak: 0.022 * gain,
        pan: clamp(pan + 0.15, -1, 1),
        delay: 0.38,
        rate: 0.5,
        target: bed,
        attack: 0.05,
      });
    },
    drip(pan = 0) {
      tone({ freq: 1400, dur: 0.06, type: 'sine', peak: 0.02, pan, target: bed, attack: 0.002 });
      tone({ freq: 900, dur: 0.08, type: 'sine', peak: 0.01, pan, delay: 0.09, target: bed });
    },
    wind(pan = 0) {
      noiseHit({
        dur: 1.3,
        freq: 400,
        q: 0.4,
        type: 'lowpass',
        peak: 0.04,
        pan,
        rate: 0.25,
        target: bed,
        attack: 0.2,
      });
    },
    humTick(pan = 0) {
      tone({ freq: 60, dur: 0.5, type: 'sine', peak: 0.03, pan, target: bed, attack: 0.04 });
      tone({ freq: 180, dur: 0.4, type: 'sine', peak: 0.012, pan, target: bed, attack: 0.04 });
    },
    ambience({ room: roomId, pan = 0, danger = 0, index = 0 } = {}) {
      const kind = pickAmbience(roomId, index);
      const gain = 0.75 + clamp(danger, 0, 1) * 0.4;
      if (kind === 'creak') this.creak(pan);
      else if (kind === 'metal') this.metal(pan, gain);
      else if (kind === 'whisper') this.whisper(pan, gain);
      else if (kind === 'breath') this.breath(pan, gain);
      else if (kind === 'drip') this.drip(pan);
      else if (kind === 'wind') this.wind(pan);
      else this.humTick(pan);
      return kind;
    },
    dread() {
      tone({ freq: 70, dur: 0.55, type: 'sine', peak: 0.06, slide: -18, target: bed, attack: 0.02 });
      noiseHit({ dur: 0.4, freq: 300, q: 1.2, peak: 0.03, rate: 0.3, target: bed });
    },
    sub(weight = 1) {
      tone({
        freq: 46,
        dur: 0.36,
        type: 'sine',
        peak: 0.18 * clamp(weight, 0, 1),
        slide: -14,
        attack: 0.004,
      });
    },
    cut({ weight = 0.5, hold = 0.05, restore = 0.42 } = {}) {
      if (!ensure()) return;
      stopRiser();
      duckTo(0.02, 0.012, hold, restore);
      this.sub(weight);
    },
    scareBuild({ duration = CUE.scare.buildup, weight = 1, reduce: quiet = false } = {}) {
      if (duck && ctx) {
        const t = now();
        duck.gain.cancelScheduledValues(t);
        duck.gain.setValueAtTime(Math.max(0.0001, duck.gain.value), t);
        duck.gain.linearRampToValueAtTime(1, t + 0.05);
      }
      startRiser(duration, (quiet ? 0.035 : 0.08) * clamp(weight, 0.2, 1));
      tone({
        freq: 42,
        dur: duration,
        type: 'sine',
        peak: (quiet ? 0.02 : 0.04) * weight,
        slide: 16,
        target: bed,
        attack: 0.08,
      });
    },
    scareSilence() {
      if (!ensure()) return;
      stopRiser();
      duckTo(0.0001, 0.018);
    },
    scareHit(opts = {}) {
      scareHit(opts);
    },
    scareRelease() {
      if (!duck || !ctx) return;
      const t = now();
      duck.gain.cancelScheduledValues(t);
      duck.gain.setValueAtTime(Math.max(0.0001, duck.gain.value), t);
      duck.gain.linearRampToValueAtTime(1, t + 0.4);
    },
    transition({ weight = 0.5, heavy = false, calm = false, reduce: quiet = false, duration = 0.7 } = {}) {
      if (!ensure()) return;
      const dip = calm ? 0.55 : heavy ? 0.12 : 0.32;
      duckTo(dip, 0.06, heavy ? 0.18 : 0.08, duration * 0.45);
      if (!calm) startRiser(Math.min(0.45, duration * 0.5), (quiet ? 0.02 : 0.045) * weight);
    },
    introSequence(hitIn, quiet = false) {
      if (!ensure()) return;
      const lead = quiet ? 0.04 : CUE.intro.silence;
      const riseFor = Math.max(0.05, hitIn - lead);
      startRiser(riseFor, quiet ? 0.03 : 0.07);
      const t = now();
      const hit = t + hitIn;
      const silenceAt = t + Math.max(0, hitIn - lead);
      duck.gain.cancelScheduledValues(t);
      duck.gain.setValueAtTime(1, t);
      duck.gain.setValueAtTime(1, silenceAt);
      duck.gain.linearRampToValueAtTime(0.0001, silenceAt + 0.015);
      duck.gain.setValueAtTime(0.0001, hit);
      duck.gain.linearRampToValueAtTime(1, hit + 0.62);
      scareHit({ delay: hitIn, weight: quiet ? 0.45 : 0.8, face: !quiet, reduce: quiet, keepDuck: true });
    },
    resolve() {
      if (!ensure()) return;
      stopRiser();
      const t = now();
      duck.gain.cancelScheduledValues(t);
      duck.gain.setValueAtTime(Math.max(0.0001, duck.gain.value), t);
      duck.gain.linearRampToValueAtTime(0.35, t + 0.4);
      duck.gain.linearRampToValueAtTime(0.7, t + 1.6);
      tone({ freq: 220, dur: 1.2, type: 'sine', peak: 0.02, slide: 6, target: punch, attack: 0.08 });
    },
    hasCinematic() {
      return !!cine;
    },
    cinematic(mode) {
      if (!mode || mode === 'off') {
        this.endCinematic(0.35);
        return;
      }
      const bed = ensureCine();
      if (!bed) return;
      holdGameBed(0.0001);
      const presets = {
        title: { wind: 0.02, body: 0.011, air: 0.0025, hz: 500 },
        intro: { wind: 0.016, body: 0.015, air: 0.0035, hz: 440 },
        tight: { wind: 0.007, body: 0.02, air: 0.01, hz: 280 },
        garage: { wind: 0.005, body: 0.018, air: 0.0015, hz: 180 },
      };
      const preset = presets[mode] || presets.intro;
      const t = now();
      bed.bus.gain.cancelScheduledValues(t);
      bed.bus.gain.setValueAtTime(Math.max(0.0001, bed.bus.gain.value), t);
      bed.bus.gain.setTargetAtTime(1, t, 0.35);
      bed.windGain.gain.setTargetAtTime(preset.wind, t, 0.4);
      bed.body.gain.setTargetAtTime(preset.body, t, 0.4);
      bed.air.gain.setTargetAtTime(preset.air, t, 0.35);
      bed.windFilter.frequency.setTargetAtTime(preset.hz, t, 0.5);
    },
    fadeCinematic(seconds = 0.6) {
      if (!cine || !ctx) return;
      const t = now();
      cine.bus.gain.cancelScheduledValues(t);
      cine.bus.gain.setValueAtTime(Math.max(0.0001, cine.bus.gain.value), t);
      cine.bus.gain.linearRampToValueAtTime(0.0001, t + Math.max(0.05, seconds));
    },
    endCinematic(fade = 0.25) {
      if (!cine || !ctx) {
        cine = null;
        return;
      }
      const dying = cine;
      cine = null;
      const t = now();
      const span = Math.max(0.05, fade);
      try {
        dying.bus.gain.cancelScheduledValues(t);
        dying.bus.gain.setValueAtTime(Math.max(0.0001, dying.bus.gain.value), t);
        dying.bus.gain.linearRampToValueAtTime(0.0001, t + span);
        for (const node of dying.nodes) {
          try { node.stop(t + span + 0.02); } catch { /* one-shot already ended */ }
        }
      } catch { /* context already closed */ }
      setTimeout(() => {
        try { dying.bus.disconnect(); } catch { /* already disconnected */ }
      }, (span + 0.08) * 1000);
    },
    duckGame() {
      holdGameBed(0.0001);
    },
    phoneBuzz() {
      const bed = ensureCine();
      if (!bed) return;
      tone({ freq: 740, dur: 0.06, type: 'square', peak: 0.016, target: bed.bus, attack: 0.004 });
      tone({ freq: 520, dur: 0.08, type: 'square', peak: 0.01, delay: 0.09, target: bed.bus, attack: 0.004 });
    },
    voidHit(strength = 1) {
      const quiet = strength < 0.6;
      stopRiser();
      scareHit({ delay: 0, weight: quiet ? 0.4 : 0.72, face: !quiet, reduce: quiet });
    },
    engineDie() {
      const bed = ensureCine();
      if (!bed) return;
      noiseHit({
        dur: 0.45,
        freq: 90,
        q: 0.55,
        type: 'lowpass',
        peak: 0.07,
        rate: 0.35,
        target: bed.bus,
        attack: 0.01,
      });
      tone({
        freq: 78,
        dur: 0.55,
        type: 'sawtooth',
        peak: 0.03,
        slide: -40,
        target: bed.bus,
        attack: 0.02,
      });
    },
    bulbTick() {
      const bed = ensureCine();
      if (!bed) return;
      noiseHit({
        dur: 0.04,
        freq: 1800,
        q: 0.7,
        peak: 0.02,
        target: bed.bus,
        attack: 0.002,
      });
    },
    rise(quiet = false) {
      startRiser(2.3, quiet ? 0.028 : 0.05);
    },
    mix({
      danger = 0,
      humVol = 0,
      humPan = 0,
      distorted = false,
      room: roomId = '',
      paused = false,
      reduce: quiet = false,
      time = 0,
    } = {}) {
      if (!drone || !elec || !cluster || !room || !ctx) return;
      const bedCut = quiet ? 0.72 : 1;
      const t = now();
      const amount = paused ? 0.04 : clamp(danger, 0, 1);
      drone.gain.gain.setTargetAtTime((0.012 + amount * 0.028) * bedCut, t, 0.25);
      drone.a.frequency.setTargetAtTime(50 + amount * 8, t, 0.35);
      drone.b.frequency.setTargetAtTime(76 + amount * 16, t, 0.35);
      const profile = ROOM_BED[roomId] || ROOM_BED.default;
      if (roomId && roomId !== currentRoom) currentRoom = roomId;
      const live = ROOM_BED[currentRoom] || profile;
      const breathe = 0.9 + Math.sin(time * 0.35) * 0.1;
      room.gain.gain.setTargetAtTime((paused ? live.noise * 0.45 : live.noise) * breathe, t, 0.4);
      room.hp.frequency.setTargetAtTime(live.hp, t, 0.45);
      room.lp.frequency.setTargetAtTime(distorted ? live.lp * 0.72 : live.lp, t, 0.45);
      const humPulse = 0.82 + Math.sin(time * 0.7) * 0.18;
      elec.gain.gain.setTargetAtTime(Math.max(0.0001, live.hum * humPulse * (paused ? 0.4 : 1)), t, 0.3);
      cluster.gain.gain.setTargetAtTime(Math.max(0.0001, paused ? humVol * 0.35 : humVol), t, 0.18);
      cluster.panner.pan.setTargetAtTime(clamp(humPan, -1, 1), t, 0.12);
      cluster.filter.frequency.setTargetAtTime(distorted ? 420 : 860, t, 0.25);
    },
  };
}

function makeNoise(context, seconds) {
  const count = Math.floor(context.sampleRate * seconds);
  const buffer = context.createBuffer(1, count, context.sampleRate);
  const data = buffer.getChannelData(0);
  let hold = 0;
  for (let i = 0; i < data.length; i += 1) {
    if (i % 3 === 0) hold = Math.random() * 2 - 1;
    data[i] = hold * 0.65 + (Math.random() * 2 - 1) * 0.35;
  }
  return buffer;
}
