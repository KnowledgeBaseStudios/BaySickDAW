#include "MidiLearnRegistry.h"

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
        mLearnCaptureFn = nullptr;
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
    // tolerate being held off too long.  In practice typical mapping count is
    // tiny (<10 in flight) so the snapshot copy is cheap.
    std::vector<std::pair<juce::String, MessageType>> hits;
    {
        const juce::SpinLock::ScopedTryLockType tryLk (mLock);
        if (! tryLk.isLocked()) return 0;
        for (const auto& kv : mMappings)
        {
            if (eventMatchesMapping (kv.second, sourceDeviceName, msg))
                hits.emplace_back (kv.first, kv.second.msgType);
        }
    }

    int fired = 0;
    for (const auto& hit : hits)
    {
        const float norm = extractNormalisedValue (hit.second, msg);
        if (norm < 0.0f) continue;

        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (hit.first)))
        {
            p->setValueNotifyingHost (norm);
            ++fired;
        }
    }
    return fired;
}

// ─────────────────────────────────────────────────────────────────────────────
// Learn-mode capture
// ─────────────────────────────────────────────────────────────────────────────
void MidiLearnRegistry::beginLearn (const juce::String& paramId, LearnCaptureFn fn)
{
    {
        const juce::SpinLock::ScopedLockType lk (mLock);
        mLearnTargetParamId = paramId;
        mLearnCaptureFn     = std::move (fn);
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
        mLearnCaptureFn = nullptr;
    }
    if (wasLearning && onChanged) onChanged();
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
    juce::String       targetId;
    LearnCaptureFn     fn;
    {
        const juce::SpinLock::ScopedTryLockType tryLk (mLock);
        if (! tryLk.isLocked()) return false;
        if (mLearnTargetParamId.isEmpty()) return false;
        targetId = mLearnTargetParamId;
        fn       = mLearnCaptureFn;
    }

    Mapping captured;
    captured.paramId = targetId;
    if (! buildMappingFromEvent (captured, sourceDeviceName, msg))
        return false;   // not a learnable event type; ignore + fall through to dispatch

    bool committed = false;
    if (fn) committed = fn (captured);

    if (committed)
    {
        // Commit the mapping and clear learn state.
        {
            const juce::SpinLock::ScopedLockType lk (mLock);
            mMappings[targetId]   = captured;
            mLearnTargetParamId   = {};
            mLearnCaptureFn       = nullptr;
        }
        if (onChanged) onChanged();
    }
    return true;   // captured (whether or not committed) -- suppress regular dispatch
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
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("BaySickDAW")
              .getChildFile ("MidiMappings.xml");
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
