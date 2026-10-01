// pak.c — arcade game packs are embedded directly in the NDS ROM.
// This intentionally avoids NitroFS/argv/TWiLight dependencies for the arcade games.
// SD/DLDI is still used by save.c for save data.
#include "qu.h"

extern const u8 qu33ph_mini_embedded[];
extern const u32 qu33ph_mini_embedded_size;
extern const u8 qu33ph_ball_embedded[];
extern const u32 qu33ph_ball_embedded_size;

const u8 *arcPak;
int pakErr;
static char cur[16];

int pakIs(const char *name) { return arcPak && !strcmp(cur, name); }

int pakUse(const char *name, u32 size, u32 id) {
    if (pakIs(name)) return 1;

    // The embedded packs live in ROM and must never be freed.
    musicStop();
    for (int ch = 0; ch < 16; ch++) soundKill(ch);
    arcPak = 0;
    cur[0] = 0;

    const u8 *src = NULL;
    u32 srcSize = 0;
    if (!strcmp(name, "mini.pak")) {
        src = qu33ph_mini_embedded;
        srcSize = qu33ph_mini_embedded_size;
    } else if (!strcmp(name, "ball.pak")) {
        src = qu33ph_ball_embedded;
        srcSize = qu33ph_ball_embedded_size;
    }

    if (!src || srcSize != size) {
        pakErr = 3;
        return 0;
    }

    // Validate the embedded pack exactly as the old file loader did.
    u32 got = src[4] | (src[5] << 8) | (src[6] << 16) | ((u32)src[7] << 24);
    if (memcmp(src, "QPAK", 4) || got != id) {
        pakErr = 3;
        return 0;
    }

    // ROM data is read-only, so no cache flush or RAM copy is needed.
    arcPak = src;
    strcpy(cur, name);
    pakErr = 0;
    return 1;
}
