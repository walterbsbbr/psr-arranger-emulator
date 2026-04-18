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

void StylePanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    const int btnH  = 36;
    const int gap   = 4;

    // Linha 1: Intro
    {
        auto row = area.removeFromTop (btnH);
        for (int i = 0; i < 3; ++i)
            btnIntro[i].setBounds (row.removeFromLeft (90).reduced (gap / 2, 0));
    }
    area.removeFromTop (gap);

    // Linha 2: Main
    {
        auto row = area.removeFromTop (btnH);
        for (int i = 0; i < 4; ++i)
            btnMain[i].setBounds (row.removeFromLeft (80).reduced (gap / 2, 0));
    }
    area.removeFromTop (gap);

    // Linha 3: Fill
    {
        auto row = area.removeFromTop (btnH);
        for (int i = 0; i < 4; ++i)
            btnFill[i].setBounds (row.removeFromLeft (80).reduced (gap / 2, 0));
    }
    area.removeFromTop (gap);

    // Linha 4: Ending
    {
        auto row = area.removeFromTop (btnH);
        for (int i = 0; i < 3; ++i)
            btnEnding[i].setBounds (row.removeFromLeft (90).reduced (gap / 2, 0));
    }
    area.removeFromTop (gap * 2);

    // Linha 5: Start/Stop
    btnStartStop.setBounds (area.removeFromLeft (120).withHeight (btnH));
}

void StylePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void StylePanel::timerCallback()
{
    // Ilumina o botão Main ativo
    const auto state = styleEngine.getState();
    for (int i = 0; i < 4; ++i)
    {
        bool isActive = (state == StyleEngine::State::Main);
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
