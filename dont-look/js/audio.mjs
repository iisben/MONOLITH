// Sequenced in the browser. Buses: master, music, sfx, ambience.
const ZONES = {
  mud: [50, 74],
  foyer: [48, 70],
  living: [46, 68],
  dining: [45, 66],
  kitchen: [52, 78],
  hallN: [44, 64],
  hallS: [44, 64],
  bath: [55, 82],
  chris: [47, 71],
  her: [54, 80],
  mom: [43, 63],
  garage: [40, 58],
  basement: [36, 54],
  yard: [58, 86],
  shed: [42, 62],
};

export function createAudio() {
  let ctx = null;
  let master = null;
  let musicBus = null;
  let sfxBus = null;
  let ambBus = null;
  let wet = null;
  let volume = 0.85;
  let buses = { music: 0.85, sfx: 0.9, ambience: 0.8 };
  let drone = null;
  let hum = null;
  let noiseBuf = null;
  let failed = false;
  let heartAt = 0;
  let active = 0;
  let zoneId = '';

  function ensure() {
    if (ctx) return true;
    if (failed) return false;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) {
      failed = true;
      return false;
    }
    try {
      ctx = new AC();
      master = ctx.createGain();
      master.gain.value = volume;
      const comp = ctx.createDynamicsCompressor();
      comp.threshold.value = -16;
      comp.knee.value = 8;
      comp.ratio.value = 3.2;
      comp.attack.value = 0.004;
      comp.release.value = 0.18;
      comp.connect(master);
      master.connect(ctx.destination);
      musicBus = ctx.createGain();
      sfxBus = ctx.createGain();
      ambBus = ctx.createGain();
      musicBus.gain.value = buses.music;
      sfxBus.gain.value = buses.sfx;
      ambBus.gain.value = buses.ambience;
      musicBus.connect(comp);
      sfxBus.connect(comp);
      ambBus.connect(comp);
      wet = ctx.createGain();
      wet.gain.value = 0.22;
      const verb = ctx.createConvolver();
      verb.buffer = impulse(1.1, 2.4);
      wet.connect(ambBus);
      verb.connect(wet);
      ctx._verb = verb;
      const seconds = 1;
      noiseBuf = ctx.createBuffer(1, ctx.sampleRate * seconds, ctx.sampleRate);
      const data = noiseBuf.getChannelData(0);
      for (let i = 0; i < data.length; i += 1) data[i] = Math.random() * 2 - 1;
      drone = makeDrone();
      hum = makeHum();
      return true;
    } catch {
      ctx = null;
      master = null;
      musicBus = null;
      sfxBus = null;
      ambBus = null;
      wet = null;
      drone = null;
      hum = null;
      noiseBuf = null;
      failed = true;
      return false;
    }
  }

  function impulse(seconds, decay) {
    const rate = ctx.sampleRate;
    const length = Math.floor(rate * seconds);
    const buffer = ctx.createBuffer(2, length, rate);
    for (let channel = 0; channel < 2; channel += 1) {
      const data = buffer.getChannelData(channel);
      for (let i = 0; i < length; i += 1) {
        data[i] = (Math.random() * 2 - 1) * (1 - i / length) ** decay;
      }
    }
    return buffer;
  }

  function vary(value, amount = 0.06) {
    return value * (1 + (Math.random() * 2 - 1) * amount);
  }

  function take(dur) {
    if (active >= 14) return false;
    active += 1;
    window.setTimeout(() => {
      active = Math.max(0, active - 1);
    }, Math.ceil((dur + 0.05) * 1000));
    return true;
  }

  function envGain(peak, dur, attack = 0.01, bus = sfxBus) {
    const gain = ctx.createGain();
    const now = ctx.currentTime;
    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.exponentialRampToValueAtTime(Math.max(0.0002, peak), now + attack);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + dur);
    gain.connect(bus);
    return gain;
  }

  function burst({ dur = 0.12, freq = 180, q = 0.8, type = 'bandpass', peak = 0.08, pan = 0, rate = 1, bus, verb = false }) {
    if (!ensure() || !take(dur)) return;
    const src = ctx.createBufferSource();
    src.buffer = noiseBuf;
    src.playbackRate.value = vary(rate, 0.08);
    const filter = ctx.createBiquadFilter();
    filter.type = type;
    filter.frequency.value = vary(freq, 0.05);
    filter.Q.value = q;
    const gain = envGain(peak, dur, 0.005, bus || sfxBus);
    const panner = ctx.createStereoPanner();
    panner.pan.value = Math.max(-1, Math.min(1, pan));
    src.connect(filter);
    filter.connect(panner);
    panner.connect(gain);
    if (verb && ctx._verb) {
      const send = ctx.createGain();
      send.gain.value = 0.35;
      panner.connect(send);
      send.connect(ctx._verb);
    }
    src.start();
    src.stop(ctx.currentTime + dur + 0.02);
  }

  function tone({ freq = 220, dur = 0.2, type = 'sine', peak = 0.04, pan = 0, slide = 0, bus }) {
    if (!ensure() || !take(dur)) return;
    const osc = ctx.createOscillator();
    osc.type = type;
    const start = vary(freq, 0.035);
    osc.frequency.setValueAtTime(start, ctx.currentTime);
    if (slide) osc.frequency.exponentialRampToValueAtTime(Math.max(40, start + slide), ctx.currentTime + dur);
    const gain = envGain(peak, dur, 0.02, bus || sfxBus);
    const panner = ctx.createStereoPanner();
    panner.pan.value = Math.max(-1, Math.min(1, pan));
    osc.connect(panner);
    panner.connect(gain);
    osc.start();
    osc.stop(ctx.currentTime + dur + 0.02);
  }

  function makeDrone() {
    const a = ctx.createOscillator();
    const b = ctx.createOscillator();
    a.type = 'sine';
    b.type = 'triangle';
    a.frequency.value = 48;
    b.frequency.value = 72;
    const gain = ctx.createGain();
    gain.gain.value = 0.02;
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 220;
    a.connect(filter);
    b.connect(filter);
    filter.connect(gain);
    gain.connect(ambBus);
    a.start();
    b.start();
    return { a, b, gain, filter };
  }

  function makeHum() {
    const freqs = [196, 233, 196, 174, 146];
    const oscs = freqs.map((freq, i) => {
      const osc = ctx.createOscillator();
      osc.type = 'triangle';
      osc.frequency.value = freq;
      osc.detune.value = (i - 2) * 7;
      return osc;
    });
    const gain = ctx.createGain();
    gain.gain.value = 0.0001;
    const filter = ctx.createBiquadFilter();
    filter.type = 'bandpass';
    filter.frequency.value = 700;
    filter.Q.value = 3.2;
    const panner = ctx.createStereoPanner();
    for (const osc of oscs) {
      osc.connect(filter);
      osc.start();
    }
    filter.connect(panner);
    panner.connect(gain);
    gain.connect(musicBus);
    return { gain, panner, filter };
  }

  return {
    unlock() {
      if (!ensure()) return;
      if (ctx.state === 'suspended') ctx.resume();
    },
    setVolume(value) {
      volume = value;
      if (master) master.gain.value = value;
    },
    setBuses(next) {
      buses = { ...buses, ...next };
      if (!musicBus) return;
      musicBus.gain.value = buses.music;
      sfxBus.gain.value = buses.sfx;
      ambBus.gain.value = buses.ambience;
    },
    zone(id) {
      if (!ensure() || !id || id === zoneId) return;
      zoneId = id;
      const pair = ZONES[id] || [48, 70];
      const now = ctx.currentTime;
      drone.a.frequency.setTargetAtTime(pair[0], now, 0.6);
      drone.b.frequency.setTargetAtTime(pair[1], now, 0.6);
    },
    foot(surface, intensity) {
      const freq = { wood: 170, carpet: 120, tile: 340, concrete: 100, grass: 86 }[surface] || 150;
      burst({ dur: 0.06, freq: vary(freq, 0.12), q: 0.85, peak: 0.04 * intensity, rate: 0.55 + intensity });
      tone({ freq: Math.max(42, freq * 0.4), dur: 0.05, type: 'sine', peak: 0.022 * intensity });
    },
    moan(pan, volumeMul, kind) {
      tone({
        freq: kind === 'grin' ? 88 : 66,
        dur: kind === 'grin' ? 0.72 : 0.42,
        type: 'sawtooth',
        peak: 0.03 * volumeMul,
        pan,
        slide: kind === 'grin' ? -28 : 10,
      });
      burst({
        dur: 0.38,
        freq: kind === 'grin' ? 480 : 280,
        q: 1.3,
        peak: 0.028 * volumeMul,
        pan,
        rate: 0.32,
        verb: true,
      });
    },
    creak(pan) {
      burst({ dur: 0.32, freq: vary(230, 0.1), q: 6, peak: 0.045, pan, rate: 0.24, verb: true });
      tone({ freq: 300, dur: 0.36, type: 'sine', peak: 0.018, pan, slide: -130 });
    },
    door() {
      burst({ dur: 0.16, freq: 110, q: 0.7, peak: 0.07, type: 'lowpass' });
      tone({ freq: 90, dur: 0.12, type: 'triangle', peak: 0.02, slide: -20 });
    },
    swing() {
      burst({ dur: 0.08, freq: vary(860, 0.15), q: 0.55, peak: 0.035, rate: 1.3 });
    },
    shot() {
      burst({ dur: 0.16, freq: 80, q: 0.45, peak: 0.16, type: 'lowpass', rate: 0.38 });
      burst({ dur: 0.4, freq: 150, q: 0.4, peak: 0.05, type: 'lowpass', rate: 0.2, verb: true });
      tone({ freq: 130, dur: 0.1, type: 'square', peak: 0.04, slide: -70 });
    },
    hit() {
      burst({ dur: 0.08, freq: vary(190, 0.14), q: 0.75, peak: 0.08 });
      tone({ freq: 140, dur: 0.05, type: 'triangle', peak: 0.02, slide: -40 });
    },
    hurt() {
      tone({ freq: 96, dur: 0.2, type: 'sawtooth', peak: 0.05, slide: -36 });
    },
    pickup() {
      tone({ freq: 540, dur: 0.06, type: 'sine', peak: 0.028 });
      tone({ freq: 720, dur: 0.07, type: 'sine', peak: 0.016 });
    },
    ui() {
      tone({ freq: 660, dur: 0.04, type: 'square', peak: 0.012 });
    },
    shutter() {
      burst({ dur: 0.045, freq: 1700, q: 0.35, peak: 0.05 });
      burst({ dur: 0.1, freq: 180, q: 0.5, peak: 0.04, type: 'lowpass' });
    },
    flash() {
      burst({ dur: 0.07, freq: 1300, q: 0.28, peak: 0.06 });
    },
    stinger() {
      tone({ freq: 196, dur: 0.42, type: 'sawtooth', peak: 0.035, slide: -18, bus: musicBus });
      tone({ freq: 207, dur: 0.48, type: 'sine', peak: 0.03, slide: -8, bus: musicBus });
    },
    heart(strength) {
      if (!ensure()) return;
      const now = ctx.currentTime;
      if (heartAt > now) return;
      const gap = 0.96 - Math.min(0.62, strength) * 0.68;
      heartAt = now + gap;
      tone({ freq: 58, dur: 0.06, type: 'sine', peak: 0.04 * strength, bus: musicBus });
      tone({ freq: 42, dur: 0.1, type: 'sine', peak: 0.026 * strength, slide: -6, bus: musicBus });
    },
    voice() {
      if (!ensure()) return;
      tone({ freq: 196, dur: 0.32, type: 'triangle', peak: 0.018, bus: musicBus });
      tone({ freq: 233, dur: 0.38, type: 'triangle', peak: 0.014, slide: -12, bus: musicBus });
    },
    mix({ danger = 0, humVol = 0, humPan = 0, distorted = false }) {
      if (!drone || !hum) return;
      const now = ctx.currentTime;
      drone.gain.gain.setTargetAtTime(0.018 + danger * 0.04, now, 0.25);
      drone.filter.frequency.setTargetAtTime(180 + danger * 80, now, 0.3);
      hum.gain.gain.setTargetAtTime(Math.max(0.0001, humVol), now, 0.15);
      hum.panner.pan.setTargetAtTime(Math.max(-1, Math.min(1, humPan)), now, 0.1);
      hum.filter.frequency.setTargetAtTime(distorted ? 380 : 820, now, 0.2);
    },
  };
}
