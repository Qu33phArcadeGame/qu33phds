// pak.c — loads one arcade game's pack (mini.pak, ball.pak, ...) from inside the .nds.
// The build puts every .pak into the cartridge image's file system (NitroFS); the game reads
// the one it needs into memory when you open that game and frees the previous one.
#include "qu.h"
#ifndef PAK_TEST_DIR
#include <filesystem.h>
#endif

const u8 *arcPak;
int pakErr;
static char cur[16];
static int fsState;                 // 0 not tried yet, 1 ready, -1 unavailable

int pakIs(const char *name) { return arcPak && !strcmp(cur, name); }

int pakUse(const char *name, u32 size, u32 id) {
    if (pakIs(name)) return 1;
    // nothing may still be playing out of the pack we're about to free
    musicStop();
    for (int ch = 0; ch < 16; ch++) soundKill(ch);
    free((void *)arcPak); arcPak = 0; cur[0] = 0;

    // 1) inside the .nds (NitroFS)   2) beside it on the SD card, for loaders where 1) can't work
    char path[64]; FILE *f = 0;
#ifdef PAK_TEST_DIR
    fsState = 1; sprintf(path, "%s/%s", PAK_TEST_DIR, name); f = fopen(path, "rb");
#else
    if (fsState == 0) {
        // normally the loader tells the game where its .nds is; if it doesn't (booted as the
        // DSpico's _picoboot.nds, or straight from a cart menu), try where it's likely to be
        static const char *ROMS[] = { 0, "fat:/_picoboot.nds", "sd:/_picoboot.nds", "fat:/qu33ph.nds", "sd:/qu33ph.nds", "fat:/NDS/qu33ph.nds", "sd:/NDS/qu33ph.nds" };
        fsState = -1;
        for (int i = 0; i < 7 && fsState < 0; i++) if (nitroFSInit(ROMS[i])) fsState = 1;
    }
    if (fsState > 0) { sprintf(path, "nitro:/%s", name); f = fopen(path, "rb"); }
    static const char *SD[] = { "sd:/qu33ph/%s", "fat:/qu33ph/%s", "sd:/%s", "fat:/%s", "sd:/NDS/%s", "fat:/NDS/%s" };
    for (int i = 0; !f && i < 6; i++) { sprintf(path, SD[i], name); f = fopen(path, "rb"); }
#endif
    if (!f) { pakErr = fsState < 0 ? 1 : 2; return 0; }
    u8 *buf = malloc(size);
    u32 n = buf ? fread(buf, 1, size, f) : 0;
    int extra = fgetc(f) != EOF;      // a bigger file than expected is an old/mismatched pack too
    fclose(f);
    u32 got = buf ? (buf[4] | (buf[5] << 8) | (buf[6] << 16) | ((u32)buf[7] << 24)) : 0;
    if (!buf || n != size || extra || memcmp(buf, "QPAK", 4) || got != id) { free(buf); pakErr = 3; return 0; }
    DC_FlushRange(buf, size);         // the sound hardware reads memory directly, not through the CPU's cache
    arcPak = buf; strcpy(cur, name); pakErr = 0;
    return 1;
}
