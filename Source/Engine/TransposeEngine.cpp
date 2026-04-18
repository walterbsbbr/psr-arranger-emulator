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
    std::vector<int> scale;
    switch (chord.type)
    {
        case ChordType::Minor:
        case ChordType::Minor7:
        case ChordType::MinorAdd9:
            scale = { 0, 2, 3, 5, 7, 8, 10 };  break;
        case ChordType::Minor7b5:
            scale = { 0, 1, 3, 5, 6, 8, 10 };  break;
        case ChordType::Dominant7:
            scale = { 0, 2, 4, 5, 7, 9, 10 };  break;
        case ChordType::Diminished:
        case ChordType::Diminished7:
            scale = { 0, 2, 3, 5, 6, 8, 9, 11 }; break;
        case ChordType::Augmented:
            scale = { 0, 2, 4, 6, 8, 10 };      break;
        default:
            scale = { 0, 2, 4, 5, 7, 9, 11 };   break;
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
    const int bit = static_cast<int>(chord.type);
    return (casmCh.muteFlags & (1 << bit)) != 0;
}

// ─── transposeRoot (Root Transpose) ──────────────────────────────────────────
// Shift paralelo: desloca TODAS as notas pelo intervalo C→root.
// Ex: C3,E3,G3 em FMaj → F3,A3,C4 (shift +5 semitons)
// Usado para: Phrase1, Phrase2 (NTT=MELODY)
int TransposeEngine::transposeRoot (int note, const ChordInfo& chord)
{
    int transposed = note + chord.root;

    // Correção de tipo: ajustar 3ª/5ª/7ª conforme tipo do acorde.
    // STY gravados em CMaj: 3ªM=grau4, 5ªJ=grau7, 7ªM=grau11
    int degree = ((transposed % 12) - chord.root + 12) % 12;

    bool flatThird = false, flatFifth = false, flatSeventh = false;
    switch (chord.type)
    {
        case ChordType::Minor:
        case ChordType::MinorAdd9:   flatThird = true; break;
        case ChordType::Minor7:      flatThird = true; flatSeventh = true; break;
        case ChordType::Minor7b5:    flatThird = true; flatFifth = true; flatSeventh = true; break;
        case ChordType::Diminished:  flatThird = true; flatFifth = true; break;
        case ChordType::Diminished7: flatThird = true; flatFifth = true; flatSeventh = true; break;
        case ChordType::Dominant7:   flatSeventh = true; break;
        default: break;
    }
    if (flatThird   && degree == 4)  transposed -= 1;
    if (flatFifth   && degree == 7)  transposed -= 1;
    if (flatSeventh && degree == 11) transposed -= 1;

    return transposed;
}

// ─── transposeBass ────────────────────────────────────────────────────────────
// Baixo: segue a fundamental do acorde (C→root).
// Identico a transposeRoot mas sem correção de tipo (baixo toca a fundamental pura).
int TransposeEngine::transposeBass (int note, const ChordInfo& chord)
{
    return note + chord.root;
}

// ─── transposeGuitar (Root Fixed) ────────────────────────────────────────────
// Remapeia notas do CMaj para o acorde alvo mantendo proximidade de oitava.
// Ex: C3,E3,G3 em FMaj → C3,F3,A3 (inversão próxima, sem pular oitava)
// Usado para: Chord1, Chord2, Pad (NTT=CHORD)
int TransposeEngine::transposeGuitar (int note, const ChordInfo& chord, NTT ntt)
{
    const int noteClass = note % 12;

    // Notas alvo: chord tones ou escala conforme NTT
    const auto targetNotes = (ntt == NTT::CHORD)
                           ? chordNotes (chord)
                           : scaleNotes (chord);

    if (targetNotes.empty()) return transposeRoot (note, chord);

    // ── Fonte: CMaj = {C=0, E=4, G=7} (ou escala {0,2,4,5,7,9,11})
    const auto& srcNotes = (ntt == NTT::CHORD)
                         ? std::vector<int>{ 0, 4, 7 }
                         : std::vector<int>{ 0, 2, 4, 5, 7, 9, 11 };

    // Encontrar o grau fonte mais próximo
    int bestSrcDeg = 0, bestDist = 12;
    for (int d = 0; d < (int)srcNotes.size(); ++d)
    {
        int dist = std::abs (noteClass - srcNotes[d]);
        if (dist > 6) dist = 12 - dist;
        if (dist < bestDist) { bestDist = dist; bestSrcDeg = d; }
    }

    // Offset da nota original em relação ao chord/scale tone fonte
    int offset = noteClass - srcNotes[bestSrcDeg];
    if (offset < -6) offset += 12;
    if (offset >  6) offset -= 12;

    // Mapear grau→grau (1st→1st, 3rd→3rd, 5th→5th)
    int targetDeg = bestSrcDeg;
    if (targetDeg >= (int)targetNotes.size())
        targetDeg = (int)targetNotes.size() - 1;

    int targetClass = (targetNotes[targetDeg] + offset + 12) % 12;

    // ── ROOT FIXED: escolher a oitava mais próxima da nota original ──────────
    // Isso evita que os acordes "pulem" de oitava bruscamente.
    int candidate = (note / 12) * 12 + targetClass;

    // Testar oitava acima e abaixo, escolher a mais próxima
    int best = candidate;
    int bestAbsDist = std::abs (candidate - note);

    if (std::abs (candidate - 12 - note) < bestAbsDist)
    {
        best = candidate - 12;
        bestAbsDist = std::abs (best - note);
    }
    if (std::abs (candidate + 12 - note) < bestAbsDist)
        best = candidate + 12;

    return best;
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
        case NTR::BYPASS: return note;
        case NTR::ROOT:   result = transposeRoot   (note, chord); break;
        case NTR::GUITAR: result = transposeGuitar (note, chord, ntt); break;
        case NTR::BASS:   result = transposeBass   (note, chord); break;
    }

    // ── High Key: se o root está acima do highKey, oitavar para baixo ────────
    // Ex: highKey=F(65) e acorde=G → transpose -5 em vez de +7
    if (result > highKey && result > 0)
        result -= 12;

    // ── Note Limits: ajustar por oitavas (±12) para caber nos limites ────────
    while (result < lowLimit  && result + 12 <= 127) result += 12;
    while (result > highLimit && result - 12 >= 0)   result -= 12;

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
