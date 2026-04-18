#include "CasmParser.h"
#include <cstring>

uint32_t CasmParser::readBE32 (const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

uint16_t CasmParser::readBE16 (const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

bool CasmParser::parse (const uint8_t* data, size_t size, StyFile& out)
{
    if (size < 8) return false;

    // Verifica tag "CASM"
    if (std::memcmp (data, "CASM", 4) != 0)
    {
        DBG ("CasmParser: tag CASM nao encontrada");
        return false;
    }

    uint32_t casmLen = readBE32 (data + 4);
    if (casmLen + 8 > size) return false;

    // Procura por Cseg dentro do CASM
    const uint8_t* p   = data + 8;
    const uint8_t* end = data + 8 + casmLen;

    while (p + 8 <= end)
    {
        if (std::memcmp (p, "Cseg", 4) == 0 || std::memcmp (p, "CSEG", 4) == 0)  // SFF2=Cseg, SFF1=CSEG
        {
            uint32_t csegLen = readBE32 (p + 4);
            if (!parseCseg (p + 8, csegLen, out))
                return false;
            p += 8 + csegLen;
        }
        else
        {
            // Tag desconhecida: pular
            uint32_t len = readBE32 (p + 4);
            p += 8 + len;
        }
    }

    // Construir mapa rápido sourceChannel → índice
    out.casmIndex.fill (-1);
    for (int i = 0; i < (int)out.casmChannels.size(); ++i)
        out.casmIndex[out.casmChannels[i].sourceChannel] = i;

    DBG ("CasmParser: " + juce::String ((int)out.casmChannels.size()) + " canais CASM lidos");
    return true;
}

bool CasmParser::parseCseg (const uint8_t* data, size_t size, StyFile& out)
{
    const uint8_t* p   = data;
    const uint8_t* end = data + size;

    while (p + 8 <= end)
    {
        CasmChannel ch;
        bool ok = false;

        if (std::memcmp (p, "Ctb2", 4) == 0)
        {
            uint32_t len = readBE32 (p + 4);
            ok = parseCtb2 (p + 8, len, ch);
            p += 8 + len;
        }
        else if (std::memcmp (p, "Ctab", 4) == 0)
        {
            uint32_t len = readBE32 (p + 4);
            ok = parseCtab (p + 8, len, ch);
            p += 8 + len;
        }
        else
        {
            uint32_t len = readBE32 (p + 4);
            p += 8 + len;
            continue;
        }

        if (ok)
            out.casmChannels.push_back (ch);
    }
    return true;
}

// ─── Ctb2 (SFF2) ─────────────────────────────────────────────────────────────
// Layout do bloco Ctb2 (mínimo 9 bytes):
//  [0] SourceChannel   0x00-0x0F
//  [1] DestChannel     0x08-0x0F
//  [2] Flags           bit0=CloseHarmony
//  [3] NTR             0=ROOT,1=GUITAR,2=BASS,3=BYPASS
//  [4] NTT             0=BYPASS,1=MELODY,2=CHORD,3=MELODIC_MINOR
//  [5] HighKey         nota MIDI máxima (normalmente 0x7F)
//  [6] NoteLowLimit    nota MIDI mínima
//  [7] NoteHighLimit   nota MIDI máxima
//  [8] RTag            retrigger rule
//  [9..10] MuteFlags   16-bit bitfield (big-endian) – um bit por tipo de acorde
bool CasmParser::parseCtb2 (const uint8_t* data, size_t size, CasmChannel& ch)
{
    if (size < 9) return false;

    ch.sourceChannel = data[0] & 0x0F;
    ch.destChannel   = data[1] & 0x0F;
    ch.closeHarmony  = (data[2] & 0x01) != 0;
    ch.ntr           = static_cast<NTR> (data[3] & 0x03);
    ch.ntt           = static_cast<NTT> (data[4] & 0x03);
    ch.highKey       = data[5];
    ch.noteLowLimit  = data[6];
    ch.noteHighLimit = data[7];
    ch.rTag          = data[8];
    ch.muteFlags     = (size >= 11) ? readBE16 (data + 9) : 0;

    return true;
}

// ─── Ctab (SFF1) ─────────────────────────────────────────────────────────────
// Layout real observado nos bytes raw de STY Yamaha SFF1 (mínimo 18 bytes):
//  [0]      SourceChannel (nibble baixo, 0-7 → canais de estilo 9-16)
//  [1-8]    Nome da parte (8 bytes ASCII, pode ter null-padding)
//  [9]      DestChannel (nibble baixo, 0-15)
//  [10]     Flags / TactRange
//  [11]     HighKey (nota MIDI máxima desta entrada no split do teclado)
//  [12]     NTR: 0=ROOT, 1=GUITAR, 2=BASS, 3=BYPASS; 7=BYPASS (bateria); 0xFF→BYPASS
//  [13]     NTT (bits 2-0) + RetrigRule (bits 7-3)
//  [14]     NoteLowLimit
//  [15]     NoteHighLimit
//  [16-17]  MuteFlags (big-endian 16-bit, um bit por tipo de acorde)
bool CasmParser::parseCtab (const uint8_t* data, size_t size, CasmChannel& ch)
{
    if (size < 18) return false;

    ch.sourceChannel = data[0] & 0x0F;
    // data[1..8] = nome da parte (ignorado na reprodução)
    ch.destChannel   = data[9] & 0x0F;
    // data[10]   = flags / tact range (ignorado)
    ch.highKey       = data[11];
    uint8_t rawNtr   = data[12];
    ch.ntr           = static_cast<NTR> (rawNtr > 3 ? 3 : rawNtr);  // 7/0xFF (bateria) → BYPASS(3)
    ch.ntt           = static_cast<NTT> (data[13] & 0x03);
    ch.rTag          = (data[13] >> 2) & 0x3F;
    ch.noteLowLimit  = data[14];
    ch.noteHighLimit = data[15];
    ch.muteFlags     = readBE16 (data + 16);

    return true;
}
