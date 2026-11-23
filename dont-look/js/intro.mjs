// Directed opening for Don't Look. The plot beats are the drive, Ben's texts,
// the trail photo, the arrival, and the dead car. This module only stages them.
import { captionHold } from './captions.mjs';
import { portrait, ready } from './photos.mjs';

export const STORY = {
  kicker: 'A house, after the photo',
  title: 'Don’t Look',
  tag: 'Chris was only driving home.',
  parts: 'Six slots. Four parts. The car is in the garage.',
  same: 'Same road',
  ben: 'Ben',
  chris: 'chris',
  void: 'i was messing with the trail photo. i opened something. a void.',
  doNot: 'do not open it.',
  img: 'IMG_4041',
  dont: 'don\u2019t open what ben sent.',
  looked: 'i looked. i\u2019m fine.',
  wake: 'You wake in the driver\u2019s seat. Same road. Wrong color. The phone is dead.',
  house: 'Mom\u2019s house is ahead. Every window is dark.',
  garage: 'You put the car in the garage. It doesn\u2019t turn over.',
  smile: 'The smile is in the picture.',
};

const VIEW = {
  titleA: { horizon: 0.63, vanish: 0.5, houseX: 0.5, houseS: 1.12, travel: 0.05, grade: 0.06, cabin: 0, lights: 1, headlights: 0, mirror: 0 },
  titleB: { horizon: 0.61, vanish: 0.53, houseX: 0.55, houseS: 1.2, travel: 0.2, grade: 0.1, cabin: 0, lights: 1, headlights: 0, mirror: 0 },
  roadA: { horizon: 0.57, vanish: 0.5, houseX: 0.72, houseS: 0.38, travel: 0, grade: 0.02, cabin: 0, lights: 0.75, headlights: 0.15, mirror: 0 },
  roadB: { horizon: 0.5, vanish: 0.46, houseX: 0.64, houseS: 0.72, travel: 1, grade: 0.16, cabin: 0.2, lights: 0.5, headlights: 1, mirror: 0 },
  phoneA: { horizon: 0.5, vanish: 0.48, houseX: 0.6, houseS: 0.78, travel: 1, grade: 0.2, cabin: 1, lights: 0.35, headlights: 0.7, mirror: 0 },
  phoneB: { horizon: 0.47, vanish: 0.45, houseX: 0.57, houseS: 0.95, travel: 1.55, grade: 0.3, cabin: 1, lights: 0.22, headlights: 0.45, mirror: 0 },
  arriveA: { horizon: 0.5, vanish: 0.5, houseX: 0.62, houseS: 0.82, travel: 1.55, grade: 0.7, cabin: 1, lights: 0.12, headlights: 0.28, mirror: 1 },
  arriveB: { horizon: 0.44, vanish: 0.5, houseX: 0.54, houseS: 1.12, travel: 2.15, grade: 0.92, cabin: 0.9, lights: 0.02, headlights: 0.08, mirror: 1 },
  houseA: { horizon: 0.58, vanish: 0.5, houseX: 0.5, houseS: 1.02, travel: 2.15, grade: 0.48, cabin: 0, lights: 0.05, headlights: 0, mirror: 0 },
  houseB: { horizon: 0.66, vanish: 0.5, houseX: 0.5, houseS: 1.62, travel: 2.25, grade: 0.34, cabin: 0, lights: 0, headlights: 0, mirror: 0 },
  still: { horizon: 0.5, vanish: 0.5, houseX: 0.5, houseS: 1, travel: 0, grade: 0, cabin: 0, lights: 0, headlights: 0, mirror: 0 },
};

function scaleDur(seconds, still) {
  return Math.round(seconds * (still ? 0.62 : 1) * 100) / 100;
}

function fitReading(beat) {
  const marks = [...(beat.lines || [])];
  if (beat.aside) marks.push(beat.aside);
  let dur = beat.dur;
  for (const mark of marks) {
    const at = Math.min(0.92, mark.at || 0);
    const need = captionHold(mark.text);
    if ((1 - at) * dur < need) dur = need / (1 - at);
  }
  return { ...beat, dur: Math.round(dur * 100) / 100 };
}

export function sequenceBeats(still) {
  const d = (seconds) => scaleDur(seconds, still);
  return [
    {
      id: 'drive',
      dur: d(6.6),
      scene: 'world',
      bed: 'intro',
      from: VIEW.roadA,
      to: VIEW.roadB,
      kicker: STORY.same,
      lines: [{ at: 0.14, text: STORY.tag }],
      cues: [],
    },
    {
      id: 'phone',
      dur: d(10.6),
      scene: 'cabin',
      bed: 'tight',
      from: VIEW.phoneA,
      to: VIEW.phoneB,
      kicker: STORY.ben,
      lines: [
        { at: 0.06, text: STORY.chris },
        { at: 0.24, text: STORY.void },
        { at: 0.5, text: STORY.doNot },
        { at: 0.74, text: STORY.img },
      ],
      cues: [
        { at: 0.06, id: 'p1', play: (audio) => audio.phoneBuzz() },
        { at: 0.24, id: 'p2', play: (audio) => audio.phoneBuzz() },
        { at: 0.5, id: 'p3', play: (audio) => audio.phoneBuzz() },
      ],
    },
    {
      id: 'fine',
      dur: d(5.6),
      scene: 'fine',
      bed: 'intro',
      from: VIEW.still,
      to: VIEW.still,
      kicker: '',
      lines: [
        { at: 0.1, text: STORY.dont },
        { at: 0.42, text: STORY.looked },
      ],
      cues: [],
    },
    {
      id: 'photo',
      dur: d(5.2),
      scene: 'photo',
      bed: 'tight',
      from: VIEW.still,
      to: VIEW.still,
      kicker: STORY.img,
      lines: [{ at: 0.08, text: STORY.void }],
      cues: [{ at: 0.04, id: 'rise', play: (audio, quiet) => audio.rise(quiet) }],
    },
    {
      id: 'void',
      dur: d(4.4),
      scene: 'void',
      bed: 'tight',
      from: VIEW.still,
      to: VIEW.still,
      kicker: 'A void',
      lines: [{ at: 0.08, text: STORY.doNot }],
      aside: { at: 0.34, text: STORY.smile },
      cues: [{ at: 0.08, id: 'hit', play: (audio, quiet) => audio.voidHit(quiet ? 0.4 : 1) }],
    },
    {
      id: 'arrive',
      dur: d(7.2),
      scene: 'arrive',
      bed: 'intro',
      from: VIEW.arriveA,
      to: VIEW.arriveB,
      kicker: STORY.same,
      lines: [{ at: 0.1, text: STORY.wake }],
      cues: [],
    },
    {
      id: 'house',
      dur: d(6.6),
      scene: 'house',
      bed: 'tight',
      from: VIEW.houseA,
      to: VIEW.houseB,
      kicker: '',
      lines: [{ at: 0.12, text: STORY.house }],
      omen: { start: 0.58, end: 0.8 },
      cues: [],
    },
    {
      id: 'garage',
      dur: d(6.5),
      scene: 'garage',
      bed: 'garage',
      from: VIEW.still,
      to: VIEW.still,
      kicker: '',
      lines: [{ at: 0.16, text: STORY.garage }],
      cues: [
        { at: 0.12, id: 'bulb', play: (audio) => audio.bulbTick() },
        { at: 0.46, id: 'engine', play: (audio) => audio.engineDie() },
        { at: 0.72, id: 'bulb2', play: (audio) => audio.bulbTick() },
      ],
    },
    {
      id: 'handoff',
      dur: d(1.35),
      scene: 'handoff',
      bed: 'garage',
      from: VIEW.still,
      to: VIEW.still,
      kicker: '',
      lines: [],
      cues: [],
    },
  ].map(fitReading);
}

export function totalDuration(beats) {
  return beats.reduce((sum, beat) => sum + beat.dur, 0);
}

export function handoffStart(beats) {
  return totalDuration(beats) - beats[beats.length - 1].dur;
}

export function activeBeat(beats, time) {
  let cursor = 0;
  for (const beat of beats) {
    if (time < cursor + beat.dur) {
      return { beat, local: Math.max(0, time - cursor), start: cursor };
    }
    cursor += beat.dur;
  }
  const last = beats[beats.length - 1];
  return { beat: last, local: last.dur, start: cursor - last.dur };
}

export function linesAt(beat, local) {
  const lines = [];
  for (const line of beat.lines || []) {
    if (local + 1e-4 >= line.at * beat.dur) lines.push(line.text);
  }
  return lines;
}

export function storyScript() {
  const beats = sequenceBeats(false);
  const lines = [];
  for (const beat of beats) {
    if (beat.kicker) lines.push(beat.kicker);
    for (const line of beat.lines) lines.push(line.text);
    if (beat.aside) lines.push(beat.aside.text);
  }
  return lines;
}

function hash(x, y = 0) {
  let n = Math.imul(x | 0, 374761393) + Math.imul(y | 0, 668265263);
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return (n ^ (n >>> 16)) >>> 0;
}

function lerp(a, b, t) {
  return a + (b - a) * t;
}

function smoothstep(edge0, edge1, value) {
  const span = edge1 - edge0 || 1;
  const t = Math.max(0, Math.min(1, (value - edge0) / span));
  return t * t * (3 - 2 * t);
}

export function bootOpacity(fadeU) {
  return 1 - smoothstep(0.45, 1, fadeU);
}

/** Window and porch brightness. Steps at 1.6 Hz so a full blink stays under three a second. */
export function flickerLevel(time, seed = 0) {
  const rate = 1.6;
  const phase = time * rate + seed;
  const step = Math.floor(phase);
  const frac = phase - step;
  const on = hash(step, Math.floor(seed * 17) + 3) % 5 > 1;
  const edge = frac < 0.12 ? frac / 0.12 : frac > 0.88 ? (1 - frac) / 0.12 : 1;
  return on ? 0.22 + 0.78 * edge : 0.04 * edge;
}

function mixHex(a, b, t) {
  const channel = (hex, index) => parseInt(hex.slice(1 + index * 2, 3 + index * 2), 16);
  const mix = (index) => Math.round(channel(a, index) + (channel(b, index) - channel(a, index)) * t);
  return `rgb(${mix(0)}, ${mix(1)}, ${mix(2)})`;
}

function mixView(from, to, t) {
  const view = {};
  for (const key of Object.keys(from)) view[key] = lerp(from[key], to[key], t);
  return view;
}

function buildTrees() {
  const canvas = document.createElement('canvas');
  canvas.width = 720;
  canvas.height = 200;
  const ctx = canvas.getContext('2d');
  for (let i = 0; i < 22; i += 1) {
    const x = (hash(i, 4) % 680) + 10;
    const tall = 60 + (hash(i, 5) % 110);
    const spread = 10 + (hash(i, 6) % 16);
    ctx.fillStyle = i % 4 === 0 ? '#050607' : '#0b0e12';
    ctx.beginPath();
    ctx.moveTo(x, 200);
    ctx.lineTo(x + spread, 200);
    ctx.lineTo(x + spread * 0.5, 200 - tall);
    ctx.closePath();
    ctx.fill();
  }
  return canvas;
}

function buildStars() {
  return Array.from({ length: 90 }, (_, i) => ({
    x: (hash(i, 21) % 1000) / 1000,
    y: (hash(i, 22) % 1000) / 1000,
    a: 0.2 + (hash(i, 23) % 60) / 100,
    s: hash(i, 24) % 3 === 0 ? 2 : 1,
  }));
}

function buildDust() {
  return Array.from({ length: 80 }, (_, i) => ({
    x: (hash(i, 31) % 1000) / 1000,
    y: (hash(i, 32) % 1000) / 1000,
    z: 0.25 + (hash(i, 33) % 75) / 100,
    s: 1 + (hash(i, 34) % 2),
  }));
}

export function createIntro({
  canvas,
  boot,
  title,
  plate,
  kicker,
  body,
  controls,
  skipBtn,
  holdBtn,
  audio,
  getSettings,
  onPrepare,
  onLive,
}) {
  const ctx = canvas.getContext('2d', { alpha: false });
  const trees = buildTrees();
  const stars = buildStars();
  const dust = buildDust();
  const faces = {};

  let phase = 'off';
  let running = false;
  let raf = 0;
  let last = 0;
  let titleTime = 0;
  let held = false;
  let beats = [];
  let clock = 0;
  let fired = new Set();
  let lastBeatId = '';
  let prepared = false;
  let skipping = false;
  let finished = true;
  let plateSig = '';
  let plateLines = [];
  let runStill = false;

  function visuals() {
    const settings = getSettings() || {};
    const os = globalThis.matchMedia?.('(prefers-reduced-motion: reduce)')?.matches ?? false;
    const quiet = !!settings.reduceFlash || os;
    return { still: quiet, quiet };
  }

  function face(id, options) {
    if (!ready(id)) return null;
    const key = `${id}|${options.crop}|${options.size}|${options.corrupt || 0}|${options.dark || 0}`;
    if (!faces[key]) faces[key] = portrait(id, options);
    return faces[key];
  }

  function resize() {
    const cssW = canvas.clientWidth || globalThis.innerWidth || 1280;
    const cssH = canvas.clientHeight || globalThis.innerHeight || 720;
    if (cssW < 2 || cssH < 2) return { w: 0, h: 0 };
    const dpr = Math.min(1.75, globalThis.devicePixelRatio || 1, 1920 / cssW);
    const w = Math.max(2, Math.floor(cssW * dpr));
    const h = Math.max(2, Math.floor(cssH * dpr));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
    return { w, h };
  }

  function clearPlate() {
    kicker.textContent = '';
    body.replaceChildren();
    plateSig = '';
    plateLines = [];
  }

  function appendLine(text, hidden) {
    const node = document.createElement('p');
    if (hidden) node.className = 'visually-hidden';
    node.textContent = text;
    body.appendChild(node);
  }

  function syncPlate(beat, local, quiet) {
    if (!beat || beat.id === 'handoff') {
      if (plateSig) clearPlate();
      return;
    }
    const lines = linesAt(beat, local);
    const aside = beat.aside && !quiet && local >= beat.aside.at * beat.dur ? beat.aside.text : '';
    const sig = `${beat.id}|${beat.kicker}|${lines.join('\n')}|${aside}`;
    if (sig === plateSig) return;
    const grown = plateLines.length
      && plateLines.length < lines.length
      && plateLines.every((line, index) => line === lines[index])
      && kicker.textContent === (beat.kicker || '');
    if (!grown) {
      kicker.textContent = beat.kicker || '';
      body.replaceChildren();
      for (const line of lines) appendLine(line, false);
      if (aside) appendLine(aside, true);
    } else {
      for (const line of lines.slice(plateLines.length)) appendLine(line, false);
      if (aside && !body.querySelector('.visually-hidden')) appendLine(aside, true);
    }
    plateLines = lines.slice();
    plateSig = sig;
  }

  function setHoldLabel() {
    holdBtn.textContent = held ? 'Let it move' : 'Hold still';
  }

  function startLoop() {
    if (running) return;
    running = true;
    last = 0;
    raf = requestAnimationFrame(frame);
  }

  function stopLoop() {
    running = false;
    if (raf) cancelAnimationFrame(raf);
    raf = 0;
  }

  function finish() {
    if (finished) return;
    finished = true;
    phase = 'off';
    stopLoop();
    boot.style.opacity = '0';
    boot.hidden = true;
    boot.dataset.phase = 'off';
    boot.dataset.scene = '';
    controls.hidden = true;
    clearPlate();
    audio.endCinematic(0.2);
    onLive();
  }

  function enterHandoff() {
    if (prepared) return;
    prepared = true;
    audio.fadeCinematic(skipping || runStill ? 0.32 : 0.8);
    onPrepare();
  }

  function skip() {
    if (phase !== 'sequence' || finished) return;
    if (skipping) {
      finish();
      return;
    }
    skipping = true;
    skipBtn.disabled = true;
    const start = handoffStart(beats);
    if (clock < start) clock = start;
    enterHandoff();
    audio.fadeCinematic(runStill ? 0.2 : 0.32);
  }

  function fireCues(beat, local) {
    for (const cue of beat.cues || []) {
      const key = `${beat.id}:${cue.id}`;
      if (fired.has(key)) continue;
      if (local < cue.at * beat.dur) continue;
      fired.add(key);
      cue.play(audio, runStill);
    }
  }

  function drawWorld(w, h, view, time, still, omen) {
    const grade = view.grade;
    const horizonY = h * view.horizon;
    const sky = ctx.createLinearGradient(0, 0, 0, Math.max(1, horizonY));
    sky.addColorStop(0, mixHex('#07060c', '#0c1212', grade));
    sky.addColorStop(0.72, mixHex('#16121c', '#1c2824', grade));
    sky.addColorStop(1, mixHex('#221826', '#2a3832', grade));
    ctx.fillStyle = sky;
    ctx.fillRect(0, 0, w, horizonY);
    ctx.fillStyle = mixHex('#0c0b09', '#101614', grade);
    ctx.fillRect(0, horizonY, w, h - horizonY);

    ctx.fillStyle = '#d5d8de';
    for (const star of stars) {
      if (star.y > view.horizon * 0.92) continue;
      const twinkle = still ? star.a : star.a * (0.65 + 0.35 * (0.5 + 0.5 * Math.sin(time * 0.7 + star.x * 12)));
      ctx.globalAlpha = twinkle * (1 - grade * 0.65);
      ctx.fillRect(star.x * w, star.y * h, star.s, star.s);
    }
    ctx.globalAlpha = 1;

    const moonX = w * 0.8;
    const moonY = h * 0.14;
    const moonR = Math.max(16, w * 0.026);
    ctx.fillStyle = `rgba(214, 216, 222, ${0.7 - grade * 0.45})`;
    ctx.beginPath();
    ctx.arc(moonX, moonY, moonR, 0, Math.PI * 2);
    ctx.arc(moonX + moonR * 0.46, moonY - moonR * 0.08, moonR * 0.78, 0, Math.PI * 2);
    ctx.fill('evenodd');

    const shift = still ? 0 : (view.travel * 90) % trees.width;
    const treeH = Math.min(h * 0.28, 220);
    const treeY = horizonY - treeH * 0.92;
    ctx.drawImage(trees, -shift, treeY, trees.width, treeH);
    ctx.drawImage(trees, trees.width - shift, treeY, trees.width, treeH);

    drawRoad(w, h, view);
    drawPoles(w, h, view);
    const hx = w * view.houseX;
    const scale = view.houseS * Math.min(w, h) / 780;
    drawHouse(hx, horizonY, scale, time, still, view.lights, omen);
    drawFog(w, h, horizonY, time, still, grade);
    drawDust(w, h, time, still, view);
    if (view.headlights > 0.02) drawHeadlights(w, h, view);
    if (view.cabin > 0.02) drawCabin(w, h, view.cabin);
    if (view.mirror > 0.02) drawMirror(w, h, view.mirror);
  }

  function drawRoad(w, h, view) {
    const horizonY = h * view.horizon;
    const vanishX = w * view.vanish;
    ctx.fillStyle = mixHex('#141210', '#121816', view.grade);
    ctx.beginPath();
    ctx.moveTo(vanishX - w * 0.02, horizonY);
    ctx.lineTo(vanishX + w * 0.02, horizonY);
    ctx.lineTo(w * 0.9, h);
    ctx.lineTo(w * 0.1, h);
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = mixHex('#3a342c', '#3c4e48', view.grade);
    ctx.lineWidth = Math.max(1, w * 0.002);
    ctx.beginPath();
    ctx.moveTo(vanishX - w * 0.018, horizonY);
    ctx.lineTo(w * 0.1, h);
    ctx.moveTo(vanishX + w * 0.018, horizonY);
    ctx.lineTo(w * 0.9, h);
    ctx.stroke();
    for (let i = 0; i < 14; i += 1) {
      const z = ((i / 14) + view.travel * 0.45) % 1;
      const y = horizonY + (z ** 1.55) * (h - horizonY);
      const width = lerp(2, Math.max(10, w * 0.012), z);
      const markH = lerp(4, 22, z);
      ctx.globalAlpha = 0.25 + z * 0.55;
      ctx.fillStyle = mixHex('#cbb892', '#8aa396', view.grade);
      ctx.fillRect(vanishX - width / 2, y, width, markH);
    }
    ctx.globalAlpha = 1;
  }

  function drawPoles(w, h, view) {
    const horizonY = h * view.horizon;
    const vanishX = w * view.vanish;
    ctx.fillStyle = '#100e0c';
    for (let i = 0; i < 7; i += 1) {
      const z = ((i / 7) + view.travel * 0.28) % 1;
      const y = horizonY + (z ** 1.4) * (h - horizonY);
      const spread = lerp(w * 0.04, w * 0.42, z);
      const poleH = lerp(10, h * 0.16, z);
      const x = vanishX + spread;
      ctx.fillRect(x, y - poleH, Math.max(2, z * 4), poleH);
      ctx.fillRect(x - 8 * z, y - poleH, 16 * z + 4, Math.max(2, z * 3));
    }
  }

  function drawHouse(x, base, scale, time, still, lights, omen) {
    ctx.save();
    ctx.translate(x, base);
    ctx.scale(scale, scale);
    ctx.fillStyle = '#100e0c';
    ctx.fillRect(-200, -250, 400, 250);
    ctx.beginPath();
    ctx.moveTo(-230, -250);
    ctx.lineTo(0, -390);
    ctx.lineTo(230, -250);
    ctx.closePath();
    ctx.fill();
    ctx.fillRect(120, -360, 34, 90);
    ctx.fillRect(-80, -20, 160, 20);
    ctx.fillRect(-60, -70, 120, 50);
    const windows = [
      [-150, -200], [-60, -200], [40, -200], [120, -200],
      [-150, -110], [120, -110],
    ];
    windows.forEach((spot, index) => {
      const level = still ? 0.35 : flickerLevel(time, index * 1.3 + 0.4);
      const alpha = lights * (0.04 + level * 0.62);
      ctx.fillStyle = `rgba(228, 192, 122, ${alpha})`;
      ctx.fillRect(spot[0], spot[1], 42, 54);
      ctx.fillStyle = '#100e0c';
      ctx.fillRect(spot[0] + 19, spot[1], 4, 54);
      ctx.fillRect(spot[0], spot[1] + 24, 42, 4);
      if (omen && index === 2) {
        const grin = face('grin', { crop: 'face', size: 96, corrupt: 0.12, dark: 0.15 });
        if (grin) {
          ctx.save();
          ctx.globalAlpha = 0.9;
          ctx.drawImage(grin, spot[0], spot[1], 42, 54);
          ctx.restore();
        }
      }
    });
    const bulb = (still ? 0.7 : flickerLevel(time, 2.4)) * Math.max(lights, 0.35);
    const glow = ctx.createRadialGradient(0, -30, 4, 0, -10, 150);
    glow.addColorStop(0, `rgba(240, 215, 160, ${0.45 * bulb})`);
    glow.addColorStop(1, 'rgba(240, 215, 160, 0)');
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(0, -20, 150, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = `rgba(240, 216, 170, ${0.4 + bulb * 0.6})`;
    ctx.beginPath();
    ctx.arc(0, -48, 5, 0, Math.PI * 2);
    ctx.fill();
    if (!still && lights > 0.4) {
      for (let i = 0; i < 5; i += 1) {
        const ang = time * 0.8 + i * 1.3;
        const rx = Math.cos(ang) * (28 + i * 6);
        const ry = Math.sin(ang * 1.3) * 12;
        ctx.fillStyle = 'rgba(214, 206, 186, 0.8)';
        ctx.fillRect(rx - 1, -48 + ry, 2, 2);
      }
    }
    ctx.restore();
  }

  function drawFog(w, h, horizonY, time, still, grade) {
    ctx.fillStyle = `rgba(186, 196, 198, ${0.035 + grade * 0.03})`;
    for (let i = 0; i < 4; i += 1) {
      const drift = still ? 0 : (time * 8 + i * 30) % (h * 0.2);
      const y = horizonY - 20 + i * 16 + drift;
      ctx.fillRect(0, y, w, 10 + i * 8);
    }
  }

  function drawDust(w, h, time, still, view) {
    const count = still ? 22 : dust.length;
    const shift = still ? 0 : time * 0.012;
    for (let i = 0; i < count; i += 1) {
      const speck = dust[i];
      const x = ((speck.x + shift * speck.z) % 1) * w;
      const y = ((speck.y + shift * 0.25) % 1) * h;
      ctx.globalAlpha = 0.08 + speck.z * (still ? 0.12 : 0.28);
      ctx.fillStyle = view.grade > 0.5 ? '#9fb2aa' : '#d2cbb8';
      ctx.fillRect(x, y, speck.s, speck.s);
    }
    ctx.globalAlpha = 1;
  }

  function drawHeadlights(w, h, view) {
    const horizonY = h * view.horizon;
    const vanishX = w * view.vanish;
    const grad = ctx.createLinearGradient(w * 0.5, h, vanishX, horizonY);
    grad.addColorStop(0, `rgba(228, 192, 122, ${0.2 * view.headlights})`);
    grad.addColorStop(1, 'rgba(228, 192, 122, 0)');
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.moveTo(w * 0.38, h);
    ctx.lineTo(vanishX - w * 0.03, horizonY);
    ctx.lineTo(vanishX + w * 0.03, horizonY);
    ctx.lineTo(w * 0.62, h);
    ctx.closePath();
    ctx.fill();
  }

  function drawCabin(w, h, amount) {
    ctx.save();
    ctx.globalAlpha = Math.min(1, amount);
    ctx.fillStyle = '#050403';
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(w * 0.18, h * 0.1);
    ctx.lineTo(w * 0.12, h);
    ctx.lineTo(0, h);
    ctx.closePath();
    ctx.fill();
    ctx.beginPath();
    ctx.moveTo(w, 0);
    ctx.lineTo(w * 0.82, h * 0.1);
    ctx.lineTo(w * 0.88, h);
    ctx.lineTo(w, h);
    ctx.closePath();
    ctx.fill();
    ctx.fillRect(0, 0, w, h * 0.08);
    ctx.fillRect(0, h * 0.8, w, h * 0.2);
    ctx.fillStyle = '#1a1612';
    ctx.fillRect(0, h * 0.78, w, h * 0.035);
    ctx.restore();
  }

  function drawMirror(w, h, amount) {
    const chris = face('chris', { crop: 'face', size: 280, clean: true, dark: 0.2 });
    const mw = Math.min(w * 0.24, 280);
    const mh = mw * 0.72;
    const x = (w - mw) / 2;
    const y = h * 0.11;
    ctx.save();
    ctx.globalAlpha = amount;
    ctx.fillStyle = '#0c0a09';
    ctx.fillRect(x - 10, y - 10, mw + 20, mh + 20);
    ctx.fillStyle = '#171410';
    ctx.fillRect(x, y, mw, mh);
    if (chris) {
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(chris, x, y, mw, mh);
      ctx.imageSmoothingEnabled = true;
    }
    ctx.fillStyle = 'rgba(12, 18, 16, 0.28)';
    ctx.fillRect(x, y, mw, mh);
    ctx.strokeStyle = '#5c4b38';
    ctx.lineWidth = 2;
    ctx.strokeRect(x - 10, y - 10, mw + 20, mh + 20);
    ctx.restore();
  }

  function drawPhoto(w, h, local, quiet, revealGrin) {
    ctx.fillStyle = '#070605';
    ctx.fillRect(0, 0, w, h);
    const evan = face('evan', { crop: 'bust', size: 520, clean: true });
    const grin = revealGrin ? face('grin', { crop: 'face', size: 520, corrupt: 0.08 }) : null;
    const bw = Math.min(w * 0.62, h * 0.78);
    const bh = bw * 0.78;
    const x = (w - bw) / 2;
    const y = h * 0.1;
    ctx.fillStyle = '#d7cbb6';
    ctx.fillRect(x - 22, y - 36, bw + 44, bh + 78);
    ctx.fillStyle = '#1a1714';
    ctx.fillRect(x, y, bw, bh);
    const grinMix = revealGrin ? smoothstep(0.12, 0.46, local) : 0;
    const tearing = revealGrin && grinMix > 0.12 && grinMix < 0.62;
    ctx.imageSmoothingEnabled = false;
    if (evan) {
      const ox = tearing ? 8 : 0;
      ctx.drawImage(evan, x + ox, y, bw, bh);
      if (tearing) {
        ctx.save();
        ctx.globalCompositeOperation = 'screen';
        ctx.globalAlpha = 0.4;
        ctx.drawImage(evan, x - 10, y + 2, bw, bh);
        ctx.restore();
      }
    }
    if (grin && grinMix > 0) {
      ctx.save();
      ctx.globalAlpha = grinMix;
      ctx.drawImage(grin, x, y, bw, bh);
      ctx.restore();
    }
    ctx.imageSmoothingEnabled = true;
    ctx.fillStyle = '#2a241c';
    for (let i = 0; i < 8; i += 1) {
      ctx.fillRect(x - 16, y - 8 + i * ((bh + 40) / 8), 8, 10);
      ctx.fillRect(x + bw + 8, y - 8 + i * ((bh + 40) / 8), 8, 10);
    }
    if (!quiet) {
      const flash = local < 0.16 ? (1 - local / 0.16) * 0.4 : 0;
      if (flash > 0) {
        ctx.fillStyle = `rgba(239, 230, 212, ${flash})`;
        ctx.fillRect(0, 0, w, h);
      }
    }
  }

  function drawFine(w, h) {
    ctx.fillStyle = '#070605';
    ctx.fillRect(0, 0, w, h);
    const shot = face('teammate', { crop: 'bust', size: 420, clean: true, dark: 0.12 });
    if (!shot) return;
    const bw = Math.min(w * 0.34, 420);
    const bh = bw * (shot.height / shot.width);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(shot, w * 0.08, h * 0.18, bw, bh);
    ctx.imageSmoothingEnabled = true;
    ctx.fillStyle = 'rgba(0, 0, 0, 0.28)';
    ctx.fillRect(w * 0.08, h * 0.18, bw, bh);
  }

  function drawGarage(w, h, time, still, cover) {
    ctx.fillStyle = '#0c0b0a';
    ctx.fillRect(0, 0, w, h);
    const wall = ctx.createLinearGradient(0, 0, 0, h);
    wall.addColorStop(0, '#161310');
    wall.addColorStop(0.62, '#100e0c');
    wall.addColorStop(1, '#080706');
    ctx.fillStyle = wall;
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = '#1a1714';
    ctx.beginPath();
    ctx.moveTo(w * 0.08, h);
    ctx.lineTo(w * 0.22, h * 0.62);
    ctx.lineTo(w * 0.78, h * 0.62);
    ctx.lineTo(w * 0.92, h);
    ctx.closePath();
    ctx.fill();

    const carX = w * 0.5;
    const carY = h * 0.58;
    const carW = Math.min(w * 0.46, 560);
    const carH = carW * 0.42;
    ctx.fillStyle = '#2a3038';
    ctx.fillRect(carX - carW / 2, carY, carW, carH * 0.7);
    ctx.fillStyle = '#14181c';
    ctx.fillRect(carX - carW * 0.28, carY + carH * 0.08, carW * 0.22, carH * 0.28);
    ctx.fillRect(carX + carW * 0.06, carY + carH * 0.08, carW * 0.22, carH * 0.28);
    ctx.fillStyle = '#0e1014';
    ctx.fillRect(carX - carW * 0.42, carY + carH * 0.48, carW * 0.16, carH * 0.2);
    ctx.fillRect(carX + carW * 0.26, carY + carH * 0.48, carW * 0.16, carH * 0.2);
    const mom = face('mom', { crop: 'face', size: 80, clean: true, dark: 0.45 });
    if (mom && !still) {
      ctx.save();
      ctx.globalAlpha = 0.45;
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(mom, carX + carW * 0.08, carY + carH * 0.1, carW * 0.16, carH * 0.22);
      ctx.imageSmoothingEnabled = true;
      ctx.restore();
    }

    const pulse = still ? 0.62 : 0.38 + 0.62 * (0.5 + 0.5 * Math.sin(time * 2.1));
    const bx = w * 0.5;
    const by = h * 0.16;
    ctx.strokeStyle = '#3a342c';
    ctx.beginPath();
    ctx.moveTo(bx, 0);
    ctx.lineTo(bx, by);
    ctx.stroke();
    const glow = ctx.createRadialGradient(bx, by, 4, bx, by, w * 0.28);
    glow.addColorStop(0, `rgba(228, 192, 122, ${0.28 * pulse})`);
    glow.addColorStop(1, 'rgba(228, 192, 122, 0)');
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(bx, by, w * 0.28, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = `rgba(240, 220, 170, ${0.35 + pulse * 0.65})`;
    ctx.beginPath();
    ctx.arc(bx, by, 7, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#070605';
    ctx.fillRect(w * 0.08, h * 0.28, w * 0.07, h * 0.5);
    ctx.fillStyle = 'rgba(142, 180, 196, 0.05)';
    ctx.fillRect(w * 0.08, h * 0.28, w * 0.015, h * 0.5);

    if (cover > 0) {
      ctx.fillStyle = `rgba(0, 0, 0, ${cover})`;
      ctx.fillRect(0, 0, w, h);
    }
  }

  function drawLetterbox(w, h) {
    const bar = Math.round(h * 0.07);
    ctx.fillStyle = '#050403';
    ctx.fillRect(0, 0, w, bar);
    ctx.fillRect(0, h - bar, w, bar);
  }

  function drawVignette(w, h) {
    const g = ctx.createRadialGradient(w / 2, h / 2, w * 0.2, w / 2, h / 2, w * 0.72);
    g.addColorStop(0, 'rgba(0, 0, 0, 0)');
    g.addColorStop(1, 'rgba(0, 0, 0, 0.72)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
  }

  function drawGrain(w, h, time) {
    const seed = Math.floor(time * 12);
    ctx.fillStyle = 'rgba(255, 255, 255, 0.045)';
    for (let i = 0; i < 120; i += 1) {
      const x = hash(i + seed, 3) % w;
      const y = hash(i, seed + 8) % h;
      ctx.fillRect(x, y, 2, 1);
    }
  }

  function render(time, beat, local, fadeU) {
    const { w, h } = resize();
    if (!ctx || w < 2) return;
    const { still, quiet } = visuals();
    ctx.imageSmoothingEnabled = true;
    ctx.fillStyle = '#070605';
    ctx.fillRect(0, 0, w, h);
    const scene = beat ? beat.scene : 'world';
    if (!beat) {
      const drift = still ? 0 : 0.5 + 0.5 * Math.sin(time * 0.15);
      const view = mixView(VIEW.titleA, VIEW.titleB, drift);
      const omen = !quiet && (time % 14) > 13.55;
      drawWorld(w, h, view, time, still, omen);
    } else if (scene === 'photo' || scene === 'void') {
      const u = beat.dur ? local / beat.dur : 0;
      drawPhoto(w, h, u, quiet || scene === 'photo', scene === 'void' && !quiet);
    } else if (scene === 'fine') {
      drawFine(w, h);
    } else if (scene === 'garage' || scene === 'handoff') {
      const cover = scene === 'handoff' ? smoothstep(0.15, 0.85, fadeU) : 0;
      drawGarage(w, h, time, quiet, cover);
    } else {
      const u = still ? 0 : smoothstep(0, 1, beat.dur ? local / beat.dur : 0);
      const view = mixView(beat.from, beat.to, u);
      let omen = false;
      if (!quiet && beat.omen) {
        const at = local / beat.dur;
        omen = at >= beat.omen.start && at <= beat.omen.end;
      }
      drawWorld(w, h, view, titleTime + clock, still, omen);
    }
    if (phase !== 'title') drawLetterbox(w, h);
    drawVignette(w, h);
    if (!still) drawGrain(w, h, time);
  }

  function frame(now) {
    if (!running) return;
    raf = requestAnimationFrame(frame);
    if (!last) last = now;
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    const { still } = visuals();

    if (phase === 'title') {
      if (!held && !still) titleTime += dt;
      boot.dataset.phase = 'title';
      boot.dataset.scene = 'title';
      boot.style.opacity = '1';
      render(titleTime, null, 0, 0);
      if (held) stopLoop();
      return;
    }

    if (phase !== 'sequence') {
      stopLoop();
      return;
    }

    clock += dt;
    const total = totalDuration(beats);
    const { beat, local, start } = activeBeat(beats, Math.min(clock, total));
    if (beat.id !== lastBeatId) {
      lastBeatId = beat.id;
      boot.dataset.scene = beat.scene;
      if (beat.id !== 'handoff') audio.cinematic(beat.bed || 'intro');
    }
    fireCues(beat, local);
    const fadeU = beat.id === 'handoff'
      ? (skipping ? Math.min(1, (clock - start) / 0.42) : (beat.dur ? local / beat.dur : 1))
      : 0;
    if (beat.id === 'handoff') enterHandoff();
    boot.style.opacity = String(beat.id === 'handoff' ? bootOpacity(fadeU) : 1);
    syncPlate(beat, local, runStill);
    render(titleTime + clock, beat, local, fadeU);

    if (skipping && clock >= start + 0.42) finish();
    else if (clock >= total) finish();
  }

  function onVisibility() {
    if (document.hidden) {
      if (audio.hasCinematic?.()) audio.fadeCinematic(0.2);
      stopLoop();
      return;
    }
    if (phase === 'off') return;
    if (audio.hasCinematic?.() && lastBeatId !== 'handoff') {
      const beat = beats.find((item) => item.id === lastBeatId);
      audio.cinematic(phase === 'title' ? 'title' : (beat?.bed || 'intro'));
    }
    if (!(phase === 'title' && held)) startLoop();
  }

  holdBtn.addEventListener('click', () => {
    held = !held;
    setHoldLabel();
    if (phase === 'title' && !held) startLoop();
    if (phase === 'title' && held) {
      render(titleTime, null, 0, 0);
      stopLoop();
    }
  });
  skipBtn.addEventListener('click', skip);
  document.addEventListener('visibilitychange', onVisibility);
  window.addEventListener('resize', () => {
    if (phase === 'off') return;
    if (phase === 'sequence' && beats.length) {
      const current = activeBeat(beats, clock);
      render(titleTime + clock, current.beat, current.local, 0);
      return;
    }
    render(titleTime, null, 0, 0);
  });

  return {
    startTitle({ warm = false } = {}) {
      finished = false;
      phase = 'title';
      held = false;
      titleTime = 0;
      clock = 0;
      beats = [];
      prepared = false;
      skipping = false;
      lastBeatId = '';
      setHoldLabel();
      clearPlate();
      boot.hidden = false;
      boot.style.opacity = '1';
      boot.dataset.phase = 'title';
      boot.dataset.scene = 'title';
      title.hidden = false;
      controls.hidden = true;
      skipBtn.disabled = false;
      if (warm) {
        audio.unlock();
        audio.cinematic('title');
      }
      startLoop();
    },
    startSequence() {
      finished = false;
      phase = 'sequence';
      runStill = visuals().still;
      beats = sequenceBeats(runStill);
      clock = 0;
      fired = new Set();
      lastBeatId = '';
      prepared = false;
      skipping = false;
      held = false;
      clearPlate();
      title.hidden = true;
      controls.hidden = false;
      skipBtn.disabled = false;
      boot.hidden = false;
      boot.style.opacity = '1';
      boot.dataset.phase = 'sequence';
      audio.unlock();
      audio.cinematic('intro');
      startLoop();
      skipBtn.focus();
    },
    armAudio() {
      if (phase === 'off') return;
      audio.unlock();
      if (audio.hasCinematic?.()) return;
      audio.cinematic(phase === 'title' ? 'title' : 'intro');
    },
    skip,
    shutdown() {
      finished = true;
      phase = 'off';
      stopLoop();
      boot.style.opacity = '0';
      boot.hidden = true;
      boot.dataset.phase = 'off';
      boot.dataset.scene = '';
      controls.hidden = true;
      clearPlate();
      audio.endCinematic(0.15);
    },
  };
}
