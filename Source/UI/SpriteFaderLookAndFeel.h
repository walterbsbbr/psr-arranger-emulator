#pragma once
#include <JuceHeader.h>

/**
 * SpriteFaderLookAndFeel
 *
 * Desenha sliders verticais usando o sprite MB_SLIDER.png embutido no
 * binário (BinaryData): 31 frames de 59x128px empilhados verticalmente.
 * Frame 0 = cursor no topo do curso (valor máximo), frame 30 = cursor
 * embaixo (valor mínimo) -- convenção de fader de mesa de mixagem.
 */
class SpriteFaderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SpriteFaderLookAndFeel();

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;

private:
    juce::Image spriteSheet;

    static constexpr int NUM_FRAMES = 31;
    static constexpr int FRAME_W    = 59;
    static constexpr int FRAME_H    = 128;
};
