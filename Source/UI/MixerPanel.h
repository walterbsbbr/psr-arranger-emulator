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
class MixerPanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit MixerPanel (StyleEngine& engine);
    ~MixerPanel() override { stopTimer(); }
    void resized() override;
    void paint   (juce::Graphics& g) override;

private:
    void timerCallback() override;

    StyleEngine& styleEngine;

    static constexpr int NUM_PARTS = 8;
    static const char* PART_NAMES[NUM_PARTS];
    // Canais FluidSynth correspondentes a cada part (0-indexed)
    // Part 0=Sub-Rhy(ch7), 1=Rhythm(ch8), 2=Bass(ch10), 3=Chord1(ch11),
    // 4=Chord2(ch12), 5=Pad(ch13), 6=Phr1(ch14), 7=Phr2(ch15)
    static const int PART_FS_CHANNEL[NUM_PARTS];

    juce::Slider     sliderVol[NUM_PARTS];
    juce::TextButton btnMute[NUM_PARTS];
    juce::Label      lblPart[NUM_PARTS];
    juce::Label      lblProgram[NUM_PARTS]; // nome do instrumento carregado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
