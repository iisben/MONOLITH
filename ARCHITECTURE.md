# MONOLITH — System & Acoustic DSP Architecture

## 1. Overview
**MONOLITH** is a zero-sample, real-time physical-acoustic choral swarm synthesizer built with C++20 and JUCE 8 (`VST3`, `AU`, `Standalone`). Up to 12 polyphonic voices dynamically allocate up to **24 concurrent virtual vocalists** modeled from glottal pulse physics, 5-formant vocal tract acoustics, per-voice analog tape entropy, and a modulated architectural FDN reverb with Spectral Freeze.

---

## 2. Audio Signal Flow

```
MIDI Note / CC1 / CC11 / CC64 / CC121 / PitchBend / Aftertouch
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
  │  │    • Band-Limited LF Glottal Wavetable (8 tension x 10 mips, 2048-pt Hermite) │
  │  │    • Glottal-Phase-Modulated Aspirated Breath Noise              │
  │  │    • 5-Formant Parallel ZDF SVF Bank (+F1 -F2 +F3 -F4 +F5)       │
  │  │      across Basso Profundo / Tenor / Alto / Soprano + Jaw Track  │
  │  ├─ HarmonicLayers: Sub-Abyss (0.5*f0 + chest sat) + Spectral Crown │
  │  └─ Per-Voice Pre-Sum EntropyStage (Stereo):                        │
  │       • 4x/8x Polyphase IIR Oversampled ADAA Asymmetric Tube/Tape Drive│
  │       • DC Blocker -> Cubic Delay Tape Wow & Flutter -> Oxide LPF   │
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
  Master Gain Trim -> MasterBus [18 Hz subsonic HPF -> 1.6:1 soft-knee glue comp -> 1.5 ms lookahead + 12 ms hold true-peak limiter @ -1 dBTP] -> Output
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
  - `juce::ScopedNoDenormals` wraps every block; DC blockers follow non-linear saturation and reverb feedback; every output sample is sanitized against `NaN`/`Inf` and passed through a lookahead brickwall limiter (4x polyphase true-peak detection, peak-hold with a 12 ms release hold so low fundamentals are not gain-modulated, box-filtered gain ramp, overshoot-free, no waveshaping distortion). The reverb tank keeps total loop gain < 1 at every Decay/Shimmer setting (energy-conserving shimmer), and Spectral Freeze runs a lossless unity-gain loop (integer delays, damping bypassed) instead of make-up gain, so neither can run away or build resonant squeal.
- **UI Thread (`PluginEditor` @ 60 Hz)**:
  - Polls `TelemetrySnapshot` (`std::atomic` relaxed loads) to animate the 2D Phoneme Field halo, formant energy rings, swarm motes, expression meter, voice badge, and stereo RMS/Peak meters.
  - Preset switching and A/B compare occur exclusively on the message thread via `PresetManager`.

---

## 4. Sample-Rate Independence (44.1 kHz – 192 kHz)
- All filter cutoffs are derived analytically via `tan(pi * fc / fs)` inside `ZdfSvf` and clamped strictly to `[20.0f, 0.44f * sampleRate]`.
- `LfGlottalTables` automatically selects the mip-map level satisfying `maxHarmonic * f0 <= 0.45 * sampleRate` so glottal pulses remain alias-free across the entire pitch and sample-rate range.
- All delay lines (`EntropyStage` wow/flutter, `BrutalistReverb` pre-delay, allpass diffusers, 8 FDN lines, and pitch-shifter grain buffers) scale capacity from `sampleRate` during `prepare()`.
- `EntropyStage` reports its active `4x` or `8x` polyphase IIR oversampling latency, plus the limiter lookahead and 6-sample true-peak detector delay, via `AudioProcessor::setLatencySamples()`.
- The drive waveshaper uses first-order ADAA of the Padé tanh (closed-form antiderivative, linear tails beyond |u| >= 3), so harmonics above the oversampled Nyquist are suppressed before decimation.
- Glottal tables are 2048 samples per cycle, read with 4-point Hermite interpolation after bilinear blending across tension and mip level; the inharmonic floor stays below -97 dB at every rate (about -104 dB at 44.1/48 kHz).

---

## 5. Gain Staging & Calibration
- Voice summing uses `1 / sqrt(activeWeightSum)` perceptual normalization.
- LF glottal tables are RMS-normalized across all 8 tension slices so Effort sweeps harmonic density smoothly without volume jumps.
- All 38 factory presets are loudness-matched so a sustained triad (velocity `0.78`) renders at roughly **-17 dBFS RMS**, and a full-velocity 8-note chord needs at most a few dB of limiting. The `MonolithDspAudit stress` pass enforces this.
- MIDI CC11 acts as a squared-law expression fader on the summed voice bus (per-sample 15 ms smoothing) before the reverb, so tails ring out naturally when the fader is pulled down; CC121 resets CC11, pitch bend and aftertouch.
