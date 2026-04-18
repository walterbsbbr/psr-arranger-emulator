#pragma once
#include <JuceHeader.h>
#include "../Engine/StyleEngine.h"

/** Controla BPM, transpose global e exibe o estado do transporte */
class TransportPanel : public juce::Component
{
public:
    explicit TransportPanel (StyleEngine& engine);
    void resized() override;
    void paint   (juce::Graphics& g) override;

    void setStyleName (const juce::String& name);

    std::function<void()> onStyleFileRequested;

private:
    StyleEngine& styleEngine;

    juce::Label         lblBpm;
    juce::Slider        sliderBpm;
    juce::Label         lblTranspose;
    juce::Slider        sliderTranspose;
    juce::TextButton    btnLoadStyle;
    juce::Label         lblStyleName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportPanel)
};
