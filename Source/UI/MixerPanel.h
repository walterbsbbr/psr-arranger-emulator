#pragma once
#include <JuceHeader.h>
#include "../Engine/StyleEngine.h"

/**
 * MixerPanel
 *
 * 8 faixas verticais (Sub-Rhy, Rhythm, Bass, Chord1, Chord2, Pad, Phr1, Phr2)
 * cada uma com:
 *  - Label do nome
 *  - Slider de volume (0-127)
 *  - Botão Mute
 */
class MixerPanel : public juce::Component
{
public:
    explicit MixerPanel (StyleEngine& engine);
    void resized() override;
    void paint   (juce::Graphics& g) override;

private:
    StyleEngine& styleEngine;

    static constexpr int NUM_PARTS = 8;
    static const char* PART_NAMES[NUM_PARTS];

    juce::Slider     sliderVol[NUM_PARTS];
    juce::TextButton btnMute[NUM_PARTS];
    juce::Label      lblPart[NUM_PARTS];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
