# MONOLITH II — V2 Ledger

Branch: `feat/monolith-flagship-dsp` (existing feat/ branch, kept per spec rule 2). Base HEAD: `d2fd50f`.

## Stage 1 — DSP Freeze, Repo Audit & V2 Blueprint — PASS (2026-11-26)

### Environment
- Build: CMake ≥3.22 + Ninja, `build/release` (Release, `-O3 -DNDEBUG`), `build/debug`. `build/` gitignored ✔
- JUCE 8.0.15 (FetchContent), C++20, formats AU / VST3 / Standalone. Codes: manu `Mnlt`, plugin `Mnch`, AU type `aumu`. Bundle `com.monolithaudio.monolith`.
- Targets: `MONOLITH` (plugin), `MonolithDspAudit` (Tests/DspAuditTest.cpp), `MonolithQA` (Tests/QA/MonolithQA.cpp, headless, `MONOLITH_HEADLESS_QA=1`, ctest).
- Samples drive `/Volumes/Audio Blob/Samples`: **MOUNTED** (not scanned).

### Frozen DSP Core (read-only math)
`Source/DSP/DspMath.h` · `VocalTract.h` · `ThroatDrive.h` · `SwarmVoice.{h,cpp}` · `SwarmEngine.{h,cpp}` · `PhonemeField.{h,cpp}` · `HarmonicLayers.{h,cpp}` · `EntropyStage.{h,cpp}` · `BrutalistReverb.{h,cpp}`
Control plane only may touch `SwarmEngine::handleMidiMessage` input mapping via new hooks; no edits to render math.

### Index (`file : symbol (lines)`)
- `Source/PluginProcessor.h` : `MonolithAudioProcessor` (13-69), apvts/engine/telemetry/presetManager members (57-63)
- `Source/PluginProcessor.cpp` : ctor (8-15), `prepareToPlay` (22-27), `processBlock` (43-59), `applyFactoryPreset` (98), `timerCallback` PDC poll (113-119), `get/setStateInformation` (121-142) — no state version tag yet
- `Source/State/ParameterIDs.h` : 41 V1 IDs (8-56), `defaults` (59-108), `createParameterLayout` (110-222); all `ParameterID{id,1}`
- `Source/Presets/PresetManager.h` : `FactoryPreset{name,category,description,params}` (15-21), `PresetManager` (27-60)
- `Source/Presets/FactoryPresets.cpp` : `buildDefaultsList` (13-~60), 30 presets (69-545), `applyPreset` (591), A/B `initializeAB/toggleAB/copyActiveToInactive` (613-646), `save/loadStateToXml` (648-687)
- `Source/DSP/SwarmEngine.cpp` : `prepare` (27), `getLatencySamples` (141), note on/off (203/265), `handleMidiMessage` (316-392: PB, ch/poly AT, CC1 mod-wheel, CC64), `processBlock` (393-)
- `Source/State/TelemetrySnapshot.h` : `TelemetrySnapshot` atomics (13-52)
- `Source/PluginEditor.{h,cpp}` : editor 1180×720, resizable 885×540–2360×1440 (h:17-18, cpp:42-50), 20 Hz timer (73)
- `Source/UI/` : `MonolithLookAndFeel` + `palette` (h:8-96), `HeaderBar` (preset box/step/A-B), `SectionPanels` (Ensemble/ToneLayers/Orbit/Entropy/Space/PerformanceDock), `PhonemeFieldComponent` (XY vowel field + trail), `MeterComponents` (`MonolithDial`, `StereoLevelMeter`, `ExpressionBar`, `VoiceBadge`)

### V1 Preset Inventory
30 presets across 5 categories: Epic Cinematic (01-08), Lorn & Dystopia (09-15), Witch House & Darkwave (16-22), Organic Acoustic (23-26), Evolving & Horror Soundscapes (27-30).

### V1 Baseline Regression Snapshot (Release -O3, 48 kHz, deterministic: two runs bit-identical)
- `MonolithQA --phase 4`: **16/16 sub-gates PASS**, exit 0. Log `build/v2_qa/qa_phase4.log`.
- Locked V2 baseline: `Tests/QA/preset_baseline_v2_stage1.csv` (30 rows). V2 regression gate = |ΔRMS| ≤ 0.05 dB against this file.
- Preset RMS −18.64 … −14.22 dBFS; peak −8.43 … −1.54 dBFS (#23); centroid 189 … 2010 Hz; sub150 corr −0.24 (#20) … 0.89.
- Latency (PDC): 60 smp (drive 2x), 67 (4x HQ), 0 (drive bypassed). CPU median 8.49 % RT (Phase-2 baseline 7.78 %).

### V2 Feature Blueprint
- **Audio, additive and post-core, bypassed by default:** `v2::TextureLayer` (Layer A: pre-decoded breath/air/choir buffers, Hermite rate, vel envelope, HP/LP, width) and `v2::IrSpace` (Layer B: `juce::dsp::Convolution`, prepared in `prepareToPlay`, PDC is added only when the mix is above 0). Both are summed after `engine.processBlock` in `PluginProcessor::processBlock`. At the default of 0 % they skip early, so they add no latency and change no output samples.
- **New params (append-only, `ParameterID{…,2}`):** `v2_tex_*`, `v2_ir_*`, `v2_xy_*`, `v2_mpe_*`, `v2_vel_curve`, plus macro routing stored in the preset/state tree rather than as params.
- **Macros:** reuse the existing 4 V1 macro params (COLOSSUS/SPECTER/ROT/ABYSS) as the MACRO 1–4 slots, and add per-preset assignable routing on top. This adds no new IDs and keeps V1 behaviour. (Decision for Stage 3.)
- **Control plane:** XY Sculptor drives `phoneme_x/y` + custom axes; MPE zone (±48 st PB, CC74, poly AT) and CC2 map onto per-voice expression targets; MIDI Learn is a lock-free CC→param table; UndoManager on APVTS; state XML gets `version="2"` with a V1 migration path (the current state has no version attribute, so its absence means V1).
- **UI (Stage 4):** cached static chassis images; top bar (wordmark, preset bar + browser overlay, A/B, undo/redo, MPE, zoom 75–150 %, meter); centre spectral/formant visualiser + XY; bottom dock (4 macros + Texture/Space mix).
- **20-preset map (Stage 5):** Monumental & Cathedral Choirs ×4 · Operatic & Expressive Soloists ×4 · Festival/DJ/Club Anthems ×4 · Dark Cinematic & Witch House Rituals ×4 · Ethereal Hybrids & Evolving Textures ×4.

### Warnings
- V1 #20 (sub150 corr −0.24) and #18 (0.06) would fail the Stage 5 mono rule. That rule applies only to the new presets, so V1 stays frozen.
- 8cpu is 0.7 pt above the Phase-2 baseline but within its threshold.

Stage 1: PASS · Next: Stage 2
