# MONOLITH Flagship DSP Ledger

## Status
| Phase | Status | Notes |
|---|---|---|
| 1 — Setup & Audit | PASS | Release `-O3` build clean (exit 0, 6 JUCE-only Metal deprecation warnings); legacy `MonolithDspAudit` 6/6 PASS, 10.91% RT (12v/24s, 64-spl @48k) |
| 2 — QA Harness | PASS | `MonolithQA` (Release -O3, headless, 9 s run); baseline 9 PASS / 7 PENDING |
| 3.1 — Glottal & Aspiration | PASS | `--phase 3.1` exit 0; 12 PASS / 4 PENDING |
| 3.2 — Vocal Tract / Formant | PASS (after user re-scope of gate-2 vowel RMS) | `--phase 3.2` exit 0; 13 PASS / 3 PENDING; first run rolled back (b06c863) |
| 3.3 — Swarm / Humanization / Voice Mgmt | PASS (after user re-baseline of 8snap preset RMS) | `--phase 3.3` exit 0; 14 PASS / 2 PENDING |
| 3.4 — Throat Drive ADAA / 64-bit FDN | PASS | `--phase 3.4` exit 0; 16 PASS / 0 PENDING |
| 3.5 — Swarm SoA SIMD / Lock-Free Polish | PASS | `--phase 3.5` (all gates) exit 0; 16 PASS / 0 PENDING |
| 4 — Final Build & Validation | PASS | Debug + Release build; 16/16 gates; ctest, legacy audit, `auval` PASS; `pluginval` SKIPPED |

**STATUS: ALL_PHASES_COMPLETE**

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

## Phase 3.1 — Glottal Source & Aspiration (PASS)
- `LfGlottalTables` (DspMath.h): true LF-Rd model (ε/α solved: return continuity + zero net flow), **closed-form Fourier coefficients** → analytical tilt (Ta) & OQ (Te); tension→Rd 1.6…0.3; 12 slices × 25 mips (3/oct, bright ≤0.466 fs, dark ≤0.37 fs) × 2048-pt; cubic Hermite + bilinear. Per-slice loudness calibrated to legacy pulse energy in h4–32 (preserves preset gain staging).
- Aspiration gated by band-limited glottal **airflow** table (`glottalOpenGate` = normalized U(φ)), 0.25 leakage floor (was flat 0.35 + parabola).
- `DCBlocker64` (5 Hz) on per-singer excitation; EntropyStage DC blocker → end-of-chain 64-bit 15 Hz (kills drive bias + tape-flutter LF, D13-adjacent).
- Block invariance (D7 fixed): SwarmEngine absolute 64-spl control cells (`ControlCell`), per-voice/oxide/reverb absolute ctrl counters, persistent `ctrlTarget`, per-sample voice silence counter + deactivation only at cell end, EntropyStage routing/tape/oxide gating switched only at cell start.
- PDC (D12 partial): EntropyStage measures integer impulse-peak latency at prepare (2x=5, 4x=6); bypass route (no OS, 0 latency) when effective drive (drive+Rot+Colossus) is 0; processor re-reports via async on drive/rot/colossus/HQ changes.
| Gate | Baseline → 3.1 |
|---|---|
| 1 | alias ≤−91.4 dBc (C4 h61 −51 → −31 dBc, brighter), DC −148 |
| 6block / 6pdc | −57 dBFS → exact; 2x 4/5 → 5/5, bypass 4 → 0 |
| 7dc | −84.3 → −99.8 dBFS |
| 8snap / 8cpu | max dRMS 1.24 dB (#18 Neon Mausoleum −1.24; mean 0.53), peak −1.11 / 7.90% RT |
| inactive | 2 vowel dev 7.2 dB; 7steal 5.84× (baseline 6.23×); others unchanged |
Notes: `triggerAsyncUpdate` now also fires on drive automation (audio thread; verify lock-free in 3.5). Preset RMS margin: 0.26 dB left on #18 — 3.2 vowel normalization will need justification per spec.

## Phase 3.2 — first run (FAIL — 3-strike rollback; superseded by re-run below)
The attempt-3 work was saved as a patch in `build/monolith_qa/` (removed in Phase 4; superseded by 52c6f63).
- Tract: 64-bit Cytomic TPT SVFs loaded from Vicanek-matched BPs (impulse-invariant poles, |H| matched at fc + Nyquist, exact DC zero). Phase-progressive parallel sum: all +, F2–F5 rotated by phase-matched 1st-order allpasses (fa/Fk 1.397/0.824/0.396/0.276 → −71/−101/−136/−149° at resonance), branch magnitudes untouched. Plain Klatt ± sum stays −23.6 dB (F1 skirt vs F2+F3, /u/ 1.9 kHz); new sum worst −9 dB over 600 random phoneme/register/throat/f0 points.
- Nasal pole-zero (270 Hz SVF, identity at nasalDip 0) on voice sum; log-domain epsilon-settled formant smoother + per-singer dirty coefficients.
- Vowel norm: gain −0.65·(L_vowel − L_field-centre) dB at the same pitch/register/throat/focus, loudness from calibrated LF harmonic powers.

| Attempt | Gate 2 Ferr / notch / vowel dev | 8snap presets outside ±1.5 dB | Other |
|---|---|---|---|
| 1 tract only | 0.64% / −5.4 / 6.11 ✗ | P23 +2.49 | 7dc −85.5 ✗ |
| 2 + smooth-loudness norm | 0.64% / −5.4 / 2.89 ✓ | P01 +3.05, P17 −1.63, P18 +2.19, P23 +5.24, P29 −3.77 | |
| 3 + harmonic-exact norm | 0.64% / −5.4 / 2.86 ✓ | P01 +3.18, P23 +5.50, P26 −1.55, P29 −2.16 | 7dc −79.0 ✗; other 12 gates ✓, 8cpu 9.0% |

Root cause (a conflict between locked gates, not an implementation defect):
- Gate-2 vowel levels are set by how F1 lines up with the source harmonics. At C3, /i/ and /u/ put F1 on h2, while /a/'s F1 falls between h5 and h6. A single singer measures −12.9 / −1.2 / −3.2 dB, so passing needs a ≥5 dB relative /a/ vs /i/,/u/ shift.
- P01 (the default patch at y 0.86) and P23 sit on the /a/ anchor and P14 on the /u/ anchor (same register, note 48), so any loudness law that passes gate 2 breaks 8snap. A 30-preset model with measured tract sensitivity per preset (0.15–0.98) gives: best non-monotonic law P07 1.74 dB; monotonic laws ≥3.1 dB.
- Filling the old F1/F2 antiphase notch alone raises /a/ voices 1.3–2.0 dB (rotation-independent, notch-safe candidates within 0.35 dB), so P23 is +2.49 dB even with no normalization.
- 7dc: louder tract output lifts sub-Hz IM residue through drive/tape (the filters have an exact DC zero). If revisited, this needs a stronger post-nonlinearity DC blocker.
- Decision (user, 2026-11-26): (b) re-scope the gate-2 vowel-RMS assertion.

## Phase 3.2 re-run — Vocal Tract / Formant Engine (PASS)
- Re-scope (user): `g2VowelRmsDb` (3.0, value unchanged) now asserts the tract's flat-source RMS (TractProbe IR energy, all 4 rates). The full-processor C3/C4 spread becomes a non-regression guard, `g2VowelSpreadDb` 7.86 (the Phase 2 baseline). No other threshold touched.
- `DSP/VocalTract.h`: 64-bit Vicanek-matched TPT SVF bank. Keeps the Klatt +F1/−F2 polarity, which sets the below-F1 harmonic balance the presets are voiced on. F2–F5 get phase-matched allpass rotations (fa/Fk 1.462/1.784/0.456/0.505, F3–F5 +). Worst notch −8.8 dB over 600 random states; source-weighted loudness vs the legacy sum −0.07 dB mean (p95 0.44); +4.4 dB air above F5, where the legacy sum cancelled.
- Rejected: the first run's all-+ rotation. It added up to +1.7 dB and darkened /a/ presets (P23 centroid 693→594 Hz). No vowel-gain normaliser either: all three laws broke 8snap, and so did tract-power levelling (P18 −2.3…−4.2 dB).
- Nasal pole-zero (270 Hz SVF) on the voice sum: identity at 0, replaces the legacy 0.35·nasalDip F2–F5 multiplier with the same HF loss. Log-domain ε-settled formant smoother; per-singer coefficient rebuild only when dirty. Breath and onset shapers → `Bandpass64` (64-bit, same clamps and cache).
- EntropyStage: added a 2nd DC stage (5 Hz) after the 15 Hz blocker. The tract change lifted sub-Hz drive-DC drift (7dc −88.3 without it).

| Gate | 3.1 → 3.2 |
|---|---|
| 2 | Ferr 1.48→0.66%, track 0.00%, lvl 0.02 dB, notch −23.5→−4.5 dB, tract vowel dev 2.40 dB, full-proc spread 7.22→7.37 dB |
| 3morph / 3toggle | d2 0.79× ref / 0.25× bound |
| 7dc | −99.8 → −123.5 dBFS |
| 8snap / 8cpu | max dRMS 1.31 dB (P24 −1.31, harness #23), peak −1.64 dBFS / 8.58% RT |
| inactive | 7steal 5.84→6.41× (3.3 target ≤5); 4alias / 4adaa pending (3.4) |
Notes: full-processor vowel loudness stays source-coupled by design (C3 /a/ −22.4 vs /i/ −10.6 dB). Remaining float filters: HarmonicLayers, EntropyStage oxide/head-bump, FDN damping (3.4 scope).

## Phase 3.3 — Choral Swarm, Humanization & Voice Management (PASS)
- Steal / fast release (D10): sample-accurate 4 ms raised-cosine output ramp. A stolen voice starts its pending note only when the ramp reaches exactly 0; synthesis is split into `renderSynth` segments at the ramp end, so this is block-invariant. EntropyStage is no longer reset on steal. CC120 / legato fast release uses the same ramp, then holds the voice muted. The old C0 last-sample bridge is gone.
- Voice management: budget-conserving loudness. Voices normalise to the uncapped singers-per-voice (`singersReference`), so a 24-singer choir split over many notes keeps its total energy (12-note cluster −5.3 dB; ≤3 notes unchanged). This was 7steal's real cause since Phase 2: the octave-higher 12-note chord drove `softSafetyLimit` (input ≈1.15). Steal clicks were never the cause; the steal-window d2 is ≈1.1× the reference.
- Humanization (D8), layered on the existing seeded singer draws so each preset keeps its ensemble take:
  - Tract and glottis: VTL scaling of formants and bandwidths (constant Q), F2–F5 shape ±1.8 %, tension offset ±0.04 (headroom-limited).
  - Vibrato 4.8–6.5 Hz with staggered delay, smoothstep onset and depth ×0.85–1.15.
  - Seeded per-note 1/f jitter (3–30 Hz, 1.0–1.7 cents rms) and shimmer (3–20 Hz, 1.6 % rms).
  - RC attack ×0.88–1.12; onset dispersion as its own render control (`onsetDispersion01` = effective spread).
  - An integer `noteSamples` counter replaces the float time accumulators.
- Placement (D9): equal-power, level-only panning (no inter-channel delay); seats mirrored per voice, seeded offset ±0.12. Rejected: a 120 Hz side high-pass on the dry bus (P20 −1 dB; gate 5 already at 0.97/0.99 correlation).
- Mode toggles: EntropyStage route crossfade 15 ms linear → 5 ms raised cosine.
- 8snap re-baseline (user, 2026-11-26), after 3 attempts that failed on random draws only (1.87 dB P20; 1.67 / 1.61 dB P24):
  - 6-seed study: 3.3 shifts preset level by +0.004 dB mean (rms 0.14, max 0.38); the 3.2 code itself passes on 2 of 6 seeds.
  - `preset_baseline.csv` re-captured from 3.3; the ±1.5 dB threshold is kept. The Phase 2 copy is archived as `Tests/QA/preset_baseline_phase2.csv`.
  - Cumulative Phase 2→3.3 drift: mean −0.50, sd 0.56 (P24 −1.61, P15 −1.33, P05 +1.15).

| Gate | 3.2 → 3.3 |
|---|---|
| 7steal | 6.41× → 3.40× |
| 3morph / 3toggle | d2 0.79× → 1.30× ref / 0.25× → 0.31× bound |
| 5 | comb −5.5/−6.9 → −5.1/−7.0 dB, corr 0.97/0.99, mono drop 0.72/0.84 dB |
| 7dc / 7dry | −123.5 → −106.7 dBFS / idle 0.49 % RT (3.2 code measures the same on this machine) |
| 8snap / 8cpu | re-baselined, peak −1.56 dBFS / 8.59 % RT |
Notes: the remaining 7steal margin is set by `softSafetyLimit` (memoryless tanh above 0.75) on dense chords, which is out of 3.3 scope. The 7dc change is random-draw level drive drift, 16.7 dB inside the limit.

## Phase 3.4 — Throat Drive ADAA & 64-bit Unitary FDN (PASS)
- 4alias root cause (D11): at 6 kHz and 8 kHz the 4th or 3rd harmonic folds exactly onto Nyquist. The JUCE half-band IIR passes fs/2 at −3 dB, and its non-linear phase makes that component non-zero on the sample grid, so it read −36 dBc in the bins next to Nyquist. Shaper aliasing was not the cause.
- New `DSP/ThroatDrive.h`:
  - `ThroatShaper`: double Padé-tanh f, closed-form F1 (log1p) and F2 (log1p/atan), ADAA1 (cached F1) and ADAA2, with a 1e-5 midpoint fallback.
  - `LinearPhase2x`: Kaiser 100 dB polyphase stage; 8-lane dot product, so it SLP-vectorizes.
- Oversampling routes:
  - Outer 48↔96k: Type II up (112 taps), Type I down (129 taps), passband 0.40 fs, stopband 0.50 fs.
  - 4x adds an inner 96↔192k stage: 30 up / 29 down taps.
  - The Type II half-sample offset cancels the ADAA1 half-sample delay, so PDC is an exact integer: 2x = 60, 4x = 67 (analytic, measured equal), bypass = 0.
  - The dry/wet blend uses a midpoint dry path, phase-matched to ADAA1.
- Route switch: the new FIR route is reset, then a pre-roll of latency + 32 samples holds the crossfade at 0 before the 5 ms raised cosine. Without it, 3toggle measured 1.75×.
- Oxide LP and head bump now use 64-bit `TptSvf64`; DC handling is 15 Hz + 5 Hz `DCBlocker64` (unchanged).
- FDN (D14), all double:
  - Unitary Hadamard 8×8 mixing; per-Space delays are rate-scaled distinct primes.
  - Phasor LFOs at ×0.6 depth; Catmull-Rom reads everywhere, linear-interp allpasses removed.
  - Shared-coefficient TPT wall low-pass, plus ISO 9613 air absorption (0.1 dB/m at 8 kHz × path length per line).
  - 5 Hz `DCBlocker64` per line.
  - `fastTanh` removed from the loop, replaced by a slow 50 ms RMS safety gain (0.35) and an identity-below-1 C1 guard.
  - Writes flush below 1e-30; feedback clamp 0.999 → 0.99995.

| Gate | 3.3 → 3.4 |
|---|---|
| 4adaa / 4alias | PENDING → PASS: F1′ err 1e-9, F2′ err 2e-9, cont 3e-8 / −29.6 → −52.7 dBc worst (6k −125, 8k −93.7, 10k −52.7 vs naive −10.6), DC −169.7 dBFS |
| 6pdc | 5/6/0 → 60/67/0 (rep = meas) |
| 7fdn | max rise −1.17 → −1.42 dB, end −184.6 → −189.1 dBFS, CPU 1.00× |
| 3toggle / 3morph | 0.31× → 0.29× bound / 1.30× → 0.78× ref |
| 7dry / 8snap / 8cpu | idle 0.49 → 0.43 % / max dRMS 0.88 dB (#21) / 8.59 → 9.95 % RT (cap 10.50) |
Audit: no audio-thread allocations or locks (design and assign happen in `prepare`), no float FDN or resonator states, no raw tanh added, no parameter-ID changes. CPU note: FIR + ADAA cost about 1.9 % RT at 12 voices; 8cpu margin is now 0.55 %.

## Phase 3.5 — Swarm SoA SIMD, Rational Transcendentals & Lock-Free Polish (PASS)
- Profile, 3.4 → 3.5: `sinf` 13 % → 0 and `log2f` 6 % → 0 of active samples. Per-singer scalar excitation, the FIR OS and the reverb are now the top costs.
- `sinTurns` (range reduction + odd Taylor polynomial to x^11) replaces every per-sample sine: singer vibrato, onset envelope, Sub-Abyss / Crown layers, tape wow/flutter and the de-click ramp.
  - Validated over 1e7 phases: max error 1.95e-7, i.e. 6e-6 cents vibrato and 3e-5 dB layer level.
- Glottal mip pick: `sampleLog2` takes log2 f exactly from MIDI pitch + cents (log2 440 + (m−69)/12 + c/1200), with `std::log2` only when f0 is clamped.
  - Validated: max 0.0018 cents and 1.6e-5 dB vs the `std::log2` path.
- `VocalTractBank` (SoA SIMD): 8 singer lanes, formant-major, fused 2×f64 kernel (GCC/Clang vector extensions, scalar fallback). Compile-time widths 2/4/8 come from the highest active lane.
  - Matches scalar `VocalTract` to 1.1e-16.
  - Microbenchmark vs scalar: 1.2× (3 singers), 1.6× (4), 1.6× (8).
  - Rejected: auto-vectorized split loops (0.55–0.83×; bool-mask selects and memory round-trips), and per-sample parked-lane snapshots (+0.35 % RT).
- Silent singers: settled singers keep the existing skip. Parked lanes inside the SIMD width run on zero excitation with masked output, so a re-entering singer starts from a decayed tract instead of a frozen one.
- Lock-free taps:
  - Telemetry: relaxed `std::atomic` throughout, now pinned by `is_always_lock_free` static_asserts (also on the BPM tap).
  - PDC: the APVTS listener + `AsyncUpdater` (a message post taking a lock, possibly from the audio thread under automation) is replaced by a 20 Hz message-thread `Timer`. It polls the atomic parameters and calls `setLatencySamples` only on change; `prepareToPlay` stays synchronous.

| Gate | 3.4 → 3.5 |
|---|---|
| 8cpu | 9.95 → 8.4–8.7 % RT (5 runs; cap 10.50) |
| 7dry / 7fdn | idle 0.43 → 0.45 % / CPU 1.00 → 1.01× (noise) |
| 1–6, 7dc, 7steal, 8snap | unchanged to the printed digit (8snap max dRMS 0.88 dB) |
Audit: no audio-thread allocations or locks (stack lane arrays only), no float tract states, no raw tanh added, no parameter-ID changes. The build is warning-clean apart from 1 pre-existing harness warning.

## Phase 4 — Final Build, Validation & Cleanup (PASS)
- Builds: clean Release and Debug of AU/VST3/Standalone, `MonolithQA`, `MonolithDspAudit`: 0 errors. Remaining warnings: 3 JUCE-only `-Wdeprecated-declarations` (1 harness cast fixed).
- Fixed (pre-existing): non-ASCII `char*` literals (`—`, `·`) hit JUCE's ASCII `String` ctor (57 Debug assertions, garbled UI text); now `String::fromUTF8` (preset 01 description, macro captions, phoneme footer). Debug re-run: 0 assertions.
- QA: Release `--phase 4` 16 PASS / 0 PENDING; Debug non-timing gates PASS (CPU gates are Release-only); `ctest` 1/1; legacy audit all PASS (13.0 % RT, was 10.91 % at Phase 1).
- `auval -v aumu Mnch Mnlt`: VALIDATION SUCCEEDED. New AU ad-hoc signed and installed temporarily, then the previous install was restored (verified identical). `pluginval`: SKIPPED (not installed).
- Packaging note: the AU in `build/release` fails `codesign -v` (no resource seal); run `codesign --force --deep -s -` after copying, or sign properly for distribution. VST3 not validated.

| Metric | Baseline (Phase 2) | Final |
|---|---|---|
| Gates | 9 PASS / 7 PENDING | 16 PASS |
| Glottal alias C5–C7 / DC | −91.9…−93.2 dBc / −144.6 | −91.4…−93.2 dBc / −147.7 dBFS |
| Formant error / notch / vowel RMS dev | 1.48 % / −23.5 / 7.86 dB | 0.66 % / −4.5 / 2.40 dB (tract; re-scoped gate) |
| Drive alias / DC | −29.6 dBc / −114 | −52.7 dBc / −169.7 dBFS |
| Block-size residual | −57.3 dBFS | exact |
| PDC 2x / 4x / bypass (rep/meas) | 4/5, 6/6, 4≠0 | 60/60, 67/67, 0/0 |
| Held-chord DC / steal d2 | −84.3 dBFS / 6.23× | −106.9 dBFS / 3.36× |
| Swarm comb / sub corr / mono drop | −7.7/−8.5 dB, 0.97/0.99, 0.75 dB | −5.0/−7.6 dB, 0.97/0.99, 0.84/0.85 dB |
| Idle CPU / FDN tail | 0.48 % / −184 dBFS | 0.45 % / −189 dBFS |
| 8cpu | 7.78 % RT | 8.75 % RT (1.12×; cap 1.35×) |
| Preset RMS | captured | max 0.88 dB vs the 3.3 re-baseline (Phase 2 copy archived) |
