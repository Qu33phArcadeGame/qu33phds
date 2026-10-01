// pak.c — loads one arcade game's pack (mini.pak, ball.pak, ...) from inside the .nds.
// The build puts every .pak into the cartridge image's file system (NitroFS); the game reads
// the one it needs into memory when you open that game and frees the previous one.
#include "qu.h"
#include <fat.h>
#ifndef PAK_TEST_DIR
#include <filesystem.h>
#endif

const u8 *arcPak;
int pakErr;
static char cur[16];
static int fsState;                 // NitroFS: 0 not tried, 1 ready, -1 unavailable
static int fatState;                // external SD fallback: 0 not tried, 1 ready, -1 unavailable

int pakIs(const char *name) { return arcPak && !strcmp(cur, name); }

int pakUse(const char *name, u32 size, u32 id) {
    if (pakIs(name)) return 1;
    // nothing may still be playing out of the pack we're about to free
    musicStop();
    for (int ch = 0; ch < 16; ch++) soundKill(ch);
    free((void *)arcPak); arcPak = 0; cur[0] = 0;

    char path[64];
    FILE *f = NULL;
#ifdef PAK_TEST_DIR
    fsState = 1; sprintf(path, "%s/%s", PAK_TEST_DIR, name);
    f = fopen(path, "rb");
#else
    // Normal builds keep the packs inside NitroFS. This works from melonDS and
    // from loaders that provide argv[0]. Some older R4 loaders don't provide
    // argv[0], however, so keep an SD-card fallback as well.
    if (fsState == 0) fsState = nitroFSInit(NULL) ? 1 : -1;
    if (fsState > 0) {
        sprintf(path, "nitro:/%s", name);
        f = fopen(path, "rb");
    }

    // If NitroFS can't be initialized (usually an old loader without argv),
    // allow mini.pak/ball.pak to be placed beside the .nds on the flashcart.
    // fatInitDefault() is called during startup by saveInit(), so fat:/ is the
    // correct BlocksDS DLDI filesystem for an R4/flashcart.
    if (!f) {
        if (fatState == 0) fatState = fatInitDefault() ? 1 : -1;
        if (fatState > 0) {
            sprintf(path, "fat:/%s", name);
            f = fopen(path, "rb");
        }
    }
#endif
    if (!f) { pakErr = (fsState < 0 && fatState < 0) ? 1 : 2; return 0; }
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
