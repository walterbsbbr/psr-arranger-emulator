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

    // Construir mapa rápido sourceChannel → índice.
    // Usar a PRIMEIRA entrada por canal (CSEG#0 = Main A/B/C/D/Fills).
    // CSEGs posteriores (Intro B, Ending B etc.) podem ter NTT=BYPASS
    // e sobrescreveriam as regras corretas.
    out.casmIndex.fill (-1);
    for (int i = 0; i < (int)out.casmChannels.size(); ++i)
    {
        int srcCh = out.casmChannels[i].sourceChannel;
        if (out.casmIndex[srcCh] < 0)
            out.casmIndex[srcCh] = i;
    }

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
// Layout real de 27 bytes (confirmado via dump hex de ROCK01.STY e outros):
//  [0]      SourceChannel (nibble baixo, 0-7 → canais de estilo 9-16)
//  [1-8]    Nome da parte (8 bytes ASCII, space-padded)
//  [9]      DestChannel (nibble baixo, 0-15)
//  [10]     Source chord root / key editable
//  [11]     Source chord type editable
//  [12]     Source bass note editable (0xFF = não aplicável)
//  [13]     Channel type: 0x03=melodic, 0x07=drum → mapeia para NTR
//  [14-17]  Reserved / extended chord mask (geralmente FF FF FF FF)
//  [18]     Unknown / flag
//  [19]     Unknown (sempre 0x02 nos arquivos testados)
//  [20]     Drum flag: 0x01=drum(bypass), 0x00=melodic
//  [21]     NTT: 0=BYPASS, 1=MELODY, 2=CHORD, 3=MELODIC_MINOR
//  [22]     HighKey / voice octave control
//  [23]     NoteLowLimit
//  [24]     NoteHighLimit (0x7F=127)
//  [25]     RetrigRule
//  [26]     Unknown / padding
bool CasmParser::parseCtab (const uint8_t* data, size_t size, CasmChannel& ch)
{
    if (size < 20) return false;

    ch.sourceChannel = data[0] & 0x0F;
    ch.destChannel   = data[9] & 0x0F;

    // Extrair nome da parte (bytes [1-8], 8 chars ASCII, space-padded)
    ch.partName = juce::String ((const char*)(data + 1), 8).trim();

    // Detectar drums: SOMENTE byte [13]=0x07.
    // Byte [20] NÃO é drum flag — Piano/Choir/PedalStl têm [20]=0x01 mas são melódicos.
    bool isDrum = (data[13] == 0x07);

    if (size >= 25)
    {
        // ── Layout completo de 27 bytes ──────────────────────────────────────
        ch.ntt = static_cast<NTT> (std::min<uint8_t> (data[21], 3));

        // Derivar NTR a partir do NTT (SFF1):
        //  - Drums / NTT=BYPASS → NTR::BYPASS (sem transposição)
        //  - NTT=CHORD         → NTR::GUITAR (Root Fixed: inversões próximas)
        //  - NTT=MELODY        → NTR::ROOT   (Root Transpose: shift paralelo)
        //  - NTT=MELODIC_MINOR → NTR::BASS   (segue fundamental do baixo)
        if (isDrum || ch.ntt == NTT::BYPASS)
            ch.ntr = NTR::BYPASS;
        else if (ch.ntt == NTT::CHORD)
            ch.ntr = NTR::GUITAR;    // Root Fixed — Chord1, Chord2, Pad
        else if (ch.ntt == NTT::MELODIC_MINOR)
            ch.ntr = NTR::BASS;      // Bass
        else
            ch.ntr = NTR::ROOT;      // NTT::MELODY — Phrase1, Phrase2

        // HighKey: byte [22] é um indicador de oitava (valores 3,6,7 observados).
        // Converter para nota MIDI: oitava N → última nota da oitava = (N+1)*12 - 1
        // Ex: 3→47(B3), 6→83(B6), 7→95(B7). Se >= 12, tratar como MIDI note direto.
        uint8_t rawHK = data[22];
        if (rawHK == 0 || rawHK >= 127)
            ch.highKey = 127;
        else if (rawHK < 12)
            ch.highKey = (uint8_t)((rawHK + 1) * 12 - 1);
        else
            ch.highKey = rawHK;

        ch.noteLowLimit  = data[23];
        ch.noteHighLimit = data[24];
        ch.rTag          = (size >= 26) ? data[25] : 0;
        ch.muteFlags     = 0;
    }
    else
    {
        ch.ntr           = isDrum ? NTR::BYPASS : NTR::ROOT;
        ch.ntt           = NTT::BYPASS;
        ch.highKey       = 127;
        ch.noteLowLimit  = 0;
        ch.noteHighLimit = 127;
        ch.rTag          = 0;
        ch.muteFlags     = 0;
    }

    DBG ("  Ctab: src=" + juce::String (ch.sourceChannel)
         + " dst=" + juce::String (ch.destChannel)
         + " \"" + ch.partName + "\""
         + " NTR=" + juce::String ((int)ch.ntr)
         + " NTT=" + juce::String ((int)ch.ntt)
         + " HK=" + juce::String (ch.highKey)
         + " lo=" + juce::String (ch.noteLowLimit)
         + " hi=" + juce::String (ch.noteHighLimit));

    return true;
}
