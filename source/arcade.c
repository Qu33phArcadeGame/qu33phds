// arcade.c — the arcade menu, and MINI QU33PH (the website's mini.html). Qu33ph-Ball is in ball.c.
//
// MINI QU33PH on the DS: the website's far-end table photo spans both screens, the same way
// the main game's field does — the far table and the brick wall up top, the near table you
// throw from on the bottom (touch) screen. All positions use the website's own lane maths
// (u across the table 0..1, v up the lane 0..1) and its photo calibration, so the physics,
// scoring and timings are the website's, step for step at 60 fps.
//
// CONTROLS: flick up on the bottom screen (the marker follows your finger, then throws), or
// D-pad left/right to move along the edge, L/R to angle, hold A for power and release.
// START pauses (SELECT then quits to the arcade). X music, Y sound effects.
#include "qu.h"
#include "assets_mini.h"
#include "assets_ball.h"

// ══ ARCADE MENU ═══════════════════════════════════════════════════════════
static const char *ARC_NAME[ARC_COUNT] = { "MINI QU33PH", "QU33PH-BALL", "FIDGET", "BOWLING", "STACK", "FLIP", "DOZER", "JUMP", "PINBALL" };
static const int ARC_READY[ARC_COUNT] = { 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const char *ARC_BLURB[ARC_COUNT] = { "four mini markers, three tables", "three machines, nine markers" };
static int arcErrT;                    // frames left to show "couldn't load" on the top screen
static int arcSel;
static Btn AB[ARC_COUNT + 1];
static void arcLayout(void) {
    int w = (SW - 12) / 2;
    for (int i = 0; i <= ARC_COUNT; i++) {
        AB[i].x = 4 + (i % 2) * (w + 4); AB[i].y = 4 + (i / 2) * 37; AB[i].w = w; AB[i].h = 33;
        AB[i].col = 0; AB[i].dim = i < ARC_COUNT && !ARC_READY[i];
        strcpy(AB[i].label, i < ARC_COUNT ? ARC_NAME[i] : "BACK");
    }
}
void drawArcade(void) {
    arcLayout();
    fillScreen(bufTop, DARK);
    blit(bufTop, logo, LOGO_W, LOGO_H, (SW - LOGO_W) / 2, 0);
    coinCount(bufTop, 8, 8);
    textC(bufTop, 118, "ARCADE", GOLD, 2);
    if (arcSel < ARC_COUNT) {
        textC(bufTop, 150, ARC_NAME[arcSel], WHITE, 1);
        char s[40];
        if (!ARC_READY[arcSel]) strcpy(s, "coming soon to the DS");
        else if (sv.arcadePlays[arcSel]) sprintf(s, "best %d   played %d", sv.arcadeBest[arcSel], sv.arcadePlays[arcSel]);
        else strcpy(s, ARC_BLURB[arcSel]);
        textC(bufTop, 170, s, ARC_READY[arcSel] ? YELLOW : GREY, 1);
    }
    if (arcErrT > 0) {                  // a game's pack couldn't be loaded
        arcErrT--;
        box(bufTop, 8, 138, 240, 50, COL(6, 0, 0), RED);
        textC(bufTop, 144, pakErr == 1 ? "CAN'T READ THE GAME'S FILES" : pakErr == 2 ? "GAME FILE MISSING" : "GAME FILE IS OUT OF DATE", WHITE, 1);
        textC(bufTop, 162, pakErr == 1 ? "start Qu33ph from TWiLight Menu++" : "rebuild with the new .pak files", YELLOW, 1);
    }
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, AB, ARC_COUNT + 1, arcSel);
}
void inputArcade(void) {
    arcLayout();
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    int h = btnInput(AB, ARC_COUNT + 1, &arcSel, 2);
    if (h == ARC_COUNT) { goScreen(S_TITLE); return; }
    if (h == ARC_MINI) { if (pakUse(MINI_PAK, MINI_PAK_SIZE, MINI_PAK_ID)) { miniThemeChanged(); goScreen(S_MINI_MENU); } else arcErrT = 240; }
    if (h == ARC_BALL) { if (ballEnter()) goScreen(S_BALL_MENU); else arcErrT = 240; }
}

// ══ MINI QU33PH ═══════════════════════════════════════════════════════════
// the lane, measured from the website's far-end photo (VIEW.far in mini.html)
#define INV_A   2.311f
#define INV_B   2.694f
#define HW0     0.2094f
#define FY0     0.167f
#define SLOPE   0.3414f
#define VCX     0.4968f
#define LANE_AR 4.0f
#define V_DESTINY 0.5f                 // the seam between the first two tables
#define V_WALL  1.0f
#define WALL_BOUNCE 0.42f
#define MK_LEN  0.115f
#define RAD     (MK_LEN * 0.42f)
#define TOUCH_AR 3.2f
#define TOUCH_R (RAD * 2.6f)
#define BOXV 0.74f
#define BOXU 0.5f
#define BOXW 0.15f
#define BOXH 0.045f
#define DT (1.0f / 60.0f)
#define DRAG 0.964801f                 // exp(-2.15 / 60)
#define SPIN_DECAY 0.957592f           // exp(-2.6 / 60)
enum { MC_GREEN, MC_PINK, MC_YELLOW, MC_BLUE };
static const u16 *MSPR(int c) { return c == 0 ? mm_green : c == 1 ? mm_pink : c == 2 ? mm_yellow : mm_blue; }   // in mini.pak

typedef struct { int col; float u, v, du, dv, rot, spin, sndCd; int off, cap, hitByBlue; } MM;
static MM mk[4];
static int nmk, order[4], idx, live = -1, scored;
static int matchSecs = 60, sudden, score, turnPts, turn, coinsWon, newBest;
static float tLeft, suddenAt, turnPause, aimU = 0.5f, aimA, shakeT, shakeAmt;
static int shX, shY;
static u16 palT[256];                  // the table's palette through the current theme

void miniThemeChanged(void) { for (int i = 0; i < 256; i++) palT[i] = themeTint(mini_pal[i], sv.theme); }
// the theme changed: recolour whichever arcade game is loaded (the others recolour when opened)
void arcadeThemeChanged(void) { if (pakIs(MINI_PAK)) miniThemeChanged(); if (pakIs(BALL_PAK)) ballThemeChanged(); }

// ── popups & the end-of-turn breakdown ────────────────────────────────────
typedef struct { char t[32]; u16 c; int life, big, peef; } Pop;
static Pop pops[8];
static void popsClear(void) { memset(pops, 0, sizeof pops); }
static void popAdd(const char *t, u16 c, int big, int peef) {     // newest at the end, like the website's list
    int n = 0; while (n < 8 && pops[n].life > 0) n++;
    if (n == 8) { for (int i = 0; i < 7; i++) pops[i] = pops[i + 1]; n = 7; }
    Pop *p = &pops[n]; strncpy(p->t, t, 31); p->t[31] = 0; p->c = c; p->big = big; p->peef = peef; p->life = big ? 96 : 70;
}
static char msgL[6][32]; static int msgN, msgT;
static void addShake(float a) { if (a > shakeAmt) shakeAmt = a; shakeT = 0.42f; }

// ── sound ─────────────────────────────────────────────────────────────────
static int vol127(float v) { if (v < 0) v = 0; if (v > 1) v = 1; return (int)(v * 120); }
static void sCap(float v) { playAdpcm(ms_cap, MS_CAP_LEN, 16000, vol127(v)); }
static void playImpact(float hard) {
    float v = hard / 3; if (v < 0.35f) v = 0.35f; if (v > 1) v = 1;
    int r = rand() & 1;
    if (hard < 1.0f) { if (r) playAdpcm(ms_short1, MS_SHORT1_LEN, 16000, vol127(v)); else playAdpcm(ms_short2, MS_SHORT2_LEN, 16000, vol127(v)); }
    else if (hard < 2.0f) { if (r) playAdpcm(ms_hit1, MS_HIT1_LEN, 16000, vol127(v)); else playAdpcm(ms_hit2, MS_HIT2_LEN, 16000, vol127(v)); }
    else { if (r) playAdpcm(ms_long2, MS_LONG2_LEN, 16000, vol127(v)); else playAdpcm(ms_long3, MS_LONG3_LEN, 16000, vol127(v)); }
}
static void playThrow(float p) {
    int v = vol127(0.6f + p * 0.4f);
    if (p < 0.3f) playAdpcm(ms_throw1, MS_THROW1_LEN, 16000, v);
    else if (p < 0.6f) playAdpcm(ms_throw2, MS_THROW2_LEN, 16000, v);
    else if (p < 0.85f) playAdpcm(ms_throw3, MS_THROW3_LEN, 16000, v);
    else playAdpcm(ms_throw4, MS_THROW4_LEN, 16000, v);
}
static void playPeef(float v) {
    if (v < 0.33f) playAdpcm(ms_peef1, MS_PEEF1_LEN, 16000, 120);
    else if (v < 0.66f) playAdpcm(ms_peef2, MS_PEEF2_LEN, 16000, 120);
    else playAdpcm(ms_peef3, MS_PEEF3_LEN, 16000, 120);
}

// ── the lane → the screens ────────────────────────────────────────────────
static float hwOf(float v) { return 1.0f / (INV_A + INV_B * v); }
static void proj(float u, float v, int *x, int *gy, float *s) {
    float hw = hwOf(v), fy = FY0 + (hw - HW0) / SLOPE;
    *x = (int)((VCX + (u - 0.5f) * 2 * hw) * 256) + shX;
    *gy = (int)(fy * MINI_PH) - MINI_OFF + shY;
    if (s) *s = hw * 256;
}
static float uAtEdge(int tx) {                                  // touch x → u along the near edge
    float hw = hwOf(0.02f), u = ((tx / 256.0f) - VCX) / (2 * hw) + 0.5f;
    return u < 0.06f ? 0.06f : u > 0.94f ? 0.94f : u;
}

static int dragging, dsx, dsy, dnx, dny, charging, chargeT, ph;
static float power;
static void dragReset(void) { dragging = 0; charging = 0; chargeT = 0; }

// ── a match ───────────────────────────────────────────────────────────────
static void newTurn(void) {
    memset(mk, 0, sizeof mk); nmk = 0; live = -1; idx = 0; turnPts = 0; scored = 0;
    int three[3] = { MC_GREEN, MC_PINK, MC_YELLOW };
    for (int i = 2; i > 0; i--) { int j = rand() % (i + 1), t = three[i]; three[i] = three[j]; three[j] = t; }
    order[0] = three[0]; order[1] = three[1]; order[2] = three[2]; order[3] = MC_BLUE;   // blue always goes last
}
static void startMini(int secs) {
    matchSecs = secs; suddenAt = secs * 0.2f; tLeft = secs;
    sudden = 0; score = 0; turn = 1; coinsWon = 0; newBest = 0; turnPause = 0;
    popsClear(); msgN = 0; msgT = 0; shakeT = 0; shakeAmt = 0;
    newTurn(); dragReset();
    screen = S_MINI;
}
static int nextColor(void) { return idx < 4 ? order[idx] : -1; }
static void launch(float pw, float aim) {
    int col = nextColor(); if (col < 0 || live >= 0) return;
    float p = pw < 0.06f ? 0.06f : pw > 1 ? 1 : pw, a = aim < -1 ? -1 : aim > 1 ? 1 : aim;
    MM *m = &mk[nmk];
    memset(m, 0, sizeof *m);
    m->col = col; m->u = aimU; m->v = 0.02f;
    m->du = a * 0.52f * (0.5f + p) + (frand() - 0.5f) * 0.05f;
    m->dv = 0.55f + p * 1.52f;
    m->rot = (frand() - 0.5f) * 0.5f; m->spin = (frand() - 0.5f) * 7 * (0.4f + p);
    m->cap = 1;
    live = nmk; nmk++; idx++;
    playThrow(p);
}
static void fellOff(MM *m, int byBlue) {
    m->off = 1;
    int d;
    if (sudden) d = (m->v > V_DESTINY) ? 10 : 6;              // SD: off past the line, or short of the chair
    else if (m->col == MC_BLUE) d = (m->v < V_DESTINY) ? 10 : 0;
    else d = (m->v < V_DESTINY) ? 50 : 0;                   // any other marker off before it clears the first table
    if (byBlue) d += 6;                                     // blue knocking another marker off
    playPeef(m->v);
    popAdd("PEEF", COL(31, 4, 4), 1, 1);
    if (d > 0) { char t[32]; turnPts -= d; sprintf(t, "-%d%s", d, byBlue ? " KNOCKED OFF" : ""); popAdd(t, COL(31, 4, 4), 0, 1); }
    addShake(12);
}
static void popCap(MM *m) {
    if (!m->cap) return;
    m->cap = 0;
    int d = m->col == MC_BLUE ? 6 : 3; char t[24];
    turnPts -= d; sprintf(t, "-%d CAP OFF", d); popAdd(t, COL(31, 20, 9), 0, 0);
    sCap(0.55f);
}
static float lenUV(float du, float dv) { return fsqrt(du * du + dv * dv); }
static int touchingM(MM *a, MM *b) { float dx = a->u - b->u, dy = (a->v - b->v) * TOUCH_AR; return lenUV(dx, dy) < TOUCH_R; }
static int scoreTable(void) {
    MM *on[4]; int n = 0;
    for (int i = 0; i < nmk; i++) if (!mk[i].off) on[n++] = &mk[i];
    int par[4]; for (int i = 0; i < n; i++) par[i] = i;
    int pairs = 0, bluePairs = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {
        if (!touchingM(on[i], on[j])) continue;
        pairs++;
        if (on[i]->col == MC_BLUE || on[j]->col == MC_BLUE) bluePairs++;
        int a = i, b = j; while (par[a] != a) a = par[a]; while (par[b] != b) b = par[b];
        if (a != b) par[a] = b;
    }
    int sizes[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < n; i++) { int r = i; while (par[r] != r) r = par[r]; sizes[r]++; }
    int pts = 0; msgN = 0;
    if (sudden) {
        if (pairs) { pts += pairs * 6; sprintf(msgL[msgN++], "TOUCHING x%d  +%d", pairs, pairs * 6); }
        if (bluePairs) { pts += bluePairs * 10; sprintf(msgL[msgN++], "BLUE TOUCHING  +%d", bluePairs * 10); }
        for (int i = 0; i < 4; i++) { if (sizes[i] == 3) { pts += 15; strcpy(msgL[msgN++], "THREE TOUCHING  +15"); }
                                      if (sizes[i] == 4) { pts += 20; strcpy(msgL[msgN++], "ALL FOUR  +20"); } }
    } else {
        if (pairs) { pts += pairs; sprintf(msgL[msgN++], "TOUCHING x%d  +%d", pairs, pairs); }
        if (bluePairs) { pts += bluePairs * 3; sprintf(msgL[msgN++], "BLUE TOUCHING  +%d", bluePairs * 3); }
        for (int i = 0; i < 4; i++) { if (sizes[i] == 3) { pts += 5; strcpy(msgL[msgN++], "THREE TOUCHING  +5"); }
                                      if (sizes[i] == 4) { pts += 10; strcpy(msgL[msgN++], "QU33PH!  +10"); } }
    }
    int mega = 0;                                            // MEGA QU33PH: any marker resting on the box
    for (int i = 0; i < n; i++) if (fabsf_(on[i]->u - BOXU) < BOXW / 2 && fabsf_(on[i]->v - BOXV) < BOXH / 2) mega++;
    if (mega && msgN < 6) { pts += mega * 30; sprintf(msgL[msgN++], "MEGA QU33PH x%d  +%d", mega, mega * 30); }
    return pts;
}
static void endTurn(void) {
    turnPts += scoreTable();
    int extra = 0;
    if (turnPts >= 33) { turnPts += 10; extra = 1; }          // 33 in a turn buys another turn and 10 more
    score += turnPts;
    if (!msgN) { strcpy(msgL[0], turnPts < 0 ? "ROUGH TURN" : "NOTHING TOUCHING"); msgN = 1; }
    msgT = 150;
    char t[32]; sprintf(t, "%s%d TURN %d", turnPts >= 0 ? "+" : "", turnPts, turn);
    popAdd(t, turnPts >= 0 ? COL(31, 27, 0) : COL(31, 13, 13), 1, 0);
    if (extra) popAdd("33+  EXTRA TURN  +10", COL(17, 31, 29), 1, 0);
    else turn++;
    turnPause = 0.45f;
}
static void gameOver(void) {
    live = -1; shakeT = 0; dragReset();
    int slot = ARC_MINI;
    sv.arcadePlays[slot]++;
    newBest = score > sv.arcadeBest[slot];                   // as on the website, the best starts at 0
    if (newBest) sv.arcadeBest[slot] = score;
    coinsWon = score > 0 ? score / 10 : 0;
    int before = sv.coins;
    if (coinsWon > 0) addCoins(coinsWon);
    coinsWon = sv.coins - before;                            // shows the Coin Doubler if it's on
    saveWrite();
    screen = S_MINI_OVER;
}

// ── physics: the website's step(), at a fixed 1/60 s ──────────────────────
static int chairCount(void) { return sudden ? 2 : 0; }
static const float CH_U[2] = { 0.5f, 0.5f }, CH_V[2] = { 0.62f, 0.86f }, CH_W = 0.17f, CH_H = 0.045f;
void updateMini(void) {
    tLeft -= DT;
    if (!sudden && tLeft <= suddenAt) { sudden = 1; popAdd("SUDDEN DEATH", COL(31, 11, 11), 1, 0); }
    if (tLeft <= 0) { tLeft = 0; gameOver(); return; }
    int moving = 0;
    for (int i = 0; i < nmk; i++) {
        MM *m = &mk[i];
        if (m->off) continue;
        if (m->sndCd > 0) m->sndCd -= DT;
        m->u += m->du * DT; m->v += m->dv * DT;
        m->rot += m->spin * DT; m->spin *= SPIN_DECAY;
        m->du *= DRAG; m->dv *= DRAG;
        if (fabsf_(m->du) + fabsf_(m->dv) < 0.045f) { m->du = 0; m->dv = 0; m->spin *= 0.7f; } else moving = 1;
        for (int c = 0; c < chairCount(); c++) {                 // chairs are solid in sudden death
            if (fabsf_(m->u - CH_U[c]) < CH_W / 2 + RAD && fabsf_(m->v - CH_V[c]) < CH_H / 2 + RAD) {
                float push = (m->u < CH_U[c]) ? -1 : 1, preDv = m->dv;
                m->u = CH_U[c] + push * (CH_W / 2 + RAD); m->du = fabsf_(m->du) * push * 0.55f; m->spin += push * 4;
                if (!(m->sndCd > 0)) { playImpact(fabsf_(preDv) * 1.4f); m->sndCd = 0.09f; }
                if (fabsf_(m->dv) > 1.1f) popCap(m);
            }
        }
        if (m->v > V_WALL - RAD) {                              // the far end is a wall, not a drop
            m->v = V_WALL - RAD;
            if (m->dv > 0) {
                float preDv = m->dv;
                m->dv = -m->dv * WALL_BOUNCE;
                m->spin += (frand() - 0.5f) * 3 - m->du * 1.5f;
                if (!(m->sndCd > 0)) { float v = preDv / 2.2f; playAdpcm(ms_wall, MS_WALL_LEN, 16000, vol127(v < 0.4f ? 0.4f : v)); m->sndCd = 0.09f; }
                if (fabsf_(m->dv) > 1.35f) popCap(m);
                addShake(4);
            }
        }
        if (m->u < -0.02f || m->u > 1.02f) { fellOff(m, m->hitByBlue); if (i == live) live = -1; }   // PEEF: only ever the sides
    }
    for (int i = 0; i < nmk; i++) for (int j = i + 1; j < nmk; j++) {     // marker on marker
        MM *a = &mk[i], *b = &mk[j]; if (a->off || b->off) continue;
        float dx = a->u - b->u, dy = (a->v - b->v) * LANE_AR, d = lenUV(dx, dy);
        if (d >= RAD * 2 || d < 1e-6f) continue;
        float nx = dx / d, ny = dy / d, ov = (RAD * 2 - d) / 2;
        a->u += nx * ov; a->v += ny * ov / LANE_AR; b->u -= nx * ov; b->v -= ny * ov / LANE_AR;
        float rvx = a->du - b->du, rvy = (a->dv - b->dv) * LANE_AR, sep = rvx * nx + rvy * ny;
        if (sep < 0) {
            float imp = sep * 0.86f;
            a->du -= imp * nx; a->dv -= imp * ny / LANE_AR; b->du += imp * nx; b->dv += imp * ny / LANE_AR;
            a->spin += (frand() - 0.5f) * 7; b->spin += (frand() - 0.5f) * 7;
            if (!(a->sndCd > 0) && !(b->sndCd > 0)) { playImpact(fabsf_(sep)); a->sndCd = b->sndCd = 0.09f; }
            if (fabsf_(sep) > 1.5f) { if (frand() < 0.30f) popCap(a); if (frand() < 0.30f) popCap(b); }
            if (a->col == MC_BLUE) b->hitByBlue = 1;
            if (b->col == MC_BLUE) a->hitByBlue = 1;
            moving = 1;
        }
    }
    if (live >= 0 && !moving) live = -1;
    if (turnPause > 0) { turnPause -= DT; if (turnPause <= 0) newTurn(); }
    else if (live < 0 && idx >= 4 && !moving && nmk > 0 && !scored) { scored = 1; endTurn(); }
    int k = 0;                                               // age the popups, dropping finished ones in order
    for (int i = 0; i < 8; i++) if (pops[i].life > 0 && --pops[i].life > 0) pops[k++] = pops[i];
    for (; k < 8; k++) pops[k].life = 0;
    if (msgT > 0) msgT--;
    if (shakeT > 0) { shakeT -= DT; if (shakeT <= 0) { shakeT = 0; shakeAmt = 0; } }
}

// ── input ─────────────────────────────────────────────────────────────────
static int canThrow(void) { return screen == S_MINI && live < 0 && nextColor() >= 0 && turnPause <= 0; }
void inputMini(void) {
    if (screen == S_MINI_PAUSE) {
        if (kDown & KEY_START) screen = S_MINI;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_MINI_PAUSE; dragging = 0; charging = 0; return; }
    if (kDown & KEY_X) { sv.musicOn = !sv.musicOn; if (sv.musicOn) musicStart(); else musicStop(); }
    if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    // flick: the marker follows your finger along the near edge, then goes where you flick it
    if ((kDown & KEY_TOUCH) && canThrow()) { dragging = 1; dsx = dnx = tX; dsy = dny = tY; aimU = uAtEdge(tX); charging = 0; }
    if (dragging && (kHeld & KEY_TOUCH)) { dnx = tX; dny = tY; aimU = uAtEdge(tX); }
    if (dragging && (kUp & KEY_TOUCH)) {
        dragging = 0;
        float dx = (dnx - dsx) / 256.0f, dy = (dny - dsy) / 192.0f, up = dy < 0 ? -dy : 0;
        if (up >= 0.04f && canThrow()) launch(up / 0.5f > 1 ? 1 : up / 0.5f, dx / 0.28f);
    }
    // buttons: walk the throw spot, angle it, hold A for power
    if (!dragging && canThrow()) {
        if (kHeld & KEY_LEFT)  { aimU -= 0.45f * DT; if (aimU < 0.06f) aimU = 0.06f; chargeT = 90; }
        if (kHeld & KEY_RIGHT) { aimU += 0.45f * DT; if (aimU > 0.94f) aimU = 0.94f; chargeT = 90; }
        if (kHeld & KEY_L) { aimA -= 1.2f * DT; if (aimA < -1) aimA = -1; chargeT = 90; }
        if (kHeld & KEY_R) { aimA += 1.2f * DT; if (aimA > 1) aimA = 1; chargeT = 90; }
        if (kDown & KEY_A) { charging = 1; ph = 0; power = 0; }
    }
    if (charging) {
        ph++; float p = (ph % 64) / 32.0f; power = p < 1 ? p : 2 - p;
        chargeT = 90;
        if (kDown & KEY_B) charging = 0;
        else if (kUp & KEY_A) { charging = 0; if (canThrow()) launch(power, aimA); }
    }
    if (chargeT > 0 && !charging) chargeT--;
}

// ── drawing ───────────────────────────────────────────────────────────────
static void drawTable(void) {
    const u8 *t = mini_table;
    if (shX == 0 && shY == 0) { drawIndexed(t, palT); return; }
    for (int y = 0; y < 2 * SH; y++) {                       // shaking: the whole picture jolts
        int sy = y - shY; if (sy < 0) sy = 0; if (sy >= 2 * SH) sy = 2 * SH - 1;
        u16 *row = y < SH ? &bufTop[y * SW] : &bufBot[(y - SH) * SW];
        const u8 *src = &mini_table[sy * SW];
        for (int x = 0; x < SW; x++) { int sx = x - shX; if (sx < 0) sx = 0; if (sx >= SW) sx = SW - 1; row[x] = palT[src[sx]]; }
    }
}
static void drawMarkerAt(float u, float v, float rot, int col) {
    int x, gy; float s; proj(u, v, &x, &gy, &s);
    float len = MK_LEN * s * 2 * 1.35f;
    int rx = (int)(len * 0.36f), ry = (int)(len * 0.13f) + 1, oy = (int)(len * 0.10f);
    for (int j = -ry; j <= ry; j++) for (int i = -rx; i <= rx; i++)
        if (i * i * ry * ry + j * j * rx * rx <= rx * rx * ry * ry) gdark(x + i, gy + oy + j);
    blitRotScale(MSPR(col), MM_GREEN_W, MM_GREEN_H, x, gy, rot, len / MM_GREEN_H);
}
static void drawProps(void) {
    int x, gy; float s;
    proj(BOXU, BOXV, &x, &gy, &s);                           // the mini case: MEGA QU33PH
    float bs = BOXW * s * 2;
    blitRotScale(mm_case, MM_CASE_W, MM_CASE_H, x, gy, 0, bs / MM_CASE_W);
    for (int c = 0; c < chairCount(); c++) {                 // sudden death's two chairs
        proj(CH_U[c], CH_V[c], &x, &gy, &s);
        float cs = CH_W * s * 2;
        blitRotScale(mm_chair, MM_CHAIR_W, MM_CHAIR_H, x, gy - (int)(cs / 2), 0, cs / MM_CHAIR_W);
    }
}
static void drawAimGuide(void) {                             // button play: where this throw would slide
    float p = charging ? (power < 0.06f ? 0.06f : power) : 0.5f;
    float u = aimU, v = 0.02f, du = aimA * 0.52f * (0.5f + p), dv = 0.55f + p * 1.52f;
    int hx, hy, px = 0, py = 0; proj(aimU, 0.02f, &hx, &hy, 0);
    for (int f = 1; f <= 36; f++) {
        u += du * DT; v += dv * DT; du *= DRAG; dv *= DRAG;
        if (f == 10) proj(u, v, &px, &py, 0);
        if (f % 3 || charging) continue;
        int x, gy; proj(u, v, &x, &gy, 0);
        grect(x - 1, gy - 1, 3, 3, WHITE);
    }
    if (charging) {                                          // the power marker, pointing the way it'll go
        float dx = px - hx, dy = py - hy, l = fsqrt(dx * dx + dy * dy); if (l < 1) l = 1;
        powerMarker(hx, hy, dx / l, dy / l, 30 + power * 120, p);
    }
}
void drawMini(void) {
    if (shakeT > 0 && screen == S_MINI) { float k = shakeAmt * (shakeT / 0.42f) * 0.5f; shX = (int)((frand() - 0.5f) * k); shY = (int)((frand() - 0.5f) * k); }
    else shX = shY = 0;
    drawTable();
    gClipLo = 0; gClipHi = 2 * SH;
    drawProps();
    // markers ride on top, furthest first
    int ordr[4], n = 0;
    for (int i = 0; i < nmk; i++) if (!mk[i].off) ordr[n++] = i;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (mk[ordr[j]].v > mk[ordr[i]].v) { int t = ordr[i]; ordr[i] = ordr[j]; ordr[j] = t; }
    for (int i = 0; i < n; i++) drawMarkerAt(mk[ordr[i]].u, mk[ordr[i]].v, mk[ordr[i]].rot, mk[ordr[i]].col);
    // still to throw: stacked off to the left of the near edge
    int ready = canThrow() || (screen == S_MINI_PAUSE && live < 0 && nextColor() >= 0 && turnPause <= 0);
    int first = idx + (ready ? 1 : 0);
    if (first < 4) {
        int bx, by; proj(0.5f, 0.02f, &bx, &by, 0); (void)bx;
        for (int i = first, k = 0; i < 4; i++, k++)
            blitRotScale(MSPR(order[i]), MM_GREEN_W, MM_GREEN_H, 40 + shX, by - 12 - k * 23, 0, 22.0f / MM_GREEN_H);
    }
    if (ready) {                                             // the marker in your hand
        drawMarkerAt(aimU, 0.02f, 0, nextColor());
        if (!dragging && (charging || chargeT > 0)) drawAimGuide();
    }
    // HUD (top screen): clock in the middle, best & turn on the left, score & theme on the right
    char s[40];
    sprintf(s, "%d.%d", (int)tLeft, (int)(tLeft * 10) % 10);
    textC(bufTop, 2, s, sudden ? COL(31, 11, 11) : COL(31, 27, 0), 2);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_MINI]); text(bufTop, 6, 4, s, GREY, 1);
    if (sudden) textC(bufTop, 31, "SUDDEN DEATH", COL(31, 11, 11), 1);
    else { sprintf(s, "TURN %d", turn); text(bufTop, 6, 20, s, COL(17, 26, 31), 1); }
    sprintf(s, "%d PTS", score); text(bufTop, SW - 6 - textW(s, 1), 4, s, score < 0 ? COL(31, 13, 13) : WHITE, 1);
    text(bufTop, SW - 6 - textW(THEME_NAME[sv.theme], 1), 20, THEME_NAME[sv.theme], GOLD, 1);
    // popups, then the turn breakdown, on the bottom screen: the near table is clear once
    // you've thrown, so nothing covers the far end where the markers land
    int py = 6;
    for (int i = 0; i < 8; i++) {
        Pop *p = &pops[i]; if (p->life <= 0) continue;
        int sc = p->big ? 2 : 1; if (textW(p->t, sc) > SW - 8) sc = 1;
        int jx = p->peef ? (int)((frand() - 0.5f) * 9 * p->life / 70) : 0;
        text(bufBot, (SW - textW(p->t, sc)) / 2 + jx, py, p->t, p->c, sc);
        py += sc == 2 ? 30 : 17;
        if (py > 120) break;
    }
    if (msgT > 0) for (int i = 0; i < msgN; i++) { textC(bufBot, py + 2, msgL[i], WHITE, 1); py += 15; }
    // bottom screen: what to do next
    if (ready) {
        int blue = nextColor() == MC_BLUE;
        textC(bufBot, SH - 15, blue ? "BLUE - FLICK IT LAST" : "FLICK UP TO THROW", blue ? COL(13, 22, 31) : COL(21, 21, 21), 1);
    }
    if (screen == S_MINI_PAUSE) {
        textC(bufTop, 84, "PAUSED", YELLOW, 2);
        textC(bufBot, 70, "START  resume", WHITE, 1);
        textC(bufBot, 94, "SELECT  quit to the arcade", WHITE, 1);
    }
    shX = shY = 0;
}

// ── the menu before a match, and the one after ────────────────────────────
static int mmSel, moSel;
static Btn MB[4];
static void mmLayout(void) {
    MB[0] = (Btn){ 38, 6, 180, 32, "60 SECONDS", 0, 0 };
    MB[1] = (Btn){ 38, 44, 180, 32, "90 SECONDS", 0, 0 };
    MB[2] = (Btn){ 38, 82, 180, 32, "120 SECONDS", 0, 0 };
    MB[3] = (Btn){ 68, 120, 120, 28, "BACK", 0, 0 };
}
void drawMiniMenu(void) {
    mmLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 2, "MINI QU33PH", GOLD, 2);
    textC(bufTop, 32, "4 MINI MARKERS, 3 TABLES", GREY, 1);
    textC(bufTop, 47, "BLUE ALWAYS GOES LAST", COL(13, 22, 31), 1);
    textC(bufTop, 68, "Flick up to throw.", WHITE, 1);
    textC(bufTop, 83, "All four touching: QU33PH", WHITE, 1);
    textC(bufTop, 98, "On the box: MEGA QU33PH +30", WHITE, 1);
    textC(bufTop, 113, "Off the side costs points.", WHITE, 1);
    textC(bufTop, 128, "Blue off early costs most.", WHITE, 1);
    textC(bufTop, 143, "Last fifth: SUDDEN DEATH", COL(31, 11, 11), 1);
    char s[32];
    coinCount(bufTop, 6, 170);
    if (sv.arcadePlays[ARC_MINI]) { sprintf(s, "BEST %d", sv.arcadeBest[ARC_MINI]); text(bufTop, SW - 6 - textW(s, 1), 172, s, YELLOW, 1); }
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, MB, 4, mmSel);
    textC(bufBot, 158, "or D-pad to move, L/R to angle,", GREY, 1);
    textC(bufBot, 174, "hold A for power and let go", GREY, 1);
}
void inputMiniMenu(void) {
    mmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(MB, 4, &mmSel, 1);
    if (h == 0) startMini(60);
    if (h == 1) startMini(90);
    if (h == 2) startMini(120);
    if (h == 3) goScreen(S_ARCADE);
}
void drawMiniOver(void) {
    char s[40];
    drawMini();                                              // the final layout stays behind the results
    box(bufTop, 28, 40, 200, 112, COL(2, 2, 4), GOLD);
    textC(bufTop, 50, "TIME UP", GOLD, 2);
    sprintf(s, "%d POINTS", score); textC(bufTop, 86, s, WHITE, 1);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_MINI]); textC(bufTop, 104, s, newBest ? LIME : GREY, 1);
    if (newBest) textC(bufTop, 120, "NEW BEST!", LIME, 1);
    sprintf(s, "%d COINS EARNED", coinsWon); textC(bufTop, 134, s, GOLD, 1);
    fillScreen(bufBot, DARK);
    MB[0] = (Btn){ 38, 50, 180, 36, "PLAY AGAIN", 0, 0 }; MB[1] = (Btn){ 38, 100, 180, 36, "ARCADE", 0, 0 };
    drawBtns(bufBot, MB, 2, moSel);
}
void inputMiniOver(void) {
    MB[0] = (Btn){ 38, 50, 180, 36, "", 0, 0 }; MB[1] = (Btn){ 38, 100, 180, 36, "", 0, 0 };
    int h = btnInput(MB, 2, &moSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) goScreen(S_MINI_MENU);                       // as on the website: back to pick a length
    if (h == 1) goScreen(S_ARCADE);
}
