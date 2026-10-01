// pak.c — loads one arcade game's pack from NitroFS (inside the .nds).
// If NitroFS is unavailable (typically a flashcart loader that doesn't provide argv[0]),
// fall back to the same pack on the SD card. This keeps emulator testing self-contained
// while giving real flashcart hardware a simple fallback.
#include "qu.h"
#include <filesystem.h>

const u8 *arcPak;
int pakErr;
static char cur[16];
static int fsState; // 0 = not tried, 1 = NitroFS ready, -1 = NitroFS unavailable

int pakIs(const char *name) { return arcPak && !strcmp(cur, name); }

static int readPack(FILE *f, u32 size, u32 id) {
    if (!f) return 0;
    u8 *buf = malloc(size);
    if (!buf) { fclose(f); pakErr = 3; return 0; }
    u32 n = fread(buf, 1, size, f);
    int extra = fgetc(f) != EOF;
    fclose(f);
    u32 got = n >= 8 ? (buf[4] | (buf[5] << 8) | (buf[6] << 16) | ((u32)buf[7] << 24)) : 0;
    if (n != size || extra || memcmp(buf, "QPAK", 4) || got != id) {
        free(buf); pakErr = 3; return 0;
    }
    DC_FlushRange(buf, size);
    arcPak = buf;
    return 1;
}

int pakUse(const char *name, u32 size, u32 id) {
    if (pakIs(name)) return 1;

    musicStop();
    for (int ch = 0; ch < 16; ch++) soundKill(ch);
    free((void *)arcPak); arcPak = 0; cur[0] = 0;

    // First choice: packs embedded in the ROM's NitroFS. On melonDS and other
    // cartridge/emulator environments this requires no external files.
    if (fsState == 0) fsState = nitroFSInit(NULL) ? 1 : -1;
    if (fsState > 0) {
        char path[64];
        sprintf(path, "nitro:/%s", name);
        FILE *f = fopen(path, "rb");
        if (f && readPack(f, size, id)) {
            strcpy(cur, name); pakErr = 0; return 1;
        }
    }

    // Hardware fallback: if the loader can't supply argv[0], put the same
    // .pak files beside the ROM on the SD card. We try both NDS/ and root.
    const char *paths[2];
    char p0[80], p1[80];
    sprintf(p0, "fat:/NDS/%s", name);
    sprintf(p1, "fat:/%s", name);
    paths[0] = p0; paths[1] = p1;
    for (int i = 0; i < 2; i++) {
        FILE *f = fopen(paths[i], "rb");
        if (f && readPack(f, size, id)) {
            strcpy(cur, name); pakErr = 0; return 1;
        }
    }

    pakErr = (fsState < 0) ? 1 : 2;
    return 0;
}
