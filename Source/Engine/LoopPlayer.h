#pragma once
#include <JuceHeader.h>
#include "../STY/StyFile.h"
#include <functional>
#include <atomic>

/**
 * LoopPlayer
 *
 * Toca uma MidiMessageSequence em loop (para seções Main) ou uma vez
 * (para Intro, Fill, Ending) usando juce::HighResolutionTimer.
 *
 * As mensagens MIDI geradas são entregues via callback onMidiEvent,
 * que deve ser thread-safe (é chamado do timer thread).
 *
 * Quantização: ao receber um pedido de troca de seção (setSectionQueued),
 * o LoopPlayer espera o fim do compasso atual antes de fazer a transição.
 */
class LoopPlayer : private juce::HighResolutionTimer
{
public:
    using MidiCallback = std::function<void(const juce::MidiMessage&)>;

    explicit LoopPlayer (MidiCallback callback);
    ~LoopPlayer() override;

    void prepareToPlay (double bpm, int ppq, double sampleRate);

    /** Inicia a reprodução da seção dada imediatamente */
    void playSection (const StyFile::SectionData& section,
                      StyleSection sectionId,
                      bool loop);

    /** Enfileira a próxima seção para ser tocada no fim do compasso atual */
    void queueSection  (const StyFile::SectionData& section,
                        StyleSection sectionId,
                        bool loop);

    void stop();
    void pause();
    void resume();

    bool isPlaying() const noexcept { return playing.load(); }
    StyleSection currentSection() const noexcept { return activeSection; }

    /** Callback chamado quando uma seção one-shot termina */
    std::function<void(StyleSection)> onSectionEnded;

    void setBpm (double bpm);
    double getBpm() const noexcept { return currentBpm; }

private:
    void hiResTimerCallback() override;

    void advancePlayhead (int64_t nowNs);
    void flushEvents (int64_t startNs, int64_t endNs);
    void loadSection (const StyFile::SectionData& sec, StyleSection id, bool loop);

    MidiCallback midiCallback;

    // ── Estado da reprodução ─────────────────────────────────────────────────
    juce::MidiMessageSequence currentEvents;
    int totalTicks  { 0 };
    int playheadTick{ 0 };
    int nextEventIdx{ 0 };
    bool looping    { true };
    StyleSection activeSection { StyleSection::Unknown };

    // ── Seção enfileirada ────────────────────────────────────────────────────
    struct QueuedSection {
        StyFile::SectionData data;
        StyleSection         id   { StyleSection::Unknown };
        bool                 loop { true };
        bool                 pending { false };
    };
    QueuedSection queued;
    juce::CriticalSection queueLock;

    // ── Timing ───────────────────────────────────────────────────────────────
    double currentBpm    { 120.0 };
    int    currentPpq    { 480   };
    double nsPerTick     { 0.0   };   // nanosegundos por tick
    int64_t lastCallNs   { 0     };

    std::atomic<bool> playing { false };
    std::atomic<bool> paused  { false };

    static constexpr int TIMER_INTERVAL_MS = 1; // 1ms de resolução

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopPlayer)
};
