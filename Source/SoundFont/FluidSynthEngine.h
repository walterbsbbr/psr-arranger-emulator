#pragma once
#include <JuceHeader.h>
#include <fluidsynth.h>
#include <array>
#include <string>
#include <vector>

/** Um preset (instrumento) disponível no SoundFont carregado. */
struct SoundFontPresetInfo
{
    int bank;
    int program;
    juce::String name;
};

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

    /** Retorna o nome do preset carregado num canal (0-indexed), ou "" se vazio */
    juce::String getChannelPresetName (int ch) const;

    /** Retorna o número de programa ativo num canal (0-indexed) */
    int getChannelProgram (int ch) const noexcept { return programNum[ch]; }

    /** Retorna true se o canal foi configurado como drum bank (MSB=127) */
    bool isDrumBank (int ch) const noexcept { return ch >= 0 && ch < 16 && bankMsb[ch] == 127; }

    // ── Seleção manual de timbre (painel/mixer) ──────────────────────────────

    /** Lista todos os presets do SoundFont carregado, ordenados por banco/programa. */
    std::vector<SoundFontPresetInfo> listPresets() const;

    /**
     * Atribui explicitamente um preset (banco/programa exatos, vindos de
     * listPresets()) a um canal, ignorando qualquer fallback/cascata de banco.
     * Marca o canal como "travado": Program Changes vindos do arquivo STY
     * deixam de afetá-lo até clearChannelPresetOverride() ser chamado.
     */
    void setChannelPresetOverride (int channel, int bank, int program);

    /** Libera o canal para voltar a seguir os Program Changes do STY. */
    void clearChannelPresetOverride (int channel);

    /** Libera todos os canais (chamado ao carregar um novo estilo). */
    void clearAllPresetOverrides();

    bool isChannelOverridden (int ch) const noexcept { return ch >= 0 && ch < 16 && overridden[ch]; }

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
    std::array<bool, 16> overridden {};   // true = timbre travado manualmente

    juce::CriticalSection synthLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluidSynthEngine)
};
