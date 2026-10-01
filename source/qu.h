// qu.h — shared by every Qu33ph DS source file
#pragma once
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assets.h"

#define SW 256
#define SH 192
#define COL(r, g, b) (RGB15(r, g, b) | BIT(15))
#define WHITE  COL(31, 31, 31)
#define BLACK  COL(0, 0, 0)
#define YELLOW COL(31, 27, 4)
#define GOLD   COL(31, 24, 2)
#define RED    COL(31, 6, 6)
#define LIME   COL(8, 31, 8)
#define GREY   COL(18, 18, 18)
#define DARK   COL(2, 2, 3)

// ── screens ───────────────────────────────────────────────────────────────
enum { S_TITLE, S_PLAY, S_PAUSE, S_HANDOFF, S_RESULTS, S_SHOP, S_ACH, S_CAREER, S_HIGHS,
       S_SETTINGS, S_THEMES, S_SLOT, S_PLINKO, S_OLY_SELECT, S_OLY_BRACKET, S_NAME,
       S_ARCADE, S_MINI_MENU, S_MINI, S_MINI_PAUSE, S_MINI_OVER,
       S_BALL_MENU, S_BALL, S_BALL_PAUSE, S_BALL_OVER };
extern int screen;
enum { M_SINGLE = 1, M_TWO = 2, M_OLYMPICS = 3 };
extern int mode;

// ── save data (one small file on the microSD card) ────────────────────────
typedef struct { char name[9]; int score2; } HighEntry;          // scores are doubled (half points exist)
typedef struct {
    u32 magic, version;
    int coins, coinsEarned;
    HighEntry high1p[10], highOly[10];
    int games, qu33phs, megas, peefs, slotSpins, slotWins, forfeits;
    int gold, silver, bronze;
    u32 ach;                     // one bit per achievement
    u32 shopOwned, shopOn;       // one bit per shop item
    u32 themesOwned; int theme;
    int musicOn, sfxOn, twoRounds, timer1p, timer2p, orient;
    char name[9];
    // ── added in save version 2 (the arcade). Older saves load untouched; these start at 0.
    int arcadeBest[16], arcadePlays[16];     // one slot per arcade game (see ARC_* in arcade.c)
    int ballBest[3];                         // Qu33ph-Ball: best on each machine (was spare room, so old saves read 0)
    u32 spare[13];                           // room to grow without another version bump
} SaveData;
extern SaveData sv;
extern int saveOK;               // 1 if the microSD card can be written
void saveInit(void);
void saveWrite(void);
void resetHighScores(void);
void resetEverything(void);

// shop items (bit numbers)
enum { SH_DOUBLER, SH_MAGNET, SH_XTIME, SH_MEGABOOST, SH_PEEFGUARD, SH_GLOW, SH_TRAILS, SH_CONFETTI, SH_GOLDSLOT, SH_COUNT };
extern const char *SHOP_NAME[SH_COUNT], *SHOP_DESC[SH_COUNT];
extern const int SHOP_PRICE[SH_COUNT];
int shopActive(int item);        // owned AND switched on

// achievements
#define ACH_COUNT 24
extern const char *ACH_NAME[ACH_COUNT], *ACH_DESC[ACH_COUNT];
enum { A_FIRST_GAME, A_FIRST_QU33PH, A_FIRST_MEGA, A_FIVE_MEGA, A_TEN_MEGA, A_FIRST_PEEF, A_TEN_PEEF, A_TEN_GAMES,
       A_FIFTY_GAMES, A_HUNDRED_GAMES, A_TEN_QU33PH, A_SD_QU33PH, A_MEGA_25, A_FIFTY_PEEF, A_SCORE_10, A_SCORE_25,
       A_FIRST_MEDAL, A_GOLD_MEDAL, A_SLOT_SPIN, A_SLOT_WIN, A_SLOT_JACKPOT, A_COINS_100, A_COINS_500, A_UNLOCK_THEME };
void unlockAch(int a);
void addCoins(int n);            // applies the coin doubler to earnings
void trackGameEnd(int score2);
void trackQu33ph(int suddenDeath);
void trackMega(void);
void trackPeef(void);
int  highQualifies(HighEntry *list, int score2);
void highInsert(HighEntry *list, const char *name, int score2);

// themes
#define THEME_COUNT 6
extern const char *THEME_NAME[THEME_COUNT];
#define THEME_COST 50
void applyTheme(void);           // rebuilds the tinted field for the current theme
u16  themeTint(u16 p, int t);    // one pixel through theme t's colour grade
extern const u16 *fieldPix;      // the field strip the game draws, built from the 256-colour photo per theme

// ── drawing (draw.c) ──────────────────────────────────────────────────────
extern u16 bufTop[SW * SH], bufBot[SW * SH];
void gpx(int x, int gy, u16 c);
extern int gClipLo, gClipHi;
void grect(int x, int gy, int w, int h, u16 c);
void blitRot(const u16 *spr, int w, int h, int cx, int cy, float ang);
void blit(u16 *buf, const u16 *spr, int w, int h, int x, int y);
void blitRotScale(const u16 *spr, int w, int h, int cx, int cy, float ang, float scale);
void gdark(int x, int gy);
void drawIndexed(const u8 *idx, const u16 *pal);   // both screens from a 256-colour picture
void markerShape(u16 *buf, int x, int y, int w, int h, u16 edge, const u16 *grad);
void powerMarker(int x0, int gy0, float ux, float uy, float len, float power);
void rect(u16 *buf, int x, int y, int w, int h, u16 c);
int  textW(const char *t, int sc);
void text(u16 *buf, int x, int y, const char *t, u16 col, int sc);
void textC(u16 *buf, int y, const char *t, u16 col, int sc);
void gtext(int x, int gy, const char *t, u16 col, int sc);
void fillScreen(u16 *buf, u16 c);
void box(u16 *buf, int x, int y, int w, int h, u16 fill, u16 edge);
void scoreStr(char *o, int doubled);
void drawFlag(u16 *buf, int x, int y, int w, int h, int nation);
void coinCount(u16 *buf, int x, int y);
float fsqrt(float v); float fatan2r(float y, float x); float fabsf_(float v); float fsin(float a); float fcos(float a); float frand(void);

// ── buttons: drawn + navigated the same way on every screen ───────────────
typedef struct { int x, y, w, h; char label[28]; u16 col; int dim; } Btn;
void drawBtns(u16 *buf, Btn *b, int n, int sel);
int  btnInput(Btn *b, int n, int *sel, int cols);   // returns pressed index or -1

// ── sound ─────────────────────────────────────────────────────────────────
void sfxThrow(int orient); void sfxPeef(int orient); void sfxPlop(void);
void musicStart(void); void musicStop(void);
enum { MUS_MAIN, MUS_MINI, MUS_BALL };
void musicSet(int track);        // switch tracks (restarts only if it changes)
void musicTick(void);            // once per frame: loops the ADPCM tracks
void playAdpcm(const u8 *d, int len, int rate, int vol);   // one-shot sound effect

// ── game packs (pak.c): each arcade game's art & sound lives in its own file inside the
//    .nds (NitroFS) and is loaded only while you're in that game, so adding games doesn't
//    eat into the DS's 4 MB of memory.
extern const u8 *arcPak;         // the loaded pack (the assets_<game>.h macros point into it)
int  pakUse(const char *name, u32 size, u32 id);   // 1 = ready; 0 = couldn't load (see pakErr)
int  pakIs(const char *name);
extern int pakErr;               // 1 can't open the cartridge's files, 2 file missing, 3 old/mismatched file

// ── the match (game.c) ────────────────────────────────────────────────────
extern int score2[2], player, p2Round, frameCount, roundFrames, totalFrames, suddenDeath;
extern int orient;
void startMatch(void);           // begins a turn in the current mode
void matchUpdate(void);
void matchDraw(void);
void matchInput(int down, int held, int up, int tx, int ty);
extern int matchOver;            // set when the turn's time runs out

// ── extras (extras.c) ─────────────────────────────────────────────────────
#define NATION_COUNT 18
extern const char *NATION[NATION_COUNT];
extern int olyNation;
void olyNew(void);
void olyAfterMatch(int playerScore2);
void drawOlySelect(void); void inputOlySelect(int down, int tx, int ty);
void drawOlyBracket(void); void inputOlyBracket(int down, int tx, int ty);
void drawSlot(void); void inputSlot(int down, int tx, int ty); void updateSlot(void);
void drawPlinko(void); void inputPlinko(int down, int held, int tx, int ty); void updatePlinko(void); void plinkoEnter(void);
extern int olyFinished;

// ── toast: short message on the top screen (achievements, coins) ─────────
void toast(const char *a, const char *b);
void drawToast(void);

// name entry
void nameEntry(int returnScreen, HighEntry *list, int score2);

// ── the arcade (arcade.c) ─────────────────────────────────────────────────
void drawArcade(void); void inputArcade(void);
void drawMiniMenu(void); void inputMiniMenu(void);
void drawMini(void); void inputMini(void); void updateMini(void);
void drawMiniOver(void); void inputMiniOver(void);
void arcadeThemeChanged(void);
enum { ARC_MINI, ARC_BALL, ARC_FIDGET, ARC_BOWLING, ARC_STACK, ARC_FLIP, ARC_DOZER, ARC_JUMP, ARC_PINBALL, ARC_COUNT };
void miniThemeChanged(void);
// Qu33ph-Ball (ball.c)
int  ballEnter(void);            // loads ball.pak; 0 if it couldn't
void ballThemeChanged(void);
void drawBallMenu(void); void inputBallMenu(void);
void drawBall(void); void inputBall(void); void updateBall(void);
void drawBallOver(void); void inputBallOver(void);
extern int gStip;                // 1 = draw sprites see-through (every other pixel)

// this frame's input (set once per frame in main.c)
extern int kDown, kHeld, kUp, tX, tY;

// screen changes & match hooks shared between main.c and extras.c
void goScreen(int s);
void startOlympicMatch(void);
void olympicsDone(void);
int  olyTournamentTotal(void);
