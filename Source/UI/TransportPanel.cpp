#include "TransportPanel.h"

TransportPanel::TransportPanel (StyleEngine& engine) : styleEngine (engine)
{
    lblBpm.setText ("BPM", juce::dontSendNotification);
    addAndMakeVisible (lblBpm);

    sliderBpm.setRange (20, 300, 1);
    sliderBpm.setValue (120, juce::dontSendNotification);
    sliderBpm.setSliderStyle (juce::Slider::IncDecButtons);
    sliderBpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 55, 24);
    sliderBpm.onValueChange = [this] {
        styleEngine.setBpm (sliderBpm.getValue());
    };
    addAndMakeVisible (sliderBpm);

    lblTranspose.setText ("Transpose", juce::dontSendNotification);
    addAndMakeVisible (lblTranspose);

    sliderTranspose.setRange (-12, 12, 1);
    sliderTranspose.setValue (0, juce::dontSendNotification);
    sliderTranspose.setSliderStyle (juce::Slider::IncDecButtons);
    sliderTranspose.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);
    sliderTranspose.onValueChange = [this] {
        styleEngine.setTranspose ((int)sliderTranspose.getValue());
    };
    addAndMakeVisible (sliderTranspose);

    btnLoadStyle.setButtonText ("Load Style...");
    btnLoadStyle.onClick = [this] {
        if (onStyleFileRequested) onStyleFileRequested();
    };
    addAndMakeVisible (btnLoadStyle);

    lblStyleName.setText ("(nenhum estilo carregado)", juce::dontSendNotification);
    lblStyleName.setJustificationType (juce::Justification::centredLeft);
    lblStyleName.setColour (juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible (lblStyleName);
}

void TransportPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    const int h = 28;

    // Linha 1: BPM slider
    {
        auto row = area.removeFromTop (h);
        lblBpm.setBounds (row.removeFromLeft (40));
        sliderBpm.setBounds (row.removeFromLeft (130));
    }
    area.removeFromTop (4);

    // Linha 2: Transpose
    {
        auto row = area.removeFromTop (h);
        lblTranspose.setBounds (row.removeFromLeft (70));
        sliderTranspose.setBounds (row.removeFromLeft (110));
    }
    area.removeFromTop (4);

    // Linha 3: Load Style + nome
    {
        auto row = area.removeFromTop (h);
        btnLoadStyle.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (6);
        lblStyleName.setBounds (row);
    }
}

void TransportPanel::setStyleName (const juce::String& name)
{
    if (name.isEmpty())
        lblStyleName.setText ("(nenhum estilo carregado)", juce::dontSendNotification);
    else
        lblStyleName.setText (name, juce::dontSendNotification);
}

void TransportPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16213e));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}
