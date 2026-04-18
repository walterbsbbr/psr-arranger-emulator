#pragma once
#include <JuceHeader.h>
#include "StyFile.h"
#include "CasmParser.h"

/**
 * StyParser
 *
 * Parser completo de arquivos .sty (Yamaha SFF1 e SFF2).
 *
 * Algoritmo:
 *  1. Usa juce::MidiFile para ler o bloco MIDI (trilha SMF tipo 0)
 *  2. Varre os Marker Events (0xFF 0x06) para mapear seções
 *  3. Para cada seção, extrai a MidiMessageSequence correspondente
 *  4. Lê os bytes raw após o End of Track e delega ao CasmParser
 *  5. Extrai BPM do Set Tempo event (0xFF 0x51)
 */
class StyParser
{
public:
    /**
     * Carrega e analisa um arquivo .sty.
     * @return StyFile com valid=true em sucesso, ou valid=false em falha.
     */
    static StyFile parse (const juce::File& styFile);

    /** Retorna lista de seções disponíveis no arquivo */
    static std::vector<StyleSection> getAvailableSections (const StyFile& sty);

private:
    static void detectFormat      (const juce::MidiMessageSequence& track, StyFile& out);
    static void extractSections   (const juce::MidiMessageSequence& track, StyFile& out);
    static void extractBpm        (const juce::MidiMessageSequence& track, StyFile& out);
    static bool parseCasmFromRaw  (const juce::MemoryBlock& rawFile, StyFile& out);

    /** Encontra o offset em bytes do End of Track na trilha MIDI raw */
    static int64_t findEndOfTrackOffset (const uint8_t* data, int64_t size);
};
