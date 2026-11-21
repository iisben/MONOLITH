# MONOLITH

**MONOLITH** is a zero-sample, real-time **Physical-Acoustic Choral Swarm Synthesizer** built in C++20 with JUCE 8 (`VST3`, `AU`, and `Standalone`). It synthesizes up to 24 concurrent virtual vocalists across 12 polyphonic voices—combining band-limited Liljencrants-Fant (LF) glottal excitation, register-accurate 5-formant vocal tract modeling, per-voice oversampled analog tape entropy, and a modulated architectural FDN reverb with Spectral Freeze.

---

## Core Architecture
- **Multi-Singer Choral Swarm (`Source/DSP/SwarmVoice.*`, `SwarmEngine.*`):** Up to 8 virtual singers per MIDI voice (dynamically budgeted up to 24 active singers) with deterministic vocal-tract length dispersion, delayed vibrato (4.5–6.2 Hz), 1/f pink pitch wander, onset timing stagger, and velocity-sensitive consonant articulation (`H`, `M/N`, `Sh`).
- **2D Phoneme & Timbre Vector Field (`Source/DSP/PhonemeField.*`):** Modified-Shepard Gaussian RBF interpolation across 7 acoustic vowel states (`AH`, `OH`, `OO`, `EH`, `EE`, `HUM`, `WHISPER`) and 4 vocal registers (`Basso Profundo`, `Tenor`, `Alto`, `Soprano`, plus `SATB Auto`), with a toggleable `OrganicOrbitEngine` (free-running or host BPM synced).
- **Sub-Abyss & Spectral Crown (`Source/DSP/HarmonicLayers.*`):** Phase-coherent chest-resonant sub-octave layer and detuned stereo upper-octave harmonic sheen.
- **Per-Voice Entropy & Witch-House Engine (`Source/DSP/EntropyStage.*`):** Constant-latency 4x (standard) / 8x (`Drive 8x OS`) polyphase IIR oversampled asymmetric tape/tube drive with first-order antiderivative anti-aliasing (ADAA), applied **per voice prior to stereo bus summing** (worst audible-band alias below -100 dBc), followed by cubic-interpolated tape wow/flutter, oxide damping, and formant-detached `Screw Pitch` / `Screw Shadow`.
- **Brutalist Architectural Space + Spectral Freeze (`Source/DSP/BrutalistReverb.*`):** 4-stage allpass diffuser + 8×8 Hadamard modulated FDN + dual-grain Hann-windowed shimmer across *Cathedral*, *Concrete Silo*, *Obsidian Abyss*, and *Ghost Void*, with click-free Spectral Freeze and RMS energy stabilization.
- **Mastering-Grade Output Stage (`Source/DSP/MasterBus.h`):** 18 Hz subsonic filter, soft-knee 1.6:1 glue compressor, and a 1.5 ms lookahead brickwall limiter with 4x true-peak detection and a 12 ms gain hold (-1 dBTP ceiling, no waveshaping, THD below -78 dB at +6 dB overload) so full-velocity 12-voice chords never clip, intersample-overshoot or distort.
- **38 Calibrated Factory Presets (`Source/Presets/FactoryPresets.cpp`):** Organised by purpose (*Epic & Trailer*, *Chamber & Sacred*, *Solo & Legato*, *Low & Ancient*, *Pads & Atmospheres*, *Tension & Horror*, *Dystopian & Lo-Fi*, *Witch House & Darkwave*), each with a one-line brief shown under the browser, plus A/B compare. Sessions recall presets by name, so reordering never changes what a saved project loads. All presets are loudness-matched (sustained triad ~ -17 dBFS RMS) and stress-tested with full-velocity 8-note chords.

- **Interface:** Cast-concrete slabs, incised inscriptions and gilt/verdigris inlays; the vowel field is drawn as an architectural plan with formant "organ pipes", the four macros are rising stone slabs, and the window resizes from the bottom-right corner with no grip graphic.

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
Verifies all 41 APVTS parameters, 38 factory presets (loudness-matched, zero clipping), a pro stress pass (full-velocity 8-note chords, soprano extremes and worst-case knob settings checked for overload, reverb runaway and non-harmonic squeal), sample-rate stability (`44.1 / 48 / 96 / 192 kHz` at `4x` and `8x` oversampling), Spectral Freeze sustain, Screw Pitch octave tracking, Legato, CC1 Expression, CC11 expression fader, a spectral quality lab (glottal oscillator spurs, drive aliasing, sub layer purity, master-bus THD and true peak; run it alone with `MonolithDspAudit lab`), and all 4 Master Macros (`COLOSSUS`, `SPECTER`, `ROT`, `ABYSS`).
