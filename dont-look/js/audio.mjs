// Baked stacks: transient, body, material, and air. Music is four looping stems.
const ZONES = {
  mud: [50, 74, 0.22],
  foyer: [48, 70, 0.2],
  living: [46, 68, 0.18],
  dining: [45, 66, 0.2],
  kitchen: [52, 78, 0.16],
  hallN: [44, 64, 0.24],
  hallS: [44, 64, 0.24],
  bath: [55, 82, 0.34],
  chris: [47, 71, 0.14],
  her: [54, 80, 0.15],
  mom: [43, 63, 0.16],
  garage: [40, 58, 0.12],
  basement: [36, 54, 0.4],
  yard: [58, 86, 0.08],
  shed: [42, 62, 0.18],
};

const CHORDS = [
  [146.83, 174.61, 220.0],
  [116.54, 146.83, 174.61],
  [87.31, 130.81, 174.61],
  [98.0, 123.47, 146.83],
  [146.83, 174.61, 220.0],
  [98.0, 116.54, 146.83],
  [116.54, 146.83, 174.61],
  [110.0, 138.59, 164.81],
];

export function createAudio() {
  let ctx = null;
  let master = null;
  let musicBus = null;
  let sfxBus = null;
  let ambBus = null;
  let uiBus = null;
  let volume = 0.85;
  let buses = { music: 0.85, sfx: 0.9, ambience: 0.8 };
  let drone = null;
  let hum = null;
  let failed = false;
  let heartAt = 0;
  let active = 0;
  let zoneId = '';
  let bank = null;
  let stems = null;
  let verb = null;

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
      comp.threshold.value = -14;
      comp.knee.value = 10;
      comp.ratio.value = 3;
      comp.attack.value = 0.005;
      comp.release.value = 0.22;
      const limit = ctx.createDynamicsCompressor();
      limit.threshold.value = -3;
      limit.knee.value = 0;
      limit.ratio.value = 12;
      limit.attack.value = 0.002;
      limit.release.value = 0.08;
      comp.connect(limit);
      limit.connect(master);
      master.connect(ctx.destination);
      musicBus = ctx.createGain();
      sfxBus = ctx.createGain();
      ambBus = ctx.createGain();
      uiBus = ctx.createGain();
      musicBus.gain.value = buses.music;
      sfxBus.gain.value = buses.sfx;
      ambBus.gain.value = buses.ambience;
      uiBus.gain.value = Math.min(1, buses.sfx);
      musicBus.connect(comp);
      sfxBus.connect(comp);
      ambBus.connect(comp);
      uiBus.connect(comp);
      verb = ctx.createConvolver();
      verb.buffer = impulse(1.4, 2.2);
      const wet = ctx.createGain();
      wet.gain.value = 0.2;
      verb.connect(wet);
      wet.connect(ambBus);
      ctx._wet = wet;
      bank = bakeBank();
      drone = makeDrone();
      hum = makeHum();
      stems = startScore();
      return true;
    } catch {
      ctx = null;
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

  function bufferFrom(recipe, seconds) {
    const rate = ctx.sampleRate;
    const length = Math.floor(rate * seconds);
    const data = new Float32Array(length);
    recipe(data, rate);
    let peak = 0;
    for (let i = 0; i < length; i += 1) peak = Math.max(peak, Math.abs(data[i]));
    const gain = peak > 0.85 ? 0.85 / peak : 1;
    const buffer = ctx.createBuffer(1, length, rate);
    const out = buffer.getChannelData(0);
    for (let i = 0; i < length; i += 1) out[i] = data[i] * gain;
    return buffer;
  }

  function addTone(data, rate, freq, start, dur, amp, harm, slide = 0) {
    const from = Math.max(0, Math.floor(start * rate));
    const to = Math.min(data.length, Math.floor((start + dur) * rate));
    for (let i = from; i < to; i += 1) {
      const t = (i - from) / rate;
      const env = Math.exp(-t / Math.max(0.02, dur * 0.42));
      const f = freq * (1 + slide * Math.min(1, t / dur));
      let sample = 0;
      for (let h = 0; h < harm.length; h += 1) {
        sample += Math.sin(2 * Math.PI * f * (h + 1) * t) * harm[h];
      }
      data[i] += sample * amp * env;
    }
  }

  function addNoise(data, rate, start, dur, amp, color) {
    const from = Math.max(0, Math.floor(start * rate));
    const to = Math.min(data.length, Math.floor((start + dur) * rate));
    let low = 0;
    const lp = color === 'air' ? 0.08 : color === 'wood' ? 0.22 : color === 'cloth' ? 0.35 : 0.55;
    for (let i = from; i < to; i += 1) {
      const t = (i - from) / Math.max(1, to - from);
      const n = Math.random() * 2 - 1;
      low += (n - low) * lp;
      const shaped = color === 'metal' ? n - low : low;
      const env = color === 'tick' ? Math.exp(-t * 18) : (1 - t) * Math.exp(-t * 3);
      data[i] += shaped * amp * env;
    }
  }

  function stack(data, rate, parts) {
    for (const part of parts) {
      if (part.noise) addNoise(data, rate, part.start || 0, part.dur, part.amp, part.noise);
      else addTone(data, rate, part.freq, part.start || 0, part.dur, part.amp, part.harm || [1, 0.35, 0.12], part.slide || 0);
    }
  }

  function bakeBank() {
    const foot = {};
    for (const surface of ['wood', 'carpet', 'tile', 'concrete', 'grass']) {
      const body = { wood: 140, carpet: 90, tile: 240, concrete: 80, grass: 70 }[surface];
      foot[surface] = [0, 1, 2, 3].map(() => bufferFrom((data, rate) => {
        stack(data, rate, [
          { noise: 'tick', dur: 0.03, amp: 0.7 },
          { freq: body * (0.94 + Math.random() * 0.12), dur: 0.09, amp: 0.45, harm: [1, 0.2] },
          { freq: body * 2.1, dur: 0.06, amp: surface === 'tile' ? 0.28 : 0.12, harm: [0.4, 1, 0.2] },
          { noise: surface === 'grass' ? 'cloth' : surface, start: 0.01, dur: 0.12, amp: 0.35 },
        ]);
      }, 0.18));
    }
    const take = (parts, seconds = 0.4) => bufferFrom((data, rate) => stack(data, rate, parts), seconds);
    return {
      foot,
      swing: {
        stick: take([{ noise: 'wood', dur: 0.12, amp: 0.4 }, { freq: 180, dur: 0.1, amp: 0.2, slide: -0.4 }, { noise: 'air', dur: 0.16, amp: 0.25 }]),
        knife: take([{ noise: 'metal', dur: 0.08, amp: 0.45 }, { freq: 880, dur: 0.07, amp: 0.16, harm: [0.2, 1, 0.4] }, { noise: 'air', dur: 0.1, amp: 0.2 }]),
        pistol: take([{ noise: 'tick', dur: 0.02, amp: 0.5 }, { freq: 140, dur: 0.05, amp: 0.3 }, { noise: 'metal', dur: 0.06, amp: 0.2 }]),
        fist: take([{ noise: 'cloth', dur: 0.08, amp: 0.4 }, { freq: 90, dur: 0.06, amp: 0.25 }, { noise: 'air', dur: 0.08, amp: 0.15 }]),
        camera: take([{ noise: 'tick', dur: 0.03, amp: 0.4 }, { freq: 420, dur: 0.05, amp: 0.12 }, { noise: 'air', dur: 0.08, amp: 0.18 }]),
      },
      shot: take([
        { noise: 'tick', dur: 0.015, amp: 0.9 },
        { freq: 90, dur: 0.12, amp: 0.7, slide: -0.55, harm: [1, 0.4, 0.15] },
        { noise: 'air', dur: 0.22, amp: 0.45 },
        { freq: 180, start: 0.02, dur: 0.18, amp: 0.15, harm: [0.3, 0.2, 1] },
      ], 0.45),
      hit: take([{ noise: 'cloth', dur: 0.05, amp: 0.6 }, { freq: 110, dur: 0.08, amp: 0.4, slide: -0.3 }, { noise: 'wood', dur: 0.1, amp: 0.25 }, { freq: 60, dur: 0.12, amp: 0.2 }]),
      grinHit: take([{ noise: 'tick', dur: 0.02, amp: 0.4 }, { freq: 70, dur: 0.16, amp: 0.35, slide: -0.2 }, { noise: 'air', dur: 0.2, amp: 0.3 }, { freq: 210, dur: 0.1, amp: 0.08 }]),
      hurt: take([{ freq: 196, dur: 0.18, amp: 0.25, slide: -0.35, harm: [1, 0.5, 0.2] }, { noise: 'cloth', dur: 0.12, amp: 0.3 }, { freq: 90, dur: 0.2, amp: 0.2 }]),
      door: {
        wood: take([{ noise: 'wood', dur: 0.12, amp: 0.45 }, { freq: 120, dur: 0.16, amp: 0.25, slide: -0.2 }, { noise: 'air', dur: 0.2, amp: 0.2 }], 0.35),
        yard: take([{ noise: 'metal', dur: 0.2, amp: 0.35 }, { freq: 70, dur: 0.28, amp: 0.3 }, { noise: 'air', dur: 0.3, amp: 0.25 }], 0.45),
        base: take([{ noise: 'metal', dur: 0.08, amp: 0.5 }, { freq: 90, dur: 0.14, amp: 0.28 }, { noise: 'tick', dur: 0.04, amp: 0.3 }], 0.3),
        shed: take([{ noise: 'wood', dur: 0.1, amp: 0.4 }, { freq: 160, dur: 0.08, amp: 0.15 }, { noise: 'air', dur: 0.12, amp: 0.2 }]),
      },
      creak: take([{ freq: 520, dur: 0.28, amp: 0.12, slide: -0.45, harm: [0.2, 1, 0.3] }, { noise: 'wood', dur: 0.3, amp: 0.28 }, { noise: 'air', dur: 0.22, amp: 0.12 }], 0.4),
      families: {
        wood: take([{ noise: 'wood', dur: 0.08, amp: 0.4 }, { freq: 180, dur: 0.1, amp: 0.2 }, { noise: 'air', dur: 0.1, amp: 0.15 }]),
        metal: take([{ noise: 'metal', dur: 0.06, amp: 0.45 }, { freq: 640, dur: 0.08, amp: 0.12, harm: [0.3, 1] }, { noise: 'tick', dur: 0.03, amp: 0.3 }]),
        brass: take([{ noise: 'tick', dur: 0.02, amp: 0.45 }, { freq: 740, dur: 0.12, amp: 0.16, harm: [1, 0.4, 0.2] }, { freq: 1480, dur: 0.06, amp: 0.06 }]),
        cloth: take([{ noise: 'cloth', dur: 0.1, amp: 0.4 }, { freq: 140, dur: 0.08, amp: 0.1 }, { noise: 'air', dur: 0.08, amp: 0.12 }]),
        plastic: take([{ noise: 'tick', dur: 0.03, amp: 0.35 }, { freq: 300, dur: 0.06, amp: 0.12 }, { noise: 'cloth', dur: 0.08, amp: 0.2 }]),
        paper: take([{ noise: 'cloth', dur: 0.07, amp: 0.35 }, { freq: 420, dur: 0.04, amp: 0.06 }, { noise: 'air', dur: 0.06, amp: 0.15 }]),
        liquid: take([{ noise: 'cloth', dur: 0.16, amp: 0.4 }, { freq: 80, dur: 0.18, amp: 0.25 }, { noise: 'air', dur: 0.12, amp: 0.15 }], 0.3),
        glass: take([{ noise: 'tick', dur: 0.02, amp: 0.4 }, { freq: 1200, dur: 0.08, amp: 0.1, harm: [0.2, 0.5, 1] }, { noise: 'air', dur: 0.06, amp: 0.12 }]),
        ceramic: take([{ noise: 'tick', dur: 0.03, amp: 0.4 }, { freq: 480, dur: 0.08, amp: 0.14 }, { noise: 'metal', dur: 0.05, amp: 0.18 }]),
      },
      moan: take([{ freq: 92, dur: 0.42, amp: 0.3, slide: 0.15, harm: [1, 0.45, 0.2, 0.08] }, { noise: 'air', dur: 0.36, amp: 0.22 }, { freq: 180, start: 0.05, dur: 0.3, amp: 0.08 }], 0.5),
      grin: take([{ freq: 78, dur: 0.6, amp: 0.28, slide: -0.25, harm: [1, 0.2, 0.55, 0.1] }, { noise: 'air', dur: 0.5, amp: 0.25 }, { freq: 156, dur: 0.4, amp: 0.1 }], 0.7),
      yard: take([{ freq: 70, dur: 0.36, amp: 0.25, harm: [1, 0.3] }, { noise: 'cloth', dur: 0.3, amp: 0.35 }, { noise: 'air', dur: 0.28, amp: 0.2 }], 0.45),
      ui: take([{ freq: 660, dur: 0.04, amp: 0.2, harm: [1, 0.2] }, { freq: 990, start: 0.02, dur: 0.05, amp: 0.08 }, { noise: 'tick', dur: 0.015, amp: 0.15 }], 0.1),
      shutter: take([{ noise: 'tick', dur: 0.02, amp: 0.6 }, { noise: 'metal', dur: 0.05, amp: 0.3 }, { freq: 180, dur: 0.06, amp: 0.2 }]),
      flash: take([{ noise: 'air', dur: 0.05, amp: 0.45 }, { freq: 1400, dur: 0.04, amp: 0.08 }, { noise: 'tick', dur: 0.02, amp: 0.3 }]),
      stinger: take([{ freq: 196, dur: 0.45, amp: 0.2, slide: -0.08, harm: [1, 0.4, 0.2] }, { freq: 207, dur: 0.5, amp: 0.14 }, { freq: 155, dur: 0.4, amp: 0.12 }, { noise: 'air', dur: 0.3, amp: 0.1 }], 0.6),
      heart: take([{ freq: 58, dur: 0.08, amp: 0.6, harm: [1, 0.3] }, { freq: 42, start: 0.09, dur: 0.1, amp: 0.35 }, { noise: 'cloth', dur: 0.05, amp: 0.2 }], 0.22),
      voice: take([{ freq: 196, dur: 0.32, amp: 0.16, harm: [1, 0.45, 0.15] }, { freq: 233, dur: 0.36, amp: 0.1, slide: -0.05 }, { noise: 'air', dur: 0.2, amp: 0.08 }], 0.4),
    };
  }

  function startScore() {
    const seconds = 12;
    const rate = ctx.sampleRate;
    const pad = bufferFrom((data) => writePad(data, rate, seconds), seconds);
    const bass = bufferFrom((data) => writeBass(data, rate, seconds), seconds);
    const motif = bufferFrom((data) => writeMotif(data, rate, seconds), seconds);
    const pulse = bufferFrom((data) => writePulse(data, rate, seconds), seconds);
    const make = (buffer, gainValue) => {
      const src = ctx.createBufferSource();
      src.buffer = buffer;
      src.loop = true;
      const gain = ctx.createGain();
      gain.gain.value = gainValue;
      src.connect(gain);
      gain.connect(musicBus);
      src.start();
      return gain;
    };
    return {
      pad: make(pad, 0.08),
      bass: make(bass, 0.05),
      motif: make(motif, 0.04),
      pulse: make(pulse, 0.0001),
    };
  }

  function writePad(data, rate, seconds) {
    const bar = seconds / CHORDS.length;
    for (let i = 0; i < data.length; i += 1) {
      const t = i / rate;
      const chord = CHORDS[Math.min(CHORDS.length - 1, Math.floor(t / bar))];
      const trem = 0.85 + 0.15 * Math.sin(t * 0.6);
      let sample = 0;
      for (const freq of chord) {
        sample += Math.sin(2 * Math.PI * freq * t) * 0.2;
        sample += Math.sin(2 * Math.PI * freq * 1.003 * t) * 0.12;
        sample += Math.sin(2 * Math.PI * freq * 2 * t) * 0.04;
      }
      const edge = Math.min(1, t / 0.4, (seconds - t) / 0.4);
      data[i] = sample * trem * edge;
    }
  }

  function writeBass(data, rate, seconds) {
    const bar = seconds / CHORDS.length;
    for (let i = 0; i < data.length; i += 1) {
      const t = i / rate;
      const freq = CHORDS[Math.min(CHORDS.length - 1, Math.floor(t / bar))][0] / 2;
      const local = (t % bar) / bar;
      const env = Math.exp(-local * 2.2);
      data[i] = (Math.sin(2 * Math.PI * freq * t) * 0.7 + Math.sin(2 * Math.PI * freq * 2 * t) * 0.18) * env;
    }
  }

  function writeMotif(data, rate, seconds) {
    const notes = [0.4, 440, 1.3, 392, 2.4, 349, 3.8, 392, 5.1, 330, 6.4, 349, 7.8, 294, 9.2, 330, 10.4, 392];
    for (let n = 0; n < notes.length; n += 2) {
      addTone(data, rate, notes[n + 1], notes[n], 0.55, 0.34, [1, 0.45, 0.18, 0.05]);
    }
    void seconds;
  }

  function writePulse(data, rate, seconds) {
    const bar = seconds / CHORDS.length;
    for (let barIndex = 0; barIndex < CHORDS.length; barIndex += 1) {
      addTone(data, rate, 72, barIndex * bar, 0.12, 0.7, [1, 0.2], -0.4);
      addNoise(data, rate, barIndex * bar + bar * 0.5, 0.04, 0.18, 'tick');
    }
  }

  function takeSlot(dur) {
    if (active >= 16) return false;
    active += 1;
    window.setTimeout(() => {
      active = Math.max(0, active - 1);
    }, Math.ceil((dur + 0.05) * 1000));
    return true;
  }

  function play(buffer, { pan = 0, gain = 0.2, bus = sfxBus, verbSend = 0, rate = 1 } = {}) {
    if (!ensure() || !buffer || !takeSlot(buffer.duration || 0.2)) return;
    const src = ctx.createBufferSource();
    src.buffer = buffer;
    src.playbackRate.value = rate * (0.96 + Math.random() * 0.08);
    const amp = ctx.createGain();
    amp.gain.value = gain * (0.92 + Math.random() * 0.12);
    const panner = ctx.createStereoPanner();
    panner.pan.value = Math.max(-1, Math.min(1, pan));
    src.connect(panner);
    panner.connect(amp);
    amp.connect(bus || sfxBus);
    if (verbSend > 0 && verb) {
      const send = ctx.createGain();
      send.gain.value = verbSend;
      amp.connect(send);
      send.connect(verb);
    }
    src.start();
  }

  function familyFor(id) {
    if (!id) return 'wood';
    if (String(id).startsWith('key')) return 'brass';
    return {
      stick: 'wood',
      knife: 'metal',
      pistol: 'metal',
      ammo: 'brass',
      bandage: 'cloth',
      pills: 'plastic',
      film: 'paper',
      tape: 'cloth',
      fuse: 'ceramic',
      gas: 'liquid',
      camera: 'glass',
      flashlight: 'metal',
      battery: 'plastic',
      note: 'paper',
    }[id] || 'wood';
  }

  function makeDrone() {
    const a = ctx.createOscillator();
    const b = ctx.createOscillator();
    a.type = 'sine';
    b.type = 'triangle';
    a.frequency.value = 48;
    b.frequency.value = 72;
    const gain = ctx.createGain();
    gain.gain.value = 0.012;
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 200;
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
      osc.detune.value = (i - 2) * 6;
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

  function setStem(node, value) {
    if (!node) return;
    node.gain.setTargetAtTime(Math.max(0.0001, value), ctx.currentTime, 0.4);
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
      uiBus.gain.value = Math.min(1, buses.sfx);
    },
    zone(id) {
      if (!ensure() || !id || id === zoneId) return;
      zoneId = id;
      const pair = ZONES[id] || [48, 70, 0.2];
      const now = ctx.currentTime;
      drone.a.frequency.setTargetAtTime(pair[0], now, 0.7);
      drone.b.frequency.setTargetAtTime(pair[1], now, 0.7);
      ctx._wet.gain.setTargetAtTime(pair[2], now, 0.5);
    },
    foot(surface, intensity = 0.6) {
      if (!ensure()) return;
      const set = bank.foot[surface] || bank.foot.concrete;
      play(set[Math.floor(Math.random() * set.length)], { gain: 0.16 * intensity, verbSend: 0.15 });
    },
    moan(pan, volumeMul, kind, id) {
      if (!ensure()) return;
      const buffer = id === 'yardbug' ? bank.yard : kind === 'grin' ? bank.grin : bank.moan;
      play(buffer, { pan, gain: 0.18 * volumeMul, verbSend: 0.45, rate: 0.96 + Math.random() * 0.08 });
    },
    creak(pan) {
      if (!ensure()) return;
      play(bank.creak, { pan, gain: 0.14, verbSend: 0.4 });
    },
    door(id) {
      if (!ensure()) return;
      const buffer = bank.door[id] || bank.door.wood;
      play(buffer, { gain: 0.2, verbSend: id === 'yard' ? 0.15 : 0.28 });
    },
    swing(id) {
      if (!ensure()) return;
      play(bank.swing[id] || bank.swing.fist, { gain: 0.14, verbSend: 0.08 });
    },
    shot() {
      if (!ensure()) return;
      play(bank.shot, { gain: 0.28, verbSend: 0.35 });
    },
    hit(kind) {
      if (!ensure()) return;
      play(kind === 'grin' ? bank.grinHit : bank.hit, { gain: 0.2, verbSend: 0.12 });
    },
    hurt() {
      if (!ensure()) return;
      play(bank.hurt, { gain: 0.18, verbSend: 0.1 });
    },
    pickup(id) {
      if (!ensure()) return;
      play(bank.families[familyFor(id)], { gain: 0.12, verbSend: 0.05 });
    },
    ui() {
      if (!ensure()) return;
      play(bank.ui, { gain: 0.08, bus: uiBus });
    },
    shutter() {
      if (!ensure()) return;
      play(bank.shutter, { gain: 0.16 });
    },
    flash() {
      if (!ensure()) return;
      play(bank.flash, { gain: 0.14 });
    },
    stinger() {
      if (!ensure()) return;
      play(bank.stinger, { gain: 0.16, bus: musicBus, verbSend: 0.2 });
    },
    heart(strength) {
      if (!ensure()) return;
      const now = ctx.currentTime;
      if (heartAt > now) return;
      heartAt = now + (0.92 - Math.min(0.6, strength) * 0.62);
      play(bank.heart, { gain: 0.12 * strength, bus: musicBus });
    },
    voice() {
      if (!ensure()) return;
      play(bank.voice, { gain: 0.1, bus: musicBus, verbSend: 0.25 });
    },
    mix({ danger = 0, humVol = 0, humPan = 0, distorted = false }) {
      if (!drone || !hum || !stems) return;
      const now = ctx.currentTime;
      drone.gain.gain.setTargetAtTime(0.01 + danger * 0.02, now, 0.3);
      drone.filter.frequency.setTargetAtTime(160 + danger * 70, now, 0.3);
      hum.gain.gain.setTargetAtTime(Math.max(0.0001, humVol), now, 0.15);
      hum.panner.pan.setTargetAtTime(Math.max(-1, Math.min(1, humPan)), now, 0.1);
      hum.filter.frequency.setTargetAtTime(distorted ? 360 : 820, now, 0.2);
      const quiet = zoneId === 'mom' || zoneId === 'her';
      const chase = danger > 0.5;
      const tension = danger > 0.16;
      setStem(stems.pad, quiet ? 0.11 : chase ? 0.05 : 0.09);
      setStem(stems.bass, chase ? 0.1 : tension ? 0.07 : 0.045);
      setStem(stems.motif, chase ? 0.02 : quiet ? 0.07 : 0.045);
      setStem(stems.pulse, chase ? 0.08 : tension ? 0.035 : 0.0001);
    },
  };
}
