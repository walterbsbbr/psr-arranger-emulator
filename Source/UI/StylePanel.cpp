#include "StylePanel.h"

StylePanel::StylePanel (StyleEngine& engine) : styleEngine (engine)
{
    buildButtons();
    startTimer (100); // atualiza estado dos botões a cada 100ms
}

StylePanel::~StylePanel() { stopTimer(); }

void StylePanel::buildButtons()
{
    // ── Intro ─────────────────────────────────────────────────────────────
    const char* introLabels[] = { "A", "B", "C" };
    for (int i = 0; i < 3; ++i)
    {
        btnIntro[i].setButtonText (juce::String ("Intro ") + introLabels[i]);
        btnIntro[i].onClick = [this, i] { styleEngine.selectIntro (i); };
        addAndMakeVisible (btnIntro[i]);
    }

    // ── Main ──────────────────────────────────────────────────────────────
    const char* mainLabels[] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i)
    {
        btnMain[i].setButtonText (juce::String ("Main ") + mainLabels[i]);
        btnMain[i].onClick = [this, i] { styleEngine.selectMain (i); };
        addAndMakeVisible (btnMain[i]);
    }

    // ── Fill In ───────────────────────────────────────────────────────────
    const char* fillLabels[] = { "AA", "AB", "BA", "BB" };
    for (int i = 0; i < 4; ++i)
    {
        btnFill[i].setButtonText (juce::String ("Fill ") + fillLabels[i]);
        btnFill[i].onClick = [this, i] { styleEngine.selectFill (i); };
        addAndMakeVisible (btnFill[i]);
    }

    // ── Ending ────────────────────────────────────────────────────────────
    const char* endingLabels[] = { "A", "B", "C" };
    for (int i = 0; i < 3; ++i)
    {
        btnEnding[i].setButtonText (juce::String ("Ending ") + endingLabels[i]);
        btnEnding[i].onClick = [this, i] { styleEngine.selectEnding (i); };
        addAndMakeVisible (btnEnding[i]);
    }

    // ── Rótulos de grupo ──────────────────────────────────────────────────
    lblIntro.setText  ("INTRO",  juce::dontSendNotification);
    lblMain.setText   ("MAIN",   juce::dontSendNotification);
    lblFill.setText   ("FILL",   juce::dontSendNotification);
    lblEnding.setText ("ENDING", juce::dontSendNotification);
    for (auto* l : { &lblIntro, &lblMain, &lblFill, &lblEnding })
    {
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, juce::Colours::grey);
        l->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*l);
    }

    // ── Start/Stop ────────────────────────────────────────────────────────
    btnStartStop.setButtonText ("START");
    btnStartStop.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    btnStartStop.onClick = [this] {
        if (styleEngine.getState() == StyleEngine::State::Idle)
        {
            styleEngine.start();
            btnStartStop.setButtonText ("STOP");
            btnStartStop.setColour (juce::TextButton::buttonColourId, juce::Colours::darkred);
        }
        else
        {
            styleEngine.stop();
            btnStartStop.setButtonText ("START");
            btnStartStop.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgreen);
        }
    };
    addAndMakeVisible (btnStartStop);
}

// Layout em grade: todas as linhas com a mesma altura, botões de cada linha
// dividindo igualmente a largura disponível (em vez de largura fixa por
// botão) -- assim toda linha ocupa a largura toda e nenhum botão fica maior
// ou menor que os outros da mesma fileira. Um rótulo fixo à esquerda
// identifica cada grupo (INTRO/MAIN/FILL/ENDING).
void StylePanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    const int labelW    = 56;
    const int gap       = 6;
    const int startStopH = 44;

    const int rowsAvailable = 4;
    const int totalGapH = gap * (rowsAvailable - 1) + gap * 2 /* antes do Start/Stop */;
    const int rowH = (area.getHeight() - startStopH - totalGapH) / rowsAvailable;

    auto layoutRow = [&] (juce::Label& label, juce::TextButton* buttons, int count)
    {
        auto row = area.removeFromTop (rowH);
        label.setBounds (row.removeFromLeft (labelW));
        row.removeFromLeft (gap);

        const int btnW = (row.getWidth() - gap * (count - 1)) / count;
        for (int i = 0; i < count; ++i)
        {
            buttons[i].setBounds (row.removeFromLeft (btnW));
            if (i < count - 1) row.removeFromLeft (gap);
        }
        area.removeFromTop (gap);
    };

    layoutRow (lblIntro,  btnIntro,  3);
    layoutRow (lblMain,   btnMain,   4);
    layoutRow (lblFill,   btnFill,   4);
    layoutRow (lblEnding, btnEnding, 3);

    area.removeFromTop (gap);
    btnStartStop.setBounds (area.removeFromTop (startStopH));
}

void StylePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void StylePanel::timerCallback()
{
    // Ilumina apenas o botão Main da variação ativa
    const auto state = styleEngine.getState();
    const int  activeMain = styleEngine.getActiveMainIndex();
    for (int i = 0; i < 4; ++i)
    {
        bool isActive = (state == StyleEngine::State::Main && i == activeMain);
        btnMain[i].setColour (juce::TextButton::buttonColourId,
                              isActive ? juce::Colour (0xff005500)
                                       : juce::Colour (0xff2a2a3e));
    }

    // Atualiza Start/Stop label
    if (state == StyleEngine::State::Idle)
        btnStartStop.setButtonText ("START");
    else
        btnStartStop.setButtonText ("STOP");
}
