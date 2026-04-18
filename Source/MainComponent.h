#pragma once
#include <JuceHeader.h>

#include "SoundFont/FluidSynthEngine.h"
#include "SoundFont/SoundFontManager.h"
#include "Engine/ChordDetector.h"
#include "Engine/MidiRouter.h"
#include "Engine/StyleEngine.h"
#include "UI/StylePanel.h"
#include "UI/TransportPanel.h"
#include "UI/MixerPanel.h"
#include "UI/ChordDisplay.h"
#include "UI/SetupDialog.h"

/**
 * MainComponent
 *
 * Componente raiz da aplicação. Implementa AudioIOCallback para
 * integrar o FluidSynth no pipeline de áudio do JUCE.
 *
 * Hierarquia de ownership:
 *   MainComponent
 *     ├─ FluidSynthEngine    (gerador de som)
 *     ├─ SoundFontManager    (carrega SF2)
 *     ├─ ChordDetector       (detecta acordes)
 *     ├─ MidiRouter          (divide teclado, roteia STY)
 *     ├─ StyleEngine         (motor do arranjador)
 *     └─ UI Panels
 */
class MainComponent : public juce::AudioAppComponent,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // ── AudioAppComponent ─────────────────────────────────────────────────────
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // ── juce::Component ──────────────────────────────────────────────────────
    void paint   (juce::Graphics& g) override;
    void resized () override;

private:
    void timerCallback() override;

    void loadStyleFile();
    void openSetupDialog();
    void saveState();
    void restoreState();

    // ── Subsistemas ──────────────────────────────────────────────────────────
    FluidSynthEngine  fluidSynth;
    SoundFontManager  sfManager   { fluidSynth };
    ChordDetector     chordDetector;
    MidiRouter        midiRouter  { fluidSynth, chordDetector };
    StyleEngine       styleEngine { fluidSynth, midiRouter };

    // ── UI ────────────────────────────────────────────────────────────────────
    TransportPanel    transportPanel { styleEngine };
    StylePanel        stylePanel     { styleEngine };
    MixerPanel        mixerPanel     { styleEngine };
    ChordDisplay      chordDisplay;

    juce::TextButton  btnSetup;

    // ── Persistência ─────────────────────────────────────────────────────────
    juce::ApplicationProperties appProperties;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
