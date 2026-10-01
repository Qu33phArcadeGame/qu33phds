// save.c — the save file, coins, achievements, high scores, themes
#include "qu.h"
#include <fat.h>

SaveData sv;
int saveOK;
#define SAVE_MAGIC 0x51553350   // "QU3P"
#define SAVE_VERSION 1

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
const char *THEME_NAME[THEME_COUNT] = { "REALISTIC", "CARTOON", "NIGHT", "SUNSET", "NEON", "GOLDEN" };

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
            SaveData t;
            if (fread(&t, 1, sizeof t, f) == sizeof t && t.magic == SAVE_MAGIC && t.version == SAVE_VERSION) sv = t;
            fclose(f); path = PATHS[i]; saveOK = 1;
        } else {
            f = fopen(PATHS[i], "wb");                  // first run: create it
            if (f) { fwrite(&sv, 1, sizeof sv, f); fclose(f); path = PATHS[i]; saveOK = 1; }
        }
    }
}
void saveWrite(void) {
    if (!saveOK) return;
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(&sv, 1, sizeof sv, f); fclose(f); }
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

// ── themes: colour-graded copies of the field photo ───────────────────────
static u16 fieldTint[FIELD_W * FIELD_H];
const u16 *fieldPix = field;
static int clamp31(int v) { return v < 0 ? 0 : v > 31 ? 31 : v; }
void applyTheme(void) {
    int t = sv.theme;
    if (t <= 1) { fieldPix = field; return; }        // REALISTIC and CARTOON use the photo as-is
    for (int i = 0; i < FIELD_W * FIELD_H; i++) {
        u16 p = field[i]; int r = p & 31, g = (p >> 5) & 31, b = (p >> 10) & 31, l = (r * 3 + g * 5 + b * 2) / 10;
        int R, G, B;
        if (t == 2)      { R = r * 45 / 100;      G = g * 50 / 100;      B = b * 80 / 100 + 3; }   // NIGHT
        else if (t == 3) { R = r + 4;             G = g * 75 / 100 + 1;  B = b * 55 / 100; }       // SUNSET
        else if (t == 4) { R = l * 60 / 100 + (b > r ? 2 : 9); G = g * 40 / 100; B = b * 70 / 100 + 9; }   // NEON
        else             { R = l * 11 / 10 + 4;   G = l * 9 / 10 + 2;    B = l * 45 / 100; }       // GOLDEN
        fieldTint[i] = clamp31(R) | (clamp31(G) << 5) | (clamp31(B) << 10) | 0x8000;
    }
    fieldPix = fieldTint;
}
