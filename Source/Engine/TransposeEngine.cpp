#include "TransposeEngine.h"
#include <algorithm>
#include <cmath>

// ─── Intervalos do acorde alvo ────────────────────────────────────────────────
std::vector<int> TransposeEngine::chordNotes (const ChordInfo& chord)
{
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
    // Escala adequada ao tipo de acorde (não sempre Maior!)
    std::vector<int> scale;

    switch (chord.type)
    {
        case ChordType::Minor:
        case ChordType::Minor7:
        case ChordType::MinorAdd9:
            scale = { 0, 2, 3, 5, 7, 8, 10 };  // Natural minor (Aeolian)
            break;
        case ChordType::Minor7b5:
            scale = { 0, 1, 3, 5, 6, 8, 10 };  // Locrian
            break;
        case ChordType::Dominant7:
            scale = { 0, 2, 4, 5, 7, 9, 10 };  // Mixolydian
            break;
        case ChordType::Diminished:
        case ChordType::Diminished7:
            scale = { 0, 2, 3, 5, 6, 8, 9, 11 }; // Diminished (W-H)
            break;
        case ChordType::Augmented:
            scale = { 0, 2, 4, 6, 8, 10 };      // Whole tone
            break;
        default:
            scale = { 0, 2, 4, 5, 7, 9, 11 };   // Major (Ionian)
            break;
    }

    std::vector<int> notes;
    for (int iv : scale)
        notes.push_back ((chord.root + iv) % 12);
    return notes;
}

// ─── shouldMute ───────────────────────────────────────────────────────────────
bool TransposeEngine::shouldMute (const CasmChannel& casmCh, const ChordInfo& chord)
{
    if (casmCh.muteFlags == 0) return false;

    static const std::array<int, static_cast<int>(ChordType::Count)> typeToBit =
    {{
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
    }};

    const int bit = typeToBit[static_cast<int>(chord.type)];
    return (casmCh.muteFlags & (1 << bit)) != 0;
}

// ─── transposeRoot ────────────────────────────────────────────────────────────
int TransposeEngine::transposeRoot (int note, const ChordInfo& chord)
{
    // 1. Shift por fundamental: desloca pelo intervalo C → root
    int transposed = note + chord.root;

    // 2. Correção de tipo: ajusta a 3ª e 7ª para refletir o tipo do acorde.
    //    Os STY são gravados em CMaj (3ª maior=4, 7ª maior=11).
    //    Para acordes menores, flatten a 3ª; para dom7, flatten a 7ª; etc.
    int degree = ((transposed % 12) - chord.root + 12) % 12;

    bool flatThird  = false;
    bool flatFifth  = false;
    bool flatSeventh = false;

    switch (chord.type)
    {
        case ChordType::Minor:
        case ChordType::MinorAdd9:
            flatThird = true;
            break;
        case ChordType::Minor7:
            flatThird = true;
            flatSeventh = true;
            break;
        case ChordType::Minor7b5:
            flatThird = true;
            flatFifth = true;
            flatSeventh = true;
            break;
        case ChordType::Diminished:
            flatThird = true;
            flatFifth = true;
            break;
        case ChordType::Diminished7:
            flatThird = true;
            flatFifth = true;
            flatSeventh = true; // dim7 = bb7
            break;
        case ChordType::Dominant7:
            flatSeventh = true;
            break;
        default:
            break; // Major, Aug, Sus — sem correção na 3ª/5ª/7ª do CMaj
    }

    if (flatThird  && degree == 4)  transposed -= 1;  // 3ª maior → 3ª menor
    if (flatFifth  && degree == 7)  transposed -= 1;  // 5ª justa → 5ª dim
    if (flatSeventh && degree == 11) transposed -= 1;  // 7ª maior → 7ª menor

    while (transposed > 127) transposed -= 12;
    while (transposed < 0)   transposed += 12;
    return transposed;
}

// ─── transposeBass ────────────────────────────────────────────────────────────
int TransposeEngine::transposeBass (int note, const ChordInfo& chord)
{
    int transposed = note + chord.bassNote;
    while (transposed > 127) transposed -= 12;
    while (transposed < 0)   transposed += 12;
    return transposed;
}

// ─── Utilitário: encontra a nota mais próxima num conjunto ───────────────────
static int findClosestNoteClass (int noteClass, const std::vector<int>& targets)
{
    int bestDist = 12;
    int bestNote = noteClass;
    for (int t : targets)
    {
        int dist = std::abs (noteClass - t);
        if (dist > 6) dist = 12 - dist;
        if (dist < bestDist)
        {
            bestDist = dist;
            bestNote = t;
        }
    }
    return bestNote;
}

// ─── transposeGuitar ─────────────────────────────────────────────────────────
int TransposeEngine::transposeGuitar (int note, const ChordInfo& chord, NTT ntt)
{
    const int noteClass = note % 12;
    const int octave    = note / 12;

    if (ntt == NTT::CHORD)
    {
        // NTT=CHORD: remapeia grau-a-grau do acorde fonte (CMaj) para o acorde alvo.
        // Fonte CMaj: root=0(C), 3rd=4(E), 5th=7(G)
        // Notas que não são chord tones: mantêm o offset relativo ao chord tone mais próximo.

        static const std::vector<int> srcChordTones = { 0, 4, 7 };
        const auto targetChordTones = chordNotes (chord);

        if (targetChordTones.empty()) return transposeRoot (note, chord);

        // Encontra o chord tone fonte mais próximo e o offset
        int bestSrcDeg  = 0;
        int bestSrcDist = 12;
        for (int d = 0; d < (int)srcChordTones.size(); ++d)
        {
            int dist = noteClass - srcChordTones[d];
            if (dist < 0) dist += 12;
            int distAbs = (dist > 6) ? (12 - dist) : dist;
            if (distAbs < bestSrcDist)
            {
                bestSrcDist = distAbs;
                bestSrcDeg  = d;
            }
        }

        // Offset da nota em relação ao chord tone fonte mais próximo
        int offset = noteClass - srcChordTones[bestSrcDeg];
        if (offset < -6) offset += 12;
        if (offset >  6) offset -= 12;

        // Mapeia o grau para o acorde alvo (1st→1st, 3rd→3rd, 5th→5th)
        int targetDeg = bestSrcDeg;
        if (targetDeg >= (int)targetChordTones.size())
            targetDeg = (int)targetChordTones.size() - 1;

        int targetClass = (targetChordTones[targetDeg] + offset + 12) % 12;
        int result = octave * 12 + targetClass;

        while (result > 127) result -= 12;
        while (result < 0)   result += 12;
        return result;
    }
    else // NTT::MELODY
    {
        // NTT=MELODY: remapeia grau-a-grau da escala fonte (C Maior) para a escala alvo.
        // A escala alvo depende do tipo de acorde (maior, menor, mixolídio, etc.)

        static const std::vector<int> srcScale = { 0, 2, 4, 5, 7, 9, 11 };
        const auto targetScale = scaleNotes (chord);

        if (targetScale.empty()) return transposeRoot (note, chord);

        // Encontra o grau da escala fonte mais próximo
        int bestDeg  = 0;
        int bestDist = 12;
        for (int d = 0; d < (int)srcScale.size(); ++d)
        {
            int dist = std::abs (noteClass - srcScale[d]);
            if (dist > 6) dist = 12 - dist;
            if (dist < bestDist)
            {
                bestDist = dist;
                bestDeg  = d;
            }
        }

        // Mapeia grau-a-grau para a escala alvo
        int targetDeg = bestDeg;
        if (targetDeg >= (int)targetScale.size())
            targetDeg = (int)targetScale.size() - 1;

        int targetClass = targetScale[targetDeg];
        int result = octave * 12 + targetClass;

        while (result > 127) result -= 12;
        while (result < 0)   result += 12;
        return result;
    }
}

// ─── transposeNote ───────────────────────────────────────────────────────────
int TransposeEngine::transposeNote (int note, const ChordInfo& chord,
                                    NTR ntr, NTT ntt,
                                    int highKey, int lowLimit, int highLimit)
{
    if (!chord.valid) return note;

    int result = note;

    switch (ntr)
    {
        case NTR::BYPASS: result = note; break;
        case NTR::ROOT:   result = transposeRoot   (note, chord); break;
        case NTR::GUITAR: result = transposeGuitar (note, chord, ntt); break;
        case NTR::BASS:   result = transposeBass   (note, chord); break;
    }

    while (result > highKey && result > 0) result -= 12;

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
