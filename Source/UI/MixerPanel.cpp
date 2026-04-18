#include "MixerPanel.h"

const char* MixerPanel::PART_NAMES[NUM_PARTS] = {
    "Sub-Rhy", "Rhythm", "Bass", "Chord1", "Chord2", "Pad", "Phr1", "Phr2"
};

// Mapeamento Part → canal FluidSynth (0-indexed).
// Segue convenção PSR: channels 7-15 raw = JUCE 8-16.
// Part 0 = Sub-Rhy (ch7), Part 1 = Rhythm (ch8), Part 2 = Bass (ch10), etc.
const int MixerPanel::PART_FS_CHANNEL[NUM_PARTS] = {
    7,   // Sub-Rhy  → raw ch 7  (JUCE ch 8)
    8,   // Rhythm   → raw ch 8  (JUCE ch 9)  [drums]
    10,  // Bass     → raw ch 10 (JUCE ch 11)
    11,  // Chord1   → raw ch 11 (JUCE ch 12)
    12,  // Chord2   → raw ch 12 (JUCE ch 13)
    13,  // Pad      → raw ch 13 (JUCE ch 14)
    14,  // Phr1     → raw ch 14 (JUCE ch 15)
    15,  // Phr2     → raw ch 15 (JUCE ch 16)
};

MixerPanel::MixerPanel (StyleEngine& engine) : styleEngine (engine)
{
    for (int i = 0; i < NUM_PARTS; ++i)
    {
        lblPart[i].setText (PART_NAMES[i], juce::dontSendNotification);
        lblPart[i].setJustificationType (juce::Justification::centred);
        lblPart[i].setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (lblPart[i]);

        lblProgram[i].setText ("---", juce::dontSendNotification);
        lblProgram[i].setJustificationType (juce::Justification::centred);
        lblProgram[i].setFont (juce::FontOptions (9.0f));
        lblProgram[i].setColour (juce::Label::textColourId, juce::Colours::lightyellow);
        addAndMakeVisible (lblProgram[i]);

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

    startTimer (500); // atualiza nomes dos instrumentos a cada 500ms
}

void MixerPanel::timerCallback()
{
    auto& synth = styleEngine.getSynthEngine();
    for (int i = 0; i < NUM_PARTS; ++i)
    {
        int ch = PART_FS_CHANNEL[i];
        auto name = synth.getChannelPresetName (ch);
        if (name.isEmpty())
            name = "---";
        else if (name.length() > 12)
            name = name.substring (0, 12);

        if (lblProgram[i].getText() != name)
            lblProgram[i].setText (name, juce::dontSendNotification);
    }
}

void MixerPanel::resized()
{
    const int colW  = getWidth() / NUM_PARTS;
    const int lblH  = 16;
    const int progH = 14;
    const int muteH = 24;
    const int volH  = getHeight() - lblH - progH - muteH - 12;

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        auto col = getLocalBounds().removeFromLeft (colW).withX (i * colW).reduced (2, 0);
        lblPart[i]   .setBounds (col.removeFromTop (lblH));
        lblProgram[i].setBounds (col.removeFromTop (progH));
        col.removeFromTop (2);
        sliderVol[i] .setBounds (col.removeFromTop (volH));
        col.removeFromTop (4);
        btnMute[i]   .setBounds (col.removeFromTop (muteH));
    }
}

void MixerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d1a));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);

    const int colW = getWidth() / NUM_PARTS;
    g.setColour (juce::Colour (0xff2a2a3e));
    for (int i = 1; i < NUM_PARTS; ++i)
        g.drawVerticalLine (i * colW, 0.0f, (float)getHeight());
}
