---
name: apvts-reference
description: BaySickDAW APVTS parameter IDs - global params, per-bus M/S EQ prefixes, the per-mixer-strip lazy param table (level/pan/mute/solo/polarity/width/bypass/arm), routing send params, and the MixerChannelIds id ranges. Load when working with parameter ids, mixer strip params, routing, or automation lanes.
---

# APVTS Parameter IDs

Moved out of CLAUDE.md 2026-07-28: this is a schema reference derived from the
APVTS layout in `PluginProcessor`, so it does not need to be resident in every
session -- but it is tedious to reconstruct by grepping, which is why it is kept
here rather than deleted.  Source of truth remains the code.

## APVTS Parameter IDs

### Global
- `masterGain` (Float 0-1)

### Drums M/S EQ (prefix `drums_mid_eq{b}_` / `drums_side_eq{b}_` where b=0..7)
- `freq` (Float 20-20000), `gain` (Float -18-18), `q` (Float 0.1-10), `type` (Int 0-7), `on` (Bool)

### Bass M/S EQ (prefix `bass_mid_eq{b}_` / `bass_side_eq{b}_`)
- Same as drums EQ above

### Layers M/S EQ (prefix `layers_mid_eq{b}_` / `layers_side_eq{b}_`)
- Same as drums EQ above

### EQ type codes: 0=Bell, 1=LP, 2=HP, 3=LowShelf, 4=HiShelf, 5=Notch, 6=BandPass, 7=AllPass
### EQ defaults: all 8 bands Bell type, 0 dB gain — flat curve with 8 moveable handles

### Mixer — per-strip lazy APVTS (5F-4a)
Every mixer strip has its own set of params, registered lazily the first time the strip is created. Prefix format varies by strip type:

| Strip type     | Prefix format                    | Params (suffixes)                                           |
|----------------|----------------------------------|-------------------------------------------------------------|
| Master         | `mixer_master`                   | `_level`, `_pan`, `_width`                                  |
| Bus (5 total)  | `mixer_{layers,bass,drums,fx,clipsbus}` | `_level`, `_pan`, `_mute`, `_solo`, `_polarity`, `_width` |
| Layer insert   | `mixer_layer_{0..7}`             | `_level`, `_pan`, `_mute`, `_solo`, `_polarity`, `_width`, `_bypass`, `_arm` |
| Bass insert    | `mixer_bass_{0..3}`              | same as Layer insert                                        |
| Drum insert    | `mixer_drum_{0..15}`             | same as Layer insert                                        |
| Audio insert   | `mixer_audio_{0..49}`            | same as Layer insert                                        |

Param ranges: `_level` Float `-60..+10` dB (default 0), `_pan` Float `-1..+1` (default 0), `_mute`/`_solo`/`_polarity`/`_bypass`/`_arm` Bool (default false), `_width` Float `0..2` (default 1.0 — M/S encode → scale side → decode).

Registration entry points (PluginProcessor):
- `ensureMixerBusAndMasterParams()` — bulk-registers master + 5 buses (called once at startup)
- `ensureMixerStripParams(prefix, kind)` — lazy per-insert; `kind ∈ {Master, Bus, Insert}`

### Per-Insert Audio Nodes (5F-4a)
Per-insert audio processing moved from `PluginProcessor`'s render loop into new `InsertNode` type in `VibeGraph`. Each insert owns: `EffectRack`, post-rack `EQ8MsDSP`, peakDb atomic, CompDelayLine. Process order: polarity flip → M/S width → rack (bypassable) → EQ → fader × mute × solo → peak push. Registered via `VibeGraph::ensureInsertNode(kind, index, displayName, apvtsPrefix)`. Kinds: `Layer`, `Bass`, `Drum`, `Audio`.

### Routing (5F-4b B1a)
Every mixer strip gains routing params registered lazily alongside the rest:

| Suffix                | Type  | Range        | Default            | Purpose |
|-----------------------|-------|--------------|--------------------|---------|
| `_sendTo`             | Int   | `0..999`     | natural parent bus | Main-out destination channel id |
| `_send{0..3}_to`      | Int   | `-1..999`    | `-1` (inactive)    | Additional send destination (or inactive) |
| `_send{0..3}_amount`  | Float | `-60..+6` dB | `0` dB             | Send level |
| `_send{0..3}_prepost` | Bool  | —            | `false` (post)     | Pre-fader vs post-fader tap |

Channel ids (see `MixerChannelIds` namespace in `Source/VibeGraph.h`):
- `0` = Output (terminal — only Master routes here)
- `1`–`6` = Layers / Bass / Drums / Master / FX / Clips buses
- `100..115` = Aux 0..15
- `200..215` = Layer insert 0..15
- `300..315` = Bass insert 0..15
- `400..449` = Audio insert 0..49
- `500..515` = Drum insert 0..15

Defaults preserve current behavior (Layer insert → Layers Bus, etc.). `RoutingGraph` (in `VibeGraph.h/.cpp`) provides `wouldCreateCycle(src, dst)` for UI pre-flight + `rebuildFromApvts()` for block-rate graph resolution with Kahn's topo sort + cycle drop. Audio path wiring lands in B1b; these params are registered and cycle-checked now but are no-ops in the audio domain until then.

---
