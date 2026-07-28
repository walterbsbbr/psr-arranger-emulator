/**
 * midi_trunc_test.cpp — Tests if truncating STY to MIDI-only portion works
 * Simulates what our fixed StyParser::parse() does
 *
 * Compile: clang++ -std=c++17 midi_trunc_test.cpp -o midi_trunc_test
 * Usage:   ./midi_trunc_test arquivo.sty
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}
static uint16_t be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("Uso: %s arquivo.sty\n", argv[0]); return 1; }

    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file.is_open()) { printf("ERRO: nao abriu\n"); return 1; }
    size_t fileSize = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(fileSize);
    file.read((char*)data.data(), fileSize);
    file.close();

    printf("Arquivo: %s (%zu bytes)\n", argv[1], fileSize);

    // 1. Check MThd
    if (fileSize < 14 || memcmp(data.data(), "MThd", 4) != 0) {
        printf("ERRO: sem MThd\n"); return 1;
    }

    uint32_t headerLen = be32(data.data() + 4);
    uint16_t format    = be16(data.data() + 8);
    uint16_t numTracks = be16(data.data() + 10);
    uint16_t ppq       = be16(data.data() + 12);

    printf("MThd: format=%d tracks=%d ppq=%d headerLen=%u\n", format, numTracks, ppq, headerLen);

    // 2. Calculate MIDI-only end offset (same as findEndOfTrackOffset)
    size_t offset = 8 + headerLen;
    printf("Track area starts at offset %zu\n", offset);

    // For each track declared in the header
    size_t totalMidiEnd = offset;
    for (int t = 0; t < numTracks; ++t) {
        if (offset + 8 > fileSize) {
            printf("ERRO: nao ha espaco para track %d header em offset %zu\n", t, offset);
            return 1;
        }

        char tag[5] = {};
        memcmp(data.data() + offset, "MTrk", 4);
        memcpy(tag, data.data() + offset, 4);
        uint32_t trackLen = be32(data.data() + offset + 4);

        printf("  Track %d: tag='%s' offset=%zu len=%u end=%zu\n",
               t, tag, offset, trackLen, offset + 8 + trackLen);

        if (memcmp(data.data() + offset, "MTrk", 4) != 0) {
            printf("  AVISO: tag nao e MTrk!\n");
        }

        offset += 8 + trackLen;
    }
    totalMidiEnd = offset;

    printf("\nMIDI-only portion: 0 to %zu (%zu bytes)\n", totalMidiEnd, totalMidiEnd);
    printf("Remaining data after MIDI: %zu bytes\n", fileSize - totalMidiEnd);

    // 3. Check what's at totalMidiEnd
    if (totalMidiEnd < fileSize) {
        printf("First bytes after MIDI: ");
        for (size_t i = totalMidiEnd; i < totalMidiEnd + 16 && i < fileSize; ++i)
            printf("%02X ", data[i]);
        printf("\n");

        // Check for known tags
        if (totalMidiEnd + 4 <= fileSize) {
            char postTag[5] = {};
            memcpy(postTag, data.data() + totalMidiEnd, 4);
            printf("Post-MIDI tag: '%s'\n", postTag);
        }
    }

    // 4. Simulate what JUCE readFrom() does with the truncated data
    // JUCE's check: after reading header (14 bytes) + track chunks (8+trackLen each),
    // size must be exactly 0
    size_t simSize = totalMidiEnd;
    size_t simConsumed = 0;

    // parseMidiHeader: reads MThd(4) + length(4) + data(6) = 14 bytes
    simConsumed += 14; // assuming headerLen=6

    // For each track: reads chunkType(4) + chunkSize(4) + chunkData
    for (int t = 0; t < numTracks; ++t) {
        size_t trkOffset = simConsumed;
        if (trkOffset + 8 > simSize) {
            printf("\nJUCE SIM: failed at track %d - not enough data\n", t);
            break;
        }
        uint32_t chunkSize = be32(data.data() + trkOffset + 4);
        simConsumed += 8 + chunkSize;
    }

    printf("\nJUCE SIMULATION:\n");
    printf("  Truncated size: %zu\n", simSize);
    printf("  Bytes consumed: %zu\n", simConsumed);
    printf("  Remaining: %zu\n", simSize - simConsumed);
    printf("  readFrom would return: %s\n", (simSize == simConsumed) ? "TRUE" : "FALSE");

    return 0;
}
