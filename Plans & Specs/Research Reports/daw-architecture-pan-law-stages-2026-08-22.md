# DAW Architecture Research -- Pan Law Architecture Across Mixing Stages -- 2026-08-22

## Problem statement
A signal in BaySickDAW passes three mixer pan stages in series (strip -> bus ->
master) plus up to three engine-internal pan stages (engine pan knob, per-note
pan, unison spread), and the project-level Pan Law only reaches the three mixer
stages. The owner has ruled that every pan knob follows the one project law and
needs the right architecture before the rework: does a law stack through
centered stages, how does a panner treat stereo content, which of our pan sites
belong to the law, and what shape should the single helper take.

## What BaySickDAW currently does
- `Source/BaySickGraph.cpp:266-288` `applyPanLaw`: 0 = Circular cos/sin
  (0.707 per channel at center), 1 = Triangular linear crossfade (0.5 per
  channel at center, -6 dB), 2 = Square opposite-side-only taper (1.0 at
  center).
- `Source/BaySickGraph.cpp:293-300` `applyStereoPan`: per-channel BALANCE
  gains (no cross-mixing), EARLY-OUT at |pan| < 1e-4 (`:295`). Consequence:
  center is unity for every law, and the first off-center tick jumps to 0.707
  (Circular) or 0.5 (Triangular) -- a 3 dB / 6 dB discontinuity. No gain ramp
  (the fader at `:568-573` ramps; the pan does not).
- Applied at all three stages: insert after fader `:579`; bus after
  polarity/width `:806`; master before width `:846`. Each reads a cached
  `master_pan_law` atomic pointer (`:385`, `:443`, `:693`, `:733`).
- `Source/PluginProcessor.cpp:495` declares `master_pan_law` (Int 0-2);
  `:3769-3773` re-resolves it by STRING every block (two
  `getRawParameterValue("master_pan_law")` calls) into `BlockContext::panLaw`
  (`Source/Engine/BlockContext.h:20-23`, whose comment says "0=-3dB constant
  power / 1=linear / 2=-6dB" -- does not match the graph's 1=-6 dB / 2=0 dB).
  `Source/BaySickGraph.h:450-455`: `processBus` keeps the `panLaw` arg "for
  API stability" and ignores it.
- Menu: `Source/Standalone/StandaloneEditor.cpp:7677-7683`, three entries,
  comment claims FL parity.
- Engine-internal pans, all hardcoded cos/sin with NO center bypass, so every
  one of them sits at 0.707 (-3 dB per channel) at dead center today:
  timeline clip decode `Source/PluginProcessor.cpp:1010-1013`; BaySickPlayer
  voice pan `Source/BaySickPlayer/BaySickPlayerDSP.cpp:1212-1219` (multiplied
  with note pan at `:1162-1174`); BaySickSolstice master pan
  `Source/BaySickSolstice/AdditiveVoice.cpp:786-792` (multiplied with note pan at
  `:726-728`); BaySickSolstice unison spread `:1110-1116`; our SFZ loader
  `Source/SlideSampler/SlideSampler.cpp:1032-1033`. BaySickSynth has NO engine
  pan, so it is the one engine at unity at center.
- Per-note pan: linear opposite-side-only taper, unity at center
  (`Source/BaySickSynth/BaySickSynthVoice.cpp:912-914`,
  `AdditiveVoice.cpp:726-727`, `BaySickPlayerDSP.cpp:1162`).
- Vendored sfizz `libs/sfizz/src/sfizz/Panning.cpp:18-48`: 4095-entry cosine
  table, `tickPan` = L *= cos(p*pi/2), R *= cos((1-p)*pi/2) = sin(p*pi/2),
  per voice; 0.707 each at center. `width` (`:101-111`) uses the same table.
- Engines gate their pan push on a cached value
  (`Source/BaySickSolstice/BaySickSolsticeProcessor.cpp:637`,
  `Source/BaySickPlayer/BaySickPlayerProcessor.cpp:184`), so the law would
  need to be part of that cache key.
- Baseline level picture for an untouched project: mixer stages 0 dB at
  center x3; BaySickSolstice / BaySickPlayer / clip decode / SlideSampler output
  -3 dB at center; BaySickSynth and hosted VST3 0 dB. The engines are already
  3 dB apart from each other at default pan.

## State of the art

### FL Studio (UX reference) -- what the manual actually documents
- **Approach:** The project-level setting is "Panning law" under Options >
  Project general settings > Advanced, with exactly TWO entries. Verbatim from
  the manual: "Circular - Level compensation is applied when panning (orange).
  Level drops 3 dB at center pan." and "Triangular - No level compensation is
  used as a function of pan position (green). The overall level is 3 dB lower,
  across the pan range, compared to the extreme pan positions for circular
  mode." Also: "Circular panning maintains a constant apparent volume by
  progressively lowering the combined volume of the L+R channels by -3 dB as
  the pan passes dead center. Triangular panning does not apply this
  compensation, so the apparent loudness will increase as the sound passes the
  center position." The mixer track pan is NOT a balance control: "The pan
  works by mixing one side of the stereo track into the other, so a 100% pan
  is the mono-sum of the Left and Right channels. If you would like to pan
  using independent Left & Right level changes with no cross channel mixing
  use Fruity Stereo Shaper." The Channel Rack pan is a different mechanism:
  "The Pan and Volume controls are sent directly to the plugin, rather than
  acting on the output audio, so how they are interpreted will depend on the
  plugin. All native plugins will pan, some VST/AU plugins won't respond."
  Per-note PAN exists in the Piano roll note properties / event editor.
- **Source:** [songsettings_settings.htm](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/songsettings_settings.htm),
  [mixer.htm](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer.htm),
  [channelrack.htm](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/channelrack.htm),
  [pianoroll.htm](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/pianoroll.htm) -- all WebFetched.
- **Confidence:** HIGH for every quoted sentence.
- **What this means for our three entries:** "Square" is NOT an FL law (the
  manual lists two). The manual's arithmetic for Triangular ("3 dB lower ...
  compared to the extreme pan positions for circular mode") only works if
  Triangular's extreme is 0 dB and Circular's extreme is +3 dB, i.e. the
  owner's description (Triangular: 0 dB at center, the opposite side tapers,
  the near side stays at 100%) is what the manual implies. That is the formula
  we currently label "Square". Our current "Triangular" (-6 dB linear
  crossfade) is not an FL law at all. The owner's "Square-law = sqrt(1-p)/
  sqrt2, sqrt(1+p)/sqrt2" formula: UNVERIFIED -- not in any Image-Line page
  found this run.
- **Tradeoffs / blind spots:** The manual never states the absolute level of a
  centered track, never states whether the Master track's pan behaves
  differently, and never gives the crossfeed coefficients of the fold.

### FL Studio -- measured absolute levels (third-party)
- **Approach:** admiralbumblebee's DAW-v-DAW sweep (1 kHz -3 dBFS sine,
  main pan automated center -> full right) reports for FL Studio 20.5.1:
  "0dB center, but opposing side rises +3dB. This means that panning can
  cause digital clipping." -- word-for-word the same verdict as Live 10.1,
  whose manual (below, HIGH) confirms center-unity / +3 dB extremes. Cubase
  and Pro Tools are reported in the other category (-3 dB at center, 0 dB at
  the extremes). No FL discontinuity at center is remarked on (REAPER's
  mid-sweep bump is).
- **Source:** [admiralbumblebee DAW v DAW Part 6](https://www.admiralbumblebee.com/music/2019/12/08/Daw-V-Daw-Pan-Curves.html) -- WebFetched.
- **Confidence:** MEDIUM (third-party measurement; the default law, i.e.
  Circular, was measured; Triangular was not).
- **Implication:** FL's Circular is "compensated": a centered mixer track is
  at unity, and the near side rises to +3 dB (dual-mono source, merged) at
  100%. "Level drops 3 dB at center" in the manual describes the curve
  relative to its extremes, not an attenuation of untouched tracks. Two
  Image-Line forum threads were partially readable: staff member Nucleon
  ("We chose -3dB because as you pan audio across 2..." -- truncated behind
  the login wall) and a user measuring a 3 dB drop at the sides in a MONO
  SUM, which is consistent with either formulation and does not
  disambiguate. [t=220524](https://forum.image-line.com/viewtopic.php?t=220524),
  [t=239820](https://forum.image-line.com/viewtopic.php?t=239820) -- MEDIUM/LOW.

### FL Studio plugin SDK -- how native instruments get their L/R levels
- **Approach:** The FL SDK host interface exposes
  `ComputeLRVol(float &LVol, float &RVol, float Pan, float Volume)` ("compute
  left & right levels using pan & volume info"). Voice parameters carry
  `TLevelParams { Pan (-1..1), Vol (0..1), Pitch, FCut, FRes }` as
  `InitLevels` and `FinalLevels` inside `TVoiceParams`. So a native
  instrument (Sytrus, Harmor, the Sampler) does not own a pan formula: it
  hands the host its per-voice pan (channel pan and note pan combined by the
  host into the voice's levels) and the HOST returns the left/right gains.
  The SDK also forwards channel pan to MIDI-style plugins as `FPE_MIDI_Pan`
  ("MIDI channel panning (0..127) in EventValue, FL panning in -64..+64 in
  Flags"). No line in the SDK mentions "law", so whether `ComputeLRVol`
  honors the project's Panning law setting is an INFERENCE (strong: the host
  owns the formula and the host owns the setting), not a documented fact.
- **Source:** [fp_plugclass.h (Barsay SDK mirror)](https://raw.githubusercontent.com/Barsay/FL-Studio-SDK-CMake-Template/main/SDK/fp_plugclass.h),
  [FP_PlugClass.pas (RuudErmers mirror)](https://raw.githubusercontent.com/RuudErmers/RMSVST3/master/FruityPlug/FP_PlugClass.pas) -- both WebFetched.
  [JUCE forum: FL note pan is FL-SDK-only](https://forum.juce.com/t/fl-studio-note-pan-support/56558) -- WebFetched, MEDIUM.
- **Confidence:** HIGH for the mechanism; the "follows the project law"
  inference is UNVERIFIED.
- **Why it matters:** This is the precedent for the owner's ruling. FL pushes
  one host-computed pan formula into its own instruments' per-voice (per-note
  + channel) pan. Third-party plugins get raw pan values and do their own
  thing.

### Ableton Live 12
- **Approach:** Audio Fact Sheet, verbatim: "Live uses constant power panning
  with sinusoidal gain curves. Output is 0 dB at the center position and
  signals panned fully left or right will be increased by +3 dB." No pan law
  option. Mixer manual: default Stereo Pan Mode "positions the track's output
  by increasing the volume of the channel to which the control is set, and
  simultaneously decreasing the volume of the opposite channel" (a balance
  with boost, no fold); Split Stereo Pan Mode gives independent L and R
  positions ("panning one channel ... does not affect the volume of the other
  channel"). The fact sheet recommends narrowing width before extreme pans to
  limit the +3 dB rise.
- **Source:** [Audio Fact Sheet](https://www.ableton.com/en/manual/audio-fact-sheet/),
  [Mixing](https://www.ableton.com/en/manual/mixing/) -- WebFetched.
- **Confidence:** HIGH.
- **Tradeoffs:** Center-unity everywhere means nothing stacks through
  returns/main at center by construction; cost is that hard pans exceed
  unity (+3 dB), which Live documents as a clipping risk.

### Logic Pro
- **Approach:** Project Settings > Audio > Pan Law: 0 dB, -3 dB, -3 dB
  Compensated (default), -4.5 dB (+compensated), -6 dB (+compensated). Apple's
  text (snippet): -3 dB Compensated routes a centered track "with the same
  level" to L and R, and hard left/right "with a +3 dB level increase"; plain
  -3 dB applies -3 dB at center with no change at the extremes; 0 dB changes
  nothing anywhere. A separate checkbox "Apply Pan Law compensation to stereo
  balancers" applies the chosen compensation "to Stereo Balance controls" --
  i.e. stereo channel strips (and therefore aux/bus/output strips, which are
  stereo) use BALANCE controls that are OUTSIDE the pan law unless the user
  opts in. That is why "compensated" laws exist: in-the-box, a centered mono
  track should meter at its fader value and a stereo bus should not lose 3 dB
  for being a bus.
- **Source:** [Apple: General Audio project settings](https://support.apple.com/guide/logicpro/general-project-settings-lgcp4f230784/mac)
  (page is JS-rendered; WebFetch returned navigation only, so the text comes
  from WebSearch snippets of that page), admiralbumblebee (above) confirms the
  default measures 0 dB center / +3 dB sides.
- **Confidence:** MEDIUM (auto-demoted: could not read the page body).

### Cubase / Nuendo
- **Approach:** Project Setup > Pan Law, verbatim: "If you pan a channel left
  or right, the sum of the left and right side is higher (louder), than if
  this channel is panned center. These modes allow you to attenuate signals
  panned center. 0 dB turns off constant-power panning. Equal Power means
  that the power of the signal remains the same regardless of the pan
  setting." Options (snippets): Equal Power (default), -3 dB, -4.5 dB, -6 dB,
  0 dB. Stereo channels default to the Stereo Balance Panner ("control the
  balance between the left and right channels ... activated by default"); the
  Stereo Combined Panner is a linked dual-pan whose handles can cross, and
  setting both to one position sums to mono, which "increases the volume of
  the signal". Forum users (no Steinberg staff): "the stereo track has not
  been panned at all therefore pan law cannot come into play"; the newer
  "Stereo Pan Law" entry "only applies to mono channels".
- **Source:** [Project Setup dialog (v10 archive)](https://archive.steinberg.help/cubase_pro_artist/v10/en/cubase_nuendo/topics/project_handling/project_handling_project_setup_dialog_r.html),
  [Stereo Combined Panner](https://archive.steinberg.help/cubase_pro_artist/v10/en/cubase_nuendo/topics/mixconsole/mixconsole_stereo_combined_panner_c.html),
  [Stereo Balance Panner](https://archive.steinberg.help/cubase_pro_artist/v10/en/cubase_nuendo/topics/mixconsole/mixconsole_stereo_balance_panner_c.html) -- WebFetched (HIGH).
  [Steinberg forum 891355](https://forums.steinberg.net/t/qustions-about-stereo-pan-law-at-project-setting-dialog/891355),
  [Steinberg forum 641149](https://forums.steinberg.net/t/stereo-to-mono-and-pan-law/641149) -- WebFetched, MEDIUM.
- **Confidence:** HIGH for the manual text; MEDIUM for "law is a mono-source
  property, balance at center is unity".

### REAPER
- **Approach:** Project Settings > Advanced > Pan law/mode. Pan law is a
  numeric center attenuation (0.0 dB default, down to -6 dB, or typed), with
  a "Gain compensation (boost pans)" checkbox that flips attenuate-center into
  boost-sides; per-track pan law override; pan modes "Stereo balance / mono
  pan" (default, pre-v4 behavior), "Stereo pan" (folds), "Dual pan"; tapers
  sine / linear / hybrid. admiralbumblebee measured the default as "0.0dB, no
  gain compensation" with a roughly +2 dB mid-sweep rise (linear taper).
- **Source:** [REAPER user guide PDFs (dlz.reaper.fm)](https://www.reaper.fm/userguide.php)
  -- official PDFs either 404'd, exceeded the fetch size cap, or could not be
  text-extracted; text comes from WebSearch snippets of those PDFs.
  [Cockos forum thread](https://forums.cockos.com/showthread.php?p=2570615) -- bot-walled.
- **Confidence:** MEDIUM.

### Pro Tools
- **Approach:** Setup > Session > Pan Depth: -2.5 dB (legacy LE default),
  -3 dB (default, constant power), -4.5 dB (SSL-style), -6 dB (constant
  gain). Production Expert: "the signal level is down by 3db in each speaker
  when panned centrally vs when it's hard panned to either side" and pan
  depth "applies to mono tracks routed to stereo outputs"; meters on mono
  tracks vs the master differ by the pan depth. Uncompensated at center.
- **Source:** [Production Expert](https://www.production-expert.com/production-expert-1/stereo-pan-depth-why-its-important-for-metering) -- WebFetched (third-party), Avid reference guide not fetched.
- **Confidence:** MEDIUM.

### Bitwig Studio
- **Approach:** No pan law option found in the user guide; measured by
  admiralbumblebee as a balance pot that "compensates sides by 4.2dB" (odd
  value, unexplained). User guide (HIGH) on per-note expressions: "Pan
  expressions represent a stereo placement control for each note event. The
  pan expression is often applied at the beginning of the audio signal path.
  The pan expression has no direct interaction with pan automation, which is
  applied by the track mixer after the device chain." Gain expression is
  "applied at the beginning of the audio signal path -- in this case, at the
  output of the instrument device (pre-FX Chain)". So Bitwig treats per-note
  pan as an instrument-stage operation, separate from the mixer pan.
- **Source:** [Bitwig user guide: Working with Note Events](https://www.bitwig.com/userguide/latest/working_with_note_events/) -- WebFetched (HIGH);
  [Bitwig Utility devices](https://www.bitwig.com/userguide/latest/utility/) -- WebFetched (Dual Pan device exists);
  admiralbumblebee + [KVR](https://www.kvraudio.com/forum/viewtopic.php?p=5921432) for the mixer pot -- MEDIUM.
- **Confidence:** HIGH for note-expression placement; MEDIUM for the mixer pot.

### Studio One (now "Fender Studio Pro")
- **Approach:** Console pan measured as "0dB change at center with no opposing
  channel compensation" (balance, center unity). The Dual Pan plug-in (not the
  console) exposes a per-instance Pan Law list: -6 dB Linear, -3 dB Constant
  Power Sin/Cos, -3 dB Constant Power Sqrt, 0 dB Balance Sin/Cos, 0 dB Linear.
- **Source:** s1manual.presonus.com redirects to a login-walled Fender manual;
  text from WebSearch snippets of the manual + admiralbumblebee.
- **Confidence:** MEDIUM.

### Per-note pan in plugin standards (VST3 note expression, CLAP)
- **Approach:** VST3 defines a standard per-note expression `kPanTypeID`:
  "Panning (L-R), plain range [0 = left, 0.5 = center, 1 = right]". The host
  delivers the value with the note; the PLUGIN implements it (there is no
  host-side application path). The VST3 channel-context interface the host
  uses to tell a plugin about its track carries UID, name, color, index,
  namespace, image and plugin location -- no pan, no pan law. CLAP's
  track-info extension carries name, color, channel count, port type and
  return/bus/master flags -- no pan law. There is no standard by which a host
  can push a pan law into a hosted instrument.
- **Source:** [ivstnoteexpression.h](https://raw.githubusercontent.com/steinbergmedia/vst3_pluginterfaces/master/vst/ivstnoteexpression.h),
  [ivstchannelcontextinfo.h](https://raw.githubusercontent.com/steinbergmedia/vst3_pluginterfaces/master/vst/ivstchannelcontextinfo.h),
  [clap/ext/track-info.h](https://raw.githubusercontent.com/free-audio/clap/main/include/clap/ext/track-info.h) -- all WebFetched.
- **Confidence:** HIGH.

### Synth unison spread: Surge XT and Vital
- **Approach:** Surge XT's unison setup (sst-basic-blocks
  `OscillatorDriftUnisonCharacter.h`): single voice -> panL = panR = 1;
  multi-voice -> `d = |voice - mid| / mid` with alternating sign,
  `panL = 1 - d; panR = 1 + d;` (LINEAR, reaches 2.0/0.0 at the outer
  voices), then everything scaled by `1/sqrt(n_unison)`. Vital
  (`synth_oscillator.cpp`): the unison pair is blended between a "stereo"
  and a "center" mix with `equalPowerFade(t) = sin(pi*t/2)` /
  `equalPowerFadeInverse` -- a cos/sin crossfade. Neither consults the host
  (no API to do so). The same Surge library ships `PanLaws.h` ("shamelessly
  borrowed ... from MixMaster"): a 4-gain matrix `{L, R, RinL, LinR}` with
  `monoLinear`, `monoEqualPower` (sin/cos x sqrt2: center unity, +3 dB
  extremes), `monoEqualPowerUnityGainAtExtrema` (plain sin/cos),
  `stereoEqualPower` (balance, exact-center short-circuit to 1/1, else
  sqrt2 sin/cos) and `stereoTruePanning` (near channel stays 1.0, far channel
  cos-fades out while sin-crossfeeding into the near channel; 100% = L+R).
  That matrix is the right general shape for our helper.
- **Source:** [OscillatorDriftUnisonCharacter.h](https://raw.githubusercontent.com/surge-synthesizer/sst-basic-blocks/main/include/sst/basic-blocks/dsp/OscillatorDriftUnisonCharacter.h),
  [PanLaws.h](https://raw.githubusercontent.com/surge-synthesizer/sst-basic-blocks/main/include/sst/basic-blocks/dsp/PanLaws.h),
  [Vital synth_oscillator.cpp](https://raw.githubusercontent.com/mtytel/vital/main/src/synthesis/producers/synth_oscillator.cpp),
  [Vital futils.h](https://raw.githubusercontent.com/mtytel/vital/main/src/synthesis/framework/futils.h) -- all WebFetched.
- **Confidence:** HIGH. Sytrus / Harmor unison pan law: UNVERIFIED (closed
  source; they are FL-native so they COULD route through ComputeLRVol, but
  nothing documents it).

### SFZ `pan` opcode, sfizz, Kontakt
- **Approach:** SFZ spec, verbatim: "If a mono sample is used, pan value
  defines the position in the stereo image where the sample will be placed.
  When a stereo sample is used, the pan value the relative amplitude of one
  channel respect the other." (range -100..100, default 0) -- i.e. placement
  for mono, balance for stereo, law unspecified. sfizz implements it with the
  cosine table (constant power, -3 dB per channel at center), per voice, and
  reuses the table for `width`. Kontakt's instrument pan is reported by users
  as a plain balance; no NI document on its law was found.
- **Source:** [sfzformat.com/opcodes/pan](https://sfzformat.com/opcodes/pan) -- WebFetched (HIGH);
  `libs/sfizz/src/sfizz/Panning.cpp` -- local vendored source (HIGH);
  Kontakt -- WebSearch snippet only (LOW).
- **Confidence:** HIGH / HIGH / LOW.

## Comparative analysis

| Dimension | FL Studio | Live | Logic | Cubase | Pro Tools | REAPER | Bitwig | Studio One |
|---|---|---|---|---|---|---|---|---|
| Law options | Circular / Triangular (2) | none (fixed) | 0, -3, -3c, -4.5(c), -6(c) | EqPow, -3, -4.5, -6, 0 | -2.5, -3, -4.5, -6 | 0..-6 numeric + boost flag, per-track | none found | console fixed; Dual Pan plug-in has 5 |
| Default centered level | unity (measured, MED) | unity (HIGH) | unity (-3 dB compensated, MED) | -3 dB on a MONO source (HIGH/MED) | -3 dB on a mono source (MED) | 0 dB (MED) | unity (MED) | unity (MED) |
| Hard-pan level | +3 dB (Circular, dual-mono merged) | +3 dB | +3 dB | 0 dB | 0 dB | 0 dB (no boost) | +4.2 dB (?) | 0 dB |
| Stereo source at panner | fold: far side mixed into near, 100% = mono-sum (HIGH) | balance with boost; split mode = dual pan | balance (stereo strips) | balance default; combined = linked dual pan | n/a this run | balance default; stereo-pan = fold; dual | balance | balance; dual pan plug-in |
| Law at bus / master | center unity -> nothing stacks | center unity -> nothing stacks | balancers exempt unless checkbox | balance at center untouched | law is a mono-track property | per-track law; center 0 dB default | n/a | n/a |
| Per-note pan | native plugins via host ComputeLRVol | n/a | n/a | VST3 note expression (plugin-implemented) | n/a | n/a | instrument-stage expression, separate from mixer pan | n/a |
| Host pushes law into instruments | yes, FL-native only (mechanism HIGH, law-following inferred) | no | no | no (no VST3 API) | no | no | no | no |

The cross-DAW convention, stated plainly:
1. A pan law is a statement about placing a MONO signal into a stereo field.
   It shapes the sweep; it is never a standing attenuation applied to stereo
   content sitting at center. Every DAW examined leaves a centered stereo
   balance/pan at unity, either by definition (balance controls are exempt:
   Logic, Cubase) or by normalizing the law to center-unity ("compensated":
   Live, Logic default, FL as measured). Therefore -3 dB does NOT stack
   through insert -> bus -> master in any of them.
2. The uncompensated -3 dB-at-center laws (Pro Tools, Cubase EqPow, Logic
   plain -3 dB) are console-heritage behaviors that attenuate mono tracks
   only; the in-the-box DAWs that target non-engineers (Live, FL) chose
   center-unity with a +3 dB rise at the extremes and documented the
   clipping caveat instead.
3. Stereo handling is a separate axis from the law: balance (most DAWs) vs
   fold/"true pan" (FL's mixer, REAPER's stereo-pan mode, MixMaster's
   stereoTruePanning). FL is the only one of the eight whose DEFAULT mixer
   pan folds.
4. Instrument-internal pans (per-note, unison, sampler opcodes, Kontakt) are
   owned by the instrument everywhere -- with ONE exception, which happens
   to be our reference: FL-native instruments get their per-voice L/R gains
   from the host. No plugin standard carries a pan law, so that precedent
   stops at the FL-native boundary; VST/AU plugins in FL own their pan.

## Recommendation for BaySickDAW

### 1. Stage stacking: keep all three stages on the law, but formulate every law center-unity ("compensated")
Do NOT adopt the literal "-3 dB at center" reading at every stage. Define each
law so that pan = 0 yields gains of exactly 1.0 and the curve is continuous
through center; then strip, bus and master can all honor the law (as ruled)
without any centered stage changing level, and the `|pan| < 1e-4` early-out
becomes a pure optimization instead of a behavior cliff. Panned stages still
compound (a strip panned right feeding a bus panned right) -- that is what
Logic/Cubase/Live balance-on-balance does and what a user expects.

Level consequences for a default centered mix, relative to today:
- Recommended (center-unity laws at all stages): 0 dB change at every mixer
  stage. Panned strips get LOUDER than today: Circular hard pan +3 dB on the
  near side (today 0 dB), mid-sweep no longer drops to 0.707 at the first
  tick. Triangular hard pan 0 dB (today's "Square" behavior); the -6 dB
  crossfade disappears.
- Rejected literal option (uncompensated -3 dB Circular at every stage):
  -3 dB per centered stage = -9 dB strip->bus->master, and another -3 dB for
  each engine pan / note pan stage that also adopts it: up to -15 dB on an
  untouched project, contradicting FL's measured center-unity. Not FL parity.
- Rejected strips-only option (Logic-style exempt buses, uncompensated law on
  strips): -3 dB once, still moves every project, still contradicts the
  measurement, and contradicts the owner's ruling that every knob follows
  the law.

### 2. Stereo source at a panner: two tiers
- Pragmatic: keep BALANCE semantics (what Live/Logic/Cubase/Studio One do by
  default), just with the corrected laws. Cheapest; far-side content is lost
  at 100%.
- Do-it-right for FL parity: implement FL's documented FOLD ("mixing one
  side of the stereo track into the other, so a 100% pan is the mono-sum").
  Dual-mono contract that satisfies both the manual and the measurement:
  for pan p >= 0, with (gL, gR) = the law's center-unity mono gains,
  `L_out = gL(p) * L`, `R_out = a(p) * R + b(p) * L` with `a + b = gR(p)`,
  `a(0) = 1`, `b(0) = 0`, `a(1) = b(1) = gR(1) / 2`. Circular gives
  `R_out = 0.707 (L + R)` at 100% = +3 dB for dual-mono (matches the
  measurement); Triangular gives `0.5 (L + R)` = 0 dB (matches "3 dB lower
  than circular's extremes"). The split between a(p) and b(p) for TRUE
  stereo content is not documented anywhere and is a design choice; see
  Open questions for the 2-minute FL measurement that pins it. Cost: a 2x2
  matrix (4 multiply-adds per frame instead of 2) -- negligible. Known FL
  side effect to accept knowingly: anti-phase stereo content cancels when
  folded (the forum complaint), and mono-sum at 100% collapses width. Apply
  the fold only at the three MIXER stages (that is where FL does it); engine
  pans stay mono-placement or balance (below).

### 3. Which pan sites follow the project law
- Mixer strip / bus / master: yes (stereo matrix form of the law).
- Engine pan knobs (BaySickPlayer pan, BaySickSolstice master pan, the Clips-page
  pan that drives timeline clip decode): yes -- FL precedent is exactly this
  (channel pan -> host ComputeLRVol). Use the MONO form for synthesized /
  mono-sample voices and the balance form for stereo samples and stereo clip
  files (SFZ-spec semantics: placement for mono, balance for stereo).
  CONSEQUENCE: these sites sit at -3 dB today (cos/sin, no center bypass);
  center-unity laws lift BaySickSolstice, BaySickPlayer, timeline clips and
  SlideSampler by +3 dB at default pan. BaySickSynth (no engine pan) and
  hosted VST3 do not move, so the relative balance between engine families
  shifts by 3 dB. Two ways to handle it, spec call: (a) accept the +3 dB
  (removes today's hidden inconsistency where BaySickSynth is already 3 dB
  hotter than BaySickSolstice at default), or (b) fold a fixed -3 dB into each
  affected engine's output trim so nothing moves.
- Per-note pan: yes, but combine POSITIONS, not gains. FL hands the host one
  Pan value per voice (`FinalLevels.Pan`), not two stages multiplied. Sum the
  engine pan and the note pan, clamp to [-1, 1], evaluate the law once per
  voice (at note-on / ramp target; the existing CC89 ramps keep working on
  the position). Multiplying two uncompensated laws would double-attenuate;
  with center-unity laws the multiply is harmless at center but still wrong
  at the extremes (+6 dB). How FL combines channel pan and note pan
  (additive? weighted?) is UNVERIFIED; additive-clamped is the simplest
  reading of a single Pan field.
- Unison spread: keep engine-internal cos/sin (Surge: linear + 1/sqrt(N);
  Vital: equal-power blend; nobody routes spread through a host law, and it
  is a stereo-image generator, not a placement knob -- the "constant
  loudness across a sweep" rationale does not apply to a static spread).
  If the ruling is read as covering it anyway: run each voice's spread
  position through the mono helper and add a 1/sqrt(N) (or equal-power pair)
  normalization so Circular's +3 dB outer voices do not raise unison
  loudness with spread. Spec call; recommend keep internal.
- Our SFZ loader (SlideSampler `pan` opcode): keep the current constant
  power. The opcode is instrument CONTENT authored by the SFZ writer, not a
  user knob, and sfizz uses the identical cosine law, so the Guitars/Basses/
  RustyDrums engines and our loader stay consistent with each other.
- Vendored sfizz: leave untouched (third-party; no API; patching a vendored
  lib is a maintenance tax; FL treats a third-party instrument's internal pan
  as the plugin's business).
- Hosted VST3: impossible (no VST3/CLAP channel-context field for pan law).
  Their law-following pan is the Plugins-tab mixer strip.

### 4. Helper shape
One header-only unit, `Source/DSP/PanLaw.h`, `namespace baysick::pan`:
- `enum class Law : int { Circular = 0, Triangular = 1 };` -- drop Square
  (it is FL's Triangular under a wrong name); the APVTS Int stays 0..2 or
  shrinks to 0..1 (spec call; pre-v1, no load shim needed).
- `struct MonoGains { float l, r; };`
  `MonoGains monoGains (float pan, Law) noexcept;` -- center-unity:
  Circular `l = sqrt2 * cos(t*pi/2), r = sqrt2 * sin(t*pi/2)` with
  `t = (pan+1)/2`; Triangular `l = pan <= 0 ? 1 : 1 - pan`,
  `r = pan >= 0 ? 1 : 1 + pan`.
- `struct StereoMatrix { float ll, rr, lr /*L into R*/, rl /*R into L*/; };`
  `StereoMatrix balance (float pan, Law) noexcept;` (lr = rl = 0)
  `StereoMatrix fold (float pan, Law) noexcept;` (FL merge-and-pan, section 2)
- `void apply (juce::AudioBuffer<float>&, const StereoMatrix& from,
  const StereoMatrix& to) noexcept;` -- ramped across the block like the
  fader (`BaySickGraph.cpp:568-573`), replacing the unramped `applyGain`
  pair; identity short-circuit when both matrices are {1,1,0,0}.
- Law distribution without per-block APVTS lookups: a processor-owned
  `std::atomic<int> mPanLaw` updated from an APVTS listener on
  `master_pan_law` (message thread). The graph nodes already cache the raw
  atomic pointer; replace the two per-block string lookups at
  `PluginProcessor.cpp:3770-3773` with that atomic. Engines are separate
  processors with their own APVTS, so EngineRig stamps each engine at
  creation with `setPanLawSource (const std::atomic<int>*)` (mirrors how
  model-side registration already works); each engine does one relaxed load
  per block and folds the law into its existing change-gate
  (`BaySickSolsticeProcessor.cpp:637`, `BaySickPlayerProcessor.cpp:184`) so gains
  are only recomputed when pan OR law changed (CPU Safeguarding rule).
  Retire `BlockContext::panLaw` and the dead `processBus` argument in the
  same pass.

## Implementation sketch (if adopted)
1. `Source/DSP/PanLaw.h` (new): enum, MonoGains, StereoMatrix, monoGains,
   balance, fold, apply-with-ramp. Unit-check values at pan = -1, 0, +1 and
   continuity at +/-1e-3 for both laws.
2. `Source/BaySickGraph.cpp`: delete `applyPanLaw` / `applyStereoPan`
   (`:259-300`); insert (`:579`), bus (`:806`), master (`:846`) call
   `pan::apply` with a per-node `StereoMatrix mLastPan` for the ramp; choose
   `fold` (FL parity) or `balance` per the section-2 decision. Fix the
   `BlockContext.h:20-23` comment or delete the field; drop the `panLaw`
   argument from `processBus` (`BaySickGraph.h:454-455`,
   `PassiveStripTask.cpp:83`).
3. `Source/PluginProcessor.cpp`: `std::atomic<int> mPanLaw` + APVTS listener;
   remove the string lookups at `:3769-3773`; EngineRig hands the pointer to
   every engine it creates (`Source/EngineRig.cpp`).
4. Engines: `BaySickPlayerVoice::setPan` (`BaySickPlayerDSP.cpp:1212`),
   `AdditiveVoice::setPan` (`AdditiveVoice.cpp:786`), clip decode
   (`PluginProcessor.cpp:1010-1013`) -> `pan::monoGains` (or `balance` for
   stereo material). Note pan: replace the `npL/npR` taper at
   `BaySickSynthVoice.cpp:912`, `AdditiveVoice.cpp:726`,
   `BaySickPlayerDSP.cpp:1162` with a combined-position evaluation; keep the
   ramp state on the position. Decide (a)/(b) from section 3 for the +3 dB
   shift. Leave `AdditiveVoice.cpp:1110-1116` (unison), `SlideSampler.cpp:1032`
   and `libs/sfizz` alone.
5. Menu (`StandaloneEditor.cpp:7677-7683`): two entries. Labels are a spec
   call; FL's own wording is "Circular" / "Triangular". Doc sites that
   currently describe three entries: `Plans & Specs/System Reference/
   Mixer.md:244` and `:318`, `Verbatim Strings.md:10` and `:28`,
   `MANUAL-1 Screenshot List.md:1214`, `Callout Registry.md:2058`.
6. Batch sizing: helper + graph + processor wiring = one task with one build
   gate; engine sites + note pan = a second task; menu/docs = third. Ear
   check: a mono sample on a strip swept L->R under each law (no step at
   center, +3 dB at the extreme under Circular), a stereo file folded at 100%,
   and the default-project level before/after (should read 0 dB change at the
   master for option (b), +3 dB on the affected engines for option (a)).

## Open questions / further reading
- FL fold coefficients for TRUE stereo content: a 2-minute measurement in FL
  settles it -- a test file with a tone in L only, mixer pan 100% right,
  Circular: if R reads -3 dB relative to the source, b(1) = 0.707 (the
  dual-mono contract above); if it reads 0 dB, FL uses MixMaster-style
  near-channel-untouched crossfeed and the dual-mono +3 dB measurement was
  produced by different coefficients than assumed. Repeat with the same file
  and a tone in R only to get a(1).
- Whether FL's Master track pan is the same code path as an insert's (manual
  silent; measurement did not isolate it).
- Whether `ComputeLRVol` follows the Panning law setting (inference only).
- How FL combines channel pan with note pan into `FinalLevels.Pan`.
- Whether Jeff wants a third, non-FL law kept as an advanced option (-6 dB
  linear crossfade is real on consoles but is not in FL).
- Logic / REAPER manual bodies could not be fetched this run; the table rows
  for those two rest on snippets and a third-party measurement.

## Methodology + caveats
- Main-session verification (2026-08-22, after the agent run): the FL manual
  page (two laws, both quotes), the admiralbumblebee FL measurement ("0dB
  center, but opposing side rises +3dB", FL 20.5.1.522, 1 kHz -3 dBFS sine,
  main pan automated center -> full right) and the Ableton Audio Fact Sheet
  quote were re-fetched and confirmed verbatim.
- WebFetched URLs (HIGH):
  - https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/songsettings_settings.htm
  - https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer.htm
  - https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/channelrack.htm
  - https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/pianoroll.htm
  - https://raw.githubusercontent.com/Barsay/FL-Studio-SDK-CMake-Template/main/SDK/fp_plugclass.h
  - https://raw.githubusercontent.com/RuudErmers/RMSVST3/master/FruityPlug/FP_PlugClass.pas
  - https://www.ableton.com/en/manual/audio-fact-sheet/
  - https://www.ableton.com/en/manual/mixing/
  - https://archive.steinberg.help/cubase_pro_artist/v10/en/cubase_nuendo/topics/project_handling/project_handling_project_setup_dialog_r.html
  - https://archive.steinberg.help/cubase_pro_artist/v10/en/cubase_nuendo/topics/mixconsole/mixconsole_stereo_combined_panner_c.html
  - https://archive.steinberg.help/cubase_pro_artist/v10/en/cubase_nuendo/topics/mixconsole/mixconsole_stereo_balance_panner_c.html
  - https://www.bitwig.com/userguide/latest/working_with_note_events/
  - https://www.bitwig.com/userguide/latest/utility/
  - https://sfzformat.com/opcodes/pan
  - https://raw.githubusercontent.com/steinbergmedia/vst3_pluginterfaces/master/vst/ivstnoteexpression.h
  - https://raw.githubusercontent.com/steinbergmedia/vst3_pluginterfaces/master/vst/ivstchannelcontextinfo.h
  - https://raw.githubusercontent.com/free-audio/clap/main/include/clap/ext/track-info.h
  - https://raw.githubusercontent.com/surge-synthesizer/sst-basic-blocks/main/include/sst/basic-blocks/dsp/OscillatorDriftUnisonCharacter.h
  - https://raw.githubusercontent.com/surge-synthesizer/sst-basic-blocks/main/include/sst/basic-blocks/dsp/PanLaws.h
  - https://raw.githubusercontent.com/mtytel/vital/main/src/synthesis/producers/synth_oscillator.cpp
  - https://raw.githubusercontent.com/mtytel/vital/main/src/synthesis/framework/futils.h
  - local: libs/sfizz/src/sfizz/Panning.cpp, Panning.h
- WebFetched third-party / forum (MEDIUM):
  - https://www.admiralbumblebee.com/music/2019/12/08/Daw-V-Daw-Pan-Curves.html
  - https://www.production-expert.com/production-expert-1/stereo-pan-depth-why-its-important-for-metering
  - https://forum.juce.com/t/fl-studio-note-pan-support/56558
  - https://forum.image-line.com/viewtopic.php?t=67821 (login-truncated)
  - https://forum.image-line.com/viewtopic.php?t=220524 (login-truncated)
  - https://forum.image-line.com/viewtopic.php?t=239820
  - https://forum.image-line.com/viewtopic.php?t=333236 (login-truncated)
  - https://forums.steinberg.net/t/qustions-about-stereo-pan-law-at-project-setting-dialog/891355
  - https://forums.steinberg.net/t/stereo-to-mono-and-pan-law/641149
  - https://forums.steinberg.net/t/pan-law-equal-power-vs-3db/960506
- WebSearch-only / snippet (MEDIUM, page body not readable):
  - https://support.apple.com/guide/logicpro/general-project-settings-lgcp4f230784/mac (JS-rendered)
  - https://www.reaper.fm/userguide.php and the dlz.reaper.fm user-guide PDFs (404 / size cap / unparseable)
  - https://s1manual.presonus.com/en/Content/Built-In_Effects_Topics/Mixing.htm (redirects to a login-walled Fender manual)
  - https://www.kvraudio.com/forum/viewtopic.php?p=5921432 (Bitwig pan pot)
  - Kontakt pan law: NI manual snippet only (LOW)
- Failed fetches: forum.image-line.com t=180986 (login wall), gearspace (403),
  logicprohelp.com (403), forums.cockos.com + wiki.cockos.com (bot wall),
  help.ableton.com Split Stereo article (403), Surge main-repo code search
  (login wall; resolved via the sst-basic-blocks raw files instead).
- What I did NOT find that I'd expect to find: any Image-Line statement of
  the absolute level of a centered track or of the fold's coefficients; any
  FL document describing a "Square" law; any vendor statement that a host
  pushes a pan law into hosted instruments (none exists in VST3 or CLAP);
  Sytrus/Harmor unison pan law.
- Confidence in recommendation: HIGH that center-unity laws at every stage
  plus the two-law menu is the FL-consistent architecture (manual text +
  measurement + every other DAW converging on center-unity for stereo);
  MEDIUM on the exact fold coefficients for true-stereo content (needs the
  FL measurement listed above).
