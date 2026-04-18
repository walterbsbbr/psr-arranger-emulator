#include "SetupDialog.h"

SetupDialog::SetupDialog (MidiRouter& router,
                          SoundFontManager& sfMgr,
                          ChordDetector& detector)
    : midiRouter (router), sfManager (sfMgr), chordDetector (detector)
{
    setSize (460, 300);

    // ── MIDI IN ───────────────────────────────────────────────────────────
    lblMidi.setText ("MIDI Input:", juce::dontSendNotification);
    addAndMakeVisible (lblMidi);

    comboMidi.onChange = [this] {
        auto devices = juce::MidiInput::getAvailableDevices();
        const int idx = comboMidi.getSelectedItemIndex();
        if (idx >= 0 && idx < devices.size())
            midiRouter.openMidiInput (devices[idx]);
    };
    addAndMakeVisible (comboMidi);

    btnRefreshMidi.setButtonText ("Refresh");
    btnRefreshMidi.onClick = [this] { refreshMidiDevices(); };
    addAndMakeVisible (btnRefreshMidi);

    // ── SoundFont ─────────────────────────────────────────────────────────
    lblSf.setText ("SoundFont:", juce::dontSendNotification);
    addAndMakeVisible (lblSf);

    lblSfName.setText (sfManager.isLoaded() ? sfManager.getCurrentName()
                                            : "(nenhum)", juce::dontSendNotification);
    lblSfName.setColour (juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible (lblSfName);

    btnLoadSf.setButtonText ("Carregar .sf2...");
    btnLoadSf.onClick = [this] {
        sfManager.openFileChooser (this, [this] (bool loaded) {
            lblSfName.setText (loaded ? sfManager.getCurrentName()
                                      : "(erro ao carregar)",
                               juce::dontSendNotification);
        });
    };
    addAndMakeVisible (btnLoadSf);

    // ── Split Point ───────────────────────────────────────────────────────
    lblSplit.setText ("Split Point:", juce::dontSendNotification);
    addAndMakeVisible (lblSplit);

    sliderSplit.setRange (24, 96, 1);
    sliderSplit.setValue (54, juce::dontSendNotification); // F#3
    sliderSplit.setSliderStyle (juce::Slider::IncDecButtons);
    sliderSplit.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 24);
    sliderSplit.textFromValueFunction = [] (double val) {
        return ChordDetector::noteName ((int)val);
    };
    sliderSplit.onValueChange = [this] {
        midiRouter.setSplitPoint ((int)sliderSplit.getValue());
    };
    addAndMakeVisible (sliderSplit);

    // ── Chord Mode ────────────────────────────────────────────────────────
    lblChordMode.setText ("Chord Mode:", juce::dontSendNotification);
    addAndMakeVisible (lblChordMode);

    comboChordMode.addItem ("Single Finger",   1);
    comboChordMode.addItem ("Fingered",        2);
    comboChordMode.addItem ("Fingered on Bass",3);
    comboChordMode.addItem ("Full Keyboard",   4);
    comboChordMode.setSelectedId (2, juce::dontSendNotification);
    comboChordMode.onChange = [this] {
        chordDetector.setMode (static_cast<ChordMode> (comboChordMode.getSelectedId() - 1));
    };
    addAndMakeVisible (comboChordMode);

    refreshMidiDevices();
}

void SetupDialog::refreshMidiDevices()
{
    comboMidi.clear();
    auto devices = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
        comboMidi.addItem (devices[i].name, i + 1);

    // Seleciona o dispositivo já aberto, se existir
    const auto openedName = midiRouter.getOpenedDeviceName();
    for (int i = 0; i < devices.size(); ++i)
        if (devices[i].name == openedName)
            comboMidi.setSelectedId (i + 1, juce::dontSendNotification);
}

void SetupDialog::resized()
{
    auto area = getLocalBounds().reduced (12);
    const int rowH = 32;
    const int gap  = 8;
    const int lblW = 100;

    auto row = [&] { auto r = area.removeFromTop (rowH); area.removeFromTop (gap); return r; };

    // MIDI IN row
    {
        auto r = row();
        lblMidi.setBounds       (r.removeFromLeft (lblW));
        btnRefreshMidi.setBounds(r.removeFromRight (70));
        comboMidi.setBounds     (r.reduced (2, 0));
    }

    // SoundFont row
    {
        auto r = row();
        lblSf.setBounds         (r.removeFromLeft (lblW));
        btnLoadSf.setBounds     (r.removeFromLeft (140));
        r.removeFromLeft (6);
        lblSfName.setBounds     (r);
    }

    // Split point row
    {
        auto r = row();
        lblSplit.setBounds      (r.removeFromLeft (lblW));
        sliderSplit.setBounds   (r.removeFromLeft (180));
    }

    // Chord mode row
    {
        auto r = row();
        lblChordMode.setBounds  (r.removeFromLeft (lblW));
        comboChordMode.setBounds(r.removeFromLeft (180));
    }
}

void SetupDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16213e));
    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("Configurações", getLocalBounds().removeFromTop (28), juce::Justification::centred);
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void SetupDialog::show (juce::Component* parent,
                        MidiRouter& router,
                        SoundFontManager& sfMgr,
                        ChordDetector& detector)
{
    auto dialog = std::make_unique<SetupDialog> (router, sfMgr, detector);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (dialog.release());
    opts.dialogTitle    = "Configurações do PSR Emulator";
    opts.dialogBackgroundColour = juce::Colour (0xff16213e);
    opts.resizable      = false;
    opts.useNativeTitleBar = true;
    opts.launchAsync();
}
