# MONOLITH

**MONOLITH** is a zero-sample, real-time **Physical-Acoustic Choral Swarm Synthesizer** built in C++20 with JUCE 8 (`VST3`, `AU`, and `Standalone`). It synthesizes up to 24 concurrent virtual vocalists across 12 polyphonic voices—combining band-limited Liljencrants-Fant (LF) glottal excitation, register-accurate 5-formant vocal tract modeling, per-voice oversampled analog tape entropy, and a modulated architectural FDN reverb with Spectral Freeze.

---

## Core Architecture
- **Multi-Singer Choral Swarm (`Source/DSP/SwarmVoice.*`, `SwarmEngine.*`):** Up to 8 virtual singers per MIDI voice (dynamically budgeted up to 24 active singers) with deterministic vocal-tract length dispersion, delayed vibrato (4.5–6.2 Hz), 1/f pink pitch wander, onset timing stagger, and velocity-sensitive consonant articulation (`H`, `M/N`, `Sh`).
- **2D Phoneme & Timbre Vector Field (`Source/DSP/PhonemeField.*`):** Modified-Shepard Gaussian RBF interpolation across 7 acoustic vowel states (`AH`, `OH`, `OO`, `EH`, `EE`, `HUM`, `WHISPER`) and 4 vocal registers (`Basso Profundo`, `Tenor`, `Alto`, `Soprano`, plus `SATB Auto`), with a toggleable `OrganicOrbitEngine` (free-running or host BPM synced).
- **Sub-Abyss & Spectral Crown (`Source/DSP/HarmonicLayers.*`):** Phase-coherent chest-resonant sub-octave layer and detuned stereo upper-octave harmonic sheen.
- **Per-Voice Entropy & Witch-House Engine (`Source/DSP/EntropyStage.*`):** Constant-latency 2x/4x polyphase IIR oversampled asymmetric tape/tube drive applied **per voice prior to stereo bus summing**, followed by cubic-interpolated tape wow/flutter, oxide damping, and formant-detached `Screw Pitch` / `Screw Shadow`.
- **Brutalist Architectural Space + Spectral Freeze (`Source/DSP/BrutalistReverb.*`):** 4-stage allpass diffuser + 8×8 Hadamard modulated FDN + dual-grain Hann-windowed shimmer across *Cathedral*, *Concrete Silo*, *Obsidian Abyss*, and *Ghost Void*, with click-free Spectral Freeze and RMS energy stabilization.
- **Mastering-Grade Output Stage (`Source/DSP/MasterBus.h`):** 18 Hz subsonic filter, soft-knee 1.6:1 glue compressor, and a 1.5 ms lookahead brickwall limiter (-1 dBFS ceiling, zero overshoot, no waveshaping) so full-velocity 12-voice chords never clip or distort.
- **38 Calibrated Factory Presets (`Source/Presets/FactoryPresets.cpp`):** Across *Epic Cinematic*, *Lorn & Dystopia*, *Witch House & Darkwave*, *Organic Acoustic*, *Evolving & Horror Soundscapes*, and the trailer-scale *Cinematic Colossal* bank, plus A/B compare. All presets are loudness-matched (sustained triad ~ -17 dBFS RMS) and stress-tested with full-velocity 8-note chords.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the complete signal-flow and real-time threading specification.

---

## Building from Source (macOS / Cross-Platform CMake)

Requires CMake 3.22+, Ninja, and a C++20 compiler (AppleClang recommended on macOS). By default CMake fetches JUCE `8.0.15` automatically, or pass `-DMONOLITH_JUCE_DIR=/path/to/JUCE`.

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
ln -sf build/compile_commands.json .

cmake --build build --target MONOLITH_VST3 MONOLITH_AU MONOLITH_Standalone MonolithDspAudit -j
```

## Headless DSP & Preset Audit
```bash
./build/MonolithDspAudit_artefacts/Release/MonolithDspAudit
```
Verifies all 41 APVTS parameters, 38 factory presets (loudness-matched, zero clipping), a pro stress pass (full-velocity 8-note chords, soprano extremes and worst-case knob settings checked for overload, reverb runaway and non-harmonic squeal), sample-rate stability (`44.1 / 48 / 96 / 192 kHz` at `2x` and `4x` oversampling), Spectral Freeze sustain, Screw Pitch octave tracking, Legato, CC1 Expression, and all 4 Master Macros (`COLOSSUS`, `SPECTER`, `ROT`, `ABYSS`).
