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
    // Canais FluidSynth 0-indexed: parts 0-7 → canais 8-15 (JUCE 9-16)
    static constexpr int FIRST_STYLE_CH = 8; // 0-indexed

    /** Abre um menu com os presets do SoundFont carregado para o canal 'ch' (0-indexed). */
    void showPresetPicker (int ch, juce::Component* anchor);

    /** Aplica a cor do botão de oitava (neutro/verde/laranja) conforme o shift atual. */
    void refreshOctaveButtonColours (int i);

    juce::Slider     sliderVol[NUM_PARTS];
    juce::TextButton btnMute[NUM_PARTS];
    juce::Label      lblPart[NUM_PARTS];
    juce::TextButton lblProgram[NUM_PARTS]; // nome do instrumento -- clicável, troca o timbre manualmente
    juce::TextButton btnOctaveDown[NUM_PARTS]; // "-" ao lado esquerdo do rótulo do canal
    juce::TextButton btnOctaveUp[NUM_PARTS];   // "+" ao lado direito do rótulo do canal

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};
