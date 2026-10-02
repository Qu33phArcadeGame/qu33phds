// save.c — the save file, coins, achievements, high scores, themes
#include "qu.h"
#include <fat.h>
#include <stddef.h>

SaveData sv;
int saveOK;
#define SAVE_MAGIC 0x51553350   // "QU3P"
#define SAVE_VERSION 2
#define SAVE_V1_SIZE offsetof(SaveData, arcadeBest)   // version 1 files end where the arcade fields begin

// ── the website's shop, achievements and themes ───────────────────────────
const char *SHOP_NAME[SH_COUNT] = { "Coin Doubler", "Coin Magnet", "Extra Time", "Mega Boost", "Peef Guard",
                                    "Marker Glow", "Marker Trails", "Confetti", "Golden Slot Machine" };
const char *SHOP_DESC[SH_COUNT] = { "earn 2x coins from everything", "+1 coin per 15 points scored", "+5 seconds on every round",
                                    "higher chance of a mega qu33ph", "halve out-of-bounds penalties", "intense colour glow on markers",
                                    "markers leave a colour trail", "confetti burst on every QU33PH", "5x & 10x spins, 5x/10x payout" };
const int SHOP_PRICE[SH_COUNT] = { 100, 50, 80, 60, 50, 40, 30, 30, 100 };
const char *ACH_NAME[ACH_COUNT] = { "FIRST SLIDE", "QU33PH", "MEGA QU33PH", "MEGA MACHINE", "MEGA MASTER", "FIRST PEEF",
    "PEEF VETERAN", "REGULAR", "DEDICATED", "CENTURION", "QU33PH PRO", "CLUTCH", "MEGA LEGEND", "PEEF LORD",
    "DOUBLE DIGITS", "HIGH ROLLER", "PODIUM", "OLYMPIC GOLD", "ONE-ARMED", "LUCKY", "JACKPOT!", "SAVER", "HIGH SOCIETY", "STYLIST" };
const char *ACH_DESC[ACH_COUNT] = { "Play your first game", "Land all 3 markers touching", "Hit the red dot in sudden death",
    "5 MEGA QU33PHs total", "10 MEGA QU33PHs total", "Drop a marker off the left edge", "10 PEEFs career total", "Play 10 games",
    "Play 50 games", "Play 100 games", "Land 10 qu33phs total", "Land a qu33ph in sudden death", "25 MEGA QU33PHs total",
    "50 PEEFs career total", "Score 10+ in a game", "Score 25+ in a game", "Win any Olympic medal", "Win Olympic gold",
    "Spin the slot machine", "Win on the slot machine", "Hit the slot jackpot", "Earn 100 coins total", "Earn 500 coins total",
    "Unlock a new theme" };
const char *THEME_NAME[THEME_COUNT] = { "REALISTIC", "CARTOON", "NIGHT", "GOLDEN", "NEON", "GOLDEN" };   // 3 (SUNSET) became GOLDEN

// ── file ──────────────────────────────────────────────────────────────────
#ifndef SAVE_TEST_PATH
static const char *PATHS[2] = { "fat:/qu33ph_ds.sav", "sd:/qu33ph_ds.sav" };
#else
static const char *PATHS[2] = { SAVE_TEST_PATH, SAVE_TEST_PATH };
#endif
static const char *path;

static void defaults(void) {
    memset(&sv, 0, sizeof sv);
    sv.magic = SAVE_MAGIC; sv.version = SAVE_VERSION;
    sv.themesOwned = 1; sv.theme = 0;
    sv.musicOn = 1; sv.sfxOn = 1; sv.twoRounds = 5; sv.timer1p = 30; sv.timer2p = 15; sv.orient = 0;
}
void saveInit(void) {
    defaults();
    saveOK = 0;
    // the flash cart's microSD (or a DSi's own SD slot). No card → play on, just nothing is kept.
    if (!fatInitDefault()) return;
    for (int i = 0; i < 2 && !saveOK; i++) {
        FILE *f = fopen(PATHS[i], "rb");
        if (f) {
            SaveData t; memset(&t, 0, sizeof t);
            size_t n = fread(&t, 1, sizeof t, f);
            if (t.magic == SAVE_MAGIC && ((t.version == SAVE_VERSION && n == sizeof t) || (t.version == 1 && n >= SAVE_V1_SIZE))) {
                if (t.version == 1) memset((char *)&t + SAVE_V1_SIZE, 0, sizeof t - SAVE_V1_SIZE);   // keep all v1 progress
                sv = t; sv.version = SAVE_VERSION;
                if (sv.themesOwned & 8) sv.themesOwned |= 32;          // SUNSET merged into GOLDEN
                if (sv.theme == 3) sv.theme = 5;
            }
            fclose(f); path = PATHS[i]; saveOK = 1;
        } else {
            f = fopen(PATHS[i], "wb");                  // first run: create it
            if (f) { fwrite(&sv, 1, sizeof sv, f); fclose(f); path = PATHS[i]; saveOK = 1; }
        }
    }
}
// Writing to the SD card takes a noticeable moment on a real DS, and screens save as you leave
// them, so only write when something actually changed since the last write.
static SaveData lastSaved; static int haveSaved;
void saveWrite(void) {
    if (!saveOK || (haveSaved && !memcmp(&lastSaved, &sv, sizeof sv))) return;
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(&sv, 1, sizeof sv, f); fclose(f); lastSaved = sv; haveSaved = 1; }
}
void resetHighScores(void) { memset(sv.high1p, 0, sizeof sv.high1p); memset(sv.highOly, 0, sizeof sv.highOly); saveWrite(); }
void resetEverything(void) { defaults(); applyTheme(); saveWrite(); }

// ── coins & achievements ──────────────────────────────────────────────────
int shopActive(int it) { return (sv.shopOwned & (1u << it)) && (sv.shopOn & (1u << it)); }
void unlockAch(int a) {
    if (sv.ach & (1u << a)) return;
    sv.ach |= 1u << a;
    sv.coins += 1;                                   // +1 coin per achievement (not doubled, as on the website)
    toast("ACHIEVEMENT UNLOCKED", ACH_NAME[a]);
}
void spendCoins(int n) { sv.coins -= n; sv.coinsSpent += n; }
void addCoins(int n) {
    if (n > 0 && shopActive(SH_DOUBLER)) n *= 2;
    sv.coins += n;
    if (n > 0) {
        sv.coinsEarned += n;
        if (sv.coinsEarned >= 100) unlockAch(A_COINS_100);
        if (sv.coinsEarned >= 500) unlockAch(A_COINS_500);
    }
}
void trackGameEnd(int s2) {
    sv.games++;
    unlockAch(A_FIRST_GAME);
    if (sv.games >= 10) unlockAch(A_TEN_GAMES);
    if (sv.games >= 50) unlockAch(A_FIFTY_GAMES);
    if (sv.games >= 100) unlockAch(A_HUNDRED_GAMES);
    if (s2 >= 20) unlockAch(A_SCORE_10);
    if (s2 >= 50) unlockAch(A_SCORE_25);
}
void trackQu33ph(int sd) {
    sv.qu33phs++; unlockAch(A_FIRST_QU33PH);
    if (sv.qu33phs >= 10) unlockAch(A_TEN_QU33PH);
    if (sd) unlockAch(A_SD_QU33PH);
}
void trackMega(void) {
    sv.megas++; unlockAch(A_FIRST_MEGA);
    if (sv.megas >= 5) unlockAch(A_FIVE_MEGA);
    if (sv.megas >= 10) unlockAch(A_TEN_MEGA);
    if (sv.megas >= 25) unlockAch(A_MEGA_25);
}
void trackPeef(void) {
    sv.peefs++; unlockAch(A_FIRST_PEEF);
    if (sv.peefs >= 10) unlockAch(A_TEN_PEEF);
    if (sv.peefs >= 50) unlockAch(A_FIFTY_PEEF);
}

// ── Top 10 tables ─────────────────────────────────────────────────────────
int highQualifies(HighEntry *l, int s2) { return s2 > 0 && (!l[9].name[0] || s2 > l[9].score2); }
void highInsert(HighEntry *l, const char *name, int s2) {
    int i = 9;
    while (i > 0 && (!l[i - 1].name[0] || s2 > l[i - 1].score2)) { l[i] = l[i - 1]; i--; }
    strncpy(l[i].name, name, 8); l[i].name[8] = 0; l[i].score2 = s2;
    saveWrite();
}

// ── themes: the field is stored as 256 colours + an index per pixel (half the size of a
//    full-colour photo). A theme only has to recolour those 256 colours; the strip the game
//    scrolls is then rebuilt once, so drawing a frame costs exactly what it did before.
static u16 fieldTint[FIELD_W * FIELD_H];
const u16 *fieldPix = fieldTint;
static int clamp31(int v) { return v < 0 ? 0 : v > 31 ? 31 : v; }
void applyTheme(void) {
    int t = sv.theme; u16 pal[256];
    for (int i = 0; i < 256; i++) pal[i] = t <= 0 ? (field_pal[i] | 0x8000) : themeTint(field_pal[i], t);   // REALISTIC: the photo as-is
    for (int i = 0; i < FIELD_W * FIELD_H; i++) fieldTint[i] = pal[field[i]];
    if (t == 1)                                       // CARTOON: halftone dots printed over the grey photo
        for (int y = 0; y < FIELD_H; y += 4) for (int x = (y / 4 % 2) * 2; x < FIELD_W; x += 4) {
            u16 *p = &fieldTint[y * FIELD_W + x]; *p = ((*p >> 1) & 0x3DEF) | 0x8000;
        }
    arcadeThemeChanged();
    themeUI(t);
}
// The website's theme filters (CSS filter maths: sepia / saturate / hue-rotate / brightness /
// contrast), pushed further where asked: NEON is the website's green neon, much more saturated
// and contrasty; GOLDEN is its golden hour, leaning much harder on the gold. SUNSET was the same
// look as GOLDEN, so it now shows GOLDEN.
static void mat3(float m[9], float *r, float *g, float *b) {
    float R = m[0] * *r + m[1] * *g + m[2] * *b, G = m[3] * *r + m[4] * *g + m[5] * *b, B = m[6] * *r + m[7] * *g + m[8] * *b;
    *r = R < 0 ? 0 : R > 1 ? 1 : R; *g = G < 0 ? 0 : G > 1 ? 1 : G; *b = B < 0 ? 0 : B > 1 ? 1 : B;
}
static void saturate(float s, float *r, float *g, float *b) {
    float m[9] = { 0.213f + 0.787f * s, 0.715f - 0.715f * s, 0.072f - 0.072f * s, 0.213f - 0.213f * s, 0.715f + 0.285f * s, 0.072f - 0.072f * s,
                   0.213f - 0.213f * s, 0.715f - 0.715f * s, 0.072f + 0.928f * s };
    mat3(m, r, g, b);
}
static void hueRotate(float deg, float *r, float *g, float *b) {
    float a = deg * 0.0174533f, c = fcos(a), s = fsin(a);
    float m[9] = { 0.213f + c * 0.787f - s * 0.213f, 0.715f - c * 0.715f - s * 0.715f, 0.072f - c * 0.072f + s * 0.928f,
                   0.213f - c * 0.213f + s * 0.143f, 0.715f + c * 0.285f + s * 0.140f, 0.072f - c * 0.072f - s * 0.283f,
                   0.213f - c * 0.213f - s * 0.787f, 0.715f - c * 0.715f + s * 0.715f, 0.072f + c * 0.928f + s * 0.072f };
    mat3(m, r, g, b);
}
static void sepia(float k, float *r, float *g, float *b) {
    float m[9] = { 1 - k + k * 0.393f, k * 0.769f, k * 0.189f, k * 0.349f, 1 - k + k * 0.686f, k * 0.168f, k * 0.272f, k * 0.534f, 1 - k + k * 0.131f };
    mat3(m, r, g, b);
}
static void bc(float br, float ct, float *r, float *g, float *b) {          // brightness, then contrast
    float m[9] = { br * ct, 0, 0, 0, br * ct, 0, 0, 0, br * ct }; float o = 0.5f - 0.5f * ct;
    *r = *r * br * ct + o; *g = *g * br * ct + o; *b = *b * br * ct + o; (void)m;
    *r = *r < 0 ? 0 : *r > 1 ? 1 : *r; *g = *g < 0 ? 0 : *g > 1 ? 1 : *g; *b = *b < 0 ? 0 : *b > 1 ? 1 : *b;
}
u16 themeTint(u16 p, int t) {
    if (t <= 0) return p | 0x8000;                                                       // REALISTIC
    float r = (p & 31) / 31.0f, g = ((p >> 5) & 31) / 31.0f, b = ((p >> 10) & 31) / 31.0f;
    if (t == 1) { saturate(0, &r, &g, &b); bc(1.0f, 1.18f, &r, &g, &b); }               // CARTOON: grayscale, contrast 1.18
    else if (t == 2) { r = r * 0.45f; g = g * 0.5f; b = b * 0.8f + 0.1f; }               // NIGHT: moonlit blue
    else if (t == 4) { hueRotate(95, &r, &g, &b); saturate(3.2f, &r, &g, &b); bc(1.15f, 1.35f, &r, &g, &b); }   // NEON (website: hue 95, sat 2, bright 1.12)
    else { sepia(1.0f, &r, &g, &b); saturate(3.0f, &r, &g, &b); bc(1.32f, 1.12f, &r, &g, &b); hueRotate(-8, &r, &g, &b); }  // GOLDEN (website: sepia .72, sat 2.1, bright 1.14, hue -8)
    return COL((int)(r * 31 + 0.5f), (int)(g * 31 + 0.5f), (int)(b * 31 + 0.5f)) | 0x8000;
}
