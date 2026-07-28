#include "MixerPanel.h"
#include <cstdlib>

const char* MixerPanel::PART_NAMES[NUM_PARTS] = {
    "Ch 9", "Ch 10", "Ch 11", "Ch 12", "Ch 13", "Ch 14", "Ch 15", "Ch 16"
};

MixerPanel::MixerPanel (StyleEngine& engine) : styleEngine (engine)
{
    // Aplica ao painel inteiro (não só ao slider): também padroniza a fonte
    // de todos os TextButton filhos (nome do instrumento, +/-, S, M), que
    // senão calculariam um tamanho minúsculo a partir da altura baixa das
    // linhas do Mixer.
    setLookAndFeel (&spriteFaderLnf);

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        // Mesmo tamanho de fonte usado nos rótulos dos outros painéis (Style/
        // Transport) -- antes ficava menor porque as linhas do Mixer são
        // baixas e os botões calculam a fonte a partir da própria altura.
        lblPart[i].setText (PART_NAMES[i], juce::dontSendNotification);
        lblPart[i].setJustificationType (juce::Justification::centred);
        lblPart[i].setFont (juce::FontOptions (14.0f, juce::Font::bold));
        addAndMakeVisible (lblPart[i]);

        // Transpose de oitava por parte: "-" à esquerda, "+" à direita do rótulo.
        // Cor indica o deslocamento atual: neutro=0, verde=±1 oitava, laranja=±2.
        btnOctaveDown[i].setButtonText ("-");
        btnOctaveDown[i].onClick = [this, i] {
            styleEngine.setPartOctaveShift (i, styleEngine.getPartOctaveShift (i) - 1);
            refreshOctaveButtonColours (i);
        };
        addAndMakeVisible (btnOctaveDown[i]);

        btnOctaveUp[i].setButtonText ("+");
        btnOctaveUp[i].onClick = [this, i] {
            styleEngine.setPartOctaveShift (i, styleEngine.getPartOctaveShift (i) + 1);
            refreshOctaveButtonColours (i);
        };
        addAndMakeVisible (btnOctaveUp[i]);

        refreshOctaveButtonColours (i);

        lblProgram[i].setButtonText ("---");
        lblProgram[i].setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        lblProgram[i].setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        lblProgram[i].setColour (juce::TextButton::textColourOffId, juce::Colours::lightyellow);
        lblProgram[i].onClick = [this, i] { showPresetPicker (i, &lblProgram[i]); };
        addAndMakeVisible (lblProgram[i]);

        sliderVol[i].setRange (0, 127, 1);
        sliderVol[i].setValue (100, juce::dontSendNotification);
        sliderVol[i].setSliderStyle (juce::Slider::LinearVertical);
        sliderVol[i].setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sliderVol[i].setLookAndFeel (&spriteFaderLnf);
        sliderVol[i].onValueChange = [this, i] {
            styleEngine.setPartVolume (i, (uint8_t)sliderVol[i].getValue());
        };
        addAndMakeVisible (sliderVol[i]);

        btnSolo[i].setButtonText ("S");
        btnSolo[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a3e));
        btnSolo[i].onClick = [this, i] {
            const bool nowSoloed = !styleEngine.isPartSoloed (i);
            styleEngine.setPartSoloed (i, nowSoloed);
            btnSolo[i].setColour (juce::TextButton::buttonColourId,
                                  nowSoloed ? juce::Colours::green
                                            : juce::Colour (0xff2a2a3e));
        };
        addAndMakeVisible (btnSolo[i]);

        btnMute[i].setButtonText ("M");
        btnMute[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a3e));
        btnMute[i].onClick = [this, i] {
            const bool nowMuted = !styleEngine.isPartMuted (i);
            styleEngine.setPartMuted (i, nowMuted);
            btnMute[i].setColour (juce::TextButton::buttonColourId,
                                  nowMuted ? juce::Colours::green
                                           : juce::Colour (0xff2a2a3e));
        };
        addAndMakeVisible (btnMute[i]);
    }

    startTimer (500);
}

MixerPanel::~MixerPanel()
{
    stopTimer();
    for (int i = 0; i < NUM_PARTS; ++i)
        sliderVol[i].setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
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

void MixerPanel::refreshOctaveButtonColours (int i)
{
    const int shift = styleEngine.getPartOctaveShift (i);
    juce::Colour colour;
    switch (std::abs (shift))
    {
        case 0:  colour = juce::Colour (0xff2a2a3e); break;   // neutro
        case 1:  colour = juce::Colours::green;      break;
        default: colour = juce::Colours::orange;     break;   // 2 (máximo)
    }
    btnOctaveDown[i].setColour (juce::TextButton::buttonColourId, colour);
    btnOctaveUp[i]  .setColour (juce::TextButton::buttonColourId, colour);
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

    // Cada faixa de canal tem uma largura FIXA (não estica com a janela) --
    // sobra de espaço numa coluna larga vira margem dos dois lados em vez de
    // esticar rótulos/botões/fader desproporcionalmente. Mesma lógica do
    // limite de escala do fader (SpriteFaderLookAndFeel): sobra = margem,
    // nunca distorção.
    const int stripW = juce::jlimit (72, 110, colW - 8);

    const int lblH  = 22;
    const int progH = 22;
    const int muteH = 26;
    const int gapH  = 6;

    // Altura do fader também tem um teto: o sprite nativo tem só 128px, então
    // deixar essa área crescer com a janela (como antes) sobrava uma caixa
    // enorme em volta de um desenho pequeno e travado -- tão desproporcional
    // quanto esticar a imagem. 170px dá uma folga pequena acima/abaixo do
    // sprite sem deixar vazio excessivo.
    const int maxVolH = 170;

    for (int i = 0; i < NUM_PARTS; ++i)
    {
        auto fullCol = getLocalBounds().removeFromLeft (colW).withX (i * colW);

        const int availableForVol = fullCol.getHeight() - lblH - progH - muteH - gapH * 3;
        const int volH = juce::jlimit (60, maxVolH, availableForVol);
        const int contentH = lblH + gapH + progH + gapH + volH + gapH + muteH;

        // Centraliza a faixa inteira (rótulo+programa+fader+botões) dentro da
        // coluna, nos dois eixos -- se sobrar altura (janela grande), vira
        // margem acima/abaixo da faixa toda, não esticamento do fader.
        auto col = fullCol.withSizeKeepingCentre (stripW, juce::jmin (contentH, fullCol.getHeight()));

        auto partRow = col.removeFromTop (lblH);
        const int octBtnW = juce::jmin (20, partRow.getHeight() + 4);
        btnOctaveDown[i].setBounds (partRow.removeFromLeft  (octBtnW));
        btnOctaveUp[i]  .setBounds (partRow.removeFromRight (octBtnW));
        lblPart[i]      .setBounds (partRow);

        col.removeFromTop (gapH);
        lblProgram[i].setBounds (col.removeFromTop (progH));
        col.removeFromTop (gapH);
        sliderVol[i] .setBounds (col.removeFromTop (volH));
        col.removeFromTop (gapH);

        auto muteRow = col.removeFromTop (muteH);
        const int gap  = 6;
        const int btnW = (muteRow.getWidth() - gap) / 2;
        btnSolo[i].setBounds (muteRow.removeFromLeft (btnW));
        muteRow.removeFromLeft (gap);
        btnMute[i].setBounds (muteRow);
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
