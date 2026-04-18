#include "ChordDisplay.h"

ChordDisplay::ChordDisplay()
{
    setSize (200, 80);
}

void ChordDisplay::setChord (const ChordInfo& chord)
{
    currentChord = chord;
    repaint();
}

void ChordDisplay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));
    g.setColour (juce::Colours::cyan);
    g.drawRect (getLocalBounds(), 1);

    auto area = getLocalBounds().reduced (8);

    // Label no topo
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colours::grey);
    g.drawText ("CHORD", area.removeFromTop (14), juce::Justification::centred, false);

    // Acorde grande
    juce::String text = currentChord.valid ? currentChord.toString() : "---";
    g.setFont (juce::FontOptions (36.0f, juce::Font::bold));
    g.setColour (currentChord.valid ? juce::Colours::limegreen : juce::Colours::grey);
    g.drawText (text, area, juce::Justification::centred, false);
}
