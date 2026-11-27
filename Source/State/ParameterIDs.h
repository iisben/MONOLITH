#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../V2/TextureAssetTable.h"

namespace monolith::params
{

// ── Field & Orbit ─────────────────────────────────────────────────────────────
inline constexpr const char* phonemeX       = "phoneme_x";
inline constexpr const char* phonemeY       = "phoneme_y";
inline constexpr const char* orbitEnable    = "orbit_enable";
inline constexpr const char* orbitRate      = "orbit_rate";
inline constexpr const char* orbitDepth     = "orbit_depth";
inline constexpr const char* orbitSync      = "orbit_sync";

// ── Ensemble & Register ───────────────────────────────────────────────────────
inline constexpr const char* registerMode   = "register_mode";
inline constexpr const char* ensembleSize   = "ensemble_size";
inline constexpr const char* ensembleSpread = "ensemble_spread";
inline constexpr const char* vibratoDepth   = "vibrato_depth";
inline constexpr const char* attackTime     = "attack_time";
inline constexpr const char* releaseTime    = "release_time";
inline constexpr const char* onsetMode      = "onset_mode";
inline constexpr const char* onsetAmount    = "onset_amount";

// ── Tone, Formant & Layers ────────────────────────────────────────────────────
inline constexpr const char* glottalTension = "glottal_tension";
inline constexpr const char* breathAmount   = "breath_amount";
inline constexpr const char* throatShift    = "throat_shift";
inline constexpr const char* formantFocus   = "formant_focus";
inline constexpr const char* subAbyss       = "sub_abyss";
inline constexpr const char* spectralCrown  = "spectral_crown";
inline constexpr const char* masterGain     = "master_gain";

// ── Entropy & Degradation ─────────────────────────────────────────────────────
inline constexpr const char* driveAmount    = "drive_amount";
inline constexpr const char* tapeInstability= "tape_instability";
inline constexpr const char* oxideDamping   = "oxide_damping";
inline constexpr const char* screwPitch     = "screw_pitch";
inline constexpr const char* screwDepth     = "screw_depth";
inline constexpr const char* oversampleHq   = "oversample_hq";

// ── Architectural Space + Freeze ──────────────────────────────────────────────
inline constexpr const char* spaceType      = "space_type";
inline constexpr const char* spaceMix       = "space_mix";
inline constexpr const char* spaceDecay     = "space_decay";
inline constexpr const char* spaceDamp      = "space_damp";
inline constexpr const char* spaceShimmer   = "space_shimmer";
inline constexpr const char* spaceFreeze    = "space_freeze";
inline constexpr const char* cc64Freeze     = "cc64_freeze";

// ── Performance Dock & Macros ─────────────────────────────────────────────────
inline constexpr const char* macroColossus  = "macro_colossus";
inline constexpr const char* macroSpecter   = "macro_specter";
inline constexpr const char* macroRot       = "macro_rot";
inline constexpr const char* macroAbyss     = "macro_abyss";
inline constexpr const char* legatoEnable   = "legato_enable";
inline constexpr const char* glideTime      = "glide_time";
inline constexpr const char* expressionLevel= "expression_level";

// ── V2 Layer A: Texture Engine (append-only, ParameterID version 2) ───────────
inline constexpr const char* v2TexMix       = "v2_tex_mix";
inline constexpr const char* v2TexAsset     = "v2_tex_asset";
inline constexpr const char* v2TexTune      = "v2_tex_tune";
inline constexpr const char* v2TexKeytrack  = "v2_tex_keytrack";
inline constexpr const char* v2TexAttack    = "v2_tex_attack";
inline constexpr const char* v2TexRelease   = "v2_tex_release";
inline constexpr const char* v2TexVelSens   = "v2_tex_vel_sens";
inline constexpr const char* v2TexHighPass  = "v2_tex_hp";
inline constexpr const char* v2TexLowPass   = "v2_tex_lp";
inline constexpr const char* v2TexWidth     = "v2_tex_width";

// ── V2 Stage 3: XY Sculptor, MPE & expressive MIDI (append-only, control plane only) ──
inline constexpr const char* v2XyX          = "v2_xy_x";
inline constexpr const char* v2XyY          = "v2_xy_y";
inline constexpr const char* v2XyMode       = "v2_xy_mode";
inline constexpr const char* v2MpeEnable    = "v2_mpe_enable";
inline constexpr const char* v2MpePbRange   = "v2_mpe_pb_range";
inline constexpr const char* v2VelCurve     = "v2_vel_curve";
inline constexpr const char* v2ExprVibrato  = "v2_expr_vibrato";
inline constexpr const char* v2ExprEffort   = "v2_expr_effort";
inline constexpr const char* v2ExprOpenness = "v2_expr_openness";
inline constexpr const char* v2ExprSwell    = "v2_expr_swell";

/** Controller-setup parameters: global performance settings that presets never store or reset. */
inline bool isGlobalPerformanceParam (const juce::String& id)
{
    return id == v2MpeEnable || id == v2MpePbRange || id == v2VelCurve;
}

/** State schema version written into the APVTS root (absent attribute = V1 session). */
inline constexpr const char* stateVersionAttr = "monolithStateVersion";
inline constexpr int         stateVersion     = 2;

// ── Default Values (Calibrated to Preset 01: Monolith Awakening) ──────────────
namespace defaults
{
    inline constexpr float phonemeX        = 0.50f;
    inline constexpr float phonemeY        = 0.86f;
    inline constexpr bool  orbitEnable     = false;
    inline constexpr float orbitRate       = 0.18f;
    inline constexpr float orbitDepth      = 0.35f;
    inline constexpr bool  orbitSync       = false;

    inline constexpr int   registerMode    = 4; // SATB Choir
    inline constexpr float ensembleSize    = 20.0f;
    inline constexpr float ensembleSpread  = 0.68f;
    inline constexpr float vibratoDepth    = 0.36f;
    inline constexpr float attackTime      = 0.38f;
    inline constexpr float releaseTime     = 1.50f;
    inline constexpr int   onsetMode       = 0; // Soft Breath (H)
    inline constexpr float onsetAmount     = 0.28f;

    inline constexpr float glottalTension  = 0.68f;
    inline constexpr float breathAmount    = 0.20f;
    inline constexpr float throatShift     = 0.0f;
    inline constexpr float formantFocus    = 0.60f;
    inline constexpr float subAbyss        = 0.42f;
    inline constexpr float spectralCrown   = 0.36f;
    inline constexpr float masterGain      = 0.0f;

    inline constexpr float driveAmount     = 0.24f;
    inline constexpr float tapeInstability = 0.14f;
    inline constexpr float oxideDamping    = 0.12f;
    inline constexpr float screwPitch      = 0.0f;
    inline constexpr float screwDepth      = 0.0f;
    inline constexpr bool  oversampleHq    = false;

    inline constexpr int   spaceType       = 0; // Cathedral
    inline constexpr float spaceMix        = 0.46f;
    inline constexpr float spaceDecay      = 0.68f;
    inline constexpr float spaceDamp       = 0.34f;
    inline constexpr float spaceShimmer    = 0.22f;
    inline constexpr bool  spaceFreeze     = false;
    inline constexpr bool  cc64Freeze      = false;

    inline constexpr float macroColossus   = 0.0f;
    inline constexpr float macroSpecter    = 0.0f;
    inline constexpr float macroRot        = 0.0f;
    inline constexpr float macroAbyss      = 0.0f;
    inline constexpr bool  legatoEnable    = false;
    inline constexpr float glideTime       = 0.14f;
    inline constexpr float expressionLevel = 0.78f;

    // V2 texture layer: 0 % mix = fully bypassed (no added samples, no latency)
    inline constexpr float v2TexMix        = 0.0f;
    inline constexpr int   v2TexAsset      = 0;
    inline constexpr float v2TexTune       = 0.0f;
    inline constexpr float v2TexKeytrack   = 1.0f;
    inline constexpr float v2TexAttack     = 0.012f;
    inline constexpr float v2TexRelease    = 0.9f;
    inline constexpr float v2TexVelSens    = 0.6f;
    inline constexpr float v2TexHighPass   = 20.0f;
    inline constexpr float v2TexLowPass    = 20000.0f;
    inline constexpr float v2TexWidth      = 1.0f;

    // V2 Stage 3: XY centred (0 offset), MPE off, linear velocity = bit-identical V1 behaviour.
    // Expression depths only act on V2-only sources (MPE per-note data, CC74, CC2), never on V1 inputs.
    inline constexpr float v2XyX           = 0.5f;
    inline constexpr float v2XyY           = 0.5f;
    inline constexpr int   v2XyMode        = 0; // Vowel Field
    inline constexpr bool  v2MpeEnable     = false;
    inline constexpr float v2MpePbRange    = 48.0f;
    inline constexpr int   v2VelCurve      = 0; // Linear (identity)
    inline constexpr float v2ExprVibrato   = 0.4f;
    inline constexpr float v2ExprEffort    = 0.5f;
    inline constexpr float v2ExprOpenness  = 0.5f;
    inline constexpr float v2ExprSwell     = 0.5f;
} // namespace defaults

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto normRange = [] (float minVal, float maxVal, float step = 0.0f, float skew = 1.0f)
    {
        return juce::NormalisableRange<float> (minVal, maxVal, step, skew);
    };

    // Field & Orbit
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { phonemeX, 1 }, "Phoneme X", normRange (0.0f, 1.0f), defaults::phonemeX));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { phonemeY, 1 }, "Phoneme Y", normRange (0.0f, 1.0f), defaults::phonemeY));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { orbitEnable, 1 }, "Orbit Enable", defaults::orbitEnable));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { orbitRate, 1 }, "Orbit Rate", normRange (0.02f, 4.0f, 0.0f, 0.4f), defaults::orbitRate));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { orbitDepth, 1 }, "Orbit Depth", normRange (0.0f, 1.0f), defaults::orbitDepth));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { orbitSync, 1 }, "Orbit Sync", defaults::orbitSync));

    // Ensemble & Register
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { registerMode, 1 }, "Vocal Register",
        juce::StringArray { "Basso Profundo", "Tenor", "Alto", "Soprano", "SATB Choir" },
        defaults::registerMode));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ensembleSize, 1 }, "Ensemble Size", normRange (1.0f, 24.0f, 1.0f), defaults::ensembleSize));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ensembleSpread, 1 }, "Swarm Spread", normRange (0.0f, 1.0f), defaults::ensembleSpread));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { vibratoDepth, 1 }, "Vibrato Depth", normRange (0.0f, 1.0f), defaults::vibratoDepth));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackTime, 1 }, "Attack", normRange (0.005f, 6.0f, 0.0f, 0.35f), defaults::attackTime));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseTime, 1 }, "Release", normRange (0.02f, 8.0f, 0.0f, 0.35f), defaults::releaseTime));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { onsetMode, 1 }, "Onset Mode",
        juce::StringArray { "Soft Breath (H)", "Closed Hum (M/N)", "Ghost Sibilant (Sh)" },
        defaults::onsetMode));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { onsetAmount, 1 }, "Onset Bite", normRange (0.0f, 1.0f), defaults::onsetAmount));

    // Tone, Formant & Layers
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { glottalTension, 1 }, "Tension / Effort", normRange (0.0f, 1.0f), defaults::glottalTension));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { breathAmount, 1 }, "Aspirated Breath", normRange (0.0f, 1.0f), defaults::breathAmount));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { throatShift, 1 }, "Throat Size", normRange (-12.0f, 12.0f), defaults::throatShift));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { formantFocus, 1 }, "Formant Focus", normRange (0.0f, 1.0f), defaults::formantFocus));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { subAbyss, 1 }, "Sub-Abyss", normRange (0.0f, 1.0f), defaults::subAbyss));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spectralCrown, 1 }, "Spectral Crown", normRange (0.0f, 1.0f), defaults::spectralCrown));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { masterGain, 1 }, "Output Gain", normRange (-18.0f, 6.0f), defaults::masterGain));

    // Entropy & Degradation
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveAmount, 1 }, "Analog Drive", normRange (0.0f, 1.0f), defaults::driveAmount));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tapeInstability, 1 }, "Tape Drift", normRange (0.0f, 1.0f), defaults::tapeInstability));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { oxideDamping, 1 }, "Oxide Damp", normRange (0.0f, 1.0f), defaults::oxideDamping));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { screwPitch, 1 }, "Screw Pitch", normRange (-12.0f, 0.0f), defaults::screwPitch));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { screwDepth, 1 }, "Screw Shadow", normRange (0.0f, 1.0f), defaults::screwDepth));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { oversampleHq, 1 }, "Drive 4x OS", defaults::oversampleHq));

    // Architectural Space + Freeze
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { spaceType, 1 }, "Space Model",
        juce::StringArray { "Cathedral", "Concrete Silo", "Obsidian Abyss", "Ghost Void" },
        defaults::spaceType));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spaceMix, 1 }, "Space Mix", normRange (0.0f, 1.0f), defaults::spaceMix));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spaceDecay, 1 }, "Decay", normRange (0.0f, 1.0f), defaults::spaceDecay));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spaceDamp, 1 }, "Damping", normRange (0.0f, 1.0f), defaults::spaceDamp));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { spaceShimmer, 1 }, "Dark Shimmer", normRange (0.0f, 1.0f), defaults::spaceShimmer));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { spaceFreeze, 1 }, "Spectral Freeze", defaults::spaceFreeze));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { cc64Freeze, 1 }, "CC64 Freeze", defaults::cc64Freeze));

    // Performance Dock & Master Macros
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { macroColossus, 1 }, "COLOSSUS", normRange (0.0f, 1.0f), defaults::macroColossus));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { macroSpecter, 1 }, "SPECTER", normRange (0.0f, 1.0f), defaults::macroSpecter));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { macroRot, 1 }, "ROT", normRange (0.0f, 1.0f), defaults::macroRot));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { macroAbyss, 1 }, "ABYSS", normRange (0.0f, 1.0f), defaults::macroAbyss));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { legatoEnable, 1 }, "Legato", defaults::legatoEnable));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { glideTime, 1 }, "Glide Time", normRange (0.01f, 1.5f, 0.0f, 0.4f), defaults::glideTime));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { expressionLevel, 1 }, "Expression", normRange (0.0f, 1.0f), defaults::expressionLevel));

    // ── V2 Layer A: Texture Engine (appended after every V1 ID; V1 indices untouched)
    {
        juce::StringArray assetNames;
        for (const auto& a : v2::kTextureAssets)
            assetNames.add (a.displayName);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexMix, 2 }, "Texture Mix", normRange (0.0f, 1.0f), defaults::v2TexMix));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { v2TexAsset, 2 }, "Texture Source", assetNames, defaults::v2TexAsset));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexTune, 2 }, "Texture Tune", normRange (-24.0f, 24.0f, 0.01f), defaults::v2TexTune));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexKeytrack, 2 }, "Texture Keytrack", normRange (0.0f, 1.0f), defaults::v2TexKeytrack));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexAttack, 2 }, "Texture Attack", normRange (0.001f, 4.0f, 0.0f, 0.3f), defaults::v2TexAttack));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexRelease, 2 }, "Texture Release", normRange (0.02f, 8.0f, 0.0f, 0.35f), defaults::v2TexRelease));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexVelSens, 2 }, "Texture Velocity", normRange (0.0f, 1.0f), defaults::v2TexVelSens));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexHighPass, 2 }, "Texture High-Pass", normRange (20.0f, 2000.0f, 0.0f, 0.3f), defaults::v2TexHighPass));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexLowPass, 2 }, "Texture Low-Pass", normRange (400.0f, 20000.0f, 0.0f, 0.3f), defaults::v2TexLowPass));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { v2TexWidth, 2 }, "Texture Width", normRange (0.0f, 2.0f), defaults::v2TexWidth));
    }

    // ── V2 Stage 3: control plane (appended after every Stage-2 ID)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { v2XyX, 2 }, "XY Sculptor X", normRange (0.0f, 1.0f), defaults::v2XyX));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { v2XyY, 2 }, "XY Sculptor Y", normRange (0.0f, 1.0f), defaults::v2XyY));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { v2XyMode, 2 }, "XY Sculptor Mode",
        juce::StringArray { "Vowel Field", "Light / Shadow", "Preset Axes" }, defaults::v2XyMode));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { v2MpeEnable, 2 }, "MPE", defaults::v2MpeEnable));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { v2MpePbRange, 2 }, "MPE Bend Range", normRange (0.0f, 48.0f, 1.0f), defaults::v2MpePbRange));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { v2VelCurve, 2 }, "Velocity Curve",
        juce::StringArray { "Linear", "Soft", "Hard", "S-Curve", "Fixed" }, defaults::v2VelCurve));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { v2ExprVibrato, 2 }, "Pressure > Vibrato", normRange (0.0f, 1.0f), defaults::v2ExprVibrato));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { v2ExprEffort, 2 }, "Pressure > Effort", normRange (0.0f, 1.0f), defaults::v2ExprEffort));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { v2ExprOpenness, 2 }, "Slide > Openness", normRange (0.0f, 1.0f), defaults::v2ExprOpenness));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { v2ExprSwell, 2 }, "Pressure > Texture Swell", normRange (0.0f, 1.0f), defaults::v2ExprSwell));

    return layout;
}

} // namespace monolith::params
