#pragma once
#include <JuceHeader.h>
#include "../STY/StyFile.h"
#include "ChordDetector.h"
#include <array>

/**
 * TransposeEngine
 *
 * Responsável por transpor as notas dos canais de acompanhamento do STY
 * em função do acorde detectado pela mão esquerda e das regras CASM.
 *
 * Implementa as quatro regras NTR:
 *  BYPASS  - sem transposição (bateria/percussão)
 *  ROOT    - desloca todas as notas pelo intervalo root→nova_root
 *  GUITAR  - remapeia notas dentro das notas do acorde atual
 *  BASS    - transpõe para o baixo do acorde, mantendo registro
 *
 * E as tabelas NTT para GUITAR:
 *  MELODY  - preserva graus da escala maior
 *  CHORD   - usa apenas as notas do acorde
 */
class TransposeEngine
{
public:
    /**
     * Transpõe uma nota MIDI dada a regra CASM e o acorde atual.
     * A nota de origem foi gravada em CMaj7 (root=C=0, tipo Major7).
     *
     * @param note       Nota MIDI original (0-127)
     * @param chord      Acorde alvo detectado pela mão esquerda
     * @param ntr        Regra de transposição do canal
     * @param ntt        Tabela de transposição do canal
     * @param highKey    Nota máxima antes de oitavar para baixo
     * @param lowLimit   Limite inferior de nota
     * @param highLimit  Limite superior de nota
     * @return nota transposta (0-127), ou -1 para suprimir a nota
     */
    static int transposeNote (int note,
                              const ChordInfo& chord,
                              NTR ntr,
                              NTT ntt,
                              int highKey    = 127,
                              int lowLimit   = 0,
                              int highLimit  = 127);

    /**
     * Transpõe uma MidiMessage completa (cuida de note on/off,
     * pitchbend e retorna a mensagem modificada).
     * Retorna false se a nota deve ser suprimida (muteFlags).
     */
    static bool transposeMidiMessage (juce::MidiMessage& msg,
                                      const ChordInfo& chord,
                                      const CasmChannel& casmCh);

    /**
     * Verifica mute flags: retorna true se o canal deve ser silenciado
     * para o tipo de acorde atual.
     */
    static bool shouldMute (const CasmChannel& casmCh, const ChordInfo& chord);

private:
    // ── Implementações de cada NTR ───────────────────────────────────────────
    static int transposeRoot   (int note, const ChordInfo& chord);
    static int transposeGuitar (int note, const ChordInfo& chord, NTT ntt);
    static int transposeBass   (int note, const ChordInfo& chord);

    // ── Notas do acorde e da escala ──────────────────────────────────────────
    static std::vector<int> chordNotes  (const ChordInfo& chord);
    static std::vector<int> scaleNotes  (const ChordInfo& chord);

    // Notas do CMaj7 (fonte dos arquivos STY): C E G B
    static constexpr std::array<int,4> SOURCE_CHORD_NOTES = { 0, 4, 7, 11 };
    // Escala Maior de C: C D E F G A B
    static constexpr std::array<int,7> SOURCE_SCALE_NOTES = { 0, 2, 4, 5, 7, 9, 11 };
};
