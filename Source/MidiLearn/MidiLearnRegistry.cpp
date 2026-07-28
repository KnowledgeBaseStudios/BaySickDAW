#include "MidiLearnRegistry.h"
#include "../AppPaths.h"

namespace
{
    constexpr const char* kMappingTag      = "Mapping";
    constexpr const char* kAttrParamId     = "paramId";
    constexpr const char* kAttrMsgType     = "msgType";
    constexpr const char* kAttrCcNumber    = "cc";
    constexpr const char* kAttrChannel     = "ch";        // 0 = Omni, 1..16
    constexpr const char* kAttrDeviceName  = "device";    // empty = any
    constexpr const char* kAttrFormula     = "formula";   // empty = linear

    juce::String msgTypeToString (MidiLearnRegistry::MessageType t)
    {
        switch (t)
        {
            case MidiLearnRegistry::MessageType::Cc:              return "cc";
            case MidiLearnRegistry::MessageType::PitchBend:       return "pitchbend";
            case MidiLearnRegistry::MessageType::ChannelPressure: return "aftertouch";
        }
        return "cc";
    }

    MidiLearnRegistry::MessageType msgTypeFromString (const juce::String& s)
    {
        if (s == "pitchbend")  return MidiLearnRegistry::MessageType::PitchBend;
        if (s == "aftertouch") return MidiLearnRegistry::MessageType::ChannelPressure;
        return MidiLearnRegistry::MessageType::Cc;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mutation
// ─────────────────────────────────────────────────────────────────────────────
void MidiLearnRegistry::setMapping (const juce::String& paramId, const Mapping& m)
{
    if (paramId.isEmpty()) return;
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        Mapping copy = m;
        copy.paramId = paramId;
        mMappings[paramId] = std::move (copy);
    }
    if (onChanged) onChanged();
}

void MidiLearnRegistry::removeMapping (const juce::String& paramId)
{
    bool fired = false;
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        fired = (mMappings.erase (paramId) > 0);
    }
    if (fired && onChanged) onChanged();
}

void MidiLearnRegistry::clear()
{
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        mMappings.clear();
        mLearnTargetParamId = {};
        mCaptureReady.store (false, std::memory_order_relaxed);
    }
    if (onChanged) onChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Read
// ─────────────────────────────────────────────────────────────────────────────
bool MidiLearnRegistry::getMapping (const juce::String& paramId, Mapping& out) const
{
    const juce::SpinLock::ScopedLockType lk (mLock);
    auto it = mMappings.find (paramId);
    if (it == mMappings.end()) return false;
    out = it->second;
    return true;
}

bool MidiLearnRegistry::hasMapping (const juce::String& paramId) const
{
    const juce::SpinLock::ScopedLockType lk (mLock);
    return mMappings.find (paramId) != mMappings.end();
}

juce::Array<juce::String> MidiLearnRegistry::getAllParamIds() const
{
    juce::Array<juce::String> out;
    const juce::SpinLock::ScopedLockType lk (mLock);
    out.ensureStorageAllocated ((int) mMappings.size());
    for (const auto& kv : mMappings)
        out.add (kv.first);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio-thread dispatch
// ─────────────────────────────────────────────────────────────────────────────
float MidiLearnRegistry::extractNormalisedValue (MessageType expected,
                                                  const juce::MidiMessage& msg)
{
    switch (expected)
    {
        case MessageType::Cc:
            if (! msg.isController()) return -1.0f;
            return juce::jlimit (0.0f, 1.0f, (float) msg.getControllerValue() / 127.0f);

        case MessageType::PitchBend:
            if (! msg.isPitchWheel()) return -1.0f;
            // 14-bit value 0..16383; centre is 8192.
            return juce::jlimit (0.0f, 1.0f, (float) msg.getPitchWheelValue() / 16383.0f);

        case MessageType::ChannelPressure:
            if (! msg.isChannelPressure()) return -1.0f;
            return juce::jlimit (0.0f, 1.0f, (float) msg.getChannelPressureValue() / 127.0f);
    }
    return -1.0f;
}

bool MidiLearnRegistry::eventMatchesMapping (const Mapping& m,
                                              const juce::String& sourceDeviceName,
                                              const juce::MidiMessage& msg)
{
    // Device check first (cheapest non-match): empty mapping device = any.
    if (m.deviceName.isNotEmpty() && m.deviceName != sourceDeviceName)
        return false;

    // Channel check: 0 = Omni, otherwise must match exactly.  MIDI channels
    // are 1-16 in the user-facing world; juce::MidiMessage::getChannel returns
    // 1-16 for channel-voice messages, 0 for sysex/other.
    if (m.channel != 0)
    {
        const int evChan = msg.getChannel();
        if (evChan == 0 || evChan != m.channel) return false;
    }

    // Message-type-specific checks.
    switch (m.msgType)
    {
        case MessageType::Cc:
            if (! msg.isController()) return false;
            if (msg.getControllerNumber() != m.ccNumber) return false;
            return true;

        case MessageType::PitchBend:
            return msg.isPitchWheel();

        case MessageType::ChannelPressure:
            return msg.isChannelPressure();
    }
    return false;
}

int MidiLearnRegistry::dispatchEvent (juce::AudioProcessorValueTreeState& apvts,
                                       const juce::String& sourceDeviceName,
                                       const juce::MidiMessage& msg)
{
    // Snapshot the matching mappings under lock, then release before calling
    // setValueNotifyingHost -- the host's listener notification path may not
    // tolerate being held off too long.
    //
    // 2026-07-19: this snapshot used to be a std::vector of {juce::String,
    // MessageType}, i.e. a heap allocation plus a string copy per dispatched
    // event -- in the render callback, continuously, for the whole duration of
    // a knob sweep.  It is now a fixed stack array holding already-resolved
    // parameter POINTERS.  Resolving inside the lock is what makes releasing it
    // safe: APVTS parameters outlive the app, whereas a pointer into the map's
    // keys would dangle if the message thread erased that mapping.
    struct Hit { juce::RangedAudioParameter* param; MessageType type; };
    // One event firing more than this many mappings means the user bound 17+
    // params to a single CC; the tail is dropped rather than allocated for.
    static constexpr int kMaxHitsPerEvent = 16;
    Hit hits[kMaxHitsPerEvent];
    int numHits = 0;

    {
        const juce::SpinLock::ScopedTryLockType tryLk (mLock);
        if (! tryLk.isLocked()) return 0;
        for (const auto& kv : mMappings)
        {
            if (numHits >= kMaxHitsPerEvent) break;
            if (! eventMatchesMapping (kv.second, sourceDeviceName, msg)) continue;
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (kv.first)))
                hits[numHits++] = { p, kv.second.msgType };
        }
    }

    int fired = 0;
    for (int i = 0; i < numHits; ++i)
    {
        const float norm = extractNormalisedValue (hits[i].type, msg);
        if (norm < 0.0f) continue;
        hits[i].param->setValueNotifyingHost (norm);
        ++fired;
    }
    return fired;
}

// ─────────────────────────────────────────────────────────────────────────────
// Learn-mode capture
// ─────────────────────────────────────────────────────────────────────────────
void MidiLearnRegistry::beginLearn (const juce::String& paramId)
{
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        mLearnTargetParamId = paramId;
        // Drop any stale capture on THIS thread, so the audio thread only ever
        // assigns into empty strings (see takeCapturedMapping).
        mPendingCapture = {};
        mCaptureReady.store (false, std::memory_order_relaxed);
    }
    if (onChanged) onChanged();
}

void MidiLearnRegistry::cancelLearn()
{
    bool wasLearning = false;
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        wasLearning = mLearnTargetParamId.isNotEmpty();
        mLearnTargetParamId = {};
        mCaptureReady.store (false, std::memory_order_relaxed);
    }
    if (wasLearning && onChanged) onChanged();
}

bool MidiLearnRegistry::takeCapturedMapping (Mapping& out)
{
    if (! mCaptureReady.load (std::memory_order_acquire)) return false;
    const juce::SpinLock::ScopedLockType lk (mLock);
    if (! mCaptureReady.exchange (false, std::memory_order_acq_rel)) return false;
    out = mPendingCapture;
    // Release our reference HERE, on the message thread.  If this is left for
    // the audio thread's next assignment to overwrite, and the mapping this
    // paramId belonged to has since been removed, that overwrite drops the last
    // reference and frees in the render callback -- the same defect one level
    // down.  `out` holds a reference at this point, so this cannot free either.
    mPendingCapture = {};
    return out.paramId.isNotEmpty();
}

bool MidiLearnRegistry::isLearning() const noexcept
{
    const juce::SpinLock::ScopedLockType lk (mLock);
    return mLearnTargetParamId.isNotEmpty();
}

juce::String MidiLearnRegistry::getLearnTargetParamId() const
{
    const juce::SpinLock::ScopedLockType lk (mLock);
    return mLearnTargetParamId;
}

bool MidiLearnRegistry::buildMappingFromEvent (Mapping& outM,
                                                const juce::String& sourceDeviceName,
                                                const juce::MidiMessage& msg)
{
    if (msg.isController())
    {
        outM.msgType    = MessageType::Cc;
        outM.ccNumber   = msg.getControllerNumber();
        outM.channel    = msg.getChannel();
        outM.deviceName = sourceDeviceName;
        return true;
    }
    if (msg.isPitchWheel())
    {
        outM.msgType    = MessageType::PitchBend;
        outM.ccNumber   = 0;
        outM.channel    = msg.getChannel();
        outM.deviceName = sourceDeviceName;
        return true;
    }
    if (msg.isChannelPressure())
    {
        outM.msgType    = MessageType::ChannelPressure;
        outM.ccNumber   = 0;
        outM.channel    = msg.getChannel();
        outM.deviceName = sourceDeviceName;
        return true;
    }
    return false;
}

bool MidiLearnRegistry::tryCaptureLearn (const juce::String& sourceDeviceName,
                                          const juce::MidiMessage& msg)
{
    // Audio thread.  Everything below stays inside the existing try-lock and
    // touches only pre-constructed storage: no allocation, no map insert, no
    // std::function, no onChanged() notify.  The message thread does all of
    // that when it drains via takeCapturedMapping().
    const juce::SpinLock::ScopedTryLockType tryLk (mLock);
    if (! tryLk.isLocked()) return false;
    if (mLearnTargetParamId.isEmpty()) return false;

    // Assigning into mPendingCapture only moves refcounts: paramId's source is
    // mLearnTargetParamId (still alive at this point) and deviceName's is the
    // event queue's interned name table, which outlives the queue's events.
    // Neither assignment can drop a last reference, so neither can free.
    if (! buildMappingFromEvent (mPendingCapture, sourceDeviceName, msg))
        return false;   // not a learnable event type; fall through to dispatch

    mPendingCapture.paramId = mLearnTargetParamId;
    mLearnTargetParamId     = {};
    mCaptureReady.store (true, std::memory_order_release);
    return true;   // captured -- suppress regular dispatch
}

// ─────────────────────────────────────────────────────────────────────────────
// Persistence
// ─────────────────────────────────────────────────────────────────────────────
juce::ValueTree MidiLearnRegistry::saveToValueTree() const
{
    juce::ValueTree root (kRootTag);
    const juce::SpinLock::ScopedLockType lk (mLock);
    for (const auto& kv : mMappings)
    {
        const auto& m = kv.second;
        juce::ValueTree node (kMappingTag);
        node.setProperty (kAttrParamId,    m.paramId,                          nullptr);
        node.setProperty (kAttrMsgType,    msgTypeToString (m.msgType),        nullptr);
        node.setProperty (kAttrCcNumber,   m.ccNumber,                         nullptr);
        node.setProperty (kAttrChannel,    m.channel,                          nullptr);
        node.setProperty (kAttrDeviceName, m.deviceName,                       nullptr);
        node.setProperty (kAttrFormula,    m.formula,                          nullptr);
        root.appendChild (node, nullptr);
    }
    return root;
}

void MidiLearnRegistry::loadFromValueTree (const juce::ValueTree& tree)
{
    if (! tree.isValid() || ! tree.hasType (kRootTag)) return;

    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        mMappings.clear();
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto node = tree.getChild (i);
            if (! node.hasType (kMappingTag)) continue;
            Mapping m;
            m.paramId    = node.getProperty (kAttrParamId,    "").toString();
            m.msgType    = msgTypeFromString (node.getProperty (kAttrMsgType, "cc").toString());
            m.ccNumber   = (int) node.getProperty (kAttrCcNumber,   0);
            m.channel    = (int) node.getProperty (kAttrChannel,    0);
            m.deviceName = node.getProperty (kAttrDeviceName, "").toString();
            m.formula    = node.getProperty (kAttrFormula,    "").toString();
            if (m.paramId.isNotEmpty())
                mMappings[m.paramId] = std::move (m);
        }
    }
    if (onChanged) onChanged();
}

juce::File MidiLearnRegistry::globalDefaultsFile()
{
    return AppPaths::appRoot().getChildFile ("MidiMappings.xml");
}

bool MidiLearnRegistry::saveAsGlobalDefaults() const
{
    const auto file = globalDefaultsFile();
    file.getParentDirectory().createDirectory();
    auto vt = saveToValueTree();
    auto xml = vt.createXml();
    if (! xml) return false;
    return xml->writeTo (file, juce::XmlElement::TextFormat());
}

bool MidiLearnRegistry::loadGlobalDefaults()
{
    const auto file = globalDefaultsFile();
    if (! file.existsAsFile()) return false;
    auto xml = juce::parseXML (file);
    if (! xml) return false;
    auto vt = juce::ValueTree::fromXml (*xml);
    if (! vt.isValid()) return false;
    loadFromValueTree (vt);
    return true;
}
