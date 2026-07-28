/**
 * sysex_dump.cpp — Dump ALL raw bytes of SysEx messages + PC/CC in the first N ticks of a .sty file.
 * Compile: clang++ -std=c++17 sysex_dump.cpp -o sysex_dump
 * Usage:   ./sysex_dump file.sty [max_tick=1000]
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static uint32_t readVlq(const uint8_t* data, size_t size, size_t& pos) {
    uint32_t value = 0;
    for (int i = 0; i < 4 && pos < size; ++i) {
        uint8_t b = data[pos++];
        value = (value << 7) | (b & 0x7F);
        if (!(b & 0x80)) break;
    }
    return value;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: %s file.sty [max_tick=1000]\n", argv[0]); return 1; }

    uint32_t maxTick = 1000;
    if (argc >= 3) maxTick = (uint32_t)atoi(argv[2]);

    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file.is_open()) { printf("ERROR: cannot open file\n"); return 1; }
    size_t fileSize = file.tellg(); file.seekg(0);
    std::vector<uint8_t> data(fileSize);
    file.read((char*)data.data(), fileSize);

    if (fileSize < 14 || memcmp(data.data(), "MThd", 4) != 0) { printf("Not MIDI\n"); return 1; }
    uint32_t headerLen = be32(data.data()+4);
    size_t trackOff = 8 + headerLen;
    if (trackOff+8 > fileSize || memcmp(data.data()+trackOff, "MTrk", 4) != 0) {
        printf("MTrk not found\n"); return 1;
    }
    uint32_t trackLen = be32(data.data()+trackOff+4);

    const uint8_t* trk = data.data() + trackOff + 8;
    size_t trkSize = trackLen;
    size_t pos = 0;
    uint32_t tick = 0;
    uint8_t lastStatus = 0;

    int sysexCount = 0, pcCount = 0, bankMsbCount = 0, bankLsbCount = 0;
    int xgVoiceSysexCount = 0;

    printf("=== SYSEX DUMP: %s (max tick=%u) ===\n", argv[1], maxTick);
    printf("Track offset=%zu  Track len=%u bytes\n\n", trackOff, trackLen);

    while (pos < trkSize) {
        uint32_t delta = readVlq(trk, trkSize, pos);
        tick += delta;
        if (tick > maxTick) break;
        if (pos >= trkSize) break;

        uint8_t status = trk[pos];
        if (status < 0x80) { status = lastStatus; }
        else { lastStatus = status; pos++; }

        if (status == 0xFF) { // Meta
            uint8_t type = trk[pos++];
            uint32_t len = readVlq(trk, trkSize, pos);
            if (type == 0x06) { // Marker
                printf("tick=%-6u MARKER: \"%.*s\"\n", tick, (int)len, trk+pos);
            } else if (type == 0x51 && len >= 3) {
                uint32_t uspb = ((uint32_t)trk[pos]<<16)|((uint32_t)trk[pos+1]<<8)|trk[pos+2];
                printf("tick=%-6u TEMPO: %.1f BPM\n", tick, 60000000.0/uspb);
            } else if (type == 0x2F) {
                printf("tick=%-6u END OF TRACK\n", tick);
                pos += len;
                break;
            } else if (type == 0x03) {
                printf("tick=%-6u TRACK_NAME: \"%.*s\"\n", tick, (int)len, trk+pos);
            } else if (type == 0x01) {
                printf("tick=%-6u TEXT: \"%.*s\"\n", tick, (int)len, trk+pos);
            }
            pos += len;

        } else if (status == 0xF0) { // SysEx
            uint32_t len = readVlq(trk, trkSize, pos);
            sysexCount++;
            printf("tick=%-6u SYSEX [%u bytes]: F0", tick, len+1); // +1 for the F0 byte
            // Print ALL bytes - no truncation
            for (uint32_t j = 0; j < len && pos+j < trkSize; j++)
                printf(" %02X", trk[pos+j]);
            printf("\n");

            // Decode Yamaha XG SysEx
            if (len >= 7 && trk[pos] == 0x43 && (trk[pos+1] & 0xF0) == 0x10 && trk[pos+2] == 0x4C) {
                uint8_t addrH = trk[pos+3], addrM = trk[pos+4], addrL = trk[pos+5];
                if (addrH == 0x08) { // XG Multi Part
                    int part = addrM;
                    printf("    --> XG Multi Part %d (MIDI ch %d): ", part, part+1);
                    if (addrL == 0x01)      { printf("BANK_MSB = %d", trk[pos+6]); }
                    else if (addrL == 0x02) { printf("BANK_LSB = %d", trk[pos+6]); }
                    else if (addrL == 0x03) { printf("PROGRAM_NUMBER = %d", trk[pos+6]); xgVoiceSysexCount++; }
                    else if (addrL == 0x00) { printf("Element_Reserve = %d", trk[pos+6]); }
                    else if (addrL == 0x04) { printf("Rcv_Channel = %d", trk[pos+6]); }
                    else if (addrL == 0x05) { printf("Mono/Poly = %d", trk[pos+6]); }
                    else if (addrL == 0x07) { printf("Part_Mode = %d (0=normal,1=drum,2=drums1,3=drums2)", trk[pos+6]); }
                    else if (addrL == 0x08) { printf("Note_Shift = %d", trk[pos+6]); }
                    else if (addrL == 0x09) { printf("Detune = %d %d", trk[pos+6], len>7?trk[pos+7]:0); }
                    else if (addrL == 0x0B) { printf("Volume = %d", trk[pos+6]); }
                    else if (addrL == 0x0C) { printf("Vel_Sense_Depth = %d", trk[pos+6]); }
                    else if (addrL == 0x0D) { printf("Vel_Sense_Offset = %d", trk[pos+6]); }
                    else if (addrL == 0x0E) { printf("Pan = %d", trk[pos+6]); }
                    else if (addrL == 0x11) { printf("Dry_Level = %d", trk[pos+6]); }
                    else if (addrL == 0x12) { printf("Chorus_Send = %d", trk[pos+6]); }
                    else if (addrL == 0x13) { printf("Reverb_Send = %d", trk[pos+6]); }
                    else if (addrL == 0x14) { printf("Variation_Send = %d", trk[pos+6]); }
                    else { printf("param_0x%02X = %d", addrL, trk[pos+6]); }
                    printf("\n");

                    // Check for multi-byte writes (consecutive addresses)
                    if (len > 8) { // Multiple parameters in one SysEx
                        printf("    --> (multi-byte write starting at addr 0x%02X, %u data bytes)\n",
                               addrL, len - 7); // subtract header(3)+addr(3)+F7(1)
                    }
                } else if (addrH == 0x00 && addrM == 0x00 && addrL == 0x7E) {
                    printf("    --> XG SYSTEM ON\n");
                } else if (addrH == 0x02) {
                    printf("    --> XG Effect 1 (Reverb): param %02X/%02X = %d\n", addrM, addrL, trk[pos+6]);
                } else if (addrH == 0x02 && addrM == 0x01) {
                    printf("    --> XG Effect 2 (Chorus)\n");
                } else {
                    printf("    --> XG addr=%02X/%02X/%02X data=%d\n", addrH, addrM, addrL, trk[pos+6]);
                }
            } else if (len >= 3 && trk[pos] == 0x43) {
                printf("    --> Yamaha SysEx (model byte: 0x%02X, sub: 0x%02X)\n",
                       trk[pos+2], len > 3 ? trk[pos+3] : 0);
            } else if (len >= 3 && trk[pos] == 0x7E) {
                printf("    --> Universal Non-Realtime (sub1=%02X sub2=%02X)\n", trk[pos+1], trk[pos+2]);
            } else if (len >= 3 && trk[pos] == 0x7F) {
                printf("    --> Universal Realtime\n");
            }
            pos += len;

        } else if (status == 0xF7) { // SysEx continuation
            uint32_t len = readVlq(trk, trkSize, pos);
            printf("tick=%-6u SYSEX_CONT F7 [%u bytes]:", tick, len);
            for (uint32_t j = 0; j < len && pos+j < trkSize; j++)
                printf(" %02X", trk[pos+j]);
            printf("\n");
            pos += len;

        } else {
            uint8_t type = status & 0xF0;
            uint8_t ch = status & 0x0F;
            if (type == 0xC0) {
                uint8_t pc = trk[pos++];
                printf("tick=%-6u PC      ch=%-2d  program=%-3d\n", tick, ch+1, pc);
                pcCount++;
            } else if (type == 0xB0) {
                uint8_t cc = trk[pos++], val = trk[pos++];
                const char* ccName = "";
                if (cc == 0)       { ccName = " (Bank MSB)"; bankMsbCount++; }
                else if (cc == 32) { ccName = " (Bank LSB)"; bankLsbCount++; }
                else if (cc == 7)  ccName = " (Volume)";
                else if (cc == 10) ccName = " (Pan)";
                else if (cc == 11) ccName = " (Expression)";
                else if (cc == 91) ccName = " (Reverb Send)";
                else if (cc == 93) ccName = " (Chorus Send)";
                else if (cc == 1)  ccName = " (Mod Wheel)";
                else if (cc == 64) ccName = " (Sustain)";
                else if (cc == 71) ccName = " (Resonance/Timbre)";
                else if (cc == 74) ccName = " (Brightness)";
                else if (cc == 65) ccName = " (Portamento)";
                else if (cc == 5)  ccName = " (Portamento Time)";
                else if (cc == 72) ccName = " (Release Time)";
                else if (cc == 73) ccName = " (Attack Time)";
                else if (cc == 76) ccName = " (Vibrato Rate)";
                else if (cc == 77) ccName = " (Vibrato Depth)";
                else if (cc == 78) ccName = " (Vibrato Delay)";
                printf("tick=%-6u CC      ch=%-2d  cc=%-3d val=%-3d%s\n",
                       tick, ch+1, cc, val, ccName);
            } else if (type == 0x90 || type == 0x80) { pos += 2; }
            else if (type == 0xD0) { pos++; }
            else if (type == 0xE0 || type == 0xA0) { pos += 2; }
            else { pos++; }
        }
    }

    printf("\n=== SUMMARY (ticks 0-%u) ===\n", maxTick);
    printf("  SysEx messages:        %d\n", sysexCount);
    printf("  XG Voice SysEx (PC):   %d\n", xgVoiceSysexCount);
    printf("  Standard MIDI PC msgs: %d\n", pcCount);
    printf("  Bank Select MSB (CC0): %d\n", bankMsbCount);
    printf("  Bank Select LSB(CC32): %d\n", bankLsbCount);
    printf("\n");

    if (xgVoiceSysexCount > 0 && pcCount == 0) {
        printf("*** FINDING: This file uses XG SysEx for voice selection (NO standard PC messages) ***\n");
    } else if (xgVoiceSysexCount > 0 && pcCount > 0) {
        printf("*** FINDING: This file uses BOTH XG SysEx and standard PC for voice selection ***\n");
    } else if (pcCount > 0) {
        printf("*** FINDING: This file uses standard MIDI PC messages for voice selection ***\n");
    } else {
        printf("*** FINDING: No voice selection messages found in this range ***\n");
    }

    return 0;
}
