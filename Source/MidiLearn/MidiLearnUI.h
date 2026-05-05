#pragma once
#include <JuceHeader.h>
#include "MidiLearnRegistry.h"
#include "../Standalone/SharedUI.h"   // VKnob full type for dynamic_cast in findTargetRecursive

// ─────────────────────────────────────────────────────────────────────────────
// MidiLearnOutlineOverlay — paints a dashed yellow outline on top of any
// control when MIDI Learn targets it.  Component-based instead of paint-only
// so it works for plain juce::Slider widgets (mixer faders) just as well as
// VKnobs -- the controller parents one of these onto the active target's
// parent and sizes it over the target's bounds.
// ─────────────────────────────────────────────────────────────────────────────
class MidiLearnOutlineOverlay : public juce::Component
{
public:
    MidiLearnOutlineOverlay()
    {
        setInterceptsMouseClicks (false, false);
        setAlwaysOnTop (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        juce::Path outline;
        outline.addRoundedRectangle (bounds, 4.0f);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (juce::Colours::yellow);
        g.fillPath (dashed);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnOutlineOverlay)
};

// ─────────────────────────────────────────────────────────────────────────────
// MidiLearnUI — Phase I-3c (2026-05-02)
// ─────────────────────────────────────────────────────────────────────────────
// Front-end for MidiLearnRegistry.  Owns the learn-mode lifecycle:
//   * 30 second auto-cancel timer (locked spec call)
//   * Escape key cancels learn (locked spec call)
//   * Notifies the UI when learn target changes so the dashed-yellow
//     knob outline can update
//
// Wires into VKnobAutomation namespace via StandaloneEditor's startup so
// every right-clickable knob in the app gets MIDI Learn / Forget / Save-
// as-global-default menu items with no per-component code.
// ─────────────────────────────────────────────────────────────────────────────

class MidiLearnUI : public juce::Timer,
                    public juce::KeyListener
{
public:
    explicit MidiLearnUI (MidiLearnRegistry& reg)
        : mRegistry (reg)
    {}

    ~MidiLearnUI() override
    {
        if (mTopLevel != nullptr)
            mTopLevel->removeKeyListener (this);
    }

    // Begin learning: register a capture callback in the registry.  When the
    // user wiggles a hardware control, the registry's tryCaptureLearn fires
    // (on the audio thread), calls the callback, which commits the mapping
    // and clears the learn state.  Also starts the 30s timeout, and parents
    // a dashed-yellow outline overlay on top of the targeted control so the
    // user can see which knob is in learn mode.
    void beginLearn (const juce::String& paramId)
    {
        if (paramId.isEmpty()) return;

        // If something else was learning, cancel it first.  No-op if not.
        cancelLearn();

        mRegistry.beginLearn (paramId,
            [this] (const MidiLearnRegistry::Mapping& captured) -> bool
            {
                // Audio-thread context: commit by returning true.  The
                // registry handles atomic commit; we just request a UI
                // refresh on the message thread (which removes the overlay).
                juce::MessageManager::callAsync ([this]
                {
                    stopTimer();
                    removeOutlineOverlay();
                    if (onLearnStateChanged) onLearnStateChanged();
                });
                juce::ignoreUnused (captured);
                return true;
            });

        startTimer (30 * 1000);   // 30s auto-cancel
        installOutlineOverlay (paramId);
        if (onLearnStateChanged) onLearnStateChanged();
    }

    // Cancel current learn (timeout, Escape, or user action).  Safe no-op
    // when not currently learning.
    void cancelLearn()
    {
        if (! mRegistry.isLearning()) return;
        mRegistry.cancelLearn();
        stopTimer();
        removeOutlineOverlay();
        if (onLearnStateChanged) onLearnStateChanged();
    }

    // Forget mapping for paramId.  No-op if no mapping exists.
    void forget (const juce::String& paramId)
    {
        mRegistry.removeMapping (paramId);
        if (onLearnStateChanged) onLearnStateChanged();
    }

    // Save current registry to Documents/BaySickDAW/MidiMappings.xml.
    bool saveAsGlobalDefaults() const
    {
        return mRegistry.saveAsGlobalDefaults();
    }

    // Query: is paramId currently being learned?  Used by VKnob to draw the
    // dashed yellow outline.
    bool isLearningTarget (const juce::String& paramId) const
    {
        return mRegistry.isLearning()
            && mRegistry.getLearnTargetParamId() == paramId;
    }

    bool isLearning() const { return mRegistry.isLearning(); }

    bool isMapped (const juce::String& paramId) const
    {
        return mRegistry.hasMapping (paramId);
    }

    // For the right-click menu's "MIDI Forget" item label: returns a short
    // human-readable description of the current mapping, e.g.
    // "USB Keyboard CC#74 ch1".  Empty when no mapping.
    juce::String describeMapping (const juce::String& paramId) const
    {
        MidiLearnRegistry::Mapping m;
        if (! mRegistry.getMapping (paramId, m)) return {};

        juce::String s;
        if (m.deviceName.isNotEmpty()) s << m.deviceName << " ";
        switch (m.msgType)
        {
            case MidiLearnRegistry::MessageType::Cc:
                s << "CC#" << m.ccNumber; break;
            case MidiLearnRegistry::MessageType::PitchBend:
                s << "Pitch Wheel"; break;
            case MidiLearnRegistry::MessageType::ChannelPressure:
                s << "Aftertouch"; break;
        }
        if (m.channel == 0)        s << " omni";
        else                        s << " ch" << m.channel;
        return s;
    }

    // Install on the main window so Escape cancels learn from anywhere.
    void installOnTopLevelComponent (juce::Component* topLevel)
    {
        if (mTopLevel == topLevel) return;
        if (mTopLevel != nullptr) mTopLevel->removeKeyListener (this);
        mTopLevel = topLevel;
        if (mTopLevel != nullptr)
        {
            mTopLevel->addKeyListener (this);
            mTopLevel->setWantsKeyboardFocus (true);
        }
    }

    // Listener: called whenever learn state or mapping changes (so VKnobs
    // can repaint).
    std::function<void()> onLearnStateChanged;

    // ── juce::Timer ──────────────────────────────────────────────────────────
    void timerCallback() override
    {
        // 30s auto-cancel.
        cancelLearn();
    }

    // ── juce::KeyListener ────────────────────────────────────────────────────
    bool keyPressed (const juce::KeyPress& key, juce::Component* /*originator*/) override
    {
        if (! mRegistry.isLearning()) return false;
        if (key == juce::KeyPress::escapeKey)
        {
            cancelLearn();
            return true;   // consumed
        }
        return false;
    }

private:
    // Walk the component tree under mTopLevel looking for a control whose
    // identity matches `paramId`.  Two match strategies:
    //   (a) VKnob: a direct child whose `paramId` member equals the target.
    //       Returns the VKnob's slider so the outline frames the rotary
    //       artwork rather than the label-included VKnob bounds.
    //   (b) Plain juce::Slider: getComponentID() == target.  Mixer faders
    //       and other plain controls set componentID via their automation
    //       wiring (see VKnobAutomation::sOnAutomate registration sites).
    juce::Component* findTargetComponent (const juce::String& paramId) const
    {
        if (mTopLevel == nullptr) return nullptr;
        return findTargetRecursive (mTopLevel, paramId);
    }

    juce::Component* findTargetRecursive (juce::Component* node,
                                           const juce::String& paramId) const
    {
        if (node == nullptr) return nullptr;

        // VKnob: check if this is one and matches.  (We match by paramId
        // member, not by componentID, because VKnob doesn't always set its
        // componentID -- the inner Slider does, but for mapping purposes we
        // want to outline the whole knob.)
        if (auto* vk = dynamic_cast<VKnob*>(node))
        {
            if (vk->paramId == paramId)
                return &vk->slider;   // outline the rotary, not the label
        }

        // Plain Slider with matching componentID -- mixer faders fall here.
        if (auto* slider = dynamic_cast<juce::Slider*>(node))
        {
            if (slider->getComponentID() == paramId)
                return slider;
        }

        for (int i = 0; i < node->getNumChildComponents(); ++i)
        {
            if (auto* hit = findTargetRecursive (node->getChildComponent (i), paramId))
                return hit;
        }
        return nullptr;
    }

    void installOutlineOverlay (const juce::String& paramId)
    {
        removeOutlineOverlay();

        auto* target = findTargetComponent (paramId);
        if (target == nullptr) return;
        auto* parent = target->getParentComponent();
        if (parent == nullptr) return;

        mOverlay = std::make_unique<MidiLearnOutlineOverlay>();
        parent->addAndMakeVisible (*mOverlay);
        // Inflate by 2px so the dashed outline doesn't sit exactly on the
        // control's edge (where it'd be hidden by the control's own paint).
        mOverlay->setBounds (target->getBounds().expanded (2));
        mOverlayTarget = target;
    }

    void removeOutlineOverlay()
    {
        if (mOverlay && mOverlay->getParentComponent() != nullptr)
            mOverlay->getParentComponent()->removeChildComponent (mOverlay.get());
        mOverlay.reset();
        mOverlayTarget = nullptr;
    }

    MidiLearnRegistry& mRegistry;
    juce::Component*   mTopLevel { nullptr };

    std::unique_ptr<MidiLearnOutlineOverlay> mOverlay;
    juce::Component*                          mOverlayTarget { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnUI)
};
