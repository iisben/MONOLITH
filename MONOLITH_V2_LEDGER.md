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

## Stage 3 — Performance, Expression & Preset-Engine Architecture — PASS (2026-11-26)
- **Control plane** `Source/V2/ControlPlane.{h,cpp}` + `ModMatrix.h`: 35 lock-free shadow atomics (31 destinations + 4 macros). Engine/texture bind to them via a resolver (`SwarmEngine::prepare(..., resolver)`, `TextureLayer::bindParameters(..., resolver)`). Each block: shadow = base (exact copy when idle) or `from0to1(clamp(to0to1(base)+Σoffsets))`. Routing POD reaches the audio thread via a lock-free `TripleBuffer`.
- **Macros 1–4** = V1 macro params (no new IDs). Each has 8 routes (depth ±1, uni/bipolar), a per-preset label, and a legacy V1-curve gate. Further sources: XY X/Y, ModWheel, Pressure, Breath.
- **XY Sculptor** `v2_xy_x/y/mode` (Vowel Field · Light/Shadow · Preset Axes). One-pole smoothing (τ 60 ms) uses the real block dt, so it is invariant to sample rate and block size.
- **Expression** `Source/V2/MpeExpression.{h,cpp}`: MPE lower zone, per-note PB ±48 st, member CC74, per-channel pressure, poly AT, plus global CC74 and CC2. It folds into a per-voice copy of `VoiceRenderParams` (bend, effort/tilt, vibrato, openness). Texture swell is multiplicative and never un-bypasses the layer. Velocity curve (Linear/Soft/Hard/S-Curve/Fixed) is rewritten in place in the MidiBuffer.
- **MIDI Learn** `Source/V2/MidiLearn.{h,cpp}`: atomic CC→param table and SPSC FIFO, drained on a 50 Hz timer with gesture-bracketed host notifications. Scope is per preset or global (`~/Library/Application Support/Monolith Audio/MONOLITH II/MidiLearn.xml`). Right-click menu on every `MonolithDial` (`PluginEditor.cpp`).
- **Workflow**: A/B swaps routing as well as params, with a 60 ms shadow glide. `UndoHistory.h` holds snapshot undo/redo (350 ms debounce; preset/A-B/route edits commit). `Presets/PresetLibrary.{h,cpp}` uses a `version="2"` XML schema (name/author/category/tags/description/favourite/macro labels/params/V2Control/MidiMap), user dir `~/Library/Audio/Presets/Monolith Audio/MONOLITH II`, search and favourites.
- New params (append-only, `ParameterID{…,2}`): `v2_xy_x` `v2_xy_y` `v2_xy_mode` `v2_mpe_enable` `v2_mpe_pb_range` `v2_vel_curve` `v2_expr_vibrato` `v2_expr_effort` `v2_expr_openness` `v2_expr_swell`. MPE enable/range and velocity curve are global and never reset by presets.
- State: the session adds `V2Control`, `MidiMap`, `V2AB`, `V2PresetMeta`; all are stripped before `replaceState`. **Pre-existing bug fixed:** SlotA/SlotB used to nest into the APVTS tree, so the session grew on every save/load. It is now stable (9704 B over 3 cycles).
- Frozen DSP: render math untouched. The only `SwarmEngine` diff is the resolver in `prepare` and a nullable hook in `handleMidiMessage` and the voice loop; the V1 loop is kept verbatim as the `else` path.
- QA `MonolithQA --phase v2.3`: **34/34 PASS** (16 V1 + 7 V2 + 11 V3). Log `build/v2_qa/qa_v2_stage3.log`. `MonolithDspAudit` 6/6 PASS. AU/VST3/Standalone Release build clean.
  - v2reg 0.0005 dB (unchanged) · v3null armed-idle 0.0 diff · v3macro route +2.0 dB, legacy-off dead 0.0, bipolar@0.5 0.0 · v3xy smoothing err <1e-5 (44.1–192k) · v3mpe +12 st odd/even 7.0→−37.3 dB, all channel-isolation diffs 0.0
  - v3ab/v3undo/v3state/v3preset (30/30 V1 presets bit-exact via V2 XML)/v3learn all PASS · v3alloc 0 allocs · 8cpu 8.58 % RT
- Adapter change: the `v2state` gate counts v2_* params dynamically (Stage 2 hard-coded 10; there are now 20). Thresholds are unchanged.

### Warnings
- MIDI-Learn values reach the audio ≤20 ms late (message-thread drain). In an offline bounce, learned-CC moves take effect after timer ticks, not sample-accurately.
- Undo also records settled host-automation changes (snapshot polling cannot tell where a change came from).
- Stage 4 must surface the following in the UI: macro labels and routing editor, XY pad on `v2_xy_*`, MPE indicator (`MpeExpression::uiMpeActive`), undo/redo, and the preset browser. The only Stage 3 UI is the right-click Learn menu.
- ASSET_PROVENANCE licensing blocker from Stage 2 still open.

Stage 3: PASS · Next: Stage 4

## Stage 4 — NAMM 2027 Flagship UI/UX Craft & Visual Self-Inspection — PASS (2026-11-26)
- **MONOLITH II editor** (1200×800 design canvas, 8 px grid, root scaled by an `AffineTransform`). Look: obsidian chassis, hairline bevels, matte plates, amber/ivory arcs, Avenir Next + Menlo numerals. Rewritten: `PluginEditor`, `UI/MonolithLookAndFeel`, `UI/MeterComponents`, `UI/SectionPanels`. New: `UI/{CachedLayer.h, TopBar, HeroVisualizer, OutputAnalyser, PresetBrowser, MacroEditor, UiText.h}`. Removed: `UI/HeaderBar`, `UI/PhonemeFieldComponent`.
- **Zero-frame-drop rendering:** `CachedLayer` pre-renders static chrome (chassis, plates, knob faces/ticks, sculptor grid and vowel map, axis chips) at the physical pixel scale. It re-renders only when scale, size or content changes. Snapshot runner check: 0 chassis re-renders over 120 live frames.
- **Output tap** `Source/V2/AudioTap.h`: preallocated SPSC `AbstractFifo` stereo ring (32k frames). The audio thread pushes the final bus (texture included) only while the editor is open, and drops frames when full instead of blocking. `OutputAnalyser` runs on the message thread: peak with hold and clip latch, K-weighted LUFS (M 400 ms / S 3 s), 2048-pt FFT → 160 log bands, formant picking F1–F4.
- **Hero:** spectral/formant contour plus swarm presence field (haze, formant rings, singer motes), refreshed at 60 Hz. Draggable XY Vowel/Light/Axes sculptor with a glowing trail, tether, drag readout and double-click reset.
- **Top bar:** wordmark, Prev/Next, preset display, browser, SAVE, INIT (`loadInitPatch`), A/B/A→B, undo/redo, MPE badge (right-click sets bend range), zoom menu (75–150 %) plus corner drag with locked aspect ratio, persisted as `<V2UI scale>`, and a stereo peak/LUFS-S meter with clip latch.
- **Dock:** 4 hero macros with per-preset names, route captions and live mod rings; route editor overlay (8 routes, depth ±100, bipolar, V1 curve gate); Texture/Space quick mixes and source; perform section.
- **Overlays:** preset browser (search, categories with counts, favourites, tags, detail pane, load/delete) and save dialog.
- **Tactile:** custom `MonolithDial` with hover/drag precise readouts, Shift/Cmd fine drag, double-click/Alt reset, wheel, tooltips, right-click MIDI Learn, accessibility values, and bipolar or zero-origin arcs. Routed destinations show live modulation rings.
- **Snapshot runner** `Tests/UI/UISnapshotRunner.cpp` (`MonolithUISnapshot`, headless `ScopedJuceInitialiser_GUI`) writes 14 PNGs to `build/v2_qa/ui_snapshots/`: main view, browser, macro/mod-active, macro editor, texture+expression, save dialog (each at 1x and 2x), plus zoom at 75 % and 150 %.
  - Inspection passes fixed: cramped knob rows (uniform 80 px cells, 8 px gaps), −12..0 arcs drawn from 0, "-0.0" readouts, idle SLIDE meter showing 50 %, axis captions buried under the swarm (moved to a cached chip layer), mod rings hidden headless (`isShowing` replaced by a visibility check), the hidden search glyph, a clipped half-row in the browser, ambiguous route-glyph placement, redundant destination names, and "20000 Hz" (now 20.0 kHz).
- **QA** `MonolithQA --phase v2.4`: **36/36 PASS** (34 prior + `v4tap`, `v4ui`). Log `build/v2_qa/qa_v2_stage4.log`.
  - v2reg 0.0005 dB (unchanged) · v4tap: on/off diff 0.0, tap == output bit-exact, 0 allocs, overflow drops without blocking · v4ui: scale round-trip, clamp and V1-session default OK; Init leaves 0 params off default · 8cpu 8.55 % RT
- `MonolithDspAudit` 6/6 PASS · `auval` SUCCEEDED · AU/VST3/Standalone/QA/Audit/UISnapshot Release `-O3` build: 0 warnings · `git diff Source/DSP` empty.

### Warnings
- Resolves Stage 2's "meter excludes texture" warning: the meter now reads the post-texture tap. Every Stage 3 UI item listed above is now surfaced.
- Fonts: Avenir Next/Menlo are macOS system fonts. A Windows build needs embedded font binaries; JUCE falls back silently.
- The ASSET_PROVENANCE licensing blocker (Stage 2) is still open.

Stage 4: PASS · Next: Stage 5

## Stage 5 — 20 Flagship Production-Ready Presets (Closed-Loop Audio QA) — PASS (2026-11-27)
- **Source of truth:** `Tools/v2_flagship_presets.py` holds 20 presets tuned from V1 sweet spots. It writes `assets/v2_presets/NN_Name.mnlpreset` (V2 schema: full 58-param state with no globals, 4 named macros, CC1/pressure/CC2 routes, XY Preset Axes on Ethereal, MPE depths). It is embedded via `juce_add_binary_data(MonolithV2Presets)` (93.5 KB) and validates ids, ranges, routes and slot-bound V1 labels. It also writes a preset→asset dependency table into `ASSET_PROVENANCE.md`.
- **Library/host:** `PresetRecord::flagshipIndex` / `isBuiltIn()` and `preset_schema::loadFlagshipPresets()` (embedded or from a directory). Browser order: flagship, then V1, then user. Host programs: 0–29 are V1 (indices unchanged) and 30–49 are flagship. Session meta stores `flagshipIndex`.
- **UI:** built-ins can't be deleted, and the detail pane shows a MONOLITH II badge. Long category names wrap to two lines instead of showing an ellipsis, and the top-bar subtitle drops the author when it doesn't fit. Snapshot runner adds `08_flagship_main`, `09_flagship_browser` (inspected).
- **QA runner** `Tests/QA/PresetQA.cpp` (`MonolithPresetQA`, ctest): 20 renders/preset (susC3/C4, vel-110 4-note chord, C5 melody, 4 macro sweeps + 4 at 100 %, all-max, CC1 sweep/0/127, pressure 127, MPE ± per-note pressure/slide, audition). Locked thresholds: 4x true peak ≤ −0.5 dBTP; chord RMS −19…−11; NaN/Inf 0; DC (Hann-weighted 0 Hz) < −80; sub-150 corr ≥ 0.4 and mono drop ≤ 4 dB on chord and C3; macro Δ ≥ 1.5 dB; CC1 Δ ≥ 1.5; pressure and MPE Δ ≥ 1.0; meta; near-duplicate score ≥ 1. `--preset-dir` gives a no-rebuild tuning loop and flags a stale embedded copy.
- **Result (embedded, Release):** **20/20 PASS**, diversity PASS (min score 1.98, #1~#3), library PASS. Snapshot: `Tests/QA/preset_qa_v2_stage5.csv`. Log `build/v2_qa/preset_qa_stage5.log`.
  - TP ≤ −1.0 dBTP worst (#19) · chord RMS −16.3…−12.6 · sub150 corr ≥ 0.74 · mono drop ≤ 1.0 dB · DC ≤ −103 dBFS · macro Δ ≥ 2.0 dB · CC1 ≥ 11.1 · pressure ≥ 3.1 · MPE ≥ 4.9
  - Audition: 20 × 4 s 48 kHz/24-bit WAV → `build/v2_qa/audition/01_…20_*.wav`. The phrase follows the tags: melody (solo/lead/hook), stabs, evolving (MACRO 1 bloom), drone, otherwise progression.

| Category | Preset — TP dBTP / chord RMS dBFS / weakest macro Δ dB |
|---|---|
| Monumental & Cathedral Choirs | Cathedral of Dunes −1.1/−15.1/2.5 · Basilica Eternal −1.6/−13.2/2.6 · Siege of Giants −3.2/−14.3/3.0 · Seraphim Ascendant −1.2/−13.2/2.7 |
| Operatic & Expressive Soloists | Prima Donna −1.3/−16.2/2.6 · Basso Lamentoso −2.7/−14.6/3.5 · Tenore Eroico −2.8/−14.7/3.1 · Moonlit Mezzo −1.3/−14.9/2.6 |
| Festival, DJ & Club Anthems | Euphoria Mainstage −1.1/−16.3/3.5 · Formant Ignition −6.1/−14.1/3.1 · Afterhours Hymn −2.0/−12.9/3.9 · Siren Tension −1.8/−13.5/3.6 |
| Dark Cinematic & Witch House Rituals | Obsidian Rite −3.7/−13.3/3.6 · Haunted Mixtape −2.7/−13.3/3.2 · Mourning Wraiths −1.5/−13.5/2.5 · Leviathan Vigil −1.7/−12.6/2.9 |
| Ethereal Hybrids & Evolving Textures | Prism Bloom −1.3/−15.4/2.6 · Breathing Nebula −1.4/−13.2/2.1 · Bowed Spirit Choir −1.0/−15.9/2.0 · Aeolian Drift −1.2/−14.5/4.5 |

- **Regression lock:** `MonolithQA --phase v2.4` **36/36 PASS** (v2reg 0.0005 dB unchanged · 8snap 0.88 dB = Stage 4 · 8cpu 8.80 % RT). Adapter: gate8/v2reg count V1 presets via `getPresetManager()` because programs now number 50; thresholds untouched. `MonolithDspAudit` 6/6. AU/VST3/Standalone + all tools Release `-O3`: 0 project warnings (9 JUCE-internal Metal deprecations, macOS 27 SDK). `git diff Source/DSP` empty.

### Warnings
- Tuning finding: vowel **orbit** combined with closed (low-Y) vowels produces real infrasonic/DC drift in the frozen engine (up to −72 dBFS). Presets avoid it with shallow orbits and vowel centres pY ≥ 0.42. User patches can still reach it.
- 10/20 presets use the texture layer, so they inherit the open **ASSET_PROVENANCE licensing blocker** (table in that file).
- QA is locked at 48 kHz. Stage 6 should spot-check 44.1/96 kHz true peak, and AU/VST3 hosts with 50 programs (auval/pluginval).

Stage 5: PASS · Next: Stage 6

## Stage 6 — NAMM 2027 Release Build, Host Validation & macOS Installation — PASS (2026-11-27)
- **Build:** clean (`--clean-first`) Release `-O3` + Debug, all targets (AU/VST3/Standalone, MonolithQA, DspAudit, PresetQA, UISnapshot): 0 errors, **0 project warnings**. The 30 remaining warnings are 3 JUCE-internal Metal deprecations (`juce_CGMetalLayerRenderer_mac.h`, macOS 27 SDK), repeated across translation units: not critical, and the fix belongs upstream.
- **Tests (Release):** DspAudit 6/6 · MonolithQA v2.4 36/36 (16 V1 gates; v2reg 0.0005 dB, 30 V1 presets; 8snap 0.88 dB; 8cpu 8.69 % RT) · PresetQA 20/20 + diversity + library · ctest 2/2 · UISnapshot OK (0 chassis re-renders) · `git diff Source/DSP` empty.
- **Debug (asserts on):** PresetQA subset 6/6 with no assertion hits. DspAudit 5/6: its CPU gate fails at −O0 (130 % RT, expected). The V1 harness also logs JUCE Timer asserts because it builds an APVTS without a MessageManager; it is untouched since `a135246`, so it stays as is.
- **Host-rate spot-check (Stage 5 warning):** new `MonolithPresetQA --rate <Hz>` re-runs every LOCKED assertion; audition export is 48 kHz only. At 96 kHz #18 Breathing Nebula failed at TP −0.22 dBTP (pressure127, engine-dominated). Fix: `master_gain` −5.5 → −6.5 dB in `Tools/v2_flagship_presets.py`, regenerated. Now **20/20 at 44.1 / 48 / 96 kHz**. Snapshots in `Tests/QA/preset_qa_v2_stage6_{44100,48000,96000}.csv`; at 48 kHz only #18 differs from Stage 5.
- **auval** `-strict -v aumu Mnch Mnlt`: **AU VALIDATION SUCCEEDED** (50 factory presets, 61 params, cold open 29 ms).
- **pluginval 2.0.0** (Tracktion, notarized; `~/Downloads/pluginval.app`) strictness 5: **VST3 PASS 21/21 · AU PASS 21/21**. Covers 50 programs, Steinberg vstvalidator exit 0, auval, editor whilst processing, automation, and 44.1/48/96 kHz × block 64–1024. Info only: the AU current program is −1 until the host picks one (AU SDK `AUBase` default).
  - Extra strictness 10: VST3 29/31, AU 30/31. The only failures are the (Discrete) parameter state restoration tests on the 7 bool params. Cause is in JUCE 8.0.15: `AudioParameterBool::setValue` keeps the raw value unsnapped, and the APVTS skips the restore when the snapped value already matches. The effective on/off state is always correct and the engine reads the snapped value.
- **Sign + install:** ad-hoc (`codesign --force --deep -s -`; the keychain has no Developer ID), `--verify --deep --strict` OK. Installed to `~/Library/Audio/Plug-Ins/Components/MONOLITH.component`, `~/Library/Audio/Plug-Ins/VST3/MONOLITH.vst3` and `~/Applications/MONOLITH.app`; AU registered. The previous V1 install (2026-11-20) was moved to `build/install_backup_v1_20261120/`. Repeat with `Tools/v2_release_install.sh [identity]`.
- **Cleanup:** removed the Stage 1–6 scratch logs/CSVs from `build/v2_qa/`; kept `audition/` (20 WAVs), `ui_snapshots/` (18 PNGs) and `asset_manifest.json`.

### NAMM 2027 Readiness Summary
- **V1 DSP integrity:** frozen core untouched in Stages 1–6. All 30 V1 presets within 0.0005 dB of the Stage 1 baseline (lock ±0.05). DspAudit 6/6.
- **V2 features:**
  - ✔ Texture Layer A (8 assets, 8.0 MB, bypassed at 0 %)
  - ✘ IR Space Layer B (no IRs on the drive)
  - ✔ Control plane (4 macros × 8 routes, XY Sculptor, MPE/AT/CC2/CC74, MIDI Learn, A/B, undo)
  - ✔ V2 schema + V1 migration
  - ✔ MONOLITH II editor
  - ✔ 20 flagship presets (programs 31–50)
- **20 presets (44.1/48/96 kHz):** chord RMS −16.4…−12.0 dBFS · worst TP −0.67 dBTP (96 k), −1.0 (48 k) · sub-150 Hz corr ≥ 0.74 · mono drop ≤ 1.2 dB · DC ≤ −102 dBFS.
- **Host validation:** auval strict PASS · pluginval s5 VST3 PASS / AU PASS · s10 VST3 31/31 / AU 31/31 after the post-release bool fix (below).
- **Before sale:**
  - (1) ⚠ `ASSET_PROVENANCE.md`: all 8 Splice texture assets are NOT CLEARED; 10/20 presets depend on them.
  - (2) Developer ID signing and notarization are needed; the ad-hoc build is for this Mac only.
  - (3) Bundle version is still 1.0.0 (`project(MONOLITH VERSION …)`).
  - (4) ✔ Done post-release (below): a snapping bool param for the s10 restore tests.
  - (5) Orbit plus closed-vowel DC behaviour in the frozen engine (Stage 5).
  - (6) Windows needs embedded fonts.

Stage 6: PASS

## Post-release — pluginval strictness 10 (2026-11-27)
- **Fix:** new `Source/State/SnappedBoolParameter.h`, a `RangedAudioParameter` that stores only 0 / 1 (snaps at the same ≥ 0.5 threshold `AudioParameterBool::get()` uses). It replaces `juce::AudioParameterBool` for the 7 bool params (`orbit_enable`, `orbit_sync`, `oversample_hq`, `space_freeze`, `cc64_freeze`, `legato_enable`, `v2_mpe_enable`), with the same `ParameterID{id, version}`, names, defaults, 2 steps, discrete/boolean flags and On/Off text. The effective state and the APVTS raw value the engine reads are unchanged. `git diff Source/DSP` is empty.
- **pluginval 2.0.0 strictness 10:** **VST3 31/31 · AU 31/31** (was 29/31 · 30/31). Before/after on VST3: identical `Parameter info` for all params; the Stage 6 build still fails 7/14 discrete restores, the new build passes 14/14. AU reports the 7 params as `isBoolean 1`, 2 steps.
- **Regression:** Release `-O3`, 0 project warnings (only the 3 JUCE Metal deprecations) · MonolithQA v2.4 36/36 (v2reg 0.0005 dB, 8snap 0.88 dB, 8cpu 8.78 % RT) · DspAudit 6/6 · PresetQA 20/20 + diversity + library. Reinstalled with `Tools/v2_release_install.sh`; AU registered.

STATUS: ALL_STAGES_COMPLETE
