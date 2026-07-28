#include "SpriteFaderLookAndFeel.h"
#include "BinaryData.h"

SpriteFaderLookAndFeel::SpriteFaderLookAndFeel()
{
    spriteSheet = juce::ImageCache::getFromMemory (BinaryData::MB_SLIDER_png,
                                                    BinaryData::MB_SLIDER_pngSize);
}

void SpriteFaderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float /*sliderPos*/, float /*minSliderPos*/, float /*maxSliderPos*/,
                                               const juce::Slider::SliderStyle /*style*/, juce::Slider& slider)
{
    if (spriteSheet.isNull())
    {
        // Fallback caso o sprite não tenha sido embutido corretamente.
        g.setColour (juce::Colours::darkgrey);
        g.fillRect (x, y, width, height);
        return;
    }

    const double prop = slider.valueToProportionOfLength (slider.getValue());

    int frame = juce::roundToInt ((1.0 - prop) * (double) (NUM_FRAMES - 1)); // frame 0 = topo = valor máximo
    frame = juce::jlimit (0, NUM_FRAMES - 1, frame);

    // Centraliza o sprite (escalado mantendo proporção, sem distorcer) dentro
    // da área do slider. A escala nunca passa de 1.0: o sprite tem só
    // 59x128px de resolução nativa, então ampliar além disso (ex: janela
    // maximizada, coluna bem maior que o slider) deixa a imagem borrada e
    // desproporcional -- em vez de esticar, sobra margem em volta, o que é
    // o efeito pedido ("centralizado com margem dos dois lados").
    const float scale = juce::jmin (1.0f, (float) width / (float) FRAME_W, (float) height / (float) FRAME_H);
    const float drawW = FRAME_W * scale;
    const float drawH = FRAME_H * scale;
    const float drawX = (float) x + ((float) width  - drawW) * 0.5f;
    const float drawY = (float) y + ((float) height - drawH) * 0.5f;

    g.drawImage (spriteSheet,
                juce::roundToInt (drawX), juce::roundToInt (drawY),
                juce::roundToInt (drawW), juce::roundToInt (drawH),   // destino (centralizado, escalado)
                0, frame * FRAME_H, FRAME_W, FRAME_H);                // origem (só o frame atual)
}

juce::Font SpriteFaderLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jlimit (12.0f, 15.0f, (float) buttonHeight * 0.6f)));
}
