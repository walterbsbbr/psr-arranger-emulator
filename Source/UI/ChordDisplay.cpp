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

    juce::String text = currentChord.valid ? currentChord.toString() : "---";
    g.setFont (juce::FontOptions (36.0f, juce::Font::bold));
    g.setColour (currentChord.valid ? juce::Colours::limegreen : juce::Colours::grey);
    g.drawText (text, getLocalBounds().reduced (8), juce::Justification::centred, false);
}
