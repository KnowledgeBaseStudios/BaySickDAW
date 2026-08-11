#!/usr/bin/env python3
"""
Factory preset generator for BaySickDAW.

Translates §P3 Preset Recipe Catalogue entries (blueprint lines 916-1067) into
BaySickSynth-format / BaySickBass-format XML preset files in the appropriate
per-player Presets folder.

Usage:
    python gen_factory_presets.py
    python gen_factory_presets.py --starter-only

Writes to:
    %USERPROFILE%/Documents/BaySickDAW/Presets/BaySickDrums/<family>/*.xml
    %USERPROFILE%/Documents/BaySickDAW/Presets/BaySickSynth/<family>/*.xml
    %USERPROFILE%/Documents/BaySickDAW/Presets/BaySickBass/<family>/*.xml
    Family subfolders ride the in-app picker grouping (see categorized_dir).

XMLs use the canonical "lay_0" trackId.  The trackId-substitution fix in each
player's loadPreset rewrites IDs to match the loading instance's trackId, so
the same XML loads correctly into any drum slot, layer, or bass page.
"""
import os
import sys
import argparse
from pathlib import Path

# ---- Paths ------------------------------------------------------------------

# Updated 2026-04-23: root moved from Roaming AppData to Documents per the
# unified user-folder layout (P4b).  Each engine's preset folder now also has
# subfolders per family (TR-808, Pads, Leads, etc.) for the grouped picker.
USERPROFILE = Path.home()
PRESETS_ROOT = USERPROFILE / "Documents" / "BaySickDAW" / "Presets"
DRUMS_DIR    = PRESETS_ROOT / "BaySickDrums"
SYNTH_DIR    = PRESETS_ROOT / "BaySickSynth"
BASS_DIR     = PRESETS_ROOT / "BaySickBass"
HARMLESS_DIR = PRESETS_ROOT / "Harmless"
PLAYER_DIR   = PRESETS_ROOT / "BaySickPlayer"

# Core Library — where the SFZ packs live.  Used both at runtime by the C++
# code (SampleLibrary::getCoreLibraryDir, LOCALAPPDATA/BaySickDAW/CoreLibrary
# on Windows) and here at preset-gen time so we can verify SFZ refs exist.
LOCAL_APPDATA = Path(os.environ.get("LOCALAPPDATA", "")) if os.name == "nt" else None
CORE_LIBRARY_DIR = (LOCAL_APPDATA / "BaySickDAW" / "CoreLibrary") if LOCAL_APPDATA else None

# ---- Engine formats ---------------------------------------------------------
# BaySickSynth and BaySickBass share the same DSP / param layout (per
# BaySickBassProcessor.cpp:65 "Layout identical to BaySickSynthProcessor").
# Only the param-id prefix tag and APVTS state root tag differ.
# Harmless is its own additive engine — different param schema, different tag.
# BaySickPlayer is the sample player — different schema again, plus its
# preset XML has a <Sample kind path/> child element pointing at the loaded
# audio (see DrumPage::savePatchAs / loadPlayerPreset for the round-trip).

ENGINE_BSS    = ("bss",  "BaySickSynthState")   # BaySickSynth + drum-slot synth patches
ENGINE_BSB    = ("bsb",  "BaySickBassState")    # BaySickBass page
ENGINE_HARM   = ("harm", "HarmlessState")       # Harmless additive synth
ENGINE_BSP    = ("bsp",  "BaySickPlayerState")  # BaySickPlayer (sample player)

# ---- Defaults (mirror of createLayout defaults) -----------------------------
# Every preset includes ALL params explicitly so replaceState produces a
# fully-known state regardless of what was on the slot before.

DEFAULTS = {
    # OSC
    "waveform":       0,    # SAW
    "transpose":      0,
    "modifier":       0.5,
    "dualOscMode":    0,    # Musical
    "oscSync":        0,    # false
    "ringMod":        0,
    "drift":          0.0,
    "unison_voices":  1,
    "unison_detune":  0.2,
    "unison_spread":  0.8,
    "noise":          0.0,
    "noiseOnly":      0,
    "noiseColor":     0,    # White
    # Voice mode
    "voiceMode":      0,    # Poly
    "glide":          0.0,
    "cutSelf":        0,
    # Mod wheel
    "modWheelDest":   0,
    "modWheelAmt":    0.0,
    # Amp ADSR
    "amp_attack":     0.01,
    "amp_decay":      0.10,
    "amp_sustain":    0.8,
    "amp_release":    0.30,
    "velAmpTrack":    1.0,
    # Pitch envelope
    "pEnv_attack":    0.01,
    "pEnv_decay":     0.10,
    "pEnv_sustain":   0.0,
    "pEnv_release":   0.30,
    "pEnv_amt":       0.0,
    # Transient injector
    "trans_amount":   0.0,
    "trans_duration": 5.0,
    "trans_colour":   5000.0,
    # Multi-burst envelope
    "burst_mode":     0,
    "burst_count":    4,
    "burst_spacing":  20.0,
    # Filter
    "flt_type":       0,    # LP
    "flt_cutoff":     20000.0,
    "flt_res":        0.0,
    "flt_env_amt":    0.0,
    "flt_kbtrack":    0.0,
    "flt_veltrack":   0.0,
    # Filter ADSR
    "flt_attack":     0.01,
    "flt_decay":      0.10,
    "flt_sustain":    0.5,
    "flt_release":    0.30,
    # LFO
    "lfo_shape":      0,    # Sine
    "lfo_dest":       0,
    "lfo_rate":       1.0,
    "lfo_sync":       0,
    "lfo_division":   2,
    "lfo_amount":     0.0,
}

# Choice index aliases
W_SAW = 0; W_SAWSAW = 1; W_PULSE = 2; W_SAWSQUARE = 3; W_SQUARESQUARE = 4
W_SUPERSAW = 5; W_BELL = 6; W_DEAFSAW = 7; W_SPREADOCT = 8; W_SPREAD5TH = 9
W_SINE = 10
DUAL_MUSICAL = 0; DUAL_HZOFFSET = 1; DUAL_ABSHZ = 2
NOISE_WHITE = 0; NOISE_PINK = 1; NOISE_BROWN = 2
VOICE_POLY = 0; VOICE_MONO = 1; VOICE_LEAD = 2; VOICE_LEGATO = 3
FLT_LP = 0; FLT_HP = 1; FLT_BP = 2; FLT_NOTCH = 3
LFO_SINE = 0; LFO_SAW = 1; LFO_SQUARE = 2
LFO_DEST_FILTER = 0; LFO_DEST_PITCH = 1; LFO_DEST_OSCMOD = 2

# ─── Harmless engine defaults (2026-04-26) ───────────────────────────────────
# Mirror of HarmlessProcessor::createLayout (Source/Harmless/HarmlessProcessor.cpp:119).
# Param prefix is "tk_<trackId>_harm_".  ~90 params total — every preset writes
# every param so replaceState produces a fully-known state regardless of slot
# history.  Choice/Int values written as <int>.0 in the XML (matches existing
# convention in write_preset_xml).
HARM_DEFAULTS = {
    # Timbre
    "timbre_shape":         1,     # 0=Sine 1=Saw 2=Square 3=Triangle
    "partB_timbre_shape":   2,
    "timbre_blend":         0.0,
    "partA_level":          1.0,
    "partB_level":          0.0,
    # Filter 1
    "flt_cutoff":           20000.0,
    "flt_res":              0.7071,
    "flt_env_amt":          0.0,
    # Amp ADSR
    "amp_a":                0.01,
    "amp_d":                0.10,
    "amp_s":                0.8,
    "amp_r":                0.30,
    # Filter ADSR
    "flt_a":                0.01,
    "flt_d":                0.10,
    "flt_s":                0.5,
    "flt_r":                0.30,
    # Unison
    "unison_voices":        1,
    "unison_detune":        15.0,   # cents
    "unison_spread":        0.7,
    # Spectral modules — Part A
    "prism_amount":         0.0,
    "pluck_decay":          0.0,
    "blur_size":            0.0,
    "blur_time":            1.0,
    "blur_harm":            0.0,
    "filter_mask_cutoff":   5000.0,
    "phaser_mask_rate":     1.0,
    "brownian_amount":      1.0,
    # Spectral modules — Part B mirrors
    "partB_prism_amount":         0.0,
    "partB_pluck_decay":          0.0,
    "partB_blur_size":            0.0,
    "partB_blur_time":            1.0,
    "partB_blur_harm":            0.0,
    "partB_filter_mask_cutoff":   5000.0,
    "partB_phaser_mask_rate":     1.0,
    "partB_brownian_amount":      1.0,
    # Portamento / legato / strum / cutSelf
    "glide_time":           0.0,
    "legato":               0,      # bool as 0/1
    "cutSelf":              0,
    "strum_time":           0.0,
    # Global
    "volume":               0.8,
    "pan":                  0.0,
    "oeq_mix":              0.0,
    # Phase init
    "phase_start":          0.0,
    "phase_rand":           1.0,
    # Tremolo
    "trem_shape":           0,
    "trem_depth":           0.0,
    "trem_speed":           3.0,
    "trem_gap":             0.0,
    # Vibrato
    "vib_shape":            0,
    "vib_depth":            0.0,
    "vib_speed":            5.0,
    "vib_env":              0.0,
    # Filter 1 extensions
    "flt1_type":            0,      # 0=LP 1=HP 2=BP 3=Notch
    "flt1_kb_track":        0.0,
    "flt2_kb_track":        0.0,
    "flt1_cutoff_ofs":      0.0,
    "flt2_cutoff_ofs":      0.0,
    # Filter 2
    "flt2_type":            3,      # default Notch (transparent when env=0)
    "flt2_cutoff":          20000.0,
    "flt2_res":             0.7071,
    "flt2_env_amt":         0.0,
    "flt2_a":               0.01,
    "flt2_d":               0.10,
    "flt2_s":               0.5,
    "flt2_r":               0.30,
    # Pitch offset
    "pitch_semitones":      0.0,
    "pitch_cents":          0.0,
    "pitch_freq_frac":      0,
    # Legato limit
    "legato_limit":         0.5,
    # Unison extensions
    "unison_type":          0,
    "unison_alt":           0,
    "unison_phase":         0.0,
    # Prism mode
    "prism_mode":           0,
    "partB_prism_mode":     0,
    # Routing matrix (sub/prot/clip/fx/vol/env)
    "rm_sub":               0.0,
    "rm_prot":              0.0,
    "rm_clip":              0.0,
    "rm_fx":                1.0,
    "rm_vol":               1.0,
    "rm_env":               1.0,
    # Output phaser
    "ophaser_mix":          0.0,
    "ophaser_depth":        0.5,
    "ophaser_rate":         1.0,
    "ophaser_width":        0.5,
    "ophaser_ofs":          1000.0,
    # Strum extensions
    "strum_dir":            0,
    "strum_tns":            0.0,
    # Global LFO (S4 Batch 4)
    "lfo_rate":             7,      # index 7 = 1 beat
    "lfo_shape":            0,
    "lfo_tempo":            1,      # bool default true
    # Part select + vel link
    "part_sel":             0,
    "vel_link":             0,
    # XYZ mod pad
    "mod_x":                0.0,
    "mod_y":                0.0,
    "mod_z":                0.0,
    # Auto-gain mode
    "auto_gain_mode":       0,
    # Pluck blur toggles
    "pluck_blur":           0,
    "partB_pluck_blur":     0,
}

# Harmless timbre shape aliases (per createLayout choice arrays)
HT_SINE = 0; HT_SAW = 1; HT_SQUARE = 2; HT_TRIANGLE = 3
# Harmless filter type aliases (flt1_type / flt2_type)
HF_LP = 0; HF_HP = 1; HF_BP = 2; HF_NOTCH = 3

# ─── BaySickPlayer engine defaults (2026-04-26) ──────────────────────────────
# Mirror of BaySickPlayerProcessor::createLayout (Source/BaySickPlayer/
# BaySickPlayerProcessor.cpp).  Param prefix is "tk_<trackId>_bsp_".
# Preset XML format wraps the apvts state inside an outer <BaySickPlayerState>
# along with a <Sample kind="sfz" path="library:Pack/File.sfz"/> sibling
# (see savePatchAs in DrumPage.cpp:817).
BSP_DEFAULTS = {
    # UI knobs
    "lfoAmt":        0.0,
    "cutoff":        20000.0,
    "res":           0.0,
    # UI sliders
    "drive":         0.0,
    "reduct":        0.0,
    # Envelope
    "decay":         0.5,
    "release":       0.3,
    "attack":        0.001,
    "sustain":       1.0,
    # Output
    "pan":           0.0,
    "volume":        0.8,
    "stereo":        0.0,
    "lfo_rate":      5.5,
    "treble":        0.0,
    "stretch":       1.0,
    # Velocity routings
    "velToMuffle":   0.0,
    "muffle":        0.0,
    "velToHardness": 0.0,
    "hardness":      0.0,
    "sensitivity":   0.5,
    "velToVolume":   1.0,
    # Pitch
    "tune":          0.0,
    "detune":        0.0,
    # Articulation
    "artic_group":   0,
    # Voice mode
    "cutSelf":       0,
    "detuneMode":    0,    # 0=simple 1=random 2=pair
    "voiceCap":      16,
    # Sample modes
    "reverse":       0,
    "sampleStart":   0.0,
    # Unison
    "unisonVoices":  1,
    "unisonSpread":  0.0,
}

# ---- Categories (2026-04-23 family grouping) -------------------------------
# Each preset is written into <engine_dir>/<category>/ so the in-app picker
# can render the menu with section headers per family.  Names not in any
# map fall back to <engine_dir>/ (root) and render under "My Presets".

# 2026-04-25 (Batch 5 expansion): folder names updated to match the new
# 10-category doc structure.  User-confirmed moves: Cabasa Shaker / Tambourine /
# Rimshot Acoustic / Stick-Hit Drum -> Hand Percussion (were Tuned).
DRUM_CATEGORIES = {
    # 808 Group — was TR-808
    "808 Kick": "808 Group",
    "808 Handclap": "808 Group",
    "808 Snare": "808 Group",
    "808 Closed Hat": "808 Group",
    "808 Open Hat": "808 Group",
    "808 Cowbell": "808 Group",
    "808 Conga Hi": "808 Group",
    "808 Conga Mid": "808 Group",
    "808 Conga Lo": "808 Group",
    "808 Tom Hi": "808 Group",
    "808 Tom Mid": "808 Group",
    "808 Tom Lo": "808 Group",
    "808 Rimshot": "808 Group",
    "808 Maraca": "808 Group",
    "808 Claves": "808 Group",
    # 909 Group — was TR-909
    "909 Kick": "909 Group",
    "909 Snare": "909 Group",
    "909 Closed Hat": "909 Group",
    "909 Open Hat": "909 Group",
    "909 Ride Crash": "909 Group",
    "909 Tom Hi": "909 Group",
    "909 Tom Mid": "909 Group",
    "909 Tom Lo": "909 Group",
    # 606 Group — was TR-606
    "606 Kick": "606 Group",
    "606 Snare": "606 Group",
    # Simmons Group — was Simmons
    "Simmons Kick": "Simmons Group",
    "Simmons Snare": "Simmons Group",
    "Simmons Tom Hi": "Simmons Group",
    "Simmons Tom Lo": "Simmons Group",
    "Simmons SDS-7 Kick": "Simmons Group",
    # Yamaha Group — was Yamaha FM
    "RX-11 Kick": "Yamaha Group",
    "RX-11 Snare": "Yamaha Group",
    "DX7 Glass": "Yamaha Group",
    "DX7 Metal": "Yamaha Group",
    "DX7 Woodblock": "Yamaha Group",
    # Tuned Percussion (no rename, but several moved out to Hand Percussion)
    "Glockenspiel": "Tuned Percussion",
    "Marimba": "Tuned Percussion",
    "Xylophone": "Tuned Percussion",
    "Tubular Bells": "Tuned Percussion",
    "Celesta": "Tuned Percussion",
    "Triangle": "Tuned Percussion",
    "Agogo": "Tuned Percussion",
    "Woodblock": "Tuned Percussion",
    "Vibraphone Roll": "Tuned Percussion",
    "Flam Double-Hit": "Tuned Percussion",
    # Hand Percussion (existing + 4 moves from Tuned per user mapping)
    "Tabla Hi": "Hand Percussion",
    "Tabla Lo": "Hand Percussion",
    "Tabla Roll": "Hand Percussion",
    "Bongo Hi": "Hand Percussion",
    "Bongo Lo": "Hand Percussion",
    "Cabasa Shaker": "Hand Percussion",      # moved from Tuned
    "Tambourine": "Hand Percussion",          # moved from Tuned
    "Rimshot Acoustic": "Hand Percussion",    # moved from Tuned
    "Stick-Hit Drum": "Hand Percussion",      # moved from Tuned
    # Three new categories populated entirely by Batch 5 expansion below:
    #   "Modern EDM & Trap"
    #   "Lo-Fi, Chiptune & Texture"
    #   "Cinematic, Industrial & FX"
}

# 2026-04-25 (Batch 5 expansion): synth folders restructured to the doc's
# 10-category model.  User-confirmed mapping (see chat 2026-04-25):
#   - All Keys & Organs entries -> Keys & Electric Pianos
#   - Doctor Who Theremin -> Leads & Solos (theremin = lead instrument)
#   - CS-80 Bell -> Plucks & Mallets
#   - Moog Hz Interval -> Cinematic & Drones
#   - Lead-named Brass items (Brass Scoop, CS-80 Brass Lead, OB-8 Brass) -> Brass & Strings
#   - Pad-named String/Brass items (Jupiter Brass Pad, OB-8 String Pad, Solina Strings) -> Brass & Strings
#   - Square Lead 8-bit -> Chiptune & 8-Bit
#   - JP-8000 Supersaw Pad -> Synthwave & Vintage (per user)
#   - Sound FX bucket -> Cinematic & Drones (drones+FX absorbed)
SYNTH_CATEGORIES = {
    # 1. Keys & Electric Pianos
    "Clavinet": "Keys & Electric Pianos",
    "DX EP": "Keys & Electric Pianos",
    "Farfisa Organ": "Keys & Electric Pianos",
    "Hammond Drawbar": "Keys & Electric Pianos",
    "Hammond Organ": "Keys & Electric Pianos",
    "Harpsichord": "Keys & Electric Pianos",
    "Rhodes EP": "Keys & Electric Pianos",
    "Rhodes Wurli Strike": "Keys & Electric Pianos",
    "Vox Continental": "Keys & Electric Pianos",
    "Wurlitzer EP": "Keys & Electric Pianos",
    # 2. Plucks & Mallets
    "CS-80 Bell": "Plucks & Mallets",
    # 3. Pads & Atmospheres
    "Glass Pad": "Pads & Atmospheres",
    "Juno Warm Pad": "Pads & Atmospheres",
    "Ocean Pad": "Pads & Atmospheres",
    "Wind Pad": "Pads & Atmospheres",
    # 4. Leads & Solos
    "Classic 80s Sync Lead": "Leads & Solos",
    "Detuned Supersaw Lead": "Leads & Solos",
    "Doctor Who Theremin": "Leads & Solos",
    "DX Lead FM": "Leads & Solos",
    "Moog Lead": "Leads & Solos",
    "Moog Lead Woop": "Leads & Solos",
    "PWM Lead": "Leads & Solos",
    "Theremin": "Leads & Solos",
    "Whistle": "Leads & Solos",
    # 5. Brass & Strings
    "Brass Scoop": "Brass & Strings",
    "CS-80 Brass Lead": "Brass & Strings",
    "Jupiter Brass Pad": "Brass & Strings",
    "OB-8 Brass": "Brass & Strings",
    "OB-8 String Pad": "Brass & Strings",
    "Solina Strings": "Brass & Strings",
    # 7. Chiptune & 8-Bit
    "Square Lead 8-bit": "Chiptune & 8-Bit",
    # 8. Cinematic & Drones (was Pads/Sound FX bucket)
    "Ambient Drone": "Cinematic & Drones",
    "Dark Sub Rumble": "Cinematic & Drones",
    "Horror Pad": "Cinematic & Drones",
    "Horror Sync Ring": "Cinematic & Drones",
    "Impact Hit": "Cinematic & Drones",
    "Moog Hz Interval": "Cinematic & Drones",
    "Riser FX": "Cinematic & Drones",
    "Robot Voice": "Cinematic & Drones",
    "Sci-fi Laser": "Cinematic & Drones",
    "Sci-fi Zap R2-D2": "Cinematic & Drones",
    "Sub Drop FX": "Cinematic & Drones",
    "Wind Howl": "Cinematic & Drones",
    # 9. Synthwave & Vintage
    "JP-8000 Supersaw Pad": "Synthwave & Vintage",
    # Categories 6 (Arp & Sequencer Tones) and 10 (Modern EDM & Hyperpop)
    # are populated entirely by the Batch 5 new recipes below.
}

# 2026-04-25 (Batch 5 expansion): bass folders restructured to the doc's
# 10-category model.  User-confirmed mapping:
#   - 303 Acid Bass -> Acid & 303
#   - Moog Sub, Sub Bass -> Sub Bass & 808s
#   - Synthwave Bass -> Synthwave & Retrowave
#   - Picked Bass, Reggae Dub Bass -> Slap & Electric
#   - Moog Minimoog Bass -> Vintage Analog
#   - DX Bass FM, FM Growl Bass -> Pluck, Donk & FM
BASS_CATEGORIES = {
    # 1. Sub Bass & 808s
    "Sub Bass": "Sub Bass & 808s",
    "Moog Sub": "Sub Bass & 808s",
    # 2. Acid & 303
    "303 Acid Bass": "Acid & 303",
    # 4. Pluck, Donk & FM
    "DX Bass FM": "Pluck, Donk & FM",
    "FM Growl Bass": "Pluck, Donk & FM",
    # 5. Synthwave & Retrowave
    "Synthwave Bass": "Synthwave & Retrowave",
    # 9. Vintage Analog
    "Moog Minimoog Bass": "Vintage Analog",
    # 10. Slap & Electric
    "Picked Bass": "Slap & Electric",
    "Reggae Dub Bass": "Slap & Electric",
    # Categories 3 (Reese & Neuro), 6 (Dubstep & Wobble), 7 (Chiptune & 8-Bit),
    # 8 (Midtempo & Cyberpunk) populated entirely by Batch 5 new recipes below.
}

# ─── Harmless categories — single map, all 78 patches (2026-04-26) ───────────
# Each Harmless preset goes into Presets/Harmless/<category>/<name>.xml.
# 9 genre clusters derived from the YouTube reference list (Files For Claude/
# Preset Links.txt).  No factory Harmless presets pre-existed so no dedupe.
HARMLESS_CATEGORIES = {
    # Modern Hip-Hop (RTJ, Logic, Aesop Rock) — 8 layer + 3 bass
    "Boom-Bap Rhodes":          "Modern Hip-Hop",
    "Dusty EP":                 "Modern Hip-Hop",
    "Sample Chop Pluck":        "Modern Hip-Hop",
    "Soulful Pad":              "Modern Hip-Hop",
    "Vocal-Style Lead":         "Modern Hip-Hop",
    "Trap Bell":                "Modern Hip-Hop",
    "Dark Brooding Pad":        "Modern Hip-Hop",
    "Distorted Lo-Fi Lead":     "Modern Hip-Hop",
    "Boom-Bap 808":             "Modern Hip-Hop",
    "Hip-Hop Sub":              "Modern Hip-Hop",
    "Dirty Square Bass":        "Modern Hip-Hop",
    # Psytrance (Astrix, Infected Mushroom) — 8 layer + 4 bass
    "Supersaw Lead":            "Psytrance",
    "Trance Pluck":             "Psytrance",
    "FM Bell Stab":             "Psytrance",
    "Glitch Lead":              "Psytrance",
    "Acid Hoover":              "Psytrance",
    "Gated Arp Pad":            "Psytrance",
    "Tribal Pluck":             "Psytrance",
    "Hi-Pass Pad":              "Psytrance",
    "Acid Roll Bass":           "Psytrance",
    "Psytrance Sub Pulse":      "Psytrance",
    "Gated Bass Roll":          "Psytrance",
    "Reese Bass":               "Psytrance",
    # Psybient (Shpongle) — 6 layer + 2 bass
    "Ethereal Choir Pad":       "Psybient",
    "Evolving Drone":           "Psybient",
    "Glass Bell Drone":         "Psybient",
    "Tribal Flute Lead":        "Psybient",
    "Sitar-Style Pluck":        "Psybient",
    "Wood Pluck Atmos":         "Psybient",
    "Deep Drone Bass":          "Psybient",
    "Sub Hum":                  "Psybient",
    # Daft Punk (RAM) — 7 layer + 3 bass
    "Vocoder Pad":              "French Disco",
    "Disco String Stack":       "French Disco",
    "Talkbox Lead":             "French Disco",
    "Funky Pluck":              "French Disco",
    "Analog Brass":             "French Disco",
    "Warm Rhodes":              "French Disco",
    "Phaser Lead":              "French Disco",
    "Funky Synth Bass":         "French Disco",
    "Disco Sub":                "French Disco",
    "Plucky Bass":              "French Disco",
    # Kraftwerk (Autobahn) — 5 layer + 2 bass
    "ARP Lead":                 "Krautrock",
    "Vocoder Robot":            "Krautrock",
    "Ribbon Glide":             "Krautrock",
    "Motorik Pad":              "Krautrock",
    "Vintage String":           "Krautrock",
    "Pulse Sub Bass":           "Krautrock",
    "Sequenced Bass":           "Krautrock",
    # Deadmau5 (Random Album Title) — 5 layer + 2 bass
    "Sidechain Saw Lead":       "Prog House",
    "Plucky Stab":              "Prog House",
    "Wet Pad":                  "Prog House",
    "Bell Lead":                "Prog House",
    "Lasersaw Stab":            "Prog House",
    "Big Saw Bass":             "Prog House",
    "Plucked Sub":              "Prog House",
    # Skrillex (Quest For Fire) — 5 layer + 3 bass
    "Future Bass Chord":        "Bass Music",
    "Hyper Lead":               "Bass Music",
    "Aggressive Saw":           "Bass Music",
    "Resonant Lead":            "Bass Music",
    "Vocal Chop Pad":           "Bass Music",
    "Skrillex Reese":           "Bass Music",
    "Wobble Bass":              "Bass Music",
    "FM Growl":                 "Bass Music",
    # Depeche Mode (Violator) — 5 layer + 2 bass
    "Dark Minor Pad":           "Synth-Pop",
    "FM Plucky EP":             "Synth-Pop",
    "Industrial Lead":          "Synth-Pop",
    "Glassy Stab":              "Synth-Pop",
    "Choir Pad":                "Synth-Pop",
    "80s Synth Bass":           "Synth-Pop",
    "Filtered Saw Bass":        "Synth-Pop",
    # Neo-Soul / Vintage (Gnarls Barkley, Aretha Franklin) — 6 layer + 2 bass
    "Vintage Wurli":            "Neo-Soul",
    "Hammond B3 Smooth":        "Neo-Soul",
    "Harpsichord Stab":         "Neo-Soul",
    "Theremin Lead":            "Neo-Soul",
    "Mellotron Pad":            "Neo-Soul",
    "Soulful Rhodes":           "Neo-Soul",
    "Walking Synth Bass":       "Neo-Soul",
    "Vintage Round Bass":       "Neo-Soul",

    # ── 2026-04-26 (round 2): 74 type-based patches ──────────────────────────
    # Keys & Electric Pianos (8)
    "Bright FM Tines":          "Keys & Electric Pianos",
    "B3 Drawbar":               "Keys & Electric Pianos",
    "Phase EP":                 "Keys & Electric Pianos",
    "Toy Piano":                "Keys & Electric Pianos",
    "Glass Keys":               "Keys & Electric Pianos",
    "Crystal EP":               "Keys & Electric Pianos",
    "House Organ":              "Keys & Electric Pianos",
    "Synth Clav":               "Keys & Electric Pianos",
    # Plucks & Mallets (8)
    "Hard Pluck":               "Plucks & Mallets",
    "Soft Pluck":               "Plucks & Mallets",
    "Marimba Pluck":            "Plucks & Mallets",
    "FM Bell Pluck":            "Plucks & Mallets",
    "Crystal Pluck":            "Plucks & Mallets",
    "Tropical Pluck":           "Plucks & Mallets",
    "Ice Pluck":                "Plucks & Mallets",
    "Wood Pluck":               "Plucks & Mallets",
    # Pads & Atmospheres (10)
    "Glass Shimmer Pad":        "Pads & Atmospheres",
    "Warm Analog Pad":          "Pads & Atmospheres",
    "Vintage Strings Pad":      "Pads & Atmospheres",
    "Ocean Wave Pad":           "Pads & Atmospheres",
    "Lush Pad":                 "Pads & Atmospheres",
    "Lo-Fi Tape Pad":           "Pads & Atmospheres",
    "Vocal Choir Pad":          "Pads & Atmospheres",
    "Stadium Pad":              "Pads & Atmospheres",
    "Sweep Pad":                "Pads & Atmospheres",
    "Misty Pad":                "Pads & Atmospheres",
    # Leads & Solos (10)
    "PWM Lead":                 "Leads & Solos",
    "Sync Lead":                "Leads & Solos",
    "Flute Lead":               "Leads & Solos",
    "Whistle":                  "Leads & Solos",
    "Lasersaw":                 "Leads & Solos",
    "Acid Lead":                "Leads & Solos",
    "Bright Saw Lead":          "Leads & Solos",
    "Square Glide":             "Leads & Solos",
    "Crystal Lead":             "Leads & Solos",
    "Detuned Lead":             "Leads & Solos",
    # Brass & Strings (8)
    "Synth Brass":              "Brass & Strings",
    "Strings Ensemble":         "Brass & Strings",
    "Synth Cello":              "Brass & Strings",
    "Epic Horns":               "Brass & Strings",
    "Soft Choir":               "Brass & Strings",
    "Glass Choir":              "Brass & Strings",
    "OB-8 Brass":               "Brass & Strings",
    "Marcato":                  "Brass & Strings",
    # Arp & Sequencer (8)
    "Acid Arp":                 "Arp & Sequencer Tones",
    "Square Bounce":            "Arp & Sequencer Tones",
    "FM Arp":                   "Arp & Sequencer Tones",
    "Bell Arp":                 "Arp & Sequencer Tones",
    "Trance Gate":              "Arp & Sequencer Tones",
    "Modern Arp":               "Arp & Sequencer Tones",
    "Bubble Arp":               "Arp & Sequencer Tones",
    "Retro Arp":                "Arp & Sequencer Tones",
    # Chiptune & 8-Bit (6)
    "8-Bit Lead":               "Chiptune & 8-Bit",
    "Gameboy Pulse":            "Chiptune & 8-Bit",
    "Bell Chip":                "Chiptune & 8-Bit",
    "Square Chip":              "Chiptune & 8-Bit",
    "Bit-Crush Pad":            "Chiptune & 8-Bit",
    "Arcade Stab":              "Chiptune & 8-Bit",
    # Cinematic & Drones (8)
    "Sci-Fi Wash":              "Cinematic & Drones",
    "Wind Howl":                "Cinematic & Drones",
    "Sub Drop FX":              "Cinematic & Drones",
    "Riser FX":                 "Cinematic & Drones",
    "Tension Riser":            "Cinematic & Drones",
    "Cinematic Sweep":          "Cinematic & Drones",
    "Granular Texture":         "Cinematic & Drones",
    "Glass Bell Pad":           "Cinematic & Drones",
    # Synthwave & Vintage (4)
    "Outrun Lead":              "Synthwave & Vintage",
    "VHS Keys":                 "Synthwave & Vintage",
    "Neon Pluck":               "Synthwave & Vintage",
    "Retrowave Bell":           "Synthwave & Vintage",
    # Modern EDM & Hyperpop (4)
    "Festival Lead":            "Modern EDM & Hyperpop",
    "Future Bass":              "Modern EDM & Hyperpop",
    "Modern Pluck":             "Modern EDM & Hyperpop",
    "Hyper Glitch":             "Modern EDM & Hyperpop",
}

def categorized_dir(base_dir: Path, name: str, cat_map: dict) -> Path:
    """Returns base_dir/<category>/ when name is mapped, else base_dir/."""
    cat = cat_map.get(name)
    return (base_dir / cat) if cat else base_dir

# ---- Recipes ----------------------------------------------------------------
# (target_dir, engine, filename, overrides)
# All values are sourced from blueprint §P3 Preset Recipe Catalogue.
# Drum slot always sends MIDI 60 (C5 in FL convention) to the engine.
# transpose values shift the effective base; pEnv_amt adds semitones at attack.

DRUM_RECIPES = [
    # ── Kicks ────────────────────────────────────────────────────────────────
    ("808 Kick", {
        "waveform": W_SINE, "transpose": -24,
        "pEnv_amt": 24.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_decay": 0.35, "amp_sustain": 0.0,
        "trans_amount": 0.3, "trans_duration": 3.0, "trans_colour": 5000.0,
    }),
    ("909 Kick", {
        "waveform": W_SINE, "transpose": -24,
        "pEnv_amt": 24.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_decay": 0.35, "amp_sustain": 0.0,
        "trans_amount": 0.6, "trans_duration": 3.0, "trans_colour": 6000.0,
    }),
    ("606 Kick", {
        # Faster/tighter than 808 (line 981).
        "waveform": W_SINE, "transpose": -24,
        "pEnv_amt": 18.0, "pEnv_decay": 0.04, "pEnv_sustain": 0.0,
        "amp_decay": 0.20, "amp_sustain": 0.0,
    }),
    # ── Snares ───────────────────────────────────────────────────────────────
    ("808 Snare", {
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 1500.0, "flt_res": 0.5,
        "amp_attack": 0.001, "amp_decay": 0.15, "amp_sustain": 0.0,
    }),
    ("909 Snare", {
        # Pink noise + tuned osc mix (line 976).
        "waveform": W_SAWSAW, "modifier": 0.0,
        "noise": 0.4, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 2000.0, "flt_res": 0.4,
        "amp_attack": 0.001, "amp_decay": 0.18, "amp_sustain": 0.0,
    }),
    ("606 Snare", {
        # Drier than 808/909 (line 982).
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 1800.0, "flt_res": 0.4,
        "amp_attack": 0.001, "amp_decay": 0.10, "amp_sustain": 0.0,
    }),
    # ── Hi-Hats / Cymbals ────────────────────────────────────────────────────
    ("808 Closed Hat", {
        "noiseOnly": 1, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_HP, "flt_cutoff": 9000.0,
        "amp_attack": 0.001, "amp_decay": 0.08, "amp_sustain": 0.0,
    }),
    ("808 Open Hat", {
        "noiseOnly": 1, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_HP, "flt_cutoff": 10000.0,
        "amp_attack": 0.001, "amp_decay": 0.4, "amp_sustain": 0.0,
    }),
    ("909 Closed Hat", {
        "noiseOnly": 1, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_HP, "flt_cutoff": 7000.0,
        "amp_attack": 0.001, "amp_decay": 0.06, "amp_sustain": 0.0,
    }),
    ("909 Open Hat", {
        "noiseOnly": 1, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_HP, "flt_cutoff": 6000.0,
        "amp_attack": 0.001, "amp_decay": 0.35, "amp_sustain": 0.0,
    }),
    ("909 Ride Crash", {
        # Long noise BP decay (line 979).
        "noiseOnly": 1, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_BP, "flt_cutoff": 8000.0, "flt_res": 0.3,
        "amp_attack": 0.001, "amp_decay": 1.0, "amp_sustain": 0.0,
    }),
    # ── Toms ─────────────────────────────────────────────────────────────────
    ("808 Tom Hi", {
        # line 965: Sine, Transpose +2 (relative to natural), pEnv +12 / 80ms.
        "waveform": W_SINE, "transpose": -10,   # -12 + 2; deeper than nominal C5
        "pEnv_amt": 12.0, "pEnv_decay": 0.08, "pEnv_sustain": 0.0,
        "amp_decay": 0.25, "amp_sustain": 0.0,
    }),
    ("808 Tom Mid", {
        "waveform": W_SINE, "transpose": -12,
        "pEnv_amt": 10.0, "pEnv_decay": 0.10, "pEnv_sustain": 0.0,
        "amp_decay": 0.30, "amp_sustain": 0.0,
    }),
    ("808 Tom Lo", {
        "waveform": W_SINE, "transpose": -17,
        "pEnv_amt": 8.0, "pEnv_decay": 0.12, "pEnv_sustain": 0.0,
        "amp_decay": 0.40, "amp_sustain": 0.0,
    }),
    ("909 Tom Hi", {
        # Sine + pEnv +10 / 70ms + transient click (line 980).
        "waveform": W_SINE, "transpose": -10,
        "pEnv_amt": 10.0, "pEnv_decay": 0.07, "pEnv_sustain": 0.0,
        "amp_decay": 0.25, "amp_sustain": 0.0,
        "trans_amount": 0.3, "trans_duration": 2.0, "trans_colour": 4000.0,
    }),
    ("909 Tom Mid", {
        "waveform": W_SINE, "transpose": -12,
        "pEnv_amt": 10.0, "pEnv_decay": 0.07, "pEnv_sustain": 0.0,
        "amp_decay": 0.30, "amp_sustain": 0.0,
        "trans_amount": 0.3, "trans_duration": 2.0, "trans_colour": 4000.0,
    }),
    ("909 Tom Lo", {
        "waveform": W_SINE, "transpose": -17,
        "pEnv_amt": 10.0, "pEnv_decay": 0.07, "pEnv_sustain": 0.0,
        "amp_decay": 0.40, "amp_sustain": 0.0,
        "trans_amount": 0.3, "trans_duration": 2.0, "trans_colour": 4000.0,
    }),
    # ── Cowbell / Rim / Claves ───────────────────────────────────────────────
    ("808 Cowbell", {
        "waveform": W_SQUARESQUARE, "dualOscMode": DUAL_ABSHZ,
        "transpose": 10, "modifier": 0.60,
        "amp_attack": 0.001, "amp_decay": 0.20, "amp_sustain": 0.0,
    }),
    ("808 Rimshot", {
        # Square+Square abs Hz Modifier ~0.55 (~600 Hz), short decay (line 968).
        "waveform": W_SQUARESQUARE, "dualOscMode": DUAL_ABSHZ,
        "transpose": 8, "modifier": 0.55,
        "pEnv_amt": -3.0, "pEnv_decay": 0.02,
        "amp_attack": 0.001, "amp_decay": 0.04, "amp_sustain": 0.0,
    }),
    ("808 Claves", {
        # Square+Square abs Hz Modifier ~0.72 (~2.5 kHz), very short, transient.
        "waveform": W_SQUARESQUARE, "dualOscMode": DUAL_ABSHZ,
        "transpose": 12, "modifier": 0.72,
        "amp_attack": 0.001, "amp_decay": 0.015, "amp_sustain": 0.0,
        "trans_amount": 0.5, "trans_duration": 2.0, "trans_colour": 8000.0,
    }),
    # ── Hand percussion / Shakers ────────────────────────────────────────────
    ("808 Maraca", {
        # Pink noise BP ~5kHz (line 971).
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 5000.0, "flt_res": 0.4,
        "amp_attack": 0.001, "amp_decay": 0.05, "amp_sustain": 0.0,
    }),
    ("808 Handclap", {
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 1500.0, "flt_res": 0.5,
        "amp_attack": 0.001, "amp_decay": 0.30, "amp_sustain": 0.0,
        "burst_mode": 1, "burst_count": 4, "burst_spacing": 20.0,
    }),
    # ── Congas / Bongos / Tablas ─────────────────────────────────────────────
    ("808 Conga Hi", {
        "waveform": W_SINE, "transpose": -7,
        "pEnv_amt": 6.0, "pEnv_decay": 0.05, "pEnv_sustain": 0.0,
        "amp_decay": 0.20, "amp_sustain": 0.0,
    }),
    ("808 Conga Mid", {
        "waveform": W_SINE, "transpose": -12,
        "pEnv_amt": 5.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_decay": 0.25, "amp_sustain": 0.0,
    }),
    ("808 Conga Lo", {
        "waveform": W_SINE, "transpose": -16,
        "pEnv_amt": 4.0, "pEnv_decay": 0.08, "pEnv_sustain": 0.0,
        "amp_decay": 0.30, "amp_sustain": 0.0,
    }),
    ("Bongo Hi", {
        "waveform": W_SINE, "transpose": -2,
        "pEnv_amt": 5.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_decay": 0.15, "amp_sustain": 0.0,
    }),
    ("Bongo Lo", {
        "waveform": W_SINE, "transpose": -7,
        "pEnv_amt": 4.0, "pEnv_decay": 0.08, "pEnv_sustain": 0.0,
        "amp_decay": 0.20, "amp_sustain": 0.0,
    }),
    ("Tabla Hi", {
        # BELL Modifier ~0.7, +7 transpose, slight pEnv -4 (line 1011).
        "waveform": W_BELL, "modifier": 0.7, "transpose": 7,
        "pEnv_amt": -4.0, "pEnv_decay": 0.05,
        "amp_decay": 0.20, "amp_sustain": 0.0,
    }),
    ("Tabla Lo", {
        # Sine + small noise mix, transpose -5, pEnv +5 / 100ms (line 1012).
        "waveform": W_SINE, "transpose": -17,   # -12 + (-5)
        "noise": 0.2,
        "pEnv_amt": 5.0, "pEnv_decay": 0.10, "pEnv_sustain": 0.0,
        "amp_decay": 0.40, "amp_sustain": 0.0,
    }),
    ("Cabasa Shaker", {
        # Pink noise BP ~4kHz, BURST 3 spacing 25ms (line 1015).
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 4000.0, "flt_res": 0.3,
        "amp_attack": 0.001, "amp_decay": 0.08, "amp_sustain": 0.0,
        "burst_mode": 1, "burst_count": 3, "burst_spacing": 25.0,
    }),
    # ── Stick / Misc rhythmic ────────────────────────────────────────────────
    ("Stick-Hit Drum", {
        "noiseOnly": 1, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_BP, "flt_cutoff": 2000.0, "flt_res": 0.3,
        "amp_attack": 0.001, "amp_decay": 0.05, "amp_sustain": 0.0,
        "trans_amount": 0.8, "trans_duration": 2.0, "trans_colour": 10000.0,
    }),
    ("Flam Double-Hit", {
        # Any waveform + short decay + BURST 2 spacing 30ms (line 954).
        "waveform": W_SINE, "transpose": -12,
        "amp_attack": 0.001, "amp_decay": 0.08, "amp_sustain": 0.0,
        "burst_mode": 1, "burst_count": 2, "burst_spacing": 30.0,
    }),
    ("Tabla Roll", {
        # BELL high transpose, BURST 6 spacing 15ms (line 955).
        "waveform": W_BELL, "modifier": 0.5, "transpose": 7,
        "amp_attack": 0.001, "amp_decay": 0.06, "amp_sustain": 0.0,
        "burst_mode": 1, "burst_count": 6, "burst_spacing": 15.0,
    }),
    # ── Simmons (sine sweep signature) ───────────────────────────────────────
    ("Simmons Kick", {
        # SDS-V kick (line 985).
        "waveform": W_SINE, "transpose": -24,
        "pEnv_amt": 24.0, "pEnv_decay": 0.10, "pEnv_sustain": 0.0,
        "amp_decay": 0.30, "amp_sustain": 0.0,
        "trans_amount": 0.2, "trans_duration": 5.0, "trans_colour": 4000.0,
    }),
    ("Simmons Snare", {
        # Sine + noise mix ~0.4 Pink, slight LP, pEnv +12 / 80ms (line 986).
        "waveform": W_SINE, "transpose": -7,
        "noise": 0.4, "noiseColor": NOISE_PINK,
        "flt_type": FLT_LP, "flt_cutoff": 8000.0,
        "pEnv_amt": 12.0, "pEnv_decay": 0.08, "pEnv_sustain": 0.0,
        "amp_decay": 0.20, "amp_sustain": 0.0,
    }),
    ("Simmons Tom Hi", {
        # Sine + pEnv +12 / 120ms (line 987).
        "waveform": W_SINE, "transpose": -7,
        "pEnv_amt": 12.0, "pEnv_decay": 0.12, "pEnv_sustain": 0.0,
        "amp_decay": 0.35, "amp_sustain": 0.0,
    }),
    ("Simmons Tom Lo", {
        "waveform": W_SINE, "transpose": -17,
        "pEnv_amt": 12.0, "pEnv_decay": 0.12, "pEnv_sustain": 0.0,
        "amp_decay": 0.45, "amp_sustain": 0.0,
    }),
    ("Simmons SDS-7 Kick", {
        # Longer decay, deeper pEnv (line 988).
        "waveform": W_SINE, "transpose": -24,
        "pEnv_amt": 18.0, "pEnv_decay": 0.15, "pEnv_sustain": 0.0,
        "amp_decay": 0.45, "amp_sustain": 0.0,
        "trans_amount": 0.25, "trans_duration": 5.0, "trans_colour": 3500.0,
    }),
    # ── Yamaha FM drums (BELL-based) ─────────────────────────────────────────
    ("RX-11 Kick", {
        # BELL Modifier ~0.4 + pEnv +18 / 60ms (line 991).
        "waveform": W_BELL, "modifier": 0.4, "transpose": -24,
        "pEnv_amt": 18.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_decay": 0.25, "amp_sustain": 0.0,
        "trans_amount": 0.3, "trans_duration": 3.0, "trans_colour": 5000.0,
    }),
    ("RX-11 Snare", {
        # BELL Modifier ~0.7 + noise mix 0.3 Pink (line 992).
        "waveform": W_BELL, "modifier": 0.7,
        "noise": 0.3, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 2000.0, "flt_res": 0.3,
        "amp_attack": 0.001, "amp_decay": 0.18, "amp_sustain": 0.0,
    }),
    ("DX7 Woodblock", {
        # BELL ~0.8, +12 transpose, pEnv +8 / 30ms, very short (line 993).
        "waveform": W_BELL, "modifier": 0.8, "transpose": 12,
        "pEnv_amt": 8.0, "pEnv_decay": 0.03, "pEnv_sustain": 0.0,
        "amp_decay": 0.08, "amp_sustain": 0.0,
    }),
    ("DX7 Glass", {
        # BELL ~0.6, transpose +12+, long decay (line 994).
        "waveform": W_BELL, "modifier": 0.6, "transpose": 12,
        "amp_decay": 1.2, "amp_sustain": 0.1,
    }),
    ("DX7 Metal", {
        # BELL ~0.9, transient, decay 500ms (line 995).
        "waveform": W_BELL, "modifier": 0.9,
        "trans_amount": 0.4, "trans_duration": 2.0, "trans_colour": 10000.0,
        "amp_decay": 0.5, "amp_sustain": 0.0,
    }),
    # ── Tuned percussion ─────────────────────────────────────────────────────
    ("Glockenspiel", {
        # BELL ~0.4, +12, decay ~1s (line 998).
        "waveform": W_BELL, "modifier": 0.4, "transpose": 12,
        "amp_decay": 1.0, "amp_sustain": 0.1,
    }),
    ("Marimba", {
        # BELL ~0.3, decay ~600ms, slight pEnv -3 (line 999).
        "waveform": W_BELL, "modifier": 0.3,
        "pEnv_amt": -3.0, "pEnv_decay": 0.04,
        "amp_decay": 0.6, "amp_sustain": 0.0,
    }),
    ("Xylophone", {
        # BELL ~0.5, +7 transpose, decay 400ms (line 1000).
        "waveform": W_BELL, "modifier": 0.5, "transpose": 7,
        "amp_decay": 0.4, "amp_sustain": 0.0,
    }),
    ("Tubular Bells", {
        # BELL ~0.55, long release, full sustain (line 1001).
        "waveform": W_BELL, "modifier": 0.55,
        "amp_attack": 0.005, "amp_decay": 0.5, "amp_sustain": 0.7, "amp_release": 2.0,
    }),
    ("Celesta", {
        # BELL ~0.2 (sine-like), +12, moderate decay (line 1002).
        "waveform": W_BELL, "modifier": 0.2, "transpose": 12,
        "amp_decay": 0.5, "amp_sustain": 0.0,
    }),
    ("Triangle", {
        # Square+Square abs Hz Mod ~0.8 + Ring Mod, decay 2s (line 1003).
        "waveform": W_SQUARESQUARE, "dualOscMode": DUAL_ABSHZ,
        "transpose": 12, "modifier": 0.8,
        "ringMod": 1,
        "amp_decay": 2.0, "amp_sustain": 0.1,
    }),
    ("Tambourine", {
        # White noise HP ~6kHz + BURST 2 spacing 15ms, decay 200ms (line 1004).
        "noiseOnly": 1, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_HP, "flt_cutoff": 6000.0,
        "amp_attack": 0.001, "amp_decay": 0.20, "amp_sustain": 0.0,
        "burst_mode": 1, "burst_count": 2, "burst_spacing": 15.0,
    }),
    ("Agogo", {
        # Square+Square abs Hz Mod ~0.65 (~1.5kHz), short decay (line 1006).
        "waveform": W_SQUARESQUARE, "dualOscMode": DUAL_ABSHZ,
        "transpose": 10, "modifier": 0.65,
        "amp_attack": 0.001, "amp_decay": 0.08, "amp_sustain": 0.0,
    }),
    ("Rimshot Acoustic", {
        # BELL ~0.5, transient (line 1007).
        "waveform": W_BELL, "modifier": 0.5,
        "noise": 0.2, "noiseColor": NOISE_WHITE,
        "trans_amount": 0.7, "trans_duration": 2.0, "trans_colour": 6000.0,
        "amp_attack": 0.001, "amp_decay": 0.05, "amp_sustain": 0.0,
    }),
    ("Woodblock", {
        # BELL ~0.85, +5 transpose, very short (line 1008).
        "waveform": W_BELL, "modifier": 0.85, "transpose": 5,
        "amp_attack": 0.001, "amp_decay": 0.04, "amp_sustain": 0.0,
    }),
    ("Vibraphone Roll", {
        # BELL + gentle pEnv + BURST 8 spacing 40ms (line 951).
        "waveform": W_BELL, "modifier": 0.35,
        "pEnv_amt": 2.0, "pEnv_decay": 0.05,
        "amp_attack": 0.005, "amp_decay": 0.30, "amp_sustain": 0.4,
        "burst_mode": 1, "burst_count": 8, "burst_spacing": 40.0,
    }),
    # NOTE 2026-04-23: Impact Hit + Sub Drop FX moved to SYNTH_RECIPES (see
    # bottom of that list).  They were generated into BaySickDrums originally
    # but conceptually belong to BaySickSynth's Sound FX family.
]

SYNTH_RECIPES = [
    # ── Classic synth leads / pads ───────────────────────────────────────────
    ("Moog Lead Woop", {
        # SAW Mono + small pEnv + glide (line 935).
        "waveform": W_SAW, "voiceMode": VOICE_MONO,
        "glide": 0.05,
        "pEnv_amt": 4.0, "pEnv_decay": 0.04, "pEnv_sustain": 0.0,
    }),
    ("Detuned Supersaw Lead", {
        # SAW+SAW Musical, Modifier ~0.5 detune (line 936).
        "waveform": W_SAWSAW, "modifier": 0.5,
        "amp_attack": 0.005, "amp_sustain": 1.0,
    }),
    ("Classic 80s Sync Lead", {
        # SAW+SAW abs Hz, Sync ON (line 937).
        "waveform": W_SAWSAW, "dualOscMode": DUAL_ABSHZ,
        "modifier": 0.5, "oscSync": 1,
        "lfo_dest": LFO_DEST_OSCMOD, "lfo_amount": 0.4, "lfo_rate": 2.0,
    }),
    ("Brass Scoop", {
        # SAW+SAW + pEnv -7 / fast attack / 200ms decay (line 938).
        "waveform": W_SAWSAW,
        "pEnv_amt": -7.0, "pEnv_decay": 0.20, "pEnv_sustain": 0.0,
        "amp_attack": 0.005, "amp_sustain": 0.7,
    }),
    ("Moog Hz Interval", {
        # SAW+SQUARE Hz Offset (line 939).
        "waveform": W_SAWSQUARE, "dualOscMode": DUAL_HZOFFSET,
        "modifier": 0.5,
    }),
    ("Moog Lead", {
        # SAW Mono/Legato + glide + LP filter + filter env (line 1028).
        "waveform": W_SAW, "voiceMode": VOICE_LEGATO,
        "glide": 0.05,
        "flt_type": FLT_LP, "flt_cutoff": 1500.0, "flt_res": 0.4,
        "flt_env_amt": 0.5, "flt_decay": 0.20,
    }),
    ("CS-80 Brass Lead", {
        # SAW+SAW small detune, Polymode, BP/LP, slow attack 50ms (line 1029).
        "waveform": W_SAWSAW, "modifier": 0.45,
        "flt_type": FLT_LP, "flt_cutoff": 3000.0, "flt_res": 0.2,
        "amp_attack": 0.05, "amp_sustain": 0.9,
        "pEnv_amt": 2.0, "pEnv_decay": 0.05,
    }),
    ("OB-8 Brass", {
        # CS-80 + slower filter + would-use Unison (line 1030).
        "waveform": W_SAWSAW, "modifier": 0.45,
        "flt_type": FLT_LP, "flt_cutoff": 2000.0, "flt_res": 0.3,
        "flt_attack": 0.10,
        "amp_attack": 0.08, "amp_sustain": 0.9,
        "unison_voices": 3, "unison_detune": 0.3,
    }),
    ("DX Lead FM", {
        # BELL ~0.6, bright env, slight LFO vibrato (line 1033).
        "waveform": W_BELL, "modifier": 0.6,
        "amp_sustain": 0.7,
        "lfo_dest": LFO_DEST_PITCH, "lfo_amount": 0.05, "lfo_rate": 5.0,
    }),
    ("Whistle", {
        # Sine +12/+24 + LFO pitch vibrato + slow attack (line 1035).
        "waveform": W_SINE, "transpose": 12,
        "amp_attack": 0.10, "amp_sustain": 0.9,
        "lfo_dest": LFO_DEST_PITCH, "lfo_amount": 0.04, "lfo_rate": 6.0,
    }),
    ("Square Lead 8-bit", {
        # PULSE Modifier 0.5, Mono, fast env (line 1036).
        "waveform": W_PULSE, "modifier": 0.5, "voiceMode": VOICE_MONO,
        "amp_sustain": 0.9,
    }),
    ("PWM Lead", {
        # PULSE + LFO on modifier (line 1037).
        "waveform": W_PULSE, "modifier": 0.5,
        "lfo_dest": LFO_DEST_OSCMOD, "lfo_amount": 0.5, "lfo_rate": 2.0,
        "amp_sustain": 0.9,
    }),
    # ── Bells / metallic / sci-fi ────────────────────────────────────────────
    ("CS-80 Bell", {
        # Square+Square abs Hz, RING ON, low transpose (line 942).
        "waveform": W_SQUARESQUARE, "dualOscMode": DUAL_ABSHZ,
        "modifier": 0.5, "ringMod": 1,
        "transpose": -12,
        "amp_decay": 1.0, "amp_sustain": 0.2,
    }),
    ("Sci-fi Zap R2-D2", {
        # SAW+SAW abs Hz, RING, high modifier 2-10kHz, short env (line 943).
        "waveform": W_SAWSAW, "dualOscMode": DUAL_ABSHZ,
        "modifier": 0.85, "ringMod": 1,
        "amp_decay": 0.15, "amp_sustain": 0.0,
    }),
    ("Horror Sync Ring", {
        # SAW+SAW + Sync + Ring + LFO sweep on Modifier (line 944).
        "waveform": W_SAWSAW, "dualOscMode": DUAL_HZOFFSET,
        "modifier": 0.5, "oscSync": 1, "ringMod": 1,
        "lfo_dest": LFO_DEST_OSCMOD, "lfo_amount": 0.6, "lfo_rate": 0.5,
        "amp_sustain": 0.8,
    }),
    ("Doctor Who Theremin", {
        # SAW+SAW + Ring + abs Hz + LFO modifier (line 945).
        "waveform": W_SAWSAW, "dualOscMode": DUAL_ABSHZ,
        "modifier": 0.4, "ringMod": 1,
        "lfo_dest": LFO_DEST_OSCMOD, "lfo_amount": 0.3, "lfo_rate": 1.5,
        "amp_sustain": 0.8,
    }),
    # ── Keys / EP ────────────────────────────────────────────────────────────
    ("Hammond Organ", {
        # Sine, fast env (line 948).
        "waveform": W_SINE,
        "amp_attack": 0.005, "amp_sustain": 1.0, "amp_release": 0.10,
    }),
    ("Rhodes Wurli Strike", {
        # BELL or SAW+SQUARE + transient (line 949).
        "waveform": W_BELL, "modifier": 0.3,
        "trans_amount": 0.3, "trans_duration": 5.0, "trans_colour": 3000.0,
        "amp_decay": 0.4, "amp_sustain": 0.3,
    }),
    ("Theremin", {
        # Sine, Legato, glide, LFO pitch (line 950).
        "waveform": W_SINE, "voiceMode": VOICE_LEGATO,
        "glide": 0.10,
        "lfo_dest": LFO_DEST_PITCH, "lfo_amount": 0.05, "lfo_rate": 5.0,
        "amp_sustain": 0.9,
    }),
    ("Rhodes EP", {
        # SAW+SQUARE base or BELL ~0.3 + transient (line 1049).
        "waveform": W_BELL, "modifier": 0.3,
        "trans_amount": 0.3, "trans_duration": 4.0, "trans_colour": 3000.0,
        "amp_decay": 0.4, "amp_sustain": 0.3,
    }),
    ("Wurlitzer EP", {
        # BELL ~0.5 brighter + transient (line 1050).
        "waveform": W_BELL, "modifier": 0.5,
        "trans_amount": 0.35, "trans_duration": 4.0, "trans_colour": 3500.0,
        "amp_decay": 0.35, "amp_sustain": 0.2,
    }),
    ("DX EP", {
        # BELL ~0.55 + transient (line 1051).
        "waveform": W_BELL, "modifier": 0.55,
        "trans_amount": 0.4, "trans_duration": 3.0, "trans_colour": 3500.0,
        "amp_decay": 0.5, "amp_sustain": 0.2,
    }),
    ("Clavinet", {
        # PULSE ~0.3 + big transient + LP + very short (line 1052).
        "waveform": W_PULSE, "modifier": 0.3,
        "flt_type": FLT_LP, "flt_cutoff": 2500.0,
        "trans_amount": 0.6, "trans_duration": 2.0, "trans_colour": 5000.0,
        "amp_decay": 0.15, "amp_sustain": 0.0,
    }),
    ("Harpsichord", {
        # SAW+SQUARE + sharp transient pluck (line 1053).
        "waveform": W_SAWSQUARE,
        "trans_amount": 0.7, "trans_duration": 1.0, "trans_colour": 6000.0,
        "amp_decay": 0.6, "amp_sustain": 0.0,
    }),
    ("Hammond Drawbar", {
        # Sine fundamental, fast env (line 1054).
        "waveform": W_SINE,
        "amp_attack": 0.005, "amp_sustain": 1.0, "amp_release": 0.08,
    }),
    ("Farfisa Organ", {
        # PULSE ~0.5 + SAW layer would, fast env (line 1055).
        "waveform": W_PULSE, "modifier": 0.5,
        "amp_attack": 0.005, "amp_sustain": 1.0, "amp_release": 0.05,
    }),
    ("Vox Continental", {
        # Square+Square, fast env (line 1056).
        "waveform": W_SQUARESQUARE, "modifier": 0.5,
        "amp_attack": 0.005, "amp_sustain": 1.0, "amp_release": 0.05,
    }),
    # ── Pads ─────────────────────────────────────────────────────────────────
    ("Juno Warm Pad", {
        # SAW+SAW slight detune, slow attack 300ms, long release 1.5s (line 1040).
        "waveform": W_SAWSAW, "modifier": 0.45,
        "flt_type": FLT_LP, "flt_cutoff": 4000.0,
        "amp_attack": 0.30, "amp_sustain": 0.9, "amp_release": 1.5,
        "lfo_dest": LFO_DEST_FILTER, "lfo_amount": 0.2, "lfo_rate": 0.5,
    }),
    ("Jupiter Brass Pad", {
        # SAW+SAW + BELL layer-feel, medium attack, high sustain (line 1041).
        "waveform": W_SAWSAW, "modifier": 0.5,
        "amp_attack": 0.10, "amp_sustain": 0.9, "amp_release": 1.0,
    }),
    ("OB-8 String Pad", {
        # SAW+SQUARE, slow attack, long release, subtle pEnv (line 1042).
        "waveform": W_SAWSQUARE, "modifier": 0.4,
        "amp_attack": 0.30, "amp_sustain": 0.9, "amp_release": 1.5,
        "pEnv_amt": 1.0, "pEnv_decay": 0.30,
        "unison_voices": 3, "unison_detune": 0.25,
    }),
    ("JP-8000 Supersaw Pad", {
        # SUPERSAW slow attack long release (line 1043).
        "waveform": W_SUPERSAW,
        "amp_attack": 0.30, "amp_sustain": 0.9, "amp_release": 1.5,
    }),
    ("Solina Strings", {
        # SAW+SAW slight detune, full poly, slow attack (line 1044).
        "waveform": W_SAWSAW, "modifier": 0.45,
        "amp_attack": 0.20, "amp_sustain": 0.9, "amp_release": 1.0,
    }),
    ("Glass Pad", {
        # BELL ~0.3 slow attack/release, very clean (line 1045).
        "waveform": W_BELL, "modifier": 0.3,
        "amp_attack": 0.40, "amp_sustain": 0.9, "amp_release": 2.0,
    }),
    ("Ambient Drone", {
        # Sine or noise-Brown, very slow attack/release, LFO filter (line 1046).
        "waveform": W_SINE, "transpose": -12,
        "amp_attack": 1.0, "amp_sustain": 0.9, "amp_release": 3.0,
        "flt_type": FLT_LP, "flt_cutoff": 2000.0,
        "lfo_dest": LFO_DEST_FILTER, "lfo_amount": 0.3, "lfo_rate": 0.2,
    }),
    # ── Ambient / textural ───────────────────────────────────────────────────
    ("Ocean Pad", {
        # Brown noise LP ~500Hz with slow LFO, long attack/release (line 958).
        "noiseOnly": 1, "noiseColor": NOISE_BROWN,
        "flt_type": FLT_LP, "flt_cutoff": 500.0,
        "amp_attack": 1.0, "amp_sustain": 0.8, "amp_release": 2.5,
        "lfo_dest": LFO_DEST_FILTER, "lfo_amount": 0.4, "lfo_rate": 0.15,
    }),
    ("Wind Pad", {
        # Pink noise LP ~2kHz with LFO (line 959).
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_LP, "flt_cutoff": 2000.0,
        "amp_attack": 0.50, "amp_sustain": 0.7, "amp_release": 2.0,
        "lfo_dest": LFO_DEST_FILTER, "lfo_amount": 0.5, "lfo_rate": 0.3,
    }),
    ("Dark Sub Rumble", {
        # Brown noise LP ~200Hz, long release (line 960).
        "noiseOnly": 1, "noiseColor": NOISE_BROWN,
        "flt_type": FLT_LP, "flt_cutoff": 200.0,
        "amp_attack": 0.50, "amp_sustain": 0.8, "amp_release": 2.5,
    }),
    # ── Sound FX ─────────────────────────────────────────────────────────────
    ("Sci-fi Laser", {
        # Sine + pEnv +24 / 200ms drop + Filter LP sweep (line 1059).
        "waveform": W_SINE,
        "pEnv_amt": 24.0, "pEnv_decay": 0.20, "pEnv_sustain": 0.0,
        "flt_type": FLT_LP, "flt_cutoff": 5000.0,
        "flt_env_amt": -0.5, "flt_decay": 0.20,
        "amp_decay": 0.30, "amp_sustain": 0.0,
    }),
    ("Riser FX", {
        # Noise LP rising, slow Amp attack, LFO rate accel (line 1061).
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_LP, "flt_cutoff": 1000.0,
        "flt_env_amt": 0.8, "flt_attack": 2.0,
        "amp_attack": 1.5, "amp_sustain": 0.9, "amp_release": 0.10,
    }),
    ("Robot Voice", {
        # SAW+SAW abs Hz, RING ON, modulated modifier (line 1063).
        "waveform": W_SAWSAW, "dualOscMode": DUAL_ABSHZ,
        "modifier": 0.4, "ringMod": 1,
        "lfo_dest": LFO_DEST_OSCMOD, "lfo_amount": 0.4, "lfo_rate": 8.0,
        "amp_sustain": 0.8,
    }),
    ("Horror Pad", {
        # SAW+SAW + Sync + Ring, Hz Offset, slow LFO modifier (line 1064).
        "waveform": W_SAWSAW, "dualOscMode": DUAL_HZOFFSET,
        "modifier": 0.5, "oscSync": 1, "ringMod": 1,
        "lfo_dest": LFO_DEST_OSCMOD, "lfo_amount": 0.5, "lfo_rate": 0.3,
        "amp_attack": 0.30, "amp_sustain": 0.8, "amp_release": 1.5,
    }),
    ("Wind Howl", {
        # Pink/Brown noise LP with slow LFO, long env (line 1065).
        "noiseOnly": 1, "noiseColor": NOISE_BROWN,
        "flt_type": FLT_LP, "flt_cutoff": 1000.0,
        "amp_attack": 0.50, "amp_sustain": 0.8, "amp_release": 2.0,
        "lfo_dest": LFO_DEST_FILTER, "lfo_amount": 0.6, "lfo_rate": 0.2,
    }),
    # ── FX moved here from DRUM_RECIPES on 2026-04-23 (folder reorg) ─────────
    ("Impact Hit", {
        "waveform": W_SINE, "transpose": -36,
        "noise": 0.5, "noiseColor": NOISE_BROWN,
        "trans_amount": 1.0, "trans_duration": 10.0, "trans_colour": 3000.0,
        "amp_decay": 0.30, "amp_sustain": 0.0,
    }),
    ("Sub Drop FX", {
        "waveform": W_SINE, "transpose": -12,
        "pEnv_amt": -24.0, "pEnv_decay": 1.5, "pEnv_sustain": 0.0,
        "amp_decay": 1.5, "amp_release": 1.5,
    }),
]

BASS_RECIPES = [
    # All bass recipes go to the BaySickBass folder with bsb_ prefix.
    ("Sub Bass", {
        # Sine, transpose -12 or -24, slow release (line 921).
        "waveform": W_SINE, "transpose": -12,
        "voiceMode": VOICE_MONO,
        "amp_sustain": 0.9, "amp_release": 0.50,
    }),
    ("303 Acid Bass", {
        # SAW Mono glide, filter LP res high, filter env high (line 924).
        "waveform": W_SAW, "voiceMode": VOICE_MONO,
        "glide": 0.05,
        "flt_type": FLT_LP, "flt_cutoff": 800.0, "flt_res": 0.85,
        "flt_env_amt": 0.8, "flt_decay": 0.20,
        "amp_sustain": 0.9,
    }),
    ("Picked Bass", {
        # SAW low transpose + Transient AMT 0.4 / 2ms / 8kHz (line 925).
        "waveform": W_SAW, "transpose": -12,
        "voiceMode": VOICE_MONO,
        "trans_amount": 0.4, "trans_duration": 2.0, "trans_colour": 8000.0,
        "amp_decay": 0.40, "amp_sustain": 0.5,
    }),
    ("Moog Minimoog Bass", {
        # SAW+SAW Musical slight detune, Mono, Filter LP, filter env (line 1018).
        "waveform": W_SAWSAW, "modifier": 0.45, "voiceMode": VOICE_MONO,
        "flt_type": FLT_LP, "flt_cutoff": 1500.0, "flt_res": 0.5,
        "flt_env_amt": 0.5, "flt_decay": 0.20,
        "amp_sustain": 0.9,
    }),
    ("Moog Sub", {
        # Sine, transpose -12, slow release (line 1019).
        "waveform": W_SINE, "transpose": -12,
        "voiceMode": VOICE_MONO,
        "amp_sustain": 0.9, "amp_release": 0.50,
    }),
    ("DX Bass FM", {
        # BELL ~0.4, transpose -12, subtle pEnv -2 / 30ms (line 1021).
        "waveform": W_BELL, "modifier": 0.4, "transpose": -12,
        "voiceMode": VOICE_MONO,
        "pEnv_amt": -2.0, "pEnv_decay": 0.03,
        "amp_sustain": 0.9,
    }),
    ("Reggae Dub Bass", {
        # Sine, transpose -12, long release with short decay (line 1023).
        "waveform": W_SINE, "transpose": -12,
        "voiceMode": VOICE_MONO,
        "amp_decay": 0.30, "amp_sustain": 0.6, "amp_release": 0.80,
    }),
    ("Synthwave Bass", {
        # SAW+SAW Musical max detune, Poly, slight upward pEnv (line 1024).
        "waveform": W_SAWSAW, "modifier": 0.7,
        "pEnv_amt": 1.0, "pEnv_decay": 0.30,
        "amp_sustain": 0.9,
    }),
    ("FM Growl Bass", {
        # BELL ~0.75 aggressive, Filter LP slight, Mono, glide (line 1025).
        "waveform": W_BELL, "modifier": 0.75, "voiceMode": VOICE_MONO,
        "glide": 0.03,
        "flt_type": FLT_LP, "flt_cutoff": 4000.0, "flt_res": 0.3,
        "amp_sustain": 0.9,
    }),
]

# ═══════════════════════════════════════════════════════════════════════════
# Batch 5 expansion (2026-04-25): 100 new recipes per engine sourced from
# Files For Claude/{Drum,Synth,Bass} Patches.txt.  Each entry carries its
# own target category so the merge step doesn't have to look it up.
# ═══════════════════════════════════════════════════════════════════════════

# Format: (name, category, overrides_dict).  noiseColor floats from doc are
# rounded to nearest int (it's a 3-choice param 0/1/2 — White/Pink/Brown).
NEW_DRUM_RECIPES = [
    # 1. 808 Group
    ("Deep Sub Boom",       "808 Group", {"waveform": 10, "transpose": -24, "amp_decay": 1.5, "amp_release": 1.5}),
    ("Long 808 Trap Kick",  "808 Group", {"waveform": 10, "transpose": -24, "pEnv_amt": 24.0, "amp_decay": 2.5}),
    ("808-Style Snare",     "808 Group", {"waveform": 10, "transpose": 5, "noise": 0.4, "pEnv_amt": 24.0}),
    ("808 Cowbell",         "808 Group", {"waveform": 2, "dualOscMode": 1, "unison_detune": 0.6, "transpose": 7}),
    ("Gliding 808",         "808 Group", {"waveform": 10, "transpose": -24, "glide": 0.5, "voiceMode": 1}),
    ("High Conga 808",      "808 Group", {"waveform": 10, "transpose": 15, "pEnv_amt": 12.0, "amp_decay": 0.4}),
    ("Muffled 808 Bass",    "808 Group", {"waveform": 10, "transpose": -24, "amp_attack": 0.05, "pEnv_amt": 0.0}),
    ("Classic 808 Rimshot", "808 Group", {"waveform": 10, "transpose": 17, "pEnv_amt": 0.0, "trans_amount": 1.0}),
    ("808 Low Tom",         "808 Group", {"waveform": 10, "transpose": -12, "pEnv_amt": 18.0, "pEnv_decay": 0.2}),
    ("808 Mid Tom",         "808 Group", {"waveform": 10, "transpose": -5, "pEnv_amt": 18.0, "pEnv_decay": 0.15}),
    # 2. 909 Group
    ("909-Style Kick",      "909 Group", {"waveform": 10, "transpose": -12, "pEnv_amt": 24.0, "pEnv_decay": 0.08, "trans_amount": 0.8}),
    ("Classic Analog Snare","909 Group", {"waveform": 10, "noise": 0.8, "pEnv_amt": 12.0, "flt_cutoff": 8000.0}),
    ("Tight Closed Hat",    "909 Group", {"noiseOnly": 1, "noiseColor": 1, "amp_decay": 0.04, "flt_cutoff": 12000.0}),
    ("Open Hat",            "909 Group", {"noiseOnly": 1, "cutSelf": 1, "amp_decay": 0.4, "amp_release": 0.3}),
    ("Synthesized Clap",    "909 Group", {"noiseOnly": 1, "burst_mode": 1, "burst_count": 4, "burst_spacing": 12.0}),
    ("White Noise Crash",   "909 Group", {"noiseOnly": 1, "amp_decay": 1.8, "flt_type": 1, "flt_cutoff": 8000.0}),
    ("Synth Rimshot",       "909 Group", {"waveform": 10, "transpose": 10, "trans_amount": 1.0, "pEnv_amt": 24.0}),
    ("Pedal Hat (Chick)",   "909 Group", {"noiseOnly": 1, "noiseColor": 1, "amp_decay": 0.08, "flt_cutoff": 8000.0}),
    ("Deep House Thump",    "909 Group", {"waveform": 10, "transpose": -18, "pEnv_amt": 15.0, "trans_amount": 0.1}),
    ("909 High Tom",        "909 Group", {"waveform": 10, "transpose": 5, "noise": 0.2, "pEnv_amt": 20.0}),
    # 3. 606 Group
    ("Analog Metronome",    "606 Group", {"waveform": 10, "transpose": 12, "amp_decay": 0.01, "trans_amount": 1.0}),
    ("Electro Castanet",    "606 Group", {"waveform": 2, "transpose": 19, "burst_mode": 1, "burst_count": 2}),
    ("High Woodblock",      "606 Group", {"waveform": 2, "transpose": 24, "amp_decay": 0.04, "trans_amount": 0.8}),
    ("Master Sync Tick",    "606 Group", {"waveform": 10, "transpose": 36, "amp_decay": 0.001, "noiseOnly": 1}),
    ("606 Closed Hat",      "606 Group", {"noiseOnly": 1, "noiseColor": 1, "amp_decay": 0.03, "flt_type": 1}),
    ("606 Open Hat",        "606 Group", {"noiseOnly": 1, "noiseColor": 1, "amp_decay": 0.25, "cutSelf": 1}),
    ("606 Snare",           "606 Group", {"waveform": 10, "transpose": 8, "noise": 0.6, "amp_decay": 0.12}),
    ("606 Kick",            "606 Group", {"waveform": 10, "transpose": -10, "pEnv_amt": 24.0, "pEnv_decay": 0.05}),
    ("606 Low Tom",         "606 Group", {"waveform": 10, "transpose": -5, "pEnv_amt": 15.0, "amp_decay": 0.2}),
    ("606 High Tom",        "606 Group", {"waveform": 10, "transpose": 10, "pEnv_amt": 15.0, "amp_decay": 0.15}),
    # 4. Simmons Group
    ("Resonant Synth Tom",  "Simmons Group", {"waveform": 10, "transpose": -5, "pEnv_amt": 12.0, "pEnv_decay": 0.2}),
    ("Sci-Fi Laser Zap",    "Simmons Group", {"waveform": 0, "transpose": 24, "pEnv_amt": -24.0, "pEnv_decay": 0.05}),
    ("Simmons 80s Tom",     "Simmons Group", {"waveform": 0, "noise": 0.3, "pEnv_amt": 24.0, "pEnv_decay": 0.15}),
    ("Gated 80s Snare",     "Simmons Group", {"waveform": 10, "noise": 0.9, "amp_sustain": 0.4, "amp_release": 0.01}),
    ("Electro Pew-Pew",     "Simmons Group", {"waveform": 0, "transpose": 0, "pEnv_amt": -24.0, "pEnv_decay": 0.15}),
    ("Disco Syndrum",       "Simmons Group", {"waveform": 10, "transpose": 12, "pEnv_amt": -24.0, "pEnv_decay": 0.2}),
    ("Vintage Simmons Snare","Simmons Group",{"waveform": 0, "transpose": 5, "noise": 0.7, "pEnv_amt": -24.0}),
    ("Tom Thud (Dead Room)","Simmons Group", {"waveform": 10, "transpose": -7, "amp_decay": 0.1, "pEnv_amt": 10.0}),
    ("Simmons Low Tom",     "Simmons Group", {"waveform": 0, "transpose": -12, "noise": 0.2, "pEnv_amt": 24.0}),
    ("Simmons Sweep FX",    "Simmons Group", {"waveform": 0, "transpose": 12, "pEnv_amt": -24.0, "amp_decay": 1.0}),
    # 5. Yamaha Group
    ("FM Metallic Cowbell", "Yamaha Group", {"waveform": 2, "transpose": 12, "ringMod": 1, "unison_detune": 0.7}),
    ("Synthetic Ride Bell", "Yamaha Group", {"waveform": 10, "transpose": 24, "ringMod": 1, "trans_amount": 0.8}),
    ("Synth Gong",          "Yamaha Group", {"waveform": 2, "transpose": -15, "ringMod": 1, "amp_decay": 3.5}),
    ("Agogo Bell",          "Yamaha Group", {"waveform": 2, "transpose": 17, "ringMod": 1, "pEnv_amt": 0.0}),
    ("Shimmering Ride",     "Yamaha Group", {"waveform": 2, "transpose": 36, "ringMod": 1, "noise": 0.7}),
    ("Synthetic Triangle",  "Yamaha Group", {"waveform": 10, "transpose": 36, "amp_decay": 1.5, "trans_amount": 0.5}),
    ("FM Pluck Percussion", "Yamaha Group", {"waveform": 2, "transpose": 12, "ringMod": 1, "pEnv_amt": 5.0}),
    ("FM Digital Bell",     "Yamaha Group", {"waveform": 2, "transpose": 24, "ringMod": 1, "amp_decay": 0.8}),
    ("Gong Crash Mix",      "Yamaha Group", {"waveform": 2, "transpose": 12, "ringMod": 1, "unison_voices": 3}),
    ("DX Style Tubulum",    "Yamaha Group", {"waveform": 6, "transpose": -5, "ringMod": 1, "amp_decay": 0.5}),
    # 6. Hand Percussion
    ("Shaker",              "Hand Percussion", {"noiseOnly": 1, "noiseColor": 1, "amp_attack": 0.05, "amp_decay": 0.1}),
    ("Tambourine",          "Hand Percussion", {"noiseOnly": 1, "burst_mode": 1, "burst_count": 3, "burst_spacing": 25.0}),
    ("Electronic Bongo",    "Hand Percussion", {"waveform": 10, "transpose": 12, "pEnv_amt": 15.0, "pEnv_decay": 0.05}),
    ("Hollow Woodblock",    "Hand Percussion", {"waveform": 2, "transpose": 19, "amp_decay": 0.05, "pEnv_amt": 5.0}),
    ("Electronic Clave",    "Hand Percussion", {"waveform": 10, "transpose": 24, "amp_decay": 0.08, "trans_amount": 0.6}),
    ("Finger Snap",         "Hand Percussion", {"noiseOnly": 1, "burst_mode": 1, "burst_count": 2, "burst_spacing": 8.0}),
    ("Guiro Scrape",        "Hand Percussion", {"noiseOnly": 1, "burst_mode": 1, "burst_count": 8, "burst_spacing": 15.0}),
    ("Cabasa",              "Hand Percussion", {"noiseOnly": 1, "burst_mode": 1, "burst_count": 6, "burst_spacing": 12.0}),
    ("Stomp Kick",          "Hand Percussion", {"waveform": 10, "transpose": -18, "noise": 0.6, "flt_cutoff": 3000.0}),
    ("Sleigh Bells",        "Hand Percussion", {"noiseOnly": 1, "burst_mode": 1, "burst_count": 4, "flt_type": 1}),
    # 7. Tuned Percussion
    ("Tribal Log Drum",     "Tuned Percussion", {"waveform": 10, "transpose": -5, "amp_attack": 0.02, "pEnv_amt": 5.0}),
    ("Synthetic Marimba",   "Tuned Percussion", {"waveform": 10, "transpose": 12, "amp_attack": 0.02, "trans_amount": 0.4}),
    ("Deep Floor Tom",      "Tuned Percussion", {"waveform": 10, "transpose": -12, "pEnv_amt": 12.0, "pEnv_decay": 0.2}),
    ("Cricket Synth Perc",  "Tuned Percussion", {"waveform": 10, "transpose": 24, "burst_mode": 1, "pEnv_amt": 12.0}),
    ("Synthetic Tabla",     "Tuned Percussion", {"waveform": 10, "transpose": 10, "pEnv_amt": 8.0, "pEnv_decay": 0.15}),
    ("PVC Tube Strike",     "Tuned Percussion", {"waveform": 2, "transpose": -5, "flt_cutoff": 400.0, "flt_res": 0.9}),
    ("Steel Drum Perc",     "Tuned Percussion", {"waveform": 6, "transpose": 12, "ringMod": 1, "amp_decay": 0.4}),
    ("Tuned Kalimba",       "Tuned Percussion", {"waveform": 10, "trans_amount": 0.7, "amp_decay": 0.3, "pEnv_amt": 2.0}),
    ("Crystal Tom",         "Tuned Percussion", {"waveform": 6, "transpose": 0, "trans_amount": 0.5, "pEnv_amt": 5.0}),
    ("Wooden Mallet",       "Tuned Percussion", {"waveform": 2, "transpose": 5, "flt_cutoff": 1500.0, "trans_amount": 0.6}),
    # 8. Modern EDM & Trap
    ("Distorted Gabber Kick","Modern EDM & Trap",{"waveform": 2, "transpose": -24, "unison_detune": 0.1, "pEnv_amt": 24.0}),
    ("Trap Snare Roll",     "Modern EDM & Trap", {"waveform": 10, "transpose": 7, "burst_mode": 1, "burst_count": 4}),
    ("Dubstep Sub Impact",  "Modern EDM & Trap", {"waveform": 0, "transpose": -24, "flt_attack": 0.1, "flt_env_amt": 0.8}),
    ("Underwater Kick",     "Modern EDM & Trap", {"waveform": 10, "transpose": -24, "flt_cutoff": 150.0, "trans_amount": 0.0}),
    ("Donk Bass Perc",      "Modern EDM & Trap", {"waveform": 2, "transpose": -12, "pEnv_amt": 24.0, "flt_res": 0.8}),
    ("Psytrance Zap Kick",  "Modern EDM & Trap", {"waveform": 10, "transpose": -24, "pEnv_amt": 24.0, "pEnv_decay": 0.02}),
    ("Distorted Slap Clap", "Modern EDM & Trap", {"noiseOnly": 1, "unison_voices": 3, "unison_detune": 0.8, "burst_mode": 1}),
    ("Granular Smear Snare","Modern EDM & Trap", {"waveform": 10, "noise": 0.8, "amp_attack": 0.08, "trans_amount": 1.0}),
    ("Stuttering Snare Tail","Modern EDM & Trap",{"noiseOnly": 1, "burst_mode": 1, "burst_count": 8, "burst_spacing": 2.0}),
    ("Festival Big Room Kick","Modern EDM & Trap",{"waveform": 10, "transpose": -24, "pEnv_amt": 24.0, "trans_duration": 5.0}),
    # 9. Lo-Fi, Chiptune & Texture
    ("8-Bit Kick",          "Lo-Fi, Chiptune & Texture", {"waveform": 2, "transpose": -12, "pEnv_amt": 24.0, "flt_cutoff": 8000.0}),
    ("Vinyl Crackle Layer", "Lo-Fi, Chiptune & Texture", {"noiseOnly": 1, "noiseColor": 0, "amp_sustain": 0.3, "flt_cutoff": 2000.0}),
    ("White Noise Sizzle",  "Lo-Fi, Chiptune & Texture", {"noiseOnly": 1, "noiseColor": 1, "amp_decay": 0.25, "trans_amount": 0.0}),
    ("8-Bit Snare",         "Lo-Fi, Chiptune & Texture", {"waveform": 2, "transpose": 5, "noise": 0.7, "pEnv_amt": 12.0}),
    ("Crunch Snare",        "Lo-Fi, Chiptune & Texture", {"waveform": 10, "noise": 0.9, "noiseColor": 0, "flt_cutoff": 2500.0}),
    ("Trash Cymbal",        "Lo-Fi, Chiptune & Texture", {"waveform": 2, "transpose": 12, "noise": 0.6, "ringMod": 1}),
    ("Fake Reverb Snare Tail","Lo-Fi, Chiptune & Texture",{"noiseOnly": 1, "noiseColor": 0, "amp_attack": 0.05, "amp_decay": 1.2}),
    ("Lo-Fi Digital Shaker","Lo-Fi, Chiptune & Texture", {"noiseOnly": 1, "noiseColor": 0, "burst_mode": 1, "flt_cutoff": 3000.0}),
    ("Square Pop",          "Lo-Fi, Chiptune & Texture", {"waveform": 2, "transpose": 12, "pEnv_amt": 18.0, "flt_cutoff": 8000.0}),
    ("Glitch Hop Squash",   "Lo-Fi, Chiptune & Texture", {"noiseOnly": 1, "unison_voices": 4, "unison_detune": 1.0, "amp_decay": 0.1}),
    # 10. Cinematic, Industrial & FX
    ("Reverse Cymbal Sweep","Cinematic, Industrial & FX",{"noiseOnly": 1, "amp_attack": 1.5, "flt_attack": 1.5, "flt_env_amt": 0.5}),
    ("Cinematic Sub Drop",  "Cinematic, Industrial & FX",{"waveform": 10, "transpose": -12, "pEnv_amt": -24.0, "pEnv_decay": 3.0}),
    ("Riser Sweep FX",      "Cinematic, Industrial & FX",{"waveform": 0, "transpose": -24, "amp_attack": 4.0, "pEnv_amt": 24.0}),
    ("Water Plop",          "Cinematic, Industrial & FX",{"waveform": 10, "transpose": 15, "pEnv_amt": -24.0, "pEnv_decay": 0.04}),
    ("Reverse Suck FX",     "Cinematic, Industrial & FX",{"noise": 0.5, "amp_attack": 1.5, "pEnv_amt": -24.0, "pEnv_attack": 1.5}),
    ("Anvil Strike",        "Cinematic, Industrial & FX",{"waveform": 2, "ringMod": 1, "unison_voices": 3, "trans_amount": 1.0}),
    ("Reverse Snare Swell", "Cinematic, Industrial & FX",{"waveform": 10, "noise": 0.8, "amp_attack": 0.8, "flt_env_amt": 0.8}),
    ("Impact Downshifter",  "Cinematic, Industrial & FX",{"noiseOnly": 1, "amp_decay": 4.0, "flt_decay": 3.0, "flt_env_amt": -0.8}),
    ("Raygun FX",           "Cinematic, Industrial & FX",{"waveform": 0, "burst_mode": 1, "burst_count": 5, "pEnv_amt": -24.0}),
    ("Formant Vocal Uh!",   "Cinematic, Industrial & FX",{"waveform": 2, "transpose": -5, "flt_cutoff": 300.0, "flt_env_amt": 0.8}),

    # 2026-04-26: 6 drums filling gaps revealed by the YouTube reference list
    # (Files For Claude/Preset Links.txt). Daft Punk RAM, Depeche Mode Violator,
    # Kraftwerk Autobahn, Skrillex Quest For Fire, Shpongle, Astrix.
    ("Linn Disco Kick",     "909 Group", {
        "waveform": W_SINE, "transpose": -12,
        "pEnv_amt": 18.0, "pEnv_decay": 0.04, "pEnv_sustain": 0.0,
        "amp_decay": 0.25, "amp_sustain": 0.0,
        "trans_amount": 0.8, "trans_duration": 2.0, "trans_colour": 8000.0,
    }),
    ("Gated 80s Snare Long", "Simmons Group", {
        "noise": 0.6, "noiseColor": NOISE_PINK,
        "waveform": W_SAWSAW, "modifier": 0.0,
        "flt_type": FLT_BP, "flt_cutoff": 2500.0, "flt_res": 0.45,
        "flt_env_amt": 0.4, "flt_decay": 0.3,
        "amp_attack": 0.001, "amp_decay": 0.55, "amp_sustain": 0.0,
        "amp_release": 0.05,
    }),
    ("FM Robot Stab",       "Yamaha Group", {
        "waveform": W_SINE, "ringMod": 1, "transpose": 0,
        "pEnv_amt": 12.0, "pEnv_decay": 0.05, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.18, "amp_sustain": 0.0,
        "trans_amount": 0.4, "trans_duration": 4.0, "trans_colour": 6000.0,
    }),
    ("Bass Music Impact",   "Modern EDM & Trap", {
        "waveform": W_SAWSAW, "transpose": -12, "modifier": 0.3,
        "noise": 0.25, "noiseColor": NOISE_PINK,
        "pEnv_amt": 12.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.4, "amp_sustain": 0.0,
        "trans_amount": 0.7, "trans_duration": 3.0, "trans_colour": 4000.0,
        "flt_type": FLT_LP, "flt_cutoff": 4000.0,
    }),
    ("Tribal Pow Wow Drum", "Hand Percussion", {
        "waveform": W_SINE, "transpose": -18,
        "pEnv_amt": 14.0, "pEnv_decay": 0.08, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.45, "amp_sustain": 0.0,
        "trans_amount": 0.3, "trans_duration": 5.0, "trans_colour": 3500.0,
    }),
    ("Acid Tom Sweep",      "808 Group", {
        "waveform": W_SAW, "transpose": -7,
        "flt_type": FLT_LP, "flt_cutoff": 800.0, "flt_res": 0.85,
        "flt_env_amt": 0.5, "flt_decay": 0.18, "flt_sustain": 0.0,
        "pEnv_amt": 12.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.3, "amp_sustain": 0.0,
    }),

    # 2026-04-26 (round 2): 8 drums filling specific gaps in the catalog.
    ("Acoustic Live Kick",  "909 Group", {
        # Punchy real-feel kick — body resonance + woody attack click.
        "waveform": W_SINE, "transpose": -16,
        "noise": 0.1, "noiseColor": NOISE_BROWN,
        "pEnv_amt": 14.0, "pEnv_decay": 0.05, "pEnv_sustain": 0.0,
        "amp_decay": 0.30, "amp_sustain": 0.0,
        "trans_amount": 0.6, "trans_duration": 4.0, "trans_colour": 5500.0,
    }),
    ("Splash Cymbal",       "909 Group", {
        # Short bright crash — pink noise + high BP.
        "noiseOnly": 1, "noiseColor": NOISE_PINK,
        "flt_type": FLT_BP, "flt_cutoff": 9000.0, "flt_res": 0.4,
        "amp_attack": 0.001, "amp_decay": 0.25, "amp_sustain": 0.0,
        "amp_release": 0.4,
    }),
    ("China Cymbal",        "Modern EDM & Trap", {
        # Trashy modern crash — white noise + high notch + saw layer.
        "waveform": W_SAW, "transpose": 24, "noise": 0.6, "noiseColor": NOISE_WHITE,
        "flt_type": FLT_BP, "flt_cutoff": 6000.0, "flt_res": 0.5,
        "amp_attack": 0.001, "amp_decay": 0.6, "amp_sustain": 0.0,
        "amp_release": 0.5,
    }),
    ("Djembe",              "Hand Percussion", {
        # African hand drum — body slap with overtone ring.
        "waveform": W_SINE, "transpose": -12,
        "noise": 0.3, "noiseColor": NOISE_BROWN,
        "pEnv_amt": 12.0, "pEnv_decay": 0.05, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.35, "amp_sustain": 0.0,
        "trans_amount": 0.4, "trans_duration": 3.0, "trans_colour": 4500.0,
    }),
    ("Cajon Hit",           "Hand Percussion", {
        # Box-drum acoustic feel — punchy front / hollow body.
        "waveform": W_SINE, "transpose": -10,
        "noise": 0.25, "noiseColor": NOISE_BROWN,
        "pEnv_amt": 10.0, "pEnv_decay": 0.04, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.22, "amp_sustain": 0.0,
        "trans_amount": 0.5, "trans_duration": 3.0, "trans_colour": 4000.0,
    }),
    ("Frame Drum",          "Hand Percussion", {
        # Shallow tribal frame drum — sustained tone + soft attack.
        "waveform": W_SINE, "transpose": -14,
        "noise": 0.2, "noiseColor": NOISE_BROWN,
        "pEnv_amt": 8.0, "pEnv_decay": 0.06, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.5, "amp_sustain": 0.0,
    }),
    ("Metal Pipe Hit",      "Cinematic, Industrial & FX", {
        # Industrial metal-pipe strike — square + ring mod for inharmonics.
        "waveform": W_PULSE, "modifier": 0.7, "ringMod": 1,
        "transpose": 5,
        "amp_attack": 0.001, "amp_decay": 0.25, "amp_sustain": 0.0,
        "amp_release": 0.3,
        "trans_amount": 0.7, "trans_duration": 2.0, "trans_colour": 7000.0,
    }),
    ("Container Slam",      "Cinematic, Industrial & FX", {
        # Industrial container/dumpster impact — sub thump + resonant noise.
        "waveform": W_SAWSAW, "transpose": -18, "modifier": 0.4,
        "noise": 0.4, "noiseColor": NOISE_PINK,
        "pEnv_amt": 12.0, "pEnv_decay": 0.07, "pEnv_sustain": 0.0,
        "amp_attack": 0.001, "amp_decay": 0.6, "amp_sustain": 0.0,
        "trans_amount": 0.5, "trans_duration": 5.0, "trans_colour": 3000.0,
    }),
]

NEW_SYNTH_RECIPES = [
    # 1. Keys & Electric Pianos
    ("Vintage Rhodes",      "Keys & Electric Pianos", {"waveform": 10, "ringMod": 1, "amp_decay": 1.5, "amp_sustain": 0.3}),
    ("Wurli Tremolo",       "Keys & Electric Pianos", {"waveform": 2, "flt_cutoff": 1500.0, "amp_decay": 2.0}),
    ("FM Tines",            "Keys & Electric Pianos", {"waveform": 10, "ringMod": 1, "amp_decay": 0.5, "amp_sustain": 0.0}),
    ("B3 Organ",            "Keys & Electric Pianos", {"waveform": 10, "unison_voices": 3, "unison_detune": 0.0, "amp_sustain": 1.0}),
    ("Synth Clav",          "Keys & Electric Pianos", {"waveform": 2, "flt_cutoff": 3000.0, "flt_env_amt": 0.2, "flt_decay": 0.2}),
    ("Toy Piano",           "Keys & Electric Pianos", {"waveform": 10, "noise": 0.1, "amp_decay": 0.8}),
    ("Lo-Fi Keys",          "Keys & Electric Pianos", {"waveform": 10, "drift": 0.5, "noise": 0.2, "flt_cutoff": 2000.0}),
    ("House Organ",         "Keys & Electric Pianos", {"waveform": 2, "dualOscMode": 1, "transpose": 0, "amp_sustain": 1.0}),
    ("Crystal Keys",        "Keys & Electric Pianos", {"waveform": 10, "ringMod": 1, "amp_release": 2.0}),
    ("Phase EP",            "Keys & Electric Pianos", {"waveform": 0, "unison_voices": 2, "flt_cutoff": 1000.0, "drift": 0.3}),
    # 2. Plucks & Mallets
    ("Classic Trance Pluck","Plucks & Mallets", {"waveform": 0, "flt_cutoff": 200.0, "flt_env_amt": 0.6, "flt_decay": 0.15}),
    ("Marimba Pluck",       "Plucks & Mallets", {"waveform": 10, "pEnv_amt": 12.0, "pEnv_decay": 0.05, "amp_decay": 0.3}),
    ("Wood Pluck",          "Plucks & Mallets", {"waveform": 2, "flt_cutoff": 1500.0, "amp_decay": 0.15}),
    ("Harp Pluck",          "Plucks & Mallets", {"waveform": 10, "amp_decay": 0.8, "amp_release": 1.5}),
    ("FM Bell Pluck",       "Plucks & Mallets", {"waveform": 10, "ringMod": 1, "amp_decay": 0.5}),
    ("Pizzicato Synth",     "Plucks & Mallets", {"waveform": 0, "amp_decay": 0.08, "flt_cutoff": 3000.0}),
    ("Tropical Pluck",      "Plucks & Mallets", {"waveform": 2, "flt_type": 1, "amp_decay": 0.2}),
    ("Ice Pluck",           "Plucks & Mallets", {"waveform": 10, "noise": 0.4, "noiseColor": 2, "amp_decay": 0.2}),
    ("Analog Pluck",        "Plucks & Mallets", {"waveform": 0, "drift": 0.4, "flt_decay": 0.2}),
    ("Muted Pluck",         "Plucks & Mallets", {"waveform": 10, "flt_cutoff": 800.0, "amp_decay": 0.1}),
    # 3. Pads & Atmospheres
    ("Warm Analog Pad",     "Pads & Atmospheres", {"waveform": 0, "amp_attack": 1.5, "amp_release": 2.0, "flt_cutoff": 2000.0}),
    ("Glass Pad",           "Pads & Atmospheres", {"waveform": 10, "ringMod": 1, "amp_attack": 2.0, "amp_release": 3.0}),
    ("Sweep Pad",           "Pads & Atmospheres", {"waveform": 0, "flt_attack": 2.0, "flt_env_amt": 0.4}),
    ("Choir Pad",           "Pads & Atmospheres", {"waveform": 2, "flt_res": 0.8, "amp_attack": 1.0, "flt_cutoff": 1200.0}),
    ("Lo-Fi Tape Pad",      "Pads & Atmospheres", {"waveform": 2, "drift": 0.7, "noise": 0.3, "amp_attack": 0.8}),
    ("Dark Ambient Pad",    "Pads & Atmospheres", {"waveform": 10, "transpose": -12, "flt_cutoff": 500.0, "amp_attack": 3.0}),
    ("Shimmer Pad",         "Pads & Atmospheres", {"waveform": 0, "unison_voices": 4, "flt_type": 1, "amp_attack": 2.0}),
    ("Space Pad",           "Pads & Atmospheres", {"waveform": 2, "drift": 0.9, "amp_attack": 4.0, "amp_release": 4.0}),
    ("Ocean Pad",           "Pads & Atmospheres", {"waveform": 0, "noiseOnly": 1, "flt_attack": 5.0, "amp_attack": 3.0}),
    ("Vintage String Pad",  "Pads & Atmospheres", {"waveform": 0, "unison_voices": 3, "unison_spread": 0.8, "amp_attack": 1.2}),
    # 4. Leads & Solos
    ("Moog Style Lead",     "Leads & Solos", {"waveform": 0, "voiceMode": 1, "glide": 0.4, "flt_cutoff": 3000.0}),
    ("Sine Glide Lead",     "Leads & Solos", {"waveform": 10, "voiceMode": 1, "glide": 0.6}),
    ("Distorted Lead",      "Leads & Solos", {"waveform": 0, "unison_voices": 2, "unison_detune": 0.3, "flt_cutoff": 8000.0}),
    ("Square Lead",         "Leads & Solos", {"waveform": 2, "drift": 0.5, "voiceMode": 1}),
    ("Sync Lead",           "Leads & Solos", {"waveform": 0, "oscSync": 1, "flt_env_amt": 0.4}),
    ("Flute Lead",          "Leads & Solos", {"waveform": 10, "noise": 0.3, "amp_attack": 0.05}),
    ("Theremin Lead",       "Leads & Solos", {"waveform": 10, "drift": 0.8, "glide": 0.9, "voiceMode": 1}),
    ("R&B Glide Lead",      "Leads & Solos", {"waveform": 10, "glide": 0.7, "voiceMode": 1, "flt_cutoff": 2000.0}),
    ("EDM Saw Lead",        "Leads & Solos", {"waveform": 0, "unison_voices": 4, "unison_detune": 0.2, "amp_release": 0.3}),
    ("Acid Lead",           "Leads & Solos", {"waveform": 0, "flt_res": 0.85, "flt_env_amt": 0.6, "flt_decay": 0.3}),
    # 5. Brass & Strings
    ("80s Synth Brass",     "Brass & Strings", {"waveform": 0, "amp_attack": 0.1, "flt_env_amt": 0.3, "flt_attack": 0.1}),
    ("Vangelis Brass",      "Brass & Strings", {"waveform": 0, "amp_attack": 0.3, "flt_cutoff": 4000.0, "amp_release": 1.5}),
    ("Orchestral Strings",  "Brass & Strings", {"waveform": 0, "unison_voices": 4, "amp_attack": 0.4, "amp_release": 1.0}),
    ("Chamber Strings",     "Brass & Strings", {"waveform": 10, "unison_voices": 2, "amp_attack": 0.2}),
    ("Epic Horns",          "Brass & Strings", {"waveform": 2, "flt_cutoff": 6000.0, "amp_attack": 0.15}),
    ("Marcato Strings",     "Brass & Strings", {"waveform": 0, "amp_attack": 0.05, "amp_decay": 0.5, "amp_sustain": 0.6}),
    ("Muted Brass",         "Brass & Strings", {"waveform": 0, "flt_cutoff": 1000.0, "flt_env_amt": 0.15, "flt_decay": 0.2}),
    ("Synth Cello",         "Brass & Strings", {"waveform": 0, "transpose": -12, "drift": 0.4, "amp_attack": 0.2}),
    ("Lo-Fi Brass",         "Brass & Strings", {"waveform": 0, "drift": 0.6, "noise": 0.2, "flt_cutoff": 2000.0}),
    ("Future Brass",        "Brass & Strings", {"waveform": 0, "unison_detune": 0.3, "pEnv_amt": 12.0, "pEnv_decay": 0.05}),
    # 6. Arp & Sequencer Tones
    ("Short Saw Arp",       "Arp & Sequencer Tones", {"waveform": 0, "amp_decay": 0.15, "amp_sustain": 0.0}),
    ("Square Bounce",       "Arp & Sequencer Tones", {"waveform": 2, "flt_env_amt": 0.4, "flt_decay": 0.1}),
    ("FM Arp",              "Arp & Sequencer Tones", {"waveform": 10, "ringMod": 1, "amp_decay": 0.2, "amp_sustain": 0.0}),
    ("Acid Arp",            "Arp & Sequencer Tones", {"waveform": 0, "flt_res": 0.8, "flt_decay": 0.15, "voiceMode": 1}),
    ("Bubble Arp",          "Arp & Sequencer Tones", {"waveform": 10, "pEnv_amt": 24.0, "pEnv_decay": 0.05, "amp_decay": 0.1}),
    ("Muted Arp",           "Arp & Sequencer Tones", {"waveform": 0, "flt_cutoff": 800.0, "amp_decay": 0.1}),
    ("Bell Arp",            "Arp & Sequencer Tones", {"waveform": 10, "transpose": 12, "ringMod": 1, "amp_decay": 0.3}),
    ("Noise Arp",           "Arp & Sequencer Tones", {"waveform": 0, "noiseOnly": 1, "flt_res": 0.9, "flt_decay": 0.1}),
    ("Trance Gate Arp",     "Arp & Sequencer Tones", {"waveform": 0, "amp_attack": 0.0, "amp_decay": 0.1, "amp_release": 0.0}),
    ("Retro Arp",           "Arp & Sequencer Tones", {"waveform": 10, "drift": 0.3, "flt_cutoff": 3000.0}),
    # 7. Chiptune & 8-Bit
    ("8-Bit Lead",          "Chiptune & 8-Bit", {"waveform": 2, "amp_release": 0.0, "flt_cutoff": 20000.0}),
    ("Gameboy Pulse",       "Chiptune & 8-Bit", {"waveform": 2, "amp_decay": 0.2, "amp_sustain": 0.5}),
    ("SID Chip Arp",        "Chiptune & 8-Bit", {"waveform": 2, "burst_mode": 1, "burst_spacing": 5.0}),
    ("8-Bit Flute",         "Chiptune & 8-Bit", {"waveform": 10, "transpose": 12, "amp_attack": 0.02}),
    ("Chiptune Chords",     "Chiptune & 8-Bit", {"waveform": 2, "unison_voices": 3, "amp_decay": 0.5}),
    ("16-Bit Strings",      "Chiptune & 8-Bit", {"waveform": 0, "flt_cutoff": 6000.0, "amp_attack": 0.2}),
    ("Bitcrush Pad",        "Chiptune & 8-Bit", {"waveform": 2, "noise": 0.4, "amp_attack": 1.0, "amp_release": 1.0}),
    ("Retro Laser Lead",    "Chiptune & 8-Bit", {"waveform": 0, "pEnv_amt": -24.0, "pEnv_decay": 0.1}),
    ("Bleep Synth",         "Chiptune & 8-Bit", {"waveform": 10, "amp_decay": 0.05, "amp_sustain": 0.0}),
    ("Arcade Victory",      "Chiptune & 8-Bit", {"waveform": 2, "glide": 0.8, "transpose": 12}),
    # 8. Cinematic & Drones
    ("Tension Drone",       "Cinematic & Drones", {"waveform": 10, "transpose": -24, "drift": 0.9, "amp_sustain": 1.0}),
    ("Evolving Texture",    "Cinematic & Drones", {"waveform": 2, "flt_attack": 8.0, "flt_env_amt": 0.5}),
    ("Dystopian Siren",     "Cinematic & Drones", {"waveform": 0, "unison_detune": 0.8, "glide": 1.0, "amp_attack": 2.0}),
    ("Abyss Drone",         "Cinematic & Drones", {"waveform": 10, "noise": 0.5, "flt_cutoff": 400.0, "amp_attack": 4.0}),
    ("Shimmer Drone",       "Cinematic & Drones", {"waveform": 0, "flt_type": 1, "unison_voices": 4, "amp_attack": 3.0}),
    ("Granular Smear",      "Cinematic & Drones", {"waveform": 10, "noiseOnly": 1, "burst_mode": 1, "amp_attack": 2.0}),
    ("Dark Matter",         "Cinematic & Drones", {"waveform": 2, "transpose": -12, "flt_cutoff": 800.0, "drift": 0.6}),
    ("Sci-Fi Wash",         "Cinematic & Drones", {"waveform": 10, "ringMod": 1, "amp_attack": 3.0, "amp_release": 5.0}),
    ("Horror Squeak",       "Cinematic & Drones", {"waveform": 10, "transpose": 36, "unison_detune": 0.9}),
    ("Angelic Drone",       "Cinematic & Drones", {"waveform": 10, "unison_voices": 3, "unison_spread": 1.0, "amp_attack": 4.0}),
    # 9. Synthwave & Vintage
    ("Juno Poly",           "Synthwave & Vintage", {"waveform": 0, "unison_voices": 2, "unison_spread": 0.6, "amp_release": 0.8}),
    ("Jupiter Brass",       "Synthwave & Vintage", {"waveform": 0, "flt_attack": 0.1, "flt_env_amt": 0.4}),
    ("Outrun Lead",         "Synthwave & Vintage", {"waveform": 2, "glide": 0.3, "flt_cutoff": 5000.0}),
    ("VHS Keys",            "Synthwave & Vintage", {"waveform": 10, "drift": 0.8, "flt_cutoff": 2000.0, "amp_decay": 1.0}),
    ("80s Pop Pluck",       "Synthwave & Vintage", {"waveform": 0, "flt_decay": 0.15, "flt_env_amt": 0.5}),
    ("Synthwave Pad",       "Synthwave & Vintage", {"waveform": 0, "amp_attack": 1.0, "flt_cutoff": 1500.0}),
    ("Retrowave Bell",      "Synthwave & Vintage", {"waveform": 10, "ringMod": 1, "amp_decay": 0.8}),
    ("Analog Strings",      "Synthwave & Vintage", {"waveform": 0, "drift": 0.5, "amp_attack": 0.6, "amp_release": 1.5}),
    ("Miami Arp",           "Synthwave & Vintage", {"waveform": 2, "amp_decay": 0.2, "flt_cutoff": 4000.0}),
    ("Neon Lead",           "Synthwave & Vintage", {"waveform": 0, "flt_res": 0.7, "glide": 0.2}),
    # 10. Modern EDM & Hyperpop
    ("Supersaw Chords",     "Modern EDM & Hyperpop", {"waveform": 5, "unison_voices": 4, "unison_detune": 0.6, "unison_spread": 1.0}),
    ("Hyperpop Lead",       "Modern EDM & Hyperpop", {"waveform": 2, "glide": 0.8, "transpose": 12, "flt_cutoff": 15000.0}),
    ("Future Bass Chord",   "Modern EDM & Hyperpop", {"waveform": 0, "unison_voices": 4, "flt_attack": 0.1, "flt_decay": 0.2}),
    ("Metallic Pluck",      "Modern EDM & Hyperpop", {"waveform": 0, "ringMod": 1, "amp_decay": 0.15}),
    ("Slap House Lead",     "Modern EDM & Hyperpop", {"waveform": 2, "pEnv_amt": 12.0, "pEnv_decay": 0.05, "amp_decay": 0.3}),
    ("Festival Lead",       "Modern EDM & Hyperpop", {"waveform": 0, "unison_detune": 0.4, "pEnv_amt": 8.0, "pEnv_decay": 0.1}),
    ("Glitch Lead",         "Modern EDM & Hyperpop", {"waveform": 0, "burst_mode": 1, "burst_spacing": 10.0}),
    ("Lasersaw",            "Modern EDM & Hyperpop", {"waveform": 0, "pEnv_amt": -24.0, "pEnv_decay": 0.15}),
    ("Crystal Chords",      "Modern EDM & Hyperpop", {"waveform": 10, "noise": 0.3, "unison_voices": 3, "amp_release": 1.0}),
    ("EDM Pluck",           "Modern EDM & Hyperpop", {"waveform": 0, "trans_amount": 0.8, "flt_decay": 0.1, "flt_env_amt": 0.8}),
]

NEW_BASS_RECIPES = [
    # 1. Sub Bass & 808s
    ("Pure Sine Sub",       "Sub Bass & 808s", {"waveform": 10, "transpose": -24, "amp_decay": 2.0, "amp_sustain": 1.0}),
    ("Filtered Pulse Sub",  "Sub Bass & 808s", {"waveform": 2, "transpose": -24, "flt_cutoff": 800.0, "amp_release": 0.8}),
    ("Punchy 808 Bass",     "Sub Bass & 808s", {"waveform": 10, "transpose": -24, "pEnv_amt": 12.0, "pEnv_decay": 0.1}),
    ("Gliding Sub",         "Sub Bass & 808s", {"waveform": 10, "transpose": -24, "voiceMode": 1, "glide": 0.6}),
    ("Saturation Sub",      "Sub Bass & 808s", {"waveform": 10, "transpose": -24, "unison_voices": 2, "unison_detune": 0.05}),
    ("Deep House Sub",      "Sub Bass & 808s", {"waveform": 2, "flt_type": 0, "flt_cutoff": 250.0, "amp_decay": 0.5}),
    ("Sub Thump",           "Sub Bass & 808s", {"waveform": 10, "trans_amount": 0.8, "trans_duration": 5.0, "amp_decay": 1.0}),
    ("Hovering Sub",        "Sub Bass & 808s", {"waveform": 10, "drift": 0.5, "amp_attack": 0.5, "amp_sustain": 1.0}),
    ("R&B Glide Bass",      "Sub Bass & 808s", {"waveform": 10, "transpose": -24, "voiceMode": 1, "glide": 0.8}),
    ("Cinema Rumble Sub",   "Sub Bass & 808s", {"waveform": 10, "transpose": -36, "unison_detune": 0.4, "amp_release": 3.0}),
    # 2. Acid & 303
    ("Classic 303 Saw",     "Acid & 303", {"waveform": 0, "flt_cutoff": 200.0, "flt_res": 0.8, "flt_env_amt": 0.5, "flt_decay": 0.3}),
    ("Classic 303 Pulse",   "Acid & 303", {"waveform": 2, "flt_cutoff": 200.0, "flt_res": 0.8, "flt_env_amt": 0.5, "flt_decay": 0.3}),
    ("Rubber Acid",         "Acid & 303", {"waveform": 0, "flt_res": 0.95, "flt_env_amt": 0.8, "flt_decay": 0.15}),
    ("Hollow Acid",         "Acid & 303", {"waveform": 2, "transpose": -12, "flt_res": 0.7, "flt_env_amt": 0.3}),
    ("Distorted 303",       "Acid & 303", {"waveform": 0, "unison_voices": 2, "unison_detune": 0.1, "flt_res": 0.85}),
    ("Plucky Acid",         "Acid & 303", {"waveform": 0, "amp_decay": 0.1, "amp_sustain": 0.0, "flt_decay": 0.1}),
    ("Acid Glide",          "Acid & 303", {"waveform": 2, "glide": 0.7, "voiceMode": 1, "flt_res": 0.8}),
    ("Squelch Bass",        "Acid & 303", {"waveform": 0, "flt_cutoff": 100.0, "flt_res": 0.9, "flt_env_amt": 1.0}),
    ("Dark Acid",           "Acid & 303", {"waveform": 0, "flt_cutoff": 50.0, "flt_res": 0.6, "flt_env_amt": 0.2}),
    ("Buzzing Acid",        "Acid & 303", {"waveform": 2, "ringMod": 1, "flt_res": 0.8, "flt_decay": 0.2}),
    # 3. Reese & Neuro
    ("Standard Reese",      "Reese & Neuro", {"waveform": 0, "unison_voices": 3, "unison_detune": 0.6, "flt_cutoff": 4000.0}),
    ("Wide Reese",          "Reese & Neuro", {"waveform": 0, "unison_voices": 4, "unison_detune": 0.8, "unison_spread": 1.0}),
    ("Dark Neuro",          "Reese & Neuro", {"waveform": 2, "unison_voices": 3, "flt_cutoff": 800.0, "flt_env_amt": -0.5}),
    ("Phasing Reese",       "Reese & Neuro", {"waveform": 0, "unison_voices": 2, "unison_detune": 0.3, "drift": 0.8}),
    ("Growl Bass",          "Reese & Neuro", {"waveform": 0, "unison_voices": 3, "flt_cutoff": 200.0, "flt_env_amt": 0.4}),
    ("Square Reese",        "Reese & Neuro", {"waveform": 2, "unison_voices": 3, "unison_detune": 0.5}),
    ("Sub-Reese Blend",     "Reese & Neuro", {"waveform": 0, "dualOscMode": 1, "unison_voices": 3}),
    ("High-Pass Neuro",     "Reese & Neuro", {"waveform": 0, "flt_type": 1, "flt_cutoff": 200.0, "unison_detune": 0.7}),
    ("Vowel Reese",         "Reese & Neuro", {"waveform": 0, "flt_res": 0.6, "flt_env_amt": 0.2, "flt_attack": 0.5}),
    ("Muffled Reese",       "Reese & Neuro", {"waveform": 0, "unison_voices": 4, "flt_cutoff": 1000.0, "amp_attack": 0.2}),
    # 4. Pluck, Donk & FM
    ("Classic Donk",        "Pluck, Donk & FM", {"waveform": 2, "flt_cutoff": 1500.0, "flt_res": 0.8, "pEnv_amt": 24.0}),
    ("FM Metallic Pluck",   "Pluck, Donk & FM", {"waveform": 10, "ringMod": 1, "amp_decay": 0.2, "amp_sustain": 0.0}),
    ("Deep House Pluck",    "Pluck, Donk & FM", {"waveform": 2, "flt_cutoff": 100.0, "flt_env_amt": 0.3, "flt_decay": 0.15}),
    ("Organ Bass",          "Pluck, Donk & FM", {"waveform": 10, "unison_voices": 2, "unison_detune": 0.0, "transpose": -12}),
    ("Hollow Wood Pluck",   "Pluck, Donk & FM", {"waveform": 2, "amp_decay": 0.1, "flt_cutoff": 2000.0}),
    ("Garage Sub Pluck",    "Pluck, Donk & FM", {"waveform": 10, "trans_amount": 0.5, "amp_decay": 0.4, "amp_sustain": 0.0}),
    ("Tech House Stub",     "Pluck, Donk & FM", {"waveform": 0, "flt_cutoff": 150.0, "flt_env_amt": 0.4, "flt_decay": 0.08}),
    ("Rubber Band Bass",    "Pluck, Donk & FM", {"waveform": 10, "pEnv_amt": 12.0, "pEnv_decay": 0.1, "flt_res": 0.5}),
    ("Bell Bass",           "Pluck, Donk & FM", {"waveform": 10, "ringMod": 1, "amp_decay": 1.0, "transpose": -12}),
    ("Marimba Bass",        "Pluck, Donk & FM", {"waveform": 10, "trans_amount": 0.4, "amp_decay": 0.3, "amp_release": 0.3}),
    # 5. Synthwave & Retrowave
    ("80s Drive Saw",       "Synthwave & Retrowave", {"waveform": 0, "unison_voices": 2, "unison_detune": 0.15, "flt_cutoff": 2500.0}),
    ("Outrun Pulse",        "Synthwave & Retrowave", {"waveform": 2, "flt_cutoff": 800.0, "flt_env_amt": 0.2, "flt_decay": 0.3}),
    ("Cyber-Slap",          "Synthwave & Retrowave", {"waveform": 0, "trans_amount": 0.6, "amp_decay": 0.2, "amp_sustain": 0.5}),
    ("Chorus Bass",         "Synthwave & Retrowave", {"waveform": 0, "unison_voices": 3, "unison_detune": 0.4, "unison_spread": 0.8}),
    ("Analog Moog Style",   "Synthwave & Retrowave", {"waveform": 0, "flt_cutoff": 500.0, "flt_res": 0.3, "drift": 0.5}),
    ("Italo Disco Bass",    "Synthwave & Retrowave", {"waveform": 2, "amp_decay": 0.2, "amp_sustain": 0.0, "flt_cutoff": 5000.0}),
    ("Warm Pulse",          "Synthwave & Retrowave", {"waveform": 2, "flt_cutoff": 1200.0, "drift": 0.3}),
    ("Retrowave Sub",       "Synthwave & Retrowave", {"waveform": 10, "unison_voices": 2, "unison_detune": 0.1, "amp_sustain": 1.0}),
    ("Resonant Sweep",      "Synthwave & Retrowave", {"waveform": 0, "flt_res": 0.6, "flt_attack": 0.1, "flt_env_amt": 0.3}),
    ("Synth-Brass Bass",    "Synthwave & Retrowave", {"waveform": 0, "flt_attack": 0.05, "flt_decay": 0.4, "flt_env_amt": 0.4}),
    # 6. Dubstep & Wobble
    ("Classic Wobble",      "Dubstep & Wobble", {"waveform": 0, "flt_cutoff": 200.0, "flt_attack": 0.2, "flt_env_amt": 0.5}),
    ("Fast Wub",            "Dubstep & Wobble", {"waveform": 2, "flt_attack": 0.05, "flt_decay": 0.05, "flt_env_amt": 0.6}),
    ("Yoi Bass",            "Dubstep & Wobble", {"waveform": 0, "flt_res": 0.8, "flt_attack": 0.1, "flt_env_amt": 0.4}),
    ("Screech Bass",        "Dubstep & Wobble", {"waveform": 0, "ringMod": 1, "flt_env_amt": 0.8, "flt_decay": 0.4}),
    ("Pulse Wobble",        "Dubstep & Wobble", {"waveform": 2, "unison_voices": 2, "flt_attack": 0.15, "flt_decay": 0.15}),
    ("Dirty Wub",           "Dubstep & Wobble", {"waveform": 0, "noise": 0.3, "flt_attack": 0.1, "flt_env_amt": 0.5}),
    ("Sub Wobble",          "Dubstep & Wobble", {"waveform": 10, "flt_cutoff": 100.0, "flt_attack": 0.2, "flt_env_amt": 0.1}),
    ("Laser Wobble",        "Dubstep & Wobble", {"waveform": 0, "pEnv_amt": -24.0, "pEnv_decay": 0.2, "flt_attack": 0.1}),
    ("Heavy Formant Wub",   "Dubstep & Wobble", {"waveform": 0, "flt_res": 0.9, "flt_attack": 0.3, "flt_env_amt": 0.3}),
    ("Stutter Wub",         "Dubstep & Wobble", {"waveform": 0, "burst_mode": 1, "burst_count": 4, "flt_env_amt": 0.4}),
    # 7. Chiptune & 8-Bit
    ("Gameboy Pulse",       "Chiptune & 8-Bit", {"waveform": 2, "flt_cutoff": 20000.0, "amp_release": 0.0}),
    ("8-Bit Bell",          "Chiptune & 8-Bit", {"waveform": 6, "flt_cutoff": 20000.0, "amp_decay": 0.2}),
    ("Arcade Pluck",        "Chiptune & 8-Bit", {"waveform": 2, "amp_decay": 0.1, "amp_sustain": 0.0}),
    ("Boss Battle Bass",    "Chiptune & 8-Bit", {"waveform": 2, "unison_voices": 2, "unison_detune": 0.05}),
    ("Bit-Crush Sub",       "Chiptune & 8-Bit", {"waveform": 10, "noise": 0.2, "noiseColor": 2}),
    ("8-Bit Glide",         "Chiptune & 8-Bit", {"waveform": 2, "glide": 0.2, "voiceMode": 1}),
    ("Retro Zap Bass",      "Chiptune & 8-Bit", {"waveform": 2, "pEnv_amt": -12.0, "pEnv_decay": 0.05}),
    ("Chiptune Drone",      "Chiptune & 8-Bit", {"waveform": 2, "amp_sustain": 1.0, "drift": 0.0}),
    ("SID Chip Bass",       "Chiptune & 8-Bit", {"waveform": 2, "ringMod": 1, "amp_decay": 0.2}),
    ("Pixel Slap",          "Chiptune & 8-Bit", {"waveform": 2, "trans_amount": 1.0, "amp_sustain": 0.5}),
    # 8. Midtempo & Cyberpunk
    ("Cyber Drive",         "Midtempo & Cyberpunk", {"waveform": 0, "unison_voices": 3, "unison_detune": 0.8, "flt_cutoff": 3000.0}),
    ("Hard Sync Bass",      "Midtempo & Cyberpunk", {"waveform": 0, "oscSync": 1, "flt_env_amt": 0.6}),
    ("Industrial Grind",    "Midtempo & Cyberpunk", {"waveform": 2, "unison_voices": 4, "ringMod": 1}),
    ("Dystopian Sub",       "Midtempo & Cyberpunk", {"waveform": 10, "noise": 0.4, "unison_detune": 0.2}),
    ("Doom Saw",            "Midtempo & Cyberpunk", {"waveform": 0, "transpose": -36, "flt_res": 0.4, "flt_env_amt": 0.2}),
    ("Cyberpunk Pluck",     "Midtempo & Cyberpunk", {"waveform": 0, "amp_decay": 0.15, "flt_env_amt": 0.8}),
    ("Metallic Tear",       "Midtempo & Cyberpunk", {"waveform": 0, "ringMod": 1, "flt_attack": 0.1}),
    ("Rezz Style Bass",     "Midtempo & Cyberpunk", {"waveform": 2, "unison_voices": 2, "unison_detune": 0.1, "glide": 0.5}),
    ("Distorted Wub",       "Midtempo & Cyberpunk", {"waveform": 0, "unison_detune": 0.7, "flt_attack": 0.2, "flt_env_amt": 0.5}),
    ("Grime Bass",          "Midtempo & Cyberpunk", {"waveform": 2, "flt_cutoff": 800.0, "pEnv_amt": 12.0, "pEnv_decay": 0.1}),
    # 9. Vintage Analog
    ("70s Moog Bass",       "Vintage Analog", {"waveform": 0, "flt_cutoff": 800.0, "flt_res": 0.2, "drift": 0.6}),
    ("Analog Pulse",        "Vintage Analog", {"waveform": 2, "flt_cutoff": 2000.0, "drift": 0.4}),
    ("Fat Juno Sub",        "Vintage Analog", {"waveform": 2, "flt_cutoff": 400.0, "unison_voices": 2, "unison_detune": 0.1}),
    ("Smooth Glide",        "Vintage Analog", {"waveform": 0, "glide": 0.6, "voiceMode": 1, "flt_cutoff": 1000.0}),
    ("Warm Saw Pluck",      "Vintage Analog", {"waveform": 0, "flt_env_amt": 0.3, "flt_decay": 0.3, "amp_sustain": 0.2}),
    ("Prophet Sync",        "Vintage Analog", {"waveform": 0, "oscSync": 1, "flt_cutoff": 1500.0}),
    ("Dusty Bass",          "Vintage Analog", {"waveform": 0, "noise": 0.1, "noiseColor": 2, "flt_cutoff": 600.0}),
    ("Resonant Analog",     "Vintage Analog", {"waveform": 2, "flt_res": 0.7, "flt_decay": 0.4}),
    ("Vintage Round Bass",  "Vintage Analog", {"waveform": 10, "flt_cutoff": 300.0, "amp_attack": 0.02}),
    ("Classic Electro Bass","Vintage Analog", {"waveform": 0, "flt_env_amt": 0.4, "flt_decay": 0.2, "amp_release": 0.2}),
    # 10. Slap & Electric
    ("Synth Slap",          "Slap & Electric", {"waveform": 2, "trans_amount": 0.8, "flt_env_amt": 0.3, "flt_decay": 0.1}),
    ("Picked Bass",         "Slap & Electric", {"waveform": 0, "trans_amount": 0.6, "amp_decay": 0.5, "amp_sustain": 0.2}),
    ("Muted Electric",      "Slap & Electric", {"waveform": 10, "trans_amount": 0.3, "amp_decay": 0.2, "amp_sustain": 0.0}),
    ("Fretless Glide",      "Slap & Electric", {"waveform": 10, "glide": 0.8, "voiceMode": 1, "amp_attack": 0.05}),
    ("Funk Pop Bass",       "Slap & Electric", {"waveform": 2, "pEnv_amt": 5.0, "pEnv_decay": 0.02, "trans_amount": 0.5}),
    ("Distorted Electric",  "Slap & Electric", {"waveform": 0, "unison_voices": 2, "flt_cutoff": 4000.0, "trans_amount": 0.4}),
    ("String Buzz Bass",    "Slap & Electric", {"waveform": 0, "ringMod": 1, "amp_decay": 0.6}),
    ("Upright Jazz Sub",    "Slap & Electric", {"waveform": 10, "trans_amount": 0.2, "amp_decay": 1.5, "amp_sustain": 0.0}),
    ("Heavy Pick",          "Slap & Electric", {"waveform": 2, "trans_amount": 1.0, "flt_env_amt": 0.2, "flt_decay": 0.1}),
    ("Chorus Slap",         "Slap & Electric", {"waveform": 2, "unison_voices": 2, "trans_amount": 0.7, "amp_decay": 0.4}),
]

# ─── Merge with dedupe ──────────────────────────────────────────────────────
# Compare each new recipe against the existing recipe list (same engine).
# Match key = name (case-sensitive).  When names match, compare the params
# the new recipe specifies against the existing's overrides.  If all the
# new recipe's specified params equal the existing's values (within float
# tolerance) -> SKIP_DUP.  Otherwise -> KEEP_BOTH (suffix the new with
# " (2)").  When name doesn't match -> ADD as-is.

def _params_match(new_overrides: dict, existing_overrides: dict) -> bool:
    """True when every specified param in new_overrides has the same effective
    value in existing_overrides (default-aware)."""
    for k, v in new_overrides.items():
        ev = existing_overrides.get(k, DEFAULTS.get(k))
        if ev is None:
            return False
        try:
            if abs(float(v) - float(ev)) > 1e-3:
                return False
        except (ValueError, TypeError):
            if v != ev:
                return False
    return True

# ─── Harmless recipes (2026-04-26) ───────────────────────────────────────────
# 55 layer + 23 bass = 78 patches.  Genre clusters from Files For Claude/
# Preset Links.txt — see HARMLESS_CATEGORIES for the cluster->folder map.
# Patches lean on Harmless's strengths: additive timbres (Sine/Saw/Square/Tri),
# spectral modules (Prism/Pluck/Blur/Filter Mask/Phaser Mask/Brownian), dual
# SVF + ADSR + LFO + Trem + Vibrato + output Phaser.

# (name, overrides)  — every recipe writes overrides on top of HARM_DEFAULTS.
HARMLESS_RECIPES = [
    # ── Modern Hip-Hop (RTJ, Logic, Aesop Rock) — 8 layer + 3 bass ──────────
    ("Boom-Bap Rhodes", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE,
        "partB_level": 0.7, "amp_a": 0.02, "amp_d": 0.6, "amp_s": 0.4, "amp_r": 0.6,
        "flt_cutoff": 4000.0, "ophaser_mix": 0.2,
    }),
    ("Dusty EP", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "unison_voices": 2, "unison_detune": 8.0,
        "amp_a": 0.05, "amp_d": 0.4, "amp_s": 0.3, "amp_r": 0.5,
        "flt_cutoff": 2200.0, "brownian_amount": 0.85,
    }),
    ("Sample Chop Pluck", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.4, "prism_amount": 0.3,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
        "flt_cutoff": 5500.0,
    }),
    ("Soulful Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.5,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.9, "amp_r": 2.5,
        "flt_cutoff": 4500.0, "blur_size": 0.4, "blur_time": 1.4,
    }),
    ("Vocal-Style Lead", {
        "timbre_shape": HT_SINE, "glide_time": 0.4, "vib_depth": 0.6, "vib_speed": 5.5,
        "cutSelf": 1, "amp_a": 0.05, "amp_d": 0.3, "amp_s": 0.7, "amp_r": 0.4,
        "flt_cutoff": 6000.0,
    }),
    ("Trap Bell", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.6, "prism_mode": 1,
        "amp_a": 0.001, "amp_d": 0.6, "amp_s": 0.0, "amp_r": 0.7,
    }),
    ("Dark Brooding Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "unison_voices": 3, "unison_detune": 18.0,
        "amp_a": 1.2, "amp_d": 0.8, "amp_s": 0.85, "amp_r": 2.0,
        "flt_cutoff": 800.0, "flt_res": 0.4, "brownian_amount": 0.9,
    }),
    ("Distorted Lo-Fi Lead", {
        "timbre_shape": HT_SQUARE, "rm_clip": 0.5, "rm_prot": 0.3,
        "amp_a": 0.005, "amp_d": 0.3, "amp_s": 0.5, "amp_r": 0.3,
        "flt_cutoff": 3500.0, "flt_res": 0.5,
    }),
    ("Boom-Bap 808", {
        "timbre_shape": HT_SINE, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.45, "amp_s": 0.0, "amp_r": 0.5,
        "flt_cutoff": 350.0,
    }),
    ("Hip-Hop Sub", {
        "timbre_shape": HT_SINE, "pitch_semitones": -24.0,
        "amp_a": 0.001, "amp_d": 0.6, "amp_s": 0.8, "amp_r": 0.4,
        "flt_cutoff": 200.0,
    }),
    ("Dirty Square Bass", {
        "timbre_shape": HT_SQUARE, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.6, "amp_r": 0.3,
        "flt_cutoff": 900.0, "flt_res": 0.5,
    }),

    # ── Psytrance (Astrix, Infected Mushroom) — 8 layer + 4 bass ────────────
    ("Supersaw Lead", {
        "timbre_shape": HT_SAW, "unison_voices": 7, "unison_detune": 30.0,
        "unison_spread": 1.0, "amp_a": 0.005, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.3,
        "flt_cutoff": 12000.0,
    }),
    ("Trance Pluck", {
        "timbre_shape": HT_SQUARE, "pluck_decay": 0.6,
        "amp_a": 0.001, "amp_d": 0.18, "amp_s": 0.0, "amp_r": 0.15,
        "flt_cutoff": 6000.0, "flt_env_amt": 0.5, "flt_d": 0.15,
    }),
    ("FM Bell Stab", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.8, "prism_mode": 2,
        "amp_a": 0.001, "amp_d": 0.4, "amp_s": 0.0, "amp_r": 0.4,
    }),
    ("Glitch Lead", {
        "timbre_shape": HT_SAW, "blur_size": 0.6, "prism_amount": 0.5,
        "phaser_mask_rate": 4.5,
        "amp_a": 0.005, "amp_d": 0.3, "amp_s": 0.7, "amp_r": 0.3,
    }),
    ("Acid Hoover", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SQUARE, "partB_level": 0.7,
        "flt_res": 0.85, "flt_env_amt": 0.4, "flt_d": 0.4,
        "glide_time": 0.5, "vib_depth": 0.3, "vib_speed": 6.0,
        "amp_s": 0.7, "amp_r": 0.4,
    }),
    ("Gated Arp Pad", {
        "timbre_shape": HT_SAW, "trem_depth": 1.0, "trem_speed": 8.0, "trem_shape": 2,
        "amp_a": 0.05, "amp_d": 0.3, "amp_s": 0.8, "amp_r": 0.4,
        "flt_cutoff": 5000.0,
    }),
    ("Tribal Pluck", {
        "timbre_shape": HT_TRIANGLE, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.2, "amp_s": 0.0, "amp_r": 0.2,
        "ophaser_mix": 0.3,
    }),
    ("Hi-Pass Pad", {
        "timbre_shape": HT_SAW, "flt1_type": HF_HP, "flt_cutoff": 1500.0,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 2.0,
        "brownian_amount": 0.7, "blur_size": 0.3,
    }),
    ("Acid Roll Bass", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "flt_res": 0.9, "flt_env_amt": 0.5, "flt_d": 0.18,
        "glide_time": 0.05, "amp_s": 0.4, "amp_r": 0.2,
    }),
    ("Psytrance Sub Pulse", {
        "timbre_shape": HT_SINE, "pitch_semitones": -12.0,
        "trem_depth": 0.6, "trem_speed": 8.0, "trem_shape": 2,
        "amp_s": 0.85,
    }),
    ("Gated Bass Roll", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "trem_depth": 0.9, "trem_speed": 16.0, "trem_shape": 2,
        "flt_cutoff": 1200.0, "amp_s": 0.7,
    }),
    ("Reese Bass", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.8,
        "unison_voices": 3, "unison_detune": 35.0,
        "pitch_semitones": -12.0, "flt_cutoff": 700.0,
        "amp_s": 0.8, "amp_r": 0.4,
    }),

    # ── Psybient (Shpongle) — 6 layer + 2 bass ──────────────────────────────
    ("Ethereal Choir Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.7,
        "amp_a": 3.0, "amp_d": 1.0, "amp_s": 0.95, "amp_r": 4.0,
        "blur_size": 0.5, "blur_time": 1.6, "flt_cutoff": 6000.0,
    }),
    ("Evolving Drone", {
        "timbre_shape": HT_SAW, "vib_depth": 0.8, "vib_speed": 0.3,
        "amp_a": 2.0, "amp_d": 0.5, "amp_s": 0.95, "amp_r": 5.0,
        "brownian_amount": 0.85, "blur_size": 0.4,
    }),
    ("Glass Bell Drone", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.7, "prism_mode": 2,
        "amp_a": 0.5, "amp_d": 1.5, "amp_s": 0.6, "amp_r": 3.0,
        "blur_size": 0.3,
    }),
    ("Tribal Flute Lead", {
        "timbre_shape": HT_SINE, "vib_depth": 0.5, "vib_speed": 4.5,
        "glide_time": 0.2, "amp_a": 0.05, "amp_d": 0.2, "amp_s": 0.85, "amp_r": 0.4,
        "flt_cutoff": 3500.0,
    }),
    ("Sitar-Style Pluck", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.3, "prism_amount": 0.4,
        "vib_depth": 0.4, "vib_speed": 6.0,
        "amp_a": 0.001, "amp_d": 0.5, "amp_s": 0.0, "amp_r": 0.6,
    }),
    ("Wood Pluck Atmos", {
        "timbre_shape": HT_TRIANGLE, "prism_amount": 0.5,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.3,
        "rm_clip": 0.2,
    }),
    ("Deep Drone Bass", {
        "timbre_shape": HT_SINE, "pitch_semitones": -24.0,
        "amp_a": 1.0, "amp_d": 0.5, "amp_s": 1.0, "amp_r": 3.0,
        "flt_cutoff": 250.0,
    }),
    ("Sub Hum", {
        "timbre_shape": HT_SINE, "pitch_semitones": -24.0,
        "vib_depth": 0.3, "vib_speed": 0.4,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 1.0, "amp_r": 2.5,
        "flt_cutoff": 100.0,
    }),

    # ── Daft Punk (RAM) — 7 layer + 3 bass ─────────────────────────────────
    ("Vocoder Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 3, "unison_detune": 12.0,
        "amp_a": 0.6, "amp_d": 0.4, "amp_s": 0.85, "amp_r": 1.5,
        "ophaser_mix": 0.7, "ophaser_rate": 0.4, "blur_size": 0.3,
    }),
    ("Disco String Stack", {
        "timbre_shape": HT_SAW, "unison_voices": 5, "unison_detune": 25.0,
        "unison_spread": 0.9, "amp_a": 0.2, "amp_d": 0.4, "amp_s": 0.85, "amp_r": 0.8,
        "flt_cutoff": 6500.0,
    }),
    ("Talkbox Lead", {
        "timbre_shape": HT_SAW, "glide_time": 0.5, "vib_depth": 0.4, "vib_speed": 5.5,
        "ophaser_mix": 0.7, "ophaser_rate": 3.5, "ophaser_depth": 0.7,
        "amp_a": 0.05, "amp_s": 0.75, "amp_r": 0.4, "flt_cutoff": 4500.0,
    }),
    ("Funky Pluck", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.0, "amp_r": 0.25,
        "flt_cutoff": 5000.0,
    }),
    ("Analog Brass", {
        "timbre_shape": HT_SAW, "amp_a": 0.3, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.6,
        "flt_cutoff": 5500.0, "flt_a": 0.2, "flt_env_amt": 0.3,
        "brownian_amount": 0.9,
    }),
    ("Warm Rhodes", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.6,
        "amp_a": 0.05, "amp_d": 0.5, "amp_s": 0.4, "amp_r": 0.6,
        "flt_cutoff": 3500.0, "brownian_amount": 0.85,
    }),
    ("Phaser Lead", {
        "timbre_shape": HT_SAW, "ophaser_mix": 0.9, "ophaser_rate": 1.2, "ophaser_depth": 0.8,
        "glide_time": 0.15, "amp_a": 0.05, "amp_s": 0.8, "amp_r": 0.4,
    }),
    ("Funky Synth Bass", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.6, "amp_r": 0.3,
        "glide_time": 0.1, "flt_cutoff": 1100.0,
    }),
    ("Disco Sub", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.4,
        "pitch_semitones": -12.0, "flt_cutoff": 600.0,
        "amp_a": 0.001, "amp_d": 0.4, "amp_s": 0.7, "amp_r": 0.3,
    }),
    ("Plucky Bass", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.6, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
    }),

    # ── Kraftwerk (Autobahn) — 5 layer + 2 bass ────────────────────────────
    ("ARP Lead", {
        "timbre_shape": HT_SQUARE, "glide_time": 0.3,
        "vib_depth": 0.2, "vib_speed": 5.0,
        "amp_a": 0.02, "amp_s": 0.85, "amp_r": 0.3, "flt_cutoff": 5000.0,
    }),
    ("Vocoder Robot", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SQUARE, "partB_level": 0.6,
        "ophaser_mix": 0.6, "ophaser_rate": 4.0,
        "trem_depth": 0.4, "trem_speed": 6.0, "trem_shape": 2,
        "amp_s": 0.8, "amp_r": 0.3,
    }),
    ("Ribbon Glide", {
        "timbre_shape": HT_SINE, "glide_time": 0.8,
        "vib_depth": 0.6, "vib_speed": 4.0,
        "amp_a": 0.05, "amp_s": 0.85, "amp_r": 0.5,
    }),
    ("Motorik Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "amp_a": 1.5, "amp_d": 0.4, "amp_s": 0.9, "amp_r": 2.0,
        "brownian_amount": 0.85, "flt_cutoff": 4500.0,
    }),
    ("Vintage String", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "unison_voices": 3, "unison_detune": 18.0,
        "amp_a": 0.8, "amp_d": 0.4, "amp_s": 0.85, "amp_r": 1.2,
        "flt_cutoff": 4000.0,
    }),
    ("Pulse Sub Bass", {
        "timbre_shape": HT_SQUARE, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.7, "amp_r": 0.3,
        "flt_cutoff": 800.0,
    }),
    ("Sequenced Bass", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.2, "amp_s": 0.0, "amp_r": 0.15,
        "flt_cutoff": 1500.0, "flt_env_amt": 0.3,
    }),

    # ── Deadmau5 (Random Album Title) — 5 layer + 2 bass ───────────────────
    ("Sidechain Saw Lead", {
        "timbre_shape": HT_SAW, "unison_voices": 5, "unison_detune": 22.0,
        "amp_a": 0.005, "amp_d": 0.3, "amp_s": 0.8, "amp_r": 0.15,
        "flt_cutoff": 8000.0,
    }),
    ("Plucky Stab", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
        "ophaser_mix": 0.3, "ophaser_rate": 2.0,
    }),
    ("Wet Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SINE, "partB_level": 0.5,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 2.0,
        "blur_size": 0.5, "blur_time": 1.4, "ophaser_mix": 0.4,
    }),
    ("Bell Lead", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.6, "prism_mode": 1,
        "amp_a": 0.001, "amp_d": 0.5, "amp_s": 0.0, "amp_r": 0.6,
    }),
    ("Lasersaw Stab", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0, "glide_time": 0.0,
        "amp_a": 0.001, "amp_d": 0.4, "amp_s": 0.0, "amp_r": 0.3,
        "blur_size": 0.3, "flt_cutoff": 4500.0,
    }),
    ("Big Saw Bass", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.75, "amp_r": 0.4,
        "flt_cutoff": 1200.0,
    }),
    ("Plucked Sub", {
        "timbre_shape": HT_SINE, "pitch_semitones": -12.0,
        "pluck_decay": 0.7, "amp_a": 0.001, "amp_d": 0.4, "amp_s": 0.0, "amp_r": 0.3,
    }),

    # ── Skrillex (Quest For Fire) — 5 layer + 3 bass ───────────────────────
    ("Future Bass Chord", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 4, "unison_detune": 22.0,
        "amp_a": 0.1, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.6,
        "vib_depth": 0.4, "vib_speed": 5.0,
    }),
    ("Hyper Lead", {
        "timbre_shape": HT_SAW, "pitch_semitones": 12.0, "glide_time": 0.6,
        "amp_a": 0.005, "amp_d": 0.3, "amp_s": 0.8, "amp_r": 0.3,
        "flt_cutoff": 12000.0,
    }),
    ("Aggressive Saw", {
        "timbre_shape": HT_SAW, "unison_voices": 3, "unison_detune": 25.0,
        "rm_clip": 0.6, "amp_s": 0.85, "amp_r": 0.3, "flt_cutoff": 8000.0,
    }),
    ("Resonant Lead", {
        "timbre_shape": HT_SAW, "flt_res": 0.85, "flt_env_amt": 0.3,
        "glide_time": 0.3, "amp_s": 0.8, "amp_r": 0.3,
    }),
    ("Vocal Chop Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.5,
        "blur_size": 0.6, "blur_time": 1.5,
        "trem_depth": 0.5, "trem_speed": 4.0, "trem_shape": 0,
        "amp_a": 0.3, "amp_s": 0.85, "amp_r": 1.0,
    }),
    ("Skrillex Reese", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.8,
        "unison_voices": 5, "unison_detune": 35.0,
        "pitch_semitones": -12.0, "flt_cutoff": 600.0, "rm_clip": 0.3,
        "amp_s": 0.85, "amp_r": 0.4,
    }),
    ("Wobble Bass", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "trem_depth": 0.8, "trem_speed": 5.0, "trem_shape": 0,
        "flt_cutoff": 1200.0, "flt_env_amt": 0.4,
        "amp_s": 0.85,
    }),
    ("FM Growl", {
        "timbre_shape": HT_SINE, "prism_amount": 0.8, "prism_mode": 2,
        "pitch_semitones": -12.0, "rm_clip": 0.4,
        "amp_a": 0.005, "amp_d": 0.3, "amp_s": 0.8, "amp_r": 0.3,
    }),

    # ── Depeche Mode (Violator) — 5 layer + 2 bass ─────────────────────────
    ("Dark Minor Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "unison_voices": 2, "unison_detune": 12.0,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 2.0,
        "flt_cutoff": 1500.0, "brownian_amount": 0.9,
    }),
    ("FM Plucky EP", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.6,
        "prism_amount": 0.5, "prism_mode": 1,
        "amp_a": 0.02, "amp_d": 0.4, "amp_s": 0.3, "amp_r": 0.5,
    }),
    ("Industrial Lead", {
        "timbre_shape": HT_SQUARE, "rm_clip": 0.4,
        "glide_time": 0.2, "amp_a": 0.005, "amp_d": 0.3, "amp_s": 0.8, "amp_r": 0.3,
        "flt_cutoff": 4500.0, "flt_res": 0.5,
    }),
    ("Glassy Stab", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.7, "prism_mode": 1,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.0, "amp_r": 0.3,
    }),
    ("Choir Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.5,
        "amp_a": 2.0, "amp_d": 0.5, "amp_s": 0.9, "amp_r": 2.5,
        "blur_size": 0.4, "flt_cutoff": 5000.0,
    }),
    ("80s Synth Bass", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.75, "amp_r": 0.4,
        "flt_cutoff": 1300.0,
    }),
    ("Filtered Saw Bass", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "flt_env_amt": 0.4, "flt_d": 0.25,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.0, "amp_r": 0.2,
    }),

    # ── Neo-Soul / Vintage (Gnarls + Aretha) — 6 layer + 2 bass ────────────
    ("Vintage Wurli", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.5,
        "amp_a": 0.02, "amp_d": 0.5, "amp_s": 0.4, "amp_r": 0.5,
        "trem_depth": 0.6, "trem_speed": 5.5, "trem_shape": 0,
        "brownian_amount": 0.85, "flt_cutoff": 4000.0,
    }),
    ("Hammond B3 Smooth", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.7,
        "vib_depth": 0.4, "vib_speed": 6.0,
        "amp_a": 0.05, "amp_s": 0.95, "amp_r": 0.3, "flt_cutoff": 6000.0,
    }),
    ("Harpsichord Stab", {
        "timbre_shape": HT_SAW, "prism_amount": 0.4,
        "amp_a": 0.001, "amp_d": 0.18, "amp_s": 0.0, "amp_r": 0.2,
        "flt_cutoff": 6000.0,
    }),
    ("Theremin Lead", {
        "timbre_shape": HT_SINE, "vib_depth": 1.0, "vib_speed": 4.0,
        "glide_time": 0.7, "amp_a": 0.05, "amp_s": 0.9, "amp_r": 0.5,
    }),
    ("Mellotron Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "unison_voices": 2, "unison_detune": 8.0,
        "amp_a": 1.0, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 1.8,
        "blur_size": 0.3,
    }),
    ("Soulful Rhodes", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.65,
        "amp_a": 0.05, "amp_d": 0.5, "amp_s": 0.4, "amp_r": 0.6,
        "brownian_amount": 0.8, "flt_cutoff": 4500.0,
    }),
    ("Walking Synth Bass", {
        "timbre_shape": HT_SINE, "pitch_semitones": -12.0,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.4, "amp_r": 0.3,
        "glide_time": 0.05, "flt_cutoff": 700.0,
    }),
    ("Vintage Round Bass", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.4,
        "pitch_semitones": -12.0, "flt_cutoff": 900.0,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.65, "amp_r": 0.3,
        "brownian_amount": 0.85,
    }),

    # ═══════════════════════════════════════════════════════════════════════
    # 2026-04-26 (round 2): 74 type-based Harmless patches.
    # Parallels BaySickSynth's 10-folder structure so users searching for
    # "a pad" or "a lead" don't have to know an artist's name.  Existing 78
    # genre-folder patches stay; these add a second discovery axis.
    # ═══════════════════════════════════════════════════════════════════════

    # ── Keys & Electric Pianos (8) ──────────────────────────────────────────
    ("Bright FM Tines", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.7,
        "prism_amount": 0.5, "prism_mode": 1,
        "amp_a": 0.005, "amp_d": 0.6, "amp_s": 0.4, "amp_r": 0.5,
    }),
    ("B3 Drawbar", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.85,
        "vib_depth": 0.5, "vib_speed": 6.5,
        "amp_a": 0.02, "amp_s": 0.95, "amp_r": 0.25, "flt_cutoff": 6500.0,
    }),
    ("Phase EP", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.6,
        "ophaser_mix": 0.6, "ophaser_rate": 0.5,
        "amp_a": 0.03, "amp_d": 0.5, "amp_s": 0.5, "amp_r": 0.6,
    }),
    ("Toy Piano", {
        "timbre_shape": HT_TRIANGLE, "prism_amount": 0.4, "prism_mode": 1,
        "amp_a": 0.001, "amp_d": 0.7, "amp_s": 0.0, "amp_r": 0.5,
        "flt_cutoff": 5500.0,
    }),
    ("Glass Keys", {
        "timbre_shape": HT_SINE, "prism_amount": 0.7, "prism_mode": 2,
        "amp_a": 0.005, "amp_d": 0.8, "amp_s": 0.2, "amp_r": 1.0,
    }),
    ("Crystal EP", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.5,
        "prism_amount": 0.3, "amp_a": 0.005, "amp_d": 0.6, "amp_s": 0.3, "amp_r": 0.8,
        "blur_size": 0.2,
    }),
    ("House Organ", {
        "timbre_shape": HT_SQUARE, "partB_timbre_shape": HT_SINE, "partB_level": 0.6,
        "amp_a": 0.005, "amp_s": 1.0, "amp_r": 0.2, "flt_cutoff": 4500.0,
    }),
    ("Synth Clav", {
        "timbre_shape": HT_SQUARE, "pluck_decay": 0.3,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
        "flt_cutoff": 4000.0, "flt_env_amt": 0.2, "flt_d": 0.15,
    }),

    # ── Plucks & Mallets (8) ────────────────────────────────────────────────
    ("Hard Pluck", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.7,
        "amp_a": 0.001, "amp_d": 0.18, "amp_s": 0.0, "amp_r": 0.15,
        "flt_cutoff": 6000.0,
    }),
    ("Soft Pluck", {
        "timbre_shape": HT_SINE, "pluck_decay": 0.4,
        "amp_a": 0.005, "amp_d": 0.4, "amp_s": 0.0, "amp_r": 0.4,
        "flt_cutoff": 4000.0, "brownian_amount": 0.85,
    }),
    ("Marimba Pluck", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.5,
        "prism_amount": 0.35, "amp_a": 0.001, "amp_d": 0.35, "amp_s": 0.0, "amp_r": 0.3,
    }),
    ("FM Bell Pluck", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.7, "prism_mode": 2,
        "amp_a": 0.001, "amp_d": 0.5, "amp_s": 0.0, "amp_r": 0.5,
    }),
    ("Crystal Pluck", {
        "timbre_shape": HT_SINE, "prism_amount": 0.5, "prism_mode": 1,
        "pluck_decay": 0.4,
        "amp_a": 0.001, "amp_d": 0.4, "amp_s": 0.0, "amp_r": 0.6,
    }),
    ("Tropical Pluck", {
        "timbre_shape": HT_SQUARE, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
        "flt_cutoff": 3500.0,
    }),
    ("Ice Pluck", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.6, "prism_mode": 1,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.0, "amp_r": 0.4,
        "blur_size": 0.2,
    }),
    ("Wood Pluck", {
        "timbre_shape": HT_TRIANGLE, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.2, "amp_s": 0.0, "amp_r": 0.18,
        "flt_cutoff": 3500.0, "rm_clip": 0.15,
    }),

    # ── Pads & Atmospheres (10) ─────────────────────────────────────────────
    ("Glass Shimmer Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.4,
        "prism_amount": 0.5, "blur_size": 0.6, "blur_time": 1.5,
        "amp_a": 2.0, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 3.0,
    }),
    ("Warm Analog Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "unison_voices": 3, "unison_detune": 14.0,
        "amp_a": 1.2, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 1.8,
        "flt_cutoff": 3500.0, "brownian_amount": 0.9,
    }),
    ("Vintage Strings Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 5, "unison_detune": 22.0, "unison_spread": 0.9,
        "amp_a": 0.8, "amp_d": 0.4, "amp_s": 0.85, "amp_r": 1.5,
        "flt_cutoff": 5000.0,
    }),
    ("Ocean Wave Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.5,
        "vib_depth": 0.6, "vib_speed": 0.5,
        "amp_a": 3.0, "amp_d": 0.5, "amp_s": 0.95, "amp_r": 4.0,
        "blur_size": 0.4,
    }),
    ("Lush Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 5, "unison_detune": 18.0,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.9, "amp_r": 2.5,
        "blur_size": 0.5, "ophaser_mix": 0.3,
    }),
    ("Lo-Fi Tape Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.5,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 2.0,
        "brownian_amount": 0.95, "flt_cutoff": 3000.0,
        "vib_depth": 0.2, "vib_speed": 1.0,
    }),
    ("Vocal Choir Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "amp_a": 1.8, "amp_d": 0.5, "amp_s": 0.9, "amp_r": 2.2,
        "blur_size": 0.5, "filter_mask_cutoff": 3000.0,
    }),
    ("Stadium Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 7, "unison_detune": 25.0, "unison_spread": 1.0,
        "amp_a": 0.5, "amp_d": 0.4, "amp_s": 0.9, "amp_r": 1.5,
        "ophaser_mix": 0.4,
    }),
    ("Sweep Pad", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 2.0,
        "flt_cutoff": 1200.0, "flt_env_amt": 0.5, "flt_a": 2.0,
    }),
    ("Misty Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SINE, "partB_level": 0.7,
        "amp_a": 2.5, "amp_d": 0.5, "amp_s": 0.9, "amp_r": 3.0,
        "blur_size": 0.7, "blur_time": 1.8, "vib_depth": 0.3, "vib_speed": 0.4,
    }),

    # ── Leads & Solos (10) ──────────────────────────────────────────────────
    ("PWM Lead", {
        "timbre_shape": HT_SQUARE, "vib_depth": 0.4, "vib_speed": 4.0,
        "amp_a": 0.005, "amp_s": 0.85, "amp_r": 0.3,
    }),
    ("Sync Lead", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 2, "unison_detune": 25.0, "unison_alt": 1,
        "amp_a": 0.005, "amp_s": 0.85, "amp_r": 0.3,
    }),
    ("Flute Lead", {
        "timbre_shape": HT_SINE, "vib_depth": 0.5, "vib_speed": 5.0,
        "amp_a": 0.05, "amp_d": 0.2, "amp_s": 0.9, "amp_r": 0.3,
        "flt_cutoff": 4000.0,
    }),
    ("Whistle", {
        "timbre_shape": HT_SINE, "vib_depth": 0.7, "vib_speed": 6.0,
        "amp_a": 0.05, "amp_d": 0.2, "amp_s": 0.85, "amp_r": 0.4,
        "filter_mask_cutoff": 2500.0,
    }),
    ("Lasersaw", {
        "timbre_shape": HT_SAW, "unison_voices": 5, "unison_detune": 20.0,
        "amp_a": 0.005, "amp_d": 0.4, "amp_s": 0.0, "amp_r": 0.3,
        "blur_size": 0.3, "flt_cutoff": 7000.0,
    }),
    ("Acid Lead", {
        "timbre_shape": HT_SAW, "flt_res": 0.9, "flt_env_amt": 0.5, "flt_d": 0.3,
        "glide_time": 0.15, "amp_s": 0.7, "amp_r": 0.3,
    }),
    ("Bright Saw Lead", {
        "timbre_shape": HT_SAW, "unison_voices": 3, "unison_detune": 15.0,
        "amp_a": 0.005, "amp_s": 0.85, "amp_r": 0.3,
        "flt_cutoff": 10000.0,
    }),
    ("Square Glide", {
        "timbre_shape": HT_SQUARE, "glide_time": 0.4,
        "amp_a": 0.005, "amp_s": 0.85, "amp_r": 0.3,
    }),
    ("Crystal Lead", {
        "timbre_shape": HT_SINE, "prism_amount": 0.6, "prism_mode": 1,
        "amp_a": 0.005, "amp_d": 0.4, "amp_s": 0.5, "amp_r": 0.5,
    }),
    ("Detuned Lead", {
        "timbre_shape": HT_SAW, "unison_voices": 2, "unison_detune": 30.0,
        "amp_a": 0.005, "amp_s": 0.8, "amp_r": 0.3,
    }),

    # ── Brass & Strings (8) ─────────────────────────────────────────────────
    ("Synth Brass", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "amp_a": 0.1, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.5,
        "flt_cutoff": 5500.0, "flt_a": 0.15, "flt_env_amt": 0.3,
        "brownian_amount": 0.9,
    }),
    ("Strings Ensemble", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 5, "unison_detune": 20.0, "unison_spread": 0.9,
        "amp_a": 0.4, "amp_d": 0.4, "amp_s": 0.85, "amp_r": 1.2,
        "flt_cutoff": 5500.0,
    }),
    ("Synth Cello", {
        "timbre_shape": HT_SAW, "pitch_semitones": -12.0,
        "vib_depth": 0.4, "vib_speed": 4.5,
        "amp_a": 0.2, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.8,
        "flt_cutoff": 3500.0, "brownian_amount": 0.85,
    }),
    ("Epic Horns", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SQUARE, "partB_level": 0.5,
        "unison_voices": 3, "unison_detune": 12.0,
        "amp_a": 0.15, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.6,
        "flt_cutoff": 4500.0, "rm_clip": 0.15,
    }),
    ("Soft Choir", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.5,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.9, "amp_r": 2.0,
        "blur_size": 0.4, "filter_mask_cutoff": 2500.0,
    }),
    ("Glass Choir", {
        "timbre_shape": HT_SINE, "prism_amount": 0.4,
        "amp_a": 1.0, "amp_d": 0.5, "amp_s": 0.9, "amp_r": 1.8,
        "blur_size": 0.5,
    }),
    ("OB-8 Brass", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "unison_voices": 2, "unison_detune": 10.0,
        "amp_a": 0.05, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.4,
        "flt_cutoff": 6000.0,
    }),
    ("Marcato", {
        "timbre_shape": HT_SAW, "unison_voices": 4, "unison_detune": 18.0,
        "amp_a": 0.05, "amp_d": 0.5, "amp_s": 0.6, "amp_r": 0.6,
        "flt_cutoff": 5000.0,
    }),

    # ── Arp & Sequencer (8) ─────────────────────────────────────────────────
    ("Acid Arp", {
        "timbre_shape": HT_SAW, "flt_res": 0.85, "flt_env_amt": 0.5, "flt_d": 0.18,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.18,
    }),
    ("Square Bounce", {
        "timbre_shape": HT_SQUARE, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.15,
    }),
    ("FM Arp", {
        "timbre_shape": HT_SINE, "prism_amount": 0.55, "prism_mode": 1,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
    }),
    ("Bell Arp", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.7, "prism_mode": 2,
        "amp_a": 0.001, "amp_d": 0.4, "amp_s": 0.0, "amp_r": 0.45,
    }),
    ("Trance Gate", {
        "timbre_shape": HT_SAW, "trem_depth": 1.0, "trem_speed": 16.0, "trem_shape": 2,
        "amp_a": 0.001, "amp_s": 0.85, "amp_r": 0.2,
    }),
    ("Modern Arp", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.4, "blur_size": 0.2,
        "amp_a": 0.001, "amp_d": 0.2, "amp_s": 0.0, "amp_r": 0.18,
    }),
    ("Bubble Arp", {
        "timbre_shape": HT_SINE, "pitch_semitones": 12.0, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.18, "amp_s": 0.0, "amp_r": 0.15,
    }),
    ("Retro Arp", {
        "timbre_shape": HT_SQUARE, "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
        "flt_cutoff": 4500.0, "brownian_amount": 0.85,
    }),

    # ── Chiptune & 8-Bit (6) ────────────────────────────────────────────────
    ("8-Bit Lead", {
        "timbre_shape": HT_SQUARE, "amp_a": 0.001, "amp_s": 0.85, "amp_r": 0.0,
        "flt_cutoff": 20000.0,
    }),
    ("Gameboy Pulse", {
        "timbre_shape": HT_SQUARE, "pluck_decay": 0.3,
        "amp_a": 0.001, "amp_d": 0.2, "amp_s": 0.5, "amp_r": 0.0,
    }),
    ("Bell Chip", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.6, "pitch_semitones": 12.0,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.0, "amp_r": 0.0,
    }),
    ("Square Chip", {
        "timbre_shape": HT_SQUARE, "amp_a": 0.001, "amp_d": 0.15, "amp_s": 0.0, "amp_r": 0.0,
    }),
    ("Bit-Crush Pad", {
        "timbre_shape": HT_SQUARE, "rm_clip": 0.5,
        "amp_a": 0.5, "amp_d": 0.4, "amp_s": 0.85, "amp_r": 0.8,
    }),
    ("Arcade Stab", {
        "timbre_shape": HT_SQUARE, "pitch_semitones": 7.0, "pluck_decay": 0.6,
        "amp_a": 0.001, "amp_d": 0.15, "amp_s": 0.0, "amp_r": 0.1,
    }),

    # ── Cinematic & Drones (8) ──────────────────────────────────────────────
    ("Sci-Fi Wash", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.6,
        "prism_amount": 0.5, "amp_a": 3.0, "amp_d": 0.5, "amp_s": 0.95, "amp_r": 5.0,
        "blur_size": 0.6, "ophaser_mix": 0.5,
    }),
    ("Wind Howl", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 5, "unison_detune": 50.0, "unison_spread": 1.0,
        "amp_a": 2.0, "amp_d": 0.5, "amp_s": 0.95, "amp_r": 4.0,
        "filter_mask_cutoff": 1500.0,
    }),
    ("Sub Drop FX", {
        "timbre_shape": HT_SINE, "pitch_semitones": -24.0,
        "glide_time": 1.5, "vib_depth": 0.3, "vib_speed": 0.3,
        "amp_a": 0.5, "amp_d": 0.5, "amp_s": 0.95, "amp_r": 2.0,
    }),
    ("Riser FX", {
        "timbre_shape": HT_SAW, "unison_voices": 4, "unison_detune": 30.0,
        "amp_a": 4.0, "amp_d": 0.5, "amp_s": 0.95, "amp_r": 0.3,
        "flt_cutoff": 800.0, "flt_env_amt": 0.7, "flt_a": 4.0,
    }),
    ("Tension Riser", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 3, "unison_detune": 25.0,
        "amp_a": 5.0, "amp_d": 0.5, "amp_s": 0.95, "amp_r": 0.3,
        "vib_depth": 0.6, "vib_speed": 0.3, "vib_env": 0.8,
    }),
    ("Cinematic Sweep", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.5,
        "unison_voices": 3, "unison_detune": 18.0,
        "amp_a": 2.5, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 3.0,
        "flt_cutoff": 800.0, "flt_env_amt": 0.6, "flt_a": 3.0,
    }),
    ("Granular Texture", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_SAW, "partB_level": 0.5,
        "blur_size": 0.8, "blur_time": 1.8, "phaser_mask_rate": 4.0,
        "amp_a": 1.5, "amp_d": 0.5, "amp_s": 0.85, "amp_r": 2.0,
    }),
    ("Glass Bell Pad", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.7, "prism_mode": 2,
        "amp_a": 1.0, "amp_d": 1.0, "amp_s": 0.7, "amp_r": 2.5,
        "blur_size": 0.4,
    }),

    # ── Synthwave & Vintage (4) ─────────────────────────────────────────────
    ("Outrun Lead", {
        "timbre_shape": HT_SQUARE, "glide_time": 0.3, "vib_depth": 0.3, "vib_speed": 5.0,
        "amp_a": 0.005, "amp_s": 0.85, "amp_r": 0.4,
        "flt_cutoff": 5500.0, "ophaser_mix": 0.2,
    }),
    ("VHS Keys", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.6,
        "amp_a": 0.05, "amp_d": 0.5, "amp_s": 0.4, "amp_r": 0.6,
        "brownian_amount": 0.9, "flt_cutoff": 3000.0,
        "vib_depth": 0.15, "vib_speed": 1.5,
    }),
    ("Neon Pluck", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.5,
        "amp_a": 0.001, "amp_d": 0.3, "amp_s": 0.0, "amp_r": 0.3,
        "flt_cutoff": 5500.0, "ophaser_mix": 0.25,
    }),
    ("Retrowave Bell", {
        "timbre_shape": HT_SINE, "partB_timbre_shape": HT_TRIANGLE, "partB_level": 0.4,
        "prism_amount": 0.5, "prism_mode": 1,
        "amp_a": 0.005, "amp_d": 0.6, "amp_s": 0.0, "amp_r": 0.8,
    }),

    # ── Modern EDM & Hyperpop (4) ───────────────────────────────────────────
    ("Festival Lead", {
        "timbre_shape": HT_SAW, "unison_voices": 5, "unison_detune": 25.0,
        "pitch_semitones": 12.0, "amp_a": 0.005, "amp_s": 0.85, "amp_r": 0.3,
        "flt_cutoff": 12000.0,
    }),
    ("Future Bass", {
        "timbre_shape": HT_SAW, "partB_timbre_shape": HT_SAW, "partB_level": 0.7,
        "unison_voices": 4, "unison_detune": 20.0,
        "vib_depth": 0.5, "vib_speed": 5.0,
        "amp_a": 0.1, "amp_d": 0.3, "amp_s": 0.85, "amp_r": 0.5,
    }),
    ("Modern Pluck", {
        "timbre_shape": HT_SAW, "pluck_decay": 0.5, "blur_size": 0.2,
        "amp_a": 0.001, "amp_d": 0.25, "amp_s": 0.0, "amp_r": 0.2,
        "flt_cutoff": 6000.0,
    }),
    ("Hyper Glitch", {
        "timbre_shape": HT_SQUARE, "blur_size": 0.5, "phaser_mask_rate": 5.0,
        "rm_clip": 0.4, "pitch_semitones": 12.0,
        "amp_a": 0.001, "amp_d": 0.2, "amp_s": 0.5, "amp_r": 0.2,
    }),
]

# ─── BaySickPlayer factory presets (2026-04-26) ──────────────────────────────
# 75 SFZ-wrapping presets — one per file in the Core Library packs.
# Each preset is a thin BaySickPlayer state wrapper that points to an SFZ via
# `library:` path so the SampleLibrary resolver locates it relative to
# LOCALAPPDATA/BaySickDAW/CoreLibrary/.
#
# (preset_name, pack_folder, sfz_filename, overrides)
# Empty overrides = use BSP_DEFAULTS for everything (the SFZ owns the timbre).
BSP_RECIPES = [
    # ── Brass Package (14) ──────────────────────────────────────────────────
    ("French Horn (Muted)",            "Brass",      "Brass Package/FHornMute.sfz",                {"attack": 0.02}),
    ("French Horn (Staccato)",         "Brass",      "Brass Package/FHornStac.sfz",                {}),
    ("French Horn (Sustained)",        "Brass",      "Brass Package/FHornSus.sfz",                 {"attack": 0.04}),
    ("Trombone (Staccato)",            "Brass",      "Brass Package/TromboneStac.sfz",             {}),
    ("Trombone (Sustained)",           "Brass",      "Brass Package/TromboneSus.sfz",              {"attack": 0.04}),
    ("Trombone (Vibrato)",             "Brass",      "Brass Package/TromboneVib.sfz",              {"attack": 0.05}),
    ("Trumpet (Harmon Mute)",          "Brass",      "Brass Package/TrumpetHarmonMuteSus.sfz",     {"attack": 0.03}),
    ("Trumpet (Staccato)",             "Brass",      "Brass Package/TrumpetStac.sfz",              {}),
    ("Trumpet (Straight Mute)",        "Brass",      "Brass Package/TrumpetStraightMuteSus.sfz",   {"attack": 0.03}),
    ("Trumpet (Sustained)",            "Brass",      "Brass Package/TrumpetSus.sfz",               {"attack": 0.03}),
    ("Trumpet (Sustained Vibrato)",    "Brass",      "Brass Package/TrumpetSusVib.sfz",            {"attack": 0.04}),
    ("Tuba (Keyswitch)",               "Brass",      "Brass Package/Tuba-KS.sfz",                  {"attack": 0.04}),
    ("Tuba (Staccato)",                "Brass",      "Brass Package/TubaStac.sfz",                 {}),
    ("Tuba (Sustained)",               "Brass",      "Brass Package/TubaSus.sfz",                  {"attack": 0.05}),
    # ── Keys Package (6) ────────────────────────────────────────────────────
    ("Organ Loud",                     "Keys",       "Keys Package/OrganLoud.sfz",                 {}),
    ("Organ Loud (Pedal)",             "Keys",       "Keys Package/OrganLoudPedal.sfz",            {}),
    ("Organ Quiet",                    "Keys",       "Keys Package/OrganQuiet.sfz",                {}),
    ("Organ Quiet (Pedal)",            "Keys",       "Keys Package/OrganQuietPedal.sfz",           {}),
    ("Upright Piano",                  "Keys",       "Keys Package/UprightPiano.sfz",              {}),
    ("Upright Piano (VS)",             "Keys",       "Keys Package/VSUpright1.sfz",                {}),
    # ── Percussion Package (7) ──────────────────────────────────────────────
    ("GM Percussion",                  "Percussion", "Percussion Package/GM-StylePerc.sfz",        {}),
    ("Glockenspiel",                   "Percussion", "Percussion Package/Glockenspiel.sfz",        {}),
    ("Marimba",                        "Percussion", "Percussion Package/Marimba.sfz",             {}),
    ("Timpani",                        "Percussion", "Percussion Package/Timpani.sfz",             {"release": 0.6}),
    ("Timpani Rolls",                  "Percussion", "Percussion Package/TimpaniRolls.sfz",        {"release": 0.5}),
    ("Tubular Bells",                  "Percussion", "Percussion Package/TubularBells.sfz",        {"release": 1.0}),
    ("Xylophone",                      "Percussion", "Percussion Package/Xylophone.sfz",           {}),
    # ── Strings Package (32) ────────────────────────────────────────────────
    ("Cello Ens (Keyswitch)",          "Strings",    "Strings Package/CelloEns-KS.sfz",            {"attack": 0.05}),
    ("Cello Ens (Pizzicato)",          "Strings",    "Strings Package/CelloEnsPizz.sfz",           {}),
    ("Cello Ens (Spiccato)",           "Strings",    "Strings Package/CelloEnsSpic.sfz",           {}),
    ("Cello Ens (Sustained Quiet)",    "Strings",    "Strings Package/CelloEnsSusVib-Quiet.sfz",   {"attack": 0.08, "release": 0.6}),
    ("Cello Ens (Sustained)",          "Strings",    "Strings Package/CelloEnsSusVib.sfz",         {"attack": 0.06, "release": 0.5}),
    ("Cello Ens (Tremolo)",            "Strings",    "Strings Package/CelloEnsTrem.sfz",           {"attack": 0.05}),
    ("Contrabass (Keyswitch)",         "Strings",    "Strings Package/Contrabass-KS.sfz",          {"attack": 0.05}),
    ("Contrabass (Pizzicato)",         "Strings",    "Strings Package/ContrabassPizz.sfz",         {}),
    ("Contrabass (Spiccato)",          "Strings",    "Strings Package/ContrabassSpic.sfz",         {}),
    ("Contrabass (Sustained No Vib)",  "Strings",    "Strings Package/ContrabassSusNV.sfz",        {"attack": 0.06, "release": 0.5}),
    ("Contrabass (Sustained Quiet)",   "Strings",    "Strings Package/ContrabassSusVB-Quiet.sfz",  {"attack": 0.08, "release": 0.6}),
    ("Contrabass (Sustained)",         "Strings",    "Strings Package/ContrabassSusVB.sfz",        {"attack": 0.06, "release": 0.5}),
    ("Contrabass (Tremolo)",           "Strings",    "Strings Package/ContrabassTrem.sfz",         {"attack": 0.05}),
    ("Harp",                           "Strings",    "Strings Package/Harp.sfz",                   {"release": 1.5}),
    ("Solo Violin (Keyswitch)",        "Strings",    "Strings Package/SViolin-KS.sfz",             {"attack": 0.05}),
    ("Solo Violin (Pizzicato)",        "Strings",    "Strings Package/SViolinPizz.sfz",            {}),
    ("Solo Violin (Spiccato)",         "Strings",    "Strings Package/SViolinSpic.sfz",            {}),
    ("Solo Violin (Tremolo)",          "Strings",    "Strings Package/SViolinTrem.sfz",            {"attack": 0.05}),
    ("Solo Violin (Sustained Quiet)",  "Strings",    "Strings Package/SViolinVib-Quiet.sfz",       {"attack": 0.08, "release": 0.6}),
    ("Solo Violin (Sustained)",        "Strings",    "Strings Package/SViolinVib.sfz",             {"attack": 0.06, "release": 0.5}),
    ("Viola Ens (Keyswitch)",          "Strings",    "Strings Package/ViolaEns-KS.sfz",            {"attack": 0.05}),
    ("Viola Ens (Pizzicato)",          "Strings",    "Strings Package/ViolaEnsPizz.sfz",           {}),
    ("Viola Ens (Spiccato)",           "Strings",    "Strings Package/ViolaEnsSpic.sfz",           {}),
    ("Viola Ens (Sustained Quiet)",    "Strings",    "Strings Package/ViolaEnsSusVib-Quiet.sfz",   {"attack": 0.08, "release": 0.6}),
    ("Viola Ens (Sustained)",          "Strings",    "Strings Package/ViolaEnsSusVib.sfz",         {"attack": 0.06, "release": 0.5}),
    ("Viola Ens (Tremolo)",            "Strings",    "Strings Package/ViolaEnsTrem.sfz",           {"attack": 0.05}),
    ("Violin Ens (Keyswitch)",         "Strings",    "Strings Package/ViolinEns-KS.sfz",           {"attack": 0.05}),
    ("Violin Ens (Pizzicato)",         "Strings",    "Strings Package/ViolinEnsPizz.sfz",          {}),
    ("Violin Ens (Spiccato)",          "Strings",    "Strings Package/ViolinEnsSpic.sfz",          {}),
    ("Violin Ens (Sustained Quiet)",   "Strings",    "Strings Package/ViolinEnsSusVib-Quiet.sfz",  {"attack": 0.08, "release": 0.6}),
    ("Violin Ens (Sustained)",         "Strings",    "Strings Package/ViolinEnsSusVib.sfz",        {"attack": 0.06, "release": 0.5}),
    ("Violin Ens (Tremolo)",           "Strings",    "Strings Package/ViolinEnsTrem.sfz",          {"attack": 0.05}),
    # ── Woodwinds Package (16) ──────────────────────────────────────────────
    ("Bassoon (Staccato)",             "Woodwinds",  "Woodwinds Package/BassoonStac.sfz",          {}),
    ("Bassoon (Sustained)",            "Woodwinds",  "Woodwinds Package/BassoonSus.sfz",           {"attack": 0.05}),
    ("Bassoon (Vibrato)",              "Woodwinds",  "Woodwinds Package/BassoonVib.sfz",           {"attack": 0.06}),
    ("Clarinet (Keyswitch)",           "Woodwinds",  "Woodwinds Package/Clarinet-KS.sfz",          {"attack": 0.04}),
    ("Clarinet (Staccato)",            "Woodwinds",  "Woodwinds Package/ClarinetStac.sfz",         {}),
    ("Clarinet (Sustained)",           "Woodwinds",  "Woodwinds Package/ClarinetSus.sfz",          {"attack": 0.05}),
    ("Flute (Keyswitch)",              "Woodwinds",  "Woodwinds Package/Flute-KS.sfz",             {"attack": 0.04}),
    ("Flute (Expressive Vibrato)",     "Woodwinds",  "Woodwinds Package/FluteExpVib.sfz",          {"attack": 0.05}),
    ("Flute (Staccato)",               "Woodwinds",  "Woodwinds Package/FluteStac.sfz",            {}),
    ("Flute (Sustained No Vib)",       "Woodwinds",  "Woodwinds Package/FluteSusNV.sfz",           {"attack": 0.05}),
    ("Flute (Sustained Vibrato)",      "Woodwinds",  "Woodwinds Package/FluteSusVib.sfz",          {"attack": 0.05}),
    ("Oboe (Staccato)",                "Woodwinds",  "Woodwinds Package/OboeStac.sfz",             {}),
    ("Oboe (Sustained No Vib)",        "Woodwinds",  "Woodwinds Package/OboeSusNV.sfz",            {"attack": 0.04}),
    ("Oboe (Sustained Vibrato)",       "Woodwinds",  "Woodwinds Package/OboeSusVib.sfz",           {"attack": 0.05}),
    ("Piccolo (Staccato)",             "Woodwinds",  "Woodwinds Package/PiccoloStac.sfz",          {}),
    ("Piccolo (Sustained)",            "Woodwinds",  "Woodwinds Package/PiccoloSus.sfz",           {"attack": 0.04}),
]

def write_bsp_preset_xml(target_dir: Path, name: str, sample_lib_rel: str,
                          overrides: dict, kind: str = "sfz") -> Path:
    """Writes a BaySickPlayer factory preset.

    Format mirrors DrumPage::savePatchAs (Source/Standalone/DrumPage.cpp:817):
      <BaySickPlayerState>
        <BaySickPlayerState>             <!-- inner apvts state -->
          <PARAM id="tk_lay_0_bsp_..." value="..."/>
          ...
        </BaySickPlayerState>
        <Sample kind="sfz|file|folder" path="library:Pack/File.ext"/>
      </BaySickPlayerState>

    kind defaults to "sfz" for backward compat with the orchestral SFZ wrappers;
    sample-based presets (Hip Hop / EDM .wav one-shots) pass kind="file".
    """
    target_dir.mkdir(parents=True, exist_ok=True)
    params = {**BSP_DEFAULTS, **overrides}
    prefix = "tk_lay_0_bsp_"
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<BaySickPlayerState>',
        '  <BaySickPlayerState>',
    ]
    for k, v in params.items():
        if isinstance(v, int):
            vstr = f"{v}.0"
        else:
            vstr = repr(float(v))
        lines.append(f'    <PARAM id="{prefix}{k}" value="{vstr}"/>')
    lines.append('  </BaySickPlayerState>')
    lines.append(f'  <Sample kind="{kind}" path="library:{_xml_attr_escape(sample_lib_rel)}"/>')
    lines.append('</BaySickPlayerState>')
    out = target_dir / f"{name}.xml"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out

# ─── BaySickPlayer sample-based factory presets (2026-04-26 round 4) ─────────
# Each entry wraps a single .wav from the Hip Hop or EDM Drums Package.
# (preset_name, sub_folder_under_Presets/BaySickPlayer/, wav_lib_rel)
# kind is always "file" — single-shot drum sample played at the played MIDI note.
# No overrides — defaults keep the engine transparent so the sample sounds
# exactly as recorded.  Subfolders under the destination organize by drum role
# (Kicks / Snares / Hats / etc), not by source pack folder name.

BSP_SAMPLE_RECIPES = [
    # ── Hip Hop Drums Package — 72 files ───────────────────────────────────
    # Kicks (8): BassDrums folder
    ("Hip Hop Kick 01", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_01.wav"),
    ("Hip Hop Kick 02", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_02.wav"),
    ("Hip Hop Kick 03", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_03.wav"),
    ("Hip Hop Kick 04", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_04.wav"),
    ("Hip Hop Kick 05", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_05.wav"),
    ("Hip Hop Kick 06", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_06.wav"),
    ("Hip Hop Kick 07 (A)", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_07_A.wav"),
    ("Hip Hop Kick 08 (G#)", "Hip Hop Drums/Kicks", "Hip Hop Drums Package/BassDrums/r-loops_BD_08_G_.wav"),
    # Snares (8)
    ("Hip Hop Snare 01", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_01.wav"),
    ("Hip Hop Snare 02", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_02.wav"),
    ("Hip Hop Snare 03", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_03.wav"),
    ("Hip Hop Snare 04", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_04.wav"),
    ("Hip Hop Snare 05", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_05.wav"),
    ("Hip Hop Snare 06", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_06.wav"),
    ("Hip Hop Snare 07", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_07.wav"),
    ("Hip Hop Snare 08", "Hip Hop Drums/Snares", "Hip Hop Drums Package/SnareDrums/r-loops_SD_08.wav"),
    # Closed Hats (8)
    ("Hip Hop Closed Hat 01", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_01.wav"),
    ("Hip Hop Closed Hat 02", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_02.wav"),
    ("Hip Hop Closed Hat 03", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_03.wav"),
    ("Hip Hop Closed Hat 04", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_04.wav"),
    ("Hip Hop Closed Hat 05", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_05.wav"),
    ("Hip Hop Closed Hat 06", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_06.wav"),
    ("Hip Hop Closed Hat 07", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_07.wav"),
    ("Hip Hop Closed Hat 08", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatClosed/r-loops_HHC_08.wav"),
    # Open Hats (8)
    ("Hip Hop Open Hat 01", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_01.wav"),
    ("Hip Hop Open Hat 02", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_02.wav"),
    ("Hip Hop Open Hat 03", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_03.wav"),
    ("Hip Hop Open Hat 04", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_04.wav"),
    ("Hip Hop Open Hat 05", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_05.wav"),
    ("Hip Hop Open Hat 06 (Part 1)", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_06_part1_.wav"),
    ("Hip Hop Open Hat 07 (Part 2)", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_07_part2_.wav"),
    ("Hip Hop Open Hat 08", "Hip Hop Drums/Hats", "Hip Hop Drums Package/HiHatOpen/r-loops_OH_08.wav"),
    # Claps (8)
    ("Hip Hop Clap 01", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_01.wav"),
    ("Hip Hop Clap 02", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_02.wav"),
    ("Hip Hop Clap 03", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_03.wav"),
    ("Hip Hop Clap 04", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_04.wav"),
    ("Hip Hop Clap 05", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_05.wav"),
    ("Hip Hop Clap 06", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_06.wav"),
    ("Hip Hop Clap 07", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_07.wav"),
    ("Hip Hop Clap 08", "Hip Hop Drums/Claps", "Hip Hop Drums Package/Claps/r-loops_CP_08.wav"),
    # Toms (4 of 8)
    ("Hip Hop Tom 01", "Hip Hop Drums/Toms", "Hip Hop Drums Package/Toms/r-loops_TOM_01.wav"),
    ("Hip Hop Tom 02", "Hip Hop Drums/Toms", "Hip Hop Drums Package/Toms/r-loops_TOM_02.wav"),
    ("Hip Hop Tom 03", "Hip Hop Drums/Toms", "Hip Hop Drums Package/Toms/r-loops_TOM_03.wav"),
    ("Hip Hop Tom 04", "Hip Hop Drums/Toms", "Hip Hop Drums Package/Toms/r-loops_TOM_04.wav"),
    # Cymbals + Crash (4 of 16)
    ("Hip Hop Cymbal 01", "Hip Hop Drums/Cymbals", "Hip Hop Drums Package/Cymbals/r-loops_CYM_01.wav"),
    ("Hip Hop Cymbal 02", "Hip Hop Drums/Cymbals", "Hip Hop Drums Package/Cymbals/r-loops_CYM_02.wav"),
    ("Hip Hop Crash 01", "Hip Hop Drums/Cymbals", "Hip Hop Drums Package/Crash/r-loops_CR_01.wav"),
    ("Hip Hop Crash 02", "Hip Hop Drums/Cymbals", "Hip Hop Drums Package/Crash/r-loops_CR_02.wav"),
    # FX (8)
    ("Hip Hop FX 01", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_01.wav"),
    ("Hip Hop FX 02 (90 BPM)", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_02_90BPM.wav"),
    ("Hip Hop FX 03", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_03.wav"),
    ("Hip Hop FX 04", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_04.wav"),
    ("Hip Hop FX 05", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_05.wav"),
    ("Hip Hop FX 06", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_06.wav"),
    ("Hip Hop FX 07 (140 BPM)", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_07_140BPM.wav"),
    ("Hip Hop FX 08", "Hip Hop Drums/FX", "Hip Hop Drums Package/FX/r-loops_FX_08.wav"),
    # Beatbox (4 of 8)
    ("Hip Hop Beatbox 01", "Hip Hop Drums/Beatbox", "Hip Hop Drums Package/BeatBox/r-loops_BB_01.wav"),
    ("Hip Hop Beatbox 02", "Hip Hop Drums/Beatbox", "Hip Hop Drums Package/BeatBox/r-loops_BB_02.wav"),
    ("Hip Hop Beatbox 03", "Hip Hop Drums/Beatbox", "Hip Hop Drums Package/BeatBox/r-loops_BB_03.wav"),
    ("Hip Hop Beatbox 04", "Hip Hop Drums/Beatbox", "Hip Hop Drums Package/BeatBox/r-loops_BB_04.wav"),
    # Percussion (4 of 8)
    ("Hip Hop Perc 01", "Hip Hop Drums/Percussion", "Hip Hop Drums Package/Percussion/r-loops_PERC_01.wav"),
    ("Hip Hop Perc 02", "Hip Hop Drums/Percussion", "Hip Hop Drums Package/Percussion/r-loops_PERC_02.wav"),
    ("Hip Hop Perc 03", "Hip Hop Drums/Percussion", "Hip Hop Drums Package/Percussion/r-loops_PERC_03.wav"),
    ("Hip Hop Perc 04", "Hip Hop Drums/Percussion", "Hip Hop Drums Package/Percussion/r-loops_PERC_04.wav"),
    # Vox (8 — tonal, melodic)
    ("Hip Hop Vox 01", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_01.wav"),
    ("Hip Hop Vox 02 (D)", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_02_D.wav"),
    ("Hip Hop Vox 03", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_03.wav"),
    ("Hip Hop Vox 04", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_04.wav"),
    ("Hip Hop Vox 05 (E)", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_05_E.wav"),
    ("Hip Hop Vox 06 (F#)", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_06_F_.wav"),
    ("Hip Hop Vox 07 (D)", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_07_D.wav"),
    ("Hip Hop Vox 08 (F#)", "Hip Hop Drums/Vox", "Hip Hop Drums Package/Vox/r-loops_VOX_08_F_.wav"),

    # ── EDM Drums Package — 70 files ───────────────────────────────────────
    # Dubstep Kicks (10)
    ("Dubstep Acoustic Kick 1", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Acoustic_Kick_1.wav"),
    ("Dubstep Acoustic Kick 2", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Acoustic_Kick_2.wav"),
    ("Dubstep Airy Kick", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Airy_Kick.wav"),
    ("Dubstep Clappy Kick", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Clappy_Kick.wav"),
    ("Dubstep Kick 1", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Dubstep_Kick_1.wav"),
    ("Dubstep Kick 2", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Dubstep_Kick_2.wav"),
    ("Dubstep Kick 3", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Dubstep_Kick_3.wav"),
    ("Dubstep Muffled Kick", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Muffled_Kick.wav"),
    ("Dubstep Pre-Kick", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Pre-Kick.wav"),
    ("Dubstep Smol Kick", "EDM Drums/Dubstep Kicks", "EDM Drums Package/Dubstep Kicks/WADS_Smol_Kick.wav"),
    # Hardstyle Kicks (10)
    ("Hardstyle Kick 1 (A)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick1_A.wav"),
    ("Hardstyle Kick 1 (E)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick1_E.wav"),
    ("Hardstyle Kick 2 (D)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick2_D.wav"),
    ("Hardstyle Kick 2 (F)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick2_F.wav"),
    ("Hardstyle Kick 3 (A#)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick3_A#.wav"),
    ("Hardstyle Kick 3 (F#)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick3_F#.wav"),
    ("Hardstyle Kick 5 (G)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick5_G.wav"),
    ("Hardstyle Kick 6 (G)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick6_G.wav"),
    ("Hardstyle Kick 7 (B)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick7_B.wav"),
    ("Hardstyle Kick 7 (E)", "EDM Drums/Hardstyle Kicks", "EDM Drums Package/Hardstyle Kicks/BE_HM_Kick7_E.wav"),
    # House Kicks (10)
    ("House Kick 01", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_01.wav"),
    ("House Kick 02", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_02.wav"),
    ("House Kick 03", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_03.wav"),
    ("House Kick 04", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_04.wav"),
    ("House Kick 05", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_05.wav"),
    ("House Kick 06", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_06.wav"),
    ("House Kick 07", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_07.wav"),
    ("House Kick 08", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_08.wav"),
    ("House Kick 09", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_09.wav"),
    ("House Kick 10", "EDM Drums/House Kicks", "EDM Drums Package/House Kicks/WA_HR_House_Kick_10.wav"),
    # Hardstyle Snares (10)
    ("Hardstyle Snare 1", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare1.wav"),
    ("Hardstyle Snare 2", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare2.wav"),
    ("Hardstyle Snare 3", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare3.wav"),
    ("Hardstyle Snare 4", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare4.wav"),
    ("Hardstyle Snare 5", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare5.wav"),
    ("Hardstyle Snare 6", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare6.wav"),
    ("Hardstyle Snare 7", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare7.wav"),
    ("Hardstyle Snare 8", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare8.wav"),
    ("Hardstyle Snare 9", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare9.wav"),
    ("Hardstyle Snare 10", "EDM Drums/Hardstyle Snares", "EDM Drums Package/Hardstyle Snares/BE_HM_Snare10.wav"),
    # House Claps & Snares (10)
    ("House Clap-Snare 01", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_01.wav"),
    ("House Clap-Snare 02", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_02.wav"),
    ("House Clap-Snare 03", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_03.wav"),
    ("House Clap-Snare 04", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_04.wav"),
    ("House Clap-Snare 05", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_05.wav"),
    ("House Clap-Snare 06", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_06.wav"),
    ("House Clap-Snare 07", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_07.wav"),
    ("House Clap-Snare 08", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_08.wav"),
    ("House Clap-Snare 09", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_09.wav"),
    ("House Clap-Snare 10", "EDM Drums/House Claps & Snares", "EDM Drums Package/House Claps & Snares/WA_HR_House_Claps_&_Snares_10.wav"),
    # Hardstyle Percussion (10)
    ("Hardstyle Perc 1", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion1.wav"),
    ("Hardstyle Perc 2", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion2.wav"),
    ("Hardstyle Perc 3", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion3.wav"),
    ("Hardstyle Perc 4", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion4.wav"),
    ("Hardstyle Perc 5", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion5.wav"),
    ("Hardstyle Perc 6", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion6.wav"),
    ("Hardstyle Perc 7", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion7.wav"),
    ("Hardstyle Perc 8", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion8.wav"),
    ("Hardstyle Perc 9", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion9.wav"),
    ("Hardstyle Perc 10", "EDM Drums/Hardstyle Percussion", "EDM Drums Package/Hardstyle Percussion/BE_HM_Percussion10.wav"),
    # House Percussion (10)
    ("House Perc 01", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_01.wav"),
    ("House Perc 02", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_02.wav"),
    ("House Perc 03", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_03.wav"),
    ("House Perc 04", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_04.wav"),
    ("House Perc 05", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_05.wav"),
    ("House Perc 06", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_06.wav"),
    ("House Perc 07", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_07.wav"),
    ("House Perc 08", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_08.wav"),
    ("House Perc 09", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_09.wav"),
    ("House Perc 10", "EDM Drums/House Percussion", "EDM Drums Package/House Percussion/WA_HR_House_Perc_10.wav"),
]

def write_harmless_preset_xml(target_dir: Path, name: str, overrides: dict) -> Path:
    """Mirrors write_preset_xml but uses HARM_DEFAULTS + the harm prefix.
    Param IDs are written as `tk_lay_0_harm_<param>` for trackId-substitution
    portability across drum slots / layer pages."""
    target_dir.mkdir(parents=True, exist_ok=True)
    params = {**HARM_DEFAULTS, **overrides}
    prefix = "tk_lay_0_harm_"
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<HarmlessState>']
    for k, v in params.items():
        if isinstance(v, int):
            vstr = f"{v}.0"
        else:
            vstr = repr(float(v))
        lines.append(f'  <PARAM id="{prefix}{k}" value="{vstr}"/>')
    lines.append('</HarmlessState>')
    out = target_dir / f"{name}.xml"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out

def merge_with_dedupe(existing_recipes, new_with_cats, category_map):
    """Returns (merged_recipes, dedupe_report).

    existing_recipes : list of (name, overrides) tuples
    new_with_cats    : list of (name, target_cat, overrides) tuples
    category_map     : dict (mutated — new categories added)
    """
    existing_by_name = {n: o for n, o in existing_recipes}
    merged = list(existing_recipes)
    report = {"added": [], "skipped_dup": [], "kept_with_suffix": []}
    for name, target_cat, overrides in new_with_cats:
        if name in existing_by_name:
            if _params_match(overrides, existing_by_name[name]):
                report["skipped_dup"].append((name, target_cat))
                continue
            new_name = f"{name} (2)"
            merged.append((new_name, overrides))
            category_map[new_name] = target_cat
            report["kept_with_suffix"].append((name, new_name, target_cat))
            continue
        merged.append((name, overrides))
        category_map[name] = target_cat
        report["added"].append((name, target_cat))
    return merged, report

# ---- XML emitter ------------------------------------------------------------

def write_preset_xml(target_dir: Path, engine: tuple, name: str, overrides: dict,
                     extra_overrides: dict | None = None):
    """engine = (tag, root_name) e.g. ("bss", "BaySickSynthState").
    extra_overrides applied AFTER per-recipe overrides (used to inject e.g.
    cutSelf=1 across an entire bank like Drums)."""
    tag, root_name = engine
    prefix = f"tk_lay_0_{tag}_"
    target_dir.mkdir(parents=True, exist_ok=True)
    params = {**DEFAULTS, **overrides}
    if extra_overrides:
        params.update(extra_overrides)
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             f'<{root_name}>']
    for k, v in params.items():
        if isinstance(v, int):
            vstr = f"{v}.0"
        else:
            vstr = repr(float(v))
        lines.append(f'  <PARAM id="{prefix}{k}" value="{vstr}"/>')
    lines.append(f'</{root_name}>')
    out = target_dir / f"{name}.xml"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out

# ═══════════════════════════════════════════════════════════════════════════
# Batch 5: Factory kit generation
# ═══════════════════════════════════════════════════════════════════════════
# Kit XML format (factory-reference flavor — no embedded engine state):
#   <BaySickKit name="..." version="1">
#     <Drum slot="N" engine="BaySickSynth" presetPath="808 Group/808 Kick.xml"
#            locked="1"/>
#     ...
#   </BaySickKit>
#
# StandaloneEditor::loadKit reads this format and dispatches each Drum
# entry's preset through DrumPage::loadSynthPreset / loadPlayerPreset.
# Drums in factory kits are locked (locked="1") so beginners don't
# accidentally swap one out — they unlock via the drum's right-click menu
# first.

KITS_DIR      = USERPROFILE / "Documents" / "BaySickDAW" / "Kits" / "Factory"
TEMPLATES_DIR = USERPROFILE / "Documents" / "BaySickDAW" / "Templates" / "Factory"

# 10 styles × 16 drum slots each.  Basic = first 4, Condensed = first 8,
# Full = all 16.  Each entry is (presetPath, engine).
# All factory drum presets are BaySickSynth-engine (the gen_factory_presets
# script writes them with ENGINE_BSS).
KIT_STYLES = {
    "TR-808": [
        ("808 Group/808 Kick.xml",          "BaySickSynth"),
        ("808 Group/808 Snare.xml",         "BaySickSynth"),
        ("808 Group/808 Closed Hat.xml",    "BaySickSynth"),
        ("808 Group/808 Open Hat.xml",      "BaySickSynth"),
        # Condensed extras
        ("808 Group/808 Handclap.xml",      "BaySickSynth"),
        ("808 Group/808 Cowbell.xml",       "BaySickSynth"),
        ("808 Group/808 Tom Lo.xml",        "BaySickSynth"),
        ("808 Group/808 Conga Lo.xml",      "BaySickSynth"),
        # Full extras
        ("808 Group/808 Rimshot.xml",       "BaySickSynth"),
        ("808 Group/808 Claves.xml",        "BaySickSynth"),
        ("808 Group/808 Maraca.xml",        "BaySickSynth"),
        ("808 Group/808 Conga Mid.xml",     "BaySickSynth"),
        ("808 Group/808 Conga Hi.xml",      "BaySickSynth"),
        ("808 Group/808 Tom Mid.xml",       "BaySickSynth"),
        ("808 Group/808 Tom Hi.xml",        "BaySickSynth"),
        ("808 Group/Long 808 Trap Kick.xml","BaySickSynth"),
    ],
    "TR-909": [
        ("909 Group/909 Kick.xml",          "BaySickSynth"),
        ("909 Group/909 Snare.xml",         "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",    "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",      "BaySickSynth"),
        # Condensed
        ("909 Group/Synthesized Clap.xml",  "BaySickSynth"),
        ("909 Group/909 Ride Crash.xml",    "BaySickSynth"),
        ("909 Group/909 Tom Lo.xml",        "BaySickSynth"),
        ("909 Group/909 Tom Mid.xml",       "BaySickSynth"),
        # Full
        ("909 Group/909 Tom Hi.xml",            "BaySickSynth"),
        ("909 Group/909-Style Kick.xml",        "BaySickSynth"),
        ("909 Group/Classic Analog Snare.xml",  "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",      "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",         "BaySickSynth"),
        ("909 Group/Pedal Hat (Chick).xml",     "BaySickSynth"),
        ("909 Group/Deep House Thump.xml",      "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",     "BaySickSynth"),
    ],
    "TR-606": [
        ("606 Group/606 Kick.xml",              "BaySickSynth"),
        ("606 Group/606 Snare.xml",             "BaySickSynth"),
        ("606 Group/606 Closed Hat.xml",        "BaySickSynth"),
        ("606 Group/606 Open Hat.xml",          "BaySickSynth"),
        # Condensed
        ("606 Group/Electro Castanet.xml",      "BaySickSynth"),
        ("606 Group/High Woodblock.xml",        "BaySickSynth"),
        ("606 Group/606 Low Tom.xml",           "BaySickSynth"),
        ("606 Group/606 High Tom.xml",          "BaySickSynth"),
        # Full — borrows from neighbouring groups for missing pieces
        ("606 Group/Analog Metronome.xml",      "BaySickSynth"),
        ("606 Group/Master Sync Tick.xml",      "BaySickSynth"),
        ("606 Group/606 Kick (2).xml",          "BaySickSynth"),
        ("606 Group/606 Snare (2).xml",         "BaySickSynth"),
        ("808 Group/808 Handclap.xml",          "BaySickSynth"),
        ("909 Group/909 Tom Hi.xml",            "BaySickSynth"),
        ("909 Group/909 Tom Mid.xml",           "BaySickSynth"),
        ("909 Group/909 Tom Lo.xml",            "BaySickSynth"),
    ],
    "80s Electronic": [
        # Special Basic — Simmons doesn't really do hi-hats, lean tom-and-snare.
        ("Simmons Group/Simmons Kick.xml",      "BaySickSynth"),
        ("Simmons Group/Simmons Snare.xml",     "BaySickSynth"),
        ("Simmons Group/Simmons Tom Lo.xml",    "BaySickSynth"),
        ("Simmons Group/Simmons Tom Hi.xml",    "BaySickSynth"),
        # Condensed
        ("Simmons Group/Simmons SDS-7 Kick.xml","BaySickSynth"),
        ("Simmons Group/Resonant Synth Tom.xml","BaySickSynth"),
        ("Simmons Group/Simmons 80s Tom.xml",   "BaySickSynth"),
        ("Simmons Group/Gated 80s Snare.xml",   "BaySickSynth"),
        # Full
        ("Simmons Group/Disco Syndrum.xml",         "BaySickSynth"),
        ("Simmons Group/Vintage Simmons Snare.xml", "BaySickSynth"),
        ("Simmons Group/Tom Thud (Dead Room).xml",  "BaySickSynth"),
        ("Simmons Group/Simmons Low Tom.xml",       "BaySickSynth"),
        ("Simmons Group/Simmons Sweep FX.xml",      "BaySickSynth"),
        ("Simmons Group/Sci-Fi Laser Zap.xml",      "BaySickSynth"),
        ("Simmons Group/Electro Pew-Pew.xml",       "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",            "BaySickSynth"),
    ],
    "FM Digital": [
        # Special Basic — Yamaha emphasizes bells/rides over standard hats.
        ("Yamaha Group/RX-11 Kick.xml",             "BaySickSynth"),
        ("Yamaha Group/RX-11 Snare.xml",            "BaySickSynth"),
        ("Yamaha Group/DX7 Glass.xml",              "BaySickSynth"),
        ("Yamaha Group/Synthetic Ride Bell.xml",    "BaySickSynth"),
        # Condensed
        ("Yamaha Group/DX7 Metal.xml",              "BaySickSynth"),
        ("Yamaha Group/FM Metallic Cowbell.xml",    "BaySickSynth"),
        ("Yamaha Group/Agogo Bell.xml",             "BaySickSynth"),
        ("Yamaha Group/Synthetic Triangle.xml",     "BaySickSynth"),
        # Full
        ("Yamaha Group/DX7 Woodblock.xml",          "BaySickSynth"),
        ("Yamaha Group/Synth Gong.xml",             "BaySickSynth"),
        ("Yamaha Group/Shimmering Ride.xml",        "BaySickSynth"),
        ("Yamaha Group/FM Pluck Percussion.xml",    "BaySickSynth"),
        ("Yamaha Group/FM Digital Bell.xml",        "BaySickSynth"),
        ("Yamaha Group/Gong Crash Mix.xml",         "BaySickSynth"),
        ("Yamaha Group/DX Style Tubulum.xml",       "BaySickSynth"),
        ("Tuned Percussion/Steel Drum Perc.xml",    "BaySickSynth"),
    ],
    "Acoustic Rock": [
        # Basic — 909 sounds are the most "acoustic-feeling" drum-machine kicks/snares.
        ("909 Group/909 Kick.xml",                  "BaySickSynth"),
        ("909 Group/Classic Analog Snare.xml",      "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",            "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",              "BaySickSynth"),
        # Condensed
        ("Hand Percussion/Stomp Kick.xml",          "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",          "BaySickSynth"),
        ("Hand Percussion/Bongo Hi.xml",            "BaySickSynth"),
        ("Hand Percussion/Bongo Lo.xml",            "BaySickSynth"),
        # Full
        ("Hand Percussion/Rimshot Acoustic.xml",    "BaySickSynth"),
        ("Hand Percussion/Stick-Hit Drum.xml",      "BaySickSynth"),
        ("Hand Percussion/Tabla Hi.xml",            "BaySickSynth"),
        ("Hand Percussion/Tabla Lo.xml",            "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",       "BaySickSynth"),
        ("Hand Percussion/Shaker.xml",              "BaySickSynth"),
        ("Hand Percussion/Sleigh Bells.xml",        "BaySickSynth"),
        ("Tuned Percussion/Marimba.xml",            "BaySickSynth"),
    ],
    "Trap": [
        ("808 Group/Long 808 Trap Kick.xml",            "BaySickSynth"),
        ("Modern EDM & Trap/Trap Snare Roll.xml",       "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",              "BaySickSynth"),
        ("808 Group/808 Open Hat.xml",                  "BaySickSynth"),
        # Condensed
        ("808 Group/808 Cowbell.xml",                   "BaySickSynth"),
        ("808 Group/808 Handclap.xml",                  "BaySickSynth"),
        ("808 Group/808 Tom Lo.xml",                    "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                 "BaySickSynth"),
        # Full
        ("Modern EDM & Trap/Distorted Slap Clap.xml",   "BaySickSynth"),
        ("808 Group/808 Conga Lo.xml",                  "BaySickSynth"),
        ("808 Group/808 Rimshot.xml",                   "BaySickSynth"),
        ("Modern EDM & Trap/Stuttering Snare Tail.xml", "BaySickSynth"),
        ("909 Group/909 Snare.xml",                     "BaySickSynth"),
        ("Modern EDM & Trap/Granular Smear Snare.xml",  "BaySickSynth"),
        ("Modern EDM & Trap/Festival Big Room Kick.xml","BaySickSynth"),
        ("Modern EDM & Trap/Psytrance Zap Kick.xml",    "BaySickSynth"),
    ],
    "Lo-Fi Hip-Hop": [
        ("Lo-Fi, Chiptune & Texture/8-Bit Kick.xml",            "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/8-Bit Snare.xml",           "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("Lo-Fi, Chiptune & Texture/Vinyl Crackle Layer.xml",   "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Crunch Snare.xml",          "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Trash Cymbal.xml",          "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Lo-Fi Digital Shaker.xml",  "BaySickSynth"),
        # Full
        ("Lo-Fi, Chiptune & Texture/White Noise Sizzle.xml",    "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Square Pop.xml",            "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Glitch Hop Squash.xml",     "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Fake Reverb Snare Tail.xml","BaySickSynth"),
        ("808 Group/808 Tom Lo.xml",                            "BaySickSynth"),
        ("Hand Percussion/Tabla Hi.xml",                        "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        ("808 Group/808 Handclap.xml",                          "BaySickSynth"),
    ],
    "EDM Big Room": [
        # Special Basic — clap > HHC for EDM.
        ("Modern EDM & Trap/Festival Big Room Kick.xml",        "BaySickSynth"),
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                         "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("Modern EDM & Trap/Distorted Gabber Kick.xml",         "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",                      "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
        # Full
        ("Modern EDM & Trap/Psytrance Zap Kick.xml",            "BaySickSynth"),
        ("Modern EDM & Trap/Donk Bass Perc.xml",                "BaySickSynth"),
        ("909 Group/909 Tom Hi.xml",                            "BaySickSynth"),
        ("909 Group/909 Ride Crash.xml",                        "BaySickSynth"),
        ("Modern EDM & Trap/Trap Snare Roll.xml",               "BaySickSynth"),
        ("Modern EDM & Trap/Distorted Slap Clap.xml",           "BaySickSynth"),
        ("Modern EDM & Trap/Underwater Kick.xml",               "BaySickSynth"),
        ("Modern EDM & Trap/Dubstep Sub Impact.xml",            "BaySickSynth"),
    ],
    "Cinematic": [
        # Special Basic — FX-driven, no traditional drums.
        ("Cinematic, Industrial & FX/Cinematic Sub Drop.xml",   "BaySickSynth"),
        ("Cinematic, Industrial & FX/Anvil Strike.xml",         "BaySickSynth"),
        ("Cinematic, Industrial & FX/Riser Sweep FX.xml",       "BaySickSynth"),
        ("Cinematic, Industrial & FX/Impact Downshifter.xml",   "BaySickSynth"),
        # Condensed
        ("Cinematic, Industrial & FX/Reverse Cymbal Sweep.xml", "BaySickSynth"),
        ("Cinematic, Industrial & FX/Reverse Snare Swell.xml",  "BaySickSynth"),
        ("Cinematic, Industrial & FX/Water Plop.xml",           "BaySickSynth"),
        ("Cinematic, Industrial & FX/Reverse Suck FX.xml",      "BaySickSynth"),
        # Full
        ("Cinematic, Industrial & FX/Raygun FX.xml",            "BaySickSynth"),
        ("Cinematic, Industrial & FX/Formant Vocal Uh!.xml",    "BaySickSynth"),
        ("Simmons Group/Sci-Fi Laser Zap.xml",                  "BaySickSynth"),
        ("Tuned Percussion/Tubular Bells.xml",                  "BaySickSynth"),
        ("Tuned Percussion/Glockenspiel.xml",                   "BaySickSynth"),
        ("Tuned Percussion/Triangle.xml",                       "BaySickSynth"),
        ("Tuned Percussion/Vibraphone Roll.xml",                "BaySickSynth"),
        ("808 Group/808 Tom Lo.xml",                            "BaySickSynth"),
    ],
    # ── 2026-04-26: 8 new kits inspired by the YouTube reference list ────────
    "French Disco": [
        # Daft Punk RAM — disco-funk, vintage-warm acoustic-feel.
        ("909 Group/Linn Disco Kick.xml",                       "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        ("Simmons Group/Disco Syndrum.xml",                     "BaySickSynth"),
        # Full
        ("808 Group/808 Cowbell.xml",                           "BaySickSynth"),
        ("Hand Percussion/Bongo Hi.xml",                        "BaySickSynth"),
        ("Hand Percussion/Bongo Lo.xml",                        "BaySickSynth"),
        ("808 Group/808 Tom Lo.xml",                            "BaySickSynth"),
        ("808 Group/808 Tom Hi.xml",                            "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
        ("Hand Percussion/Stick-Hit Drum.xml",                  "BaySickSynth"),
        ("Hand Percussion/Sleigh Bells.xml",                    "BaySickSynth"),
    ],
    "Psytrance": [
        # Astrix, Infected Mushroom — heavy electronic, gating.
        ("Modern EDM & Trap/Festival Big Room Kick.xml",        "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",                      "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("Modern EDM & Trap/Distorted Slap Clap.xml",           "BaySickSynth"),
        ("808 Group/Acid Tom Sweep.xml",                        "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                         "BaySickSynth"),
        # Full
        ("Modern EDM & Trap/Distorted Gabber Kick.xml",         "BaySickSynth"),
        ("Modern EDM & Trap/Psytrance Zap Kick.xml",            "BaySickSynth"),
        ("Modern EDM & Trap/Stuttering Snare Tail.xml",         "BaySickSynth"),
        ("Modern EDM & Trap/Granular Smear Snare.xml",          "BaySickSynth"),
        ("909 Group/909 Tom Hi.xml",                            "BaySickSynth"),
        ("909 Group/909 Tom Mid.xml",                           "BaySickSynth"),
        ("909 Group/909 Tom Lo.xml",                            "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
    ],
    "Boom-Bap Hip-Hop": [
        # RTJ, Logic, Aesop Rock — vintage hip-hop drums.
        ("909 Group/Linn Disco Kick.xml",                       "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("808 Group/808 Handclap.xml",                          "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                         "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
        ("Hand Percussion/Stick-Hit Drum.xml",                  "BaySickSynth"),
        # Full
        ("808 Group/808 Tom Lo.xml",                            "BaySickSynth"),
        ("808 Group/808 Tom Hi.xml",                            "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Vinyl Crackle Layer.xml",   "BaySickSynth"),
        ("Lo-Fi, Chiptune & Texture/Trash Cymbal.xml",          "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
        ("909 Group/Pedal Hat (Chick).xml",                     "BaySickSynth"),
    ],
    "Synth-Pop": [
        # Depeche Mode Violator — gated 80s drums + FM.
        ("909 Group/909-Style Kick.xml",                        "BaySickSynth"),
        ("Simmons Group/Gated 80s Snare Long.xml",              "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",                      "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("Simmons Group/Simmons Snare.xml",                     "BaySickSynth"),
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("Simmons Group/Simmons Tom Lo.xml",                    "BaySickSynth"),
        ("Simmons Group/Simmons Tom Hi.xml",                    "BaySickSynth"),
        # Full
        ("Simmons Group/Vintage Simmons Snare.xml",             "BaySickSynth"),
        ("Simmons Group/Resonant Synth Tom.xml",                "BaySickSynth"),
        ("Simmons Group/Disco Syndrum.xml",                     "BaySickSynth"),
        ("Yamaha Group/Synth Gong.xml",                         "BaySickSynth"),
        ("Yamaha Group/Synthetic Triangle.xml",                 "BaySickSynth"),
        ("Simmons Group/Sci-Fi Laser Zap.xml",                  "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                         "BaySickSynth"),
        ("606 Group/Master Sync Tick.xml",                      "BaySickSynth"),
    ],
    "Bass Music": [
        # Skrillex Quest For Fire — modern bass design, aggressive.
        ("Modern EDM & Trap/Bass Music Impact.xml",             "BaySickSynth"),
        ("Modern EDM & Trap/Trap Snare Roll.xml",               "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",                      "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("Modern EDM & Trap/Distorted Slap Clap.xml",           "BaySickSynth"),
        ("Modern EDM & Trap/Stuttering Snare Tail.xml",         "BaySickSynth"),
        ("Modern EDM & Trap/Festival Big Room Kick.xml",        "BaySickSynth"),
        ("Modern EDM & Trap/Granular Smear Snare.xml",          "BaySickSynth"),
        # Full
        ("Modern EDM & Trap/Distorted Gabber Kick.xml",         "BaySickSynth"),
        ("Modern EDM & Trap/Underwater Kick.xml",               "BaySickSynth"),
        ("Modern EDM & Trap/Psytrance Zap Kick.xml",            "BaySickSynth"),
        ("Modern EDM & Trap/Donk Bass Perc.xml",                "BaySickSynth"),
        ("Modern EDM & Trap/Dubstep Sub Impact.xml",            "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                         "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
        ("808 Group/808 Tom Lo.xml",                            "BaySickSynth"),
    ],
    "Krautrock": [
        # Kraftwerk Autobahn — vintage analog, motorik.
        ("909 Group/909-Style Kick.xml",                        "BaySickSynth"),
        ("Simmons Group/Vintage Simmons Snare.xml",             "BaySickSynth"),
        ("606 Group/Master Sync Tick.xml",                      "BaySickSynth"),
        ("606 Group/606 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("606 Group/Analog Metronome.xml",                      "BaySickSynth"),
        ("909 Group/Pedal Hat (Chick).xml",                     "BaySickSynth"),
        ("606 Group/Electro Castanet.xml",                      "BaySickSynth"),
        ("Yamaha Group/FM Robot Stab.xml",                      "BaySickSynth"),
        # Full
        ("Yamaha Group/Synthetic Triangle.xml",                 "BaySickSynth"),
        ("Simmons Group/Simmons Sweep FX.xml",                  "BaySickSynth"),
        ("Yamaha Group/Synth Gong.xml",                         "BaySickSynth"),
        ("606 Group/606 Low Tom.xml",                           "BaySickSynth"),
        ("606 Group/606 High Tom.xml",                          "BaySickSynth"),
        ("606 Group/High Woodblock.xml",                        "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
        ("Simmons Group/Sci-Fi Laser Zap.xml",                  "BaySickSynth"),
    ],
    "Prog House": [
        # Deadmau5 Random Album Title — clean modern house drums.
        ("Modern EDM & Trap/Festival Big Room Kick.xml",        "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",                      "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                         "BaySickSynth"),
        ("909 Group/Pedal Hat (Chick).xml",                     "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
        # Full
        ("909 Group/909 Ride Crash.xml",                        "BaySickSynth"),
        ("909 Group/909-Style Kick.xml",                        "BaySickSynth"),
        ("Modern EDM & Trap/Distorted Gabber Kick.xml",         "BaySickSynth"),
        ("909 Group/909 Tom Hi.xml",                            "BaySickSynth"),
        ("909 Group/909 Tom Mid.xml",                           "BaySickSynth"),
        ("909 Group/909 Tom Lo.xml",                            "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
        ("808 Group/808 Cowbell.xml",                           "BaySickSynth"),
    ],
    "Vintage Soul": [
        # Gnarls Barkley + Aretha — 60s/70s crate-dig soul drums.
        ("909 Group/Linn Disco Kick.xml",                       "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
        ("Hand Percussion/Stomp Kick.xml",                      "BaySickSynth"),
        ("Hand Percussion/Stick-Hit Drum.xml",                  "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        # Full
        ("Hand Percussion/Bongo Hi.xml",                        "BaySickSynth"),
        ("Hand Percussion/Bongo Lo.xml",                        "BaySickSynth"),
        ("Hand Percussion/Tribal Pow Wow Drum.xml",             "BaySickSynth"),
        ("Hand Percussion/Tabla Hi.xml",                        "BaySickSynth"),
        ("Hand Percussion/Tabla Lo.xml",                        "BaySickSynth"),
        ("Hand Percussion/Sleigh Bells.xml",                    "BaySickSynth"),
        ("Hand Percussion/Hollow Woodblock.xml",                "BaySickSynth"),
        ("808 Group/808 Cowbell.xml",                           "BaySickSynth"),
    ],
    # ── 2026-04-26 (round 2): 5 more styles spanning genre gaps ──────────────
    "Reggae": [
        # Dub / dancehall — laid-back acoustic feel + 808 sub.
        ("909 Group/Linn Disco Kick.xml",                       "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("808 Group/808 Cowbell.xml",                           "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        # Full
        ("808 Group/808 Tom Lo.xml",                            "BaySickSynth"),
        ("808 Group/808 Tom Hi.xml",                            "BaySickSynth"),
        ("Hand Percussion/Bongo Hi.xml",                        "BaySickSynth"),
        ("Hand Percussion/Bongo Lo.xml",                        "BaySickSynth"),
        ("Hand Percussion/Cajon Hit.xml",                       "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
        ("Hand Percussion/Hollow Woodblock.xml",                "BaySickSynth"),
        ("Hand Percussion/Stick-Hit Drum.xml",                  "BaySickSynth"),
    ],
    "Latin Salsa": [
        # Salsa / timba — heavy on bongos / congas / clave / cowbell.
        ("Hand Percussion/Stomp Kick.xml",                      "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("Hand Percussion/Bongo Hi.xml",                        "BaySickSynth"),
        ("Hand Percussion/Bongo Lo.xml",                        "BaySickSynth"),
        ("808 Group/808 Conga Hi.xml",                          "BaySickSynth"),
        ("808 Group/808 Conga Lo.xml",                          "BaySickSynth"),
        # Full
        ("808 Group/808 Conga Mid.xml",                         "BaySickSynth"),
        ("Hand Percussion/Djembe.xml",                          "BaySickSynth"),
        ("Hand Percussion/Frame Drum.xml",                      "BaySickSynth"),
        ("808 Group/808 Cowbell.xml",                           "BaySickSynth"),
        ("808 Group/808 Claves.xml",                            "BaySickSynth"),
        ("808 Group/808 Maraca.xml",                            "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
    ],
    "Funk": [
        # Tight 909 + lots of perc.
        ("909 Group/909 Kick.xml",                              "BaySickSynth"),
        ("909 Group/909 Snare.xml",                             "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("808 Group/808 Handclap.xml",                          "BaySickSynth"),
        ("909 Group/Synthesized Clap.xml",                      "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        # Full
        ("808 Group/808 Cowbell.xml",                           "BaySickSynth"),
        ("909 Group/909 Tom Hi.xml",                            "BaySickSynth"),
        ("909 Group/909 Tom Mid.xml",                           "BaySickSynth"),
        ("909 Group/909 Tom Lo.xml",                            "BaySickSynth"),
        ("909 Group/909 Ride Crash.xml",                        "BaySickSynth"),
        ("909 Group/Pedal Hat (Chick).xml",                     "BaySickSynth"),
        ("Hand Percussion/Stick-Hit Drum.xml",                  "BaySickSynth"),
        ("909 Group/Synth Rimshot.xml",                         "BaySickSynth"),
    ],
    "Industrial": [
        # NIN-flavor — Cinematic FX + 909 + metal hits.
        ("Modern EDM & Trap/Distorted Gabber Kick.xml",         "BaySickSynth"),
        ("Simmons Group/Gated 80s Snare Long.xml",              "BaySickSynth"),
        ("909 Group/Tight Closed Hat.xml",                      "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("Cinematic, Industrial & FX/Anvil Strike.xml",         "BaySickSynth"),
        ("Cinematic, Industrial & FX/Metal Pipe Hit.xml",       "BaySickSynth"),
        ("Modern EDM & Trap/China Cymbal.xml",                  "BaySickSynth"),
        ("Cinematic, Industrial & FX/Container Slam.xml",       "BaySickSynth"),
        # Full
        ("Modern EDM & Trap/Distorted Slap Clap.xml",           "BaySickSynth"),
        ("Modern EDM & Trap/Granular Smear Snare.xml",          "BaySickSynth"),
        ("Modern EDM & Trap/Stuttering Snare Tail.xml",         "BaySickSynth"),
        ("Modern EDM & Trap/Bass Music Impact.xml",             "BaySickSynth"),
        ("Simmons Group/Sci-Fi Laser Zap.xml",                  "BaySickSynth"),
        ("Cinematic, Industrial & FX/Reverse Snare Swell.xml",  "BaySickSynth"),
        ("Cinematic, Industrial & FX/Cinematic Sub Drop.xml",   "BaySickSynth"),
        ("909 Group/White Noise Crash.xml",                     "BaySickSynth"),
    ],
    "Jazz Fusion": [
        # Synthetic jazz drums + tuned mallets — 909 acoustic-leaning kit.
        ("909 Group/Acoustic Live Kick.xml",                    "BaySickSynth"),
        ("909 Group/Classic Analog Snare.xml",                  "BaySickSynth"),
        ("909 Group/909 Closed Hat.xml",                        "BaySickSynth"),
        ("909 Group/909 Open Hat.xml",                          "BaySickSynth"),
        # Condensed
        ("909 Group/909 Ride Crash.xml",                        "BaySickSynth"),
        ("909 Group/Splash Cymbal.xml",                         "BaySickSynth"),
        ("909 Group/Pedal Hat (Chick).xml",                     "BaySickSynth"),
        ("Hand Percussion/Stick-Hit Drum.xml",                  "BaySickSynth"),
        # Full
        ("909 Group/909 Tom Hi.xml",                            "BaySickSynth"),
        ("909 Group/909 Tom Mid.xml",                           "BaySickSynth"),
        ("909 Group/909 Tom Lo.xml",                            "BaySickSynth"),
        ("Hand Percussion/Cabasa Shaker.xml",                   "BaySickSynth"),
        ("Hand Percussion/Tambourine.xml",                      "BaySickSynth"),
        ("Tuned Percussion/Marimba.xml",                        "BaySickSynth"),
        ("Tuned Percussion/Vibraphone Roll.xml",                "BaySickSynth"),
        ("Tuned Percussion/Triangle.xml",                       "BaySickSynth"),
    ],
    # ── 2026-04-26 (round 4): 6 sample-based kit styles using real .wav
    # samples from the Hip Hop + EDM Drums Packages.  All preset paths are
    # rooted at "BaySickPlayer/" so generate_factory_kits keeps them as-is.
    "Hip Hop (Real)": [
        ("BaySickPlayer/Hip Hop Drums/Kicks/Hip Hop Kick 01.xml",         "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Snares/Hip Hop Snare 01.xml",       "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Closed Hat 01.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Open Hat 01.xml",      "BaySickPlayer"),
        # Condensed
        ("BaySickPlayer/Hip Hop Drums/Claps/Hip Hop Clap 01.xml",         "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Snares/Hip Hop Snare 02.xml",       "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Closed Hat 02.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Toms/Hip Hop Tom 01.xml",           "BaySickPlayer"),
        # Full
        ("BaySickPlayer/Hip Hop Drums/Toms/Hip Hop Tom 02.xml",           "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Toms/Hip Hop Tom 03.xml",           "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Cymbal 01.xml",     "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Crash 01.xml",      "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Beatbox/Hip Hop Beatbox 01.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Beatbox/Hip Hop Beatbox 02.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Percussion/Hip Hop Perc 01.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/FX/Hip Hop FX 01.xml",              "BaySickPlayer"),
    ],
    "Lo-Fi (Real)": [
        ("BaySickPlayer/Hip Hop Drums/Kicks/Hip Hop Kick 03.xml",         "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Snares/Hip Hop Snare 03.xml",       "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Closed Hat 03.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Open Hat 03.xml",      "BaySickPlayer"),
        # Condensed
        ("BaySickPlayer/Hip Hop Drums/Beatbox/Hip Hop Beatbox 01.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Beatbox/Hip Hop Beatbox 03.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/FX/Hip Hop FX 02 (90 BPM).xml",     "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Toms/Hip Hop Tom 02.xml",           "BaySickPlayer"),
        # Full
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 01.xml",            "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 02 (D).xml",        "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Percussion/Hip Hop Perc 02.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Percussion/Hip Hop Perc 03.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Cymbal 02.xml",     "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Crash 02.xml",      "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Claps/Hip Hop Clap 02.xml",         "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/FX/Hip Hop FX 03.xml",              "BaySickPlayer"),
    ],
    "Dubstep (Real)": [
        ("BaySickPlayer/EDM Drums/Dubstep Kicks/Dubstep Kick 1.xml",                  "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Snares/Hardstyle Snare 1.xml",            "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Closed Hat 04.xml",                "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Open Hat 04.xml",                  "BaySickPlayer"),
        # Condensed
        ("BaySickPlayer/EDM Drums/House Claps & Snares/House Clap-Snare 01.xml",      "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Snares/Hardstyle Snare 2.xml",            "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Dubstep Kicks/Dubstep Acoustic Kick 1.xml",         "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Dubstep Kicks/Dubstep Airy Kick.xml",               "BaySickPlayer"),
        # Full
        ("BaySickPlayer/EDM Drums/Dubstep Kicks/Dubstep Clappy Kick.xml",             "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Dubstep Kicks/Dubstep Muffled Kick.xml",            "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Dubstep Kicks/Dubstep Pre-Kick.xml",                "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Dubstep Kicks/Dubstep Smol Kick.xml",               "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Percussion/Hardstyle Perc 1.xml",         "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Percussion/Hardstyle Perc 2.xml",         "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Cymbal 01.xml",                 "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Crash 01.xml",                  "BaySickPlayer"),
    ],
    "Hardstyle (Real)": [
        ("BaySickPlayer/EDM Drums/Hardstyle Kicks/Hardstyle Kick 1 (A).xml",          "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Snares/Hardstyle Snare 1.xml",            "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Closed Hat 05.xml",                "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Open Hat 05.xml",                  "BaySickPlayer"),
        # Condensed
        ("BaySickPlayer/EDM Drums/Hardstyle Kicks/Hardstyle Kick 2 (D).xml",          "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Kicks/Hardstyle Kick 3 (F#).xml",         "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Kicks/Hardstyle Kick 5 (G).xml",          "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Kicks/Hardstyle Kick 7 (B).xml",          "BaySickPlayer"),
        # Full
        ("BaySickPlayer/EDM Drums/Hardstyle Snares/Hardstyle Snare 2.xml",            "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Snares/Hardstyle Snare 5.xml",            "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Snares/Hardstyle Snare 8.xml",            "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Percussion/Hardstyle Perc 1.xml",         "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Percussion/Hardstyle Perc 2.xml",         "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Percussion/Hardstyle Perc 3.xml",         "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/Hardstyle Percussion/Hardstyle Perc 5.xml",         "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Crash 02.xml",                  "BaySickPlayer"),
    ],
    "House (Real)": [
        ("BaySickPlayer/EDM Drums/House Kicks/House Kick 01.xml",                     "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Claps & Snares/House Clap-Snare 01.xml",      "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Closed Hat 06.xml",                "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Open Hat 06 (Part 1).xml",         "BaySickPlayer"),
        # Condensed
        ("BaySickPlayer/EDM Drums/House Kicks/House Kick 02.xml",                     "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Kicks/House Kick 03.xml",                     "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Claps & Snares/House Clap-Snare 02.xml",      "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Claps & Snares/House Clap-Snare 03.xml",      "BaySickPlayer"),
        # Full
        ("BaySickPlayer/EDM Drums/House Kicks/House Kick 04.xml",                     "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Kicks/House Kick 05.xml",                     "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Percussion/House Perc 01.xml",                "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Percussion/House Perc 02.xml",                "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Percussion/House Perc 03.xml",                "BaySickPlayer"),
        ("BaySickPlayer/EDM Drums/House Percussion/House Perc 04.xml",                "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Cymbal 02.xml",                 "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Cymbals/Hip Hop Crash 01.xml",                  "BaySickPlayer"),
    ],
    "Vox-Driven Hip Hop (Real)": [
        ("BaySickPlayer/Hip Hop Drums/Kicks/Hip Hop Kick 02.xml",         "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Snares/Hip Hop Snare 05.xml",       "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Closed Hat 07.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Hats/Hip Hop Open Hat 07 (Part 2).xml", "BaySickPlayer"),
        # Condensed
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 01.xml",            "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 02 (D).xml",        "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 03.xml",            "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 04.xml",            "BaySickPlayer"),
        # Full
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 05 (E).xml",        "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 06 (F#).xml",       "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 07 (D).xml",        "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Vox/Hip Hop Vox 08 (F#).xml",       "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Beatbox/Hip Hop Beatbox 02.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Beatbox/Hip Hop Beatbox 03.xml",    "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/Toms/Hip Hop Tom 02.xml",           "BaySickPlayer"),
        ("BaySickPlayer/Hip Hop Drums/FX/Hip Hop FX 04.xml",              "BaySickPlayer"),
    ],
}

KIT_SIZES = [("Basic", 4), ("Condensed", 8), ("Full", 16)]

# 2026-04-26: drum-role sort applied before kit slicing.  Order: kick / snare /
# hi-hat / tom / cymbal / other.  Stable sort preserves recipe author's
# sub-ordering within a bucket.  Most kits are already designed in this order
# so the sort is a refinement; for the few that drift (e.g. 606 Full borrowing
# from neighboring groups) it normalizes them.
def _drum_role_key(preset_path: str) -> int:
    name = preset_path.rsplit("/", 1)[-1].rsplit(".", 1)[0].lower()
    if "kick" in name:
        return 0
    if "snare" in name or "clap" in name or "rimshot" in name:
        return 1
    if "hat" in name or "chick" in name:
        return 2
    # Bucket 3: drum-shell percussion (toms + ethnic hand drums).
    if ("tom" in name or "conga" in name or "bongo" in name or "tabla" in name
        or "cajon" in name or "djembe" in name or "frame drum" in name
        or "pow wow" in name or "log drum" in name):
        return 3
    if "cymbal" in name or "crash" in name or "ride" in name or "gong" in name:
        return 4
    return 5

def _xml_attr_escape(s: str) -> str:
    """Escape XML attribute values.  & must always be escaped; quotes that
    delimit the attribute must be too.  We use double-quoted attributes so
    only " needs escaping, not '."""
    return (s.replace("&", "&amp;")
             .replace("<", "&lt;")
             .replace(">", "&gt;")
             .replace('"', "&quot;"))

def write_kit_xml(target_dir: Path, name: str, drums_with_engines: list) -> Path:
    """Writes a <BaySickKit> XML referencing factory presets by relative path.
    All attribute values are XML-escaped — folders like "Cinematic, Industrial &
    FX" used to break JUCE's XmlDocument::parse with raw '&'."""
    target_dir.mkdir(parents=True, exist_ok=True)
    name_e = _xml_attr_escape(name)
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             f'<BaySickKit name="{name_e}" version="1">']
    for slot, (preset_path, engine) in enumerate(drums_with_engines):
        path_e = _xml_attr_escape(preset_path)
        eng_e  = _xml_attr_escape(engine)
        lines.append(
            f'  <Drum slot="{slot}" engine="{eng_e}" '
            f'presetPath="{path_e}" locked="1"/>'
        )
    lines.append('</BaySickKit>')
    out = target_dir / f"{name}.xml"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out

# ═══════════════════════════════════════════════════════════════════════════
# 2026-04-26: Factory Templates
# ═══════════════════════════════════════════════════════════════════════════
# Each template = a kit reference + 8 layer presets + 4 bass presets, all
# locked. Tearing them open: open the kit, populate Layers/Bass/Drums tabs.
#
# Template XML format:
#   <BaySickTemplate name="..." version="1">
#     <Kit path="TR-808/TR-808 Full.xml"/>            <!-- relative to Kits/Factory/ -->
#     <Layer slot="0" engine="BaySickSynth" presetPath="BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml" locked="1"/>
#     ...
#     <Bass slot="0" engine="BaySickBass" presetPath="BaySickBass/Sub Bass & 808s/Punchy 808 Bass.xml" locked="1"/>
#     ...
#   </BaySickTemplate>
#
# StandaloneEditor::loadTemplate parses this, calls loadKit on the kit ref,
# then creates Layer/Bass tabs and applies their presets via the existing
# loadPreset machinery.

# Each entry: (name, kit_path, [(engine, preset_path), ...] layers, [(engine, preset_path), ...] basses)
# Engine is the runtime engine type ("BaySickSynth" / "BaySickBass" / "Harmless" / "BaySickPlayer").
# preset_path is relative to Documents/BaySickDAW/Presets/ (includes engine root folder).
TEMPLATES = [
    # ── Original 10 kit styles ──────────────────────────────────────────────
    ("TR-808", "TR-808/TR-808 Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/DX EP.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Soulful Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Leads & Solos/R&B Glide Lead.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Sample Chop Pluck.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Trap Bell.xml"),
        ("BaySickSynth", "BaySickSynth/Brass & Strings/Solina Strings.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dark Brooding Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Punchy 808 Bass.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Sub Bass.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Hip-Hop Sub.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Boom-Bap 808.xml"),
    ]),
    ("TR-909", "TR-909/TR-909 Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/House Organ.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Juno Warm Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Leads & Solos/Acid Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Plucks & Mallets/Classic Trance Pluck.xml"),
        ("Harmless",     "Harmless/Synth-Pop/Glassy Stab.xml"),
        ("BaySickSynth", "BaySickSynth/Arp & Sequencer Tones/Trance Gate Arp.xml"),
        ("Harmless",     "Harmless/Brass & Strings/Strings Ensemble.xml"),
        ("Harmless",     "Harmless/Pads & Atmospheres/Sweep Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Acid & 303/Classic 303 Saw.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Deep House Sub.xml"),
        ("BaySickBass", "BaySickBass/Acid & 303/Acid Glide.xml"),
        ("Harmless",    "Harmless/Psytrance/Acid Roll Bass.xml"),
    ]),
    ("TR-606", "TR-606/TR-606 Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Toy Piano.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Vintage String Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/Outrun Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Plucks & Mallets/Analog Pluck.xml"),
        ("BaySickSynth", "BaySickSynth/Brass & Strings/80s Synth Brass.xml"),
        ("Harmless",     "Harmless/Synthwave & Vintage/Retrowave Bell.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/Neon Lead.xml"),
        ("Harmless",     "Harmless/Pads & Atmospheres/Lo-Fi Tape Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Vintage Analog/70s Moog Bass.xml"),
        ("BaySickBass", "BaySickBass/Vintage Analog/Analog Pulse.xml"),
        ("Harmless",    "Harmless/Krautrock/Pulse Sub Bass.xml"),
        ("BaySickBass", "BaySickBass/Vintage Analog/Classic Electro Bass.xml"),
    ]),
    ("80s Electronic", "80s Electronic/80s Electronic Full.xml", [
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/VHS Keys.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/Synthwave Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/Outrun Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Brass & Strings/80s Synth Brass.xml"),
        ("BaySickSynth", "BaySickSynth/Brass & Strings/Vangelis Brass.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/80s Pop Pluck.xml"),
        ("Harmless",     "Harmless/Synthwave & Vintage/Outrun Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/Retrowave Bell.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Synthwave & Retrowave/Synthwave Bass.xml"),
        ("BaySickBass", "BaySickBass/Synthwave & Retrowave/80s Drive Saw.xml"),
        ("BaySickBass", "BaySickBass/Synthwave & Retrowave/Retrowave Sub.xml"),
        ("Harmless",    "Harmless/Synth-Pop/80s Synth Bass.xml"),
    ]),
    ("FM Digital", "FM Digital/FM Digital Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/DX EP.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/FM Tines.xml"),
        ("Harmless",     "Harmless/Plucks & Mallets/FM Bell Pluck.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Glass Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Leads & Solos/DX Lead FM.xml"),
        ("BaySickSynth", "BaySickSynth/Plucks & Mallets/FM Bell Pluck.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Crystal Keys.xml"),
        ("Harmless",     "Harmless/Pads & Atmospheres/Glass Shimmer Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Pluck, Donk & FM/DX Bass FM.xml"),
        ("BaySickBass", "BaySickBass/Pluck, Donk & FM/FM Growl Bass.xml"),
        ("BaySickBass", "BaySickBass/Pluck, Donk & FM/Bell Bass.xml"),
        ("BaySickBass", "BaySickBass/Pluck, Donk & FM/FM Metallic Pluck.xml"),
    ]),
    ("Acoustic Rock", "Acoustic Rock/Acoustic Rock Full.xml", [
        ("BaySickPlayer", "BaySickPlayer/Keys/Upright Piano.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Hammond Drawbar.xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Violin Ens (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Cello Ens (Sustained).xml"),
        ("BaySickSynth",  "BaySickSynth/Leads & Solos/Flute Lead.xml"),
        ("BaySickSynth",  "BaySickSynth/Plucks & Mallets/Marimba Pluck.xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trumpet (Sustained).xml"),
        ("BaySickSynth",  "BaySickSynth/Pads & Atmospheres/Warm Analog Pad.xml"),
    ], [
        ("BaySickPlayer", "BaySickPlayer/Strings/Contrabass (Pizzicato).xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Contrabass (Sustained).xml"),
        ("BaySickBass",   "BaySickBass/Slap & Electric/Picked Bass.xml"),
        ("BaySickBass",   "BaySickBass/Slap & Electric/Upright Jazz Sub.xml"),
    ]),
    ("Trap", "Trap/Trap Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Trap Bell.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Sample Chop Pluck.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Hyperpop Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Festival Lead.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dark Brooding Pad.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Distorted Lo-Fi Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Future Bass Chord.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Punchy 808 Bass.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Sub Thump.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Hip-Hop Sub.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Boom-Bap 808.xml"),
    ]),
    ("Lo-Fi Hip-Hop", "Lo-Fi Hip-Hop/Lo-Fi Hip-Hop Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Lo-Fi Keys.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml"),
        ("Harmless",     "Harmless/Pads & Atmospheres/Lo-Fi Tape Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Lo-Fi Tape Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Plucks & Mallets/Marimba Pluck.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dusty EP.xml"),
        ("BaySickSynth", "BaySickSynth/Brass & Strings/Lo-Fi Brass.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Soulful Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Vintage Analog/Dusty Bass.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Sub Bass.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Hip-Hop Sub.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Upright Jazz Sub.xml"),
    ]),
    ("EDM Big Room", "EDM Big Room/EDM Big Room Full.xml", [
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Supersaw Chords.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Festival Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Leads & Solos/EDM Saw Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Slap House Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Future Bass Chord.xml"),
        ("Harmless",     "Harmless/Modern EDM & Hyperpop/Festival Lead.xml"),
        ("Harmless",     "Harmless/Cinematic & Drones/Riser FX.xml"),
        ("BaySickSynth", "BaySickSynth/Cinematic & Drones/Sub Drop FX.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Deep House Sub.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Saturation Sub.xml"),
        ("BaySickBass", "BaySickBass/Reese & Neuro/Standard Reese.xml"),
        ("Harmless",    "Harmless/Bass Music/Skrillex Reese.xml"),
    ]),
    ("Cinematic", "Cinematic/Cinematic Full.xml", [
        ("BaySickPlayer", "BaySickPlayer/Strings/Violin Ens (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Cello Ens (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Viola Ens (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/French Horn (Sustained).xml"),
        ("Harmless",      "Harmless/Cinematic & Drones/Tension Riser.xml"),
        ("Harmless",      "Harmless/Cinematic & Drones/Sci-Fi Wash.xml"),
        ("BaySickSynth",  "BaySickSynth/Cinematic & Drones/Tension Drone.xml"),
        ("Harmless",      "Harmless/Pads & Atmospheres/Glass Shimmer Pad.xml"),
    ], [
        ("BaySickPlayer", "BaySickPlayer/Strings/Contrabass (Sustained).xml"),
        ("BaySickBass",   "BaySickBass/Sub Bass & 808s/Cinema Rumble Sub.xml"),
        ("Harmless",      "Harmless/Psybient/Deep Drone Bass.xml"),
        ("Harmless",      "Harmless/Psybient/Sub Hum.xml"),
    ]),

    # ── Round 2: 8 genre kits ───────────────────────────────────────────────
    ("French Disco", "French Disco/French Disco Full.xml", [
        ("Harmless",     "Harmless/French Disco/Vocoder Pad.xml"),
        ("Harmless",     "Harmless/French Disco/Disco String Stack.xml"),
        ("Harmless",     "Harmless/French Disco/Talkbox Lead.xml"),
        ("Harmless",     "Harmless/French Disco/Phaser Lead.xml"),
        ("Harmless",     "Harmless/French Disco/Warm Rhodes.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/Juno Poly.xml"),
        ("Harmless",     "Harmless/French Disco/Funky Pluck.xml"),
        ("Harmless",     "Harmless/French Disco/Analog Brass.xml"),
    ], [
        ("Harmless",    "Harmless/French Disco/Funky Synth Bass.xml"),
        ("Harmless",    "Harmless/French Disco/Disco Sub.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Funk Pop Bass.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Synth Slap.xml"),
    ]),
    ("Psytrance", "Psytrance/Psytrance Full.xml", [
        ("Harmless", "Harmless/Psytrance/Supersaw Lead.xml"),
        ("Harmless", "Harmless/Psytrance/Trance Pluck.xml"),
        ("Harmless", "Harmless/Psytrance/Acid Hoover.xml"),
        ("Harmless", "Harmless/Psytrance/Gated Arp Pad.xml"),
        ("Harmless", "Harmless/Psytrance/FM Bell Stab.xml"),
        ("Harmless", "Harmless/Psytrance/Glitch Lead.xml"),
        ("Harmless", "Harmless/Psytrance/Hi-Pass Pad.xml"),
        ("Harmless", "Harmless/Psytrance/Tribal Pluck.xml"),
    ], [
        ("Harmless", "Harmless/Psytrance/Acid Roll Bass.xml"),
        ("Harmless", "Harmless/Psytrance/Reese Bass.xml"),
        ("Harmless", "Harmless/Psytrance/Gated Bass Roll.xml"),
        ("Harmless", "Harmless/Psytrance/Psytrance Sub Pulse.xml"),
    ]),
    ("Boom-Bap Hip-Hop", "Boom-Bap Hip-Hop/Boom-Bap Hip-Hop Full.xml", [
        ("Harmless",     "Harmless/Modern Hip-Hop/Boom-Bap Rhodes.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dusty EP.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Soulful Pad.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Sample Chop Pluck.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Vocal-Style Lead.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Trap Bell.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dark Brooding Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Lo-Fi Keys.xml"),
    ], [
        ("Harmless",    "Harmless/Modern Hip-Hop/Boom-Bap 808.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Hip-Hop Sub.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Dirty Square Bass.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Punchy 808 Bass.xml"),
    ]),
    ("Synth-Pop", "Synth-Pop/Synth-Pop Full.xml", [
        ("Harmless",     "Harmless/Synth-Pop/Dark Minor Pad.xml"),
        ("Harmless",     "Harmless/Synth-Pop/Choir Pad.xml"),
        ("Harmless",     "Harmless/Synth-Pop/FM Plucky EP.xml"),
        ("Harmless",     "Harmless/Synth-Pop/Industrial Lead.xml"),
        ("Harmless",     "Harmless/Synth-Pop/Glassy Stab.xml"),
        ("BaySickSynth", "BaySickSynth/Brass & Strings/80s Synth Brass.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/VHS Keys.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Sweep Pad.xml"),
    ], [
        ("Harmless",    "Harmless/Synth-Pop/80s Synth Bass.xml"),
        ("Harmless",    "Harmless/Synth-Pop/Filtered Saw Bass.xml"),
        ("BaySickBass", "BaySickBass/Synthwave & Retrowave/Synthwave Bass.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Filtered Pulse Sub.xml"),
    ]),
    ("Bass Music", "Bass Music/Bass Music Full.xml", [
        ("Harmless",     "Harmless/Bass Music/Future Bass Chord.xml"),
        ("Harmless",     "Harmless/Bass Music/Hyper Lead.xml"),
        ("Harmless",     "Harmless/Bass Music/Aggressive Saw.xml"),
        ("Harmless",     "Harmless/Bass Music/Resonant Lead.xml"),
        ("Harmless",     "Harmless/Bass Music/Vocal Chop Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Hyperpop Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Glitch Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Lasersaw.xml"),
    ], [
        ("Harmless",    "Harmless/Bass Music/Skrillex Reese.xml"),
        ("Harmless",    "Harmless/Bass Music/Wobble Bass.xml"),
        ("Harmless",    "Harmless/Bass Music/FM Growl.xml"),
        ("BaySickBass", "BaySickBass/Reese & Neuro/Dark Neuro.xml"),
    ]),
    ("Krautrock", "Krautrock/Krautrock Full.xml", [
        ("Harmless",     "Harmless/Krautrock/ARP Lead.xml"),
        ("Harmless",     "Harmless/Krautrock/Vocoder Robot.xml"),
        ("Harmless",     "Harmless/Krautrock/Ribbon Glide.xml"),
        ("Harmless",     "Harmless/Krautrock/Motorik Pad.xml"),
        ("Harmless",     "Harmless/Krautrock/Vintage String.xml"),
        ("BaySickSynth", "BaySickSynth/Synthwave & Vintage/Outrun Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Vintage String Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Arp & Sequencer Tones/Retro Arp.xml"),
    ], [
        ("Harmless",    "Harmless/Krautrock/Pulse Sub Bass.xml"),
        ("Harmless",    "Harmless/Krautrock/Sequenced Bass.xml"),
        ("BaySickBass", "BaySickBass/Vintage Analog/70s Moog Bass.xml"),
        ("BaySickBass", "BaySickBass/Vintage Analog/Analog Pulse.xml"),
    ]),
    ("Prog House", "Prog House/Prog House Full.xml", [
        ("Harmless",     "Harmless/Prog House/Sidechain Saw Lead.xml"),
        ("Harmless",     "Harmless/Prog House/Plucky Stab.xml"),
        ("Harmless",     "Harmless/Prog House/Wet Pad.xml"),
        ("Harmless",     "Harmless/Prog House/Bell Lead.xml"),
        ("Harmless",     "Harmless/Prog House/Lasersaw Stab.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Supersaw Chords.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Shimmer Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/EDM Pluck.xml"),
    ], [
        ("Harmless",    "Harmless/Prog House/Big Saw Bass.xml"),
        ("Harmless",    "Harmless/Prog House/Plucked Sub.xml"),
        ("BaySickBass", "BaySickBass/Pluck, Donk & FM/Tech House Stub.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Deep House Sub.xml"),
    ]),
    ("Vintage Soul", "Vintage Soul/Vintage Soul Full.xml", [
        ("Harmless",      "Harmless/Neo-Soul/Vintage Wurli.xml"),
        ("Harmless",      "Harmless/Neo-Soul/Hammond B3 Smooth.xml"),
        ("Harmless",      "Harmless/Neo-Soul/Soulful Rhodes.xml"),
        ("Harmless",      "Harmless/Neo-Soul/Mellotron Pad.xml"),
        ("Harmless",      "Harmless/Neo-Soul/Theremin Lead.xml"),
        ("Harmless",      "Harmless/Neo-Soul/Harpsichord Stab.xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trumpet (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trombone (Sustained).xml"),
    ], [
        ("Harmless",    "Harmless/Neo-Soul/Walking Synth Bass.xml"),
        ("Harmless",    "Harmless/Neo-Soul/Vintage Round Bass.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Picked Bass.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Upright Jazz Sub.xml"),
    ]),

    # ── Round 3: 5 genre kits ───────────────────────────────────────────────
    ("Reggae", "Reggae/Reggae Full.xml", [
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Hammond Organ.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/B3 Organ.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Wurli Tremolo.xml"),
        ("BaySickSynth",  "BaySickSynth/Plucks & Mallets/Tropical Pluck.xml"),
        ("BaySickSynth",  "BaySickSynth/Brass & Strings/Brass Scoop.xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trumpet (Sustained).xml"),
        ("BaySickSynth",  "BaySickSynth/Pads & Atmospheres/Warm Analog Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Slap & Electric/Reggae Dub Bass.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Sub Bass.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Picked Bass.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Muted Electric.xml"),
    ]),
    ("Latin Salsa", "Latin Salsa/Latin Salsa Full.xml", [
        ("BaySickPlayer", "BaySickPlayer/Keys/Upright Piano.xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trumpet (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trumpet (Staccato).xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trombone (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trombone (Staccato).xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/B3 Organ.xml"),
        ("BaySickPlayer", "BaySickPlayer/Woodwinds/Flute (Sustained Vibrato).xml"),
        ("BaySickSynth",  "BaySickSynth/Plucks & Mallets/Marimba Pluck.xml"),
    ], [
        ("BaySickPlayer", "BaySickPlayer/Strings/Contrabass (Pizzicato).xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Contrabass (Sustained).xml"),
        ("BaySickBass",   "BaySickBass/Slap & Electric/Picked Bass.xml"),
        ("BaySickBass",   "BaySickBass/Slap & Electric/Upright Jazz Sub.xml"),
    ]),
    ("Funk", "Funk/Funk Full.xml", [
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Clavinet.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Hammond Drawbar.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml"),
        ("BaySickSynth",  "BaySickSynth/Brass & Strings/Brass Scoop.xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trumpet (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trombone (Sustained).xml"),
        ("BaySickSynth",  "BaySickSynth/Synthwave & Vintage/80s Pop Pluck.xml"),
        ("BaySickSynth",  "BaySickSynth/Pads & Atmospheres/Warm Analog Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Slap & Electric/Synth Slap.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Funk Pop Bass.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Picked Bass.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Chorus Slap.xml"),
    ]),
    ("Industrial", "Industrial/Industrial Full.xml", [
        ("Harmless",     "Harmless/Synth-Pop/Industrial Lead.xml"),
        ("Harmless",     "Harmless/Cinematic & Drones/Sci-Fi Wash.xml"),
        ("Harmless",     "Harmless/Cinematic & Drones/Wind Howl.xml"),
        ("Harmless",     "Harmless/Cinematic & Drones/Granular Texture.xml"),
        ("Harmless",     "Harmless/Bass Music/Aggressive Saw.xml"),
        ("BaySickSynth", "BaySickSynth/Cinematic & Drones/Dystopian Siren.xml"),
        ("BaySickSynth", "BaySickSynth/Cinematic & Drones/Tension Drone.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Distorted Lo-Fi Lead.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Midtempo & Cyberpunk/Industrial Grind.xml"),
        ("BaySickBass", "BaySickBass/Midtempo & Cyberpunk/Doom Saw.xml"),
        ("BaySickBass", "BaySickBass/Midtempo & Cyberpunk/Dystopian Sub.xml"),
        ("Harmless",    "Harmless/Bass Music/FM Growl.xml"),
    ]),
    ("Jazz Fusion", "Jazz Fusion/Jazz Fusion Full.xml", [
        ("BaySickPlayer", "BaySickPlayer/Keys/Upright Piano.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Rhodes EP.xml"),
        ("BaySickSynth",  "BaySickSynth/Keys & Electric Pianos/Hammond Drawbar.xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Violin Ens (Sustained).xml"),
        ("BaySickPlayer", "BaySickPlayer/Brass/Trumpet (Sustained).xml"),
        ("BaySickSynth",  "BaySickSynth/Plucks & Mallets/Marimba Pluck.xml"),
        ("BaySickSynth",  "BaySickSynth/Plucks & Mallets/Pizzicato Synth.xml"),
        ("BaySickSynth",  "BaySickSynth/Pads & Atmospheres/Warm Analog Pad.xml"),
    ], [
        ("BaySickPlayer", "BaySickPlayer/Strings/Contrabass (Pizzicato).xml"),
        ("BaySickPlayer", "BaySickPlayer/Strings/Contrabass (Sustained).xml"),
        ("BaySickBass",   "BaySickBass/Slap & Electric/Upright Jazz Sub.xml"),
        ("BaySickBass",   "BaySickBass/Slap & Electric/Fretless Glide.xml"),
    ]),

    # ── Round 4: 6 sample-based kits ────────────────────────────────────────
    ("Hip Hop (Real)", "Hip Hop (Real)/Hip Hop (Real) Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/DX EP.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Soulful Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Leads & Solos/R&B Glide Lead.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Sample Chop Pluck.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Trap Bell.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dark Brooding Pad.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Vocal-Style Lead.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Punchy 808 Bass.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Hip-Hop Sub.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Boom-Bap 808.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Sub Bass.xml"),
    ]),
    ("Lo-Fi (Real)", "Lo-Fi (Real)/Lo-Fi (Real) Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Lo-Fi Keys.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dusty EP.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Soulful Pad.xml"),
        ("Harmless",     "Harmless/Pads & Atmospheres/Lo-Fi Tape Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Plucks & Mallets/Marimba Pluck.xml"),
        ("BaySickSynth", "BaySickSynth/Brass & Strings/Lo-Fi Brass.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Lo-Fi Tape Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Phase EP.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Vintage Analog/Dusty Bass.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Hip-Hop Sub.xml"),
        ("BaySickBass", "BaySickBass/Slap & Electric/Upright Jazz Sub.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Sub Bass.xml"),
    ]),
    ("Dubstep (Real)", "Dubstep (Real)/Dubstep (Real) Full.xml", [
        ("Harmless",     "Harmless/Bass Music/Aggressive Saw.xml"),
        ("Harmless",     "Harmless/Bass Music/Resonant Lead.xml"),
        ("Harmless",     "Harmless/Bass Music/Vocal Chop Pad.xml"),
        ("Harmless",     "Harmless/Bass Music/Hyper Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Glitch Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Lasersaw.xml"),
        ("BaySickSynth", "BaySickSynth/Cinematic & Drones/Sub Drop FX.xml"),
        ("Harmless",     "Harmless/Cinematic & Drones/Riser FX.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Dubstep & Wobble/Classic Wobble.xml"),
        ("BaySickBass", "BaySickBass/Dubstep & Wobble/Heavy Formant Wub.xml"),
        ("Harmless",    "Harmless/Bass Music/Skrillex Reese.xml"),
        ("Harmless",    "Harmless/Bass Music/FM Growl.xml"),
    ]),
    ("Hardstyle (Real)", "Hardstyle (Real)/Hardstyle (Real) Full.xml", [
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Festival Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Leads & Solos/EDM Saw Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Lasersaw.xml"),
        ("Harmless",     "Harmless/Bass Music/Hyper Lead.xml"),
        ("Harmless",     "Harmless/Bass Music/Aggressive Saw.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Crystal Chords.xml"),
        ("Harmless",     "Harmless/Cinematic & Drones/Riser FX.xml"),
        ("BaySickSynth", "BaySickSynth/Cinematic & Drones/Sub Drop FX.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Reese & Neuro/Standard Reese.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Saturation Sub.xml"),
        ("BaySickBass", "BaySickBass/Reese & Neuro/Growl Bass.xml"),
        ("Harmless",    "Harmless/Bass Music/Skrillex Reese.xml"),
    ]),
    ("House (Real)", "House (Real)/House (Real) Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/House Organ.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Slap House Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/Supersaw Chords.xml"),
        ("BaySickSynth", "BaySickSynth/Pads & Atmospheres/Shimmer Pad.xml"),
        ("BaySickSynth", "BaySickSynth/Plucks & Mallets/Classic Trance Pluck.xml"),
        ("Harmless",     "Harmless/Prog House/Plucky Stab.xml"),
        ("Harmless",     "Harmless/Prog House/Sidechain Saw Lead.xml"),
        ("BaySickSynth", "BaySickSynth/Modern EDM & Hyperpop/EDM Pluck.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Deep House Sub.xml"),
        ("BaySickBass", "BaySickBass/Pluck, Donk & FM/Deep House Pluck.xml"),
        ("BaySickBass", "BaySickBass/Acid & 303/Classic 303 Saw.xml"),
        ("BaySickBass", "BaySickBass/Pluck, Donk & FM/Tech House Stub.xml"),
    ]),
    ("Vox-Driven Hip Hop (Real)", "Vox-Driven Hip Hop (Real)/Vox-Driven Hip Hop (Real) Full.xml", [
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/Vintage Rhodes.xml"),
        ("BaySickSynth", "BaySickSynth/Keys & Electric Pianos/DX EP.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Soulful Pad.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Vocal-Style Lead.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Sample Chop Pluck.xml"),
        ("Harmless",     "Harmless/Pads & Atmospheres/Vocal Choir Pad.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Trap Bell.xml"),
        ("Harmless",     "Harmless/Modern Hip-Hop/Dark Brooding Pad.xml"),
    ], [
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Punchy 808 Bass.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Hip-Hop Sub.xml"),
        ("Harmless",    "Harmless/Modern Hip-Hop/Boom-Bap 808.xml"),
        ("BaySickBass", "BaySickBass/Sub Bass & 808s/Sub Bass.xml"),
    ]),
]

def write_template_xml(target_dir: Path, name: str, kit_path: str,
                        layers: list, basses: list) -> Path:
    """Writes a <BaySickTemplate> XML.  Kit path is relative to Kits/Factory/;
    layer/bass presetPath relative to Presets/.  All entries written with
    locked=1 so the template lands fully populated and protected on load."""
    target_dir.mkdir(parents=True, exist_ok=True)
    name_e = _xml_attr_escape(name)
    kit_e  = _xml_attr_escape(kit_path)
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             f'<BaySickTemplate name="{name_e}" version="1">',
             f'  <Kit path="{kit_e}"/>']
    for slot, (engine, preset_path) in enumerate(layers):
        lines.append(
            f'  <Layer slot="{slot}" engine="{_xml_attr_escape(engine)}" '
            f'presetPath="{_xml_attr_escape(preset_path)}" locked="1"/>')
    for slot, (engine, preset_path) in enumerate(basses):
        lines.append(
            f'  <Bass slot="{slot}" engine="{_xml_attr_escape(engine)}" '
            f'presetPath="{_xml_attr_escape(preset_path)}" locked="1"/>')
    lines.append('</BaySickTemplate>')
    out = target_dir / f"{name}.xml"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out

def generate_factory_templates():
    """Build 29 factory templates — one per Full kit style."""
    written = 0
    missing = []
    for name, kit_path, layers, basses in TEMPLATES:
        # Verify kit ref.
        full_kit = KITS_DIR / kit_path
        if not full_kit.is_file():
            missing.append(f"{name}: kit not found at {full_kit}")
        # Verify each layer/bass preset.
        for engine, preset_path in layers + basses:
            full_preset = PRESETS_ROOT / preset_path
            if not full_preset.is_file():
                missing.append(f"{name}: preset not found at {full_preset}")
        out = write_template_xml(TEMPLATES_DIR, name, kit_path, layers, basses)
        print(f"  Template: {out.name}  ({len(layers)} layers, {len(basses)} basses)")
        written += 1
    if missing:
        print("\n  !! Missing references (templates will skip those slots at load):")
        for m in missing[:30]:
            print(m)
        if len(missing) > 30:
            print(f"    ... and {len(missing) - 30} more")
    return written, len(missing)

def generate_factory_kits():
    """Build factory kits — N styles × 3 sizes each.

    presetPath is written relative to PRESETS_ROOT (includes source subfolder).
    Factory drum presets live under BaySickDrums/<group>/, but the engine
    field still says BaySickSynth (that's the runtime engine type).

    2026-04-26: drum-role sort applied per style so each kit's drums are
    ordered kick/snare/hh/tom/cymbal/other regardless of recipe author input."""
    written = 0
    missing_refs = []
    # 2026-04-26 (round 4): paths starting with an engine prefix are used as-is
    # (e.g. "BaySickPlayer/Hip Hop Drums/Kicks/Hip Hop Kick 01.xml"); legacy
    # paths without a prefix get the historic "BaySickDrums/" prepended so all
    # the existing kits keep working without edits.
    KNOWN_ENGINE_PREFIXES = ("BaySickDrums/", "BaySickSynth/", "BaySickBass/",
                              "Harmless/", "BaySickPlayer/")
    def _full_path(p: str) -> str:
        if p.startswith(KNOWN_ENGINE_PREFIXES):
            return p
        return f"BaySickDrums/{p}"
    for style, drums in KIT_STYLES.items():
        # Sort once per style — Python's sorted() is stable so within-bucket
        # order from the recipe is preserved.
        sorted_drums = sorted(drums, key=lambda d: _drum_role_key(d[0]))
        for size_name, size_count in KIT_SIZES:
            # Resolve each preset path to its full Presets-relative form.
            kit_drums = [(_full_path(p), e) for (p, e) in sorted_drums[:size_count]]
            # Check each referenced preset exists.
            for preset_path, engine in kit_drums:
                full_path = (PRESETS_ROOT / preset_path)
                if not full_path.is_file():
                    missing_refs.append(f"{style}/{size_name}: missing {full_path}")
            kit_name = f"{style} {size_name}"
            target_dir = KITS_DIR / style
            out = write_kit_xml(target_dir, kit_name, kit_drums)
            print(f"  Kit: {target_dir.name}/{out.name}  ({len(kit_drums)} drums)")
            written += 1
    if missing_refs:
        print("\n  !! Missing preset references (kits will skip those slots at load):")
        for m in missing_refs[:20]:
            print(f"    {m}")
        if len(missing_refs) > 20:
            print(f"    ... and {len(missing_refs) - 20} more")
    return written, len(missing_refs)

# ---- main -------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--starter-only", action="store_true",
                   help="Write only the 8 starter drum recipes.")
    args = p.parse_args()

    print(f"Presets root: {PRESETS_ROOT}")

    # 2026-04-24: cleanup pass — delete old "TR-XXX *.xml" files left over
    # from before the prefix was stripped.  Subfolder names (TR-808 / TR-909 /
    # TR-606) stay; only individual preset files with the old prefix are
    # removed.  My Presets/ subfolder is left untouched.
    removed = 0
    for engine_dir in (DRUMS_DIR, SYNTH_DIR, BASS_DIR):
        if not engine_dir.is_dir(): continue
        for f in engine_dir.rglob("TR-*.xml"):
            if "My Presets" in f.parts: continue
            f.unlink()
            removed += 1
    if removed:
        print(f"Removed {removed} legacy TR-prefixed preset(s).")

    starter_set = {"808 Kick", "909 Kick", "808 Snare", "808 Closed Hat",
                   "808 Open Hat", "808 Cowbell", "808 Handclap", "Stick-Hit Drum"}

    drum_count = synth_count = bass_count = harm_count = bsp_count = 0

    # D-CC1: every drum-bank factory preset enables cut-self so retriggering a
    # drum slot kills the prior voice (no DBFS wobble on repeated hits).
    DRUM_BANK_OVERRIDES = {"cutSelf": 1}

    # ── Batch 5: merge new recipes with dedupe ──────────────────────────
    drum_recipes,  drum_report  = merge_with_dedupe(DRUM_RECIPES,  NEW_DRUM_RECIPES,  DRUM_CATEGORIES)
    synth_recipes, synth_report = merge_with_dedupe(SYNTH_RECIPES, NEW_SYNTH_RECIPES, SYNTH_CATEGORIES)
    bass_recipes,  bass_report  = merge_with_dedupe(BASS_RECIPES,  NEW_BASS_RECIPES,  BASS_CATEGORIES)

    # Stale-category cleanup: when a recipe's category mapping moves, the old
    # folder keeps its copy forever (the 2026-04-25 Hand Percussion move
    # shipped 4 duplicate drum presets that way).  Delete any same-named XML
    # sitting outside the recipe's CURRENT category folder.  My Presets/ is
    # never touched.
    stale = 0
    for eng_dir, recipes, cats in ((DRUMS_DIR, drum_recipes, DRUM_CATEGORIES),
                                   (SYNTH_DIR, synth_recipes, SYNTH_CATEGORIES),
                                   (BASS_DIR,  bass_recipes,  BASS_CATEGORIES)):
        if not eng_dir.is_dir(): continue
        for name, _ in recipes:
            expected = eng_dir / cats[name] if name in cats else eng_dir
            for f in eng_dir.rglob(name + ".xml"):
                if "My Presets" in f.parts: continue
                if f.parent != expected:
                    f.unlink()
                    stale += 1
    if stale:
        print(f"Removed {stale} stale-category preset copy(ies).")

    print(f"\nDrum presets -> {DRUMS_DIR}")
    for name, overrides in drum_recipes:
        if args.starter_only and name not in starter_set:
            continue
        target = categorized_dir(DRUMS_DIR, name, DRUM_CATEGORIES)
        out = write_preset_xml(target, ENGINE_BSS, name, overrides, DRUM_BANK_OVERRIDES)
        print(f"  {target.name}/{out.name}")
        drum_count += 1

    if not args.starter_only:
        print(f"\nSynth presets -> {SYNTH_DIR}")
        for name, overrides in synth_recipes:
            target = categorized_dir(SYNTH_DIR, name, SYNTH_CATEGORIES)
            out = write_preset_xml(target, ENGINE_BSS, name, overrides)
            print(f"  {target.name}/{out.name}")
            synth_count += 1

        print(f"\nBass presets -> {BASS_DIR}")
        for name, overrides in bass_recipes:
            target = categorized_dir(BASS_DIR, name, BASS_CATEGORIES)
            out = write_preset_xml(target, ENGINE_BSB, name, overrides)
            print(f"  {target.name}/{out.name}")
            bass_count += 1

        # ── 2026-04-26: Harmless preset generation (78 patches, 9 clusters).
        # Genre clusters from Files For Claude/Preset Links.txt. Layered into
        # Presets/Harmless/<cluster>/ via HARMLESS_CATEGORIES routing.
        print(f"\nHarmless presets -> {HARMLESS_DIR}")
        for name, overrides in HARMLESS_RECIPES:
            target = categorized_dir(HARMLESS_DIR, name, HARMLESS_CATEGORIES)
            out = write_harmless_preset_xml(target, name, overrides)
            print(f"  {target.name}/{out.name}")
            harm_count += 1

        # ── 2026-04-26 (round 3): BaySickPlayer SFZ-wrapping factory presets.
        # 75 entries — one per Core Library SFZ.  Folders mirror the pack
        # structure: Brass / Keys / Percussion / Strings / Woodwinds.
        # SFZ refs verified against LOCALAPPDATA/BaySickDAW/CoreLibrary so we
        # warn (but still write) if the user's library is missing any pack.
        print(f"\nBaySickPlayer presets -> {PLAYER_DIR}")
        bsp_missing = []
        for name, pack_folder, sfz_lib_rel, overrides in BSP_RECIPES:
            if CORE_LIBRARY_DIR is not None:
                full_sfz = CORE_LIBRARY_DIR / sfz_lib_rel
                if not full_sfz.is_file():
                    bsp_missing.append(f"  {pack_folder}/{name}: SFZ not found at {full_sfz}")
            target = PLAYER_DIR / pack_folder
            out = write_bsp_preset_xml(target, name, sfz_lib_rel, overrides, kind="sfz")
            print(f"  {pack_folder}/{out.name}")
            bsp_count += 1

        # ── 2026-04-26 (round 4): BaySickPlayer sample (.wav) factory presets.
        # 142 entries wrapping Hip Hop + EDM Drums Package one-shots.  Each
        # preset is a single-file BaySickPlayer slot played at the played
        # MIDI note (no normalize/keymap — drums are static-pitch hits).
        print(f"\nBaySickPlayer sample presets")
        for name, sub_folder, wav_lib_rel in BSP_SAMPLE_RECIPES:
            if CORE_LIBRARY_DIR is not None:
                full_wav = CORE_LIBRARY_DIR / wav_lib_rel
                if not full_wav.is_file():
                    bsp_missing.append(f"  {sub_folder}/{name}: WAV not found at {full_wav}")
            target = PLAYER_DIR / sub_folder
            out = write_bsp_preset_xml(target, name, wav_lib_rel, {}, kind="file")
            print(f"  {sub_folder}/{out.name}")
            bsp_count += 1
        if bsp_missing:
            print("\n  !! Some SFZ files were not found in your Core Library:")
            for m in bsp_missing[:10]:
                print(m)
            if len(bsp_missing) > 10:
                print(f"    ... and {len(bsp_missing) - 10} more")
            print("  Presets were written anyway; they'll fail to load until packs are installed.")

    # ── Batch 5: clean up legacy folder names that have been renamed.
    # Old folders left behind from previous script runs are removed only
    # when empty (so we never destroy user files).  My Presets/ subfolders
    # are always preserved.
    LEGACY_FOLDERS = {
        DRUMS_DIR: ["TR-808", "TR-909", "TR-606", "Simmons", "Yamaha FM"],
        SYNTH_DIR: ["Bells & Metallic", "Keys & Organs", "Leads", "Pads", "Sound FX"],
        BASS_DIR:  ["Analog Bass", "FM Bass", "Sub & Dub"],
    }
    cleanup_msgs = []
    for engine_dir, legacy_names in LEGACY_FOLDERS.items():
        for legacy in legacy_names:
            legacy_dir = engine_dir / legacy
            if not legacy_dir.is_dir(): continue
            # Move any remaining XMLs (other than under My Presets/) to root
            stray = [f for f in legacy_dir.rglob("*.xml")
                     if "My Presets" not in f.parts]
            for f in stray:
                try: f.unlink()
                except OSError: pass
            # Remove empty subdirectories
            for sub in sorted(legacy_dir.rglob("*"), reverse=True):
                if sub.is_dir() and not any(sub.iterdir()):
                    try: sub.rmdir()
                    except OSError: pass
            if not any(legacy_dir.iterdir()):
                try:
                    legacy_dir.rmdir()
                    cleanup_msgs.append(f"  removed empty legacy folder: {legacy_dir}")
                except OSError: pass
            else:
                cleanup_msgs.append(f"  KEPT (non-empty): {legacy_dir}")
    if cleanup_msgs:
        print("\nLegacy folder cleanup:")
        for m in cleanup_msgs: print(m)

    # ── Batch 5: dedupe report ──────────────────────────────────────────
    def _print_report(label, rep):
        print(f"\n--- {label} dedupe ---")
        print(f"  Added (no name conflict):   {len(rep['added'])}")
        print(f"  Skipped (exact duplicate):  {len(rep['skipped_dup'])}")
        print(f"  Kept with (2) suffix:       {len(rep['kept_with_suffix'])}")
        if rep["skipped_dup"]:
            print("  Skipped:")
            for n, c in rep["skipped_dup"]: print(f"    - {n}  ({c})")
        if rep["kept_with_suffix"]:
            print("  Suffixed:")
            for orig, new, c in rep["kept_with_suffix"]:
                print(f"    - {orig}  ->  {new}  ({c})")

    print("\n" + "=" * 62)
    print("  Batch 5 dedupe report")
    print("=" * 62)
    _print_report("Drums",  drum_report)
    _print_report("Synth",  synth_report)
    _print_report("Bass",   bass_report)

    total = drum_count + synth_count + bass_count + harm_count + bsp_count
    print(f"\nDone. {drum_count} drum + {synth_count} synth + {bass_count} bass + "
          f"{harm_count} Harmless + {bsp_count} BaySickPlayer = "
          f"{total} total presets.")

    # ── Batch 5: factory kit generation ─────────────────────────────────
    if not args.starter_only:
        print("\n" + "=" * 62)
        print("  Factory kit generation")
        print("=" * 62)
        print(f"Kits root: {KITS_DIR}")
        kit_count, missing = generate_factory_kits()
        print(f"\n{kit_count} factory kits written.")
        if missing:
            print(f"  ({missing} preset references not yet on disk — see warnings above.)")

        # ── 2026-04-26: Factory templates ──────────────────────────────
        # 29 templates, one per Full kit style — each carries a kit reference
        # plus 8 layer + 4 bass preset references.  Run AFTER kits + presets
        # so all references can be verified against existing files.
        print("\n" + "=" * 62)
        print("  Factory template generation")
        print("=" * 62)
        print(f"Templates root: {TEMPLATES_DIR}")
        tpl_count, tpl_missing = generate_factory_templates()
        print(f"\n{tpl_count} factory templates written.")
        if tpl_missing:
            print(f"  ({tpl_missing} preset/kit references not on disk — see warnings above.)")

if __name__ == "__main__":
    main()
