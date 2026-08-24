#pragma once
#include <JuceHeader.h>
#include "MidiLearnRegistry.h"
#include <functional>
#include <vector>

// -- MidiMapView -- QA-TrueLevel (Jeff's ruling, 2026-08-24) ------------------
// Help > "View Projects MidiMap": everything the current project has MIDI
// mapped, one row per binding, with enough columns to tell at a glance WHAT is
// bound (the control's full friendly name), to WHICH hardware gesture (message
// type + CC number), on which channel, from which device.  Read-only by
// design: learning and forgetting stay on the knob's own right-click menu.
//
// Refresh: a 2 Hz timer compares a cheap signature of the table and rebuilds
// only on change, so a learn or forget done while the window is open shows up
// without wiring into the registry's single onChanged hook (MidiLearnUI owns
// that one).
// -----------------------------------------------------------------------------
class MidiMapView : public juce::Component,
                    private juce::TableListBoxModel,
                    private juce::Timer
{
public:
    MidiMapView (MidiLearnRegistry& reg,
                 std::function<juce::String (const juce::String&)> resolveLabel)
        : mReg (reg), mResolve (std::move (resolveLabel))
    {
        mTable.setModel (this);
        mTable.setRowHeight (24);
        mTable.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff101214));
        auto& header = mTable.getHeader();
        header.addColumn ("Control",  1, 320, 160, -1, juce::TableHeaderComponent::notSortable);
        header.addColumn ("Hardware", 2, 110, 80,  -1, juce::TableHeaderComponent::notSortable);
        header.addColumn ("Channel",  3, 70,  60,  -1, juce::TableHeaderComponent::notSortable);
        header.addColumn ("Device",   4, 180, 100, -1, juce::TableHeaderComponent::notSortable);
        addAndMakeVisible (mTable);

        mEmpty.setText ("No MIDI mappings in this project.\n\n"
                        "Right-click any knob and choose MIDI Learn, then move a "
                        "control on your MIDI device to bind it.",
                        juce::dontSendNotification);
        mEmpty.setJustificationType (juce::Justification::centred);
        mEmpty.setColour (juce::Label::textColourId, juce::Colour (0xff8f9aa1));
        addChildComponent (mEmpty);

        refresh();
        startTimerHz (2);
    }

    void resized() override
    {
        mTable.setBounds (getLocalBounds().reduced (6));
        mEmpty.setBounds (getLocalBounds().reduced (20));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff16191c));
    }

private:
    struct Row
    {
        juce::String control, hardware, channel, device;
    };

    static juce::String hardwareText (const MidiLearnRegistry::Mapping& m)
    {
        switch (m.msgType)
        {
            case MidiLearnRegistry::MessageType::Cc:              return "CC " + juce::String (m.ccNumber);
            case MidiLearnRegistry::MessageType::PitchBend:       return "Pitch Bend";
            case MidiLearnRegistry::MessageType::ChannelPressure: return "Aftertouch";
        }
        return "?";
    }

    void refresh()
    {
        const auto maps = mReg.getAllMappings();

        juce::String sig;
        for (const auto& m : maps)
            sig << m.paramId << "|" << (int) m.msgType << "|" << m.ccNumber << "|"
                << m.channel << "|" << m.deviceName << ";";
        if (sig == mSignature) return;
        mSignature = sig;

        mRows.clear();
        mRows.reserve (maps.size());
        for (const auto& m : maps)
        {
            Row r;
            r.control  = mResolve ? mResolve (m.paramId) : juce::String();
            if (r.control.isEmpty()) r.control = m.paramId;
            r.hardware = hardwareText (m);
            r.channel  = m.channel == 0 ? juce::String ("Omni") : juce::String (m.channel);
            r.device   = m.deviceName.isEmpty() ? juce::String ("Any device") : m.deviceName;
            mRows.push_back (std::move (r));
        }
        std::sort (mRows.begin(), mRows.end(),
                   [] (const Row& a, const Row& b) { return a.control.compareIgnoreCase (b.control) < 0; });

        mTable.updateContent();
        mTable.setVisible (! mRows.empty());
        mEmpty.setVisible (mRows.empty());
        repaint();
    }

    void timerCallback() override { refresh(); }

    int getNumRows() override { return (int) mRows.size(); }

    void paintRowBackground (juce::Graphics& g, int row, int, int, bool selected) override
    {
        g.fillAll (selected ? juce::Colour (0xff2c3036)
                            : (row % 2 == 0 ? juce::Colour (0xff16191c) : juce::Colour (0xff181c20)));
    }

    void paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool) override
    {
        if (row < 0 || row >= (int) mRows.size()) return;
        const auto& r = mRows[(size_t) row];
        const juce::String* text = columnId == 1 ? &r.control
                                 : columnId == 2 ? &r.hardware
                                 : columnId == 3 ? &r.channel : &r.device;
        g.setColour (columnId == 1 ? juce::Colour (0xffe6e8ea) : juce::Colour (0xffb8bec6));
        g.setFont (juce::Font (13.0f));
        g.drawText (*text, 6, 0, width - 10, height, juce::Justification::centredLeft, true);
    }

    MidiLearnRegistry& mReg;
    std::function<juce::String (const juce::String&)> mResolve;
    juce::TableListBox mTable { "midimap", nullptr };
    juce::Label        mEmpty;
    std::vector<Row>   mRows;
    juce::String       mSignature;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMapView)
};
