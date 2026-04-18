#pragma once
#include <JuceHeader.h>
#include <fluidsynth.h>
#include <array>
#include <string>

/**
 * FluidSynthEngine
 *
 * Wrapper do FluidSynth rodando em modo "no-driver".
 * O JUCE AudioIOCallback chama processAudio() no audio thread.
 * Todas as mensagens MIDI devem ser enviadas via sendMidiMessage()
 * que é thread-safe (usa juce::CriticalSection).
 *
 * Suporte completo a 16 canais MIDI simultâneos com:
 * - Program Change + Bank Select MSB/LSB por canal
 * - Fallback automático de vozes XG/Mega Voice → GM
 * - SysEx GM reset
 */
class FluidSynthEngine
{
public:
    FluidSynthEngine();
    ~FluidSynthEngine();

    // Carrega um arquivo .sf2. Retorna true em sucesso.
    bool loadSoundFont (const juce::File& sf2File);
    void unloadSoundFont();
    bool isSoundFontLoaded() const noexcept { return soundFontId >= 0; }
    juce::String getLoadedSoundFontName() const { return loadedSfName; }

    // Deve ser chamado quando o sample rate muda (antes de processar áudio)
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

    // Chamado pelo AudioIOCallback no audio thread
    void processAudio (float* leftOut, float* rightOut, int numSamples);

    // Thread-safe: pode ser chamado de qualquer thread
    void sendMidiMessage (const juce::MidiMessage& msg);

    // Utilitários
    void allNotesOff();
    void resetAllControllers();

    double getSampleRate() const noexcept { return currentSampleRate; }

private:
    void applyProgramChange (int channel); // 0-indexed

    fluid_settings_t* settings { nullptr };
    fluid_synth_t*    synth    { nullptr };
    int               soundFontId { -1 };

    double currentSampleRate { 44100.0 };
    juce::String loadedSfName;

    // Estado de Bank Select por canal (0-indexed)
    std::array<int, 16> bankMsb {};
    std::array<int, 16> bankLsb {};
    std::array<int, 16> programNum {};

    juce::CriticalSection synthLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluidSynthEngine)
};
