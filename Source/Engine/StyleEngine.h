#pragma once
#include <JuceHeader.h>
#include "../STY/StyFile.h"
#include "../STY/StyParser.h"
#include "LoopPlayer.h"
#include "TransposeEngine.h"
#include "ChordDetector.h"
#include "MidiRouter.h"
#include "../SoundFont/FluidSynthEngine.h"
#include <atomic>

/**
 * StyleEngine
 *
 * Motor principal do arranjador. Coordena:
 *  - Carregamento e troca de arquivos STY
 *  - Máquina de estados (Idle → Intro → Main → Fill → Ending)
 *  - Roteamento CASM: canal de origem → canal de destino
 *  - Transposição em tempo real via TransposeEngine
 *  - Controle de volume e mute por parte
 *
 * Máquina de estados:
 *   IDLE ──[start]──▶ INTRO ──[fim]──▶ MAIN (loop)
 *                       ↑               │
 *                       │         [fill]▼
 *                       │          FILL ──[fim]──▶ MAIN
 *                       │
 *                    [ending]
 *                       ▼
 *                    ENDING ──[fim]──▶ IDLE
 */
class StyleEngine
{
public:
    enum class State { Idle, Intro, Main, Fill, Ending };

    StyleEngine (FluidSynthEngine& synth, MidiRouter& router);
    ~StyleEngine();

    // ── Carregamento de estilo ────────────────────────────────────────────────
    bool loadStyle   (const juce::File& styFile);
    void unloadStyle ();
    bool isStyleLoaded() const noexcept { return currentSty.valid; }
    const StyFile& getStyle() const     { return currentSty; }

    // ── Controle de transporte ────────────────────────────────────────────────
    void start    ();       // Toca Intro A (se existir) ou Main A diretamente
    void stop     ();
    void tapTempo ();

    // ── Seleção de seções ─────────────────────────────────────────────────────
    void selectIntro   (int ab); // 0=A, 1=B, 2=C
    void selectMain    (int ab); // 0=A, 1=B, 2=C, 3=D — enfileira para próximo compasso
    void selectFill    (int ab); // 0=AA, 1=AB, 2=BA, 3=BB
    void selectEnding  (int ab); // 0=A, 1=B, 2=C — enfileira para próximo compasso

    // ── Acorde (chamado pelo MidiRouter quando a mão esquerda muda) ───────────
    void onChordChanged (const ChordInfo& chord);

    // ── BPM e transpose ──────────────────────────────────────────────────────
    void   setBpm       (double bpm);
    double getBpm       () const noexcept { return currentBpm; }
    void   setTranspose (int semitones);  // -12 a +12
    int    getTranspose () const noexcept { return transposeOffset; }

    // ── Mixer: volume e mute por canal de destino (0-7 = ch9-16) ────────────
    void setPartVolume (int destCh, uint8_t volume);   // 0-127
    void setPartMuted  (int destCh, bool mute);
    uint8_t getPartVolume (int destCh) const noexcept  { return partVolume[destCh]; }
    bool    isPartMuted   (int destCh) const noexcept  { return partMuted[destCh];  }

    State getState() const noexcept { return state; }
    int   getActiveMainIndex() const noexcept { return activeMainIdx; }

private:
    void onMidiFromStyle  (const juce::MidiMessage& rawMsg);
    void onSectionEnded   (StyleSection which);
    void transitionToMain (int idx);

    // Mapeia canal de origem para canal de destino via CASM
    // Retorna -1 se o canal não deve ser reproduzido
    int resolveDestChannel (int sourceCh) const;

    FluidSynthEngine& synthEngine;
    MidiRouter&       midiRouter;

    StyFile           currentSty;
    ChordInfo         currentChord;

    std::unique_ptr<LoopPlayer> player;

    State         state           { State::Idle };
    int           activeMainIdx   { 0 };   // 0=A,1=B,2=C,3=D
    double        currentBpm      { 120.0 };
    int           transposeOffset { 0 };

    std::array<uint8_t, 8> partVolume {};
    std::array<bool,    8> partMuted  {};

    // Último acorde detectado (atualizado do message thread)
    juce::CriticalSection chordLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StyleEngine)
};
