# MONOLITH — System & Acoustic DSP Architecture

## 1. Overview
**MONOLITH** is a zero-sample, real-time physical-acoustic choral swarm synthesizer built with C++20 and JUCE 8 (`VST3`, `AU`, `Standalone`). Up to 12 polyphonic voices dynamically allocate up to **24 concurrent virtual vocalists** modeled from glottal pulse physics, 5-formant vocal tract acoustics, per-voice analog tape entropy, and a modulated architectural FDN reverb with Spectral Freeze.

---

## 2. Audio Signal Flow

```
MIDI Note / CC1 / CC64 / PitchBend / Aftertouch
        │
        ▼
  ┌─────────────────────────────────────────────────────────────────────┐
  │ Macro & Control Evaluation (41 APVTS atomics -> curved morphing)    │
  │  • COLOSSUS, SPECTER, ROT, ABYSS + OrganicOrbitEngine (2D Field)    │
  │  • Dynamic Singer Budgeting (1..8 singers/voice, <= 24 total)       │
  └─────────────────────────────────────────────────────────────────────┘
        │
        ▼  (Up to 12 SwarmVoice instances)
  ┌─────────────────────────────────────────────────────────────────────┐
  │ SwarmVoice                                                          │
  │  ├─ Pitch Engine: MIDI + Bend + Legato Glide w/ Scoop + Screw Pitch │
  │  ├─ Lung Envelope: Smooth Attack/Release + Per-Singer Stagger       │
  │  ├─ Onset Articulator: Soft Breath (H) / Hum (M/N) / Ghost (Sh)     │
  │  ├─ 1..8 Virtual Singers:                                           │
  │  │    • Micro-Detune, 1/f Pink Pitch Wander, Delayed Vibrato        │
  │  │    • Band-Limited LF Glottal Wavetable (8 tension x 10 mip-maps) │
  │  │    • Glottal-Phase-Modulated Aspirated Breath Noise              │
  │  │    • 5-Formant Parallel ZDF SVF Bank (+F1 -F2 +F3 -F4 +F5)       │
  │  │      across Basso Profundo / Tenor / Alto / Soprano + Jaw Track  │
  │  ├─ HarmonicLayers: Sub-Abyss (0.5*f0 + chest sat) + Spectral Crown │
  │  └─ Per-Voice Pre-Sum EntropyStage (Stereo):                        │
  │       • 2x/4x Polyphase IIR Oversampled Asymmetric Tube/Tape Drive  │
  │         (matched HF pre/de-emphasis, envelope-tracked bias)         │
  │       • 8 Hz DC Blocker -> Cubic Tape Wow & Flutter -> Oxide LPF    │
  └─────────────────────────────────────────────────────────────────────┘
        │
        ▼  (Pre-allocated Stereo Sum Bus)
  ┌─────────────────────────────────────────────────────────────────────┐
  │ BrutalistReverb (4 Spaces: Cathedral / Silo / Obsidian / Ghost)     │
  │  • 4-Stage Allpass Early Diffuser                                   │
  │  • 8x8 Normalized Hadamard Modulated FDN                            │
  │  • Dual-Grain Hann-Windowed Pitch Shimmer (+12st / -12st / +11.87st)│
  │  • Click-Free Spectral Freeze (40 ms crossfade + RMS Stabilizer)    │
  └─────────────────────────────────────────────────────────────────────┘
        │
        ▼
  Master Gain Trim -> Soft Safety Limiter -> NaN Sanitize -> Output
        │
        └──► TelemetrySnapshot (100% lock-free relaxed atomics @ 60 Hz UI)
```

---

## 3. Threading & Real-Time Safety Model
- **Audio Thread (`processBlock`)**:
  - **Zero heap allocations** (`new`, `malloc`, `std::vector::resize`, `push_back` are strictly confined to `prepare()`).
  - **Zero locks/mutexes** and zero UI/file/String/ValueTree operations.
  - All 41 APVTS parameters are read via cached `std::atomic<float>*` pointers (`memory_order_relaxed`).
  - Host buffers larger than `preparedMaxBlockSize` are automatically processed in safe chunks `<= preparedMaxBlockSize`.
  - `juce::ScopedNoDenormals` wraps every block; 8 Hz double-precision DC blockers follow non-linear saturation and reverb feedback; every output sample is sanitized against `NaN`/`Inf` and passed through a soft transparent safety limiter (`0.75` linear knee -> `0.985` ceiling, C1 `fastTanh` knee).
- **UI Thread (`PluginEditor` @ 60 Hz)**:
  - Polls `TelemetrySnapshot` (`std::atomic` relaxed loads) to animate the 2D Phoneme Field halo, formant energy rings, swarm motes, expression meter, voice badge, and stereo RMS/Peak meters.
  - Preset switching and A/B compare occur exclusively on the message thread via `PresetManager`.

---

## 4. Sample-Rate Independence (44.1 kHz – 192 kHz)
- TPT/ZDF integrators, phase accumulators, DC blockers, and the pink-noise poles run in `double`. `ZdfSvf` coefficients are `g = tan(π·fc/fs)`, `k = 1/Q`, clamped to `[20 Hz, 0.44·fs]`, and audio is cast back to `float` only at the filter boundary.
- `LfGlottalTables` selects the mip level satisfying `maxHarmonic * f0 <= 0.45 * sampleRate` and reads it with 4-point Catmull-Rom interpolation, so glottal pulses stay alias-free and free of bin-interpolation dulling.
- Pink-noise poles are the Paul Kellet 44.1 kHz design, warped onto the session rate in `prepare()` so breath and wander keep the same corner frequencies from 44.1 kHz to 192 kHz.
- All delay lines (`EntropyStage` wow/flutter, `BrutalistReverb` pre-delay, allpass diffusers, 8 FDN lines, and pitch-shifter grains) scale from `sampleRate` during `prepare()`. Diffuser and shimmer reads are cubic Hermite. Steady-state FDN lengths are nudged to mutually coprime integers; feedback is `10^(-3T/RT60)` from that length.
- Modulators (vibrato, tape wow/flutter, FDN) are quadrature oscillators. The flutter sideband is a real 1.07× oscillator — a wrapped phase multiplied by 1.07 used to jump once per cycle.
- Sub-octave and spectral-crown partials above `0.34·fs` fade out with a C1 smoothstep before they can fold.
- `EntropyStage` reports one latency for both oversampling modes: the shorter polyphase group delay is padded with an integer delay so a 2x↔4x crossfade is time-aligned. `AudioProcessor::setLatencySamples()` publishes that constant.

---

## 5. Gain Staging & Calibration
- Voice summing uses `1 / sqrt(activeWeightSum)` perceptual normalization.
- LF glottal tables are RMS-normalized across all 8 tension slices so Effort sweeps harmonic density smoothly without volume jumps.
- All 30 factory presets are calibrated to render a sustained triad (velocity `0.78`) within **-18 to -8 dBFS RMS** (targeting **-14 to -10 LUFS** nominal playing level) with headroom below `0 dBFS`.
