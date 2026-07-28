#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <fstream>

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

void dumpCasm(const uint8_t* data, size_t size, size_t casmOffset) {
    const uint8_t* p = data + casmOffset;
    const uint8_t* fileEnd = data + size;
    
    if (p + 8 > fileEnd || memcmp(p, "CASM", 4) != 0) {
        printf("No CASM at offset %zu\n", casmOffset);
        return;
    }
    uint32_t casmLen = be32(p + 4);
    printf("CASM at offset %zu, len=%u\n", casmOffset, casmLen);
    
    const uint8_t* end = p + 8 + casmLen;
    p += 8;
    
    int depth = 0;
    while (p + 8 <= end && p + 8 <= fileEnd) {
        char tag[5] = {};
        memcpy(tag, p, 4);
        uint32_t len = be32(p + 4);
        
        bool printable = true;
        for (int k = 0; k < 4; ++k)
            if (tag[k] < 0x20 || tag[k] > 0x7E) { printable = false; break; }
        
        if (!printable) { p++; continue; }
        
        for (int i = 0; i < depth; i++) printf("  ");
        printf("tag=\"%s\" len=%u", tag, len);
        
        if (strncmp(tag, "CSEG", 4) == 0 || strncmp(tag, "Cseg", 4) == 0) {
            printf("\n");
            // Enter the segment
            const uint8_t* segEnd = p + 8 + len;
            const uint8_t* sp = p + 8;
            while (sp + 8 <= segEnd) {
                char stag[5] = {};
                memcpy(stag, sp, 4);
                uint32_t slen = be32(sp + 4);
                
                printf("    tag=\"%s\" len=%u", stag, slen);
                if (strncmp(stag, "Ctab", 4) == 0 && slen >= 18) {
                    const uint8_t* d = sp + 8;
                    printf("  srcCh=%d name=%.8s destCh=%d highKey=%d NTR=%d NTT=%d noteLo=%d noteHi=%d mute=%04X",
                           d[0] & 0x0F,
                           (const char*)(d+1),
                           d[9] & 0x0F,
                           d[11],
                           d[12],
                           d[13] & 0x03,
                           d[14], d[15],
                           (d[16]<<8)|d[17]);
                }
                else if (strncmp(stag, "Ctb2", 4) == 0 && slen >= 9) {
                    const uint8_t* d = sp + 8;
                    printf("  srcCh=%d destCh=%d flags=%02X NTR=%d NTT=%d highKey=%d noteLo=%d noteHi=%d rTag=%d",
                           d[0] & 0x0F, d[1] & 0x0F, d[2], d[3]&0x03, d[4]&0x03, d[5], d[6], d[7], d[8]);
                }
                printf("\n");
                
                // Dump first 32 bytes hex
                size_t dumpLen = (slen < 32) ? slen : 32;
                printf("      hex: ");
                for (size_t i = 0; i < dumpLen && sp+8+i < fileEnd; i++)
                    printf("%02X ", sp[8+i]);
                printf("\n");
                
                sp += 8 + slen;
            }
            p += 8 + len;
        } else {
            printf("\n");
            p += 8 + len;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: %s file.sty\n", argv[0]); return 1; }
    
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file.is_open()) { printf("Cannot open file\n"); return 1; }
    size_t sz = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(sz);
    file.read((char*)data.data(), sz);
    
    // Find CASM
    for (size_t i = 0; i + 8 < sz; i++) {
        if (memcmp(data.data() + i, "CASM", 4) == 0) {
            dumpCasm(data.data(), sz, i);
            break;
        }
    }
    return 0;
}
