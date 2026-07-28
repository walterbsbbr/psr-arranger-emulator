#include "MixerPanel.h"

const char* MixerPanel::PART_NAMES[NUM_PARTS] = {
    "Ch 9", "Ch 10", "Ch 11", "Ch 12", "Ch 13", "Ch 14", "Ch 15", "Ch 16"
};

MixerPanel::MixerPanel (StyleEngine& engine) : styleEngine (engine)
{
    for (int i = 0; i < NUM_PARTS; ++i)
    {
        lblPart[i].setText (PART_NAMES[i], juce::dontSendNotification);
        lblPart[i].setJustificationType (juce::Justification::centred);
        lblPart[i].setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (lblPart[i]);

        lblProgram[i].setButtonText ("---");
        lblProgram[i].setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        lblProgram[i].setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        lblProgram[i].setColour (juce::TextButton::textColourOffId, juce::Colours::lightyellow);
        lblProgram[i].onClick = [this, i] { showPresetPicker (i, &lblProgram[i]); };
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

    startTimer (500);
}

void MixerPanel::timerCallback()
{
    auto& synth = styleEngine.getSynthEngine();
    for (int i = 0; i < NUM_PARTS; ++i)
    {
        int ch = FIRST_STYLE_CH + i; // canais 8-15 (0-indexed)
        auto name = synth.getChannelPresetName (ch);
        if (name.isEmpty())
            name = "---";
        else if (name.length() > 12)
            name = name.substring (0, 12);

        if (lblProgram[i].getButtonText() != name)
            lblProgram[i].setButtonText (name);

        // Timbre travado manualmente: destaca em ciano em vez de amarelo.
        const bool locked = synth.isChannelOverridden (ch);
        lblProgram[i].setColour (juce::TextButton::textColourOffId,
                                 locked ? juce::Colours::cyan : juce::Colours::lightyellow);
    }
}

void MixerPanel::showPresetPicker (int i, juce::Component* anchor)
{
    const int ch = FIRST_STYLE_CH + i; // 0-indexed
    auto& synth = styleEngine.getSynthEngine();
    auto presets = synth.listPresets();

    juce::PopupMenu menu;
    menu.addItem (1, "Seguir o estilo (auto)", true, !synth.isChannelOverridden (ch));
    menu.addSeparator();

    if (presets.empty())
    {
        menu.addItem (0, "(carregue um SoundFont primeiro)", false);
    }
    else
    {
        int itemId = 100;
        int lastBank = -1;
        juce::PopupMenu bankMenu;
        for (auto& p : presets)
        {
            if (p.bank != lastBank)
            {
                if (lastBank != -1)
                    menu.addSubMenu ("Bank " + juce::String (lastBank), bankMenu);
                bankMenu = juce::PopupMenu();
                lastBank = p.bank;
            }
            bankMenu.addItem (itemId++, juce::String (p.program) + "  " + p.name);
        }
        if (lastBank != -1)
            menu.addSubMenu ("Bank " + juce::String (lastBank), bankMenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor),
        [this, ch, presets] (int result)
        {
            if (result == 0) return;
            auto& synth2 = styleEngine.getSynthEngine();
            if (result == 1)
            {
                synth2.clearChannelPresetOverride (ch);
                return;
            }
            const int idx = result - 100;
            if (idx >= 0 && idx < (int) presets.size())
                synth2.setChannelPresetOverride (ch, presets[(size_t) idx].bank, presets[(size_t) idx].program);
        });
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
