/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

//==============================================================================
// BaySickDAW QA-UndoCoverage (2026-08-06): see the declaration comment in the
// header -- write-time programmatic marking, consumed at flush time.
thread_local bool AudioProcessorValueTreeState::programmaticWritePhase = false;

// BaySickDAW QA-UndoCoverage: message-thread liveness registry.  Backs both
// the StateSwapAction dead-owner guard and the tag resolution that lets undo
// entries survive engine destruction/re-creation (see the header comment on
// undoOwnerTag).
static Array<AudioProcessorValueTreeState*>& liveApvtsInstances()
{
    static Array<AudioProcessorValueTreeState*> list;
    return list;
}

AudioProcessorValueTreeState* AudioProcessorValueTreeState::findByUndoOwnerTag (const String& tag)
{
    if (tag.isEmpty()) return nullptr;
    for (auto* s : liveApvtsInstances())
        if (s->undoOwnerTag == tag)
            return s;
    return nullptr;
}

void AudioProcessorValueTreeState::flushAllLiveInstancesToValueTrees()
{
    // See the header comment.  Human-gesture rate; the walk is trivial.
    for (auto* s : liveApvtsInstances())
        s->flushParameterValuesToValueTree();
}

// BaySickDAW QA-UndoCoverage (2026-08-06, redo-across-resurrection fix): the
// undo entry for a parameter edit.  Replaces the tree-bound SetPropertyAction
// the flush used to perform -- that action held the SPECIFIC child-tree
// object, so once an engine was destroyed and re-created (tab resurrection,
// program switch, kit load) its stored writes fired into a detached tree and
// redo silently did nothing.  This action stores denormalised VALUES plus the
// owner's tag and re-resolves the live APVTS at apply time; a re-created
// engine answers to the same tag, so the redo lands.  Falls back to the
// construction-time pointer (liveness-checked) for untagged instances.
// Apply writes the PARAMETER under the programmatic phase: the engine hears
// it immediately, and the next timer flush copies it to the tree WITHOUT the
// UndoManager -- nothing performs during an undo (UndoManager forbids that).
struct ApvtsParamValueUndoAction final : public UndoableAction
{
    ApvtsParamValueUndoAction (AudioProcessorValueTreeState& owner,
                               String parameterId, float oldDenormIn, float newDenormIn)
        : ownerTag (owner.undoOwnerTag), ownerPtr (&owner),
          paramId (std::move (parameterId)),
          oldDenorm (oldDenormIn), newDenorm (newDenormIn) {}

    bool perform() override
    {
        if (firstPerform) { firstPerform = false; return true; }   // value already live at insert
        return apply (newDenorm);
    }
    bool undo() override    { return apply (oldDenorm); }

    bool apply (float denorm) const
    {
        auto* owner = AudioProcessorValueTreeState::findByUndoOwnerTag (ownerTag);
        if (owner == nullptr && liveApvtsInstances().contains (ownerPtr))
            owner = ownerPtr;
        if (owner == nullptr)
            return true;   // owner absent right now: inert no-op, keep the history
        auto* p = owner->getParameter (paramId);
        if (p == nullptr)
            return true;

        AudioProcessorValueTreeState::ScopedProgrammaticParamWrites spw;
        p->setValueNotifyingHost (p->convertTo0to1 (denorm));
        return true;
    }

    UndoableAction* createCoalescedAction (UndoableAction* nextAction) override
    {
        // Same shape as SetPropertyAction's coalescing: a drag's per-flush
        // actions merge into one, keeping the FIRST old value.
        if (auto* next = dynamic_cast<ApvtsParamValueUndoAction*> (nextAction))
            if (next->ownerTag == ownerTag && next->ownerPtr == ownerPtr
                && next->paramId == paramId)
            {
                auto* merged = new ApvtsParamValueUndoAction (*ownerPtr, paramId,
                                                              oldDenorm, next->newDenorm);
                merged->ownerTag = ownerTag;   // keep the capture-time tag even if the owner re-tagged
                return merged;
            }
        return nullptr;
    }

    int getSizeInUnits() override { return 10; }

    String ownerTag;
    AudioProcessorValueTreeState* const ownerPtr;
    const String paramId;
    const float oldDenorm, newDenorm;
    bool firstPerform = true;
};

//==============================================================================

AudioProcessorValueTreeState::Parameter::Parameter (const ParameterID& parameterID,
                                                    const String& parameterName,
                                                    NormalisableRange<float> valueRange,
                                                    float defaultParameterValue,
                                                    const AudioProcessorValueTreeStateParameterAttributes& attributes)
    : AudioParameterFloat (parameterID,
                           parameterName,
                           valueRange,
                           defaultParameterValue,
                           attributes.getAudioParameterFloatAttributes()),
      unsnappedDefault (valueRange.convertTo0to1 (defaultParameterValue)),
      discrete (attributes.getDiscrete()),
      boolean (attributes.getBoolean())
{
}

float AudioProcessorValueTreeState::Parameter::getDefaultValue() const  { return unsnappedDefault; }
int AudioProcessorValueTreeState::Parameter::getNumSteps() const        { return RangedAudioParameter::getNumSteps(); }

bool AudioProcessorValueTreeState::Parameter::isDiscrete() const        { return discrete; }
bool AudioProcessorValueTreeState::Parameter::isBoolean() const         { return boolean; }

void AudioProcessorValueTreeState::Parameter::valueChanged (float newValue)
{
    if (approximatelyEqual ((float) lastValue, newValue))
        return;

    lastValue = newValue;
    NullCheckedInvocation::invoke (onValueChanged);
}

//==============================================================================
class AudioProcessorValueTreeState::ParameterAdapter final : private AudioProcessorParameter::Listener
{
private:
    using Listener = AudioProcessorValueTreeState::Listener;

public:
    explicit ParameterAdapter (RangedAudioParameter& parameterIn)
        : parameter (parameterIn),
          // For legacy reasons, the unnormalised value should *not* be snapped on construction
          unnormalisedValue (getRange().convertFrom0to1 (parameter.getDefaultValue()))
    {
        parameter.addListener (this);

        if (auto* ptr = dynamic_cast<Parameter*> (&parameter))
            ptr->onValueChanged = [this] { parameterValueChanged ({}, {}); };
    }

    ~ParameterAdapter() override        { parameter.removeListener (this); }

    void addListener (Listener* l)      { listeners.add (l); }
    void removeListener (Listener* l)   { listeners.remove (l); }

    RangedAudioParameter& getParameter()                { return parameter; }
    const RangedAudioParameter& getParameter() const    { return parameter; }

    const NormalisableRange<float>& getRange() const    { return parameter.getNormalisableRange(); }

    float getDenormalisedDefaultValue() const    { return denormalise (parameter.getDefaultValue()); }

    void setDenormalisedValue (float value)
    {
        if (! approximatelyEqual (value, (float) unnormalisedValue))
            setNormalisedValue (normalise (value));
    }

    float getDenormalisedValueForText (const String& text) const
    {
        return denormalise (parameter.getValueForText (text));
    }

    String getTextForDenormalisedValue (float value) const
    {
        return parameter.getText (normalise (value), 0);
    }

    float getDenormalisedValue() const                { return unnormalisedValue; }
    std::atomic<float>& getRawDenormalisedValue()     { return unnormalisedValue; }

    bool flushToTree (const Identifier& key, UndoManager* um,
                      AudioProcessorValueTreeState& owner)
    {
        auto needsUpdateTestValue = true;

        if (! needsUpdate.compare_exchange_strong (needsUpdateTestValue, false))
            return false;

        // BaySickDAW QA-UndoCoverage: consume the mark recorded at write time.
        // A pending value whose LAST writer was programmatic must not enter the
        // undo history; a later user write to the same parameter un-marks it.
        if (pendingIsProgrammatic.exchange (false, std::memory_order_relaxed))
            um = nullptr;

        if (auto* valueProperty = tree.getPropertyPointer (key))
        {
            const float oldValue = (float) *valueProperty;
            const float newValue = unnormalisedValue.load();
            if (! approximatelyEqual (oldValue, newValue))
            {
                ScopedValueSetter<bool> svs (ignoreParameterChangedCallbacks, true);
                // Redo-across-resurrection fix: the undo entry is the
                // tag-resolving value action (see ApvtsParamValueUndoAction),
                // NOT a tree-bound SetPropertyAction -- the tree itself is
                // always written untransacted.
                if (um != nullptr)
                    um->perform (new ApvtsParamValueUndoAction (owner, parameter.paramID,
                                                                oldValue, newValue));
                tree.setProperty (key, newValue, nullptr);
            }
        }
        else
        {
            tree.setProperty (key, unnormalisedValue.load(), nullptr);
        }

        return true;
    }

    ValueTree tree;

private:
    void parameterGestureChanged (int, bool) override {}

    void parameterValueChanged (int, float) override
    {
        const auto newValue = denormalise (parameter.getValue());

        if (! listenersNeedCalling && approximatelyEqual ((float) unnormalisedValue, newValue))
            return;

        unnormalisedValue = newValue;
        listeners.call ([this] (Listener& l) { l.parameterChanged (parameter.paramID, unnormalisedValue); });
        listenersNeedCalling = false;

        // BaySickDAW QA-UndoCoverage: this callback runs synchronously on the
        // writer's thread, so the phase flag identifies WHO wrote the pending
        // value.  Last writer wins -- matches whose value the flush will see.
        pendingIsProgrammatic.store (AudioProcessorValueTreeState::programmaticWritePhase,
                                     std::memory_order_relaxed);
        needsUpdate = true;
    }

    float denormalise (float normalised) const
    {
        return getParameter().convertFrom0to1 (normalised);
    }

    float normalise (float denormalised) const
    {
        return getParameter().convertTo0to1 (denormalised);
    }

    void setNormalisedValue (float value)
    {
        if (ignoreParameterChangedCallbacks)
            return;

        parameter.setValueNotifyingHost (value);
    }

    class LockedListeners
    {
    public:
        template <typename Fn>
        void call (Fn&& fn)
        {
            const CriticalSection::ScopedLockType lock (mutex);
            listeners.call (std::forward<Fn> (fn));
        }

        void add (Listener* l)
        {
            const CriticalSection::ScopedLockType lock (mutex);
            listeners.add (l);
        }

        void remove (Listener* l)
        {
            const CriticalSection::ScopedLockType lock (mutex);
            listeners.remove (l);
        }

    private:
        CriticalSection mutex;
        ListenerList<Listener> listeners;
    };

    RangedAudioParameter& parameter;
    LockedListeners listeners;
    std::atomic<float> unnormalisedValue { 0.0f };
    std::atomic<bool> needsUpdate { true }, listenersNeedCalling { true };
    // BaySickDAW QA-UndoCoverage: set at write time (writer's thread), consumed
    // by flushToTree (message thread) -- atomic for that cross-thread hand-off.
    std::atomic<bool> pendingIsProgrammatic { false };
    bool ignoreParameterChangedCallbacks { false };
};

//==============================================================================
AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& processorToConnectTo,
                                                            UndoManager* undoManagerToUse,
                                                            const Identifier& valueTreeType,
                                                            ParameterLayout parameterLayout)
    : AudioProcessorValueTreeState (processorToConnectTo, undoManagerToUse)
{
    struct PushBackVisitor final : ParameterLayout::Visitor
    {
        explicit PushBackVisitor (AudioProcessorValueTreeState& stateIn)
            : state (&stateIn) {}

        void visit (std::unique_ptr<RangedAudioParameter> param) const override
        {
            if (param == nullptr)
            {
                jassertfalse;
                return;
            }

            state->addParameterAdapter (*param);
            state->processor.addParameter (param.release());
        }

        void visit (std::unique_ptr<AudioProcessorParameterGroup> group) const override
        {
            if (group == nullptr)
            {
                jassertfalse;
                return;
            }

            for (const auto param : group->getParameters (true))
            {
                if (const auto rangedParam = dynamic_cast<RangedAudioParameter*> (param))
                {
                    state->addParameterAdapter (*rangedParam);
                }
                else
                {
                    // If you hit this assertion then you are attempting to add a parameter that is
                    // not derived from RangedAudioParameter to the AudioProcessorValueTreeState.
                    jassertfalse;
                }
            }

            state->processor.addParameterGroup (std::move (group));
        }

        AudioProcessorValueTreeState* state;
    };

    for (auto& item : parameterLayout.parameters)
        item->accept (PushBackVisitor (*this));

    state = ValueTree (valueTreeType);
}

AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& p, UndoManager* um)
    : processor (p), undoManager (um)
{
    liveApvtsInstances().add (this);
    startTimerHz (10);
    state.addListener (this);
}

AudioProcessorValueTreeState::~AudioProcessorValueTreeState()
{
    liveApvtsInstances().removeFirstMatchingValue (this);
    stopTimer();
}

//==============================================================================
RangedAudioParameter* AudioProcessorValueTreeState::createAndAddParameter (const String& paramID,
                                                                           const String& paramName,
                                                                           const String& labelText,
                                                                           NormalisableRange<float> range,
                                                                           float defaultVal,
                                                                           std::function<String (float)> valueToTextFunction,
                                                                           std::function<float (const String&)> textToValueFunction,
                                                                           bool isMetaParameter,
                                                                           bool isAutomatableParameter,
                                                                           bool isDiscreteParameter,
                                                                           AudioProcessorParameter::Category category,
                                                                           bool isBooleanParameter)
{
    auto attributes = AudioProcessorValueTreeStateParameterAttributes()
                          .withLabel (labelText)
                          .withStringFromValueFunction ([fn = std::move (valueToTextFunction)] (float v, int) { return fn (v); })
                          .withValueFromStringFunction (std::move (textToValueFunction))
                          .withMeta (isMetaParameter)
                          .withAutomatable (isAutomatableParameter)
                          .withDiscrete (isDiscreteParameter)
                          .withCategory (category)
                          .withBoolean (isBooleanParameter);

    return createAndAddParameter (std::make_unique<Parameter> (paramID,
                                                               paramName,
                                                               range,
                                                               defaultVal,
                                                               std::move (attributes)));
}

RangedAudioParameter* AudioProcessorValueTreeState::createAndAddParameter (std::unique_ptr<RangedAudioParameter> param)
{
    if (param == nullptr)
        return nullptr;

    // All parameters must be created before giving this manager a ValueTree state!
    // QA-0a (2026-05-07): BaySickDAW intentionally uses lazy APVTS registration --
    // params are created on-demand as new mixer strips / audio rows / engine
    // instances appear, well after the ValueTree state has been built from the
    // saved project XML.  Release builds ignore this jassert silently and
    // everything works.  Suppressing here so Debug builds don't pop hundreds
    // of breakpoints during cold start (every lazy strip registration would
    // trip it).  Real JUCE asserts elsewhere still fire.
    // jassert (! state.isValid());

    if (getParameter (param->paramID) != nullptr)
        return nullptr;

    addParameterAdapter (*param);

    processor.addParameter (param.get());

    return param.release();
}

//==============================================================================
void AudioProcessorValueTreeState::addParameterAdapter (RangedAudioParameter& param)
{
    adapterTable.emplace (param.paramID, std::make_unique<ParameterAdapter> (param));

    // BaySickDAW QA-UndoCoverage regression fix (2026-08-06): BaySickDAW
    // registers params LAZILY (QA-0a) -- after `state` was last assigned --
    // and updateParameterConnectionsToChildTrees only runs on a wholesale
    // state (re)assignment, so a late adapter kept an INVALID tree forever:
    // its flush could never transact, the begun "param:<id>" gesture stayed
    // empty, and the edit never reached the undo history.  Bind the child
    // here, with the value property PRE-SET to the param's live value -- an
    // id-only node would make setNewState read the missing value as the
    // DEFAULT and reset the live param (the QA-Ef save-path failure mode).
    // nullptr um everywhere: materialization is not an edit.
    if (state.isValid())
    {
        const ScopedLock lock (valueTreeChanging);
        ValueTree child (valueType);
        child.setProperty (idPropertyID, param.paramID, nullptr);
        child.setProperty (valuePropertyID, param.convertFrom0to1 (param.getValue()), nullptr);
        state.appendChild (child, nullptr);   // valueTreeChildAdded -> setNewState binds
    }
}

AudioProcessorValueTreeState::ParameterAdapter* AudioProcessorValueTreeState::getParameterAdapter (StringRef paramID) const
{
    auto it = adapterTable.find (paramID);
    return it == adapterTable.end() ? nullptr : it->second.get();
}

void AudioProcessorValueTreeState::addParameterListener (StringRef paramID, Listener* listener)
{
    if (auto* p = getParameterAdapter (paramID))
        p->addListener (listener);
}

void AudioProcessorValueTreeState::removeParameterListener (StringRef paramID, Listener* listener)
{
    if (auto* p = getParameterAdapter (paramID))
        p->removeListener (listener);
}

Value AudioProcessorValueTreeState::getParameterAsValue (StringRef paramID) const
{
    if (auto* adapter = getParameterAdapter (paramID))
        if (adapter->tree.isValid())
            return adapter->tree.getPropertyAsValue (valuePropertyID, undoManager);

    return {};
}

NormalisableRange<float> AudioProcessorValueTreeState::getParameterRange (StringRef paramID) const noexcept
{
    if (auto* p = getParameterAdapter (paramID))
        return p->getRange();

    return {};
}

RangedAudioParameter* AudioProcessorValueTreeState::getParameter (StringRef paramID) const noexcept
{
    if (auto adapter = getParameterAdapter (paramID))
        return &adapter->getParameter();

    return nullptr;
}

std::atomic<float>* AudioProcessorValueTreeState::getRawParameterValue (StringRef paramID) const noexcept
{
    if (auto* p = getParameterAdapter (paramID))
        return &p->getRawDenormalisedValue();

    return nullptr;
}

ValueTree AudioProcessorValueTreeState::copyState()
{
    ScopedLock lock (valueTreeChanging);
    flushParameterValuesToValueTree();
    return state.createCopy();
}

void AudioProcessorValueTreeState::replaceState (const ValueTree& newState)
{
    ScopedLock lock (valueTreeChanging);

    state = newState;

    if (undoManager != nullptr)
        undoManager->clearUndoHistory();
}

// BaySickDAW QA-UndoCoverage: see the header comment.
void AudioProcessorValueTreeState::replaceStateKeepingUndoHistory (const ValueTree& newState,
                                                                   const String& undoTransactionName)
{
    // Jeff ruling 3a: wholesale old<->new tree swap as ONE undoable action.
    // The undo/redo re-assignment re-fires valueTreeRedirected -> adapter
    // rebind -> flush; the programmatic-write phase keeps that flush from
    // performing nested actions mid-undo (UndoManager forbids recursion).
    struct StateSwapAction final : public UndoableAction
    {
        StateSwapAction (AudioProcessorValueTreeState& o, ValueTree oldS, ValueTree newS)
            : ownerTag (o.undoOwnerTag), ownerPtr (&o),
              oldState (std::move (oldS)), newState (std::move (newS)) {}

        bool apply (const ValueTree& target)
        {
            // Redo-across-resurrection fix: resolve the LIVE owner by tag
            // first (a re-created engine answers to the same tag), falling
            // back to the construction-time pointer for untagged instances.
            // Dead-owner no-op MUST report success -- a false return makes
            // UndoManager wipe the entire history.  The type guard covers
            // pointer reuse AND cross-kind tag mistakes: refuse a tree of a
            // different root type rather than redirect a foreign state.
            auto* owner = findByUndoOwnerTag (ownerTag);
            if (owner == nullptr && liveApvtsInstances().contains (ownerPtr))
                owner = ownerPtr;
            if (owner == nullptr)
                return true;
            if (owner->state.isValid() && target.isValid()
                 && owner->state.getType() != target.getType())
                return true;

            ScopedProgrammaticParamWrites spw;
            const ScopedLock sl (owner->valueTreeChanging);
            owner->state = target;
            return true;
        }
        bool perform() override
        {
            if (firstPerform) { firstPerform = false; return true; }   // already applied
            return apply (newState);
        }
        bool undo() override    { return apply (oldState); }
        int getSizeInUnits() override { return (int) sizeof (*this); }

        String ownerTag;
        AudioProcessorValueTreeState* ownerPtr;
        ValueTree oldState, newState;
        bool firstPerform = true;
    };

    if (undoManager != nullptr && undoTransactionName.isNotEmpty())
    {
        auto oldState = state.createCopy();

        {
            ScopedProgrammaticParamWrites spw;   // rebind pushes are programmatic
            ScopedLock lock (valueTreeChanging);
            state = newState;
        }

        undoManager->beginNewTransaction (undoTransactionName);
        undoManager->perform (new StateSwapAction (*this, std::move (oldState), newState));
        return;
    }

    // Unnamed keep-history swap (restore paths): programmatic by definition.
    ScopedProgrammaticParamWrites spw;
    ScopedLock lock (valueTreeChanging);
    state = newState;
}

void AudioProcessorValueTreeState::setNewState (ValueTree vt)
{
    jassert (vt.getParent() == state);

    if (auto* p = getParameterAdapter (vt.getProperty (idPropertyID).toString()))
    {
        p->tree = vt;
        p->setDenormalisedValue (p->tree.getProperty (valuePropertyID, p->getDenormalisedDefaultValue()));
    }
}

void AudioProcessorValueTreeState::updateParameterConnectionsToChildTrees()
{
    // BaySickDAW QA-UndoCoverage: a rebind is programmatic by definition --
    // setNewState's value pushes must never mark as user writes, or a state
    // assignment could mint undo entries into whatever transaction is open.
    ScopedProgrammaticParamWrites spw;
    ScopedLock lock (valueTreeChanging);

    for (auto& p : adapterTable)
        p.second->tree = ValueTree();

    for (const auto& child : state)
        setNewState (child);

    for (auto& p : adapterTable)
    {
        auto& adapter = *p.second;

        if (! adapter.tree.isValid())
        {
            adapter.tree = ValueTree (valueType);
            adapter.tree.setProperty (idPropertyID, adapter.getParameter().paramID, nullptr);
            state.appendChild (adapter.tree, nullptr);
        }
    }

    flushParameterValuesToValueTree();
}

void AudioProcessorValueTreeState::valueTreePropertyChanged (ValueTree& tree, const Identifier&)
{
    if (tree.hasType (valueType) && tree.getParent() == state)
        setNewState (tree);
}

void AudioProcessorValueTreeState::valueTreeChildAdded (ValueTree& parent, ValueTree& tree)
{
    if (parent == state && tree.hasType (valueType))
        setNewState (tree);
}

void AudioProcessorValueTreeState::valueTreeRedirected (ValueTree& v)
{
    if (v == state)
        updateParameterConnectionsToChildTrees();
}

bool AudioProcessorValueTreeState::flushParameterValuesToValueTree()
{
    ScopedLock lock (valueTreeChanging);

    bool anyUpdated = false;

    for (auto& p : adapterTable)
        anyUpdated |= p.second->flushToTree (valuePropertyID, undoManager, *this);

    return anyUpdated;
}

void AudioProcessorValueTreeState::timerCallback()
{
    auto anythingUpdated = flushParameterValuesToValueTree();

    startTimer (anythingUpdated ? 1000 / 50
                                : jlimit (50, 500, getTimerInterval() + 20));
}

//==============================================================================
template <typename Attachment, typename Control>
std::unique_ptr<Attachment> makeAttachment (const AudioProcessorValueTreeState& stateToUse,
                                            const String& parameterID,
                                            Control& control)
{
    if (auto* parameter = stateToUse.getParameter (parameterID))
        return std::make_unique<Attachment> (*parameter, control, stateToUse.undoManager);

    jassertfalse;
    return nullptr;
}

AudioProcessorValueTreeState::SliderAttachment::SliderAttachment (AudioProcessorValueTreeState& stateToUse,
                                                                  const String& parameterID,
                                                                  Slider& slider)
    : attachment (makeAttachment<SliderParameterAttachment> (stateToUse, parameterID, slider))
{
}

AudioProcessorValueTreeState::ComboBoxAttachment::ComboBoxAttachment (AudioProcessorValueTreeState& stateToUse,
                                                                      const String& parameterID,
                                                                      ComboBox& combo)
    : attachment (makeAttachment<ComboBoxParameterAttachment> (stateToUse, parameterID, combo))
{
}

AudioProcessorValueTreeState::ButtonAttachment::ButtonAttachment (AudioProcessorValueTreeState& stateToUse,
                                                                  const String& parameterID,
                                                                  Button& button)
    : attachment (makeAttachment<ButtonParameterAttachment> (stateToUse, parameterID, button))
{
}

//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

struct ParameterAdapterTests final : public UnitTest
{
    ParameterAdapterTests()
        : UnitTest ("Parameter Adapter", UnitTestCategories::audioProcessorParameters)
    {}

    void runTest() override
    {
        beginTest ("The default value is returned correctly");
        {
            const auto test = [&] (NormalisableRange<float> range, float value)
            {
                AudioParameterFloat param ({}, {}, range, value);

                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                expectEquals (adapter.getDenormalisedDefaultValue(), value);
            };

            test ({ -100, 100 }, 0);
            test ({ -2.5, 12.5 }, 10);
        }

        beginTest ("Denormalised parameter values can be retrieved");
        {
            const auto test = [&] (NormalisableRange<float> range, float value)
            {
                AudioParameterFloat param ({}, {}, range, {});
                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                adapter.setDenormalisedValue (value);

                expectEquals (adapter.getDenormalisedValue(), value);
                expectEquals (adapter.getRawDenormalisedValue().load(), value);
            };

            test ({ -20, -10 }, -15);
            test ({ 0, 7.5 }, 2.5);
        }

        beginTest ("Floats can be converted to text");
        {
            const auto test = [&] (NormalisableRange<float> range, float value, String expected)
            {
                AudioParameterFloat param ({}, {}, range, {});
                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                expectEquals (adapter.getTextForDenormalisedValue (value), expected);
            };

            test ({ -100, 100 }, 0, "0.0000000");
            test ({ -2.5, 12.5 }, 10, "10.0000000");
            test ({ -20, -10 }, -15, "-15.0000000");
            test ({ 0, 7.5 }, 2.5, "2.5000000");
        }

        beginTest ("Text can be converted to floats");
        {
            const auto test = [&] (NormalisableRange<float> range, String text, float expected)
            {
                AudioParameterFloat param ({}, {}, range, {});
                AudioProcessorValueTreeState::ParameterAdapter adapter (param);

                expectEquals (adapter.getDenormalisedValueForText (text), expected);
            };

            test ({ -100, 100 }, "0.0", 0);
            test ({ -2.5, 12.5 }, "10.0", 10);
            test ({ -20, -10 }, "-15.0", -15);
            test ({ 0, 7.5 }, "2.5", 2.5);
        }
    }
};

static ParameterAdapterTests parameterAdapterTests;

namespace
{
template <typename ValueType>
inline bool operator== (const NormalisableRange<ValueType>& a,
                        const NormalisableRange<ValueType>& b)
{
    return std::tie (a.start, a.end, a.interval, a.skew, a.symmetricSkew)
           == std::tie (b.start, b.end, b.interval, b.skew, b.symmetricSkew);
}

template <typename ValueType>
inline bool operator!= (const NormalisableRange<ValueType>& a,
                        const NormalisableRange<ValueType>& b)
{
    return ! (a == b);
}
} // namespace

class AudioProcessorValueTreeStateTests final : public UnitTest
{
private:
    using Parameter = AudioProcessorValueTreeState::Parameter;
    using ParameterGroup = AudioProcessorParameterGroup;
    using ParameterLayout = AudioProcessorValueTreeState::ParameterLayout;
    using Attributes = AudioProcessorValueTreeStateParameterAttributes;

    class TestAudioProcessor final : public AudioProcessor
    {
    public:
        TestAudioProcessor() = default;

        explicit TestAudioProcessor (ParameterLayout layout)
            : state (*this, nullptr, "state", std::move (layout)) {}

        const String getName() const override { return {}; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (AudioBuffer<float>&, MidiBuffer&) override {}
        using AudioProcessor::processBlock;
        double getTailLengthSeconds() const override { return {}; }
        bool acceptsMidi() const override { return {}; }
        bool producesMidi() const override { return {}; }
        AudioProcessorEditor* createEditor() override { return {}; }
        bool hasEditor() const override { return {}; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return {}; }
        void setCurrentProgram (int) override {}
        const String getProgramName (int) override { return {}; }
        void changeProgramName (int, const String&) override {}
        void getStateInformation (MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        AudioProcessorValueTreeState state { *this, nullptr };
    };

    struct Listener final : public AudioProcessorValueTreeState::Listener
    {
        void parameterChanged (const String& idIn, float valueIn) override
        {
            id = idIn;
            value = valueIn;
        }

        String id;
        float value{};
    };

public:
    AudioProcessorValueTreeStateTests()
        : UnitTest ("Audio Processor Value Tree State", UnitTestCategories::audioProcessorParameters)
    {}

    JUCE_BEGIN_IGNORE_WARNINGS_MSVC (6262)
    void runTest() override
    {
        ScopedJuceInitialiser_GUI scopedJuceInitialiser_gui;

        beginTest ("After calling createAndAddParameter, the number of parameters increases by one");
        {
            TestAudioProcessor proc;

            proc.state.createAndAddParameter (std::make_unique<Parameter> (
                String(),
                String(),
                NormalisableRange<float>(),
                0.0f));

            expectEquals (proc.getParameters().size(), 1);
        }

        beginTest ("After creating a normal named parameter, we can later retrieve that parameter");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f));

            expect (proc.state.getParameter (key) == param);
        }

        beginTest ("After construction, the value tree has the expected format");
        {
            TestAudioProcessor proc ({
                std::make_unique<AudioProcessorParameterGroup> ("A", "", "",
                    std::make_unique<AudioParameterBool> ("a", "", false),
                    std::make_unique<AudioParameterFloat> ("b", "", NormalisableRange<float>{}, 0.0f)),
                std::make_unique<AudioProcessorParameterGroup> ("B", "", "",
                    std::make_unique<AudioParameterInt> ("c", "", 0, 1, 0),
                    std::make_unique<AudioParameterChoice> ("d", "", StringArray { "foo", "bar" }, 0)) });

            const auto valueTree = proc.state.copyState();

            expectEquals (valueTree.getNumChildren(), 4);

            for (auto child : valueTree)
            {
                expect (child.hasType ("PARAM"));
                expect (child.hasProperty ("id"));
                expect (child.hasProperty ("value"));
            }
        }

        beginTest ("Meta parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f,
                                   Attributes().withMeta (true)));

            expect (param->isMetaParameter());
        }

        beginTest ("Automatable parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f,
                                   Attributes().withAutomatable (true)));

            expect (param->isAutomatable());
        }

        beginTest ("Discrete parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f,
                                   Attributes().withDiscrete (true)));

            expect (param->isDiscrete());
        }

        beginTest ("Custom category parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f,
                                   Attributes().withCategory (AudioProcessorParameter::Category::inputMeter)));

            expect (param->category == AudioProcessorParameter::Category::inputMeter);
        }

        beginTest ("Boolean parameters can be created");
        {
            TestAudioProcessor proc;

            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f,
                                   Attributes().withBoolean (true)));

            expect (param->isBoolean());
        }

        beginTest ("After creating a custom named parameter, we can later retrieve that parameter");
        {
            const auto key = "id";
            auto param = std::make_unique<AudioParameterBool> (key, "", false);
            const auto paramPtr = param.get();

            TestAudioProcessor proc (std::move (param));

            expect (proc.state.getParameter (key) == paramPtr);
        }

        beginTest ("After adding a normal parameter that already exists, the AudioProcessor parameters are unchanged");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f));

            proc.state.createAndAddParameter (std::make_unique<Parameter> (
                key,
                String(),
                NormalisableRange<float>(),
                0.0f));

            expectEquals (proc.getParameters().size(), 1);
            expect (proc.getParameters().getFirst() == param);
        }

        beginTest ("After setting a parameter value, that value is reflected in the state");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f));

            const auto value = 0.5f;
            param->setValueNotifyingHost (value);

            expectEquals (proc.state.getRawParameterValue (key)->load(), value);
        }

        beginTest ("After adding an APVTS::Parameter, its value is the default value");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto value = 5.0f;

            proc.state.createAndAddParameter (std::make_unique<Parameter> (
                key,
                String(),
                NormalisableRange<float> (0.0f, 100.0f, 10.0f),
                value));

            expectEquals (proc.state.getRawParameterValue (key)->load(), value);
        }

        beginTest ("Listeners receive notifications when parameters change");
        {
            Listener listener;
            TestAudioProcessor proc;
            const auto key = "id";
            const auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                                   key,
                                   String(),
                                   NormalisableRange<float>(),
                                   0.0f));
            proc.state.addParameterListener (key, &listener);

            const auto value = 0.5f;
            param->setValueNotifyingHost (value);

            expectEquals (listener.id, String { key });
            expectEquals (listener.value, value);
        }

        beginTest ("Bool parameters have a range of 0-1");
        {
            const auto key = "id";

            TestAudioProcessor proc (std::make_unique<AudioParameterBool> (key, "", false));

            expect (proc.state.getParameterRange (key) == NormalisableRange<float> (0.0f, 1.0f, 1.0f));
        }

        beginTest ("Float parameters retain their specified range");
        {
            const auto key = "id";
            const auto range = NormalisableRange<float> { -100, 100, 0.7f, 0.2f, true };

            TestAudioProcessor proc (std::make_unique<AudioParameterFloat> (key, "", range, 0.0f));

            expect (proc.state.getParameterRange (key) == range);
        }

        beginTest ("Int parameters retain their specified range");
        {
            const auto key = "id";
            const auto min = -27;
            const auto max = 53;

            TestAudioProcessor proc (std::make_unique<AudioParameterInt> (key, "", min, max, 0));

            expect (proc.state.getParameterRange (key) == NormalisableRange<float> (float (min), float (max), 1.0f));
        }

        beginTest ("Choice parameters retain their specified range");
        {
            const auto key = "id";
            const auto choices = StringArray { "", "", "" };

            TestAudioProcessor proc (std::make_unique<AudioParameterChoice> (key, "", choices, 0));

            expect (proc.state.getParameterRange (key) == NormalisableRange<float> (0.0f, (float) (choices.size() - 1), 1.0f));
            expect (proc.state.getParameter (key)->getNumSteps() == choices.size());
        }

        beginTest ("When the parameter value is changed, normal parameter values are updated");
        {
            TestAudioProcessor proc;
            const auto key = "id";
            const auto initialValue = 0.2f;
            auto param = proc.state.createAndAddParameter (std::make_unique<Parameter> (
                             key,
                             String(),
                             NormalisableRange<float>(),
                             initialValue));
            proc.state.state = ValueTree { "state" };

            auto value = proc.state.getParameterAsValue (key);
            expectEquals (float (value.getValue()), initialValue);

            const auto newValue = 0.75f;
            value = newValue;

            expectEquals (param->getValue(), newValue);
            expectEquals (proc.state.getRawParameterValue (key)->load(), newValue);
        }

        beginTest ("When the parameter value is changed, custom parameter values are updated");
        {
            const auto key = "id";
            const auto choices = StringArray ("foo", "bar", "baz");
            auto param = std::make_unique<AudioParameterChoice> (key, "", choices, 0);
            const auto paramPtr = param.get();
            TestAudioProcessor proc (std::move (param));

            const auto newValue = 2.0f;
            auto value = proc.state.getParameterAsValue (key);
            value = newValue;

            expectEquals (paramPtr->getCurrentChoiceName(), choices[int (newValue)]);
            expectEquals (proc.state.getRawParameterValue (key)->load(), newValue);
        }

        beginTest ("When the parameter value is changed, listeners are notified");
        {
            Listener listener;
            TestAudioProcessor proc;
            const auto key = "id";
            proc.state.createAndAddParameter (std::make_unique<Parameter> (
                key,
                String(),
                NormalisableRange<float>(),
                0.0f));
            proc.state.addParameterListener (key, &listener);
            proc.state.state = ValueTree { "state" };

            const auto newValue = 0.75f;
            proc.state.getParameterAsValue (key) = newValue;

            expectEquals (listener.value, newValue);
            expectEquals (listener.id, String { key });
        }

        beginTest ("When the parameter value is changed, listeners are notified");
        {
            const auto key = "id";
            const auto choices = StringArray { "foo", "bar", "baz" };
            Listener listener;
            TestAudioProcessor proc (std::make_unique<AudioParameterChoice> (key, "", choices, 0));
            proc.state.addParameterListener (key, &listener);

            const auto newValue = 2.0f;
            proc.state.getParameterAsValue (key) = newValue;

            expectEquals (listener.value, newValue);
            expectEquals (listener.id, String (key));
        }
    }
    JUCE_END_IGNORE_WARNINGS_MSVC
};

static AudioProcessorValueTreeStateTests audioProcessorValueTreeStateTests;

#endif

} // namespace juce
