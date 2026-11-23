// Small procedural horror bed. No samples — the house makes its own noises.
export function createAudio() {
  let ctx = null;
  let master = null;
  let volume = 0.85;
  let drone = null;
  let hum = null;
  let noiseBuf = null;
  let failed = false;
  let heartAt = 0;

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
      master.connect(ctx.destination);
      const seconds = 1;
      noiseBuf = ctx.createBuffer(1, ctx.sampleRate * seconds, ctx.sampleRate);
      const data = noiseBuf.getChannelData(0);
      for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
      drone = makeDrone();
      hum = makeHum();
      return true;
    } catch {
      ctx = null;
      master = null;
      drone = null;
      hum = null;
      noiseBuf = null;
      failed = true;
      return false;
    }
  }

  function envGain(peak, dur, attack = 0.01) {
    const gain = ctx.createGain();
    const now = ctx.currentTime;
    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.exponentialRampToValueAtTime(Math.max(0.0002, peak), now + attack);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + dur);
    gain.connect(master);
    return gain;
  }

  function burst({ dur = 0.12, freq = 180, q = 0.8, type = 'bandpass', peak = 0.08, pan = 0, rate = 1 }) {
    if (!ensure()) return;
    const src = ctx.createBufferSource();
    src.buffer = noiseBuf;
    src.playbackRate.value = rate;
    const filter = ctx.createBiquadFilter();
    filter.type = type;
    filter.frequency.value = freq;
    filter.Q.value = q;
    const gain = envGain(peak, dur, 0.005);
    const panner = ctx.createStereoPanner();
    panner.pan.value = Math.max(-1, Math.min(1, pan));
    src.connect(filter);
    filter.connect(panner);
    panner.connect(gain);
    src.start();
    src.stop(ctx.currentTime + dur + 0.02);
  }

  function tone({ freq = 220, dur = 0.2, type = 'sine', peak = 0.04, pan = 0, slide = 0 }) {
    if (!ensure()) return;
    const osc = ctx.createOscillator();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, ctx.currentTime);
    if (slide) osc.frequency.exponentialRampToValueAtTime(Math.max(40, freq + slide), ctx.currentTime + dur);
    const gain = envGain(peak, dur, 0.02);
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
    b.type = 'sine';
    a.frequency.value = 55;
    b.frequency.value = 82;
    const gain = ctx.createGain();
    gain.gain.value = 0.012;
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 240;
    a.connect(filter);
    b.connect(filter);
    filter.connect(gain);
    gain.connect(master);
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
      osc.detune.value = (i - 2) * 8;
      return osc;
    });
    const gain = ctx.createGain();
    gain.gain.value = 0.0001;
    const filter = ctx.createBiquadFilter();
    filter.type = 'bandpass';
    filter.frequency.value = 700;
    filter.Q.value = 4;
    const panner = ctx.createStereoPanner();
    for (const osc of oscs) {
      osc.connect(filter);
      osc.start();
    }
    filter.connect(panner);
    panner.connect(gain);
    gain.connect(master);
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
    foot(surface, intensity) {
      const freq = { wood: 180, carpet: 140, tile: 320, concrete: 110, grass: 90 }[surface] || 160;
      burst({ dur: 0.07, freq, q: 0.9, peak: 0.045 * intensity, rate: 0.6 + intensity });
      tone({ freq: Math.max(48, freq * 0.42), dur: 0.07, type: 'sine', peak: 0.028 * intensity });
    },
    moan(pan, volumeMul, kind) {
      tone({
        freq: kind === 'grin' ? 92 : 70,
        dur: kind === 'grin' ? 0.7 : 0.45,
        type: 'sawtooth',
        peak: 0.035 * volumeMul,
        pan,
        slide: kind === 'grin' ? -30 : 12,
      });
      burst({ dur: 0.4, freq: kind === 'grin' ? 500 : 300, q: 1.4, peak: 0.03 * volumeMul, pan, rate: 0.35 });
    },
    creak(pan) {
      burst({ dur: 0.35, freq: 240, q: 6, peak: 0.05, pan, rate: 0.25 });
      tone({ freq: 310, dur: 0.4, type: 'sine', peak: 0.02, pan, slide: -140 });
    },
    door() {
      burst({ dur: 0.18, freq: 120, q: 0.7, peak: 0.08, type: 'lowpass' });
    },
    swing() {
      burst({ dur: 0.09, freq: 900, q: 0.6, peak: 0.04, rate: 1.4 });
    },
    shot() {
      burst({ dur: 0.18, freq: 90, q: 0.5, peak: 0.2, type: 'lowpass', rate: 0.4 });
      burst({ dur: 0.42, freq: 160, q: 0.45, peak: 0.07, type: 'lowpass', rate: 0.22 });
      tone({ freq: 140, dur: 0.12, type: 'square', peak: 0.05, slide: -80 });
    },
    hit() {
      burst({ dur: 0.1, freq: 200, q: 0.8, peak: 0.09 });
    },
    hurt() {
      tone({ freq: 98, dur: 0.22, type: 'sawtooth', peak: 0.06, slide: -40 });
    },
    pickup() {
      tone({ freq: 520, dur: 0.07, type: 'sine', peak: 0.03 });
    },
    shutter() {
      burst({ dur: 0.05, freq: 1800, q: 0.4, peak: 0.06 });
      burst({ dur: 0.12, freq: 200, q: 0.5, peak: 0.05, type: 'lowpass' });
    },
    flash() {
      burst({ dur: 0.08, freq: 1400, q: 0.3, peak: 0.07 });
    },
    stinger() {
      tone({ freq: 196, dur: 0.45, type: 'sawtooth', peak: 0.04, slide: -20 });
      tone({ freq: 207, dur: 0.5, type: 'sine', peak: 0.035, slide: -10 });
    },
    heart(strength) {
      if (!ensure()) return;
      const now = ctx.currentTime;
      if (heartAt > now) return;
      const gap = 0.98 - Math.min(0.62, strength) * 0.7;
      heartAt = now + gap;
      tone({ freq: 62, dur: 0.07, type: 'sine', peak: 0.045 * strength });
      tone({ freq: 44, dur: 0.11, type: 'sine', peak: 0.03 * strength, slide: -8 });
    },
    voice() {
      if (!ensure()) return;
      tone({ freq: 196, dur: 0.35, type: 'triangle', peak: 0.02 });
      tone({ freq: 233, dur: 0.4, type: 'triangle', peak: 0.016, slide: -12 });
    },
    mix({ danger = 0, humVol = 0, humPan = 0, distorted = false }) {
      if (!drone || !hum) return;
      const now = ctx.currentTime;
      drone.gain.gain.setTargetAtTime(0.01 + danger * 0.03, now, 0.2);
      drone.a.frequency.setTargetAtTime(52 + danger * 10, now, 0.3);
      drone.b.frequency.setTargetAtTime(78 + danger * 20, now, 0.3);
      hum.gain.gain.setTargetAtTime(Math.max(0.0001, humVol), now, 0.15);
      hum.panner.pan.setTargetAtTime(Math.max(-1, Math.min(1, humPan)), now, 0.1);
      hum.filter.frequency.setTargetAtTime(distorted ? 420 : 860, now, 0.2);
    },
  };
}
