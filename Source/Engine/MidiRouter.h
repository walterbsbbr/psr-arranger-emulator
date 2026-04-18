#pragma once
#include <JuceHeader.h>
#include "ChordDetector.h"
#include "../SoundFont/FluidSynthEngine.h"

class StyleEngine; // forward declaration

/**
 * MidiRouter
 *
 * Ponto central de roteamento de MIDI.
 * Recebe mensagens do MIDI IN (teclado externo) e do StyleEngine (arquivo STY).
 *
 * Divisão do teclado (split point configurável):
 *   nota < splitPoint → mão esquerda → ChordDetector → StyleEngine
 *   nota ≥ splitPoint → mão direita  → FluidSynth canal 0 (voz principal)
 *
 * Mensagens do STY: roteadas para FluidSynth com lógica de fallback de banco.
 */
class MidiRouter : public juce::MidiInputCallback
{
public:
    MidiRouter (FluidSynthEngine& synth, ChordDetector& chords);
    ~MidiRouter() override;

    void setStyleEngine (StyleEngine* engine) { styleEngine = engine; }

    // ── MIDI Input device ────────────────────────────────────────────────────
    bool openMidiInput  (const juce::MidiDeviceInfo& device);
    void closeMidiInput();
    juce::String getOpenedDeviceName() const { return openedDeviceName; }

    // ── Split point ──────────────────────────────────────────────────────────
    void setSplitPoint (int midiNote) { splitPoint = midiNote; }
    int  getSplitPoint() const        { return splitPoint; }

    // ── Mensagens vindas do arquivo STY (chamado pelo StyleEngine) ───────────
    void routeStyleMessage (const juce::MidiMessage& msg);

    // ── Estado de Bank Select por canal (usado pelo routeStyleMessage) ───────
    void resetBankState();

    // ── Canal da voz principal (mão direita) ─────────────────────────────────
    void setMelodyChannel (int ch1indexed) { melodyChannel = ch1indexed; }
    int  getMelodyChannel() const          { return melodyChannel; }

private:
    // juce::MidiInputCallback
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& msg) override;

    FluidSynthEngine& synthEngine;
    ChordDetector&    chordDetector;
    StyleEngine*      styleEngine { nullptr };

    std::unique_ptr<juce::MidiInput> midiInput;
    juce::String openedDeviceName;

    int splitPoint    { 54 };   // F#3 = MIDI 54
    int melodyChannel { 1  };   // canal 1 = voz principal

    // Estado de Bank Select por canal (para STY Program Changes)
    std::array<int, 16> styBankMsb {};
    std::array<int, 16> styBankLsb {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRouter)
};
