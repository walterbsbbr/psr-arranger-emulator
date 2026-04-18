#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <set>

/**
 * Tipos de acorde reconhecidos pelo detector.
 * A ordem importa para exibição e para lookup no TransposeEngine.
 */
enum class ChordType
{
    Major = 0,
    Minor,
    Dominant7,
    Major7,
    Minor7,
    Minor7b5,       // m7♭5 / half-diminished
    Diminished,
    Diminished7,
    Augmented,
    Sus2,
    Sus4,
    Add9,
    MinorAdd9,
    Count           // sentinela — número de tipos
};

/** Informação do acorde detectado */
struct ChordInfo
{
    int       root     { 0 };              // 0=C, 1=C#/Db, ..., 11=B
    ChordType type     { ChordType::Major };
    int       bassNote { 0 };              // nota de baixo (pode diferir do root em inversões)
    bool      valid    { false };          // false se não foi detectado acorde

    /** Retorna string legível: "Cm7", "F#maj7", etc. */
    juce::String toString() const;
};

/** Modo de detecção de acorde */
enum class ChordMode
{
    SingleFinger,       // 1 tecla=Maior, +branca=Menor, +preta=7ª
    Fingered,           // detecta pelas notas reais
    FingeredOnBass,     // Fingered + nota mais grave define o baixo
    FullKeyboard        // sem split, analisa todo o teclado
};

/**
 * ChordDetector
 *
 * Analisa as teclas pressionadas na mão esquerda e identifica o acorde.
 * Projetado para ser chamado do message thread (MIDI callback).
 */
class ChordDetector
{
public:
    ChordDetector();

    void setMode (ChordMode m) { mode = m; }
    ChordMode getMode() const  { return mode; }

    /** Adiciona uma nota pressionada (MIDI note 0-127) */
    void noteOn  (int midiNote);
    /** Remove uma nota liberada */
    void noteOff (int midiNote);
    /** Limpa todas as notas */
    void reset();

    /** Retorna o acorde atualmente detectado */
    ChordInfo getCurrentChord() const { return currentChord; }

    /** Nomes das notas para display */
    static juce::String noteName (int midiNote);
    static juce::String chordTypeName (ChordType t);

private:
    ChordInfo detect() const;
    ChordInfo detectFingered() const;
    ChordInfo detectSingleFinger() const;

    /** Testa se o conjunto de classes de notas (0-11) casa com um template */
    static int scoreMatch (const std::vector<int>& noteClasses,
                           int root,
                           const std::vector<int>& intervals);

    ChordMode mode { ChordMode::Fingered };
    std::set<int> heldNotes;   // notas MIDI pressionadas (valor real, não normalizado)
    ChordInfo     currentChord;

    // Tabela de intervalos por ChordType
    static const std::array<std::vector<int>, static_cast<int>(ChordType::Count)> intervalTable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordDetector)
};
