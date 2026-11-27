#include "PresetManager.h"
#include "../State/ParameterIDs.h"
#include <algorithm>
#include <array>

namespace monolith
{

namespace
{
    using namespace monolith::params;

    std::vector<PresetParamValue> buildDefaultsList()
    {
        return {
            { phonemeX,        defaults::phonemeX },
            { phonemeY,        defaults::phonemeY },
            { orbitEnable,     defaults::orbitEnable ? 1.0f : 0.0f },
            { orbitRate,       defaults::orbitRate },
            { orbitDepth,      defaults::orbitDepth },
            { orbitSync,       defaults::orbitSync ? 1.0f : 0.0f },
            { registerMode,    static_cast<float> (defaults::registerMode) },
            { ensembleSize,    defaults::ensembleSize },
            { ensembleSpread,  defaults::ensembleSpread },
            { vibratoDepth,    defaults::vibratoDepth },
            { attackTime,      defaults::attackTime },
            { releaseTime,     defaults::releaseTime },
            { onsetMode,       static_cast<float> (defaults::onsetMode) },
            { onsetAmount,     defaults::onsetAmount },
            { glottalTension,  defaults::glottalTension },
            { breathAmount,    defaults::breathAmount },
            { throatShift,     defaults::throatShift },
            { formantFocus,    defaults::formantFocus },
            { subAbyss,        defaults::subAbyss },
            { spectralCrown,   defaults::spectralCrown },
            { masterGain,      defaults::masterGain },
            { driveAmount,     defaults::driveAmount },
            { tapeInstability, defaults::tapeInstability },
            { oxideDamping,    defaults::oxideDamping },
            { screwPitch,      defaults::screwPitch },
            { screwDepth,      defaults::screwDepth },
            { oversampleHq,    defaults::oversampleHq ? 1.0f : 0.0f },
            { spaceType,       static_cast<float> (defaults::spaceType) },
            { spaceMix,        defaults::spaceMix },
            { spaceDecay,      defaults::spaceDecay },
            { spaceDamp,       defaults::spaceDamp },
            { spaceShimmer,    defaults::spaceShimmer },
            { spaceFreeze,     defaults::spaceFreeze ? 1.0f : 0.0f },
            { cc64Freeze,      defaults::cc64Freeze ? 1.0f : 0.0f },
            { macroColossus,   defaults::macroColossus },
            { macroSpecter,    defaults::macroSpecter },
            { macroRot,        defaults::macroRot },
            { macroAbyss,      defaults::macroAbyss },
            { legatoEnable,    defaults::legatoEnable ? 1.0f : 0.0f },
            { glideTime,       defaults::glideTime },
            { expressionLevel, defaults::expressionLevel }
        };
    }

    std::vector<FactoryPreset> createAllFactoryPresets()
    {
        std::vector<FactoryPreset> list;
        list.reserve (30);

        // ====================================================================
        // CATEGORY 1: Epic Cinematic — Bigger Than Life (8)
        // ====================================================================
        list.push_back ({
            "01. Monolith Awakening",
            "Epic Cinematic",
            juce::String::fromUTF8 ("Flagship signature choir wall — soaring SATB realism, wide sub-abyss, cathedral majesty."),
            {
                { phonemeX, 0.50f }, { phonemeY, 0.86f }, { registerMode, 4.0f },
                { ensembleSize, 20.0f }, { ensembleSpread, 0.68f }, { vibratoDepth, 0.36f },
                { attackTime, 0.38f }, { releaseTime, 1.50f }, { onsetMode, 0.0f }, { onsetAmount, 0.28f },
                { glottalTension, 0.68f }, { breathAmount, 0.20f }, { throatShift, 0.0f }, { formantFocus, 0.60f },
                { subAbyss, 0.42f }, { spectralCrown, 0.36f }, { driveAmount, 0.24f }, { tapeInstability, 0.14f },
                { oxideDamping, 0.12f }, { spaceType, 0.0f }, { spaceMix, 0.46f }, { spaceDecay, 0.68f },
                { spaceDamp, 0.34f }, { spaceShimmer, 0.22f }, { expressionLevel, 0.78f }, { masterGain, 0.0f }
            }
        });

        list.push_back ({
            "02. Arrakis Horizon",
            "Epic Cinematic",
            "Colossal throat-singing sub swarm meeting blazing cathedral sopranos across arid stone.",
            {
                { phonemeX, 0.34f }, { phonemeY, 0.76f }, { orbitEnable, 1.0f }, { orbitRate, 0.08f }, { orbitDepth, 0.24f },
                { registerMode, 4.0f }, { ensembleSize, 24.0f }, { ensembleSpread, 0.84f }, { vibratoDepth, 0.28f },
                { attackTime, 0.55f }, { releaseTime, 2.20f }, { onsetMode, 1.0f }, { onsetAmount, 0.35f },
                { glottalTension, 0.78f }, { breathAmount, 0.16f }, { throatShift, -2.5f }, { formantFocus, 0.72f },
                { subAbyss, 0.76f }, { spectralCrown, 0.52f }, { driveAmount, 0.34f }, { tapeInstability, 0.12f },
                { oxideDamping, 0.16f }, { spaceType, 0.0f }, { spaceMix, 0.52f }, { spaceDecay, 0.78f },
                { spaceDamp, 0.38f }, { spaceShimmer, 0.36f }, { macroColossus, 0.35f }, { expressionLevel, 0.84f },
                { masterGain, -3.5f }
            }
        });

        list.push_back ({
            "03. Throne of Ash",
            "Epic Cinematic",
            "High-tension brassy fortissimo choir engineered for decisive trailer climaxes.",
            {
                { phonemeX, 0.64f }, { phonemeY, 0.80f }, { registerMode, 4.0f },
                { ensembleSize, 22.0f }, { ensembleSpread, 0.74f }, { vibratoDepth, 0.42f },
                { attackTime, 0.16f }, { releaseTime, 1.15f }, { onsetMode, 0.0f }, { onsetAmount, 0.42f },
                { glottalTension, 0.94f }, { breathAmount, 0.12f }, { throatShift, -1.2f }, { formantFocus, 0.78f },
                { subAbyss, 0.54f }, { spectralCrown, 0.48f }, { driveAmount, 0.44f }, { tapeInstability, 0.08f },
                { oxideDamping, 0.08f }, { spaceType, 0.0f }, { spaceMix, 0.44f }, { spaceDecay, 0.62f },
                { spaceDamp, 0.28f }, { spaceShimmer, 0.20f }, { expressionLevel, 0.92f }, { masterGain, -4.5f }
            }
        });

        list.push_back ({
            "04. Titan Requiem",
            "Epic Cinematic",
            "Slow-swelling 24-voice tragic minor-key leviathan steeped in solemn liturgical resonance.",
            {
                { phonemeX, 0.20f }, { phonemeY, 0.68f }, { registerMode, 4.0f },
                { ensembleSize, 24.0f }, { ensembleSpread, 0.78f }, { vibratoDepth, 0.48f },
                { attackTime, 1.35f }, { releaseTime, 3.20f }, { onsetMode, 1.0f }, { onsetAmount, 0.30f },
                { glottalTension, 0.58f }, { breathAmount, 0.24f }, { throatShift, -1.5f }, { formantFocus, 0.58f },
                { subAbyss, 0.62f }, { spectralCrown, 0.28f }, { driveAmount, 0.20f }, { tapeInstability, 0.18f },
                { oxideDamping, 0.22f }, { spaceType, 2.0f }, { spaceMix, 0.58f }, { spaceDecay, 0.82f },
                { spaceDamp, 0.46f }, { spaceShimmer, 0.28f }, { expressionLevel, 0.76f }, { masterGain, -2.0f }
            }
        });

        list.push_back ({
            "05. Solaris Ascent",
            "Epic Cinematic",
            "Blindingly wide spectral crown + luminous infinite cathedral shimmer.",
            {
                { phonemeX, 0.74f }, { phonemeY, 0.72f }, { registerMode, 3.0f },
                { ensembleSize, 22.0f }, { ensembleSpread, 0.92f }, { vibratoDepth, 0.44f },
                { attackTime, 0.62f }, { releaseTime, 2.80f }, { onsetMode, 0.0f }, { onsetAmount, 0.22f },
                { glottalTension, 0.72f }, { breathAmount, 0.28f }, { throatShift, 1.8f }, { formantFocus, 0.66f },
                { subAbyss, 0.18f }, { spectralCrown, 0.88f }, { driveAmount, 0.14f }, { tapeInstability, 0.10f },
                { oxideDamping, 0.04f }, { spaceType, 0.0f }, { spaceMix, 0.60f }, { spaceDecay, 0.86f },
                { spaceDamp, 0.18f }, { spaceShimmer, 0.72f }, { expressionLevel, 0.84f }, { masterGain, 1.5f }
            }
        });

        list.push_back ({
            "06. Iron Liturgy",
            "Epic Cinematic",
            "Aggressive pre-summed saturated tenors and basses with sharp consonant bite.",
            {
                { phonemeX, 0.56f }, { phonemeY, 0.84f }, { registerMode, 1.0f },
                { ensembleSize, 18.0f }, { ensembleSpread, 0.58f }, { vibratoDepth, 0.22f },
                { attackTime, 0.08f }, { releaseTime, 0.95f }, { onsetMode, 2.0f }, { onsetAmount, 0.62f },
                { glottalTension, 0.90f }, { breathAmount, 0.14f }, { throatShift, -3.2f }, { formantFocus, 0.80f },
                { subAbyss, 0.52f }, { spectralCrown, 0.22f }, { driveAmount, 0.56f }, { tapeInstability, 0.10f },
                { oxideDamping, 0.14f }, { oversampleHq, 1.0f }, { spaceType, 1.0f }, { spaceMix, 0.36f },
                { spaceDecay, 0.52f }, { spaceDamp, 0.36f }, { spaceShimmer, 0.10f }, { expressionLevel, 0.88f },
                { masterGain, -4.5f }
            }
        });

        list.push_back ({
            "07. Last Light of Earth",
            "Epic Cinematic",
            "Emotional legato choir blooming organically from whispered stillness to full roar.",
            {
                { phonemeX, 0.38f }, { phonemeY, 0.66f }, { registerMode, 4.0f },
                { ensembleSize, 16.0f }, { ensembleSpread, 0.62f }, { vibratoDepth, 0.52f },
                { attackTime, 0.45f }, { releaseTime, 2.10f }, { onsetMode, 0.0f }, { onsetAmount, 0.25f },
                { glottalTension, 0.54f }, { breathAmount, 0.34f }, { throatShift, 0.5f }, { formantFocus, 0.54f },
                { subAbyss, 0.34f }, { spectralCrown, 0.44f }, { driveAmount, 0.18f }, { tapeInstability, 0.16f },
                { oxideDamping, 0.15f }, { spaceType, 0.0f }, { spaceMix, 0.54f }, { spaceDecay, 0.74f },
                { spaceDamp, 0.32f }, { spaceShimmer, 0.34f }, { legatoEnable, 1.0f }, { glideTime, 0.22f },
                { expressionLevel, 0.68f }, { masterGain, 0.5f }
            }
        });

        list.push_back ({
            "08. Godhead Cathedral",
            "Epic Cinematic",
            "Towering cavernous reverb wall with crystal-clear open vowel midrange.",
            {
                { phonemeX, 0.48f }, { phonemeY, 0.88f }, { registerMode, 4.0f },
                { ensembleSize, 24.0f }, { ensembleSpread, 0.86f }, { vibratoDepth, 0.34f },
                { attackTime, 0.48f }, { releaseTime, 2.60f }, { onsetMode, 0.0f }, { onsetAmount, 0.18f },
                { glottalTension, 0.74f }, { breathAmount, 0.18f }, { throatShift, 0.0f }, { formantFocus, 0.70f },
                { subAbyss, 0.48f }, { spectralCrown, 0.58f }, { driveAmount, 0.22f }, { tapeInstability, 0.09f },
                { oxideDamping, 0.08f }, { spaceType, 0.0f }, { spaceMix, 0.64f }, { spaceDecay, 0.88f },
                { spaceDamp, 0.24f }, { spaceShimmer, 0.48f }, { macroColossus, 0.28f }, { macroAbyss, 0.24f },
                { expressionLevel, 0.82f }, { masterGain, -1.5f }
            }
        });

        // ====================================================================
        // CATEGORY 2: Lorn & Industrial Dystopia (7)
        // ====================================================================
        list.push_back ({
            "09. Acid Rain Vault",
            "Lorn & Dystopia",
            "Saturated slow-drifting tape choir echoing inside a rain-slick concrete silo.",
            {
                { phonemeX, 0.16f }, { phonemeY, 0.46f }, { registerMode, 1.0f },
                { ensembleSize, 14.0f }, { ensembleSpread, 0.56f }, { vibratoDepth, 0.16f },
                { attackTime, 0.42f }, { releaseTime, 1.85f }, { onsetMode, 1.0f }, { onsetAmount, 0.26f },
                { glottalTension, 0.62f }, { breathAmount, 0.22f }, { throatShift, -3.5f }, { formantFocus, 0.56f },
                { subAbyss, 0.68f }, { spectralCrown, 0.08f }, { driveAmount, 0.52f }, { tapeInstability, 0.54f },
                { oxideDamping, 0.44f }, { spaceType, 1.0f }, { spaceMix, 0.50f }, { spaceDecay, 0.64f },
                { spaceDamp, 0.58f }, { spaceShimmer, 0.24f }, { expressionLevel, 0.78f }, { masterGain, -6.0f }
            }
        });

        list.push_back ({
            "10. Subterranean Hum",
            "Lorn & Dystopia",
            "Chest-rattling Basso Profundo closed-mouth hum + asymmetric analog grit.",
            {
                { phonemeX, 0.28f }, { phonemeY, 0.14f }, { registerMode, 0.0f },
                { ensembleSize, 16.0f }, { ensembleSpread, 0.48f }, { vibratoDepth, 0.14f },
                { attackTime, 0.30f }, { releaseTime, 1.40f }, { onsetMode, 1.0f }, { onsetAmount, 0.45f },
                { glottalTension, 0.74f }, { breathAmount, 0.08f }, { throatShift, -4.8f }, { formantFocus, 0.66f },
                { subAbyss, 0.92f }, { spectralCrown, 0.00f }, { driveAmount, 0.60f }, { tapeInstability, 0.32f },
                { oxideDamping, 0.52f }, { spaceType, 1.0f }, { spaceMix, 0.38f }, { spaceDecay, 0.58f },
                { spaceDamp, 0.64f }, { spaceShimmer, 0.15f }, { expressionLevel, 0.82f }, { masterGain, -6.5f }
            }
        });

        list.push_back ({
            "11. Broken Cassette Angel",
            "Lorn & Dystopia",
            "Melancholic alto/soprano choir melting in heavy magnetic wow, flutter, and oxide.",
            {
                { phonemeX, 0.24f }, { phonemeY, 0.62f }, { registerMode, 2.0f },
                { ensembleSize, 12.0f }, { ensembleSpread, 0.60f }, { vibratoDepth, 0.30f },
                { attackTime, 0.52f }, { releaseTime, 2.10f }, { onsetMode, 0.0f }, { onsetAmount, 0.18f },
                { glottalTension, 0.42f }, { breathAmount, 0.32f }, { throatShift, -1.0f }, { formantFocus, 0.48f },
                { subAbyss, 0.38f }, { spectralCrown, 0.22f }, { driveAmount, 0.46f }, { tapeInstability, 0.86f },
                { oxideDamping, 0.66f }, { spaceType, 2.0f }, { spaceMix, 0.52f }, { spaceDecay, 0.70f },
                { spaceDamp, 0.62f }, { spaceShimmer, 0.30f }, { macroRot, 0.25f }, { expressionLevel, 0.74f },
                { masterGain, -1.0f }
            }
        });

        list.push_back ({
            "12. Brutalist Concrete",
            "Lorn & Dystopia",
            "Stark short architectural reflections + heavy pre-summed tube drive.",
            {
                { phonemeX, 0.52f }, { phonemeY, 0.74f }, { registerMode, 4.0f },
                { ensembleSize, 16.0f }, { ensembleSpread, 0.52f }, { vibratoDepth, 0.10f },
                { attackTime, 0.04f }, { releaseTime, 0.75f }, { onsetMode, 2.0f }, { onsetAmount, 0.54f },
                { glottalTension, 0.86f }, { breathAmount, 0.14f }, { throatShift, -2.2f }, { formantFocus, 0.76f },
                { subAbyss, 0.60f }, { spectralCrown, 0.12f }, { driveAmount, 0.68f }, { tapeInstability, 0.20f },
                { oxideDamping, 0.30f }, { oversampleHq, 1.0f }, { spaceType, 1.0f }, { spaceMix, 0.34f },
                { spaceDecay, 0.32f }, { spaceDamp, 0.42f }, { spaceShimmer, 0.05f }, { expressionLevel, 0.85f },
                { masterGain, -3.5f }
            }
        });

        list.push_back ({
            "13. Rust & Marrow",
            "Lorn & Dystopia",
            "Dark formant-shifted tenor drone with dense sub-harmonic chest weight.",
            {
                { phonemeX, 0.18f }, { phonemeY, 0.56f }, { orbitEnable, 1.0f }, { orbitRate, 0.11f }, { orbitDepth, 0.18f },
                { registerMode, 1.0f }, { ensembleSize, 18.0f }, { ensembleSpread, 0.64f }, { vibratoDepth, 0.18f },
                { attackTime, 0.68f }, { releaseTime, 2.40f }, { onsetMode, 1.0f }, { onsetAmount, 0.32f },
                { glottalTension, 0.70f }, { breathAmount, 0.18f }, { throatShift, -6.2f }, { formantFocus, 0.62f },
                { subAbyss, 0.82f }, { spectralCrown, 0.04f }, { driveAmount, 0.58f }, { tapeInstability, 0.42f },
                { oxideDamping, 0.48f }, { spaceType, 2.0f }, { spaceMix, 0.48f }, { spaceDecay, 0.74f },
                { spaceDamp, 0.66f }, { spaceShimmer, 0.22f }, { expressionLevel, 0.80f }, { masterGain, -5.0f }
            }
        });

        list.push_back ({
            "14. Anvil Ghost",
            "Lorn & Dystopia",
            "Hollow 'Oo' vowel drift floating above a distorted sub-abyss floor.",
            {
                { phonemeX, 0.12f }, { phonemeY, 0.36f }, { orbitEnable, 1.0f }, { orbitRate, 0.14f }, { orbitDepth, 0.26f },
                { registerMode, 4.0f }, { ensembleSize, 14.0f }, { ensembleSpread, 0.72f }, { vibratoDepth, 0.26f },
                { attackTime, 0.58f }, { releaseTime, 2.25f }, { onsetMode, 0.0f }, { onsetAmount, 0.20f },
                { glottalTension, 0.48f }, { breathAmount, 0.30f }, { throatShift, -3.0f }, { formantFocus, 0.52f },
                { subAbyss, 0.74f }, { spectralCrown, 0.16f }, { driveAmount, 0.50f }, { tapeInstability, 0.48f },
                { oxideDamping, 0.40f }, { spaceType, 2.0f }, { spaceMix, 0.56f }, { spaceDecay, 0.78f },
                { spaceDamp, 0.56f }, { spaceShimmer, 0.32f }, { expressionLevel, 0.76f }, { masterGain, -5.0f }
            }
        });

        list.push_back ({
            "15. Static Horizon",
            "Lorn & Dystopia",
            "Oxide-damped lo-fi choral pad trembling with micro-pitch tape entropy.",
            {
                { phonemeX, 0.44f }, { phonemeY, 0.52f }, { registerMode, 4.0f },
                { ensembleSize, 20.0f }, { ensembleSpread, 0.70f }, { vibratoDepth, 0.20f },
                { attackTime, 0.85f }, { releaseTime, 2.60f }, { onsetMode, 0.0f }, { onsetAmount, 0.16f },
                { glottalTension, 0.50f }, { breathAmount, 0.34f }, { throatShift, -2.0f }, { formantFocus, 0.44f },
                { subAbyss, 0.50f }, { spectralCrown, 0.10f }, { driveAmount, 0.44f }, { tapeInstability, 0.74f },
                { oxideDamping, 0.74f }, { spaceType, 1.0f }, { spaceMix, 0.54f }, { spaceDecay, 0.72f },
                { spaceDamp, 0.70f }, { spaceShimmer, 0.18f }, { expressionLevel, 0.74f }, { masterGain, -5.5f }
            }
        });

        // ====================================================================
        // CATEGORY 3: Witch House & Darkwave (7)
        // ====================================================================
        list.push_back ({
            "16. Screwed Coven",
            "Witch House & Darkwave",
            "Signature pitched-down, formant-crushed chopped-and-screwed witch house choir.",
            {
                { phonemeX, 0.22f }, { phonemeY, 0.54f }, { registerMode, 4.0f },
                { ensembleSize, 16.0f }, { ensembleSpread, 0.76f }, { vibratoDepth, 0.28f },
                { attackTime, 0.22f }, { releaseTime, 1.65f }, { onsetMode, 2.0f }, { onsetAmount, 0.38f },
                { glottalTension, 0.66f }, { breathAmount, 0.26f }, { throatShift, -4.0f }, { formantFocus, 0.62f },
                { subAbyss, 0.68f }, { spectralCrown, 0.24f }, { driveAmount, 0.48f }, { tapeInstability, 0.44f },
                { oxideDamping, 0.36f }, { screwPitch, -5.0f }, { screwDepth, 0.68f },
                { spaceType, 3.0f }, { spaceMix, 0.54f }, { spaceDecay, 0.74f }, { spaceDamp, 0.44f },
                { spaceShimmer, 0.42f }, { expressionLevel, 0.80f }, { masterGain, -5.5f }
            }
        });

        list.push_back ({
            "17. Velvet Crypt",
            "Witch House & Darkwave",
            "Lush detuned ghostly synth-choir drenched in dark modulated abyss.",
            {
                { phonemeX, 0.16f }, { phonemeY, 0.42f }, { registerMode, 2.0f },
                { ensembleSize, 20.0f }, { ensembleSpread, 0.86f }, { vibratoDepth, 0.40f },
                { attackTime, 0.46f }, { releaseTime, 2.30f }, { onsetMode, 0.0f }, { onsetAmount, 0.22f },
                { glottalTension, 0.52f }, { breathAmount, 0.30f }, { throatShift, -2.0f }, { formantFocus, 0.50f },
                { subAbyss, 0.56f }, { spectralCrown, 0.40f }, { driveAmount, 0.32f }, { tapeInstability, 0.38f },
                { oxideDamping, 0.28f }, { screwPitch, 0.0f }, { screwDepth, 0.36f },
                { spaceType, 2.0f }, { spaceMix, 0.62f }, { spaceDecay, 0.82f }, { spaceDamp, 0.48f },
                { spaceShimmer, 0.54f }, { expressionLevel, 0.78f }, { masterGain, -6.0f }
            }
        });

        list.push_back ({
            "18. Hex Whisper",
            "Witch House & Darkwave",
            "Breath-dominant eerie sibilant chant trailing into a shimmering ghost void.",
            {
                { phonemeX, 0.70f }, { phonemeY, 0.14f }, { registerMode, 4.0f },
                { ensembleSize, 18.0f }, { ensembleSpread, 0.88f }, { vibratoDepth, 0.24f },
                { attackTime, 0.34f }, { releaseTime, 2.40f }, { onsetMode, 2.0f }, { onsetAmount, 0.74f },
                { glottalTension, 0.24f }, { breathAmount, 0.88f }, { throatShift, 1.5f }, { formantFocus, 0.64f },
                { subAbyss, 0.22f }, { spectralCrown, 0.48f }, { driveAmount, 0.28f }, { tapeInstability, 0.36f },
                { oxideDamping, 0.22f }, { screwPitch, -2.0f }, { screwDepth, 0.28f },
                { spaceType, 3.0f }, { spaceMix, 0.66f }, { spaceDecay, 0.90f }, { spaceDamp, 0.30f },
                { spaceShimmer, 0.66f }, { macroSpecter, 0.35f }, { expressionLevel, 0.84f }, { masterGain, 2.5f }
            }
        });

        list.push_back ({
            "19. Neon Mausoleum",
            "Witch House & Darkwave",
            "Darkwave-ready crisp chorus-smeared soprano/alto lead pad with legato glide.",
            {
                { phonemeX, 0.76f }, { phonemeY, 0.58f }, { registerMode, 3.0f },
                { ensembleSize, 12.0f }, { ensembleSpread, 0.72f }, { vibratoDepth, 0.34f },
                { attackTime, 0.12f }, { releaseTime, 1.35f }, { onsetMode, 0.0f }, { onsetAmount, 0.30f },
                { glottalTension, 0.76f }, { breathAmount, 0.22f }, { throatShift, 0.8f }, { formantFocus, 0.68f },
                { subAbyss, 0.34f }, { spectralCrown, 0.56f }, { driveAmount, 0.38f }, { tapeInstability, 0.42f },
                { oxideDamping, 0.18f }, { spaceType, 3.0f }, { spaceMix, 0.48f }, { spaceDecay, 0.68f },
                { spaceDamp, 0.28f }, { spaceShimmer, 0.44f }, { legatoEnable, 1.0f }, { glideTime, 0.16f },
                { expressionLevel, 0.82f }, { masterGain, 1.5f }
            }
        });

        list.push_back ({
            "20. Black Fog Swell",
            "Witch House & Darkwave",
            "Slow-attack cavernous shadow choir steeped in heavy magnetic tape rot.",
            {
                { phonemeX, 0.16f }, { phonemeY, 0.34f }, { registerMode, 4.0f },
                { ensembleSize, 22.0f }, { ensembleSpread, 0.80f }, { vibratoDepth, 0.32f },
                { attackTime, 1.65f }, { releaseTime, 3.40f }, { onsetMode, 1.0f }, { onsetAmount, 0.24f },
                { glottalTension, 0.46f }, { breathAmount, 0.36f }, { throatShift, -5.0f }, { formantFocus, 0.46f },
                { subAbyss, 0.72f }, { spectralCrown, 0.18f }, { driveAmount, 0.54f }, { tapeInstability, 0.72f },
                { oxideDamping, 0.56f }, { screwPitch, -4.0f }, { screwDepth, 0.52f },
                { spaceType, 2.0f }, { spaceMix, 0.64f }, { spaceDecay, 0.86f }, { spaceDamp, 0.60f },
                { spaceShimmer, 0.38f }, { macroRot, 0.30f }, { macroAbyss, 0.25f }, { expressionLevel, 0.82f },
                { masterGain, -5.0f }
            }
        });

        list.push_back ({
            "21. Midnight Ritual",
            "Witch House & Darkwave",
            "Hypnotic auto-orbiting vowel chant riding a deep sub-octave shadow.",
            {
                { phonemeX, 0.46f }, { phonemeY, 0.54f }, { orbitEnable, 1.0f }, { orbitRate, 0.36f }, { orbitDepth, 0.68f },
                { registerMode, 4.0f }, { ensembleSize, 18.0f }, { ensembleSpread, 0.74f }, { vibratoDepth, 0.26f },
                { attackTime, 0.28f }, { releaseTime, 1.80f }, { onsetMode, 1.0f }, { onsetAmount, 0.40f },
                { glottalTension, 0.64f }, { breathAmount, 0.28f }, { throatShift, -3.0f }, { formantFocus, 0.66f },
                { subAbyss, 0.78f }, { spectralCrown, 0.26f }, { driveAmount, 0.44f }, { tapeInstability, 0.34f },
                { oxideDamping, 0.32f }, { screwPitch, -3.0f }, { screwDepth, 0.40f },
                { spaceType, 3.0f }, { spaceMix, 0.52f }, { spaceDecay, 0.74f }, { spaceDamp, 0.42f },
                { spaceShimmer, 0.44f }, { expressionLevel, 0.80f }, { masterGain, -5.0f }
            }
        });

        list.push_back ({
            "22. Bleeding Halo",
            "Witch House & Darkwave",
            "Haunted high shimmer bleeding down into saturated low-mid darkness.",
            {
                { phonemeX, 0.82f }, { phonemeY, 0.44f }, { registerMode, 4.0f },
                { ensembleSize, 20.0f }, { ensembleSpread, 0.84f }, { vibratoDepth, 0.38f },
                { attackTime, 0.44f }, { releaseTime, 2.20f }, { onsetMode, 2.0f }, { onsetAmount, 0.34f },
                { glottalTension, 0.62f }, { breathAmount, 0.32f }, { throatShift, -1.5f }, { formantFocus, 0.60f },
                { subAbyss, 0.58f }, { spectralCrown, 0.74f }, { driveAmount, 0.48f }, { tapeInstability, 0.52f },
                { oxideDamping, 0.34f }, { screwDepth, 0.32f },
                { spaceType, 3.0f }, { spaceMix, 0.60f }, { spaceDecay, 0.82f }, { spaceDamp, 0.38f },
                { spaceShimmer, 0.74f }, { expressionLevel, 0.80f }, { masterGain, -5.0f }
            }
        });

        // ====================================================================
        // CATEGORY 4: Organic & Intimate Acoustic (4)
        // ====================================================================
        list.push_back ({
            "23. Pure Chamber Choir",
            "Organic Acoustic",
            "Clean, balanced, ultra-playable SATB chamber realism showcase.",
            {
                { phonemeX, 0.48f }, { phonemeY, 0.82f }, { registerMode, 4.0f },
                { ensembleSize, 14.0f }, { ensembleSpread, 0.56f }, { vibratoDepth, 0.40f },
                { attackTime, 0.24f }, { releaseTime, 1.15f }, { onsetMode, 0.0f }, { onsetAmount, 0.22f },
                { glottalTension, 0.52f }, { breathAmount, 0.18f }, { throatShift, 0.0f }, { formantFocus, 0.64f },
                { subAbyss, 0.00f }, { spectralCrown, 0.10f }, { driveAmount, 0.04f }, { tapeInstability, 0.02f },
                { oxideDamping, 0.04f }, { spaceType, 0.0f }, { spaceMix, 0.34f }, { spaceDecay, 0.52f },
                { spaceDamp, 0.38f }, { spaceShimmer, 0.04f }, { expressionLevel, 0.76f }, { masterGain, 4.5f }
            }
        });

        list.push_back ({
            "24. Candlelit Vespers",
            "Organic Acoustic",
            "Soft, breathy intimate ensemble with natural entrance stagger and warm 'Oh' vowel.",
            {
                { phonemeX, 0.20f }, { phonemeY, 0.70f }, { registerMode, 4.0f },
                { ensembleSize, 12.0f }, { ensembleSpread, 0.62f }, { vibratoDepth, 0.32f },
                { attackTime, 0.46f }, { releaseTime, 1.50f }, { onsetMode, 0.0f }, { onsetAmount, 0.34f },
                { glottalTension, 0.34f }, { breathAmount, 0.38f }, { throatShift, 0.4f }, { formantFocus, 0.54f },
                { subAbyss, 0.00f }, { spectralCrown, 0.08f }, { driveAmount, 0.02f }, { tapeInstability, 0.03f },
                { oxideDamping, 0.10f }, { spaceType, 0.0f }, { spaceMix, 0.42f }, { spaceDecay, 0.60f },
                { spaceDamp, 0.44f }, { spaceShimmer, 0.05f }, { expressionLevel, 0.72f }, { masterGain, 2.0f }
            }
        });

        list.push_back ({
            "25. Solo Baritone Shadow",
            "Organic Acoustic",
            "Expressive single-voice legato vocal with realistic vibrato and subtle chest depth.",
            {
                { phonemeX, 0.26f }, { phonemeY, 0.74f }, { registerMode, 0.0f },
                { ensembleSize, 1.0f }, { ensembleSpread, 0.12f }, { vibratoDepth, 0.56f },
                { attackTime, 0.14f }, { releaseTime, 0.85f }, { onsetMode, 0.0f }, { onsetAmount, 0.26f },
                { glottalTension, 0.60f }, { breathAmount, 0.22f }, { throatShift, -1.2f }, { formantFocus, 0.68f },
                { subAbyss, 0.16f }, { spectralCrown, 0.00f }, { driveAmount, 0.08f }, { tapeInstability, 0.02f },
                { oxideDamping, 0.08f }, { spaceType, 1.0f }, { spaceMix, 0.32f }, { spaceDecay, 0.46f },
                { spaceDamp, 0.42f }, { spaceShimmer, 0.00f }, { legatoEnable, 1.0f }, { glideTime, 0.14f },
                { expressionLevel, 0.82f }, { masterGain, -6.5f }
            }
        });

        list.push_back ({
            "26. Frozen Soprano",
            "Organic Acoustic",
            "Crystalline, pure high soprano register tailored for delicate emotional melodies.",
            {
                { phonemeX, 0.62f }, { phonemeY, 0.68f }, { registerMode, 3.0f },
                { ensembleSize, 6.0f }, { ensembleSpread, 0.42f }, { vibratoDepth, 0.48f },
                { attackTime, 0.28f }, { releaseTime, 1.90f }, { onsetMode, 0.0f }, { onsetAmount, 0.20f },
                { glottalTension, 0.48f }, { breathAmount, 0.26f }, { throatShift, 1.0f }, { formantFocus, 0.66f },
                { subAbyss, 0.00f }, { spectralCrown, 0.46f }, { driveAmount, 0.02f }, { tapeInstability, 0.04f },
                { oxideDamping, 0.02f }, { spaceType, 0.0f }, { spaceMix, 0.56f }, { spaceDecay, 0.88f },
                { spaceDamp, 0.20f }, { spaceShimmer, 0.54f }, { legatoEnable, 1.0f }, { glideTime, 0.18f },
                { expressionLevel, 0.78f }, { masterGain, 6.0f }
            }
        });

        // ====================================================================
        // CATEGORY 5: Evolving & Horror Soundscapes (4)
        // ====================================================================
        list.push_back ({
            "27. The Sunken Nave",
            "Evolving & Horror Soundscapes",
            "Self-morphing 2D vowel orbit drifting slowly through dark cavernous space.",
            {
                { phonemeX, 0.36f }, { phonemeY, 0.48f }, { orbitEnable, 1.0f }, { orbitRate, 0.14f }, { orbitDepth, 0.82f },
                { registerMode, 4.0f }, { ensembleSize, 20.0f }, { ensembleSpread, 0.82f }, { vibratoDepth, 0.34f },
                { attackTime, 1.20f }, { releaseTime, 3.50f }, { onsetMode, 1.0f }, { onsetAmount, 0.26f },
                { glottalTension, 0.54f }, { breathAmount, 0.36f }, { throatShift, -3.5f }, { formantFocus, 0.58f },
                { subAbyss, 0.66f }, { spectralCrown, 0.34f }, { driveAmount, 0.34f }, { tapeInstability, 0.44f },
                { oxideDamping, 0.32f }, { spaceType, 2.0f }, { spaceMix, 0.68f }, { spaceDecay, 0.90f },
                { spaceDamp, 0.48f }, { spaceShimmer, 0.48f }, { expressionLevel, 0.78f }, { masterGain, -3.5f }
            }
        });

        list.push_back ({
            "28. Lung of the Void",
            "Evolving & Horror Soundscapes",
            "Deep rhythmic breath pressure + shifting sub-formant inhalation.",
            {
                { phonemeX, 0.58f }, { phonemeY, 0.16f }, { orbitEnable, 1.0f }, { orbitRate, 0.48f }, { orbitDepth, 0.74f },
                { registerMode, 0.0f }, { ensembleSize, 22.0f }, { ensembleSpread, 0.88f }, { vibratoDepth, 0.18f },
                { attackTime, 0.85f }, { releaseTime, 2.80f }, { onsetMode, 2.0f }, { onsetAmount, 0.68f },
                { glottalTension, 0.30f }, { breathAmount, 0.92f }, { throatShift, -6.5f }, { formantFocus, 0.62f },
                { subAbyss, 0.74f }, { spectralCrown, 0.22f }, { driveAmount, 0.42f }, { tapeInstability, 0.50f },
                { oxideDamping, 0.44f }, { spaceType, 2.0f }, { spaceMix, 0.64f }, { spaceDecay, 0.84f },
                { spaceDamp, 0.50f }, { spaceShimmer, 0.38f }, { expressionLevel, 0.86f }, { masterGain, -5.5f }
            }
        });

        list.push_back ({
            "29. Glass Ossuary",
            "Evolving & Horror Soundscapes",
            "Eerie crystalline high harmonics with unstable pitch wandering and ghost shimmer.",
            {
                { phonemeX, 0.84f }, { phonemeY, 0.38f }, { orbitEnable, 1.0f }, { orbitRate, 0.22f }, { orbitDepth, 0.46f },
                { registerMode, 3.0f }, { ensembleSize, 16.0f }, { ensembleSpread, 0.94f }, { vibratoDepth, 0.58f },
                { attackTime, 0.75f }, { releaseTime, 3.20f }, { onsetMode, 2.0f }, { onsetAmount, 0.48f },
                { glottalTension, 0.44f }, { breathAmount, 0.46f }, { throatShift, 4.5f }, { formantFocus, 0.82f },
                { subAbyss, 0.12f }, { spectralCrown, 0.84f }, { driveAmount, 0.20f }, { tapeInstability, 0.68f },
                { oxideDamping, 0.12f }, { spaceType, 3.0f }, { spaceMix, 0.72f }, { spaceDecay, 0.94f },
                { spaceDamp, 0.18f }, { spaceShimmer, 0.84f }, { expressionLevel, 0.80f }, { masterGain, 0.5f }
            }
        });

        list.push_back ({
            "30. Event Horizon",
            "Evolving & Horror Soundscapes",
            "Massive slow-motion dread swell engaging all four signature macros simultaneously.",
            {
                { phonemeX, 0.24f }, { phonemeY, 0.50f }, { orbitEnable, 1.0f }, { orbitRate, 0.09f }, { orbitDepth, 0.55f },
                { registerMode, 4.0f }, { ensembleSize, 20.0f }, { ensembleSpread, 0.72f }, { vibratoDepth, 0.36f },
                { attackTime, 1.85f }, { releaseTime, 4.20f }, { onsetMode, 1.0f }, { onsetAmount, 0.32f },
                { glottalTension, 0.66f }, { breathAmount, 0.28f }, { throatShift, -3.5f }, { formantFocus, 0.60f },
                { subAbyss, 0.62f }, { spectralCrown, 0.48f }, { driveAmount, 0.32f }, { tapeInstability, 0.34f },
                { oxideDamping, 0.24f }, { screwPitch, -2.0f }, { screwDepth, 0.35f },
                { spaceType, 2.0f }, { spaceMix, 0.56f }, { spaceDecay, 0.80f }, { spaceDamp, 0.40f },
                { spaceShimmer, 0.44f }, { macroColossus, 0.52f }, { macroSpecter, 0.32f },
                { macroRot, 0.28f }, { macroAbyss, 0.58f }, { expressionLevel, 0.84f }, { masterGain, -4.0f }
            }
        });

        return list;
    }
} // namespace

PresetManager::PresetManager()
    : presets (createAllFactoryPresets())
{
}

const FactoryPreset& PresetManager::getPreset (int index) const
{
    const int safeIdx = std::clamp (index, 0, static_cast<int> (presets.size()) - 1);
    return presets[static_cast<size_t> (safeIdx)];
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    cats.add ("All");
    for (const auto& p : presets)
    {
        if (! cats.contains (p.category))
            cats.add (p.category);
    }
    return cats;
}

std::vector<int> PresetManager::getPresetIndicesForCategory (const juce::String& category) const
{
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int> (presets.size()); ++i)
    {
        if (category == "All" || presets[static_cast<size_t> (i)].category == category)
            indices.push_back (i);
    }
    return indices;
}

void PresetManager::setCurrentPresetIndex (int idx) noexcept
{
    currentIndex = std::clamp (idx, 0, static_cast<int> (presets.size()) - 1);
}

void PresetManager::applyPreset (int index, juce::AudioProcessorValueTreeState& apvts)
{
    currentIndex = std::clamp (index, 0, static_cast<int> (presets.size()) - 1);
    const auto& preset = presets[static_cast<size_t> (currentIndex)];

    auto setParamValue = [&apvts] (const char* id, float val)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const float norm = p->convertTo0to1 (val);
            p->setValueNotifyingHost (std::clamp (norm, 0.0f, 1.0f));
        }
    };

    // First restore clean defaults so every preset loads deterministically
    for (const auto& def : buildDefaultsList())
        setParamValue (def.id, def.value);

    for (const auto& kv : preset.params)
        setParamValue (kv.id, kv.value);
}

void PresetManager::initializeAB (juce::AudioProcessorValueTreeState& apvts)
{
    slotA = apvts.copyState();
    slotB = apvts.copyState();
    activeIsB = false;
}

void PresetManager::toggleAB (juce::AudioProcessorValueTreeState& apvts)
{
    if (! slotA.isValid() || ! slotB.isValid())
        initializeAB (apvts);

    if (activeIsB)
    {
        slotB = apvts.copyState();
        apvts.replaceState (slotA);
        activeIsB = false;
    }
    else
    {
        slotA = apvts.copyState();
        apvts.replaceState (slotB);
        activeIsB = true;
    }
}

void PresetManager::copyActiveToInactive (juce::AudioProcessorValueTreeState& apvts)
{
    auto snapshot = apvts.copyState();
    if (activeIsB)
        slotA = std::move (snapshot);
    else
        slotB = std::move (snapshot);
}

void PresetManager::saveStateToXml (juce::XmlElement& parent) const
{
    parent.setAttribute ("currentPresetIndex", currentIndex);
    parent.setAttribute ("activeSlotIsB", activeIsB);

    if (slotA.isValid())
    {
        if (auto xmlA = slotA.createXml())
        {
            auto* wrapper = parent.createNewChildElement ("SlotA");
            wrapper->addChildElement (xmlA.release());
        }
    }
    if (slotB.isValid())
    {
        if (auto xmlB = slotB.createXml())
        {
            auto* wrapper = parent.createNewChildElement ("SlotB");
            wrapper->addChildElement (xmlB.release());
        }
    }
}

void PresetManager::loadStateFromXml (const juce::XmlElement& parent)
{
    currentIndex = std::clamp (parent.getIntAttribute ("currentPresetIndex", 0),
                               0,
                               static_cast<int> (presets.size()) - 1);
    activeIsB = parent.getBoolAttribute ("activeSlotIsB", false);

    if (auto* wrapperA = parent.getChildByName ("SlotA"))
        if (auto* inner = wrapperA->getFirstChildElement())
            slotA = juce::ValueTree::fromXml (*inner);

    if (auto* wrapperB = parent.getChildByName ("SlotB"))
        if (auto* inner = wrapperB->getFirstChildElement())
            slotB = juce::ValueTree::fromXml (*inner);
}

} // namespace monolith
