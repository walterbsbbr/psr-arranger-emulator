#include "MixerPanel.h"

const char* MixerPanel::PART_NAMES[NUM_PARTS] = {
    "Sub-Rhy", "Rhythm", "Bass", "Chord1", "Chord2", "Pad", "Phr1", "Phr2"
};

MixerPanel::MixerPanel (StyleEngine& engine) : styleEngine (engine)
{
    for (int i = 0; i < NUM_PARTS; ++i)
    {
        lblPart[i].setText (PART_NAMES[i], juce::dontSendNotification);
        lblPart[i].setJustificationType (juce::Justification::centred);
        lblPart[i].setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (lblPart[i]);

        sliderVol[i].setRange (0, 127, 1);
        sliderVol[i].setValue (100, juce::dontSendNotification);
        sliderVol[i].setSliderStyle (juce::Slider::LinearBarVertical);
        sliderVol[i].setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sliderVol[i].onValueChange = [this, i] {
            styleEngine.setPartVolume (i, (uint8_t)sliderVol[i].getValue());
        };
        addAndMakeVisible (sliderVol[i]);

        btnMute[i].setButtonText ("M");
        btnMute[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a3e));
        btnMute[i].onClick = [this, i] {
            const bool nowMuted = !styleEngine.isPartMuted (i);
            styleEngine.setPartMuted (i, nowMuted);
            btnMute[i].setColour (juce::TextButton::buttonColourId,
                                  nowMuted ? juce::Colours::darkred
                                           : juce::Colour (0xff2a2a3e));
        };
        addAndMakeVisible (btnMute[i]);
    }
}

void MixerPanel::resized()
{
    const int colW  = getWidth() / NUM_PARTS;
    const int lblH  = 20;
    const int muteH = 24;
    const int volH  = getHeight() - lblH - muteH - 8;

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        auto col = getLocalBounds().removeFromLeft (colW).withX (i * colW).reduced (2, 0);
        lblPart[i] .setBounds (col.removeFromTop    (lblH));
        sliderVol[i].setBounds (col.removeFromTop   (volH));
        col.removeFromTop (4);
        btnMute[i] .setBounds (col.removeFromTop    (muteH));
    }
}

void MixerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d1a));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);

    // Linhas separadoras entre as partes
    const int colW = getWidth() / NUM_PARTS;
    g.setColour (juce::Colour (0xff2a2a3e));
    for (int i = 1; i < NUM_PARTS; ++i)
        g.drawVerticalLine (i * colW, 0.0f, (float)getHeight());
}
