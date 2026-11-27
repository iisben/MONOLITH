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

## Stage 2 — Sample Curation & Additive V2 Audio Layers — PASS (2026-11-26)
- Scan: 2 ranked Python passes over `/Volumes/Audio Blob/Samples` (≤40 lines each). **Layer B skipped**: no real IRs on the drive (only "IR" hit = Indian Roots bell loop) → Layer A expanded to 8 assets.
- Assets: `assets/v2_samples/*.flac` (8 files, **8.04 MB**, 48 kHz/24-bit, −12 dBFS active RMS, ≤−1 dBFS peak, DC ≤ −85 dBFS, loop seam step < 0.6× 99.9th-pct step). Pipeline `Tools/v2_process_assets.py` → also generates `Source/V2/TextureAssetTable.h` + `ASSET_PROVENANCE.md`.
- New: `Source/V2/TextureLayer.{h,cpp}` : `TextureBank` (shared, decoded once, guard-padded), `TextureLayer` 12 voices/16 slots, Hermite, vel env, 3 ms steal fade, CC64/120/123, TPT SVF HP→LP, M/S width, 20 ms mix smoothing, engine-PDC alignment ring (adds 0 latency), hard bypass at 0 % mix.
- Wiring: `PluginProcessor.cpp` processBlock sums layer after `engine.processBlock`; state writes `monolithStateVersion="2"`; V1 sessions → V2 defaults (bypassed). CMake `juce_add_binary_data(MonolithV2Assets)`.
- Params (append-only, `ParameterID{…,2}`): `v2_tex_mix` `v2_tex_asset` `v2_tex_tune` `v2_tex_keytrack` `v2_tex_attack` `v2_tex_release` `v2_tex_vel_sens` `v2_tex_hp` `v2_tex_lp` `v2_tex_width` (also in `buildDefaultsList`).
- QA `MonolithQA --phase v2.2`: **23/23 PASS** (16 V1 gates + 7 V2). Log `build/v2_qa/qa_v2_stage2.log`. `MonolithDspAudit`: 6/6 PASS.
  - v2reg max |ΔRMS| 0.0005 dB vs stage-1 csv (CSV rounding; output bit-exact) · v2bypass 0.0 diff (all V2 params moved, and post-fade)
  - v2lat PDC 0/60/67 unchanged, texture aligned (err −147 dB) · v2tex worst click ratio 5.92 (<12), NaN 0, tail silent · v2state version/round-trip/V1 migration OK
  - v2alloc 0 heap allocs in 390 armed blocks · v2cpu +0.14 % RT · 8cpu 8.75 % RT (within lock)
- Frozen DSP: `git diff Source/DSP` empty.

### Warnings
- **LICENSING BLOCKER for sale:** all 8 assets come from Splice packs, and the standard Splice licence bars redistributing them inside an instrument. Every row in `ASSET_PROVENANCE.md` is NOT CLEARED until the user signs it off or the asset is replaced.
- At 100 % mix a 14-note steal storm peaks the texture at −0.4 dBFS. Stage 5 gain staging must cap per-preset mix accordingly.
- Bank RAM 17 MB (decoded float, shared across instances). Notes that are already held when the mix rises from 0 stay silent; the layer reacts only to new notes.
- The output meter (`TelemetrySnapshot`) is engine-only and does not include the texture layer yet (Stage 4).

Stage 2: PASS · Next: Stage 3
