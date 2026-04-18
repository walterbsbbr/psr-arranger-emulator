#include "MainComponent.h"

MainComponent::MainComponent()
{
    // ── Configurações de persistência ────────────────────────────────────────
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "PSREmulator";
    opts.filenameSuffix      = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (opts);

    // ── Conectar StyleEngine ao MidiRouter ───────────────────────────────────
    midiRouter.setStyleEngine (&styleEngine);

    // ── Botão de configuração ─────────────────────────────────────────────────
    btnSetup.setButtonText ("Setup...");
    btnSetup.onClick = [this] { openSetupDialog(); };
    addAndMakeVisible (btnSetup);

    // ── Painel de transporte: callback para carregar estilo ──────────────────
    transportPanel.onStyleFileRequested = [this] { loadStyleFile(); };

    // ── Adicionar painéis ─────────────────────────────────────────────────────
    addAndMakeVisible (transportPanel);
    addAndMakeVisible (stylePanel);
    addAndMakeVisible (mixerPanel);
    addAndMakeVisible (chordDisplay);

    // ── Tamanho da janela ─────────────────────────────────────────────────────
    setSize (820, 520);

    // ── Iniciar áudio ─────────────────────────────────────────────────────────
    setAudioChannels (0, 2); // sem entrada, 2 saídas

    // ── Restaurar estado anterior ─────────────────────────────────────────────
    restoreState();

    // ── Timer para atualizar ChordDisplay (50ms) ────────────────────────────
    startTimer (50);
}

MainComponent::~MainComponent()
{
    stopTimer();
    saveState();
    styleEngine.stop();
    midiRouter.closeMidiInput();
    shutdownAudio();
}

// ─── AudioAppComponent ────────────────────────────────────────────────────────
void MainComponent::prepareToPlay (int samplesPerBlock, double sampleRate)
{
    fluidSynth.prepareToPlay (sampleRate, samplesPerBlock);
    styleEngine.setBpm (styleEngine.getBpm());
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    if (info.buffer->getNumChannels() < 2) return;

    float* leftOut  = info.buffer->getWritePointer (0, info.startSample);
    float* rightOut = info.buffer->getWritePointer (1, info.startSample);

    fluidSynth.processAudio (leftOut, rightOut, info.numSamples);
}

void MainComponent::releaseResources()
{
    fluidSynth.releaseResources();
}

// ─── Timer: atualiza ChordDisplay a partir do ChordDetector ──────────────────
void MainComponent::timerCallback()
{
    chordDisplay.setChord (chordDetector.getCurrentChord());
}

// ─── Layout ──────────────────────────────────────────────────────────────────
void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (4);

    // Topo: Transport + Chord Display + Setup
    {
        auto top = area.removeFromTop (110);
        btnSetup.setBounds    (top.removeFromRight (90).withHeight (32).withY (top.getY() + 4));
        chordDisplay.setBounds(top.removeFromRight (160).reduced (4));
        transportPanel.setBounds (top);
    }
    area.removeFromTop (4);

    // Meio: Style Panel (botões de seção)
    stylePanel.setBounds (area.removeFromTop (200));
    area.removeFromTop (4);

    // Base: Mixer
    mixerPanel.setBounds (area);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a1a));

    // Título
    g.setColour (juce::Colours::white);
    g.setFont   (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText  ("PSR ARRANGER EMULATOR",
                 getLocalBounds().removeFromTop (20),
                 juce::Justification::centred);
}

// ─── Carregamento de estilo ───────────────────────────────────────────────────
void MainComponent::loadStyleFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Selecione um arquivo STY",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.sty;*.STY");

    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (!file.existsAsFile()) return;

            if (styleEngine.loadStyle (file))
            {
                const auto& sty = styleEngine.getStyle();
                int numSections = 0;
                for (auto& s : sty.sections)
                    if (s.exists) ++numSections;

                transportPanel.setStyleName (
                    sty.name + " [" + juce::String (numSections) + " sec, "
                    + juce::String (sty.defaultBpm, 0) + " BPM]");
            }
            else
            {
                transportPanel.setStyleName ("ERRO: " + file.getFileName());
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Erro ao carregar estilo",
                    "Nao foi possivel carregar:\n" + file.getFullPathName()
                    + "\n\nVerifique se o arquivo e um STY valido (MIDI com MThd/MTrk).");
            }
        });
}

void MainComponent::openSetupDialog()
{
    SetupDialog::show (this, midiRouter, sfManager, chordDetector);
}

// ─── Persistência ─────────────────────────────────────────────────────────────
void MainComponent::saveState()
{
    if (auto* props = appProperties.getUserSettings())
    {
        sfManager.saveState (*props);
        props->setValue ("splitPoint",   midiRouter.getSplitPoint());
        props->setValue ("bpm",          styleEngine.getBpm());
        props->save();
    }
}

void MainComponent::restoreState()
{
    if (auto* props = appProperties.getUserSettings())
    {
        sfManager.restoreState (*props);
        midiRouter.setSplitPoint  (props->getIntValue  ("splitPoint", 54));
        styleEngine.setBpm        (props->getDoubleValue ("bpm",      120.0));
    }
}
