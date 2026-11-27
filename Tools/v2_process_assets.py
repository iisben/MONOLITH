#!/usr/bin/env python3
"""MONOLITH II offline asset pipeline (Layer A texture engine).

Stage 7: every texture is synthesised from scratch by the functions below (seeded, deterministic). No recorded
or third-party audio is read. Each signal then gets DC removal, trim, a click-free equal-power crossfade loop
(sustained assets) or fade tails (one-shots), -12 dBFS active RMS capped at -1 dBFS peak, 24-bit FLAC.
Emits assets/v2_samples/*.flac, Source/V2/TextureAssetTable.h and ASSET_PROVENANCE.md; then run
Tools/v2_flagship_presets.py to restore the preset dependency table.
"""
import json, os
import numpy as np, soundfile as sf
from scipy import signal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "assets", "v2_samples")
SR = 48000

# ── synthesis primitives ────────────────────────────────────────────────────
def times(sec): return np.arange(int(sec*SR))/SR
def midi_hz(n): return 440.0*2**((n-69)/12)

def slow(rng, n, rate_hz):
    """Smooth random curve, roughly unit variance, changing about rate_hz times a second."""
    step = max(1, int(SR/rate_hz))
    pts = rng.standard_normal(n//step+4)
    from scipy.interpolate import CubicSpline
    return CubicSpline(np.arange(len(pts))*step, pts)(np.arange(n))

def peaks(f, bands):
    """Sum of resonance curves: bands = [(centre_hz, bandwidth_hz, gain)]."""
    return sum(g/(1.0+((f-c)/(0.5*bw))**2) for c, bw, g in bands)

def shaped_noise(rng, sec, shape, hop=512, nfft=2048):
    """White noise filtered frame by frame with magnitude shape(freqs, time_s)."""
    n = int(sec*SR)
    x = rng.standard_normal(n+2*nfft)
    f, t, Z = signal.stft(x, SR, nperseg=nfft, noverlap=nfft-hop)
    H = np.stack([shape(f, max(0.0, tk-nfft/SR)) for tk in t], axis=1)
    y = signal.istft(Z*H, SR, nperseg=nfft, noverlap=nfft-hop)[1]
    return y[nfft:nfft+n]

def hall(rng, x, rt60, wet, damp_hz=6000):
    """Stereo reverb: convolution with decaying, damped, decorrelated noise (generated here)."""
    t = times(rt60)
    ir = rng.standard_normal((len(t), 2))*(10**(-3*t/rt60))[:, None]
    ir = signal.sosfilt(signal.butter(1, damp_hz, "lp", fs=SR, output="sos"), ir, axis=0)
    ir[:int(0.012*SR)] = 0
    ir /= np.sqrt((ir**2).sum(0))
    w = np.stack([signal.fftconvolve(x[:, c], ir[:, c])[:len(x)] for c in range(2)], 1)
    return (1-wet)*x + wet*w

def pan(mono, p):
    a = (p+1)*np.pi/4
    return np.stack([mono*np.cos(a), mono*np.sin(a)], 1)

def voice(rng, sec, hz, formants, vib_cents=18, tilt=1.2, nmax=60):
    """One additive 'singer' or 'player': harmonics weighted by a fixed resonance curve, with vibrato and drift."""
    t = times(sec); n = len(t)
    cents = (vib_cents*(1+0.3*slow(rng, n, 1.0))*np.sin(2*np.pi*rng.uniform(4.6, 5.9)*t+rng.uniform(0, 6.28))
             + 6*slow(rng, n, 2.5) + 3*slow(rng, n, 11) + rng.normal(0, 5))
    f0 = hz*2**(cents/1200)
    ph = 2*np.pi*np.cumsum(f0)/SR
    y = np.zeros(n)
    for k in range(1, nmax+1):
        if k*hz > 11000: break
        amp = peaks(k*f0, formants)/k**tilt
        y += amp*np.sin(k*ph+rng.uniform(0, 6.28))
    return y*(1+0.08*slow(rng, n, 3.0))

def ensemble(rng, sec, notes, per_note, formants, **kw):
    out = 0
    for hz, level in notes:
        for i in range(per_note):
            out = out + level*pan(voice(rng, sec, hz, formants, **kw), rng.uniform(-0.55, 0.55))
    return out/np.abs(out).max()

AH = [(700, 130, 1.0), (1150, 150, 0.6), (2800, 260, 0.22), (3500, 300, 0.12)]
OO = [(330, 90, 1.0), (800, 130, 0.35), (2400, 260, 0.07), (3300, 300, 0.04)]
BODY = [(290, 90, 0.7), (460, 130, 0.9), (1000, 400, 0.45), (2600, 900, 0.5), (4200, 1500, 0.2)]

# ── the eight textures (lengths include the 0.5 s loop crossfade tail where looped) ──
def breath(rng, sec, bands, env_pts):
    et, ev = zip(*env_pts)
    out = []
    for c in range(2):
        r = np.random.default_rng(rng.integers(1 << 30))
        shape = lambda f, tm: (peaks(f, [(b*(1-0.12*min(tm/sec, 1)), w, g) for b, w, g in bands]) + 0.015) \
                              * (f/(f+250.0))**2 / (1+(f/9000.0)**4)
        out.append(shaped_noise(r, sec, shape))
    x = np.stack(out, 1)
    x = 0.75*x.mean(1, keepdims=True) + 0.25*x
    env = np.interp(times(sec), et, ev)*(1+0.12*slow(rng, len(x), 14))
    return x*env[:, None]

def s_breath_exhale(rng):
    return breath(rng, 4.5, [(520, 300, 1.0), (1500, 600, 0.55), (2900, 1200, 0.35)],
                  [(0, 0), (0.08, 0.5), (0.35, 1.0), (1.4, 0.75), (3.0, 0.3), (4.5, 0.0)])

def s_breath_inhale(rng):
    return breath(rng, 2.1, [(900, 500, 0.7), (2100, 800, 1.0), (4200, 2000, 0.45)],
                  [(0, 0), (0.25, 0.2), (1.2, 0.75), (1.75, 1.0), (1.95, 0.25), (2.1, 0.0)])

def s_wind_whisper(rng):
    sec = 6.5; chans = []
    for c in range(2):
        r = np.random.default_rng(rng.integers(1 << 30))
        curves = [slow(r, int(sec*SR)+SR, 0.7) for _ in range(3)]
        def shape(f, tm, curves=curves):
            i = int(min(tm, sec)*SR)
            m = [2**(0.28*cv[i]) for cv in curves]
            return (peaks(f, [(450*m[0], 110, 1.0), (1100*m[1], 220, 0.65), (2700*m[2], 500, 0.4)]) + 0.02) * (f/(f+180.0))**2
        chans.append(shaped_noise(r, sec, shape)*(1+0.25*slow(r, int(sec*SR), 0.8)))
    return hall(rng, np.stack(chans, 1), 1.6, 0.25)

def s_air_noise(rng):
    sec = 6.5
    shape = lambda f, tm: (f/(f+120.0))**2 / np.sqrt(1+f/350.0) / (1+(f/11000.0)**2) * (1+0.5*peaks(f, [(240, 140, 1.0)]))
    x = np.stack([shaped_noise(np.random.default_rng(rng.integers(1 << 30)), sec, shape) for _ in range(2)], 1)
    x = 0.8*x + 0.2*x.mean(1, keepdims=True)
    x *= (1+0.12*slow(rng, len(x), 0.6))[:, None]
    return hall(rng, x, 3.0, 0.5, 5000)

def s_choir_drone(rng):
    sec = 8.5; f = midi_hz(58)  # A#3
    x = ensemble(rng, sec, [(f, 1.0), (f*1.5, 0.35), (f*2, 0.25)], 7, AH)
    t = times(sec)
    x *= (0.72+0.28*np.sin(2*np.pi*t/3.0-np.pi/2))[:, None]  # 3 s "breathing": two cycles per 6 s loop
    return hall(rng, x, 2.4, 0.38)[2*SR:]

def s_angel_ambience(rng):
    sec = 8.5; f = midi_hz(77)  # F5: rings an octave above the pinned F4 root
    x = ensemble(rng, sec, [(f, 1.0), (f*1.5, 0.25), (f/2, 0.2)], 6, OO[:1]+[(1000, 250, 0.5), (3200, 500, 0.15)], vib_cents=12)
    return hall(rng, x, 3.5, 0.6, 8000)[2*SR:]

def s_choir_ooh_swell(rng):
    sec = 6.0  # C minor seventh, root C4
    x = ensemble(rng, sec, [(midi_hz(n), g) for n, g in ((60, 1.0), (63, 0.75), (67, 0.7), (70, 0.55))], 5, OO)
    env = np.interp(times(sec), [0, 0.5, 2.8, 4.2, 6.0], [0, 0.35, 1.0, 0.85, 0.0])
    return hall(rng, x*env[:, None], 2.2, 0.35, 4500)

def s_bowed_ethereal(rng):
    sec = 7.7  # B major quartet swell, root B3
    notes = [(midi_hz(59), 1.0), (midi_hz(66), 0.7), (midi_hz(71), 0.55), (midi_hz(75), 0.45)]
    x = ensemble(rng, sec, notes, 3, BODY, vib_cents=11, tilt=0.9)
    bow = shaped_noise(rng, sec, lambda f, tm: peaks(f, [(2800, 2500, 1.0)]) * (f/(f+600.0))**2)
    x += 0.02*bow[:, None]/np.abs(bow).max()
    env = np.interp(times(sec), [0, 0.6, 2.0, 4.2, 6.0, 7.7], [0, 0.18, 0.6, 1.0, 0.7, 0.0])
    return hall(rng, x*env[:, None], 2.6, 0.35, 7000)

# key, display, synth, mode, length_s, pitched, root (MIDI note at original rate), mono
ASSETS = [
    ("breath_exhale",   "Breath Exhale",         s_breath_exhale,   "oneshot", 4.5, False, 60, False),
    ("breath_inhale",   "Breath Inhale",         s_breath_inhale,   "oneshot", 2.1, False, 60, True),
    ("wind_whisper",    "Whisper Wind",          s_wind_whisper,    "loop",    6.0, False, 60, False),
    ("air_noise",       "Cathedral Air",         s_air_noise,       "loop",    6.0, False, 60, False),
    ("choir_drone",     "Breathing Choir Drone", s_choir_drone,     "loop",    6.0, True,  58, False),
    ("angel_ambience",  "Guardian Halo",         s_angel_ambience,  "loop",    6.0, True,  65, False),
    ("choir_ooh_swell", "Gospel Ooh Swell",      s_choir_ooh_swell, "oneshot", 6.0, True,  60, False),
    ("bowed_ethereal",  "Bowed Ethereal",        s_bowed_ethereal,  "oneshot", 7.7, True,  59, False),
]

def db(x): return 20*np.log10(max(x, 1e-12))

def limit(x, ceil, max_gr_db=9.0):
    """Lookahead peak limiter with a continuous gain curve (no per-sample gain steps):
    centred min-filter (+/-5 ms) -> 80 ms release -> centred 5 ms moving average.
    Every sample in the averaging window has the peak inside its min window, so gain <= need at the peak."""
    from scipy.ndimage import minimum_filter1d, uniform_filter1d
    la, rel = int(0.005*SR), np.exp(-1/(0.08*SR))
    need = np.minimum(1.0, ceil/np.maximum(np.abs(x).max(1), 1e-12))
    need = np.maximum(need, 10**(-max_gr_db/20))
    m = minimum_filter1d(need, 2*la+1)
    g = np.empty_like(m); cur = 1.0
    for i, v in enumerate(m):
        cur = v if v < cur else v + (cur - v)*rel
        g[i] = cur
    g = uniform_filter1d(g, la+1)
    return x*g[:, None]

def process(index, a):
    key, disp, synth, mode, ln, pitched, root, mono = a
    x = synth(np.random.default_rng(20270700+index)).astype(np.float64)
    x = x/np.abs(x).max()
    if mono: x = x.mean(1, keepdims=True)
    x = x - x.mean(0)
    x = signal.sosfiltfilt(signal.butter(2, 10, "hp", fs=SR, output="sos"), x, axis=0)
    if mode == "loop":
        L, X = int(ln*SR), int(0.5*SR)
        seg = x[:L+X]
        assert len(seg) == L+X, key
        t = np.linspace(0, np.pi/2, X)[:, None]
        out = seg[:L].copy()
        out[:X] = seg[:X]*np.sin(t) + seg[L:L+X]*np.cos(t)  # equal-power: tail continues into head
    else:
        seg = x[:int(ln*SR)]
        env = np.sqrt(signal.lfilter([1/480], [1, -(1-1/480)], (seg**2).mean(1)))
        thr = env.max()*10**(-60/20)
        idx = np.where(env > thr)[0]
        a0 = max(0, idx[0]-int(0.005*SR)); a1 = min(len(seg), idx[-1]+int(0.05*SR))
        out = seg[a0:a1].copy()
        fi, fo = int(0.002*SR), int(0.05*SR)
        out[:fi] *= np.linspace(0, 1, fi)[:, None]
        out[-fo:] *= (0.5+0.5*np.cos(np.linspace(0, np.pi, fo)))[:, None]
    fr = (out**2).mean(1)
    w = int(0.05*SR)
    frm = np.array([fr[i:i+w].mean() for i in range(0, len(fr)-w, w)])
    act = frm[frm > frm.max()*10**(-30/10)]
    rms_act = np.sqrt(act.mean())
    g = 10**(-12/20)/rms_act
    out = limit(out*g, 10**(-1.2/20))
    pk = np.abs(out).max()
    if pk > 10**(-1/20):  # limiter cap reached: back off the whole asset
        g *= 10**(-1/20)/pk; out *= 10**(-1/20)/pk
    os.makedirs(OUT, exist_ok=True)
    dst = os.path.join(OUT, key + ".flac")
    sf.write(dst, out.astype(np.float32), SR, subtype="PCM_24", format="FLAC")
    chk = sf.read(dst, always_2d=True)[0]
    d = np.abs(np.diff(chk, axis=0)).max(1)
    seam = float(np.abs(chk[0]-chk[-1]).max()/np.percentile(d, 99.9)) if mode == "loop" else 0.0  # seam step vs 99.9th pct step
    return dict(key=key, display=disp, mode=mode, pitched=pitched, root=int(root), synth=synth.__name__, seed=20270700+index,
                dur=len(chk)/SR, ch=chk.shape[1], rms=db(np.sqrt((chk**2).mean())), rms_act=db(rms_act*g),
                peak=db(np.abs(chk).max()), dc=db(abs(chk.mean())), seam=seam, bytes=os.path.getsize(dst))

def note_name(n): return ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"][n%12]+str(n//12-1)

def main():
    rows = [process(i, a) for i, a in enumerate(ASSETS)]
    total = sum(r["bytes"] for r in rows)
    hdr = ["#pragma once", "// GENERATED by Tools/v2_process_assets.py - do not edit by hand.", "",
           "namespace monolith::v2", "{", "struct TextureAssetInfo", "{",
           "    const char* resourceName; // juce_add_binary_data symbol name", "    const char* displayName;",
           "    bool looped;   // whole file is a click-free crossfaded loop", "    bool pitched;  // follows keyboard 1:1 (unpitched assets track at 25 %)",
           "    int rootNote;  // MIDI root at original rate", "};", "",
           f"inline constexpr int kNumTextureAssets = {len(rows)};", "", "inline constexpr TextureAssetInfo kTextureAssets[kNumTextureAssets] {"]
    for r in rows:
        hdr.append(f'    {{ "{r["key"]}_flac", "{r["display"]}", {str(r["mode"]=="loop").lower()}, {str(r["pitched"]).lower()}, {r["root"]} }},')
    hdr += ["};", "} // namespace monolith::v2", ""]
    open(os.path.join(REPO, "Source", "V2", "TextureAssetTable.h"), "w").write("\n".join(hdr))
    md = ["# MONOLITH II — Asset Provenance (V2 Layer A: Texture Engine)", "",
          "Generated by `Tools/v2_process_assets.py`. All assets: 48 kHz, 24-bit FLAC, DC-removed (10 Hz HP), silence-trimmed,",
          "normalised to -12 dBFS active RMS capped at -1 dBFS peak; loops use a 0.5 s equal-power crossfade seam.", "",
          f"**Total embedded payload: {total/1e6:.2f} MB** (budget 15 MB).", "",
          "## Origin (V2 Stage 7)", "",
          "All eight textures are **synthesised from scratch** by that script (additive voices, filtered noise and generated",
          "reverb tails, fixed random seeds). It reads no recordings, samples or impulse responses, so no third-party audio licence",
          "applies to the current files. The display names are labels only.", "",
          "The Stage 2 Splice-derived files these replace are **still present in git history** (commits from Stage 2 onward).",
          "Do not ship or publish that history, or any build made before Stage 7.", "",
          "| # | Asset | Synth function (seed) | Out dur | Ch | Mode | Root | Active RMS | Peak | Size | Origin |",
          "|---|---|---|---|---|---|---|---|---|---|---|"]
    for i, r in enumerate(rows, 1):
        md.append(f'| {i} | `{r["key"]}.flac` ({r["display"]}) | `{r["synth"]}` ({r["seed"]}) | {r["dur"]:.2f} s | {r["ch"]} | {r["mode"]} | '
                  f'{note_name(r["root"]) if r["pitched"] else "n/a"} | {r["rms_act"]:.1f} dBFS | {r["peak"]:.1f} dBFS | {r["bytes"]/1e3:.0f} kB | Original (synthesised) |')
    md += [""]
    open(os.path.join(REPO, "ASSET_PROVENANCE.md"), "w").write("\n".join(md))
    os.makedirs(os.path.join(REPO, "build", "v2_qa"), exist_ok=True)
    json.dump(rows, open(os.path.join(REPO, "build", "v2_qa", "asset_manifest.json"), "w"), indent=1)
    for r in rows:
        print(f'{r["key"]:16s} {r["mode"]:7s} {r["dur"]:4.2f}s {r["ch"]}ch root {r["root"]:3d} actRMS {r["rms_act"]:6.1f} pk {r["peak"]:5.1f} dc {r["dc"]:6.1f} seam {r["seam"]:.3f} {r["bytes"]/1e3:6.0f}kB')
    print(f"TOTAL {total/1e6:.2f} MB")

if __name__ == "__main__":
    main()
