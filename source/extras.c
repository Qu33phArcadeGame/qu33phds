// extras.c — Qu33ph Olympics, the slot machine and PlinQu33ph
#include "qu.h"

// ══ OLYMPICS ══════════════════════════════════════════════════════════════
const char *NATION[NATION_COUNT] = { "Ireland", "Israel", "USA", "South Africa", "England", "Scotland", "Norway", "Japan", "Germany",
                                     "Brazil", "Australia", "Canada", "France", "Netherlands", "Sweden", "Russia", "Iran", "China" };
static const char *CODE[NATION_COUNT] = { "IRL", "ISR", "USA", "RSA", "ENG", "SCO", "NOR", "JPN", "GER", "BRA", "AUS", "CAN", "FRA", "NED", "SWE", "RUS", "IRN", "CHN" };
int olyNation, olyFinished;
static int olyN[8];                                  // the 8 nations; the player is olyN[7]
static int qf1[2], qf2[2], qf3[2], qf4[2], sf1[2], sf2[2], fin[2];   // scores (doubled), -1 = not played
static int olyRound, olyOut, olyMedal, olyCoins, olyTotal, olySel;
static const int REWARD[4] = { 0, 5, 10, 15 };      // win QF = 5, SF = 10, FINAL = 15 coins

// Rival scores: the website's formula (85-130), scaled to what's reachable on the DS —
// a flawless DS game (a QU33PH every set) scores about 40-45. Raise OLY_SCALE for a harder Olympics.
#define OLY_SCALE 30          // percent of the website's rival scores
static int simScore(void) { int b = 85 + rand() % 45; if (rand() % 10 < 7) b = 90 + rand() % 30; return b * 2 * OLY_SCALE / 100; }
static int winQ(int *m, int a, int b) { return m[0] > m[1] ? a : b; }
void olyNew(void) {
    int pool[NATION_COUNT], n = 0;
    for (int i = 0; i < NATION_COUNT; i++) if (i != olyNation) pool[n++] = i;
    for (int i = n - 1; i > 0; i--) { int j = rand() % (i + 1), t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    for (int i = 0; i < 7; i++) olyN[i] = pool[i];
    olyN[7] = olyNation;
    int *qs[3] = { qf1, qf2, qf3 };
    for (int k = 0; k < 3; k++) { qs[k][0] = simScore(); do qs[k][1] = simScore(); while (qs[k][1] == qs[k][0]); }
    qf4[0] = qf4[1] = sf1[0] = sf1[1] = sf2[0] = sf2[1] = fin[0] = fin[1] = -1;
    olyRound = 1; olyOut = 0; olyMedal = 0; olyCoins = 0; olyTotal = 0; olyFinished = 0;
}
void olyAfterMatch(int p) {
    int opp; do opp = simScore(); while (opp == p);
    olyTotal += p;
    if (olyRound == 1) {
        qf4[0] = opp; qf4[1] = p;                    // olyN[6] vs player
        sf1[0] = simScore(); do sf1[1] = simScore(); while (sf1[1] == sf1[0]);
        if (p <= opp) { olyOut = 1; olyFinished = 1; }
        else { addCoins(REWARD[1]); olyCoins += REWARD[1]; olyRound = 2; }
    } else if (olyRound == 2) {
        sf2[0] = opp; sf2[1] = p;                    // QF3 winner vs player
        if (p <= opp) { olyOut = 1; olyMedal = 1; olyFinished = 1; }
        else { addCoins(REWARD[2]); olyCoins += REWARD[2]; olyRound = 3; }
    } else {
        fin[0] = opp; fin[1] = p;                    // SF1 winner vs player
        if (p > opp) { olyMedal = 3; addCoins(REWARD[3]); olyCoins += REWARD[3]; } else olyMedal = 2;
        olyFinished = 1;
    }
    if (olyFinished) {
        if (olyMedal == 3) sv.gold++; else if (olyMedal == 2) sv.silver++; else if (olyMedal == 1) sv.bronze++;
        if (olyMedal) unlockAch(A_FIRST_MEDAL);
        if (olyMedal == 3) unlockAch(A_GOLD_MEDAL);
    }
    saveWrite();
}
int olyTournamentTotal(void) { return olyTotal; }

// nation select: tap (or D-pad) to pick, tap the same one again (or A) to confirm
static Btn natB[NATION_COUNT];
static void natLayout(void) {
    for (int i = 0; i < NATION_COUNT; i++) {
        natB[i].x = 4 + (i % 3) * 84; natB[i].y = 4 + (i / 3) * 31; natB[i].w = 80; natB[i].h = 28;
        natB[i].label[0] = 0; natB[i].col = 0; natB[i].dim = 0;
    }
}
void drawOlySelect(void) {
    natLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 14, "QU33PH OLYMPICS", GOLD, 1);
    textC(bufTop, 44, "choose your nation", WHITE, 1);
    drawFlag(bufTop, 88, 70, 80, 53, olySel);
    textC(bufTop, 132, NATION[olySel], WHITE, 2);
    textC(bufTop, 170, "tap again or A to confirm  -  B back", GREY, 1);
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, natB, NATION_COUNT, olySel);
    for (int i = 0; i < NATION_COUNT; i++) {
        drawFlag(bufBot, natB[i].x + 4, natB[i].y + 6, 24, 16, i);
        text(bufBot, natB[i].x + 32, natB[i].y + 8, CODE[i], i == olySel ? YELLOW : WHITE, 1);
    }
}

void inputOlySelect(int down, int tx, int ty) {
    (void)tx; (void)ty;
    natLayout();
    int prev = olySel, hit = btnInput(natB, NATION_COUNT, &olySel, 3);
    if (down & KEY_B) { goScreen(S_TITLE); return; }
    if ((hit >= 0 && (down & KEY_A)) || (hit >= 0 && (down & KEY_TOUCH) && hit == prev)) {
        olyNation = olySel; olyNew(); goScreen(S_OLY_BRACKET);
    }
}

static void matchRow(int y, int a, int b, int *m, const char *lbl) {
    char s[12];
    text(bufTop, 4, y, lbl, GREY, 1);
    drawFlag(bufTop, 40, y + 1, 18, 12, a); text(bufTop, 62, y, CODE[a], a == olyNation ? YELLOW : WHITE, 1);
    if (m[0] >= 0) { sprintf(s, "%d", m[0] / 2); text(bufTop, 98, y, s, m[0] > m[1] ? LIME : GREY, 1); }
    text(bufTop, 122, y, "v", GREY, 1);
    drawFlag(bufTop, 136, y + 1, 18, 12, b); text(bufTop, 158, y, CODE[b], b == olyNation ? YELLOW : WHITE, 1);
    if (m[1] >= 0) { sprintf(s, "%d", m[1] / 2); text(bufTop, 194, y, s, m[1] > m[0] ? LIME : GREY, 1); }
}
static Btn olyBtn[1]; static int olyBtnSel;
void drawOlyBracket(void) {
    fillScreen(bufTop, DARK);
    textC(bufTop, 2, "OLYMPIC BRACKET", GOLD, 1);
    matchRow(20, olyN[0], olyN[1], qf1, "QF1"); matchRow(36, olyN[2], olyN[3], qf2, "QF2");
    matchRow(52, olyN[4], olyN[5], qf3, "QF3"); matchRow(68, olyN[6], olyN[7], qf4, "QF4");
    int w1 = winQ(qf1, olyN[0], olyN[1]), w2 = winQ(qf2, olyN[2], olyN[3]), w3 = winQ(qf3, olyN[4], olyN[5]);
    int s1w = sf1[0] >= 0 ? winQ(sf1, w1, w2) : -1;
    if (sf1[0] >= 0) matchRow(94, w1, w2, sf1, "SF1");
    if (olyRound >= 2 || sf2[0] >= 0) matchRow(110, w3, olyNation, sf2, "SF2");
    if (fin[0] >= 0 || olyRound == 3) matchRow(136, s1w >= 0 ? s1w : w1, olyNation, fin, "FIN");
    fillScreen(bufBot, DARK);
    char s[40];
    if (!olyFinished) {
        const char *r = olyRound == 1 ? "PLAY QUARTER-FINAL" : olyRound == 2 ? "PLAY SEMI-FINAL" : "PLAY THE FINAL";
        textC(bufBot, 30, olyRound == 1 ? "Beat your rival to reach the semis" : olyRound == 2 ? "Win to reach the final" : "Win it all for GOLD", WHITE, 1);
        olyBtn[0] = (Btn){ 38, 70, 180, 40, "", 0, 0 }; strcpy(olyBtn[0].label, r);
        sprintf(s, "coins won so far: %d", olyCoins); textC(bufBot, 130, s, GOLD, 1);
    } else {
        const char *m = olyMedal == 3 ? "GOLD MEDAL!" : olyMedal == 2 ? "SILVER MEDAL" : olyMedal == 1 ? "BRONZE MEDAL" : "KNOCKED OUT";
        u16 c = olyMedal == 3 ? GOLD : olyMedal == 2 ? COL(24, 24, 26) : olyMedal == 1 ? COL(26, 15, 7) : RED;
        textC(bufBot, 16, m, c, 2);
        sprintf(s, "coins won: %d", olyCoins); textC(bufBot, 52, s, GOLD, 1);
        olyBtn[0] = (Btn){ 38, 80, 180, 40, "CONTINUE", 0, 0 };
    }
    drawBtns(bufBot, olyBtn, 1, olyBtnSel);
    textC(bufBot, 170, "B  quit the tournament", GREY, 1);
}


void inputOlyBracket(int down, int tx, int ty) {
    (void)tx; (void)ty;
    if (down & KEY_B) { goScreen(S_TITLE); return; }
    if (btnInput(olyBtn, 1, &olyBtnSel, 1) == 0) { if (olyFinished) olympicsDone(); else startOlympicMatch(); }
}

// ══ SLOT MACHINE ══════════════════════════════════════════════════════════
enum { Y_LOGO, Y_MEGA, Y_COIN, Y_CHAIR, Y_RED, Y_GREEN, Y_BLUE };
static const int WEIGHT[7] = { 1, 2, 4, 5, 8, 8, 8 };
static const u16 *SYM[7] = { slot_logo, slot_mega, slot_coin, slot_chair, slot_red, slot_green, slot_blue };
typedef struct { signed char s[3]; int win; const char *label; } Pay;   // -1 = any symbol
static const Pay PAYS[] = {
    { { Y_LOGO, Y_LOGO, Y_LOGO }, 200, "JACKPOT" }, { { Y_MEGA, Y_MEGA, Y_MEGA }, 50, "MEGA QU33PH" },
    { { Y_COIN, Y_COIN, Y_COIN }, 30, "TRIPLE COIN" }, { { Y_CHAIR, Y_CHAIR, Y_CHAIR }, 20, "CHAIR SPIN" },
    { { Y_LOGO, Y_LOGO, -1 }, 15, "DOUBLE LOGO" }, { { Y_MEGA, Y_MEGA, -1 }, 8, "DOUBLE MEGA" },
    { { Y_CHAIR, Y_CHAIR, -1 }, 6, "DOUBLE CHAIR" }, { { Y_COIN, Y_COIN, -1 }, 5, "DOUBLE COIN" },
    { { Y_RED, Y_RED, Y_RED }, 4, "RED TRIPLE" }, { { Y_GREEN, Y_GREEN, Y_GREEN }, 4, "GREEN TRIPLE" },
    { { Y_BLUE, Y_BLUE, Y_BLUE }, 4, "BLUE TRIPLE" }, { { -1, Y_LOGO, -1 }, 3, "ANY LOGO" },
    { { Y_RED, Y_RED, -1 }, 2, "DOUBLE RED" }, { { Y_GREEN, Y_GREEN, -1 }, 2, "DOUBLE GREEN" },
    { { Y_BLUE, Y_BLUE, -1 }, 2, "DOUBLE BLUE" }, { { -1, Y_COIN, -1 }, 2, "ANY COIN" }, { { -1, Y_CHAIR, -1 }, 1, "ANY CHAIR" } };
static int reel[3] = { Y_LOGO, Y_MEGA, Y_COIN }, result[3], spinT, spinBet, lastWin; static const char *lastLabel = "";
static int slotSel;
static int pickSym(void) { int t = 0; for (int i = 0; i < 7; i++) t += WEIGHT[i]; int r = rand() % t; for (int i = 0; i < 7; i++) { r -= WEIGHT[i]; if (r < 0) return i; } return Y_BLUE; }
static int checkWin(const int *r, const char **label) {
    int isM[3]; for (int i = 0; i < 3; i++) isM[i] = r[i] >= Y_RED;
    if (isM[0] && isM[1] && isM[2] && r[0] != r[1] && r[1] != r[2] && r[0] != r[2]) {
        if (r[2] == Y_BLUE) { *label = "TRIO - BLUE 3RD"; return 10; }
        *label = "TRIO - ANY ORDER"; return 6;
    }
    for (unsigned k = 0; k < sizeof PAYS / sizeof PAYS[0]; k++) {
        int ok = 1; for (int i = 0; i < 3; i++) if (PAYS[k].s[i] >= 0 && PAYS[k].s[i] != r[i]) ok = 0;
        if (ok) { *label = PAYS[k].label; return PAYS[k].win; }
    }
    *label = ""; return 0;
}
static void spin(int bet) {
    if (spinT > 0 || sv.coins < bet) { if (sv.coins < bet) lastLabel = "not enough coins"; return; }
    sv.coins -= bet; spinBet = bet; spinT = 13; lastWin = 0; lastLabel = "";
    for (int i = 0; i < 3; i++) result[i] = pickSym();
    sv.slotSpins++; unlockAch(A_SLOT_SPIN);
}
void updateSlot(void) {
    if (spinT <= 0) return;
    spinT--;
    for (int i = 0; i < 3; i++) { int stopAt = 9 - i * 4; if (spinT > stopAt) reel[i] = rand() % 7; else reel[i] = result[i]; }
    if (spinT == 0) {
        int w = checkWin(result, &lastLabel);
        if (w) {
            lastWin = w * spinBet; addCoins(lastWin); sv.slotWins++; unlockAch(A_SLOT_WIN);
            if (result[0] == Y_LOGO && result[1] == Y_LOGO && result[2] == Y_LOGO) unlockAch(A_SLOT_JACKPOT);
            sfxPlop();
        }
        saveWrite();
    }
}
static Btn slotB[3];
void drawSlot(void) {
    int gold = shopActive(SH_GOLDSLOT);
    fillScreen(bufTop, DARK);
    textC(bufTop, 4, gold ? "GOLDEN SLOT MACHINE" : "QU33PH SLOTS", GOLD, 1);
    const char *rows[8] = { "LOGO LOGO LOGO   200", "MEGA x3   50   COIN x3   30", "CHAIR x3  20   2 LOGOS   15",
                            "RED GREEN BLUE (blue 3rd)  10", "any trio 6   2 MEGA 8   2 CHAIR 6", "2 COIN 5   colour x3  4",
                            "logo middle 3   pair 2", "coin middle 2   chair middle 1" };
    for (int i = 0; i < 8; i++) textC(bufTop, 26 + i * 17, rows[i], i == 0 ? YELLOW : WHITE, 1);
    coinCount(bufTop, 6, 170);
    fillScreen(bufBot, COL(4, 2, 2));
    box(bufBot, 34, 14, 188, 72, BLACK, GOLD);
    for (int i = 0; i < 3; i++) blit(bufBot, SYM[reel[i]], 52, 52, 44 + i * 58, 24);
    char s[32];
    if (lastWin) { sprintf(s, "%s  +%d", lastLabel, lastWin); textC(bufBot, 94, s, LIME, 1); }
    else if (lastLabel[0]) textC(bufBot, 94, lastLabel, GREY, 1);
    int b1 = gold ? 5 : 1, b2 = gold ? 10 : 3;
    slotB[0] = (Btn){ 20, 116, 104, 36, "", 0, sv.coins < b1 }; sprintf(slotB[0].label, "SPIN  %d", b1);
    slotB[1] = (Btn){ 132, 116, 104, 36, "", 0, sv.coins < b2 }; sprintf(slotB[1].label, "%dx SPIN  %d", b2, b2);
    slotB[2] = (Btn){ 78, 158, 100, 28, "BACK", 0, 0 };
    drawBtns(bufBot, slotB, 3, slotSel);
}
void inputSlot(int down, int tx, int ty) {
    (void)tx; (void)ty;
    int gold = shopActive(SH_GOLDSLOT);
    if (down & KEY_B) { goScreen(S_TITLE); return; }
    int h = btnInput(slotB, 3, &slotSel, 2);
    if (h == 0) spin(gold ? 5 : 1);
    if (h == 1) spin(gold ? 10 : 3);
    if (h == 2) goScreen(S_TITLE);
}

// ══ PLINQU33PH ════════════════════════════════════════════════════════════
static const int BINS[7] = { 1, 2, 5, 10, 5, 2, 1 };
#define PEG_ROWS 7
typedef struct { float x, y, vx, vy, rot, spin; int live, done; } Ball;
static Ball balls[3]; static int dropIdx, paid, aimX = 128, plWin, plSel, plTouch;
static int pegX(int r, int c) { return 18 + c * 32 + (r % 2 ? 16 : 0); }
static int pegY(int r) { return 34 + r * 18; }
static int pegCount(int r) { return r % 2 ? 7 : 8; }
void plinkoEnter(void) { dropIdx = 0; paid = 0; plWin = 0; plTouch = 0; memset(balls, 0, sizeof balls); }
static void drop(void) {
    if (dropIdx >= 3) {                                                // set done: start a new one, once all have landed
        for (int i = 0; i < 3; i++) if (balls[i].live) return;
        plinkoEnter(); return;
    }
    if (!paid) { if (sv.coins < 5) return; sv.coins -= 5; paid = 1; }
    Ball *b = &balls[dropIdx++];
    b->x = aimX; b->y = 10; b->vx = (frand() - 0.5f) * 0.6f; b->vy = 0; b->live = 1; b->done = 0;
    b->rot = 1.5708f; b->spin = (frand() - 0.5f) * 0.1f;
}
void updatePlinko(void) {
    for (int i = 0; i < 3; i++) {
        Ball *b = &balls[i]; if (!b->live) continue;
        b->vy += 0.16f; b->x += b->vx; b->y += b->vy; b->vx *= 0.995f;
        b->rot += b->spin; b->spin *= 0.985f;
        // only the peg rows the marker can touch (it's within 9 px of a peg's row), and only the
        // two pegs either side of it in that row: 4 checks instead of 52
        int rn = (int)((b->y - 34 + 9) / 18);
        for (int r = rn - 1; r <= rn; r++) { if (r < 0 || r >= PEG_ROWS) continue;
            int pdy = (int)b->y - pegY(r); if (pdy > 9 || pdy < -9) continue;
            int c0 = (int)((b->x - 18 - (r % 2 ? 16 : 0)) / 32);
            for (int c = c0; c <= c0 + 1; c++) { if (c < 0 || c >= pegCount(r)) continue;
            float dx = b->x - pegX(r, c), dy = b->y - pegY(r), d2 = dx * dx + dy * dy;
            if (d2 < 81.0f && d2 > 0.01f) {                               // ball r 6 + peg r 3
                float d = fsqrt(d2), nx = dx / d, ny = dy / d, rel = b->vx * nx + b->vy * ny;
                b->x = pegX(r, c) + nx * 9; b->y = pegY(r) + ny * 9;
                if (rel < 0) { b->vx -= 1.5f * rel * nx; b->vy -= 1.5f * rel * ny; }
                b->vx += (frand() - 0.5f) * 0.5f;
                // the marker tumbles: a glancing hit spins it the way it was knocked
                b->spin += (b->vx * ny - b->vy * nx) * 0.06f;
                if (b->spin > 0.4f) b->spin = 0.4f;
                if (b->spin < -0.4f) b->spin = -0.4f;
            }
        } }
        if (b->x < 6) { b->x = 6; b->vx = -b->vx * 0.5f; }
        if (b->x > 250) { b->x = 250; b->vx = -b->vx * 0.5f; }
        if (b->y > 172) {
            int bin = (int)(b->x * 7 / 256); if (bin < 0) bin = 0; if (bin > 6) bin = 6;
            b->live = 0; b->done = 1; b->y = 178;
            addCoins(BINS[bin]); plWin += BINS[bin]; sfxPlop();
            if (i == 2) saveWrite();       // once per set (writing to the SD card takes a moment), not per marker
        }
    }
}
void drawPlinko(void) {
    fillScreen(bufTop, DARK);
    textC(bufTop, 10, "PLINQU33PH", GOLD, 2);
    textC(bufTop, 50, "5 coins for 3 markers", WHITE, 1);
    textC(bufTop, 68, "aim: D-pad or drag", GREY, 1);
    textC(bufTop, 84, "drop: A, or lift your finger", GREY, 1);
    char s[32]; sprintf(s, "won this set: %d", plWin); textC(bufTop, 100, s, LIME, 1);
    textC(bufTop, 150, "B  back", GREY, 1);
    coinCount(bufTop, 6, 170);
    fillScreen(bufBot, COL(2, 3, 6));
    for (int r = 0; r < PEG_ROWS; r++) for (int c = 0; c < pegCount(r); c++) rect(bufBot, pegX(r, c) - 2, pegY(r) - 2, 5, 5, WHITE);
    for (int i = 0; i < 7; i++) {
        int x0 = i * 256 / 7, x1 = (i + 1) * 256 / 7;
        rect(bufBot, x0, 170, 1, 22, GREY);
        sprintf(s, "%d", BINS[i]); text(bufBot, (x0 + x1) / 2 - textW(s, 1) / 2, 175, s, BINS[i] == 10 ? GOLD : WHITE, 1);
    }
    const u16 bc[3] = { COL(31, 6, 6), COL(6, 28, 8), COL(8, 12, 31) };
    (void)bc;
    if (dropIdx >= 3) { int live = 0; for (int i = 0; i < 3; i++) live |= balls[i].live; if (!live) textC(bufTop, 118, "A or tap for another set", YELLOW, 1); }
    // the real markers, drawn on the bottom screen (gy 192+ = bottom), spinning as they fall
    const u16 *PS[3] = { pm_red, pm_green, pm_blue }; const int PW[3] = { PM_RED_W, PM_GREEN_W, PM_BLUE_W }, PH[3] = { PM_RED_H, PM_GREEN_H, PM_BLUE_H };
    gClipLo = SH; gClipHi = 2 * SH;
    for (int i = 0; i < 3; i++) if (balls[i].live || balls[i].done) blitRot(PS[i], PW[i], PH[i], (int)balls[i].x, SH + (int)balls[i].y, balls[i].rot);
    if (dropIdx < 3) blitRot(PS[dropIdx], PW[dropIdx], PH[dropIdx], aimX, SH + 12, 1.5708f);
    gClipLo = 0; gClipHi = 2 * SH;
}
void inputPlinko(int down, int held, int tx, int ty) {
    if (down & KEY_B) { goScreen(S_TITLE); return; }
    if (held & KEY_LEFT) aimX -= 3;
    if (held & KEY_RIGHT) aimX += 3;
    if ((held & KEY_TOUCH) && ty < 150) aimX = tx;
    if (aimX < 10) aimX = 10;
    if (aimX > 246) aimX = 246;
    if (down & KEY_A) drop();
    // a drop on lift only counts if the touch began here (not the tap that opened this screen)
    if (down & KEY_TOUCH) plTouch = 1;
    if ((kUp & KEY_TOUCH) && plTouch) { plTouch = 0; drop(); }
}
