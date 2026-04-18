#include "StyParser.h"
#include <cstring>

// ─── parse ────────────────────────────────────────────────────────────────────
StyFile StyParser::parse (const juce::File& styFile)
{
    StyFile out;
    out.name     = styFile.getFileNameWithoutExtension();
    out.filePath = styFile.getFullPathName();

    // Ler arquivo inteiro em memória
    juce::MemoryBlock rawData;
    if (!styFile.loadFileAsData (rawData) || rawData.getSize() < 14)
    {
        DBG ("StyParser: erro lendo arquivo " + styFile.getFileName());
        return out;
    }

    // ── 1. Ler bloco MIDI com juce::MidiFile ────────────────────────────────
    // STY = MIDI SMF + blocos extras (CASM, OTS, MDB) após o End of Track.
    // JUCE MidiFile::readFrom() exige que TODOS os bytes do stream sejam consumidos
    // (retorna false se sobrar dados). Precisamos truncar o stream para conter
    // apenas a porção MIDI (MThd + MTrk).
    const auto* rawPtr = reinterpret_cast<const uint8_t*> (rawData.getData());
    int64_t midiEndOffset = findEndOfTrackOffset (rawPtr, (int64_t)rawData.getSize());
    if (midiEndOffset <= 0)
    {
        DBG ("StyParser: nao encontrou MThd/MTrk em " + styFile.getFileName());
        return out;
    }

    juce::MemoryBlock midiOnlyData (rawPtr, (size_t)midiEndOffset);
    juce::MemoryInputStream mis (midiOnlyData, false);
    juce::MidiFile midiFile;

    if (!midiFile.readFrom (mis))
    {
        DBG ("StyParser: nao e um MIDI valido: " + styFile.getFileName());
        return out;
    }

    if (midiFile.getNumTracks() < 1)
    {
        DBG ("StyParser: nenhuma trilha encontrada");
        return out;
    }

    out.ppq = midiFile.getTimeFormat();
    if (out.ppq <= 0) out.ppq = 480;

    const juce::MidiMessageSequence& track = *midiFile.getTrack (0);

    // ── 2. Detectar formato (SFF1 / SFF2) ───────────────────────────────────
    detectFormat (track, out);

    // ── 3. Extrair BPM ──────────────────────────────────────────────────────
    extractBpm (track, out);

    // ── 4. Extrair seções pelos marcadores ──────────────────────────────────
    extractSections (track, out);

    // ── 5. Parsear bloco CASM dos bytes raw ─────────────────────────────────
    parseCasmFromRaw (rawData, out);

    out.valid = true;

    int numSections = 0;
    for (auto& s : out.sections)
        if (s.exists) ++numSections;

    DBG ("StyParser: " + out.name
         + " | BPM=" + juce::String (out.defaultBpm, 1)
         + " | PPQ=" + juce::String (out.ppq)
         + " | Secoes=" + juce::String (numSections)
         + " | CASM ch=" + juce::String ((int)out.casmChannels.size()));

    return out;
}

// ─── detectFormat ─────────────────────────────────────────────────────────────
void StyParser::detectFormat (const juce::MidiMessageSequence& track, StyFile& out)
{
    for (int i = 0; i < track.getNumEvents(); ++i)
    {
        const auto& msg = track.getEventPointer(i)->message;
        if (msg.isTextMetaEvent())
        {
            auto text = msg.getTextFromTextMetaEvent();
            if (text == "SFF2") { out.formatVersion = StyFormatVersion::SFF2; return; }
            if (text == "SFF1") { out.formatVersion = StyFormatVersion::SFF1; return; }
        }
    }
    out.formatVersion = StyFormatVersion::Unknown;
}

// ─── extractBpm ───────────────────────────────────────────────────────────────
void StyParser::extractBpm (const juce::MidiMessageSequence& track, StyFile& out)
{
    out.defaultBpm = 120.0;
    for (int i = 0; i < track.getNumEvents(); ++i)
    {
        const auto& msg = track.getEventPointer(i)->message;
        if (msg.isTempoMetaEvent())
        {
            double secPerBeat = msg.getTempoSecondsPerQuarterNote();
            if (secPerBeat > 0.0)
                out.defaultBpm = 60.0 / secPerBeat;
            return; // Usa o primeiro tempo encontrado
        }
    }
}

// ─── extractSections ─────────────────────────────────────────────────────────
void StyParser::extractSections (const juce::MidiMessageSequence& track, StyFile& out)
{
    // Construir lista de marcadores com seus ticks
    struct MarkerEntry {
        int           tick;
        StyleSection  section;
    };
    std::vector<MarkerEntry> markers;

    for (int i = 0; i < track.getNumEvents(); ++i)
    {
        const auto& msg = track.getEventPointer(i)->message;
        // Marker events: meta type 0x06
        if (msg.getMetaEventType() == 6)
        {
            std::string text ((const char*)msg.getMetaEventData(),
                              (size_t)msg.getMetaEventLength());
            StyleSection sec = sectionFromMarker (text);
            if (sec != StyleSection::Unknown)
                markers.push_back ({ (int)msg.getTimeStamp(), sec });
        }
    }

    if (markers.empty()) return;

    // Calcular tick final da última seção
    int lastTick = 0;
    for (int i = 0; i < track.getNumEvents(); ++i)
        lastTick = std::max (lastTick, (int)track.getEventPointer(i)->message.getTimeStamp());

    // Para cada marcador, extrair os eventos até o próximo marcador
    for (size_t m = 0; m < markers.size(); ++m)
    {
        const int startTick = markers[m].tick;
        const int endTick   = (m + 1 < markers.size()) ? markers[m+1].tick : lastTick;
        const StyleSection sec = markers[m].section;

        auto& sdata = out.sections[static_cast<int>(sec)];
        sdata.exists    = true;
        sdata.startTick = startTick;
        sdata.endTick   = endTick;

        // Copiar eventos que caem neste intervalo, rebaseados para tick 0
        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            const auto* ev  = track.getEventPointer(i);
            const int   t   = (int)ev->message.getTimeStamp();
            if (t < startTick) continue;
            if (t >= endTick)  break;

            // Ignorar meta-events de marcador e tempo
            const auto& msg = ev->message;
            if (msg.isMetaEvent()) continue;

            // Rebasar o tick para o início da seção
            auto rebased = msg;
            rebased.setTimeStamp ((double)(t - startTick));
            sdata.events.addEvent (rebased);
        }
        sdata.events.sort();
    }
}

// ─── parseCasmFromRaw ─────────────────────────────────────────────────────────
bool StyParser::parseCasmFromRaw (const juce::MemoryBlock& rawData, StyFile& out)
{
    const uint8_t* data = reinterpret_cast<const uint8_t*> (rawData.getData());
    const int64_t  size = (int64_t)rawData.getSize();

    // Encontrar o offset após o End of Track MIDI
    // O arquivo começa com o header MIDI "MThd", depois "MTrk" com o comprimento.
    // Calculamos o offset do fim da trilha MIDI.
    int64_t offset = findEndOfTrackOffset (data, size);
    if (offset < 0 || offset + 8 >= size) return false;

    // Procurar "CASM" a partir do offset (pode haver alguns bytes de padding)
    for (int64_t i = offset; i + 8 < size; ++i)
    {
        if (std::memcmp (data + i, "CASM", 4) == 0)
            return CasmParser::parse (data + i, (size_t)(size - i), out);
    }

    DBG ("StyParser: bloco CASM nao encontrado apos o End of Track");
    return false;
}

int64_t StyParser::findEndOfTrackOffset (const uint8_t* data, int64_t size)
{
    // Formato SMF:
    //   "MThd" (4) + length BE32 (4) + header_data (6) = 14 bytes
    //   "MTrk" (4) + length BE32 (4) + track_data
    if (size < 14) return -1;
    if (std::memcmp (data, "MThd", 4) != 0) return -1;

    // Pular header
    const uint32_t headerLen = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16)
                             | ((uint32_t)data[6] <<  8) |  (uint32_t)data[7];
    int64_t trackOffset = 8 + headerLen;

    if (trackOffset + 8 > size) return -1;
    if (std::memcmp (data + trackOffset, "MTrk", 4) != 0) return -1;

    const uint32_t trackLen = ((uint32_t)data[trackOffset+4] << 24)
                            | ((uint32_t)data[trackOffset+5] << 16)
                            | ((uint32_t)data[trackOffset+6] <<  8)
                            |  (uint32_t)data[trackOffset+7];

    return trackOffset + 8 + trackLen;  // byte logo após o End of Track
}

// ─── getAvailableSections ─────────────────────────────────────────────────────
std::vector<StyleSection> StyParser::getAvailableSections (const StyFile& sty)
{
    std::vector<StyleSection> result;
    for (int i = 0; i < static_cast<int>(StyleSection::Count); ++i)
    {
        auto sec = static_cast<StyleSection>(i);
        if (sty.hasSection (sec))
            result.push_back (sec);
    }
    return result;
}
