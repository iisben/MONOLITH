#!/usr/bin/env python3
"""MONOLITH II Stage 2 offline asset pipeline (Layer A texture engine).

Reads the curated source files from /Volumes/Audio Blob/Samples, then for each:
resample -> 48 kHz, DC removal (mean + 10 Hz HP), silence trim, click-free
equal-power crossfade loop (sustained assets) or fade tails (one-shots),
normalise to -12 dBFS RMS capped at -1 dBFS peak, 24-bit FLAC.
Emits assets/v2_samples/*.flac, Source/V2/TextureAssetTable.h and ASSET_PROVENANCE.md.
"""
import hashlib, json, os, sys
import numpy as np, soundfile as sf
from scipy import signal

SRC = "/Volumes/Audio Blob/Samples/"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "assets", "v2_samples")
SR = 48000

# key, display, source, mode, (start_s, len_s), pitched, root (MIDI note at original rate), mono, vendor/pack
# Roots: pitch class from the pack's key label; octave from the lowest strong partial
# (choir_drone A#3, choir_ooh C4, bowed B3). angel_ambience's fundamental is F5 but its root is pinned to F4
# so the halo rings an octave above the played note.
ASSETS = [
    ("breath_exhale", "Breath Exhale", "Splice/sounds/packs/Sounds of KSHMR Vol. 4/KSHMR_vocals/KSHMR_vocal_tools/KSHMR_vocal_breath_exhale.wav",
     "oneshot", (0.0, 4.5), False, 60, False, "Splice - Sounds of KSHMR Vol. 4"),
    ("breath_inhale", "Breath Inhale", "Splice/sounds/packs/Body Noises/HumanBreatheIn_S08HU.67.wav",
     "oneshot", (0.0, 2.2), False, 60, True, "Splice - Body Noises"),
    ("wind_whisper", "Whisper Wind", "Splice/sounds/packs/Murmur - Ambient Vocal Loops/ModeAudio_-_Murmur/loops/fx_texture_loops/MA_MUR_120_FX_Vocal_Texture_Loop_RushOfWind.wav",
     "loop", (2.0, 6.0), False, 60, False, "Splice - ModeAudio Murmur"),
    ("air_noise", "Cathedral Air", "Splice/sounds/packs/Murmur - Ambient Vocal Loops/ModeAudio_-_Murmur/loops/fx_texture_loops/MA_MUR_120_FX_Noise_Texture_Loop_Windbreaker.wav",
     "loop", (1.0, 6.0), False, 60, False, "Splice - ModeAudio Murmur"),
    ("choir_drone", "Breathing Choir Drone", "Splice/sounds/packs/Murmur - Ambient Vocal Loops/ModeAudio_-_Murmur/loops/drone_loops/MA_MUR_120_Vocal_Drone_Loop_LikeBreathing_A#.wav",
     "loop", (1.0, 6.0), True, 58, False, "Splice - ModeAudio Murmur"),
    ("angel_ambience", "Guardian Halo", "Splice/sounds/packs/Murmur - Ambient Vocal Loops/ModeAudio_-_Murmur/loops/ambience_loops/MA_MUR_100_Vocal_Ambience_Loop_GuardianAngel_F.wav",
     "loop", (4.0, 6.0), True, 65, False, "Splice - ModeAudio Murmur"),
    ("choir_ooh_swell", "Gospel Ooh Swell", "Splice/Samples/packs/Silk & Soul with the Splice Gospel Choir/One_Shots/Chords/SO_SS_vocal_chord_ooh_seventh_filtered_wet_Cm.wav",
     "oneshot", (0.0, 6.0), True, 60, False, "Splice - Silk & Soul (Splice Gospel Choir)"),
    ("bowed_ethereal", "Bowed Ethereal", "Splice/Samples/packs/Midnight Motifs - Intimate Strings/Midnight_Motifs_-_Intimate_Strings/Loops/Quartet/Swells/Major_Swells/MNT_CS_70_strings_quartet_swells_ethereal_B.wav",
     "oneshot", (0.0, 7.7), True, 59, False, "Splice - Midnight Motifs: Intimate Strings"),
]

def db(x): return 20*np.log10(max(x, 1e-12))

def resample(x, sr):
    if sr == SR: return x
    from math import gcd
    g = gcd(SR, sr)
    return signal.resample_poly(x, SR//g, sr//g, axis=0)

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

def process(a):
    key, disp, rel, mode, (st, ln), pitched, root_hint, mono, pack = a
    path = SRC + rel
    raw, sr0 = sf.read(path, always_2d=True)
    info = sf.info(path)
    x = resample(raw.astype(np.float64), sr0)
    if mono: x = x.mean(1, keepdims=True)
    x = x - x.mean(0)
    x = signal.sosfiltfilt(signal.butter(2, 10, "hp", fs=SR, output="sos"), x, axis=0)
    s0 = int(st*SR)
    if mode == "loop":
        L, X = int(ln*SR), int(0.5*SR)
        seg = x[s0:s0+L+X]
        assert len(seg) == L+X, key
        t = np.linspace(0, np.pi/2, X)[:, None]
        out = seg[:L].copy()
        out[:X] = seg[:X]*np.sin(t) + seg[L:L+X]*np.cos(t)  # equal-power: tail continues into head
    else:
        seg = x[s0:s0+int(ln*SR)]
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
    root = root_hint  # pinned: pack key label + octave of lowest strong partial (halo deliberately sits +12)
    os.makedirs(OUT, exist_ok=True)
    dst = os.path.join(OUT, key + ".flac")
    sf.write(dst, out.astype(np.float32), SR, subtype="PCM_24", format="FLAC")
    chk = sf.read(dst, always_2d=True)[0]
    d = np.abs(np.diff(chk, axis=0)).max(1)
    seam = float(np.abs(chk[0]-chk[-1]).max()/np.percentile(d, 99.9)) if mode == "loop" else 0.0  # seam step vs 99.9th pct step
    return dict(key=key, display=disp, source=rel, pack=pack, mode=mode, pitched=pitched, root=int(root),
                src_fmt=f"{info.format} {info.subtype} {info.samplerate} Hz {info.channels}ch", src_dur=info.duration,
                src_sha1=hashlib.sha1(open(path, "rb").read()).hexdigest()[:12],
                dur=len(chk)/SR, ch=chk.shape[1], rms=db(np.sqrt((chk**2).mean())), rms_act=db(rms_act*g),
                peak=db(np.abs(chk).max()), dc=db(abs(chk.mean())), seam=seam, bytes=os.path.getsize(dst))

def note_name(n): return ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"][n%12]+str(n//12-1)

def main():
    rows = [process(a) for a in ASSETS]
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
          "Generated by `Tools/v2_process_assets.py` (Stage 2). All assets: 48 kHz, 24-bit FLAC, DC-removed (10 Hz HP), silence-trimmed,",
          "normalised to -12 dBFS active RMS capped at -1 dBFS peak; loops use a 0.5 s equal-power crossfade seam.", "",
          f"**Total embedded payload: {total/1e6:.2f} MB** (budget 15 MB). Layer B (convolution IRs): **skipped** — no impulse responses exist on the drive.", "",
          "## ⚠ Licensing — COMMERCIAL CLEARANCE REQUIRED BEFORE SALE", "",
          "Every source below comes from **Splice** packs. Splice's standard licence covers using samples *inside your own music*; it",
          "generally **prohibits redistributing samples in a sample library, virtual instrument or other competing product**. Embedding these",
          "files in a commercially sold plug-in very likely needs a separate written licence from Splice and/or the pack label, or replacement",
          "with original or properly licensed recordings. Treat every row as **NOT CLEARED** until you sign it off.", "",
          "| # | Asset | Source path (under `/Volumes/Audio Blob/Samples/`) | Pack / label | Source format | Src dur | Out dur | Ch | Mode | Root | Active RMS | Peak | Size | Clearance |",
          "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for i, r in enumerate(rows, 1):
        md.append(f'| {i} | `{r["key"]}.flac` ({r["display"]}) | `{r["source"]}` (sha1 {r["src_sha1"]}) | {r["pack"]} | {r["src_fmt"]} | {r["src_dur"]:.2f} s | {r["dur"]:.2f} s | {r["ch"]} | {r["mode"]} | '
                  f'{note_name(r["root"]) if r["pitched"] else "n/a"} | {r["rms_act"]:.1f} dBFS | {r["peak"]:.1f} dBFS | {r["bytes"]/1e3:.0f} kB | ☐ NOT CLEARED |')
    md += ["", "## Clearance checklist (user sign-off)", "",
           "- [ ] Confirm the licence terms of each Splice pack above permit embedding in a sold instrument, or obtain written permission.",
           "- [ ] Or replace any uncleared asset: drop a new file into the pipeline table, re-run `python3 Tools/v2_process_assets.py`, rebuild.",
           "- [ ] Confirm no recognisable performer likeness / lyric content (all selected assets are wordless breath, air, ooh or bowed tones).",
           "- [ ] Record licence reference numbers / invoices here before NAMM 2027 sale.", ""]
    open(os.path.join(REPO, "ASSET_PROVENANCE.md"), "w").write("\n".join(md))
    os.makedirs(os.path.join(REPO, "build", "v2_qa"), exist_ok=True)
    json.dump(rows, open(os.path.join(REPO, "build", "v2_qa", "asset_manifest.json"), "w"), indent=1)
    for r in rows:
        print(f'{r["key"]:16s} {r["mode"]:7s} {r["dur"]:4.2f}s {r["ch"]}ch root {r["root"]:3d} actRMS {r["rms_act"]:6.1f} pk {r["peak"]:5.1f} dc {r["dc"]:6.1f} seam {r["seam"]:.3f} {r["bytes"]/1e3:6.0f}kB')
    print(f"TOTAL {total/1e6:.2f} MB")

if __name__ == "__main__":
    main()
