#pragma once
#include <JuceHeader.h>
#include "../Engine/ChordDetector.h"

/** Exibe o acorde detectado com fonte grande e cor dinâmica */
class ChordDisplay : public juce::Component
{
public:
    ChordDisplay();
    void setChord (const ChordInfo& chord);
    void paint (juce::Graphics& g) override;

private:
    ChordInfo currentChord;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordDisplay)
};
