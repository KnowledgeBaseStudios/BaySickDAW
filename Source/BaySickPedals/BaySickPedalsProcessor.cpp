#include "BaySickPedalsProcessor.h"
#include "../AppPaths.h"
#include "BaySickPedalsEditor.h"
#include "../DSP/CompressorDSP.h"   // for I-15: Compressor in pedal slot defaults to Type::CS
#include "../DSP/OverdriveDSP.h"    // for I-15: Overdrive in pedal slot defaults to Type::Pedal

namespace
{
    // Per-slot APVTS bypass param IDs.  IDs are stable -- changing them breaks
    // saved projects.
    juce::String slotBypassId (int slot)
    {
        return "bsp_slot" + juce::String (slot) + "_bypass";
    }

    constexpr const char* kPedalboardRootTag = "Pedalboard";

    // QA-Verify (2026-07-25): the outer wrapper and the APVTS child both used to be
    // named "BaySickPedalsState" -- the same string the APVTS is constructed with
    // (see the ctor's initialiser).  A tag lookup starting ABOVE the pedals root can
    // therefore land on the wrapper instead of the APVTS child.  New saves write the
    // unambiguous V2 tag; the loader still ACCEPTS the legacy tag so every existing
    // project and pedalboard preset keeps loading.  That is load-tolerance, not a
    // migration system (the no-backward-compat-pre-v1 rule stands).
    constexpr const char* kStateRootTag      = "BaySickPedalsRoot";    // written by new saves
    constexpr const char* kStateRootTagLegacy = "BaySickPedalsState";  // accepted on load
    // Write-only by design: stamped into engine state + pedalboard presets so a
    // post-v1 migration can version-gate old files. No reader exists until one does.
    constexpr int         kStateVersion      = 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────
BaySickPedalsProcessor::BaySickPedalsProcessor (juce::UndoManager* undoMgr)
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, undoMgr, "BaySickPedalsState", createLayout())
{
    // Defaults: slot 0 = Tuner, slot 7 = Graphic EQ.  Construct the DSPs into
    // `active` directly (pre-audio-thread, no swap dance needed) so the editor
    // sees real DSPs on first paint -- otherwise tiles would render empty until
    // the audio thread first fires.
    mSlots[kSlotTuner].type   = EffectType::TunerStyle;
    mSlots[kSlotTuner].active = EffectRack::createEffect (EffectType::TunerStyle);
    mSlots[kSlotEQ   ].type   = EffectType::GraphicEQStyle;
    mSlots[kSlotEQ   ].active = EffectRack::createEffect (EffectType::GraphicEQStyle);
    // Slots 1-6 default to None (empty).
}

BaySickPedalsProcessor::~BaySickPedalsProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
BaySickPedalsProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using B = juce::AudioParameterBool;
    using PID = juce::ParameterID;

    for (int s = 0; s < kNumSlots; ++s)
    {
        layout.add (std::make_unique<B> (
            PID (slotBypassId (s), 1),
            "Slot " + juce::String (s) + " Bypass",
            false));
    }
    return layout;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot policy
// ─────────────────────────────────────────────────────────────────────────────
bool BaySickPedalsProcessor::isEffectAllowedInSlot (int slot,
                                                    EffectType type) const noexcept
{
    if (slot < 0 || slot >= kNumSlots) return false;

    if (slot == kSlotTuner)
        return type == EffectType::None || type == EffectType::TunerStyle;

    if (slot == kSlotEQ)
        return type == EffectType::None
            || type == EffectType::GraphicEQStyle
            || type == EffectType::BassGraphicEQStyle
            || type == EffectType::FurmanEQStyle;

    // Slots 1-6: any type EXCEPT the slot-locked types (Tuner / 3 EQs).
    // I-4 (2026-05-02): EffectType::OverdriveStyle was removed -- Overdrive
    // pedal mode is now Type::Pedal on the existing OverdriveDSP, so the
    // slot picker uses EffectType::Overdrive (allowed-by-default below).
    switch (type)
    {
        case EffectType::TunerStyle:
        case EffectType::GraphicEQStyle:
        case EffectType::BassGraphicEQStyle:
        case EffectType::FurmanEQStyle:
            return false;
        default:
            return true;
    }
}

EffectType BaySickPedalsProcessor::getSlotType (int slot) const noexcept
{
    return (slot >= 0 && slot < kNumSlots) ? mSlots[slot].type : EffectType::None;
}

DSPBase* BaySickPedalsProcessor::getSlotEffect (int slot) const noexcept
{
    if (slot < 0 || slot >= kNumSlots) return nullptr;
    const auto& s = mSlots[slot];
    if (s.swapPending.load (std::memory_order_acquire))
        return s.pending.get();
    return s.active.get();
}

bool BaySickPedalsProcessor::isSlotBypassed (int slot) const noexcept
{
    if (slot < 0 || slot >= kNumSlots) return false;
    if (auto* p = apvts.getRawParameterValue (slotBypassId (slot)))
        return p->load() > 0.5f;
    return false;
}

int BaySickPedalsProcessor::getChainLatencySamples()
{
    // Message-thread pull.  Mirror processBlock's slot walk exactly (skip
    // bsp_slot{N}_bypass) so the reported latency == the delay the audio path
    // actually introduces.  No board-level bypass exists on the pedalboard --
    // per-slot bypass is the only bypass, matching this sum.
    const juce::SpinLock::ScopedLockType lk (mSlotsLock);
    int total = 0;
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (isSlotBypassed (i)) continue;
        if (auto* eff = getSlotEffect (i))
            total += juce::jmax (0, eff->getLatencySamples());
    }
    return total;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot mutation -- wait-free swap-pending pattern (mirrors EffectRack).
// ─────────────────────────────────────────────────────────────────────────────
void BaySickPedalsProcessor::publishPending (int slot,
                                             std::unique_ptr<DSPBase> effect,
                                             EffectType type)
{
    // Caller holds mLoadLock.  Drain any pending swap on this slot on the
    // message thread (avoids audio-not-running deadlock during project load).
    if (mSlots[slot].swapPending.load (std::memory_order_acquire))
    {
        const juce::SpinLock::ScopedLockType lkAudio (mSlotsLock);
        if (mSlots[slot].swapPending.load (std::memory_order_acquire))
        {
            std::swap (mSlots[slot].active, mSlots[slot].pending);
            mSlots[slot].swapPending.store (false, std::memory_order_release);
        }
    }

    mSlots[slot].pending = std::move (effect);
    mSlots[slot].type    = type;
    mSlots[slot].swapPending.store (true, std::memory_order_release);
}

bool BaySickPedalsProcessor::loadEffect (int slot, EffectType type,
                                         const juce::String& uuidOverride)
{
    if (slot < 0 || slot >= kNumSlots)         return false;
    if (! isEffectAllowedInSlot (slot, type))  return false;

    auto effect = EffectRack::createEffect (type);

    // I-15 (2026-05-03): Compressor loaded into a pedal slot defaults to
    // Type::CS (CS-Style sustain pedal layout), matching the locked Phase I
    // spec.  In the FX rack the Compressor still defaults to Type::Modern.
    if (effect && type == EffectType::Compressor)
    {
        if (auto* comp = dynamic_cast<CompressorDSP*> (effect.get()))
            comp->setType ((int) CompressorDSP::Type::CS);
    }

    // I-15 (2026-05-03): Overdrive loaded into a pedal slot defaults to
    // Type::Pedal (OD Style pedal panel: HPF split + dual cascaded tanh).
    // FX rack still defaults to Type::Rack (full Overdrive panel).
    if (effect && type == EffectType::Overdrive)
    {
        if (auto* od = dynamic_cast<OverdriveDSP*> (effect.get()))
            od->setType ((int) OverdriveDSP::Type::Pedal);
    }

    if (effect && mSampleRate > 0.0)
        effect->prepare (mSampleRate, mMaxBlock);

    {
        const juce::ScopedLock lk (mLoadLock);
        publishPending (slot, std::move (effect), type);
        // Fresh identity on a user-facing swap (different pedal = different knobs,
        // so its old lanes should not follow); restoreFullState passes the saved
        // one so lanes survive a reload.  Mirrors EffectRack::loadEffect.
        mSlots[(size_t) slot].uuid = uuidOverride.isNotEmpty() ? uuidOverride
                                                               : juce::Uuid().toString();
    }
    fireDirty();   // 2026-05-05 lifecycle dirty
    if (onSlotAutomationChanged) onSlotAutomationChanged();
    return true;
}

bool BaySickPedalsProcessor::clearSlot (int slot)
{
    if (slot < 0 || slot >= kNumSlots) return false;
    // Slot 0 / 7 cannot be cleared -- they always hold their locked type.
    if (isSlotLocked (slot))           return false;

    {
        const juce::ScopedLock lk (mLoadLock);
        publishPending (slot, nullptr, EffectType::None);
        mSlots[(size_t) slot].uuid = {};
    }
    fireDirty();
    if (onSlotAutomationChanged) onSlotAutomationChanged();
    return true;
}

bool BaySickPedalsProcessor::moveSlot (int from, int to)
{
    // Only slots 1..kNumSlots-2 (i.e., 1-6) can move.  Locked slots stay put.
    const int firstFree = kSlotTuner + 1;
    const int lastFree  = kSlotEQ - 1;
    if (from < firstFree || from > lastFree) return false;
    if (to   < firstFree || to   > lastFree) return false;
    if (from == to)                          return true;

    const juce::ScopedLock                 lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType   lkAudio (mSlotsLock);

    auto drain = [] (Slot& s) noexcept
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
    };
    drain (mSlots[from]);
    drain (mSlots[to]);

    // Naive sequential shift: remove `from` and re-insert at `to`.  For
    // adjacent positions this is identical to a swap; for distant moves it
    // walks the slots between them.  All under mSlotsLock so audio sees one
    // coherent layout per block.
    Slot moving = std::move (mSlots[from]);
    if (from < to)
    {
        for (int i = from; i < to; ++i)
            mSlots[i] = std::move (mSlots[i + 1]);
    }
    else
    {
        for (int i = from; i > to; --i)
            mSlots[i] = std::move (mSlots[i - 1]);
    }
    mSlots[to] = std::move (moving);
    fireDirty();   // 2026-05-05 lifecycle dirty
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio thread
// ─────────────────────────────────────────────────────────────────────────────
void BaySickPedalsProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    const juce::ScopedLock                 lkMsg   (mLoadLock);
    const juce::SpinLock::ScopedLockType   lkAudio (mSlotsLock);

    mSampleRate = sampleRate;
    mMaxBlock   = maxBlock;

    for (auto& s : mSlots)
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
        if (s.active) s.active->prepare (sampleRate, maxBlock);
    }
}

void BaySickPedalsProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Try-lock against multi-slot mutations.  Single-slot loads/clears
    // publish via per-slot swapPending and don't take this lock, so audio is
    // wait-free for the common path.
    const juce::SpinLock::ScopedTryLockType tryLk (mSlotsLock);
    if (! tryLk.isLocked()) return;

    // Drain per-slot pending swaps.
    for (auto& s : mSlots)
    {
        if (s.swapPending.load (std::memory_order_acquire))
        {
            std::swap (s.active, s.pending);
            s.swapPending.store (false, std::memory_order_release);
        }
    }

    // Process slots in order: 0 (Tuner) -> 1..6 (user) -> 7 (EQ).
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (isSlotBypassed (i)) continue;
        if (auto* eff = mSlots[(size_t) i].active.get())
            eff->process (buffer);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// I-15 (2026-05-03): real BaySickPedalsEditor (4x2 pedal-rack UI).
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* BaySickPedalsProcessor::createEditor()
{
    return new BaySickPedalsEditor (*this);
}

// ─────────────────────────────────────────────────────────────────────────────
// State capture / restore.  Used by both the project XML path
// (get/setStateInformation) and the standalone Pedalboard preset library
// (savePedalboardPreset / loadPedalboardPreset).
// ─────────────────────────────────────────────────────────────────────────────
juce::ValueTree BaySickPedalsProcessor::captureFullState() const
{
    juce::ValueTree state (kStateRootTag);
    state.setProperty ("version", kStateVersion, nullptr);

    // APVTS (per-slot bypass bools).  state is a live ValueTree that the
    // listeners are bound to -- we deep-copy via createCopy() so the saved
    // snapshot doesn't track future changes (and so this method stays const).
    state.appendChild (apvts.state.createCopy(), nullptr);

    // Per-slot snapshot.
    juce::ValueTree slotsTree ("Slots");
    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto& s  = mSlots[(size_t) i];
        const bool sp  = s.swapPending.load (std::memory_order_acquire);
        DSPBase* eff   = sp ? s.pending.get() : s.active.get();

        juce::ValueTree slotTree ("Slot");
        slotTree.setProperty ("index", i,                 nullptr);
        slotTree.setProperty ("type",  (int) s.type,      nullptr);
        slotTree.setProperty ("uuid",  s.uuid,            nullptr);
        size_t dataBytes = 0;
        if (eff)
        {
            juce::MemoryBlock mb;
            eff->getStateInformation (mb);
            dataBytes = mb.getSize();
            if (dataBytes > 0)
                slotTree.setProperty ("data", mb.toBase64Encoding(), nullptr);
        }
        juce::ignoreUnused (sp, dataBytes);
        slotsTree.appendChild (slotTree, nullptr);
    }
    state.appendChild (slotsTree, nullptr);
    return state;
}

void BaySickPedalsProcessor::restoreFullState (const juce::ValueTree& state)
{
    // Accept the legacy outer tag so pre-2026-07-25 projects + pedalboard presets
    // keep loading; new saves write kStateRootTag.
    if (! state.isValid()
        || ! (state.hasType (kStateRootTag) || state.hasType (kStateRootTagLegacy)))
        return;

    // APVTS first so per-slot bypass bools restore before slots load (DSPs
    // don't depend on the bypass param themselves; this just keeps state
    // consistent before the audio swap).
    // Look the child up by the APVTS's OWN type rather than a hard-coded string:
    // on legacy files that string also names the outer wrapper, and keying off
    // apvts.state.getType() cannot drift if the APVTS type is ever renamed.
    if (auto apvtsState = state.getChildWithName (apvts.state.getType());
        apvtsState.isValid())
    {
        apvts.replaceStateKeepingUndoHistory (apvtsState);
    }

    auto slotsTree = state.getChildWithName ("Slots");
    if (! slotsTree.isValid()) return;

    for (int i = 0; i < slotsTree.getNumChildren(); ++i)
    {
        auto slotTree = slotsTree.getChild (i);
        if (! slotTree.hasType ("Slot")) continue;

        const int slotIdx = (int) slotTree.getProperty ("index", -1);
        if (slotIdx < 0 || slotIdx >= kNumSlots) continue;

        const auto type = (EffectType) (int) slotTree.getProperty ("type", 0);
        const juce::String b64 = slotTree.getProperty ("data", "").toString();

        // Build the new DSP off-thread (createEffect, prepare, restore state)
        // BEFORE parking into pending.  Mirrors EffectRack::setStateInformation
        // pattern from I-0a -- avoids the swap-pending vs blob-write race.
        std::unique_ptr<DSPBase> effect;
        if (type != EffectType::None)
        {
            effect = EffectRack::createEffect (type);
            if (effect)
            {
                if (mSampleRate > 0.0)
                    effect->prepare (mSampleRate, mMaxBlock);
                if (b64.isNotEmpty())
                {
                    // 2026-05-05 fix: save side uses MemoryBlock::toBase64Encoding
                    // which prefixes the string with `<byteCount>.<base64>`.
                    // Decoding via juce::Base64::convertFromBase64 (raw-base64
                    // only) silently produced 0 bytes on the prefix, so every
                    // slot's DSP was recreated with defaults.  fromBase64Encoding
                    // is the symmetric inverse and handles the prefix.
                    juce::MemoryBlock decoded;
                    const bool decodedOk = decoded.fromBase64Encoding (b64);
                    if (decodedOk && decoded.getSize() > 0)
                        effect->setStateInformation (decoded.getData(),
                                                      (int) decoded.getSize());
                }
            }
        }

        // Bulk-restore path: install directly into active under mSlotsLock
        // (audio try-locks and skips the block).  Same approach as
        // EffectRack::setStateInformation -- avoids the 1s drain wait when
        // audio isn't yet producing blocks during project load.
        const juce::ScopedLock                 lkMsg   (mLoadLock);
        const juce::SpinLock::ScopedLockType   lkAudio (mSlotsLock);

        if (mSlots[slotIdx].swapPending.load (std::memory_order_acquire))
        {
            std::swap (mSlots[slotIdx].active, mSlots[slotIdx].pending);
            mSlots[slotIdx].swapPending.store (false, std::memory_order_release);
        }
        mSlots[slotIdx].active = std::move (effect);
        mSlots[slotIdx].pending.reset();
        mSlots[slotIdx].type   = type;
        // Restore the saved automation identity so existing lanes keep pointing at
        // this pedal.  Pre-uuid saves carry none: mint one rather than leaving it
        // empty, or the slot's knobs would register under a blank key.
        {
            const juce::String savedUuid = slotTree.getProperty ("uuid", "").toString();
            mSlots[slotIdx].uuid = type == EffectType::None
                                     ? juce::String()
                                     : (savedUuid.isNotEmpty() ? savedUuid
                                                               : juce::Uuid().toString());
        }
    }

    // 2026-05-05 (Bug B fix): bulk-restore swaps DSP pointers on every slot.
    // The editor's timer-based rebuild only fires when the type enum changes,
    // so when the user saves + reloads the same pedal layout (types match,
    // pointers differ), the editor's cached widget bindings stay attached to
    // the destroyed DSP and the user sees stale parameter mappings (EQ
    // frequency where boost should be, etc.).  Fire the external-change
    // callback so the editor rebuilds every tile unconditionally.  Posted
    // async so the audio thread (if it pumped this load via project-load)
    // doesn't run editor mutation on its own thread.
    if (onSlotsExternallyChanged)
        juce::MessageManager::callAsync (
            [cb = onSlotsExternallyChanged] { if (cb) cb(); });

    // Every slot's (uuid, type) pair was just rewritten, so the board's lanes
    // need re-keying.  Marshalled for the same reason as the callback above:
    // a project load can pump this from a non-message thread, and registration
    // touches the editor-side registry.
    if (onSlotAutomationChanged)
        juce::MessageManager::callAsync (
            [cb = onSlotAutomationChanged] { if (cb) cb(); });
}

void BaySickPedalsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = captureFullState();
    auto xml = state.createXml();
    if (xml) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
}

void BaySickPedalsProcessor::setStateInformation (const void* data, int sz)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sz);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    restoreFullState (state);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pedalboard preset library
// ─────────────────────────────────────────────────────────────────────────────
juce::File BaySickPedalsProcessor::pedalboardPresetsRoot()
{
    return AppPaths::appRoot()
              .getChildFile ("Presets")
              .getChildFile ("Pedalboards");
}

bool BaySickPedalsProcessor::savePedalboardPreset (const juce::String& name,
                                                   juce::String& outErr)
{
    juce::String safeName = name.trim();
    if (safeName.isEmpty()) { outErr = "Preset name cannot be empty."; return false; }
    for (auto c : juce::String ("\\/:*?\"<>|"))
        safeName = safeName.replace (juce::String::charToString (c), "_");

    auto dir = pedalboardPresetsRoot();
    if (! dir.createDirectory())
    {
        outErr = "Could not create folder: " + dir.getFullPathName();
        return false;
    }
    auto target = dir.getChildFile (safeName + ".xml");

    juce::XmlElement root (kPedalboardRootTag);
    root.setAttribute ("version", kStateVersion);
    root.setAttribute ("name",    safeName);

    auto state = captureFullState();
    if (auto inner = state.createXml())
        root.addChildElement (inner.release());

    if (! root.writeTo (target, juce::XmlElement::TextFormat()))
    {
        outErr = "Could not write preset file: " + target.getFullPathName();
        return false;
    }
    return true;
}

bool BaySickPedalsProcessor::loadPedalboardPreset (const juce::File& xml,
                                                   juce::String& outErr)
{
    if (! xml.existsAsFile())
    {
        outErr = "Preset file does not exist: " + xml.getFullPathName();
        return false;
    }
    auto root = juce::parseXML (xml);
    if (! root || ! root->hasTagName (kPedalboardRootTag))
    {
        outErr = "Preset file is not a Pedalboard preset.";
        return false;
    }
    // New presets carry the V2 payload tag; pre-2026-07-25 files carry the legacy
    // one.  Accept either so existing pedalboard presets keep loading.
    auto* inner = root->getChildByName (kStateRootTag);
    if (inner == nullptr)
        inner = root->getChildByName (kStateRootTagLegacy);
    if (! inner)
    {
        outErr = "Preset file is missing the state payload.";
        return false;
    }
    auto state = juce::ValueTree::fromXml (*inner);
    if (! state.isValid())
    {
        outErr = "Preset file's state payload is invalid.";
        return false;
    }
    restoreFullState (state);
    return true;
}

juce::Array<juce::File> BaySickPedalsProcessor::enumeratePedalboardPresets()
{
    juce::Array<juce::File> out;
    auto dir = pedalboardPresetsRoot();
    if (dir.isDirectory())
    {
        dir.findChildFiles (out, juce::File::findFiles, false, "*.xml");
        out.sort();
    }
    return out;
}
