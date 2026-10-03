// flip.c — QU33PH FLIP (the website's flip.html): flick the marker from pad to pad.
//
// The website's world, zoom (0.60 about the line 70% down) and physics, frame for frame: flick
// harder to go further, flick up for a higher arc, land on a pad to score its colour (red 1,
// green 2, blue 3). Chairs launch you several pads ahead; jagged red ground tips you over.
// LEVELS has 33 levels (11 hand-made, the rest built the website's way) with a RED / GREEN /
// BLUE grade for how many points you took; ENDLESS goes on forever; TIMED gives you 45 seconds.
//
// CONTROLS: flick on the bottom screen (how fast = how far, sideways steers, upward = height).
// Or: D-pad left/right to steer, up/down for height, hold A for power and let go.
// L/R turns the marker upright / flat. START pauses (SELECT quits). X music, Y sound effects.
#include "qu.h"
#include "assets_flip.h"

#define CW 256.0f
#define CH 384.0f
#define WW 256.0f
#define Z 0.80f                                    // (the website zooms 0.60 on phones, 1.0 on wide screens; 0.80 reads best on the DS)
#define AY (CH * 0.70f)
#define HOLD 0.16f
#define G (0.0018f * CH)
#define MH (0.085f * CH)
#define CHAIR_HH (CH * 0.105f)
#define MAXP 64

typedef struct { float x, y, w; u8 uneven, hazard, chair, chairHit, graded, color; } Pad;   // color 0 R, 1 G, 2 B
static Pad P[MAXP]; static int nP, lastFlat, stairLeft; static float stairStep;
static struct { float x, y, vx, vy, rot, spin; int pIdx, skipTo, tipT; } mk;
enum { M_LEVELS, M_ENDLESS, M_TIMED };
enum { ST_READY, ST_FLYING, ST_BOOST, ST_TIP, ST_OVER };
static int flMode, levelIdx, state, score, combo, settle, gradePts, won, timerOn, upright = 1, coinsWon, overTier, overMax;
static float camX, timeLeft, aim = 0.45f, lift = 1, power; static int charging, ph;
static const char *overTitle; static u16 overCol;
typedef struct { float x, y; int life; char txt[16]; u16 col; } Pop;
static Pop pops[6];
static int dragging, dsx, dsy, dnx, dny, dT0, fFrames;

// the website's 11 hand-made levels: per pad its gap and width (fractions of the width), the
// height change (fraction of the height), jagged ground, and a chair
typedef struct { float g, w, d; u8 u, c; } PadSpec;
typedef struct { const char *name; int n; PadSpec p[10]; } Level;
static const Level LEVELS[11] = {
{"FIRST FLIPS",5,{{0.13,0.36,0,0,0},{0.13,0.36,0,0,0},{0.14,0.34,0,0,0},{0.14,0.34,0,0,0},{0.15,0.32,0,0,0}}},
{"UP THE STAIRS",6,{{0.13,0.32,0,0,0},{0.13,0.3,-0.045,0,0},{0.13,0.3,-0.045,0,0},{0.13,0.3,-0.045,0,0},{0.13,0.3,-0.045,0,0},{0.15,0.34,0,0,0}}},
{"RED TAPE",7,{{0.13,0.32,0,0,0},{0.12,0.22,0,1,0},{0.13,0.32,0,0,0},{0.12,0.2,0,0,0},{0.13,0.32,0,0,0},{0.12,0.22,0,1,0},{0.14,0.34,0,0,0}}},
{"HIGH ROAD",8,{{0.13,0.3,0,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,-0.05,0,0},{0.14,0.3,0,0,0},{0.14,0.28,0.06,0,0},{0.14,0.28,0.06,0,0},{0.15,0.34,0,0,0}}},
{"LAUNCH PAD",8,{{0.13,0.32,0,0,0},{0.13,0.3,0,0,1},{0.13,0.28,0,0,0},{0.12,0.22,0,1,0},{0.13,0.3,0,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,-0.05,0,0},{0.15,0.34,0,0,0}}},
{"THE CLIMB",10,{{0.13,0.3,0,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.12,0.2,0,1,0},{0.13,0.26,-0.045,0,0},{0.13,0.26,-0.045,0,0},{0.13,0.26,0,0,0},{0.13,0.28,0.05,0,0},{0.13,0.28,0.05,0,0},{0.15,0.34,0,0,0}}},
{"DOWN AND OUT",7,{{0.13,0.32,0,0,0},{0.13,0.3,0.045,0,0},{0.13,0.3,0.045,0,0},{0.13,0.3,0.045,0,0},{0.13,0.28,0.045,0,0},{0.14,0.32,0,0,0},{0.14,0.32,0,0,0}}},
{"ZIGZAG",8,{{0.13,0.3,0,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,0.05,0,0},{0.13,0.28,-0.05,0,0},{0.13,0.28,0.05,0,0},{0.13,0.28,-0.05,0,0},{0.14,0.32,0,0,0},{0.14,0.32,0.05,0,0}}},
{"GREEN MILE",8,{{0.13,0.26,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.13,0.24,0,0,0},{0.14,0.3,0,0,0},{0.14,0.32,0,0,0}}},
{"GAUNTLET",9,{{0.13,0.3,0,0,0},{0.12,0.2,0,1,0},{0.13,0.28,0,0,0},{0.12,0.2,0,0,0},{0.13,0.28,0,0,0},{0.12,0.2,0,1,0},{0.13,0.28,0,0,0},{0.12,0.2,0,0,0},{0.14,0.34,0,0,0}}},
{"SKY STEPS",10,{{0.13,0.3,0,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,-0.05,0,0},{0.13,0.26,0,0,0},{0.13,0.28,0.06,0,0},{0.13,0.28,0.06,0,0},{0.15,0.34,0,0,0}}} };
#define LEVEL_COUNT 33
static int tierOf(int i) { return (sv.flipTiers[i / 16] >> ((i % 16) * 2)) & 3; }
static void setTier(int i, int t) { if (tierOf(i) >= t) return; sv.flipTiers[i / 16] = (sv.flipTiers[i / 16] & ~(3u << ((i % 16) * 2))) | ((u32)t << ((i % 16) * 2)); }

// ── the backdrop: the window tile over the theme's colour, darkened top and bottom ──
static u16 bandPal[24][256];
static const u8 *winImg(void) { return (sv.theme == 1 || sv.theme == 4) ? fl_winp : fl_win; }
void flipThemeChanged(void) {
    if (!pakIs(FLIP_PAK)) return;
    int t = sv.theme; const u16 *p = (t == 1 || t == 4) ? fl_winp_pal : fl_win_pal;
    static u16 tint[256];
    for (int i = 0; i < 256; i++) tint[i] = t == 0 || t == 4 ? (p[i] | 0x8000) : themeTint(p[i], t);
    for (int b = 0; b < 24; b++) {
        float y = (b + 0.5f) / 24;
        // the theme's backdrop colour at this height (the website's paintBG)
        int br, bg, bb;
        if (t == 1) { br = bg = bb = 19; }
        else if (t == 4) { br = 0; bg = 1; bb = 2; }
        else if (t == 3 || t == 5) { br = (int)(13 - 8 * y); bg = (int)(10 - 6 * y); bb = (int)(3 - 2 * y); }
        else if (t == 2) { br = 3 - (int)(2 * y); bg = 1; bb = (int)(6 - 3 * y); }
        else { br = 5; bg = 4 + (y < 0.55f ? 1 : 0); bb = (int)(11 + 3 * (y < 0.55f ? y / 0.55f : (1 - y) / 0.45f)) - 2; }
        // the website's darkening over the scene: 42% at the top, 14% at 55%, 72% at the bottom
        float dk = y < 0.55f ? 0.42f + (0.14f - 0.42f) * y / 0.55f : 0.14f + (0.72f - 0.14f) * (y - 0.55f) / 0.45f;
        float k = 1 - dk;
        for (int i = 0; i < 256; i++) {
            u16 c = tint[i];
            int r = (int)(((c & 31) * 0.85f + br * 0.15f) * k), g = (int)((((c >> 5) & 31) * 0.85f + bg * 0.15f) * k), bl = (int)((((c >> 10) & 31) * 0.85f + bb * 0.15f) * k);
            bandPal[b][i] = COL(r, g, bl);
        }
    }
}
int flipEnter(void) { if (!pakUse(FLIP_PAK, FLIP_PAK_SIZE, FLIP_PAK_ID)) return 0; flipThemeChanged(); return 1; }

// ── building the course (the website's pad(), addPlatform(), buildLevel(), makeFlyable()) ──
static float rnd(float a, float b) { return a + frand() * (b - a); }
static float maxReach(float dy) {
    float vx = 0.022f * WW, vy = -0.038f * CH, d = vy * vy + 2 * G * dy;
    return vx * ((-vy + fsqrt(d > 0.0001f ? d : 0.0001f)) / G);
}
static void addPad(float x, float y, float w, int uneven, int chair) {
    if (nP >= MAXP) return;
    Pad *p = &P[nP++]; p->x = x; p->y = y; p->w = w; p->uneven = uneven; p->hazard = uneven; p->chair = chair;
    p->chairHit = p->graded = 0; float s = frand(); p->color = s < 0.3334f ? 0 : s < 0.6667f ? 1 : 2;
}
static float clampY(float y) { return y < CH * 0.44f ? CH * 0.44f : y > CH * 0.80f ? CH * 0.80f : y; }
static void addPlatform(void) {
    Pad *last = &P[nP - 1]; int n = nP, hard = n > 25; float d = hard ? ((n - 25) / 40.0f > 1 ? 1 : (n - 25) / 40.0f) : 0;
    if (hard && stairLeft <= 0 && frand() < 0.24f) { stairLeft = 3 + rand() % 3; stairStep = (frand() < 0.5f ? -1 : 1) * rnd(0.028f, 0.050f) * CH; }
    int uneven = !last->uneven && n > 6 && frand() < (hard ? 0.10f + d * 0.15f : 0.05f);
    float w = uneven ? rnd(0.16f, 0.23f) * WW : hard ? rnd(0.24f, 0.33f) * WW - d * 0.04f * WW : rnd(0.30f, 0.40f) * WW;
    float gap = uneven ? rnd(0.10f, 0.15f) * WW : hard ? rnd(0.14f, 0.21f) * WW + d * 0.08f * WW : rnd(0.11f, 0.17f) * WW;
    float y = stairLeft > 0 ? (stairLeft--, last->y + stairStep) : last->y + (hard ? rnd(-0.045f, 0.045f) : rnd(-0.018f, 0.018f)) * CH;
    y = clampY(y);
    float x = last->x + last->w + gap;
    if (!uneven) {
        Pad *lf = &P[lastFlat]; float lim = maxReach(y - lf->y) * 0.86f;
        if (x - lf->x > lim) x = lf->x + lim;
        float minX = last->x + last->w + WW * 0.05f; if (x < minX) x = minX;
    }
    addPad(x, y, w, uneven, !uneven && n > 3 && frand() < 0.15f);
    if (!uneven) lastFlat = nP - 1;
}
static void makeFlyable(void) {
    int last = 0, i = 1;
    while (i < nP) {
        Pad *p = &P[i];
        if (p->uneven) { i++; continue; }
        float lim = maxReach(p->y - P[last].y) * 0.86f;
        if (p->x - P[last].x > lim) {
            int cleared = 0;
            for (int k = last + 1; k < i; k++) if (P[k].uneven) { P[k].uneven = 0; cleared = 1; }
            if (cleared) { i = last + 1; continue; }
            float a = P[i - 1].x + P[i - 1].w + WW * 0.05f, b = P[last].x + lim; P[i].x = a > b ? a : b;
        }
        last = i; i++;
    }
    P[nP - 1].uneven = 0;
}
static void buildLevel(int i) {
    nP = 0; addPad(WW * 0.06f, CH * 0.70f, WW * 0.42f, 0, 0); lastFlat = 0; stairLeft = 0; stairStep = 0;
    if (i < 11) {
        const Level *L = &LEVELS[i];
        for (int k = 0; k < L->n; k++) { const PadSpec *s = &L->p[k]; Pad *last = &P[nP - 1];
            float y = clampY(last->y + s->d * CH), x = last->x + last->w + s->g * WW, w = s->w * WW;
            if (!s->u) { Pad *lf = &P[lastFlat]; float lim = maxReach(y - lf->y) * 0.86f;
                if (x - lf->x > lim) x = lf->x + lim;
                float minX = last->x + last->w + WW * 0.05f; if (x < minX) x = minX; }
            addPad(x, y, w, s->u, s->c); if (!s->u) lastFlat = nP - 1; }
    } else for (int k = 0; k < 10 + i; k++) addPlatform();
    makeFlyable();
}
static void buildEndless(void) { nP = 0; addPad(WW * 0.06f, CH * 0.70f, WW * 0.42f, 0, 0); lastFlat = 0; stairLeft = 0; stairStep = 0; for (int i = 0; i < 10; i++) addPlatform(); }
static void trimEndless(void) {                     // endless: forget pads far behind to keep going forever
    int drop = mk.pIdx - 4; if (drop <= 0 || flMode == M_LEVELS) return;
    memmove(P, P + drop, sizeof(Pad) * (nP - drop)); nP -= drop; lastFlat -= drop; if (lastFlat < 0) lastFlat = 0;
    mk.pIdx -= drop; if (mk.skipTo >= 0) mk.skipTo -= drop;
}
static void startRun(void) {
    camX = 0; score = 0; combo = 0; memset(pops, 0, sizeof pops); settle = 0; won = 0; gradePts = 0;
    mk.x = P[0].x + P[0].w * 0.5f; mk.y = P[0].y; mk.vx = mk.vy = mk.rot = mk.spin = 0; mk.pIdx = 0; mk.skipTo = -1;
    state = ST_READY; charging = 0; camX = mk.x - CW * HOLD / Z; if (camX < 0) camX = 0;
    timeLeft = 45; timerOn = 0; screen = S_FLIP;
}
static void startMode(int m) { flMode = m; if (m == M_LEVELS) buildLevel(levelIdx); else buildEndless(); startRun(); }

// ── the website's flip(), chairLaunch(), land() and step() ─────────────────
static void popAdd(float x, float y, const char *t, u16 c) {
    for (int i = 0; i < 6; i++) if (pops[i].life <= 0) { pops[i] = (Pop){ x, y, 44, "", c }; strncpy(pops[i].txt, t, 15); return; }
}
static void flip(float p, float am, float lf) {
    sfxPlop();
    mk.vx = (0.008f + p * 0.014f) * WW * (0.55f + 0.75f * (am + 0.5f > 0 ? am + 0.5f : 0));
    mk.vy = -(0.026f + p * 0.012f) * CH * lf;
    float T = 2 * fabsf_(mk.vy) / G;
    mk.spin = (6.2831853f * (1 + (int)(p * 2 + 0.5f))) / T; mk.rot = 0;
    state = ST_FLYING; timerOn = 1;
}
static void chairLaunch(int idx) {
    int target = idx + 3 + rand() % 3;
    if (flMode == M_LEVELS) { if (target > nP - 1) target = nP - 1; }
    else while (nP <= target + 10 && nP < MAXP) addPlatform();
    if (target > nP - 1) target = nP - 1;
    Pad *t = &P[target];
    if (t->uneven) { t->uneven = 0; if (target > lastFlat) lastFlat = target; }
    t->chair = 0;
    float dy = t->y - mk.y; mk.vy = -0.044f * CH;
    float d = mk.vy * mk.vy + 2 * G * dy, T = (-mk.vy + fsqrt(d > 0.0001f ? d : 0.0001f)) / G;
    mk.vx = ((t->x + t->w * 0.5f) - mk.x) / T;
    mk.spin = (6.2831853f * 4) / T; mk.skipTo = target; state = ST_BOOST;
    popAdd(mk.x, mk.y - MH * 1.4f, "CHAIR LAUNCH!", COL(31, 11, 11));
}
static void endGame(const char *title, u16 col, int win) {
    state = ST_OVER; timerOn = 0; won = win; overTitle = title; overCol = col;
    if (!win && strcmp(title, "TIME UP")) playAdpcm(fs2_miss, FS2_MISS_LEN, 16000, 80);
    if (mk.pIdx > sv.flipBestP) sv.flipBestP = mk.pIdx;
    if (score > sv.flipBest) sv.flipBest = score;
    if (score > sv.arcadeBest[ARC_FLIP]) sv.arcadeBest[ARC_FLIP] = score;
    sv.arcadePlays[ARC_FLIP]++;
    int c = score / 5; if (c > 20) c = 20;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite();
    screen = S_FLIP_OVER;
}
static void finishLevel(void) {
    if (levelIdx + 1 > sv.flipDone) sv.flipDone = levelIdx + 1;
    int maxPts = 0; for (int k = 1; k < nP; k++) if (!P[k].hazard) maxPts += P[k].color + 1;
    int got = gradePts < maxPts ? gradePts : maxPts; float r = maxPts > 0 ? (float)got / maxPts : 1;
    overTier = r >= 0.90f ? 3 : r >= 0.60f ? 2 : 1; overMax = maxPts; setTier(levelIdx, overTier);
    endGame("LEVEL CLEAR", COL(15, 31, 19), 1);
}
static void land(int i) {
    Pad *p = &P[i];
    mk.y = p->y; mk.vx = mk.vy = 0; mk.rot = 0; mk.skipTo = -1;
    if (p->uneven) { state = ST_TIP; mk.tipT = 0; return; }
    mk.pIdx = i; combo++;
    int pts = p->color + 1;                              // red 1, green 2, blue 3
    if (!p->graded) { p->graded = 1; gradePts += pts; }
    score += pts;
    char t[8]; sprintf(t, "+%d", pts);
    popAdd(mk.x, p->y - MH * 1.3f, t, p->color == 0 ? COL(31, 11, 11) : p->color == 2 ? COL(9, 19, 31) : COL(15, 31, 19));
    state = ST_READY; settle = 9;
    if (flMode == M_LEVELS) { if (i >= nP - 1) finishLevel(); }
    else { while (nP < mk.pIdx + 11 && nP < MAXP) addPlatform(); trimEndless(); }
}
void updateFlip(void) {
    fFrames++;
    if (state == ST_OVER) return;
    if (settle > 0) settle--;
    if (flMode == M_TIMED && timerOn) { timeLeft -= 1.0f / 60; if (timeLeft <= 0) { timeLeft = 0; endGame("TIME UP", COL(31, 27, 0), 0); return; } }
    if (state == ST_TIP) { mk.tipT++; mk.rot += 0.11f; if (mk.tipT > 26) endGame("TIPPED OVER", COL(28, 7, 7), 0); return; }
    if (state == ST_FLYING || state == ST_BOOST) {
        float prevY = mk.y;
        mk.x += mk.vx; mk.vy += G; mk.y += mk.vy; mk.rot += mk.spin;
        if (state == ST_FLYING)
            for (int i = 0; i < nP; i++) { Pad *p = &P[i]; if (!p->chair || p->chairHit) continue;
                float cx = p->x + p->w * 0.5f, cw = CHAIR_HH;
                if (mk.x > cx - cw * 0.5f && mk.x < cx + cw * 0.5f && mk.y > p->y - CHAIR_HH && mk.y < p->y + MH * 0.25f) { p->chairHit = 1; chairLaunch(i); break; } }
        if (mk.vy > 0) {
            if (state == ST_BOOST) { int i = mk.skipTo; if (i >= 0 && i < nP && mk.x >= P[i].x && mk.x <= P[i].x + P[i].w && prevY <= P[i].y && mk.y >= P[i].y) land(i); }
            else for (int i = 0; i < nP; i++) { Pad *p = &P[i]; if (mk.x >= p->x && mk.x <= p->x + p->w && prevY <= p->y && mk.y >= p->y) { land(i); break; } }
        }
        if (state != ST_OVER && mk.y > CH * 1.25f) endGame("OFF THE EDGE", COL(28, 7, 7), 0);
    }
    float tc = mk.x - CW * HOLD / Z; camX += (tc - camX) * 0.12f; if (camX < 0) camX = 0;
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) { pops[i].life--; pops[i].y -= CH * 0.0012f; }
}

// ── input ─────────────────────────────────────────────────────────────────
void inputFlip(void) {
    if (screen == S_FLIP_PAUSE) {
        if (kDown & KEY_START) screen = S_FLIP;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_FLIP_PAUSE; dragging = charging = 0; return; }
    if (kDown & KEY_X) musicToggle();
    if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    if (kDown & (KEY_L | KEY_R)) upright = !upright;
    if (state != ST_READY) { dragging = charging = 0; return; }
    // the website's flick: how fast = power, sideways = steering, upward = height
    if (kDown & KEY_TOUCH) { dragging = 1; dsx = dnx = tX; dsy = dny = tY; dT0 = fFrames; }
    if (dragging && (kHeld & KEY_TOUCH)) { dnx = tX; dny = tY; }
    if (dragging && (kUp & KEY_TOUCH)) {
        dragging = 0;
        float dx = dnx - dsx, dy = dny - dsy, dist = fsqrt(dx * dx + dy * dy), dt = (fFrames - dT0) * 16.67f; if (dt < 40) dt = 40;
        if (dist >= WW * 0.03f) {
            float sp = dist / dt, p = sp / (WW * 0.0042f); p = p < 0.10f ? 0.10f : p > 1 ? 1 : p;
            float am = dx / (WW * 0.35f); am = am < -1 ? -1 : am > 1 ? 1 : am;
            float lf = (-dy) / (CH * 0.16f) + 0.55f; lf = lf < 0.35f ? 0.35f : lf > 1.35f ? 1.35f : lf;
            flip(p, am, lf);
        }
        return;
    }
    if (dragging) return;
    // buttons: steer with left/right, height with up/down, hold A for power
    if (kHeld & KEY_LEFT)  { aim -= 0.03f; if (aim < -1) aim = -1; }
    if (kHeld & KEY_RIGHT) { aim += 0.03f; if (aim > 1) aim = 1; }
    if (kHeld & KEY_UP)    { lift += 0.02f; if (lift > 1.35f) lift = 1.35f; }
    if (kHeld & KEY_DOWN)  { lift -= 0.02f; if (lift < 0.35f) lift = 0.35f; }
    if (kDown & KEY_A) { charging = 1; ph = 0; }
    if (charging) {
        ph++; float p = (ph % 64) / 32.0f; power = p < 1 ? p : 2 - p; if (power < 0.1f) power = 0.1f;
        if (kDown & KEY_B) charging = 0;
        else if (kUp & KEY_A) { charging = 0; flip(power, aim, lift); }
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
static float SX(float w) { return (w - camX) * Z; }
static float SY(float w) { return AY + (w - AY) * Z; }
FAST static void drawBg(void) {
    const u8 *img = winImg(); int TW = FL_WIN_W;
    int ox = ((int)(camX * 0.10f) % TW + TW) % TW;          // the website's 0.10 parallax
    for (int gy = 0; gy < 2 * SH; gy++) {
        const u16 *pal = bandPal[gy / 16]; const u8 *s = &img[gy * TW];
        u16 *d = gy < SH ? &bufTop[gy * SW] : &bufBot[(gy - SH) * SW];
        int x = 0, sx = ox;
        while (x < SW) { int run = TW - sx; if (run > SW - x) run = SW - x;
            for (int i = 0; i < run; i++) d[x + i] = pal[s[sx + i]];
            x += run; sx = 0; }
    }
    gClipLo = 0; gClipHi = 2 * SH;
    if (sv.theme == 4) {                                      // NEON: the website's green grid
        int gs = (int)(CH * 0.042f), g0 = ((int)(-camX * 0.15f) % gs + gs) % gs;
        for (int gy = 0; gy < 2 * SH; gy++) for (int x = g0; x < SW; x += gs) gpx(x, gy, COL(2, 11, 6));
        for (int gy = 0; gy < 2 * SH; gy += gs) for (int x = 0; x < SW; x++) gpx(x, gy, COL(2, 11, 6));
    } else if (sv.theme == 1) {                               // CARTOON: ben-day dots
        for (int gy = 3; gy < 2 * SH; gy += 7) for (int x = (gy / 7 % 2) * 3; x < SW; x += 7) gpx(x, gy, COL(8, 8, 8));
    } else if (sv.theme == 2) {                               // NIGHT (the website's galaxy): stars
        for (int i = 0; i < 70; i++) { int x = (i * 97 + 13) % SW, y = (i * 53 + 7) % (2 * SH); if (((fFrames / 20) + i) % 5) gpx(x, y, COL(28, 26, 31)); }
    }
}
static void drawPad(Pad *p) {
    float x0 = SX(p->x), x1 = SX(p->x + p->w), y = SY(p->y);
    if (x1 < -20 || x0 > SW + 20) return;
    if (p->uneven) {                                          // jagged red ground
        int teeth = (int)(p->w / (CW * 0.05f)); if (teeth < 4) teeth = 4;
        for (int i = 0; i < teeth; i++) {
            float ax = x0 + (x1 - x0) * i / teeth, bx = x0 + (x1 - x0) * (i + 1) / teeth, ay = y + ((i % 2) ? -CH * 0.016f * Z : 0), by = y + (((i + 1) % 2) ? -CH * 0.016f * Z : 0);
            for (int k = 0; k <= 12; k++) { int px = (int)(ax + (bx - ax) * k / 12), py = (int)(ay + (by - ay) * k / 12); gpx(px, py, COL(31, 6, 6)); gpx(px, py + 1, COL(31, 6, 6)); }
        }
        return;
    }
    // a pad is a marker lying on its side (the website draws stackred/green/blue)
    const u16 *s = p->color == 0 ? fl_pad0 : p->color == 1 ? fl_pad1 : fl_pad2;
    int w = p->color == 0 ? FL_PAD0_W : p->color == 1 ? FL_PAD1_W : FL_PAD2_W;
    float len = (x1 - x0), sc = len / FL_PAD0_H;
    drawMarkerFx(s, w, FL_PAD0_H, (int)((x0 + x1) / 2), (int)(y + CH * 0.015f * Z), 1.5707963f, sc, p->color);
    if (p->chair && !p->chairHit) {
        float ch = CHAIR_HH * Z;
        blitRotScale(chair, CHAIR_W, CHAIR_H, (int)((x0 + x1) / 2), (int)(y - ch / 2), 0, ch / CHAIR_H);
    }
}
static void drawMk(void) {
    int cart = sv.theme == 1;
    const u16 *s = cart ? fl_mkc : fl_mk; int w = cart ? FL_MKC_W : FL_MK_W, h = cart ? FL_MKC_H : FL_MK_H;
    // the website's sizing: the marker is MH long; upright stands on its end, flat lies on its side
    int wide = w >= h; float len = MH * Z, sc = len / (wide ? w : h);
    float base = (wide == upright) ? -1.5707963f : 0, visH = upright ? len : (wide ? h : w) * sc;
    float sq = settle > 0 ? 1 + settle * 0.012f : 1;
    float cx = SX(mk.x), cy = SY(mk.y) - visH / 2;
    if (state == ST_TIP) cy = SY(mk.y) - visH / 2 + mk.tipT * 0.3f;
    drawMarkerFx(s, w, h, (int)cx, (int)cy, base + mk.rot, sc * sq, 2);
}
void drawFlip(void) {
    char s[32];
    drawBg();
    for (int i = 0; i < nP; i++) drawPad(&P[i]);
    if (state == ST_READY && !dragging) {                     // button play: where it's aimed
        float p = charging ? power : 0.5f;
        float vx = (0.008f + p * 0.014f) * WW * (0.55f + 0.75f * (aim + 0.5f > 0 ? aim + 0.5f : 0)), vy = -(0.026f + p * 0.012f) * CH * lift;
        float x = mk.x, y = mk.y - MH * 0.5f;
        for (int k = 0; k < 40; k++) { x += vx; vy += G; y += vy; if (k % 3 == 0) grect((int)SX(x) - 1, (int)SY(y) - 1, 2, 2, charging ? COL(31, 27, 4) : COL(24, 24, 24)); }
    }
    drawMk();
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) gtext((int)SX(pops[i].x) - textW(pops[i].txt, 1) / 2, (int)SY(pops[i].y), pops[i].txt, pops[i].col, 1);
    // HUD on the top screen
    sprintf(s, "%d PTS", score); text(bufTop, 6, 4, s, WHITE, 1);
    if (flMode == M_LEVELS) { sprintf(s, "%d. %s", levelIdx + 1, levelIdx < 11 ? LEVELS[levelIdx].name : "LEVEL"); text(bufTop, SW - 6 - textW(s, 1), 4, s, GOLD, 1); }
    else if (flMode == M_TIMED) { sprintf(s, "TIME %d", (int)(timeLeft + 0.99f)); text(bufTop, SW - 6 - textW(s, 1), 4, s, timeLeft < 10 ? RED : GOLD, 1); }
    else { sprintf(s, "BEST %d", sv.flipBest); text(bufTop, SW - 6 - textW(s, 1), 4, s, GOLD, 1); }
    if (state == ST_READY && score == 0 && mk.pIdx == 0) textC(bufTop, 24, "flick to flip - or hold A", GREY, 1);
    textS(bufBot, SW - 6 - textSW(upright ? "L/R: UPRIGHT" : "L/R: FLAT"), SH - 12, upright ? "L/R: UPRIGHT" : "L/R: FLAT", WHITE);
    if (screen == S_FLIP_PAUSE) {
        textC(bufTop, 80, "PAUSED", YELLOW, 2);
        textC(bufTop, 120, "START resume - SELECT quit", WHITE, 1);
    }
}

// ── menus ─────────────────────────────────────────────────────────────────
static Btn FB[4]; static int menuSel, lvlSel, overSel;
static void menuLayout(void) {
    FB[0] = (Btn){ 38, 8, 180, 36, "LEVELS", 0, 0 }; FB[1] = (Btn){ 38, 50, 180, 36, "ENDLESS", 0, 0 };
    FB[2] = (Btn){ 38, 92, 180, 36, "TIMED", 0, 0 }; FB[3] = (Btn){ 68, 140, 120, 30, "BACK", 0, 0 };
}
void drawFlipMenu(void) {
    char s[40];
    fillScreen(bufTop, DARK);
    textC(bufTop, 6, "QU33PH FLIP", GOLD, 2);
    textC(bufTop, 40, "Flick the marker pad to pad.", WHITE, 1);
    textC(bufTop, 56, "Faster flick = further,", WHITE, 1);
    textC(bufTop, 71, "upward = higher.", WHITE, 1);
    textC(bufTop, 89, "red pad 1  green 2  blue 3", WHITE, 1);
    textC(bufTop, 105, "chairs launch you ahead;", COL(31, 26, 9), 1);
    textC(bufTop, 120, "jagged red ground tips you", COL(31, 11, 11), 1);
    sprintf(s, "LEVELS %d / %d   BEST %d", sv.flipDone, LEVEL_COUNT, sv.flipBest); textC(bufTop, 146, s, GREY, 1);
    coinCount(bufTop, 6, 176);
    fillScreen(bufBot, DARK); menuLayout(); drawBtns(bufBot, FB, 4, menuSel);
}
void inputFlipMenu(void) {
    menuLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(FB, 4, &menuSel, 1);
    if (h == 0) goScreen(S_FLIP_LEVELS);
    if (h == 1) startMode(M_ENDLESS);
    if (h == 2) startMode(M_TIMED);
    if (h == 3) goScreen(S_ARCADE);
}
static Btn LB[LEVEL_COUNT + 1];
static void lvlLayout(void) {
    for (int i = 0; i < LEVEL_COUNT; i++) {            // little keys, coloured by the marker you earned
        int t = tierOf(i), open = i <= sv.flipDone;
        LB[i] = (Btn){ 8 + (i % 8) * 30, 6 + (i / 8) * 28, 28, 25, "", t == 3 ? COL(9, 19, 31) : t == 2 ? COL(15, 31, 19) : t == 1 ? COL(31, 11, 11) : 0, !open };
        sprintf(LB[i].label, "%d", i + 1);
    }
    LB[LEVEL_COUNT] = (Btn){ 68, 152, 120, 30, "BACK", 0, 0 };
}
void drawFlipLevels(void) {
    char s[40];
    fillScreen(bufTop, DARK);
    textC(bufTop, 10, "FLIP LEVELS", GOLD, 2);
    sprintf(s, "%d / %d CLEARED", sv.flipDone, LEVEL_COUNT); textC(bufTop, 46, s, WHITE, 1);
    textC(bufTop, 70, "your grade is the marker you earn:", GREY, 1);
    textC(bufTop, 88, "RED - GREEN (60%) - BLUE (90%)", WHITE, 1);
    if (lvlSel < LEVEL_COUNT) { sprintf(s, "%d. %s", lvlSel + 1, lvlSel < 11 ? LEVELS[lvlSel].name : "BUILT FOR YOU"); textC(bufTop, 120, s, GOLD, 1); }
    fillScreen(bufBot, DARK); lvlLayout(); drawBtns(bufBot, LB, LEVEL_COUNT + 1, lvlSel);
}
void inputFlipLevels(void) {
    lvlLayout();
    if (kDown & KEY_B) { goScreen(S_FLIP_MENU); return; }
    int h = btnInput(LB, LEVEL_COUNT + 1, &lvlSel, 8);
    if (h == LEVEL_COUNT) goScreen(S_FLIP_MENU);
    else if (h >= 0 && h <= sv.flipDone) { levelIdx = h; startMode(M_LEVELS); }
}
static Btn OB[3];
static int overN(void) {
    int next = won && flMode == M_LEVELS && levelIdx < LEVEL_COUNT - 1;
    int n = 0;
    if (next) OB[n++] = (Btn){ 38, 20, 180, 34, "NEXT LEVEL", 0, 0 };
    OB[n++] = (Btn){ 38, 20 + n * 46, 180, 34, "RETRY", 0, 0 };
    OB[n++] = (Btn){ 38, 20 + n * 46, 180, 34, "MENU", 0, 0 };
    return n;
}
void drawFlipOver(void) {
    char s[48];
    drawFlip();
    box(bufTop, 4, 40, 248, 116, COL(2, 2, 4), GOLD);
    textC(bufTop, 48, overTitle, overCol, 2);
    if (won && flMode == M_LEVELS) {
        int got = gradePts < overMax ? gradePts : overMax;
        sprintf(s, "%s - %d / %d PTS", levelIdx < 11 ? LEVELS[levelIdx].name : "LEVEL", got, overMax); textC(bufTop, 84, s, WHITE, 1);
        const char *T[4] = { "", "RED MARKER", "GREEN MARKER", "BLUE MARKER" }; const u16 C[4] = { 0, COL(31, 11, 11), COL(15, 31, 19), COL(9, 19, 31) };
        textC(bufTop, 104, T[overTier], C[overTier], 1);
    } else { sprintf(s, "%d PTS - %d PADS - BEST %d", score, mk.pIdx, sv.flipBest); textC(bufTop, 90, s, WHITE, 1); }
    sprintf(s, "+%d COINS", coinsWon); textC(bufTop, 128, s, GOLD, 1);
    fillScreen(bufBot, DARK); int n = overN(); drawBtns(bufBot, OB, n, overSel);
}
void inputFlipOver(void) {
    int n = overN(), h = btnInput(OB, n, &overSel, 1);
    if (kDown & KEY_B) { goScreen(S_FLIP_MENU); return; }
    if (h < 0) return;
    const char *l = OB[h].label;
    if (!strcmp(l, "NEXT LEVEL")) { levelIdx++; startMode(M_LEVELS); }
    else if (!strcmp(l, "RETRY")) startMode(flMode);
    else goScreen(S_FLIP_MENU);
}
