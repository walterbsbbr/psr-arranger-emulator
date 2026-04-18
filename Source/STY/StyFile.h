#pragma once
#include <JuceHeader.h>
#include "StySection.h"
#include <array>
#include <map>

/**
 * NTR — Note Transposition Rule
 * Define como as notas de um canal são transpostas em função do acorde.
 */
enum class NTR : uint8_t
{
    ROOT   = 0,  // Transpõe pela fundamental do acorde
    GUITAR = 1,  // Remapeia dentro das notas do acorde (voicing de guitarra)
    BASS   = 2,  // Baixo: segue a fundamental / baixo do acorde
    BYPASS = 3   // Sem transposição (bateria, percussão)
};

/**
 * NTT — Note Transposition Table
 * Define a tabela de remapeamento usada quando NTR = GUITAR
 */
enum class NTT : uint8_t
{
    BYPASS    = 0,  // Não usa tabela
    MELODY    = 1,  // Usa tabela de escala melódica
    CHORD     = 2,  // Usa tabela das notas do acorde
    MELODIC_MINOR = 3
};

/**
 * CasmChannel — mapeamento de um canal de origem MIDI para o canal de destino
 * de acompanhamento, mais as regras de transposição e mute.
 */
struct CasmChannel
{
    uint8_t sourceChannel  { 0 };   // canal de origem (0-15)
    uint8_t destChannel    { 8 };   // canal de destino (8-15 = ch 9-16)
    NTR     ntr            { NTR::BYPASS };
    NTT     ntt            { NTT::BYPASS };
    uint8_t highKey        { 127 }; // nota MIDI máxima antes de oitavar para baixo
    uint8_t noteLowLimit   { 0   };
    uint8_t noteHighLimit  { 127 };
    uint8_t rTag           { 0   }; // retrigger rule
    uint16_t muteFlags     { 0   }; // bitfield por tipo de acorde
    bool    closeHarmony   { false };
    juce::String partName;           // nome da parte do Ctab (ex: "Bass", "Chord1")
};

/**
 * StyFormatVersion — versão do formato do arquivo STY
 */
enum class StyFormatVersion { SFF1, SFF2, Unknown };

/**
 * StyFile — modelo de dados completo de um arquivo .sty carregado.
 *
 * Contém:
 *  - MidiMessageSequence por seção (extraída pelos marcadores)
 *  - Array de CasmChannel descrevendo as regras por canal de origem
 *  - Metadados (BPM, nome, versão do formato)
 */
struct StyFile
{
    bool valid { false };
    juce::String name;
    juce::String filePath;
    StyFormatVersion formatVersion { StyFormatVersion::Unknown };

    // Tempo padrão do estilo em BPM
    double defaultBpm { 120.0 };
    int    ppq        { 480    }; // ticks por quarter note

    // Dados MIDI por seção
    struct SectionData
    {
        juce::MidiMessageSequence events;
        int startTick { 0 };
        int endTick   { 0 };
        bool exists   { false };
    };

    std::array<SectionData, static_cast<int>(StyleSection::Count)> sections;

    // Bloco CASM: até 16 entradas (uma por canal de origem)
    std::vector<CasmChannel> casmChannels;

    // Mapa rápido: sourceChannel → índice em casmChannels
    std::array<int, 16> casmIndex {};   // -1 = não mapeado

    StyFile() { casmIndex.fill (-1); }

    bool hasSection (StyleSection s) const
    {
        return sections[static_cast<int>(s)].exists;
    }

    const SectionData& getSection (StyleSection s) const
    {
        return sections[static_cast<int>(s)];
    }
};
