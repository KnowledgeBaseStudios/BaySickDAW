#include "PluginEditor.h"
using namespace juce;

VibesynthEditor::VibesynthEditor(VibeSynthProcessor& p)
    : AudioProcessorEditor(p), mProcessor(p)
{
    setSize(kW, kH);
}

void VibesynthEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1c1c1e));

    g.setColour(Colour(0xffe94560));
    g.setFont(Font(22.f, Font::bold));
    g.drawText("BaySickDAW", getLocalBounds().reduced(20).removeFromTop(40),
               Justification::centredLeft);

    g.setColour(Colour(0xff808090));
    g.setFont(Font(13.f));
    g.drawText("BaySickDAW runs as a standalone application.",
               getLocalBounds().reduced(20, 0).withY(50).withHeight(22),
               Justification::centredLeft);
    g.drawText("Open the standalone app for the full interface.",
               getLocalBounds().reduced(20, 0).withY(74).withHeight(22),
               Justification::centredLeft);

    g.setColour(Colour(0xff4a4a5e));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f), 4.f, 1.f);
}
