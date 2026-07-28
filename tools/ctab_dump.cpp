#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <fstream>
#include <string>

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static std::string asciiSafe(const uint8_t* p, size_t len) {
    std::string s;
    for (size_t i = 0; i < len; i++)
        s += (p[i] >= 0x20 && p[i] <= 0x7E) ? (char)p[i] : '.';
    return s;
}

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/Volumes/STORAGE/PSR_EMU/public test/ESTILOS PSR/ROCK01.STY";

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) { printf("Cannot open %s\n", path); return 1; }
    size_t sz = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> buf(sz);
    file.read((char*)buf.data(), sz);
    const uint8_t* data = buf.data();

    // Find CASM
    size_t casmOff = 0;
    bool found = false;
    for (size_t i = 0; i + 8 < sz; i++) {
        if (memcmp(data + i, "CASM", 4) == 0) {
            casmOff = i;
            found = true;
            break;
        }
    }
    if (!found) { printf("CASM not found\n"); return 1; }

    uint32_t casmLen = be32(data + casmOff + 4);
    const uint8_t* casmStart = data + casmOff + 8;
    const uint8_t* casmEnd   = casmStart + casmLen;
    if (casmEnd > data + sz) casmEnd = data + sz;

    printf("CASM found at file offset 0x%04zX, data length = %u bytes\n\n", casmOff, casmLen);

    // Iterate CSEGs inside CASM
    const uint8_t* p = casmStart;
    int csegIdx = 0;
    while (p + 8 <= casmEnd) {
        if (memcmp(p, "CSEG", 4) != 0) { p++; continue; }

        uint32_t csegLen = be32(p + 4);
        const uint8_t* csegData = p + 8;
        const uint8_t* csegEnd  = csegData + csegLen;
        if (csegEnd > casmEnd) csegEnd = casmEnd;

        // Find Sdec inside this CSEG to get section name
        std::string sdecName = "(none)";
        {
            const uint8_t* sp = csegData;
            while (sp + 8 <= csegEnd) {
                char tag[5] = {};
                memcpy(tag, sp, 4);
                uint32_t slen = be32(sp + 4);
                if (strcmp(tag, "Sdec") == 0) {
                    // Sdec data is the section name string
                    size_t nameLen = slen;
                    if (sp + 8 + nameLen > csegEnd) nameLen = csegEnd - (sp + 8);
                    sdecName = std::string((const char*)(sp + 8), nameLen);
                    break;
                }
                sp += 8 + slen;
            }
        }

        printf("CSEG#%d [Sdec: \"%s\"]\n", csegIdx, sdecName.c_str());

        // Now iterate all sub-chunks inside this CSEG, print Ctabs
        const uint8_t* sp = csegData;
        int ctabIdx = 0;
        while (sp + 8 <= csegEnd) {
            char tag[5] = {};
            memcpy(tag, sp, 4);
            uint32_t slen = be32(sp + 4);
            const uint8_t* chunkData = sp + 8;

            if (strcmp(tag, "Ctab") == 0) {
                // Extract name field: bytes [1..8] (8 bytes starting at offset 1)
                std::string nameField;
                if (slen >= 9) {
                    nameField = asciiSafe(chunkData + 1, 8);
                } else {
                    nameField = "?";
                }

                printf("  Ctab[%d] (%u bytes):", ctabIdx, slen);
                size_t dumpLen = slen;
                if (chunkData + dumpLen > csegEnd) dumpLen = csegEnd - chunkData;
                for (size_t i = 0; i < dumpLen; i++)
                    printf(" %02X", chunkData[i]);
                printf("  name=\"%s\"\n", nameField.c_str());
                ctabIdx++;
            }
            else if (strcmp(tag, "Sdec") == 0) {
                // already printed above, skip
            }
            else {
                // Print any other chunk type for completeness
                printf("  %s[%u bytes]\n", tag, slen);
            }

            sp += 8 + slen;
        }

        printf("\n");
        p = csegData + csegLen; // advance past this CSEG
        csegIdx++;
    }

    printf("Total CSEGs: %d\n", csegIdx);
    return 0;
}
