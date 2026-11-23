// Subtitle queue. Hold time follows the line. Fades are optional so
// reduced motion can show the whole line without waiting on a CSS event.

export const FADE_IN = 0.16;
export const FADE_OUT = 0.28;

export function captionHold(text) {
  const words = String(text || '').trim().split(/\s+/).filter(Boolean);
  const count = Math.max(1, words.length);
  const seconds = 0.9 + count * 0.34;
  return Math.round(Math.min(7.2, Math.max(1.7, seconds)) * 100) / 100;
}

export function captionSpan(hold, fade) {
  return fade ? FADE_IN + hold + FADE_OUT : hold;
}

export function captionOpacity(age, hold, fade) {
  if (!fade) return age >= hold ? 0 : 1;
  if (age <= 0) return 0;
  if (age < FADE_IN) return age / FADE_IN;
  if (age < FADE_IN + hold) return 1;
  const out = (age - FADE_IN - hold) / FADE_OUT;
  return Math.max(0, 1 - out);
}

export function captionFinished(age, hold, fade) {
  return age >= captionSpan(hold, fade);
}

export class CaptionQueue {
  constructor() {
    this.current = null;
    this.pending = [];
  }

  clear() {
    this.current = null;
    this.pending = [];
  }

  push({ text, kind, priority, ambient = true }) {
    const line = String(text || '').trim();
    if (!line) return;
    if (kind === 'ambient' && !ambient) return;
    if (this.current?.text === line) return;
    if (this.pending.some((item) => item.text === line)) return;
    const entry = {
      text: line,
      kind,
      priority,
      hold: captionHold(line),
    };
    if (!this.current) {
      this.start(entry);
      return;
    }
    if (priority >= 3) this.pending = this.pending.filter((item) => item.priority >= 2);
    const index = this.pending.findIndex((item) => item.priority < priority);
    if (index < 0) this.pending.push(entry);
    else this.pending.splice(index, 0, entry);
    while (this.pending.length > 5) {
      let drop = -1;
      for (let i = this.pending.length - 1; i >= 0; i -= 1) {
        if (this.pending[i].priority <= 1) {
          drop = i;
          break;
        }
      }
      if (drop < 0) break;
      this.pending.splice(drop, 1);
    }
  }

  start(entry) {
    this.current = { ...entry, age: 0, opacity: 0 };
  }

  advance(dt, fade) {
    if (!this.current) {
      if (this.pending.length) this.start(this.pending.shift());
      if (this.current) this.current.opacity = captionOpacity(0, this.current.hold, fade);
      return this.current;
    }
    this.current.age += dt;
    const { age, hold } = this.current;
    this.current.opacity = captionOpacity(age, hold, fade);
    if (captionFinished(age, hold, fade)) {
      this.current = null;
      if (this.pending.length) this.start(this.pending.shift());
      if (this.current) this.current.opacity = captionOpacity(0, this.current.hold, fade);
    }
    return this.current;
  }
}
