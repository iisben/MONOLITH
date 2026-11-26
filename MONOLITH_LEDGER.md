# MONOLITH Flagship DSP Ledger

## Status
| Phase | Status | Notes |
|---|---|---|
| 1 — Setup & Audit | PASS | Release `-O3` build clean (exit 0, 6 JUCE-only Metal deprecation warnings); legacy `MonolithDspAudit` 6/6 PASS, 10.91% RT (12v/24s, 64-spl @48k) |
| 2 — QA Harness | PASS | `MonolithQA` (Release -O3, headless, 9 s run); baseline 9 PASS / 7 PENDING |

**Next: Phase 3.1**

## Toolchain & Architecture
- CMake 3.22+ / Ninja, C++20, JUCE 8.0.15 (FetchContent), AppleClang arm64 macOS. Targets: `MONOLITH` (AU/VST3/Standalone), `MonolithDspAudit` (console, DSP+presets only).
- Build: `cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release` → `build/release/`. Validators: `auval` present, `pluginval` absent.
- Flow: `PluginProcessor` → `SwarmEngine` (12 `SwarmVoice`, ≤24 singers, 64-spl ctrl sub-blocks) → per singer: `LfGlottalTables` (8 tension × 10 mip, 1024-pt) + pink breath → 5× parallel `ZdfSvf` BP (+F1 −F2 +F3 −F4 +F5) → `HarmonicLayers` → per-voice `EntropyStage` (JUCE 2x/4x polyphase-IIR OS + `fastTanh` drive, wow/flutter, oxide LPF) → bus → `BrutalistReverb` (4 AP + 8×8 Hadamard FDN + grain shimmer + freeze) → master gain → `softSafetyLimit`.
- 41 APVTS params (`State/ParameterIDs.h`, all `ParameterID{id,1}`), 30 factory presets (`Presets/FactoryPresets.cpp`). Telemetry = relaxed atomics (lock-free).

## Defect Table
| # | Area | Defect | Loc | Phase |
|---|---|---|---|---|
| D1 | Glottal | 1 mip/octave, linear interp on 1024-pt table w/ 256 harmonics (≈4 spl/cycle) → interpolation images ≫ −80 dBc at high mips; only 1–2 harmonics above C6 | DspMath.h:312-482 | 3.1 |
| D2 | Glottal | Static singleton built lazily (~40M trig ops); prewarmed in `SwarmEngine::prepare` but `instance()` still on audio path | DspMath.h:319, SwarmEngine.cpp:80 | 3.1 |
| D3 | Aspiration | Pink noise → static BP × parabolic gate (floor 0.35+0.65·gate) — weakly coupled, not airflow-derived; no DC blocker on source | SwarmVoice.cpp:512-515, DspMath.h:370 | 3.1 |
| D4 | Precision | ALL SVF / DC / FDN / delay states are 32-bit `float` | DspMath.h:80-303 | 3.2/3.4 |
| D5 | Formants | Parallel unit-peak BP (Q≤20) ×4.2 gain; ±polarity sum still cancels between close F1/F2 (/u/,/o/); no vowel RMS normalization; fc clamp 0.44·fs, no Nyquist-matched prototype | SwarmVoice.cpp:544-551, DspMath.h:174-196 | 3.2 |
| D6 | Formants | Freq/BW smoothing linear (not log) @ ctrlStep; cache epsilon 1e-3 Hz → setParameters `tan()` nearly every update | SwarmVoice.cpp:390-415 | 3.2 |
| D7 | Block invariance | `i % ctrlStep` restarts every render call; SwarmEngine chunks restart per host block/MIDI split → output depends on host block size | SwarmVoice.cpp:369, EntropyStage.cpp:271, BrutalistReverb.cpp:235, SwarmEngine.cpp:510 | 3.3 |
| D8 | Swarm | Identical glottal shape/tract per singer (only ±7% formant jitter), fixed pairwise pans, stagger ≤85 ms; no VTL scaling; vibrato 4.5–6.2 Hz linear onset; `noteElapsedSec` float accumulation | SwarmVoice.cpp:36-55, 451-565 | 3.3 |
| D9 | Stereo/sub | Crown L/R detuned ±0.26% (decorrelates); voice pans equal-power to full spread incl. LF → sub-150 Hz correlation not guaranteed | HarmonicLayers.cpp:79-100, SwarmVoice.cpp:292-302 | 3.3 |
| D10 | Steal/click | Retrigger declick = exponential decay of last sample (C0 only, slope step); `entropy.reset()` on steal clears OS/delay state abruptly; fast release 12 ms exp | SwarmVoice.cpp:139-151, 316 | 3.3 |
| D11 | Drive | Naive `fastTanh` (no ADAA) in JUCE IIR OS; OS up/down always runs even at drive=0 → latency + CPU ×12 voices; IIR latency non-integer, `lround`'d | EntropyStage.cpp:119-170, 71-78 | 3.4 |
| D12 | PDC | Latency from `voices[0]` + async update on 2x/4x toggle; never 0 when drive bypassed | SwarmEngine.cpp:130, PluginProcessor.cpp:22-27,106-116 | 3.4 |
| D13 | Tape | `tapeMix` blends dry with ≥4-spl delayed copy at low tape amounts → comb notch (fs/8) | EntropyStage.cpp:230-247 | 3.4 |
| D14 | FDN | float states, `fastTanh` in feedback (non-unitary, metallic at high decay), float DC blockers per line, linear-interp allpasses, delays not mutually prime, feedback clamp 0.999 | BrutalistReverb.cpp:222-328 | 3.4 |
| D15 | Denormals | `ScopedNoDenormals` only at `SwarmEngine::processBlock` entry (OK on arm64 via JUCE); tails rely on `sanitize()` not FTZ for float states | SwarmEngine.cpp:387 | 3.4 |
| D16 | CPU | Per sample per singer: `exp2` (midiToFreq), `sin` vibrato, 2× pink, 5–8 SVF, trilinear 4-row LUT; per voice OS ×12; reverb 8× `sin` LFO + `fastTanh` | SwarmVoice.cpp:344-594 | 3.5 |
| D17 | RT safety | No locks/allocs found on audio thread (grep clean); `juce::SmoothedValue` fine; `parameterChanged` → `triggerAsyncUpdate` (message-thread latency update) | — | OK |

## Symbol Index (`file : symbol (lines)`)
- DSP/DspMath.h : `fastTanh` (25-31), `softSafetyLimit` (34-46), `midiToFreq` (48-51), `DCBlocker` (80-108), `PinkNoiseGen` (112-150), `ZdfSvf` (158-233), `CubicDelayLine` (237-303), `LfGlottalTables` (312-482; `sample` 326-367, `glottalOpenGate` 370-380, ctor 390-481)
- DSP/SwarmVoice.h : `VoiceRenderParams` (21-52), `SingerState` (96-120), members (122-176)
- DSP/SwarmVoice.cpp : `prepare` (26-61), `reset` (63-132), `startNote` (134-190), `glideToNote` (192-215), `stopNote`/`forceFastRelease` (217-240), `renderBlock` (242-647: targets 256-341, ctrl update 369-431, singer loop 452-566, layers 571-589, entropy+declick 606-629, deactivate 636-646)
- DSP/PhonemeField.cpp : formant tables (32-74), register scales (83-103), `resolveRegister` (140-149), `evaluate` (151-232), `OrganicOrbitEngine::advance` (249-293)
- DSP/HarmonicLayers.cpp : `updateFilters` (30-40), `processSample` (42-106)
- DSP/EntropyStage.cpp : `prepare` (8-43), `getLatencySamples` (71-78), `processBlock` (80-291: drive lambda 119-170, OS xfade 180-204, tape 206-257, oxide 259-290)
- DSP/BrutalistReverb.h/.cpp : `AllpassStage` (h 51-70), `GrainShifter` (h 81-127), `hadamard8` (51-80), `prepare` (83-110), `processBlock` (139-331: FDN loop 267-317)
- DSP/SwarmEngine.cpp : `prepare` (27-102), `getLatencySamples` (130-134), `allocateVoiceIndex` (150-190), `triggerNoteOn/Off` (192-303), `handleMidiMessage` (305-380), `processBlock` (382-732: `renderSpan` 505-677, MIDI dispatch 680-693)
- PluginProcessor.cpp : `prepareToPlay` (22-27), `processBlock` (42-58), `handleAsyncUpdate` (113-116)
- Tests/DspAuditTest.cpp : legacy audit (`main` 836) — keep; Phase 2 adds new QA target

## Phase 2 — QA Harness & Baseline
Run: `build/release/MonolithQA_artefacts/Release/MonolithQA [--phase 3.x | --active ids | --only ids] [--write-baseline]`. Thresholds locked in `Tests/QA/MonolithQA.cpp` `namespace lock`; preset baseline `Tests/QA/preset_baseline.csv`; CPU baseline 7.78% RT. Report → `build/monolith_qa/qa_report.txt`. Exit 1 iff an active sub-gate fails. `--phase` sets are cumulative (3.1: 1,6,7dc,8 · 3.2: +2,3morph,7dry · 3.3: +3,5,7steal · 3.4: +4,7fdn · 3.5/4: all).
Adapters to re-point (never thresholds): `renderGlottal` (G1), `TractProbe` (G2, mirrors SwarmVoice 544-551), `naiveShaper` (G4), `measuredStageLatency` (G6c). G4a expects `DSP/ThroatDrive.h` → `ThroatShaper::{f,F1,F2,adaa1}` (static, double).
| Gate | Baseline | Status |
|---|---|---|
| 1 glottal | alias C5 −91.9 / C6 −93.2 / C7 −93.2 dBc, DC −144.6, C4 h61 −51 dBc | PASS |
| 2 formant | Ferr 1.48%, track 0.05%/0.01 dB, notch −23.5 dB, vowel RMS dev 7.86 dB (C3 /a/ −22.1 vs /i/ −9.7) | PENDING |
| 3morph | d2 1.08× ref, +−0.2 dB energy, finite | PASS |
| 3toggle | d2 0.40× bound (static 0.40×) | PASS |
| 4adaa / 4alias | not implemented / −29.6 dBc, −58 dB vs 1× naive, DC −114 | PENDING |
| 5 swarm | comb −7.7/−8.5 dB, sub corr 0.97/0.99, mono drop 0.75 dB | PASS |
| 6block | residual −57.3 dBFS (D7) | PENDING |
| 6rate | exact (0) | PASS |
| 6pdc | 2x rep 4 / meas 5; 4x 6/6; drive=0 still 4 (≠0) | PENDING |
| 7dc / 7steal | DC −84.3 dBFS / steal d2 6.23× ref | PENDING |
| 7dry | tail 0, voices 0, idle 0.48% RT (marginal vs 0.5%) | PASS |
| 7fdn | max rise −1.5 dB, end −184 dBFS, CPU 0.94× | PASS |
| 8snap / 8cpu | 30 presets captured / 7.78% RT (830 µs) | PASS |
Phase 3.1 blockers (gate 6 + 7dc active): fix D7 block-size dependence, integer 2x PDC (off by 1) + 0 when drive bypassed, held-chord DC. Idle CPU has ~4% headroom.
