#include "RustyDrumsMapWindow.h"
#include "../BaySickRustyDrums/BaySickRustyDrumsProcessor.h"
#include "SharedUI.h"
#include "SampleLibrary.h"

namespace
{
constexpr int kColKey       = 1;   // J-7b: musical note name (e.g. "C3", "D#5") - primary cross-ref
constexpr int kColMidi      = 2;   // raw MIDI number for advanced users
constexpr int kColSound     = 3;
constexpr int kColArtic     = 4;

// FL Studio convention: MIDI 60 = C5 (middle C).  Same convention used by the
// piano-keyboard widget in PianoRoll.cpp.
juce::String midiToKeyName (int midi)
{
    static const char* kPitch[12] = { "C", "C#", "D", "D#", "E", "F",
                                      "F#", "G", "G#", "A", "A#", "B" };
    if (midi < 0 || midi > 127) return juce::String (midi);
    const int octave = midi / 12;
    const int pc     = midi % 12;
    return juce::String (kPitch[pc]) + juce::String (octave);
}
}

RustyDrumsMapTable::RustyDrumsMapTable (BaySickRustyDrumsProcessor* engine)
    : mEngine (engine)
{
    auto& hdr = mTable.getHeader();
    hdr.addColumn ("Key",          kColKey,    70,  60, 100);
    hdr.addColumn ("MIDI",         kColMidi,   60,  50, 80);
    hdr.addColumn ("Sound",        kColSound,  160, 100, 220);
    hdr.addColumn ("Articulation", kColArtic,  240, 120, 360);

    mTable.setModel (this);
    mTable.setColour (juce::ListBox::backgroundColourId, VC::Bg);
    mTable.setOutlineThickness (0);
    mTable.setRowHeight (22);
    mTable.setHeaderHeight (24);
    addAndMakeVisible (mTable);

    rebuildRows();
}

RustyDrumsMapTable::~RustyDrumsMapTable() = default;

void RustyDrumsMapTable::resized()
{
    mTable.setBounds (getLocalBounds());
}

int RustyDrumsMapTable::getNumRows() { return (int) mRows.size(); }

void RustyDrumsMapTable::paintRowBackground (juce::Graphics& g, int row, int w, int h, bool /*selected*/)
{
    g.fillAll ((row % 2 == 0) ? juce::Colour (0xff232328) : juce::Colour (0xff1c1c20));
    juce::ignoreUnused (w, h);
}

void RustyDrumsMapTable::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool /*selected*/)
{
    if (row < 0 || row >= (int) mRows.size()) return;
    const auto& r = mRows[(size_t) row];
    g.setColour (VC::TextDim);
    g.setFont (juce::Font (13.f));
    juce::String text;
    switch (col)
    {
        case kColKey:    text = midiToKeyName (r.midi); break;
        case kColMidi:   text = juce::String (r.midi);  break;
        case kColSound:  text = r.sound;                break;
        case kColArtic:  text = r.articulation;         break;
        default:                                        break;
    }
    g.drawText (text, juce::Rectangle<int> (8, 0, w - 16, h),
                juce::Justification::centredLeft, true);
}

void RustyDrumsMapTable::rebuildRows()
{
    mRows.clear();

    // J-7b: prefer the live engine's parsed keymap when a kit is loaded -
    // it reflects whatever .sfz the user actually has open.  When no engine
    // exists (Help menu opened before adding the BaySickRustyDrums tab),
    // fall back to parsing the canonical Big Rusty Drums kit directly so
    // the map is informative even when the tab isn't spawned.
    std::vector<PianoRollKey> keymap;
    if (mEngine)
        keymap = mEngine->getPianoRollKeymap();

    if (keymap.empty())
    {
        const juce::File kitRoot = SampleLibrary::getCoreLibraryDir()
                                       .getChildFile ("Big Rusty Drums");
        if (kitRoot.isDirectory())
            keymap = BaySickRustyDrumsProcessor::parsePianoRollKeymap (kitRoot);
    }

    for (const auto& key : keymap)
        mRows.push_back ({ key.midiNote, key.soundName,
                           key.articulation, key.defineName });

    mTable.updateContent();
    mTable.repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Window
// ─────────────────────────────────────────────────────────────────────────────
RustyDrumsMapWindow::RustyDrumsMapWindow (BaySickRustyDrumsProcessor* engine)
    : juce::DocumentWindow ("Rusty Drums Map",
                            VC::Bg,
                            juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setContentOwned (new RustyDrumsMapTable (engine), true);
    centreWithSize (640, 600);
    setVisible (true);
    toFront (true);
}

void RustyDrumsMapWindow::closeButtonPressed()
{
    delete this;
}
