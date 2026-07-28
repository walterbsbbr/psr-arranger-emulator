/**
 * sty_debug.cpp — Ferramenta de diagnóstico para arquivos .sty
 * Compila com: clang++ -std=c++17 sty_debug.cpp -o sty_debug
 * Uso:         ./sty_debug arquivo.sty
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <cassert>

// ── Utilitários ───────────────────────────────────────────────────────────────
static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}
static uint16_t be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);
}

// Lê VLQ (variable-length quantity) — formato de delta time MIDI
static uint32_t readVlq(const uint8_t* data, size_t size, size_t& pos) {
    uint32_t value = 0;
    for (int i = 0; i < 4 && pos < size; ++i) {
        uint8_t byte = data[pos++];
        value = (value << 7) | (byte & 0x7F);
        if (!(byte & 0x80)) break;
    }
    return value;
}

struct MarkerEntry {
    uint32_t    tick;
    std::string text;
};

// ── Parser manual de trilha MIDI ──────────────────────────────────────────────
void parseMidiTrack(const uint8_t* data, size_t size) {
    size_t pos = 0;
    uint32_t tick = 0;
    uint8_t  lastStatus = 0;
    std::vector<MarkerEntry> markers;
    double bpm = 120.0;
    int ppq = 480;

    printf("\n=== MIDI TRACK EVENTS ===\n");

    while (pos < size) {
        // Delta time
        uint32_t delta = readVlq(data, size, pos);
        tick += delta;

        if (pos >= size) break;

        uint8_t status = data[pos];

        // Running status
        if (status < 0x80) {
            status = lastStatus;
        } else {
            lastStatus = status;
            ++pos;
        }

        uint8_t type    = status & 0xF0;
        uint8_t channel = status & 0x0F;

        if (status == 0xFF) { // Meta event
            if (pos >= size) break;
            uint8_t metaType = data[pos++];
            uint32_t metaLen = readVlq(data, size, pos);

            if (pos + metaLen > size) break;

            if (metaType == 0x06) { // Marker
                std::string text((const char*)(data + pos), metaLen);
                printf("  tick=%-8u MARKER: \"%s\"\n", tick, text.c_str());
                markers.push_back({tick, text});
            } else if (metaType == 0x51) { // Set Tempo
                if (metaLen >= 3) {
                    uint32_t uspb = ((uint32_t)data[pos]<<16)|((uint32_t)data[pos+1]<<8)|data[pos+2];
                    bpm = 60000000.0 / uspb;
                    printf("  tick=%-8u TEMPO: %.2f BPM\n", tick, bpm);
                }
            } else if (metaType == 0x2F) { // End of Track
                printf("  tick=%-8u END OF TRACK (offset=%zu)\n", tick, pos - 2 + metaLen);
                pos += metaLen;
                break;
            } else if (metaType == 0x01) { // Text
                std::string text((const char*)(data + pos), metaLen);
                printf("  tick=%-8u TEXT: \"%s\"\n", tick, text.c_str());
            } else if (metaType == 0x03) { // Track name
                std::string text((const char*)(data + pos), metaLen);
                printf("  tick=%-8u TRACK NAME: \"%s\"\n", tick, text.c_str());
            }
            pos += metaLen;

        } else if (type == 0xF0 || status == 0xF0) { // SysEx
            uint32_t len = readVlq(data, size, pos);
            if (pos + 3 < size && len >= 3) {
                printf("  tick=%-8u SYSEX: len=%u  %02X %02X %02X ...\n",
                       tick, len, data[pos], data[pos+1], data[pos+2]);
            }
            pos += len;
        } else {
            // Normal MIDI messages
            if (type == 0xC0) { // Program Change
                uint8_t pc = data[pos++];
                printf("  tick=%-8u PC ch=%d pc=%d\n", tick, channel+1, pc);
            } else if (type == 0xB0) { // CC
                uint8_t cc  = data[pos++];
                uint8_t val = data[pos++];
                if (cc == 0 || cc == 32)
                    printf("  tick=%-8u CC  ch=%d cc=%-3d val=%d\n", tick, channel+1, cc, val);
            } else if (type == 0x90) { // Note On
                uint8_t note = data[pos++];
                uint8_t vel  = data[pos++];
                (void)note; (void)vel;
            } else if (type == 0x80) { // Note Off
                pos += 2;
            } else if (type == 0xA0) { // Aftertouch
                pos += 2;
            } else if (type == 0xD0) { // Channel Pressure
                pos += 1;
            } else if (type == 0xE0) { // Pitch Bend
                pos += 2;
            } else {
                // Desconhecido
                ++pos;
            }
        }
    }

    printf("\n=== MARKERS ENCONTRADOS (%zu) ===\n", markers.size());
    for (auto& m : markers)
        printf("  tick=%-8u  \"%s\"\n", m.tick, m.text.c_str());
}

// ── Scan de seções após End of Track ──────────────────────────────────────────
void scanPostMidi(const uint8_t* data, size_t size, size_t postOffset) {
    printf("\n=== SEÇÕES PÓS-MIDI (offset=%zu) ===\n", postOffset);
    const size_t WINDOW = 64;
    size_t i = postOffset;
    while (i + 8 < size) {
        char tag[5] = {};
        memcpy(tag, data + i, 4);
        uint32_t len = be32(data + i + 4);

        // Só imprime se a tag tem caracteres imprimíveis
        bool printable = true;
        for (int k = 0; k < 4; ++k)
            if (tag[k] < 0x20 || tag[k] > 0x7E) { printable = false; break; }

        if (printable) {
            printf("  offset=%-8zu  tag=\"%s\"  len=%u\n", i, tag, len);
            if (strncmp(tag, "CASM", 4) == 0 ||
                strncmp(tag, "Cseg", 4) == 0 ||
                strncmp(tag, "Ctab", 4) == 0 ||
                strncmp(tag, "Ctb2", 4) == 0 ||
                strncmp(tag, "OTS ", 4) == 0 ||
                strncmp(tag, "MDB ", 4) == 0) {
                i += 8; // entra no bloco
                continue;
            }
            if (len > 0 && i + 8 + len <= size)
                i += 8 + len;
            else
                ++i;
        } else {
            ++i;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Uso: %s arquivo.sty\n", argv[0]);
        return 1;
    }

    const char* path = argv[1];
    printf("=== STY DEBUG: %s ===\n", path);

    // Ler arquivo
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("ERRO: nao foi possivel abrir o arquivo\n");
        return 1;
    }
    size_t fileSize = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(fileSize);
    file.read((char*)data.data(), fileSize);
    file.close();

    printf("Tamanho: %zu bytes\n", fileSize);

    // ── 1. Verificar header MIDI ──────────────────────────────────────────────
    if (fileSize < 14 || memcmp(data.data(), "MThd", 4) != 0) {
        printf("ERRO: Nao começa com 'MThd' — verificando primeiros 16 bytes:\n  ");
        for (size_t i = 0; i < std::min(fileSize, (size_t)16); ++i)
            printf("%02X ", data[i]);
        printf("\n");
        return 1;
    }

    uint32_t headerLen = be32(data.data() + 4);
    uint16_t format    = be16(data.data() + 8);
    uint16_t numTracks = be16(data.data() + 10);
    uint16_t ppq       = be16(data.data() + 12);

    printf("\n=== MIDI HEADER ===\n");
    printf("  Format:    %d (%s)\n", format,
           format == 0 ? "Type 0 (single track)" :
           format == 1 ? "Type 1 (multi track)" : "Type 2");
    printf("  Tracks:    %d\n", numTracks);
    printf("  PPQ:       %d\n", ppq);

    // ── 2. Localizar trilha ───────────────────────────────────────────────────
    size_t trackOffset = 8 + headerLen;
    printf("\n=== MIDI TRACK ===\n");

    if (trackOffset + 8 > fileSize) {
        printf("ERRO: offset da trilha fora do arquivo (%zu)\n", trackOffset);
        return 1;
    }

    if (memcmp(data.data() + trackOffset, "MTrk", 4) != 0) {
        printf("ERRO: 'MTrk' nao encontrado em offset %zu — encontrado: %02X %02X %02X %02X\n",
               trackOffset,
               data[trackOffset], data[trackOffset+1],
               data[trackOffset+2], data[trackOffset+3]);
        return 1;
    }

    uint32_t trackLen  = be32(data.data() + trackOffset + 4);
    size_t   trackData = trackOffset + 8;
    size_t   postMidi  = trackData + trackLen;

    printf("  Offset MTrk: %zu\n", trackOffset);
    printf("  Track len:   %u bytes\n", trackLen);
    printf("  Post-MIDI:   %zu\n", postMidi);

    // ── 3. Verificar marcador SFF no início da trilha ─────────────────────────
    printf("\n=== PRIMEIROS 80 BYTES DA TRILHA (hex + ascii) ===\n");
    for (size_t i = 0; i < std::min((size_t)80, (size_t)trackLen); ++i) {
        printf("%02X ", data[trackData + i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

    // ── 4. Parse dos eventos MIDI ─────────────────────────────────────────────
    parseMidiTrack(data.data() + trackData, trackLen);

    // ── 5. Seções pós-MIDI ────────────────────────────────────────────────────
    if (postMidi < fileSize)
        scanPostMidi(data.data(), fileSize, postMidi);
    else
        printf("\nNenhum dado após End of Track.\n");

    return 0;
}
