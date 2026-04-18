#pragma once
#include <string>
#include <string_view>

/**
 * Seções de um arquivo STY reconhecidas pelos marcadores MIDI.
 * A ordem reflete a ordem típica de aparecimento no arquivo.
 */
enum class StyleSection
{
    Unknown = -1,
    SInt = 0,       // Compasso 1: inicialização (SysEx, Program Changes)

    IntroA,
    IntroB,
    IntroC,

    MainA,
    MainB,
    MainC,
    MainD,

    FillInAA,       // Fill de A para A (loop)
    FillInAB,       // Fill de A para B
    FillInBA,       // Fill de B para A
    FillInBB,       // Fill de B para B
    FillInCC,
    FillInDD,
    BreakFill,      // Pausa / Break

    EndingA,
    EndingB,
    EndingC,

    Count           // sentinela
};

/** Retorna true se a seção faz loop contínuo (Main A/B/C/D) */
constexpr bool isSectionLooping (StyleSection s)
{
    return s == StyleSection::MainA || s == StyleSection::MainB
        || s == StyleSection::MainC || s == StyleSection::MainD;
}

/** Retorna true se a seção é disparada uma vez (Intro, Fill, Ending, Break) */
constexpr bool isSectionOneShot (StyleSection s)
{
    return !isSectionLooping (s) && s != StyleSection::SInt && s != StyleSection::Unknown;
}

/** Converte nome do marcador STY para enum */
inline StyleSection sectionFromMarker (const std::string& marker)
{
    if (marker == "SInt"         ) return StyleSection::SInt;
    if (marker == "Intro A"      ) return StyleSection::IntroA;
    if (marker == "Intro B"      ) return StyleSection::IntroB;
    if (marker == "Intro C"      ) return StyleSection::IntroC;
    if (marker == "Main A"       ) return StyleSection::MainA;
    if (marker == "Main B"       ) return StyleSection::MainB;
    if (marker == "Main C"       ) return StyleSection::MainC;
    if (marker == "Main D"       ) return StyleSection::MainD;
    if (marker == "Fill In AA"   ) return StyleSection::FillInAA;
    if (marker == "Fill In AB"   ) return StyleSection::FillInAB;
    if (marker == "Fill In BA"   ) return StyleSection::FillInBA;
    if (marker == "Fill In BB"   ) return StyleSection::FillInBB;
    if (marker == "Fill In CC"   ) return StyleSection::FillInCC;
    if (marker == "Fill In DD"   ) return StyleSection::FillInDD;
    if (marker == "Break Fill"   ) return StyleSection::BreakFill;
    if (marker == "Ending A"     ) return StyleSection::EndingA;
    if (marker == "Ending B"     ) return StyleSection::EndingB;
    if (marker == "Ending C"     ) return StyleSection::EndingC;
    return StyleSection::Unknown;
}

inline const char* sectionName (StyleSection s)
{
    switch (s)
    {
        case StyleSection::SInt:      return "SInt";
        case StyleSection::IntroA:    return "Intro A";
        case StyleSection::IntroB:    return "Intro B";
        case StyleSection::IntroC:    return "Intro C";
        case StyleSection::MainA:     return "Main A";
        case StyleSection::MainB:     return "Main B";
        case StyleSection::MainC:     return "Main C";
        case StyleSection::MainD:     return "Main D";
        case StyleSection::FillInAA:  return "Fill In AA";
        case StyleSection::FillInAB:  return "Fill In AB";
        case StyleSection::FillInBA:  return "Fill In BA";
        case StyleSection::FillInBB:  return "Fill In BB";
        case StyleSection::FillInCC:  return "Fill In CC";
        case StyleSection::FillInDD:  return "Fill In DD";
        case StyleSection::BreakFill: return "Break Fill";
        case StyleSection::EndingA:   return "Ending A";
        case StyleSection::EndingB:   return "Ending B";
        case StyleSection::EndingC:   return "Ending C";
        default:                      return "Unknown";
    }
}
