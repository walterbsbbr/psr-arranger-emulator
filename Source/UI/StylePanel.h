#pragma once
#include <JuceHeader.h>
#include "../Engine/StyleEngine.h"

/**
 * StylePanel
 *
 * Painel com os botões de controle do arranjador:
 *  - Intro A/B/C
 *  - Main A/B/C/D  (iluminam conforme a seção ativa)
 *  - Fill In AA/AB/BA/BB
 *  - Ending A/B/C
 *  - Start/Stop
 */
class StylePanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit StylePanel (StyleEngine& engine);
    ~StylePanel() override;

    void resized() override;
    void paint   (juce::Graphics& g) override;

private:
    void timerCallback() override;
    void buildButtons();

    StyleEngine& styleEngine;

    // Botões de seção
    juce::TextButton btnIntro[3];
    juce::TextButton btnMain[4];
    juce::TextButton btnFill[4];
    juce::TextButton btnEnding[3];
    juce::TextButton btnStartStop;

    // Labels de grupo
    juce::Label lblIntro, lblMain, lblFill, lblEnding;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StylePanel)
};
