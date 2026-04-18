#include "TransposeEngine.h"
#include <algorithm>
#include <cmath>

// ─── Intervalos do acorde alvo ────────────────────────────────────────────────
std::vector<int> TransposeEngine::chordNotes (const ChordInfo& chord)
{
    // Retorna as classes de nota (0-11) das notas do acorde alvo
    // usando a mesma tabela de intervalos do ChordDetector
    static const std::vector<std::vector<int>> intervals =
    {
        { 0, 4, 7       },  // Major
        { 0, 3, 7       },  // Minor
        { 0, 4, 7, 10   },  // Dominant7
        { 0, 4, 7, 11   },  // Major7
        { 0, 3, 7, 10   },  // Minor7
        { 0, 3, 6, 10   },  // Minor7b5
        { 0, 3, 6       },  // Diminished
        { 0, 3, 6, 9    },  // Diminished7
        { 0, 4, 8       },  // Augmented
        { 0, 2, 7       },  // Sus2
        { 0, 5, 7       },  // Sus4
        { 0, 4, 7, 2    },  // Add9
        { 0, 3, 7, 2    },  // MinorAdd9
    };

    const int idx = static_cast<int>(chord.type);
    if (idx < 0 || idx >= (int)intervals.size()) return { chord.root };

    std::vector<int> notes;
    for (int iv : intervals[idx])
        notes.push_back ((chord.root + iv) % 12);
    return notes;
}

std::vector<int> TransposeEngine::scaleNotes (const ChordInfo& chord)
{
    // Escala Maior do acorde (7 notas)
    static const std::array<int,7> majorScale = { 0, 2, 4, 5, 7, 9, 11 };
    std::vector<int> notes;
    for (int iv : majorScale)
        notes.push_back ((chord.root + iv) % 12);
    return notes;
}

// ─── shouldMute ───────────────────────────────────────────────────────────────
bool TransposeEngine::shouldMute (const CasmChannel& casmCh, const ChordInfo& chord)
{
    if (casmCh.muteFlags == 0) return false;

    // Mapeamento simplificado de ChordType para bit do muteFlags
    // (baseado na especificação CASM Yamaha SFF2)
    static const std::array<int, static_cast<int>(ChordType::Count)> typeToBit =
    {{
        0,   // Major
        1,   // Minor
        2,   // Dominant7
        3,   // Major7
        4,   // Minor7
        5,   // Minor7b5
        6,   // Diminished
        7,   // Diminished7
        8,   // Augmented
        9,   // Sus2
        10,  // Sus4
        11,  // Add9
        12,  // MinorAdd9
    }};

    const int bit = typeToBit[static_cast<int>(chord.type)];
    return (casmCh.muteFlags & (1 << bit)) != 0;
}

// ─── transposeRoot ────────────────────────────────────────────────────────────
int TransposeEngine::transposeRoot (int note, const ChordInfo& chord)
{
    // Os arquivos STY são gravados em CMaj (root = 0).
    // Transposição: desloca todas as notas pelo intervalo C → nova_root
    const int semitones = chord.root; // distância de C para o root do acorde
    int transposed = note + semitones;
    // Manter dentro de 0-127
    while (transposed > 127) transposed -= 12;
    while (transposed < 0)   transposed += 12;
    return transposed;
}

// ─── transposeBass ────────────────────────────────────────────────────────────
int TransposeEngine::transposeBass (int note, const ChordInfo& chord)
{
    // Para o baixo: a nota C da gravação vira o baixo do acorde.
    // Outras notas são transpostas pelo mesmo intervalo (root).
    const int bassTarget = chord.bassNote;
    int semitones = bassTarget; // C→bassNote
    int transposed = note + semitones;
    while (transposed > 127) transposed -= 12;
    while (transposed < 0)   transposed += 12;
    return transposed;
}

// ─── transposeGuitar ─────────────────────────────────────────────────────────
int TransposeEngine::transposeGuitar (int note, const ChordInfo& chord, NTT ntt)
{
    // Estratégia Guitar: remapeia as notas do CMaj7 para as notas do acorde alvo
    // NTT::CHORD  → mapeia para as notas do acorde
    // NTT::MELODY → mapeia para a escala do acorde

    const int noteClass = note % 12;
    const int octave    = note / 12;

    // Determina a posição da nota na fonte (CMaj7 = notas 0,4,7,11)
    // Encontra o índice da nota-fonte mais próxima
    int bestSourceIdx  = 0;
    int bestSourceDist = 12;
    for (int i = 0; i < 4; ++i)
    {
        int dist = std::abs (noteClass - SOURCE_CHORD_NOTES[i]);
        if (dist > 6) dist = 12 - dist; // distância circular
        if (dist < bestSourceDist)
        {
            bestSourceDist = dist;
            bestSourceIdx  = i;
        }
    }

    // Seleciona as notas alvo
    std::vector<int> targetNotes = (ntt == NTT::CHORD)
                                 ? chordNotes (chord)
                                 : scaleNotes (chord);

    if (targetNotes.empty()) return transposeRoot (note, chord);

    // Mapeia o índice proporcional para as notas do acorde alvo
    const int targetSize = (int)targetNotes.size();
    int targetIdx = (bestSourceIdx * targetSize) / 4;
    targetIdx = std::clamp (targetIdx, 0, targetSize - 1);

    int targetClass = targetNotes[targetIdx];
    int result      = octave * 12 + targetClass;

    while (result > 127) result -= 12;
    while (result < 0)   result += 12;
    return result;
}

// ─── transposeNote ───────────────────────────────────────────────────────────
int TransposeEngine::transposeNote (int note, const ChordInfo& chord,
                                    NTR ntr, NTT ntt,
                                    int highKey, int lowLimit, int highLimit)
{
    if (!chord.valid) return note; // sem acorde detectado: não transpõe

    int result = note;

    switch (ntr)
    {
        case NTR::BYPASS: result = note; break;
        case NTR::ROOT:   result = transposeRoot   (note, chord); break;
        case NTR::GUITAR: result = transposeGuitar (note, chord, ntt); break;
        case NTR::BASS:   result = transposeBass   (note, chord); break;
    }

    // Aplicar highKey: se a nota transposta exceder highKey, oitavar para baixo
    while (result > highKey && result > 0) result -= 12;

    // Aplicar limites do canal
    if (result < lowLimit)  result = lowLimit;
    if (result > highLimit) result = highLimit;

    return std::clamp (result, 0, 127);
}

// ─── transposeMidiMessage ─────────────────────────────────────────────────────
bool TransposeEngine::transposeMidiMessage (juce::MidiMessage& msg,
                                             const ChordInfo& chord,
                                             const CasmChannel& casmCh)
{
    if (shouldMute (casmCh, chord)) return false;

    if (msg.isNoteOn() || msg.isNoteOff())
    {
        int transposed = transposeNote (msg.getNoteNumber(), chord,
                                        casmCh.ntr, casmCh.ntt,
                                        casmCh.highKey,
                                        casmCh.noteLowLimit,
                                        casmCh.noteHighLimit);
        if (msg.isNoteOn())
            msg = juce::MidiMessage::noteOn  (msg.getChannel(), transposed, msg.getVelocity());
        else
            msg = juce::MidiMessage::noteOff (msg.getChannel(), transposed);
    }

    return true;
}
