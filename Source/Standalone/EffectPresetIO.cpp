#include "EffectPresetIO.h"
#include "../AppPaths.h"

#include "../EffectRack.h"
#include "../DSP/DSPBase.h"
#include "../DSP/CompressorDSP.h"
#include "../DSP/ReverbDSP.h"
#include "../DSP/ChorusDSP.h"
#include "../DSP/DelayDSP.h"
#include "../DSP/SaturationDSP.h"
#include "../DSP/FlangerDSP.h"
#include "../DSP/OverdriveDSP.h"
#include "../DSP/PhaserDSP.h"
#include "../DSP/TransientShaperDSP.h"
#include "../DSP/LimiterDSP.h"
#include "../DSP/DeEsserDSP.h"

// ─────────────────────────────────────────────────────────────────────────────
// EffectPresetIO - H-9 prep (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────

namespace EffectPresetIO
{

juce::String typeFolderName (EffectType type)
{
    switch (type)
    {
        case EffectType::Compressor:      return "Compressor";
        case EffectType::Reverb:          return "Reverb";
        case EffectType::Chorus:          return "Chorus";
        case EffectType::Delay:           return "Delay";
        case EffectType::Saturation:      return "Saturation";
        case EffectType::Flanger:         return "Flanger";
        case EffectType::Overdrive:       return "Overdrive";
        case EffectType::Phaser:          return "Phaser";
        case EffectType::TransientShaper: return "Transient Shaper";
        case EffectType::Tape:            return "Tape";
        case EffectType::Limiter:         return "Limiter";
        case EffectType::DeEsser:         return "DeEsser";

        // I-2 (2026-05-02): BaySickPedals 18-module folder names.  Routed to
        // a "Pedals" subfolder by typeRoot() below, not folded into the flat
        // Effects/ tree, so the user's preset library stays organised.
        //
        // Naming locked 2026-05-02 (option A picker): drop the "X Style"
        // code prefix from user-facing labels; folder names mirror picker
        // labels.  CS Style Compressor isn't here -- it's a 4th Type on
        // existing Compressor.  Same for Overdrive (Rack) / (Pedal): single
        // "Overdrive" entry with Mode dropdown switching algorithm.
        case EffectType::BluesDriveStyle:     return "Blues Drive";
        case EffectType::DistortionStyle:     return "Distortion";
        case EffectType::FuzzStyle:           return "Fuzz";
        case EffectType::NoiseGateStyle:      return "Noise Gate";
        case EffectType::HighGainStyle:       return "High-Gain";
        case EffectType::TunerStyle:          return "Tuner";
        case EffectType::AcousticPreampStyle:    return "Acoustic Preamp";
        case EffectType::AcousticSimulatorStyle: return "Acoustic Simulator";
        case EffectType::NAMPedalStyle:          return "User NAM Pedal";
        case EffectType::GraphicEQStyle:      return "Graphic EQ";
        case EffectType::SynthStyle:          return "Polyphonic Synth";
        case EffectType::OctaveStyle:         return "Octave";
        case EffectType::WahStyle:            return "Wah";
        case EffectType::BassGraphicEQStyle:  return "Bass Graphic EQ";
        case EffectType::BassCompressorStyle: return "Bass Compressor";
        case EffectType::BassDriverStyle:     return "Bass Driver";
        case EffectType::BassOverdriveStyle:  return "Bass Overdrive";
        case EffectType::FurmanEQStyle:       return "Pro Parametric EQ";

        // QA-Fe2's locked vocal-chain stages + TS6's hosted type were missing
        // entirely: an empty name collapsed their presets into the shared
        // Presets/Effects root, where a load could hand one effect another
        // effect's state.
        case EffectType::Gate:            return "Gate";
        case EffectType::DeReverb:        return "De-Reverb";
        case EffectType::VST3Plugin:      return "VST3 Plugins";

        case EffectType::None:            return {};
    }
    return {};
}

// I-2 (2026-05-02): pedal-type predicate.  Used by typeRoot() to route the 17
// new pedal-style entries into a dedicated "Pedals" subfolder under
// Presets/Effects/.  Existing 12 effect types stay flat under Presets/Effects/.
static bool isPedalType (EffectType type) noexcept
{
    switch (type)
    {
        case EffectType::BluesDriveStyle:
        case EffectType::DistortionStyle:
        case EffectType::FuzzStyle:
        case EffectType::NoiseGateStyle:
        case EffectType::HighGainStyle:
        case EffectType::TunerStyle:
        case EffectType::AcousticPreampStyle:
        case EffectType::AcousticSimulatorStyle:
        case EffectType::NAMPedalStyle:
        case EffectType::GraphicEQStyle:
        case EffectType::SynthStyle:
        case EffectType::OctaveStyle:
        case EffectType::WahStyle:
        case EffectType::BassGraphicEQStyle:
        case EffectType::BassCompressorStyle:
        case EffectType::BassDriverStyle:
        case EffectType::BassOverdriveStyle:
        case EffectType::FurmanEQStyle:
            return true;
        default:
            return false;
    }
}

juce::File presetsRoot()
{
    return AppPaths::appRoot()
              .getChildFile ("Presets")
              .getChildFile ("Effects");
}

juce::File userNamPedalsDir()
{
    return presetsRoot().getChildFile ("Pedals").getChildFile ("User NAM Pedals");
}

juce::File irDir()
{
    return presetsRoot().getChildFile ("IR");
}

juce::File typeRoot (EffectType type)
{
    // I-2: pedals nest one level deeper under Effects/Pedals/{TypeName}/.
    if (isPedalType (type))
        return presetsRoot().getChildFile ("Pedals").getChildFile (typeFolderName (type));
    return presetsRoot().getChildFile (typeFolderName (type));
}

juce::File factoryDir (EffectType type)
{
    return typeRoot (type).getChildFile ("Factory");
}

juce::File myPresetsDir (EffectType type)
{
    return typeRoot (type).getChildFile ("My Presets");
}

juce::File defaultFile (EffectType type)
{
    return typeRoot (type).getChildFile ("Default.xml");
}

void ensureFolderTree (EffectType type)
{
    if (type == EffectType::None) return;
    factoryDir (type)  .createDirectory();
    myPresetsDir (type).createDirectory();
}

// ─────────────────────────────────────────────────────────────────────────────
// XML wrapper helpers.
// Wraps the DSP's binary state blob in <EffectPreset> with metadata so
// future format changes can detect/migrate.  Blob is base64 to keep XML
// human-readable (no binary chars in the file).
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    constexpr const char* kRootTag    = "EffectPreset";
    constexpr const char* kBlobAttr   = "blob";
    constexpr const char* kTypeAttr   = "effectType";
    constexpr const char* kNameAttr   = "name";
    constexpr const char* kVersionTag = "version";
    constexpr int         kPresetVersion = 1;

    juce::String safeFileName (juce::String s)
    {
        // strip path-unsafe chars; fall back to "untitled"
        for (auto c : juce::String ("\\/:*?\"<>|"))
            s = s.replace (juce::String::charToString (c), "_");
        s = s.trim();
        return s.isNotEmpty() ? s : "untitled";
    }

    bool writePresetXml (const juce::File& dest, EffectType type,
                          const juce::String& name,
                          const juce::MemoryBlock& dspState,
                          juce::String& outErr)
    {
        juce::XmlElement xml (kRootTag);
        xml.setAttribute (kVersionTag, kPresetVersion);
        xml.setAttribute (kTypeAttr,   typeFolderName (type));
        xml.setAttribute (kNameAttr,   name);
        xml.setAttribute (kBlobAttr,   dspState.toBase64Encoding());

        if (! dest.getParentDirectory().createDirectory())
        {
            outErr = "Could not create folder: "
                       + dest.getParentDirectory().getFullPathName();
            return false;
        }
        if (! xml.writeTo (dest, juce::XmlElement::TextFormat()))
        {
            outErr = "Could not write preset file: " + dest.getFullPathName();
            return false;
        }
        return true;
    }

    bool readPresetXml (const juce::File& src, juce::MemoryBlock& outBlob,
                         juce::String& outErr)
    {
        if (! src.existsAsFile())
        {
            outErr = "Preset file does not exist: " + src.getFullPathName();
            return false;
        }
        auto xml = juce::parseXML (src);
        if (! xml || ! xml->hasTagName (kRootTag))
        {
            outErr = "Preset file is not a valid effect preset.";
            return false;
        }
        const juce::String b64 = xml->getStringAttribute (kBlobAttr);
        if (b64.isEmpty())
        {
            outErr = "Preset file has no state blob.";
            return false;
        }
        if (! outBlob.fromBase64Encoding (b64))
        {
            outErr = "Preset file has corrupted state blob.";
            return false;
        }
        return true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Save / load.
// ─────────────────────────────────────────────────────────────────────────────
bool savePreset (DSPBase& dsp, EffectType type,
                  const juce::String& presetName, juce::String& outErr)
{
    if (type == EffectType::None)
    {
        outErr = "Cannot save preset for empty effect slot.";
        return false;
    }
    juce::MemoryBlock blob;
    dsp.getStateInformation (blob);
    if (blob.getSize() == 0)
    {
        outErr = "DSP returned empty state.";
        return false;
    }
    const auto file = myPresetsDir (type).getChildFile (
                          safeFileName (presetName) + ".xml");
    return writePresetXml (file, type, presetName, blob, outErr);
}

bool loadPreset (DSPBase& dsp, const juce::File& presetFile, juce::String& outErr)
{
    juce::MemoryBlock blob;
    if (! readPresetXml (presetFile, blob, outErr)) return false;
    dsp.setStateInformation (blob.getData(), (int) blob.getSize());
    return true;
}

void restoreDefaults (DSPBase& dsp, EffectType type)
{
    // Make a fresh DSP of this type (constructor defaults), capture its
    // state, copy onto the live DSP via setStateInformation.  This is
    // the cleanest "reset to factory init" because every DSP already
    // has its own default-constructor values.
    auto fresh = EffectRack::createEffect (type);
    if (! fresh) return;
    juce::MemoryBlock blob;
    fresh->getStateInformation (blob);
    if (blob.getSize() == 0) return;
    dsp.setStateInformation (blob.getData(), (int) blob.getSize());
}

bool saveAsDefault (DSPBase& dsp, EffectType type, juce::String& outErr)
{
    if (type == EffectType::None) { outErr = "Empty slot."; return false; }
    juce::MemoryBlock blob;
    dsp.getStateInformation (blob);
    if (blob.getSize() == 0) { outErr = "Empty DSP state."; return false; }
    return writePresetXml (defaultFile (type), type, "Default", blob, outErr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Enumeration.
// ─────────────────────────────────────────────────────────────────────────────
juce::Array<juce::File> enumerateFactory (EffectType type)
{
    juce::Array<juce::File> out;
    factoryDir (type).findChildFiles (out, juce::File::findFiles, false, "*.xml");
    return out;
}

juce::Array<juce::File> enumerateMyPresets (EffectType type)
{
    juce::Array<juce::File> out;
    myPresetsDir (type).findChildFiles (out, juce::File::findFiles, false, "*.xml");
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory preset definitions.  Each entry's configure() takes a fresh DSP
// of the matching type and writes parameter values onto it; the framework
// then captures getStateInformation and writes the XML to disk.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    struct FactoryDef
    {
        EffectType                       type;
        const char*                      name;
        std::function<void(DSPBase*)>    configure;
    };

    // Helper macros to keep each preset's config readable.  Configure
    // lambdas dynamic_cast the DSPBase* to the concrete type and write
    // parameter values directly via setters.
    #define AS(T) auto* d = dynamic_cast<T*>(p); if (! d) return;

    static const std::vector<FactoryDef>& factoryTable()
    {
        static const std::vector<FactoryDef> table = {
        // ── Compressor (3) ─────────────────────────────────────────────
        // Setter contract (per CompressorDSP.h):
        //   setThreshold(dB), setRatio, setAttack(ms), setRelease(ms),
        //   setMakeup(dB), setKnee(dB), setMix(0..1).
        { EffectType::Compressor, "Vocal Light", [](DSPBase* p){ AS(CompressorDSP)
            d->setType ((int) CompressorDSP::Type::Modern);
            d->setThreshold (-18.0f);
            d->setRatio (3.0f);
            d->setAttack (8.0f);
            d->setRelease (90.0f);
            d->setMakeup (3.0f);
            d->setKnee (8.0f);
            d->setMix (1.0f);
        }},
        { EffectType::Compressor, "Drum Bus", [](DSPBase* p){ AS(CompressorDSP)
            d->setType ((int) CompressorDSP::Type::FET);
            d->setThreshold (-12.0f);
            d->setRatio (4.0f);
            d->setAttack (3.0f);
            d->setRelease (50.0f);
            d->setMakeup (2.0f);
            d->setKnee (4.0f);
            d->setMix (1.0f);
        }},
        { EffectType::Compressor, "Mix Glue", [](DSPBase* p){ AS(CompressorDSP)
            d->setType ((int) CompressorDSP::Type::Opto);
            d->setThreshold (-10.0f);
            d->setRatio (2.0f);
            d->setAttack (30.0f);
            d->setRelease (300.0f);
            d->setMakeup (1.5f);
            d->setKnee (10.0f);
            d->setMix (1.0f);
        }},

        // ── Reverb (3) -- Vocal Tame is the H-9 polish preset ──────────
        // Setter contract: setSize, setDecay (RT60 sec), setDamp (0..1
        // bright->dark), setPreDelay (ms), setWet (0..1), setStereoSep (0..200).
        // setProcessingMode(0) pins Stereo -- presets authored before the
        // fresh-instance default flipped to Mid (Task 9).
        { EffectType::Reverb, "70s Plate", [](DSPBase* p){ AS(ReverbDSP)
            d->setProcessingMode (0);
            // Pin the algorithm the NAME promises (Schroeder plate topology);
            // without the pin the preset inherits whatever the slot last ran.
            d->setAlgorithm (0);
            d->setSize (0.55f);
            d->setDecay (1.8f);
            d->setDamp (0.40f);
            d->setPreDelay (15.0f);
            d->setWet (0.35f);
            d->setStereoSep (100.0f);
        }},
        { EffectType::Reverb, "Cathedral", [](DSPBase* p){ AS(ReverbDSP)
            d->setProcessingMode (0);
            d->setAlgorithm (1);   // pin Hall FDN (was slot-inherited)
            d->setSize (0.95f);
            d->setDecay (4.5f);
            d->setDamp (0.55f);
            d->setPreDelay (40.0f);
            d->setWet (0.45f);
            d->setStereoSep (100.0f);
        }},
        { EffectType::Reverb, "Vocal Tame", [](DSPBase* p){ AS(ReverbDSP)
            // H-9 (2026-05-02): Vocal Tame uses the VocalBooth algorithm
            // (algo=4) -- tiny 4-line FDN, heavy HF damp, no modulation.
            // Vocal-safe by design; the small + dark knob values below
            // shape it for "tame the reverb behind a lead vocal" duty.
            d->setProcessingMode (0);
            d->setAlgorithm (4);
            d->setSize (0.40f);
            d->setDecay (1.2f);
            d->setDamp (0.65f);
            d->setPreDelay (20.0f);
            d->setWet (0.22f);
            d->setStereoSep (85.0f);
        }},

        // ── Chorus (3) ──────────────────────────────────────────────────
        // Setter contract: setLFOFreq(idx, hz), setDelay (ms), setDepth (ms),
        // setStereo (deg), setVoices (3 or 6), setWet (0..1).
        { EffectType::Chorus, "Subtle", [](DSPBase* p){ AS(ChorusDSP)
            d->setLFOFreq (0, 0.55f);
            d->setDelay (12.0f);
            d->setDepth (2.0f);
            d->setStereo (90.0f);
            d->setVoices (3);
            d->setWet (0.25f);
        }},
        { EffectType::Chorus, "Wide Stereo", [](DSPBase* p){ AS(ChorusDSP)
            d->setLFOFreq (0, 0.80f);
            d->setDelay (15.0f);
            d->setDepth (5.0f);
            d->setStereo (180.0f);
            d->setVoices (6);
            d->setWet (0.45f);
        }},
        { EffectType::Chorus, "Vintage", [](DSPBase* p){ AS(ChorusDSP)
            d->setLFOFreq (0, 1.20f);
            d->setDelay (20.0f);
            d->setDepth (8.0f);
            d->setStereo (120.0f);
            d->setVoices (3);
            d->setWet (0.55f);
        }},

        // ── Delay (3) ──────────────────────────────────────────────────
        { EffectType::Delay, "Slapback", [](DSPBase* p){ AS(DelayDSP)
            d->presetSlapback();   // already implemented in H-8
        }},
        { EffectType::Delay, "Quarter Note Echo", [](DSPBase* p){ AS(DelayDSP)
            d->setType ((int) DelayDSP::Type::Echo);
            d->setDelayModel (0);   // Stereo
            d->setDelayMs (500.0f);
            d->setFeedbackLevel (0.45f);
            d->setFBDistType (0);   // pin Limit -- preset authored before the fresh-instance default flipped to Sat
            d->setWetOut (0.30f);
            d->setDryOut (1.0f);
            d->setTone (0.20f);
            d->setTempoSync (true);
            d->setSyncNote (1, 4);
        }},
        { EffectType::Delay, "Vocal Doubler Wide", [](DSPBase* p){ AS(DelayDSP)
            d->setType ((int) DelayDSP::Type::VocalDoubler);
            d->setDoubleTimeLMs (12.0f);
            d->setDoubleTimeRMs (24.0f);
            d->setDoubleDetune (60.0f);
            d->setDoubleWidth (95.0f);
            d->setDoubleRate (0.8f);
            d->setWetOut (0.50f);
            d->setDryOut (1.0f);
        }},

        // ── Saturation (3) ─────────────────────────────────────────────
        { EffectType::Saturation, "Warm Tape", [](DSPBase* p){ AS(SaturationDSP)
            d->setSatType ((int) SaturationDSP::Type::Tube);
            d->setTubeType (1);   // Type B (smoother)
            d->setFlowers (3.5f);
            d->setDabs (2.0f);
            d->setBassRelief (40.0f);
            d->setTransformer (true);
            d->setTonePre (1.0f);
            d->setTonePost (-1.0f);
            d->setWet (75.0f);
            d->setOut (0.0f);
            d->setVocalBody (false);
            d->setHarmonicsMode (1);
        }},
        { EffectType::Saturation, "Console Pre", [](DSPBase* p){ AS(SaturationDSP)
            d->setSatType ((int) SaturationDSP::Type::Console);
            d->setFlowers (4.0f);
            d->setDabs (3.0f);
            d->setWet (80.0f);
            d->setOut (0.0f);
            d->setHarmonicsMode (1);
        }},
        { EffectType::Saturation, "Vintage Tube", [](DSPBase* p){ AS(SaturationDSP)
            d->setSatType ((int) SaturationDSP::Type::Tube);
            d->setTubeType (2);   // Type C (most colored)
            d->setFlowers (5.5f);
            d->setDabs (4.5f);
            d->setBassRelief (20.0f);
            d->setTransformer (true);
            d->setSensitivity (3.0f);
            d->setTonePre (2.0f);
            d->setTonePost (1.0f);
            d->setWet (90.0f);
            d->setOut (-2.0f);
            d->setVocalBody (true);
        }},
        // H-10 (2026-05-02): the 3 legacy Tape factory presets join the
        // Saturation factory list -- each sets Type::Tape first then dials
        // in the bit-exact tape values.  Loading any of these from the
        // Saturation slot's preset menu auto-switches the Mode dropdown.
        { EffectType::Saturation, "Tape Vintage", [](DSPBase* p){ AS(SaturationDSP)
            d->setSatType ((int) SaturationDSP::Type::Tape);
            d->setTapeSpeed (0.4f);
            d->setTapeVibe (0.55f);
            d->setTapeWowDepth (0.35f);
            d->setTapeFlutterDepth (0.25f);
            d->setTapeHystAmount (1.2f);
        }},
        { EffectType::Saturation, "Tape Modern", [](DSPBase* p){ AS(SaturationDSP)
            d->setSatType ((int) SaturationDSP::Type::Tape);
            d->setTapeSpeed (0.7f);
            d->setTapeVibe (0.30f);
            d->setTapeWowDepth (0.10f);
            d->setTapeFlutterDepth (0.05f);
            d->setTapeHystAmount (0.8f);
        }},
        { EffectType::Saturation, "Tape Cassette", [](DSPBase* p){ AS(SaturationDSP)
            d->setSatType ((int) SaturationDSP::Type::Tape);
            d->setTapeSpeed (0.25f);
            d->setTapeVibe (0.65f);
            d->setTapeWowDepth (0.65f);
            d->setTapeFlutterDepth (0.55f);
            d->setTapeHystAmount (1.5f);
        }},

        // ── Flanger (3) ────────────────────────────────────────────────
        // Setter contract: setRate (Hz), setDepth (ms), setFeedback (-1..1),
        // setWet (0..1).
        { EffectType::Flanger, "Subtle", [](DSPBase* p){ AS(FlangerDSP)
            d->setRate (0.30f);
            d->setDepth (1.5f);
            d->setFeedback (0.20f);
            d->setWet (0.25f);
        }},
        { EffectType::Flanger, "Jet Plane", [](DSPBase* p){ AS(FlangerDSP)
            d->setRate (0.20f);
            d->setDepth (8.0f);
            d->setFeedback (0.75f);
            d->setWet (0.60f);
        }},
        { EffectType::Flanger, "Vintage Tape", [](DSPBase* p){ AS(FlangerDSP)
            d->setRate (0.45f);
            d->setDepth (4.0f);
            d->setFeedback (0.40f);
            d->setWet (0.45f);
        }},

        // ── Overdrive (3) ──────────────────────────────────────────────
        // Setter contract: setDrive (0..1), setTone (0..1), setWet (0..1).
        { EffectType::Overdrive, "Mild Push", [](DSPBase* p){ AS(OverdriveDSP)
            d->setDrive (0.30f);
            d->setTone (0.50f);
            d->setWet (0.50f);
        }},
        { EffectType::Overdrive, "Crunchy", [](DSPBase* p){ AS(OverdriveDSP)
            d->setDrive (0.60f);
            d->setTone (0.65f);
            d->setWet (0.75f);
        }},
        { EffectType::Overdrive, "Heavy", [](DSPBase* p){ AS(OverdriveDSP)
            d->setDrive (0.85f);
            d->setTone (0.55f);
            d->setWet (0.95f);
        }},

        // ── Phaser (3) ─────────────────────────────────────────────────
        // Setter contract: setRate (Hz, panel range), setDepth (0..1),
        // setFeedback (-1.2..+1.2), setWet (0..1).
        { EffectType::Phaser, "Light Sweep", [](DSPBase* p){ AS(PhaserDSP)
            d->setRate (0.30f);
            d->setDepth (0.40f);
            d->setFeedback (0.20f);
            d->setWet (0.35f);
        }},
        { EffectType::Phaser, "Heavy 4-Stage", [](DSPBase* p){ AS(PhaserDSP)
            d->setRate (0.70f);
            d->setDepth (0.85f);
            d->setFeedback (0.60f);
            d->setWet (0.60f);
        }},
        { EffectType::Phaser, "Vintage", [](DSPBase* p){ AS(PhaserDSP)
            d->setRate (0.45f);
            d->setDepth (0.65f);
            d->setFeedback (0.45f);
            d->setWet (0.50f);
        }},

        // ── TransientShaper (3) ────────────────────────────────────────
        // Setter contract: setAttack (-100..+100 panel range, stored -1..1),
        // setSustain (-1..1 legacy path), setWet (0..1).
        { EffectType::TransientShaper, "Punchier Drums", [](DSPBase* p){ AS(TransientShaperDSP)
            d->setAttack (65.0f);
            d->setSustain (0.20f);
            d->setWet (1.0f);
        }},
        { EffectType::TransientShaper, "Softer Vocals", [](DSPBase* p){ AS(TransientShaperDSP)
            d->setAttack (-40.0f);
            d->setSustain (0.15f);
            d->setWet (1.0f);
        }},
        { EffectType::TransientShaper, "Tighter Bass", [](DSPBase* p){ AS(TransientShaperDSP)
            d->setAttack (25.0f);
            d->setSustain (-0.50f);
            d->setWet (1.0f);
        }},

        // ── Tape (3) ───────────────────────────────────────────────────
        // Setter contract: setTapeSpeed (0..1; 0=slow/squishy, 1=fast/clean),
        // setVibe (0..1 shaper asymmetry), setWowDepth (0..1),
        // setFlutterDepth (0..1), setHystAmount (0..2).  No mix knob -- Tape
        // is an in-place tone shaper.
        // H-10 cutover (2026-05-02): the 3 legacy Tape factory presets now
        // live under EffectType::Saturation alongside Tube/Console presets,
        // matching how Compressor's 3 types share one Compressor/Factory
        // directory.  Each lambda forces Type::Tape then calls setTape* so
        // loading any of these from the Saturation slot's preset menu also
        // switches the Mode dropdown to Tape.

        // ── Limiter (3) ────────────────────────────────────────────────
        // Setter contract: setCeilingDb (output ceiling), setReleaseMs.
        // No mix knob -- a limiter is by definition full-wet.
        { EffectType::Limiter, "Mastering", [](DSPBase* p){ AS(LimiterDSP)
            d->setCeilingDb (-1.0f);
            d->setReleaseMs (50.0f);
        }},
        { EffectType::Limiter, "Brick Wall", [](DSPBase* p){ AS(LimiterDSP)
            d->setCeilingDb (-0.3f);
            d->setReleaseMs (10.0f);   // setter floor is 10 ms; 5 silently clamped
        }},
        { EffectType::Limiter, "Transparent", [](DSPBase* p){ AS(LimiterDSP)
            d->setCeilingDb (-3.0f);
            d->setReleaseMs (200.0f);
        }},

        // ── DeEsser (3) ────────────────────────────────────────────────
        // Setter contract: setThresholdDb, setRangeDb (negative dB max
        // reduction), setFrequencyHz, setQ, setMix (0..1).
        { EffectType::DeEsser, "Vocal Light", [](DSPBase* p){ AS(DeEsserDSP)
            d->setThresholdDb (-22.0f);
            d->setRangeDb (-6.0f);
            d->setFrequencyHz (6500.0f);
            d->setQ (1.4f);
            d->setMix (1.0f);
        }},
        { EffectType::DeEsser, "Vocal Heavy", [](DSPBase* p){ AS(DeEsserDSP)
            d->setThresholdDb (-30.0f);
            d->setRangeDb (-12.0f);
            d->setFrequencyHz (7000.0f);
            d->setQ (1.6f);
            d->setMix (1.0f);
        }},
        { EffectType::DeEsser, "Sibilant Tame", [](DSPBase* p){ AS(DeEsserDSP)
            d->setThresholdDb (-25.0f);
            d->setRangeDb (-8.0f);
            d->setFrequencyHz (8500.0f);
            d->setQ (2.0f);
            d->setMix (1.0f);
        }},
        };
        return table;
    }

    #undef AS
}

void migrateTapeFolderToSaturation()
{
    // Source: legacy Tape/My Presets/  ->  Saturation/My Presets/.
    // Stale Tape/Factory/ files: deleted (those presets are now seeded
    // under Saturation/Factory/ with new names).
    auto tapeRoot       = presetsRoot().getChildFile (typeFolderName (EffectType::Tape));
    if (! tapeRoot.isDirectory()) return;   // nothing to do (clean install)

    // Move user presets.
    auto tapeMyPresets  = tapeRoot.getChildFile ("My Presets");
    auto satMyPresets   = myPresetsDir (EffectType::Saturation);
    if (tapeMyPresets.isDirectory())
    {
        satMyPresets.createDirectory();   // ensure target exists
        juce::Array<juce::File> userXmls;
        tapeMyPresets.findChildFiles (userXmls, juce::File::findFiles, false, "*.xml");
        for (const auto& src : userXmls)
        {
            auto dst = satMyPresets.getChildFile (src.getFileName());
            if (dst.existsAsFile()) continue;   // skip name collision; user keeps both
            src.copyFileTo (dst);
            src.deleteFile();
        }
        // Clean up if My Presets is now empty.
        if (tapeMyPresets.getNumberOfChildFiles (juce::File::findFiles) == 0)
            tapeMyPresets.deleteRecursively();
    }

    // Wipe old Factory directory unconditionally -- those XMLs were the
    // pre-cutover TapeDSP-format files; their replacements live under
    // Saturation/Factory as "Tape Vintage" etc.
    tapeRoot.getChildFile ("Factory").deleteRecursively();

    // Wipe Default.xml under Tape if it exists -- "Save as Default" for
    // EffectType::Tape no longer makes sense (no separate Tape slot UI).
    tapeRoot.getChildFile ("Default.xml").deleteFile();

    // Remove the now-empty Tape root if all subdirs are gone.
    if (tapeRoot.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
        tapeRoot.deleteRecursively();
}

void seedFactoryPresets()
{
    presetsRoot().createDirectory();

    // Versioned seeding: the skip-if-exists guard below froze factory files
    // at their first seed -- code-table or serializer changes never reached
    // an existing install (its factory presets diverged from what the current
    // binary would seed).  Bump kFactorySeedVersion whenever factoryTable()
    // values or any effect's preset serialization changes; a stale stamp
    // rewrites FACTORY files from current code on next launch.  My Presets/
    // is never touched.
    constexpr int kFactorySeedVersion = 2;   // 1 = the frozen 2026-05-02 era
    const auto stampFile = presetsRoot().getChildFile ("factory_seed_version.txt");
    const bool forceReseed =
        stampFile.loadFileAsString().trim().getIntValue() < kFactorySeedVersion;

    // I-15c (2026-05-03): ensure folder tree exists for EVERY pedal type so
    // the user sees a complete `Presets/Effects/Pedals/{TypeName}/` layout
    // even for pedals without factory presets.  Previously folders were
    // created lazily on first ensureFolderTree() call (e.g. via the per-pedal
    // preset menu), which meant brand-new installs only saw folders for the
    // pedals that happened to have factory presets seeded.
    static const EffectType kAllPedals[] = {
        EffectType::BluesDriveStyle, EffectType::DistortionStyle,
        EffectType::FuzzStyle,       EffectType::HighGainStyle,
        EffectType::NoiseGateStyle,  EffectType::TunerStyle,
        EffectType::AcousticPreampStyle, EffectType::AcousticSimulatorStyle,
        EffectType::GraphicEQStyle,  EffectType::SynthStyle,
        EffectType::OctaveStyle,     EffectType::WahStyle,
        EffectType::BassGraphicEQStyle, EffectType::BassCompressorStyle,
        EffectType::BassDriverStyle, EffectType::BassOverdriveStyle,
        EffectType::FurmanEQStyle,   EffectType::NAMPedalStyle,
    };
    for (auto t : kAllPedals)
        ensureFolderTree (t);

    // Also ensure the User NAM Pedals .nam-file folder exists (separate from
    // the User NAM Pedal preset folder above -- this one holds the actual
    // .nam capture files the user loads via the panel's Load button).
    userNamPedalsDir().createDirectory();

    // Build a temp DSP per preset, configure it via the lambda, capture
    // state, write XML to disk if the file is missing.  Idempotent.
    for (const auto& def : factoryTable())
    {
        ensureFolderTree (def.type);
        const auto file = factoryDir (def.type)
                              .getChildFile (juce::String (def.name) + ".xml");
        if (! forceReseed && file.existsAsFile()) continue;   // already seeded

        auto dsp = EffectRack::createEffect (def.type);
        if (! dsp) continue;
        dsp->prepare (44100.0, 512);   // some DSPs need prep before setters work
        if (def.configure) def.configure (dsp.get());

        juce::MemoryBlock blob;
        dsp->getStateInformation (blob);
        if (blob.getSize() == 0) continue;

        juce::String err;
        writePresetXml (file, def.type, def.name, blob, err);
        // Silent on failure -- factory presets are best-effort during boot.
    }

    stampFile.replaceWithText (juce::String (kFactorySeedVersion));
}

}   // namespace EffectPresetIO
