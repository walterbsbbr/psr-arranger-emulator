#include "ChordDetector.h"
#include <algorithm>
#include <numeric>

// ─── Tabela de intervalos ─────────────────────────────────────────────────────
// Cada entry: semitons em relação à fundamental (root = 0)
const std::array<std::vector<int>, static_cast<int>(ChordType::Count)>
ChordDetector::intervalTable =
{{
    { 0, 4, 7       },       // Major
    { 0, 3, 7       },       // Minor
    { 0, 4, 7, 10   },       // Dominant 7
    { 0, 4, 7, 11   },       // Major 7
    { 0, 3, 7, 10   },       // Minor 7
    { 0, 3, 6, 10   },       // Minor7b5
    { 0, 3, 6       },       // Diminished
    { 0, 3, 6, 9    },       // Diminished 7
    { 0, 4, 8       },       // Augmented
    { 0, 2, 7       },       // Sus2
    { 0, 5, 7       },       // Sus4
    { 0, 4, 7, 14   },       // Add9  (normalizado: 14%12=2)
    { 0, 3, 7, 14   },       // MinorAdd9
}};

// ─── ChordInfo::toString ──────────────────────────────────────────────────────
static const char* noteNames[] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

juce::String ChordInfo::toString() const
{
    if (!valid) return "---";
    return juce::String (noteNames[root % 12])
           + ChordDetector::chordTypeName (type);
}

// ─── ChordDetector ────────────────────────────────────────────────────────────
ChordDetector::ChordDetector() {}

juce::String ChordDetector::noteName (int midiNote)
{
    return juce::String (noteNames[midiNote % 12])
           + juce::String (midiNote / 12 - 1);
}

juce::String ChordDetector::chordTypeName (ChordType t)
{
    switch (t)
    {
        case ChordType::Major:       return "";
        case ChordType::Minor:       return "m";
        case ChordType::Dominant7:   return "7";
        case ChordType::Major7:      return "maj7";
        case ChordType::Minor7:      return "m7";
        case ChordType::Minor7b5:    return "m7b5";
        case ChordType::Diminished:  return "dim";
        case ChordType::Diminished7: return "dim7";
        case ChordType::Augmented:   return "aug";
        case ChordType::Sus2:        return "sus2";
        case ChordType::Sus4:        return "sus4";
        case ChordType::Add9:        return "add9";
        case ChordType::MinorAdd9:   return "madd9";
        default:                     return "?";
    }
}

void ChordDetector::noteOn (int midiNote)
{
    heldNotes.insert (midiNote);
    currentChord = detect();
}

void ChordDetector::noteOff (int midiNote)
{
    heldNotes.erase (midiNote);
    // PSR behavior: o último acorde fica ativo até o próximo.
    // Quando todas as teclas são soltas, NÃO reseta o acorde.
    // Apenas atualiza se ainda há teclas pressionadas.
    if (!heldNotes.empty())
        currentChord = detect();
}

void ChordDetector::reset()
{
    heldNotes.clear();
    currentChord = ChordInfo{};
}

// ─── Detecção principal ───────────────────────────────────────────────────────
ChordInfo ChordDetector::detect() const
{
    if (heldNotes.empty()) return {};

    if (mode == ChordMode::SingleFinger)
        return detectSingleFinger();

    return detectFingered();
}

ChordInfo ChordDetector::detectSingleFinger() const
{
    // Single Finger: a nota mais grave define a fundamental
    // 1 nota = Maior; + 1 branca à esquerda = Menor; + 1 preta à esquerda = 7ª
    std::vector<int> notes (heldNotes.begin(), heldNotes.end());
    std::sort (notes.begin(), notes.end());

    const int root = notes[0] % 12;
    ChordInfo info;
    info.root     = root;
    info.bassNote = root;
    info.valid    = true;

    if (notes.size() == 1)
    {
        info.type = ChordType::Major;
    }
    else if (notes.size() == 2)
    {
        int diff = (notes[1] - notes[0]) % 12;
        // Branca à esquerda (semitom branco): difícil saber sem contexto
        // Convenção: notas com distância <= 2 = Minor, <= 5 = 7ª
        info.type = (diff <= 2) ? ChordType::Minor : ChordType::Dominant7;
    }
    else
    {
        // Três ou mais notas: tentar Fingered
        return detectFingered();
    }
    return info;
}

int ChordDetector::scoreMatch (const std::vector<int>& noteClasses,
                                int root,
                                const std::vector<int>& intervals)
{
    int score = 0;
    for (int semitone : intervals)
    {
        int target = (root + semitone) % 12;
        if (std::find (noteClasses.begin(), noteClasses.end(), target) != noteClasses.end())
            ++score;
    }
    return score;
}

ChordInfo ChordDetector::detectFingered() const
{
    if (heldNotes.empty()) return {};

    std::vector<int> notes (heldNotes.begin(), heldNotes.end());
    std::sort (notes.begin(), notes.end());

    // Normalizar para classes de nota (0-11)
    std::vector<int> classes;
    classes.reserve (notes.size());
    for (int n : notes)
    {
        int c = n % 12;
        if (std::find (classes.begin(), classes.end(), c) == classes.end())
            classes.push_back (c);
    }

    // Testar todas as raízes e todos os tipos de acorde
    int bestScore    = 0;
    int bestRoot     = notes.front() % 12;
    ChordType bestType = ChordType::Major;

    const int numTypes = static_cast<int>(ChordType::Count);

    for (int root = 0; root < 12; ++root)
    {
        for (int t = 0; t < numTypes; ++t)
        {
            const auto& intervals = intervalTable[t];
            int score = scoreMatch (classes, root, intervals);

            // Bônus: root presente nas notas pressionadas
            int rootClass = root;
            bool rootPresent = std::find (classes.begin(), classes.end(), rootClass)
                               != classes.end();
            if (rootPresent) score += 2;

            // Bônus: raiz coincide com a nota mais grave
            if (notes.front() % 12 == root) score += 3;

            // Bonus: match completo
            if (score == (int)intervals.size() + 2 + 3) score += 5;

            if (score > bestScore)
            {
                bestScore = score;
                bestRoot  = root;
                bestType  = static_cast<ChordType> (t);
            }
        }
    }

    ChordInfo info;
    info.root     = bestRoot;
    info.type     = bestType;
    info.bassNote = notes.front() % 12;
    info.valid    = (bestScore > 0);
    return info;
}
