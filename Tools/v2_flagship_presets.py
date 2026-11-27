#!/usr/bin/env python3
"""
MONOLITH II - Stage 5 flagship preset generator (source of truth for the 20 NAMM 2027 presets).

Writes assets/v2_presets/NN_Name.mnlpreset in the V2 preset schema (<MonolithPreset version="2">) that
Source/Presets/PresetLibrary.cpp parses. CMake embeds these files with juce_add_binary_data.

Every file stores the FULL parameter state (V2 defaults parsed from Source/State/ParameterIDs.h, then the
preset's overrides), so a preset never shifts if a default changes later. Global controller settings
(MPE enable / bend range / velocity curve) are never written. Each preset also carries:
  - 4 named macros (V1 legacy curve on/off + up to 8 routes each)
  - Mod Wheel (CC1) and Pressure (channel / poly / MPE) routes, optional XY "Preset Axes" routes
  - per-preset MPE depths (v2_expr_*) and texture-layer settings

Usage:  python3 Tools/v2_flagship_presets.py [--check]
        --check: validate only (ids, ranges, route counts, labels); write nothing.
"""

import re
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parent.parent
PARAM_HEADER = ROOT / "Source/State/ParameterIDs.h"
OUT_DIR = ROOT / "assets/v2_presets"
AUTHOR = "Monolith Audio"

GLOBAL_IDS = {"v2_mpe_enable", "v2_mpe_pb_range", "v2_vel_curve"}

# Route destinations accepted by Source/V2/ModMatrix.h kModDestinations
MOD_DESTS = {
    "phoneme_x", "phoneme_y", "orbit_rate", "orbit_depth", "ensemble_size", "ensemble_spread", "vibrato_depth",
    "onset_amount", "glottal_tension", "breath_amount", "throat_shift", "formant_focus", "sub_abyss",
    "spectral_crown", "master_gain", "drive_amount", "tape_instability", "oxide_damping", "screw_pitch",
    "screw_depth", "space_mix", "space_decay", "space_damp", "space_shimmer", "v2_tex_mix", "v2_tex_tune",
    "v2_tex_hp", "v2_tex_lp", "v2_tex_width", "v2_tex_attack", "v2_tex_release",
}

CAT_CHOIR = "Monumental & Cathedral Choirs"
CAT_SOLO = "Operatic & Expressive Soloists"
CAT_CLUB = "Festival, DJ & Club Anthems"
CAT_DARK = "Dark Cinematic & Witch House Rituals"
CAT_ETHER = "Ethereal Hybrids & Evolving Textures"
CATEGORIES = [CAT_CHOIR, CAT_SOLO, CAT_CLUB, CAT_DARK, CAT_ETHER]

# Short aliases -> parameter IDs
A = {
    "pX": "phoneme_x", "pY": "phoneme_y", "orb": "orbit_enable", "oR": "orbit_rate", "oD": "orbit_depth",
    "reg": "register_mode", "sz": "ensemble_size", "spr": "ensemble_spread", "vib": "vibrato_depth",
    "att": "attack_time", "rel": "release_time", "onM": "onset_mode", "onA": "onset_amount",
    "ten": "glottal_tension", "br": "breath_amount", "thr": "throat_shift", "foc": "formant_focus",
    "sub": "sub_abyss", "crn": "spectral_crown", "gain": "master_gain", "drv": "drive_amount",
    "tape": "tape_instability", "oxd": "oxide_damping", "scP": "screw_pitch", "scD": "screw_depth",
    "hq": "oversample_hq", "spT": "space_type", "spM": "space_mix", "spD": "space_decay", "spDm": "space_damp",
    "shim": "space_shimmer", "leg": "legato_enable", "gl": "glide_time", "expr": "expression_level",
    "tMix": "v2_tex_mix", "tAsset": "v2_tex_asset", "tTune": "v2_tex_tune", "tKey": "v2_tex_keytrack",
    "tAtt": "v2_tex_attack", "tRel": "v2_tex_release", "tVel": "v2_tex_vel_sens", "tHp": "v2_tex_hp",
    "tLp": "v2_tex_lp", "tW": "v2_tex_width", "xyMode": "v2_xy_mode", "eVib": "v2_expr_vibrato",
    "eEff": "v2_expr_effort", "eOpen": "v2_expr_openness", "eSwell": "v2_expr_swell",
}

# Texture assets (Source/V2/TextureAssetTable.h order)
TEX_BREATH_EXHALE, TEX_BREATH_INHALE, TEX_WHISPER_WIND, TEX_CATHEDRAL_AIR = 0, 1, 2, 3
TEX_CHOIR_DRONE, TEX_GUARDIAN_HALO, TEX_GOSPEL_OOH, TEX_BOWED = 4, 5, 6, 7


def R(dest, depth, bipolar=False):
    return (A.get(dest, dest), float(depth), bool(bipolar))


def M(label, routes=(), legacy=False):
    return {"label": label, "legacy": legacy, "routes": list(routes)}


# ─────────────────────────────────────────────────────────────────────────────────────────────────────────
# The 20 presets. Parameter values are real units (dB, semitones, seconds, 0..1, choice index).
# Route depth is a fraction of the destination's normalised range (master_gain: 24 dB span, so -0.0417 = -1 dB).
# ─────────────────────────────────────────────────────────────────────────────────────────────────────────
PRESETS = [
    # ── Monumental & Cathedral Choirs ──────────────────────────────────────────────────────────────────
    dict(
        name="Cathedral of Dunes", category=CAT_CHOIR,
        tags=["choir", "satb", "epic", "cinematic", "widescreen", "cathedral", "air", "pad"],
        description="Dune-scale SATB wall: 22 singers over a desert-deep sub, open cathedral air and slow orbiting vowels.",
        params=dict(pX=0.50, pY=0.84, orb=1, oR=0.06, oD=0.16, reg=4, sz=22, spr=0.62, vib=0.34, att=0.60, rel=2.6,
                    onM=0, onA=0.24, ten=0.70, br=0.18, thr=-1.0, foc=0.66, sub=0.60, crn=0.42, drv=0.20, tape=0.06,
                    oxd=0.10, spT=0, spM=0.40, spD=0.78, spDm=0.32, shim=0.28, expr=0.80, gain=-4.5,
                    tMix=0.10, tAsset=TEX_CATHEDRAL_AIR, tKey=0.25, tAtt=0.8, tRel=3.0, tVel=0.3, tHp=350, tLp=9000, tW=1.0,
                    eVib=0.45, eEff=0.55, eOpen=0.50, eSwell=0.50),
        macros=[
            M("SIZE", [R("v2_tex_mix", 0.08)], legacy=True),
            M("AIR", [R("v2_tex_mix", 0.35), R("br", 0.30), R("crn", 0.30), R("shim", 0.30), R("gain", -0.04)]),
            M("DARKNESS", [R("thr", -0.15), R("crn", -0.35), R("foc", -0.15), R("spDm", 0.35), R("v2_tex_lp", -0.35), R("pY", -0.20)]),
            M("DEPTH", [R("gain", -0.02)], legacy=True),
        ],
        wheel=[R("vib", 0.22), R("crn", 0.12), R("sz", 0.15)],
        pressure=[R("ten", 0.15), R("vib", 0.18), R("v2_tex_mix", 0.12)],
    ),
    dict(
        name="Basilica Eternal", category=CAT_CHOIR,
        tags=["choir", "satb", "liturgical", "sacred", "hum", "swell", "slow", "pad"],
        description="Liturgical choir that blooms from a closed hum into an open, candle-warm basilica swell.",
        params=dict(pX=0.30, pY=0.66, reg=4, sz=20, spr=0.58, vib=0.42, att=1.10, rel=3.2, onM=1, onA=0.30,
                    ten=0.58, br=0.22, thr=-2.0, foc=0.60, sub=0.50, crn=0.22, drv=0.16, tape=0.08, oxd=0.14,
                    spT=0, spM=0.46, spD=0.84, spDm=0.40, shim=0.24, expr=0.78, gain=-5.0,
                    tMix=0.08, tAsset=TEX_GOSPEL_OOH, tKey=1.0, tAtt=1.2, tRel=3.0, tVel=0.4, tHp=180, tLp=8000, tW=0.8,
                    eVib=0.40, eEff=0.55, eOpen=0.50, eSwell=0.60),
        macros=[
            M("SWELL", [R("sz", 0.30), R("ten", 0.28), R("crn", 0.30), R("v2_tex_mix", 0.15), R("gain", -0.06)]),
            M("VOWEL", [R("pX", 0.30), R("pY", 0.25)]),
            M("HALO", [R("shim", 0.50), R("crn", 0.30), R("br", 0.15)]),
            M("NAVE", [R("spM", 0.18), R("spD", 0.12), R("spDm", -0.20), R("sub", 0.10), R("gain", -0.03)]),
        ],
        wheel=[R("vib", 0.20), R("ten", 0.10), R("pY", 0.10)],
        pressure=[R("ten", 0.18), R("crn", 0.12), R("v2_tex_mix", 0.10)],
    ),
    dict(
        name="Siege of Giants", category=CAT_CHOIR,
        tags=["choir", "trailer", "epic", "brass", "fortissimo", "aggressive", "cinematic", "staccato"],
        description="Fortissimo trailer choir with brassy bite and a thunder-sub floor, built for decisive climaxes.",
        params=dict(pX=0.62, pY=0.80, reg=4, sz=22, spr=0.60, vib=0.40, att=0.14, rel=1.2, onM=0, onA=0.46,
                    ten=0.90, br=0.10, thr=-1.4, foc=0.78, sub=0.52, crn=0.50, drv=0.40, tape=0.05, oxd=0.08, hq=1,
                    spT=0, spM=0.36, spD=0.62, spDm=0.30, shim=0.18, expr=0.90, gain=-3.5,
                    eVib=0.35, eEff=0.70, eOpen=0.45, eSwell=0.30),
        macros=[
            M("COLOSSUS", [], legacy=True),
            M("FORTISSIMO", [R("ten", 0.10), R("drv", 0.18), R("crn", 0.15), R("onA", 0.25), R("gain", -0.06)]),
            M("THUNDER", [R("sub", 0.30), R("thr", -0.12), R("foc", 0.08), R("gain", -0.04)]),
            M("HALL", [R("spM", 0.20), R("spD", 0.18), R("shim", 0.15)]),
        ],
        wheel=[R("ten", 0.08), R("crn", 0.15), R("vib", 0.12), R("drv", 0.10)],
        pressure=[R("drv", 0.15), R("ten", 0.10), R("crn", 0.10)],
    ),
    dict(
        name="Seraphim Ascendant", category=CAT_CHOIR,
        tags=["choir", "soprano", "shimmer", "widescreen", "evolving", "cinematic", "halo", "pad"],
        description="Soprano-crowned 24-voice ascension: shimmering halo, orbiting vowels and a sunrise of overtones.",
        params=dict(pX=0.70, pY=0.74, orb=1, oR=0.10, oD=0.34, reg=4, sz=24, spr=0.66, vib=0.38, att=0.80, rel=3.0,
                    onM=0, onA=0.20, ten=0.66, br=0.22, thr=0.5, foc=0.64, sub=0.46, crn=0.62, drv=0.14, tape=0.08,
                    oxd=0.06, spT=0, spM=0.42, spD=0.82, spDm=0.22, shim=0.50, expr=0.82, gain=-5.5,
                    tMix=0.08, tAsset=TEX_GUARDIAN_HALO, tKey=1.0, tAtt=1.0, tRel=3.5, tVel=0.3, tHp=500, tLp=12000, tW=1.2,
                    eVib=0.40, eEff=0.50, eOpen=0.55, eSwell=0.50),
        macros=[
            M("HORIZON", [R("spr", 0.15), R("spM", 0.15), R("shim", 0.25), R("v2_tex_width", 0.20)]),
            M("SUNRISE", [R("crn", 0.35), R("pY", 0.15), R("ten", 0.15), R("v2_tex_mix", 0.25), R("gain", -0.04)]),
            M("STORM", [R("oD", 0.40), R("oR", 0.25), R("tape", 0.25), R("drv", 0.12)]),
            M("EARTH", [R("sub", 0.35), R("thr", -0.15), R("foc", 0.10), R("gain", -0.04)]),
        ],
        wheel=[R("vib", 0.20), R("crn", 0.12), R("shim", 0.10)],
        pressure=[R("ten", 0.15), R("vib", 0.15), R("v2_tex_mix", 0.12)],
    ),

    # ── Operatic & Expressive Soloists ─────────────────────────────────────────────────────────────────
    dict(
        name="Prima Donna", category=CAT_SOLO,
        tags=["solo", "soprano", "operatic", "lead", "expressive", "mpe", "legato", "vibrato"],
        description="A single soaring soprano: breath on the mod wheel blooms into full operatic vibrato and ring.",
        params=dict(pX=0.58, pY=0.82, reg=3, sz=1, spr=0.10, vib=0.50, att=0.16, rel=1.1, onM=0, onA=0.22,
                    ten=0.64, br=0.14, thr=0.5, foc=0.76, sub=0.0, crn=0.34, drv=0.04, tape=0.03, oxd=0.04,
                    spT=0, spM=0.36, spD=0.70, spDm=0.30, shim=0.14, leg=1, gl=0.12, expr=0.80, gain=2.0,
                    eVib=0.75, eEff=0.70, eOpen=0.60, eSwell=0.50),
        macros=[
            M("BLOOM", [R("vib", 0.22), R("crn", 0.22), R("ten", 0.12), R("spM", 0.10), R("gain", -0.03)]),
            M("VOWEL", [R("pX", 0.28), R("pY", -0.30)]),
            M("BREATH", [R("br", 0.35), R("onA", 0.30), R("crn", -0.10), R("ten", -0.12)]),
            M("HALL", [R("spM", 0.25), R("spD", 0.18), R("shim", 0.20)]),
        ],
        wheel=[R("vib", 0.28), R("crn", 0.15), R("ten", 0.10), R("spM", 0.06)],
        pressure=[R("vib", 0.25), R("ten", 0.18), R("crn", 0.10)],
    ),
    dict(
        name="Basso Lamentoso", category=CAT_SOLO,
        tags=["solo", "bass", "baritone", "operatic", "lament", "legato", "expressive", "mpe"],
        description="A grave, chest-resonant basso soloist that weeps into vibrato as you lean on the keys.",
        params=dict(pX=0.32, pY=0.72, reg=0, sz=1, spr=0.12, vib=0.46, att=0.18, rel=1.2, onM=0, onA=0.24,
                    ten=0.60, br=0.18, thr=-1.5, foc=0.70, sub=0.22, crn=0.06, drv=0.08, tape=0.03, oxd=0.08,
                    spT=0, spM=0.30, spD=0.62, spDm=0.45, shim=0.06, leg=1, gl=0.16, expr=0.82, gain=-1.5,
                    eVib=0.70, eEff=0.65, eOpen=0.55, eSwell=0.50),
        macros=[
            M("GRAVITAS", [R("thr", -0.12), R("sub", 0.25), R("foc", 0.10), R("gain", -0.03)]),
            M("CHEST", [R("ten", 0.28), R("drv", 0.22), R("crn", 0.14), R("gain", -0.03)]),
            M("LAMENT", [R("vib", 0.25), R("br", 0.18), R("tape", 0.10)]),
            M("CHAPEL", [R("spM", 0.28), R("spD", 0.20)]),
        ],
        wheel=[R("vib", 0.22), R("ten", 0.12), R("crn", 0.08)],
        pressure=[R("ten", 0.15), R("vib", 0.20), R("drv", 0.08)],
    ),
    dict(
        name="Tenore Eroico", category=CAT_SOLO,
        tags=["solo", "tenor", "operatic", "heroic", "lead", "legato", "expressive", "ring"],
        description="Heroic tenor with a ringing squillo; cover the vowel for drama, then open it to the gallery.",
        params=dict(pX=0.56, pY=0.80, reg=1, sz=2, spr=0.16, vib=0.44, att=0.12, rel=1.0, onM=0, onA=0.30,
                    ten=0.76, br=0.12, thr=0.0, foc=0.80, sub=0.10, crn=0.36, drv=0.10, tape=0.03, oxd=0.05,
                    spT=0, spM=0.34, spD=0.66, spDm=0.28, shim=0.12, leg=1, gl=0.10, expr=0.84, gain=1.5,
                    eVib=0.70, eEff=0.70, eOpen=0.55, eSwell=0.50),
        macros=[
            M("RING", [R("foc", 0.15), R("crn", 0.25), R("ten", 0.12), R("gain", -0.03)]),
            M("COVER", [R("pX", -0.22), R("pY", -0.12), R("thr", -0.10), R("crn", -0.15)]),
            M("VIBRATO", [R("vib", 0.40), R("br", 0.06)]),
            M("STAGE", [R("spM", 0.25), R("spD", 0.18), R("shim", 0.15)]),
        ],
        wheel=[R("vib", 0.25), R("ten", 0.10), R("crn", 0.12)],
        pressure=[R("vib", 0.22), R("ten", 0.15), R("crn", 0.08)],
    ),
    dict(
        name="Moonlit Mezzo", category=CAT_SOLO,
        tags=["solo", "alto", "mezzo", "intimate", "breathy", "legato", "expressive", "mpe"],
        description="Close-mic mezzo with real breath texture: intimate at a whisper, warm and present at full voice.",
        params=dict(pX=0.44, pY=0.70, reg=2, sz=1, spr=0.14, vib=0.40, att=0.22, rel=1.4, onM=0, onA=0.40,
                    ten=0.46, br=0.32, thr=-0.5, foc=0.70, sub=0.04, crn=0.18, drv=0.03, tape=0.05, oxd=0.05,
                    spT=0, spM=0.40, spD=0.74, spDm=0.38, shim=0.16, leg=1, gl=0.14, expr=0.76, gain=1.5,
                    tMix=0.10, tAsset=TEX_BREATH_EXHALE, tKey=0.25, tAtt=0.02, tRel=0.8, tVel=0.7, tHp=400, tLp=10000, tW=0.6,
                    eVib=0.70, eEff=0.60, eOpen=0.60, eSwell=0.60),
        macros=[
            M("AIR", [R("br", 0.45), R("v2_tex_mix", 0.35), R("onA", 0.30), R("gain", -0.02)]),
            M("WARMTH", [R("thr", -0.10), R("crn", -0.18), R("foc", -0.10), R("sub", 0.12)]),
            M("TREMBLE", [R("vib", 0.35), R("tape", 0.10)]),
            M("DISTANCE", [R("spM", 0.28), R("spD", 0.15), R("crn", -0.10)]),
        ],
        wheel=[R("vib", 0.22), R("ten", 0.15), R("br", -0.10)],
        pressure=[R("vib", 0.20), R("ten", 0.15), R("v2_tex_mix", 0.10)],
    ),

    # ── Festival, DJ & Club Anthems ────────────────────────────────────────────────────────────────────
    dict(
        name="Euphoria Mainstage", category=CAT_CLUB,
        tags=["edm", "festival", "anthem", "chords", "bright", "pad", "euphoric", "big room"],
        description="Euphoric mainstage vocal chords: bright, wide and mix-cutting, with a lift macro for the build.",
        params=dict(pX=0.54, pY=0.82, reg=4, sz=18, spr=0.58, vib=0.22, att=0.06, rel=0.9, onM=0, onA=0.30,
                    ten=0.74, br=0.14, thr=0.5, foc=0.70, sub=0.18, crn=0.60, drv=0.30, tape=0.04, oxd=0.04,
                    spT=0, spM=0.30, spD=0.56, spDm=0.26, shim=0.30, expr=0.84, gain=-2.5,
                    eVib=0.35, eEff=0.55, eOpen=0.50, eSwell=0.40),
        macros=[
            M("LIFT", [R("crn", 0.25), R("pY", 0.10), R("shim", 0.30), R("spM", 0.10), R("spr", 0.10), R("gain", -0.04)]),
            M("LOWPASS", [R("crn", -0.45), R("foc", -0.20), R("pY", -0.30), R("thr", -0.10), R("ten", -0.20), R("gain", -0.05)]),
            M("WIDTH", [R("spr", 0.25), R("sz", 0.20), R("tape", 0.06)]),
            M("DROP", [R("sub", 0.35), R("drv", 0.25), R("ten", 0.12), R("gain", -0.06)]),
        ],
        wheel=[R("crn", 0.18), R("vib", 0.12), R("shim", 0.12)],
        pressure=[R("ten", 0.12), R("crn", 0.12), R("drv", 0.10)],
    ),
    dict(
        name="Formant Ignition", category=CAT_CLUB,
        tags=["lead", "formant", "talkbox", "edm", "mono", "legato", "vowel", "xy"],
        description="Talking formant lead: the mod wheel and XY pad sweep the vowels, BITE adds drive for the drop.",
        params=dict(pX=0.30, pY=0.60, reg=2, sz=3, spr=0.24, vib=0.18, att=0.02, rel=0.35, onM=0, onA=0.40,
                    ten=0.82, br=0.08, thr=1.0, foc=0.82, sub=0.14, crn=0.48, drv=0.42, tape=0.04, oxd=0.06,
                    spT=1, spM=0.24, spD=0.40, spDm=0.40, shim=0.10, leg=1, gl=0.08, expr=0.86, gain=0.0, xyMode=0,
                    eVib=0.50, eEff=0.60, eOpen=0.70, eSwell=0.40),
        macros=[
            M("TALK", [R("pX", 0.45), R("pY", -0.30)]),
            M("BITE", [R("onA", 0.30), R("ten", 0.10), R("drv", 0.20), R("crn", 0.15), R("gain", -0.05)]),
            M("SCREW", [R("scP", -0.42), R("scD", 0.55)]),
            M("THROW", [R("spM", 0.35), R("spD", 0.30), R("shim", 0.20)]),
        ],
        wheel=[R("pX", 0.30), R("crn", 0.10), R("vib", 0.15)],
        pressure=[R("vib", 0.25), R("drv", 0.12), R("ten", 0.10)],
    ),
    dict(
        name="Afterhours Hymn", category=CAT_CLUB,
        tags=["house", "stab", "chords", "deep", "club", "hum", "short", "warm"],
        description="Deep-house hymn stab: a short, hummed choir chord that opens up and smears into a dub tail.",
        params=dict(pX=0.40, pY=0.58, reg=4, sz=10, spr=0.46, vib=0.12, att=0.015, rel=0.40, onM=1, onA=0.55,
                    ten=0.62, br=0.16, thr=-1.0, foc=0.64, sub=0.24, crn=0.24, drv=0.28, tape=0.06, oxd=0.18,
                    spT=1, spM=0.26, spD=0.42, spDm=0.46, shim=0.08, expr=0.80, gain=-6.0,
                    eVib=0.30, eEff=0.55, eOpen=0.55, eSwell=0.40),
        macros=[
            M("OPEN", [R("pY", 0.30), R("pX", 0.15), R("crn", 0.20)]),
            M("WARMTH", [R("drv", 0.25), R("oxd", 0.25), R("thr", -0.10), R("gain", -0.04)]),
            M("TAIL", [R("spM", 0.30), R("spD", 0.35)]),
            M("DETUNE", [R("tape", 0.35), R("spr", 0.15)]),
        ],
        wheel=[R("pY", 0.15), R("crn", 0.12), R("onA", 0.15)],
        pressure=[R("ten", 0.15), R("drv", 0.10), R("crn", 0.08)],
    ),
    dict(
        name="Siren Tension", category=CAT_CLUB,
        tags=["hook", "tension", "riser", "soprano", "dark", "edm", "drama", "orbit"],
        description="A dramatic soprano siren hook: orbiting tension, a screwed shadow and a rush into the drop.",
        params=dict(pX=0.78, pY=0.50, orb=1, oR=0.45, oD=0.22, reg=3, sz=12, spr=0.52, vib=0.28, att=0.10, rel=1.2,
                    onM=2, onA=0.36, ten=0.80, br=0.16, thr=2.0, foc=0.76, sub=0.10, crn=0.56, drv=0.36, tape=0.10,
                    oxd=0.08, scP=-1.0, scD=0.20, spT=1, spM=0.34, spD=0.55, spDm=0.30, shim=0.26, expr=0.84, gain=-5.0,
                    eVib=0.45, eEff=0.60, eOpen=0.50, eSwell=0.40),
        macros=[
            M("TENSION", [R("ten", 0.15), R("crn", 0.25), R("drv", 0.20), R("foc", 0.10), R("gain", -0.05)]),
            M("SIREN", [R("oD", 0.45), R("oR", 0.30), R("vib", 0.15)]),
            M("SHADOW", [R("scD", 0.50), R("scP", -0.40)]),
            M("RUSH", [R("shim", 0.40), R("spM", 0.20), R("br", 0.15)]),
        ],
        wheel=[R("crn", 0.15), R("drv", 0.12), R("vib", 0.15)],
        pressure=[R("ten", 0.15), R("oD", 0.20), R("crn", 0.10)],
    ),

    # ── Dark Cinematic & Witch House Rituals ───────────────────────────────────────────────────────────
    dict(
        name="Obsidian Rite", category=CAT_DARK,
        tags=["drone", "dark", "ritual", "bass", "cavernous", "horror", "cinematic", "monolith"],
        description="A cavernous basso monolith drone with whispering spirits in the abyss and a dread-screwed shadow.",
        params=dict(pX=0.22, pY=0.44, reg=0, sz=12, spr=0.40, vib=0.18, att=0.90, rel=3.2, onM=1, onA=0.30,
                    ten=0.56, br=0.26, thr=-5.0, foc=0.56, sub=0.72, crn=0.10, drv=0.34, tape=0.18, oxd=0.32,
                    spT=2, spM=0.40, spD=0.82, spDm=0.56, shim=0.30, expr=0.80, gain=-8.0,
                    tMix=0.05, tAsset=TEX_WHISPER_WIND, tKey=0.25, tAtt=1.5, tRel=3.0, tVel=0.2, tHp=600, tLp=7000, tW=1.2,
                    eVib=0.30, eEff=0.60, eOpen=0.45, eSwell=0.60),
        macros=[
            M("DREAD", [R("thr", -0.15), R("scD", 0.40), R("scP", -0.25), R("crn", -0.08), R("gain", -0.03)]),
            M("SPIRITS", [R("v2_tex_mix", 0.30), R("br", 0.20), R("shim", 0.25)]),
            M("ROT", [], legacy=True),
            M("ABYSS", [], legacy=True),
        ],
        wheel=[R("ten", 0.12), R("vib", 0.12), R("sub", 0.10)],
        pressure=[R("drv", 0.15), R("ten", 0.12), R("v2_tex_mix", 0.12)],
    ),
    dict(
        name="Haunted Mixtape", category=CAT_DARK,
        tags=["witch house", "screwed", "lofi", "tape", "dark", "choir", "drag", "occult"],
        description="Chopped-and-screwed witch-house choir smeared across a dying cassette in a ghost void.",
        params=dict(pX=0.26, pY=0.52, reg=4, sz=14, spr=0.56, vib=0.26, att=0.20, rel=1.8, onM=2, onA=0.40,
                    ten=0.62, br=0.28, thr=-3.5, foc=0.60, sub=0.58, crn=0.22, drv=0.46, tape=0.34, oxd=0.40,
                    scP=-4.0, scD=0.60, spT=3, spM=0.42, spD=0.74, spDm=0.46, shim=0.40, expr=0.80, gain=-6.0,
                    eVib=0.35, eEff=0.55, eOpen=0.45, eSwell=0.50),
        macros=[
            M("SCREW", [R("scP", -0.30), R("scD", 0.30), R("thr", -0.08)]),
            M("HEX", [], legacy=True),
            M("HAZE", [R("tape", 0.40), R("oxd", 0.35), R("spM", 0.15)]),
            M("CRUSH", [R("drv", 0.35), R("oxd", 0.15), R("ten", 0.10), R("gain", -0.08)]),
        ],
        wheel=[R("ten", 0.10), R("crn", 0.12), R("vib", 0.10)],
        pressure=[R("drv", 0.15), R("scD", 0.15), R("ten", 0.10)],
    ),
    dict(
        name="Mourning Wraiths", category=CAT_DARK,
        tags=["choir", "detuned", "swell", "dark", "cinematic", "slow", "requiem", "horror"],
        description="Detuned requiem choir swelling out of the dark; wraiths drift apart as the tape decays.",
        params=dict(pX=0.18, pY=0.50, orb=1, oR=0.05, oD=0.30, reg=4, sz=22, spr=0.56, vib=0.40, att=1.40, rel=3.6,
                    onM=1, onA=0.26, ten=0.54, br=0.30, thr=-3.0, foc=0.56, sub=0.56, crn=0.18, drv=0.26, tape=0.30,
                    oxd=0.26, spT=2, spM=0.42, spD=0.82, spDm=0.50, shim=0.30, expr=0.80, gain=-5.5,
                    eVib=0.40, eEff=0.55, eOpen=0.50, eSwell=0.50),
        macros=[
            M("SWELL", [R("sz", 0.25), R("ten", 0.28), R("crn", 0.28), R("gain", -0.05)]),
            M("DETUNE", [R("tape", 0.35), R("spr", 0.12)]),
            M("ASH", [R("oxd", 0.55), R("drv", 0.40), R("crn", -0.20), R("gain", -0.04)]),
            M("GRAVE", [R("sub", 0.25), R("thr", -0.12), R("spDm", 0.25)]),
        ],
        wheel=[R("vib", 0.18), R("ten", 0.12), R("crn", 0.10)],
        pressure=[R("ten", 0.15), R("vib", 0.15), R("tape", 0.10)],
    ),
    dict(
        name="Leviathan Vigil", category=CAT_DARK,
        tags=["sub", "gothic", "atmosphere", "choir", "dark", "hum", "drone", "texture"],
        description="Sub-anchored gothic vigil: a hummed choir and breathing drone over a floor that moves air.",
        params=dict(pX=0.14, pY=0.40, reg=4, sz=16, spr=0.50, vib=0.24, att=1.2, rel=3.4, onM=1, onA=0.22,
                    ten=0.50, br=0.24, thr=-3.0, foc=0.52, sub=0.86, crn=0.08, drv=0.30, tape=0.14, oxd=0.30,
                    spT=0, spM=0.40, spD=0.78, spDm=0.60, shim=0.18, expr=0.80, gain=-6.5,
                    tMix=0.10, tAsset=TEX_CHOIR_DRONE, tKey=1.0, tAtt=1.5, tRel=3.5, tVel=0.3, tHp=220, tLp=5000, tW=1.0,
                    eVib=0.30, eEff=0.50, eOpen=0.45, eSwell=0.60),
        macros=[
            M("CRYPT", [R("sub", 0.14), R("thr", -0.15), R("spDm", 0.25), R("gain", -0.03)]),
            M("VEIL", [R("br", 0.35), R("v2_tex_mix", 0.35), R("shim", 0.30)]),
            M("IRON", [R("drv", 0.25), R("ten", 0.20), R("crn", 0.15), R("gain", -0.04)]),
            M("HOLLOW", [R("pY", -0.25), R("pX", -0.10), R("foc", -0.20), R("spM", 0.15)]),
        ],
        wheel=[R("ten", 0.12), R("sub", 0.08), R("vib", 0.10)],
        pressure=[R("ten", 0.15), R("crn", 0.10), R("v2_tex_mix", 0.10)],
    ),

    # ── Ethereal Hybrids & Evolving Textures ───────────────────────────────────────────────────────────
    dict(
        name="Prism Bloom", category=CAT_ETHER,
        tags=["texture", "hybrid", "halo", "shimmer", "evolving", "ambient", "xy", "soprano"],
        description="Soprano swarm fused with a sampled halo; push the XY pad right for texture, up for light.",
        params=dict(pX=0.76, pY=0.62, orb=1, oR=0.12, oD=0.30, reg=3, sz=14, spr=0.60, vib=0.36, att=0.90, rel=3.4,
                    onM=0, onA=0.18, ten=0.52, br=0.26, thr=2.0, foc=0.70, sub=0.12, crn=0.66, drv=0.08, tape=0.10,
                    oxd=0.04, spT=0, spM=0.44, spD=0.84, spDm=0.20, shim=0.62, expr=0.80, gain=-4.0, xyMode=2,
                    tMix=0.26, tAsset=TEX_GUARDIAN_HALO, tKey=1.0, tAtt=0.8, tRel=3.5, tVel=0.4, tHp=400, tLp=14000, tW=1.3,
                    eVib=0.45, eEff=0.45, eOpen=0.60, eSwell=0.50),
        macros=[
            M("BLOOM", [R("v2_tex_mix", 0.50), R("shim", 0.35), R("crn", 0.25), R("gain", -0.05)]),
            M("DRIFT", [R("oD", 0.40), R("tape", 0.25)]),
            M("HALO", [R("v2_tex_width", 0.60), R("v2_tex_hp", 0.40), R("spM", 0.25), R("shim", 0.20)]),
            M("SHADOW", [R("thr", -0.20), R("scD", 0.35), R("crn", -0.25), R("spDm", 0.30), R("gain", -0.03)]),
        ],
        xy_x=[R("v2_tex_mix", 0.20, True), R("br", -0.10, True), R("ten", -0.10, True)],
        xy_y=[R("crn", 0.25, True), R("shim", 0.20, True), R("pY", 0.15, True)],
        wheel=[R("crn", 0.12), R("vib", 0.15), R("v2_tex_mix", 0.12)],
        pressure=[R("v2_tex_mix", 0.15), R("ten", 0.12), R("vib", 0.12)],
    ),
    dict(
        name="Breathing Nebula", category=CAT_ETHER,
        tags=["texture", "drone", "evolving", "ambient", "choir", "orbit", "xy", "pad"],
        description="A choir that breathes: sampled choral drone and orbiting swarm drifting through a ghost void.",
        params=dict(pX=0.40, pY=0.56, orb=1, oR=0.08, oD=0.30, reg=4, sz=18, spr=0.56, vib=0.30, att=1.4, rel=3.8,
                    onM=1, onA=0.20, ten=0.50, br=0.30, thr=-1.5, foc=0.58, sub=0.44, crn=0.30, drv=0.12, tape=0.16,
                    oxd=0.14, spT=3, spM=0.42, spD=0.84, spDm=0.36, shim=0.44, expr=0.78, gain=-6.5, xyMode=2,  # Stage 6: -1 dB (96 kHz TP)
                    tMix=0.28, tAsset=TEX_CHOIR_DRONE, tKey=1.0, tAtt=1.5, tRel=4.0, tVel=0.3, tHp=200, tLp=9000, tW=1.1,
                    eVib=0.40, eEff=0.50, eOpen=0.55, eSwell=0.50),
        macros=[
            M("NEBULA", [R("v2_tex_mix", 0.25), R("spM", 0.15), R("spD", 0.10), R("gain", -0.04)]),
            M("PULSE", [R("oR", 0.35), R("oD", 0.25)]),
            M("ERODE", [R("tape", 0.30), R("oxd", 0.30), R("drv", 0.15)]),
            M("STARFIELD", [R("crn", 0.50), R("shim", 0.45), R("v2_tex_hp", 0.35)]),
        ],
        xy_x=[R("pX", 0.35, True), R("oD", 0.15, True)],
        xy_y=[R("spM", 0.15, True), R("crn", 0.20, True), R("v2_tex_lp", 0.20, True)],
        wheel=[R("v2_tex_mix", 0.12), R("crn", 0.10), R("vib", 0.12)],
        pressure=[R("v2_tex_mix", 0.15), R("ten", 0.12), R("shim", 0.10)],
    ),
    dict(
        name="Bowed Spirit Choir", category=CAT_ETHER,
        tags=["texture", "bowed", "hybrid", "strings", "choir", "cinematic", "xy", "alto"],
        description="Alto choir married to bowed crystal: the bow speaks first, the voices bloom out of it.",
        params=dict(pX=0.46, pY=0.74, reg=2, sz=10, spr=0.52, vib=0.34, att=0.45, rel=2.6, onM=0, onA=0.26,
                    ten=0.58, br=0.20, thr=0.0, foc=0.66, sub=0.22, crn=0.38, drv=0.10, tape=0.08, oxd=0.06,
                    spT=0, spM=0.40, spD=0.76, spDm=0.30, shim=0.26, expr=0.80, gain=-4.5, xyMode=2,
                    tMix=0.30, tAsset=TEX_BOWED, tKey=1.0, tAtt=0.3, tRel=2.5, tVel=0.6, tHp=160, tLp=16000, tW=1.0,
                    eVib=0.50, eEff=0.50, eOpen=0.55, eSwell=0.50),
        macros=[
            M("BOW", [R("v2_tex_mix", 0.30), R("v2_tex_width", 0.15), R("gain", -0.04)]),
            M("SPIRIT", [R("br", 0.45), R("crn", 0.40), R("shim", 0.45)]),
            M("MORPH", [R("pX", 0.25), R("pY", -0.20)]),
            M("DEPTH", [R("spM", 0.25), R("spD", 0.18), R("sub", 0.15)]),
        ],
        xy_x=[R("v2_tex_mix", 0.20, True), R("ten", -0.10, True)],
        xy_y=[R("crn", 0.20, True), R("v2_tex_lp", 0.25, True), R("pY", 0.10, True)],
        wheel=[R("v2_tex_mix", 0.12), R("vib", 0.15), R("crn", 0.10)],
        pressure=[R("v2_tex_mix", 0.15), R("vib", 0.15), R("ten", 0.10)],
    ),
    dict(
        name="Aeolian Drift", category=CAT_ETHER,
        tags=["texture", "wind", "whisper", "airy", "evolving", "ambient", "xy", "ghost"],
        description="Whispering choir carried on sampled wind; VOICE pulls a full-throated choir out of the air.",
        params=dict(pX=0.44, pY=0.42, orb=1, oR=0.16, oD=0.22, reg=4, sz=16, spr=0.58, vib=0.26, att=1.0, rel=3.4,
                    onM=2, onA=0.40, ten=0.40, br=0.48, thr=0.5, foc=0.62, sub=0.26, crn=0.40, drv=0.06, tape=0.14,
                    oxd=0.08, spT=3, spM=0.46, spD=0.84, spDm=0.26, shim=0.52, expr=0.78, gain=-4.5, xyMode=2,
                    tMix=0.24, tAsset=TEX_WHISPER_WIND, tKey=0.25, tAtt=1.0, tRel=3.5, tVel=0.3, tHp=300, tLp=12000, tW=1.2,
                    eVib=0.40, eEff=0.50, eOpen=0.55, eSwell=0.50),
        macros=[
            M("WIND", [R("v2_tex_mix", 0.30), R("br", 0.15), R("gain", -0.03)]),
            M("GHOST", [], legacy=True),
            M("TIDE", [R("oR", 0.25), R("oD", 0.10), R("spM", 0.15), R("shim", 0.15)]),
            M("VOICE", [R("ten", 0.30), R("foc", 0.15), R("sz", 0.20), R("br", -0.25), R("pY", 0.25), R("gain", -0.04)]),
        ],
        xy_x=[R("v2_tex_mix", 0.20, True), R("br", 0.10, True)],
        xy_y=[R("shim", 0.20, True), R("crn", 0.20, True), R("v2_tex_hp", 0.15, True)],
        wheel=[R("ten", 0.15), R("v2_tex_mix", 0.10), R("vib", 0.12)],
        pressure=[R("v2_tex_mix", 0.15), R("ten", 0.12), R("crn", 0.10)],
    ),
]


# ─────────────────────────────────────────────────────────────────────────────────────────────────────────
def parse_param_header():
    """Returns (ordered list of param ids, {id: default}) from ParameterIDs.h."""
    src = PARAM_HEADER.read_text(encoding="utf-8")
    ns_start = src.index("namespace defaults")
    ids_part, defaults_part = src[:ns_start], src[ns_start:src.index("} // namespace defaults")]
    name_to_id = dict(re.findall(r'inline constexpr const char\*\s+(\w+)\s*=\s*"([^"]+)"', ids_part))
    defaults = {}
    for ctype, name, val in re.findall(r'inline constexpr (float|int|bool)\s+(\w+)\s*=\s*([^;]+);', defaults_part):
        val = val.split("//")[0].strip().rstrip("f")
        v = 1.0 if val == "true" else 0.0 if val == "false" else float(val)
        defaults[name_to_id[name]] = v
    layout = src[src.index("createParameterLayout"):]
    order = []
    for name in re.findall(r'ParameterID \{ (\w+), [12] \}', layout):
        pid = name_to_id[name]
        if pid not in order:
            order.append(pid)
    missing = [p for p in order if p not in defaults]
    if missing:
        raise SystemExit(f"defaults missing for {missing}")
    return order, defaults


RANGES = {  # real-unit bounds for validation (mirrors createParameterLayout)
    "ensemble_size": (1, 24), "attack_time": (0.005, 6.0), "release_time": (0.02, 8.0), "throat_shift": (-12, 12),
    "master_gain": (-18, 6), "screw_pitch": (-12, 0), "orbit_rate": (0.02, 4.0), "glide_time": (0.01, 1.5),
    "register_mode": (0, 4), "onset_mode": (0, 2), "space_type": (0, 3), "v2_tex_asset": (0, 7),
    "v2_tex_tune": (-24, 24), "v2_tex_attack": (0.001, 4.0), "v2_tex_release": (0.02, 8.0), "v2_tex_hp": (20, 2000),
    "v2_tex_lp": (400, 20000), "v2_tex_width": (0, 2), "v2_xy_mode": (0, 2),
}


def validate(p, order):
    errs = []
    if p["category"] not in CATEGORIES:
        errs.append("unknown category")
    labels = [m["label"] for m in p["macros"]]
    if len(labels) != 4 or len(set(labels)) != 4 or any(not l or len(l) > 12 for l in labels):
        errs.append(f"macro labels {labels}")
    v1 = ["COLOSSUS", "SPECTER", "ROT", "ABYSS"]  # slot-bound V1 curves
    for i, m in enumerate(p["macros"]):
        if not m["legacy"] and not m["routes"]:
            errs.append(f"dead macro {m['label']}")
        if m["label"] in v1 and (v1.index(m["label"]) != i or not m["legacy"]):
            errs.append(f"V1 label {m['label']} must sit in slot {v1.index(m['label']) + 1} with legacy on")
    for key in ("wheel", "pressure"):
        if not p.get(key):
            errs.append(f"no {key} routes")
    for src in [m["routes"] for m in p["macros"]] + [p.get(k, []) for k in ("wheel", "pressure", "xy_x", "xy_y")]:
        if len(src) > 8:
            errs.append("more than 8 routes on a source")
        for dest, depth, _ in src:
            if dest not in MOD_DESTS:
                errs.append(f"bad route dest {dest}")
            if not -1.0 <= depth <= 1.0 or depth == 0.0:
                errs.append(f"bad depth {dest} {depth}")
    if (p.get("xy_x") or p.get("xy_y")) and p["params"].get("xyMode", 0) != 2:
        errs.append("XY routes need xyMode=2 (Preset Axes)")
    for k, v in p["params"].items():
        pid = A.get(k, k)
        if pid not in order or pid in GLOBAL_IDS:
            errs.append(f"bad param {k}")
        lo, hi = RANGES.get(pid, (0.0, 1.0))
        if not lo <= v <= hi:
            errs.append(f"{pid}={v} outside [{lo},{hi}]")
    if len(p["tags"]) < 4 or not p["description"]:
        errs.append("metadata")
    return errs


def fmt(v):
    return format(float(v), ".9g")


def to_xml(p, order, defaults):
    vals = dict(defaults)
    for k, v in p["params"].items():
        vals[A.get(k, k)] = float(v)
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           f'<MonolithPreset version="2" name={quoteattr(p["name"])} author={quoteattr(AUTHOR)} '
           f'category={quoteattr(p["category"])} tags={quoteattr(",".join(p["tags"] + ["flagship", "monolith ii"]))} '
           f'description={quoteattr(p["description"])} favorite="0">',
           '  <MacroLabels ' + " ".join(f'm{i + 1}={quoteattr(m["label"])}' for i, m in enumerate(p["macros"])) + '/>',
           '  <Params>']
    for pid in order:
        if pid not in GLOBAL_IDS:
            out.append(f'    <Param id="{pid}" value="{fmt(vals[pid])}"/>')
    out.append('  </Params>')
    out.append('  <V2Control>')
    sources = [(f"macro{i + 1}", m) for i, m in enumerate(p["macros"])]
    sources += [("xy_x", {"routes": p.get("xy_x", [])}), ("xy_y", {"routes": p.get("xy_y", [])}),
                ("mod_wheel", {"routes": p["wheel"]}), ("pressure", {"routes": p["pressure"]}),
                ("breath", {"routes": p.get("breath", p["pressure"])})]  # CC2 breath mirrors pressure by default
    for sid, s in sources:
        attrs = f'id="{sid}"'
        if sid.startswith("macro"):
            attrs += f' label={quoteattr(s["label"])} legacy="{1 if s["legacy"] else 0}"'
        if not s["routes"]:
            out.append(f'    <Source {attrs}/>')
            continue
        out.append(f'    <Source {attrs}>')
        for dest, depth, bip in s["routes"]:
            out.append(f'      <Route dest="{dest}" depth="{fmt(depth)}" bipolar="{1 if bip else 0}"/>')
        out.append('    </Source>')
    out.append('  </V2Control>')
    out.append('</MonolithPreset>')
    return "\n".join(out) + "\n"


PROVENANCE = ROOT / "ASSET_PROVENANCE.md"
PROV_BEGIN, PROV_END = "<!-- BEGIN v2_flagship_presets (generated) -->", "<!-- END v2_flagship_presets -->"


def write_provenance_section():
    """Maintains a generated block in ASSET_PROVENANCE.md mapping each texture asset to the presets that use it."""
    header = (ROOT / "Source/V2/TextureAssetTable.h").read_text(encoding="utf-8")
    names = re.findall(r'\{ "\w+", "([^"]+)", (?:true|false), (?:true|false), \d+ \}', header)
    users = {i: [] for i in range(len(names))}
    for i, p in enumerate(PRESETS):
        if p["params"].get("tMix", 0.0) > 0.0:
            users[int(p["params"].get("tAsset", 0))].append(f"{i + 1:02d} {p['name']}")
    dependent = sum(len(v) for v in users.values())
    lines = [PROV_BEGIN, "## Flagship preset dependencies", "",
             f"{dependent} of {len(PRESETS)} flagship presets use the texture layer (`v2_tex_mix` > 0). If an asset changes, "
             "re-audition these presets and re-run `MonolithPresetQA`.", "",
             "| Asset | Flagship presets using it | Origin |", "|---|---|---|"]
    for i, n in enumerate(names):
        lines.append(f"| {n} | {', '.join(users[i]) if users[i] else '— (unused)'} | Original (synthesised) |")
    lines.append(PROV_END)
    block = "\n".join(lines)
    text = PROVENANCE.read_text(encoding="utf-8")
    if PROV_BEGIN in text:
        text = text[:text.index(PROV_BEGIN)] + block + text[text.index(PROV_END) + len(PROV_END):]
    else:
        text = text.rstrip("\n") + "\n\n" + block + "\n"
    PROVENANCE.write_text(text, encoding="utf-8")


def file_name(i, p):
    safe = re.sub(r"[^A-Za-z0-9]+", "_", p["name"]).strip("_")
    return f"{i + 1:02d}_{safe}.mnlpreset"


def main():
    check_only = "--check" in sys.argv
    order, defaults = parse_param_header()
    names = [p["name"] for p in PRESETS]
    problems = []
    if len(PRESETS) != 20 or len(set(names)) != 20:
        problems.append("need 20 uniquely named presets")
    for cat in CATEGORIES:
        if sum(p["category"] == cat for p in PRESETS) != 4:
            problems.append(f"category {cat} must hold exactly 4")
    for p in PRESETS:
        problems += [f"{p['name']}: {e}" for e in validate(p, order)]
    if problems:
        print("\n".join(problems))
        return 1
    if check_only:
        print(f"OK: {len(PRESETS)} presets valid ({len(order)} params, {len(GLOBAL_IDS)} global excluded)")
        return 0
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for old in OUT_DIR.glob("*.mnlpreset"):
        old.unlink()
    total = 0
    for i, p in enumerate(PRESETS):
        text = to_xml(p, order, defaults)
        (OUT_DIR / file_name(i, p)).write_text(text, encoding="utf-8")
        total += len(text.encode("utf-8"))
    write_provenance_section()
    print(f"wrote {len(PRESETS)} presets -> {OUT_DIR.relative_to(ROOT)} ({total / 1024:.1f} KB); ASSET_PROVENANCE.md dependency table updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
