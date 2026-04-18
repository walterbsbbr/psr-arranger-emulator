#pragma once
#include <JuceHeader.h>
#include "StyFile.h"

/**
 * CasmParser
 *
 * Decodifica o bloco CASM (Chord And Style Module) de um arquivo STY.
 *
 * Estrutura do bloco CASM (SFF2):
 *   "CASM" [4 bytes length]
 *   "Cseg" [4 bytes length]
 *     "Ctb2" [4 bytes length] × N canais
 *       [dados do CasmChannel — ver StyFile.h]
 *
 * O CasmParser recebe um ponteiro para os bytes raw após o End of Track MIDI
 * e preenche o vetor casmChannels do StyFile.
 */
class CasmParser
{
public:
    /**
     * Analisa os bytes raw do bloco CASM.
     * @param data  Ponteiro para o início da tag "CASM"
     * @param size  Número de bytes disponíveis a partir de data
     * @param out   StyFile a ser preenchido
     * @return true em sucesso
     */
    static bool parse (const uint8_t* data, size_t size, StyFile& out);

private:
    static bool parseCseg (const uint8_t* data, size_t size, StyFile& out);
    static bool parseCtab (const uint8_t* data, size_t size, CasmChannel& ch);  // SFF1
    static bool parseCtb2 (const uint8_t* data, size_t size, CasmChannel& ch);  // SFF2

    static uint32_t readBE32 (const uint8_t* p);
    static uint16_t readBE16 (const uint8_t* p);
};
