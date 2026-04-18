#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiRouter.h"
#include "../SoundFont/SoundFontManager.h"

/**
 * SetupDialog
 *
 * Diálogo de configuração:
 *  - Seleção de dispositivo MIDI IN
 *  - Carregamento de SoundFont (.sf2)
 *  - Split Point (nota MIDI)
 *  - Modo de detecção de acorde
 */
class SetupDialog : public juce::Component
{
public:
    SetupDialog (MidiRouter& router,
                 SoundFontManager& sfManager,
                 ChordDetector& chordDetector);

    void resized() override;
    void paint (juce::Graphics& g) override;

    static void show (juce::Component* parent,
                      MidiRouter& router,
                      SoundFontManager& sfManager,
                      ChordDetector& detector);

private:
    void refreshMidiDevices();

    MidiRouter&       midiRouter;
    SoundFontManager& sfManager;
    ChordDetector&    chordDetector;

    // MIDI IN
    juce::Label           lblMidi;
    juce::ComboBox        comboMidi;
    juce::TextButton      btnRefreshMidi;

    // SoundFont
    juce::Label           lblSf;
    juce::Label           lblSfName;
    juce::TextButton      btnLoadSf;

    // Split point
    juce::Label           lblSplit;
    juce::Slider          sliderSplit;

    // Chord mode
    juce::Label           lblChordMode;
    juce::ComboBox        comboChordMode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SetupDialog)
};
