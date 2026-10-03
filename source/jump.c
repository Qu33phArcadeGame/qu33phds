// jump.c — QU33PH JUMP (the platformer inside the website's main page; Dreamon left out).
//
// Four endless modes, as on the website, each with its own best:
//   COMBO  the full mix: climb, run, bounce chains, chair launches to the MEGA, flappy gauntlets
//   JUMP   climb forever: dodge markers, ride moving platforms, zigzag towers
//   RUN    endless runner: dash and leap the (patrolling) markers
//   FLAP   pure flappy: thread the marker pipes forever
// The course is built the website's way (its generator, reach limits and section mix) and the
// physics are its numbers, run at its 1.14x speed. The view spans both screens.
//
// CONTROLS: D-pad left/right to run, A / B / UP to jump (and to flap). Touch: hold the left or
// right third of the bottom screen to run, tap the middle to jump. START pauses (SELECT quits).
#include "qu.h"
#include "assets_jump.h"

#define W 256.0f
#define H 384.0f
#define GAPMAX 96.0f
#define RISEMAX 70.0f
#define JUMPV (-10.4f)
#define FLAPV (-9.3f * (H / 494.0f))
#define FLAPSPD (5.2f * (W / 412.0f))
#define BASEZ 0.80f
#define NPL 200
#define NOB 80
#define NCO 100
#define NPI 24
typedef struct { float x, y, w, baseX, amp, spd, phase, dx; u8 ground, chair, chain, flappy, mz, moving; } Plat;
typedef struct { float x, y, w, h, minX, maxX, vx, roll; u8 col, patrol; } Obs;
typedef struct { float x, y; u8 got; } Coin;
typedef struct { float x, w, gapY, gapH; u8 passed, col; } Pipe;
static Plat pl[NPL]; static int nPl; static Obs ob[NOB]; static int nOb; static Coin co[NCO]; static int nCo; static Pipe pi[NPI]; static int nPi;
static struct { float x, y, vx, vy, w, h; int onGround, face; } p;
enum { M_COMBO, M_JUMP, M_RUN, M_FLAP };
static const char *MODE_NAME[4] = { "COMBO", "JUMP", "RUN", "FLAP" };
static const char *MODE_BLURB[4] = { "the full mix: climb, run, bounce & flap", "climb forever: dodge markers, ride platforms", "endless runner: dash & leap the markers", "pure flappy: thread the pipes forever" };
static int lastFlap;
static int jMode, state, score, frame, coinsWon, newBest, menuSel, overSel, upStart, cycles, lastLaunch, megaFlash, bounceCombo, bounceFlash, flappyPassed, flappyHint;
static int flight, bouncing, flappy, flappyFreeze, flapEndless, stairDir, stairLeft, flapPipeN, moveDir, jumpQ, speedAcc;
static float camX, camY, zoom, zoomT, genX, genY, segLeft, lastPlatX, noMarkerUntilX, startX, startY, bestX, bestUp, lastGroundY, progress, flappyFloorY, flappyCeilY, flappyEndX, flapLastX, flapBandY;
static int genMode;                                    // 0 up, 1 right, 2 stairs
static struct { float x, y, w, h; int hit, on; } mega;
static float deco[44][4];
enum { ST_PLAY, ST_OVER };
#define JBEST(m) sv.spare[m]                           // each mode's best (spare save room, so old saves read 0)

int jumpEnter(void) { return pakUse(JUMP_PAK, JUMP_PAK_SIZE, JUMP_PAK_ID); }

// ── the runners (the website's PF_CHARS; shown by picture only) ───────────
// cls 0 human, 1 cat (quick, jumps twice as high), 2 chicken (floats, one air jump; H3MMINGWAY
// also lays eggs that hatch into a following flock)
typedef struct { float speed, jump, grav; int airJumps, cls; const u16 *s[3][2]; int w[3][2], h; } Runner;
static Runner RUN[7];
#define JCHAR sv.spare[4]                               // the chosen runner (spare save room)
static void runnersInit(void) {
    #define SET(i, sp, jp, gr, aj, cl, nm, NM) RUN[i] = (Runner){ sp, jp, gr, aj, cl, { { nm##_stand##r, nm##_stand##l }, { nm##_run##r, nm##_run##l }, { nm##_jump##r, nm##_jump##l } }, \
        { { NM##_STANDR_W, NM##_STANDL_W }, { NM##_RUNR_W, NM##_RUNL_W }, { NM##_JUMPR_W, NM##_JUMPL_W } }, NM##_STANDR_H }
    RUN[0] = (Runner){ 1, 1, 1, 0, 0, { { jg_stand, jg_stand }, { jg_runr, jg_runl }, { jg_jumpr, jg_jumpl } }, { { JG_STAND_W, JG_STAND_W }, { JG_RUNR_W, JG_RUNL_W }, { JG_JUMPR_W, JG_JUMPL_W } }, JG_STAND_H };
    SET(1, 1, 1, 1, 0, 0, jc_coby, JC_COBY);
    SET(2, 1.5f, 1.414f, 1, 0, 1, jc_eph3, JC_EPH3);
    SET(3, 1.5f, 1.414f, 1, 0, 1, jc_smok3y, JC_SMOK3Y);
    SET(4, 1.5f, 1.025f, 0.70f, 1, 2, jc_b3ll, JC_B3LL);
    SET(5, 1.5f, 1.025f, 0.70f, 1, 2, jc_bo, JC_BO);
    SET(6, 1.5f, 1.025f, 0.70f, 1, 2, jc_hem, JC_HEM);
    #undef SET
}
static Runner *ch(void) { if (JCHAR > 6) JCHAR = 0; return &RUN[JCHAR]; }
static float baseZoom(void) { return ch()->cls ? BASEZ * 0.76f : BASEZ; }   // cats and chickens see more of the level
static int airUsed;

// ── H3MMINGWAY's flock: eggs every 50-100 m that crack, peek and hatch, then follow her trail ──
#define TRAILN 512
static float trX[TRAILN], trY[TRAILN], trD[TRAILN]; static signed char trF[TRAILN]; static int trHead, trN;
static float trailDist, runDist, lastTX, lastTY, nextEgg; static int trailFace = 1;
typedef struct { float born, gap, t1, t2, t3, sx, sy, lx, vs; int col, sf, init; } Chick;
static Chick flock[14]; static int nFlock;
static void flockReset(void) { trHead = trN = 0; trailDist = runDist = 0; nFlock = 0; trailFace = 1; nextEgg = (20 + frand() * 10) * 13; lastTX = p.x; lastTY = p.y; }
static void flockStep(void) {
    if (JCHAR != 6) return;
    float dx = p.x - lastTX, dy = p.y - lastTY, d = fsqrt(dx * dx + dy * dy);
    if (d > 0.005f) {
        trailDist += d; runDist += fabsf_(dx); lastTX = p.x; lastTY = p.y;
        if (dx > 0.35f) trailFace = 1; else if (dx < -0.35f) trailFace = -1;
        int last = (trHead + TRAILN - 1) % TRAILN;
        if (!trN || trailDist - trD[last] >= 2.5f) { trX[trHead] = p.x; trY[trHead] = p.y; trD[trHead] = trailDist; trF[trHead] = trailFace; trHead = (trHead + 1) % TRAILN; if (trN < TRAILN) trN++; }
    }
    if (runDist >= nextEgg) {
        if (nFlock >= 14) { memmove(flock, flock + 1, sizeof(Chick) * 13); nFlock = 13; }
        Chick *c = &flock[nFlock++]; memset(c, 0, sizeof *c);
        c->born = runDist; c->gap = (7 + (nFlock - 1) * 5.5f) * 13; c->col = rand() % 5;
        c->t1 = (20 + frand() * 10) * 13; c->t2 = (20 + frand() * 10) * 13; c->t3 = (20 + frand() * 10) * 13;
        nextEgg = runDist + (50 + frand() * 50) * 13;
    }
}
static int trailAt(float dist, float *x, float *y) {    // where she was, dist behind
    if (!trN) return 0;
    float want = trailDist - dist;
    for (int k = 1; k <= trN; k++) { int i = (trHead - k + TRAILN) % TRAILN;
        if (trD[i] <= want || k == trN) { int b = (i + 1) % TRAILN; if (k == 1) b = i;
            float span = trD[b] - trD[i]; float t = span > 0 ? (want - trD[i]) / span : 0; if (t < 0) t = 0; if (t > 1) t = 1;
            *x = trX[i] + (trX[b] - trX[i]) * t; *y = trY[i] + (trY[b] - trY[i]) * t; return 1; } }
    return 0;
}
void jumpThemeChanged(void) {}
static float rnd(void) { return frand(); }

// ── building the course (the website's pfGenStep and its sections) ────────
static Plat *addPlat(float x, float y, float w) { if (nPl >= NPL) return 0; Plat *q = &pl[nPl++]; memset(q, 0, sizeof *q); q->x = x; q->y = y; q->w = w; return q; }
static void addObs(float x, float y, float w, float h) { if (nOb >= NOB) return; Obs *o = &ob[nOb++]; memset(o, 0, sizeof *o); o->x = x; o->y = y; o->w = w; o->h = h; o->col = rand() % 3; }
static void addCoin(float x, float y) { if (nCo >= NCO) return; co[nCo].x = x; co[nCo].y = y; co[nCo].got = 0; nCo++; }
static void patrol(Obs *o, float lo, float hi, float span, float sp) {
    o->patrol = 1; o->minX = o->x - span > lo ? o->x - span : lo; o->maxX = o->x + span < hi ? o->x + span : hi; o->vx = (sp + rnd() * 0.7f) * (rnd() < 0.5f ? 1 : -1);
}
static void goRight(float x, float y, float segMul, float clear) { genMode = 1; genX = x; genY = y; lastPlatX = x; segLeft = W * segMul; noMarkerUntilX = clear; }
static void goUp(float y) { genMode = 0; genY = y - (46 + rnd() * 18); segLeft = 6 + rand() % 4; upStart = (int)segLeft; }
static void zigzag(float gx, float gy) {
    int n = 5 + rand() % 3; float side = rnd() < 0.5f ? 1 : -1, cx = gx, cy = gy;
    for (int k = 0; k < n; k++) {
        float pw = W * 0.14f + rnd() * W * 0.04f, px = cx + side * (W * 0.15f + rnd() * W * 0.06f) - pw / 2;
        if (px < W * 0.07f) px = W * 0.07f; if (px > W * 0.93f - pw) px = W * 0.93f - pw;
        Plat *q = addPlat(px, cy, pw);
        if (q && k >= 2 && rnd() < 0.3f) { q->moving = 1; q->baseX = q->x; q->amp = W * (0.06f + rnd() * 0.07f); q->spd = 0.02f + rnd() * 0.02f; q->phase = rnd() * 6.28f; }
        if (k > 0 && k < n - 1 && rnd() < 0.42f) addObs(px + pw * 0.5f, cy, 18, 26);
        if (rnd() < 0.5f) addCoin(px + pw * 0.5f, cy - 26);
        cx = px + pw / 2; cy -= 45 + rnd() * 15; side = -side;
    }
    float lx = cx - W * 0.17f; if (lx < W * 0.06f) lx = W * 0.06f; if (lx > W * 0.94f - W * 0.34f) lx = W * 0.94f - W * 0.34f;
    Plat *q = addPlat(lx, cy, W * 0.34f); if (q) q->ground = 1; lastPlatX = lx + W * 0.17f;
    if (jMode == M_JUMP) goUp(cy); else goRight(lx + W * 0.34f, cy, 1.0f + rnd(), lx + W * 0.4f);
}
static void gauntlet(float gx, float gy) {
    int n = 4 + rand() % 3; float x = gx, y = gy;
    for (int k = 0; k < n; k++) {
        float pw = W * 0.2f + rnd() * W * 0.1f; Plat *q = addPlat(x, y, pw); if (q) q->ground = 1;
        float ox = x + pw * (0.38f + rnd() * 0.3f); addObs(ox, y, 20, 30);
        if (rnd() < 0.5f && nOb) { float span = pw * 0.35f < W * 0.2f ? pw * 0.35f : W * 0.2f; patrol(&ob[nOb - 1], x + 10, x + pw - 10, span, 0.6f); }
        if (rnd() < 0.5f) addCoin(x + pw * 0.5f, y - H * (0.13f + rnd() * 0.08f));
        float g = W * (0.09f + rnd() * 0.08f); x += pw + (g < GAPMAX ? g : GAPMAX);
        float dy = (rnd() - 0.5f) * 34; if (dy < -RISEMAX * 0.6f) dy = -RISEMAX * 0.6f; y += dy;
    }
    Plat *q = addPlat(x, y, W * 0.4f); if (q) q->ground = 1;
    goRight(x + W * 0.4f, y, 0.9f + rnd() * 0.9f, x + W * 0.3f);
}
static void bounceChain(float chx, float gy0) {
    int n = 5 + rand() % 3; float sx = chx + W * 0.2f, sy = gy0 - 30, gap = W * 0.36f, firstY = sy;
    for (int i = 0; i < n; i++) { Plat *q = addPlat(sx - 30, sy, 60); if (q) { q->chair = 1; q->chain = 1; } if (rnd() < 0.6f) addCoin(sx, sy - 40); sx += gap; sy -= 16; }
    float floorY = firstY + H * 0.34f;
    Plat *q = addPlat(chx - W * 0.18f, floorY, (sx - chx) + W * 0.95f); if (q) q->ground = 1;
    goRight(chx + (sx - chx) + W * 0.35f, floorY, 0.9f + rnd() * 0.8f, 0); noMarkerUntilX = genX + W * 0.5f; lastPlatX = genX - W * 0.2f;
}
static void clearBox(float x0, float x1, float y0, float y1) {     // remove earlier pieces from a region
    int k = 0; for (int i = 0; i < nPl; i++) { Plat *q = &pl[i]; if (q->x + q->w > x0 && q->x < x1 && q->y > y0 && q->y < y1) continue; pl[k++] = *q; } nPl = k;
    k = 0; for (int i = 0; i < nOb; i++) { if (ob[i].x > x0 && ob[i].x < x1 && ob[i].y > y0 && ob[i].y < y1) continue; ob[k++] = ob[i]; } nOb = k;
    k = 0; for (int i = 0; i < nCo; i++) { if (co[i].x > x0 && co[i].x < x1 && co[i].y > y0 && co[i].y < y1) continue; co[k++] = co[i]; } nCo = k;
}
static void flappyRun(float chx, float gy0) {
    {   // clear the whole flappy corridor first (keep the run-up to the chair), so nothing else is in the way
        float bandY = gy0 - H * 0.34f, x0 = chx + W * 0.05f, x1 = chx + W * 0.85f + 8 * W * 0.52f + W * 0.5f;
        clearBox(x0, x1, bandY - H * 0.9f, bandY + H * 0.9f);
        clearBox(chx - W * 0.3f, chx + W * 0.3f, gy0 - H * 0.9f, gy0 - 10);     // and the air right above the chair
    }
    Plat *q = addPlat(chx - W * 0.24f, gy0, W * 0.24f); if (q) q->ground = 1;
    q = addPlat(chx - 4, gy0 + 6, W * 0.26f > 40 ? W * 0.26f : 40); if (q) { q->chair = 1; q->flappy = 1; }
    float bandY = gy0 - H * 0.34f; flappyFloorY = bandY + H * 0.62f; flappyCeilY = bandY - H * 0.66f;
    int n = 5 + rand() % 3; float gap = W * 0.52f, gapH = H * 0.47f, pw = W * 0.26f > 20 ? W * 0.26f : 20, x = chx + W * 0.85f, cy = bandY, lastX = x;
    for (int i = 0; i < n && nPi < NPI; i++) {
        cy += (rnd() - 0.5f) * H * 0.44f; if (cy < bandY - H * 0.3f) cy = bandY - H * 0.3f; if (cy > bandY + H * 0.3f) cy = bandY + H * 0.3f;
        pi[nPi++] = (Pipe){ x, pw, cy, gapH, 0, (u8)(i % 3) }; addCoin(x + pw * 0.5f, cy); lastX = x; x += gap;
    }
    flappyEndX = lastX + pw + W * 0.12f;
    float landX = lastX + gap * 0.9f, landY = cy + H * 0.2f;
    q = addPlat(landX - W * 0.35f, landY, W * 0.75f); if (q) q->ground = 1;
    goRight(landX + W * 0.4f, landY, 0.9f + rnd() * 0.8f, 0); lastPlatX = landX; noMarkerUntilX = genX + W * 0.8f;
}
static void genStep(void) {
    if (genMode == 2) {                                 // stairs
        float stepW = W * 0.15f + 8;
        Plat *q = addPlat(genX, genY, stepW + 6); if (q) q->ground = 1;
        if (progress > 14 && stairLeft > 1 && stairLeft % 3 == 0) addObs(genX + stepW * 0.5f, genY, 18, 26);
        if (rnd() < 0.35f) addCoin(genX + stepW * 0.5f, genY - 26);
        genX += stepW; genY += stairDir * 30;
        if (stairDir < 0 && genY < camY - H * 1.1f) stairDir = 1;
        if (stairDir > 0 && genY > camY + H * 0.9f) stairDir = -1;
        if (--stairLeft <= 0) { q = addPlat(genX - W * 0.05f, genY, W * 0.5f); if (q) q->ground = 1; goRight(genX + W * 0.42f, genY, 0.9f + rnd() * 0.9f, genX + W * 0.92f); }
    } else if (genMode == 0) {                          // climbing
        float span = W * 0.34f < GAPMAX * 1.35f ? W * 0.34f : GAPMAX * 1.35f, px = lastPlatX + (rnd() - 0.5f) * span;
        if (px < lastPlatX - GAPMAX * 0.72f) px = lastPlatX - GAPMAX * 0.72f; if (px > lastPlatX + GAPMAX * 0.72f) px = lastPlatX + GAPMAX * 0.72f;
        float pw = W * 0.16f + rnd() * W * 0.09f;
        Plat *q = addPlat(px - pw / 2, genY, pw);
        if (q && progress > 18 && rnd() < 0.28f) { q->moving = 1; q->baseX = q->x; q->amp = W * (0.08f + rnd() * 0.1f); q->spd = 0.02f + rnd() * 0.02f; q->phase = rnd() * 6.28f; }
        lastPlatX = px;
        int atBottom = segLeft >= upStart - 1, atTop = segLeft <= 2;
        if (progress > 26 && pw > W * 0.2f && (atBottom || atTop) && rnd() < 0.4f) addObs(px, genY, 18, 26);
        if (rnd() < 0.5f) addCoin(px, genY - 26);
        float r = 48 + rnd() * 22; genY -= r < RISEMAX ? r : RISEMAX;
        if (--segLeft <= 0) {
            if (jMode == M_JUMP) {
                if (progress > 22 && rnd() < 0.33f) zigzag(lastPlatX, genY);
                else { genY += 22; q = addPlat(lastPlatX - W * 0.16f, genY, W * 0.34f); if (q) q->ground = 1; goUp(genY); }
            } else { genY += 26; q = addPlat(lastPlatX - W * 0.12f, genY, W * 0.42f); if (q) q->ground = 1; genMode = 1; genX = lastPlatX - W * 0.12f + W * 0.42f; segLeft = W * (1.3f + rnd() * 1.1f); }
        }
    } else {                                            // running right
        float slabW = W * (0.5f + rnd() * 0.5f);
        Plat *q = addPlat(genX, genY, slabW); if (q) q->ground = 1;
        float diff = 0.4f + progress * 0.006f; if (diff > 0.85f) diff = 0.85f;
        if (progress > 10 && genX > noMarkerUntilX && rnd() < diff) {
            int nObs = 1 + (progress > 50 && rnd() < 0.28f ? 1 : 0);
            for (int oi = 0; oi < nObs; oi++) {
                float ox = genX + slabW * (0.30f + rnd() * 0.42f) + oi * (48 > slabW * 0.30f ? 48 : slabW * 0.30f);
                if (ox < genX + slabW - 16) { addObs(ox, genY, 20, 30);
                    if (progress > 20 && rnd() < 0.4f && nOb) { float span = slabW * 0.4f < W * 0.28f ? slabW * 0.4f : W * 0.28f; patrol(&ob[nOb - 1], genX + 10, genX + slabW - 10, span, 0.6f); } }
            }
        }
        if (rnd() < 0.55f) addCoin(genX + slabW * 0.5f, genY - H * (0.14f + rnd() * 0.1f));
        if (jMode == M_COMBO && cycles >= 2 && cycles - lastLaunch >= 3 && !mega.on && slabW > W * 0.7f && rnd() < 0.7f) {
            // a chair launch to the MEGA, with its stepping stones (the website's layout)
            float chx = genX + slabW * 0.5f, gy0 = genY;
            q = addPlat(chx - 30, gy0 - 20, 60); if (q) q->chair = 1;
            float mx = chx + 326 + rnd() * 26, my = gy0 - (237 + rnd() * 7);
            mega.x = mx; mega.y = my; mega.w = 42; mega.h = 30; mega.hit = 0; mega.on = 1;
            float st[7][3] = { { mx - 72, my + 48, 150 }, { chx + 90, gy0 - 118, W * 0.22f }, { chx + 205, gy0 - 186, W * 0.22f }, { mx + 50, my + 96, W * 0.26f },
                               { chx + 150, gy0 - 66, W * 0.24f }, { mx + 96, my - 44, W * 0.2f }, { mx + 178, my - 112, W * 0.2f } };
            for (int k = 0; k < 7; k++) { q = addPlat(st[k][0], st[k][1], st[k][2]); if (q) { q->ground = 1; q->mz = 1; } }
            lastLaunch = cycles; noMarkerUntilX = mx + W * 0.6f;
            int k = 0; for (int i = 0; i < nOb; i++) if (!(ob[i].x > chx - W * 0.35f && ob[i].x < noMarkerUntilX)) ob[k++] = ob[i]; nOb = k;
        }
        genX += slabW; segLeft -= slabW;
        if (segLeft > W * 0.5f && rnd() < 0.4f) { float g = W * (0.1f + rnd() * 0.12f); genX += g < GAPMAX ? g : GAPMAX; }
        if (segLeft <= 0) {
            cycles++;
            if (jMode == M_RUN) {
                if (progress > 18 && rnd() < 0.3f) gauntlet(genX, genY);
                else { genY += (rnd() - 0.5f) * 44; genMode = 1; lastPlatX = genX; segLeft = W * (1.1f + rnd()); }
            } else {
                float roll = rnd();
                if ((progress > 14 && cycles - lastFlap >= 3) || (progress > 26 && roll < 0.12f)) { flappyRun(genX, genY); lastFlap = cycles; }   // COMBO: a flappy run at least every third section
                else if (progress > 22 && roll < 0.28f && cycles - lastLaunch >= 2) bounceChain(genX, genY);
                else if (progress > 20 && roll < 0.42f) zigzag(genX, genY);
                else if (progress > 18 && roll < 0.56f) gauntlet(genX, genY);
                else if (progress > 16 && roll < 0.72f) { genMode = 2; stairDir = rnd() < 0.6f ? -1 : 1; stairLeft = 5 + rand() % 4; lastPlatX = genX; }
                else { genMode = 0; lastPlatX = genX; genY -= 48 + rnd() * 18; segLeft = 6 + rand() % 4; upStart = (int)segLeft; }
            }
        }
    }
}
static void flapSpawnAhead(void) {
    float gap = W * 0.5f, gapH = H * 0.48f, pw = W * 0.26f > 20 ? W * 0.26f : 20;
    int guard = 0;
    while (flapLastX < camX + W * 2.2f && guard++ < 40 && nPi < NPI) {
        flapBandY += (rnd() - 0.5f) * H * 0.42f;
        if (flapBandY < p.y - H * 0.55f) flapBandY = p.y - H * 0.55f; if (flapBandY > p.y + H * 0.55f) flapBandY = p.y + H * 0.55f;
        flapLastX += gap;
        pi[nPi++] = (Pipe){ flapLastX, pw, flapBandY, gapH, 0, (u8)(flapPipeN++ % 3) }; addCoin(flapLastX + pw * 0.5f, flapBandY);
    }
}
static void cull(void) {                                // forget what's far behind or below
    float ez = 1 / (zoom < 0.4f ? 0.4f : zoom), pm = ((flight || flappy || flappyFreeze) ? H * 3.6f : H * 1.7f) * ez, bx = W * 0.7f * ez;
    int k = 0; for (int i = 0; i < nPl; i++) if (pl[i].y < camY + pm && pl[i].x + pl[i].w > camX - bx) pl[k++] = pl[i]; nPl = k;
    k = 0; for (int i = 0; i < nOb; i++) if (ob[i].y < camY + pm && ob[i].x > camX - bx) ob[k++] = ob[i]; nOb = k;
    k = 0; for (int i = 0; i < nCo; i++) if (!co[i].got && co[i].y < camY + pm && co[i].x > camX - bx) co[k++] = co[i]; nCo = k;
    k = 0; for (int i = 0; i < nPi; i++) if (pi[i].x + pi[i].w > camX - bx) pi[k++] = pi[i]; nPi = k;
}

// ══ COMBO: the course as a row of separate ZONES ═════════════════════════════
// Each zone owns its own stretch of the world (left to right) and builds everything in it at
// once, so zones never overlap, a flappy corridor is always clear, and the next zone is ready
// well before you reach it. Zones: RUN, GAUNTLET, STAIRS, CLIMB (a tower in its own column),
// BOUNCE (chair chain), FLAP (chair, pipes, landing) and MEGA (chair launch to the Mega pad).
enum { Z_RUN, Z_GAUNT, Z_STAIRS, Z_CLIMB, Z_BOUNCE, Z_FLAP, Z_MEGA, Z_N };
static float zX, zY; static int zCount, zLast = -1, zSinceFlap, zSinceMega;
static Plat *pad(float x, float y, float w) { Plat *q = addPlat(x, y, w); if (q) q->ground = 1; return q; }
static float stepGap(void) { float g = W * (0.07f + rnd() * 0.08f); return g < GAPMAX * 0.8f ? g : GAPMAX * 0.8f; }
static void zRun(void) {
    float end = zX + W * (1.4f + rnd() * 0.8f), x = zX;
    while (x < end) {
        float w = W * (0.45f + rnd() * 0.4f); pad(x, zY, w);
        if (zCount > 1 && rnd() < 0.55f) { addObs(x + w * (0.35f + rnd() * 0.3f), zY, 20, 30);
            if (progress > 20 && rnd() < 0.4f) patrol(&ob[nOb - 1], x + 10, x + w - 10, w * 0.35f, 0.6f); }
        if (rnd() < 0.55f) addCoin(x + w * 0.5f, zY - H * (0.13f + rnd() * 0.08f));
        x += w + (rnd() < 0.5f ? stepGap() : 0);
    }
    zX = x;
}
static void zGauntlet(void) {
    int n = 4 + rand() % 3; float x = zX, y = zY;
    for (int k = 0; k < n; k++) {
        float w = W * (0.22f + rnd() * 0.1f); pad(x, y, w);
        addObs(x + w * (0.4f + rnd() * 0.25f), y, 20, 30);
        if (rnd() < 0.5f) patrol(&ob[nOb - 1], x + 10, x + w - 10, w * 0.35f, 0.6f);
        if (rnd() < 0.5f) addCoin(x + w * 0.5f, y - H * 0.16f);
        x += w + stepGap(); y += (rnd() - 0.5f) * 30;
    }
    pad(x, y, W * 0.4f); zX = x + W * 0.4f; zY = y;
}
static void zStairs(void) {
    int n = 5 + rand() % 4; float dir = rnd() < 0.6f ? -1 : 1, x = zX, y = zY, sw = W * 0.15f + 8;
    for (int k = 0; k < n; k++) { pad(x, y, sw + 6); if (k > 1 && k < n - 1 && k % 3 == 0) addObs(x + sw * 0.5f, y, 18, 26); if (rnd() < 0.35f) addCoin(x + sw * 0.5f, y - 26); x += sw; y += dir * 30; }
    pad(x, y, W * 0.45f); zX = x + W * 0.45f; zY = y;
}
static void zClimb(void) {
    // a tower in its own column: up 6-9 platforms, zigzagging left and right inside the column,
    // then an exit pad on the column's right edge at the top
    float colL = zX + W * 0.05f, colW = W * 0.8f, y = zY, px = colL + colW * 0.2f;
    pad(zX, zY, W * 0.3f);
    int n = 6 + rand() % 4;
    for (int k = 0; k < n; k++) {
        float r = 48 + rnd() * 16; y -= r < RISEMAX ? r : RISEMAX;
        float w = W * (0.17f + rnd() * 0.07f);
        px += (rnd() < 0.5f ? -1 : 1) * (W * (0.12f + rnd() * 0.12f)); if (px < colL) px = colL + W * 0.06f; if (px > colL + colW - w) px = colL + colW - w - W * 0.06f;
        Plat *q = addPlat(px, y, w);
        if (q && progress > 18 && k > 1 && rnd() < 0.25f) { q->moving = 1; q->baseX = q->x; q->amp = W * 0.07f; q->spd = 0.02f + rnd() * 0.02f; q->phase = rnd() * 6.28f; }
        if (rnd() < 0.5f) addCoin(px + w * 0.5f, y - 26);
    }
    y -= 46; pad(colL + colW - W * 0.32f, y, W * 0.32f); zX = colL + colW; zY = y;
}
static void zBounce(void) {
    pad(zX, zY, W * 0.4f);
    int n = 5 + rand() % 3; float sx = zX + W * 0.6f, sy = zY - 30, gap = W * 0.36f;
    for (int i = 0; i < n; i++) { Plat *q = addPlat(sx - 30, sy, 60); if (q) { q->chair = 1; q->chain = 1; } if (rnd() < 0.6f) addCoin(sx, sy - 40); sx += gap; sy -= 16; }
    float floorY = zY + H * 0.3f; pad(zX + W * 0.4f, floorY, sx - zX);         // a floor under the chain to catch misses
    float exitY = sy + 40; pad(sx, exitY, W * 0.5f); zX = sx + W * 0.5f; zY = exitY;
}
static void zFlap(void) {
    // the run-up and the big chair, then the pipe corridor (nothing else is ever built in it),
    // then a wide landing pad
    pad(zX, zY, W * 0.4f);
    float chx = zX + W * 0.4f; Plat *q = addPlat(chx, zY + 6, W * 0.26f); if (q) { q->chair = 1; q->flappy = 1; }
    float bandY = zY - H * 0.34f; flappyFloorY = bandY + H * 0.62f; flappyCeilY = bandY - H * 0.66f;
    int n = 3 + rand() % 3; float gap = W * 0.48f, gapH = H * 0.47f, pw = W * 0.26f, x = chx + W * 0.8f, cy = bandY, lastX = x;
    for (int i = 0; i < n && nPi < NPI; i++) {
        cy += (rnd() - 0.5f) * H * 0.4f; if (cy < bandY - H * 0.28f) cy = bandY - H * 0.28f; if (cy > bandY + H * 0.28f) cy = bandY + H * 0.28f;
        pi[nPi++] = (Pipe){ x, pw, cy, gapH, 0, (u8)(i % 3) }; addCoin(x + pw * 0.5f, cy); lastX = x; x += gap;
    }
    flappyEndX = lastX + pw + W * 0.12f;
    float landX = lastX + gap * 0.9f, landY = cy + H * 0.2f;
    pad(landX - W * 0.35f, landY, W * 0.9f); zX = landX + W * 0.55f; zY = landY;
}
static void zMega(void) {
    float chx = zX + W * 0.3f, gy0 = zY;
    pad(zX, zY, W * 0.6f);
    Plat *q = addPlat(chx - 30, gy0 - 20, 60); if (q) q->chair = 1;
    float mx = chx + 326 + rnd() * 26, my = gy0 - (237 + rnd() * 7);
    mega.x = mx; mega.y = my; mega.w = 42; mega.h = 30; mega.hit = 0; mega.on = 1;
    float st[7][3] = { { mx - 72, my + 48, 150 }, { chx + 90, gy0 - 118, W * 0.22f }, { chx + 205, gy0 - 186, W * 0.22f }, { mx + 50, my + 96, W * 0.26f },
                       { chx + 150, gy0 - 66, W * 0.24f }, { mx + 96, my - 44, W * 0.2f }, { mx + 178, my - 112, W * 0.2f } };
    for (int k = 0; k < 7; k++) { q = addPlat(st[k][0], st[k][1], st[k][2]); if (q) { q->ground = 1; q->mz = 1; } }
    pad(chx + 30, gy0, (mx + W * 0.4f) - (chx + 30));                         // ground all along under it
    zX = mx + W * 0.4f; zY = gy0;
}
static void comboZone(void) {
    int z;
    if (zCount == 0) z = Z_RUN;
    else if (zCount > 2 && zSinceFlap >= 3) z = Z_FLAP;                        // a flappy zone at least every 4th zone
    else if (zCount > 3 && zSinceMega >= 5 && !mega.on) z = Z_MEGA;
    else { static const u8 POOL[] = { Z_RUN, Z_GAUNT, Z_STAIRS, Z_CLIMB, Z_CLIMB, Z_BOUNCE, Z_RUN };
           do z = POOL[rand() % sizeof POOL]; while (z == zLast); }
    float gap = zCount ? stepGap() * 0.6f : 0; zX += gap;                       // a short, always-jumpable gap between zones
    switch (z) { case Z_RUN: zRun(); break; case Z_GAUNT: zGauntlet(); break; case Z_STAIRS: zStairs(); break; case Z_CLIMB: zClimb(); break;
                 case Z_BOUNCE: zBounce(); break; case Z_FLAP: zFlap(); break; default: zMega(); break; }
    zSinceFlap = z == Z_FLAP ? 0 : zSinceFlap + 1; zSinceMega = z == Z_MEGA ? 0 : zSinceMega + 1;
    zLast = z; zCount++; cycles++;
}
static void genPath(void) {
    if (flapEndless) { flapSpawnAhead(); if ((frame & 7) == 0) cull(); return; }
    float za = 1 / (zoom < 0.4f ? 0.4f : zoom);
    if (jMode == M_COMBO) {                           // COMBO: build whole zones ahead of you
        // inside a flap zone the corridor and its landing are already built: build and tidy
        // nothing until you land (that work, every frame at the zoomed-out view, was the lag)
        if (flappy || flappyFreeze) { if ((frame & 31) == 0) cull(); return; }
        float zb = 1 / baseZoom();                      // look ahead by the normal view, not the zoomed-out one
        if (zX < camX + W * 2.7f * zb) { if (nPl >= NPL - 40) cull(); if (nPl < NPL - 40) comboZone(); }   // at most one zone a frame
        if ((frame & 7) == 0) cull();
        return;
    }
    for (int g = 0; g < 800; g++) {
        if (genMode == 0) { if (genY <= camY - H * 1.8f * za) break; } else if (genX >= camX + W * 2.7f * za) break;
        if (nPl >= NPL - 12) { cull(); if (nPl >= NPL - 12) break; }
        genStep();
    }
    if ((frame & 7) == 0) cull();
}
static void startRun(int m) {
    jMode = m; nPl = nOb = nCo = nPi = 0; score = 0; progress = 0; state = ST_PLAY; moveDir = 0; jumpQ = 0; frame = 0;
    startX = W * 0.5f; startY = H * 0.72f;
    memset(&p, 0, sizeof p); p.x = startX; p.y = startY - 15; p.w = 20; p.h = 30; p.onGround = 1; p.face = 1;
    bestX = startX; bestUp = startY - 15; lastGroundY = startY - 15;
    runnersInit(); airUsed = 0;
    mega.on = 0; flight = bouncing = flappy = flappyFreeze = flapEndless = 0; zoom = zoomT = baseZoom(); cycles = 0; lastLaunch = -99; lastFlap = 0;
    megaFlash = bounceCombo = bounceFlash = flappyPassed = flappyHint = 0; noMarkerUntilX = 0; upStart = 8; flapPipeN = 0;
    camX = p.x - W * 0.4f; camY = p.y - H * 0.55f;
    for (int d = 0; d < 44; d++) { deco[d][0] = rnd(); deco[d][1] = rnd(); deco[d][2] = 0.3f + rnd() * 0.7f; deco[d][3] = 0.6f + rnd() * 1.6f; }
    if (m == M_FLAP) {
        flapEndless = 1; flapBandY = startY - H * 0.34f; flapLastX = startX + W * 0.6f;
        p.y = flapBandY; camY = p.y - H * 0.5f; camX = p.x - W * 0.5f; flappyFloorY = camY + H + 40; flappyCeilY = camY - 40; flappyEndX = 1e9f;
        zoom = zoomT = ch()->cls == 1 ? 0.80f : 0.66f; flappyFreeze = 1;
    } else {
        Plat *q = addPlat(startX - W * 0.28f, startY, W * 0.56f); q->ground = 1;
        if (m == M_COMBO) { zX = startX + W * 0.28f; zY = startY; zCount = 0; zLast = -1; zSinceFlap = 0; zSinceMega = 0; }
        if (m == M_RUN) goRight(startX + W * 0.28f, startY, 1.1f + rnd(), startX + W * 0.7f);
        else { genMode = 0; genX = startX; lastPlatX = startX; genY = startY - (48 + rnd() * 18); segLeft = 6 + rand() % 4; }
    }
    flockReset(); genPath(); screen = S_JUMP;
}
static void gameOver(void) {
    state = ST_OVER; flappy = 0;
    newBest = score > (int)JBEST(jMode); if (newBest) JBEST(jMode) = score;
    if (score > sv.arcadeBest[ARC_JUMP]) sv.arcadeBest[ARC_JUMP] = score;
    sv.arcadePlays[ARC_JUMP]++;
    int before = sv.coins; if (score > 0) addCoins(score / 25); coinsWon += sv.coins - before;
    saveWrite(); screen = S_JUMP_OVER;
}
static void hop(void) { playAdpcm(js_hop, JS_HOP_LEN, 12000, 70); }
static void resumeRight(void) { if (jMode == M_COMBO) return; goRight(p.x + 20, p.y + p.h * 0.5f, 0.9f + rnd() * 0.8f, p.x + 20 + W * 0.7f); lastPlatX = p.x; }

// ── one step of the website's updatePlatformer ─────────────────────────────
static void step(void) {
    frame++;
    for (int i = 0; i < nPl; i++) if (pl[i].moving) { float nx = pl[i].baseX + fsin(frame * pl[i].spd + pl[i].phase) * pl[i].amp; pl[i].dx = nx - pl[i].x; pl[i].x = nx; }
    for (int i = 0; i < nOb; i++) { Obs *o = &ob[i]; if (!o->patrol) continue; o->x += o->vx;
        if (o->x <= o->minX) { o->x = o->minX; o->vx = fabsf_(o->vx); } else if (o->x >= o->maxX) { o->x = o->maxX; o->vx = -fabsf_(o->vx); } o->roll += o->vx * 0.12f; }
    if (megaFlash > 0) megaFlash--;
    if (bounceFlash > 0) bounceFlash--;
    if (flappyHint > 0) flappyHint--;
    zoom += (zoomT - zoom) * 0.1f;
    if (flappyFreeze) {
        p.vx = p.vy = 0;
        if (jumpQ) { flappyFreeze = 0; flappy = 1; flappyHint = 150; p.vy = FLAPV; p.vx = FLAPSPD; hop(); }
    } else if (flappy) {
        p.vx = FLAPSPD; p.face = 1;
        if (jumpQ && p.x < flappyEndX) { p.vy = FLAPV; hop(); }
        p.vy += 0.62f * (H / 494.0f); if (p.vy > 12.5f * (H / 494.0f)) p.vy = 12.5f * (H / 494.0f);
    } else if (flight) {
        p.vx += moveDir * 0.5f; if (!moveDir) p.vx *= 0.98f; if (p.vx > 9) p.vx = 9; if (p.vx < -9) p.vx = -9;
        if (moveDir) p.face = moveDir;
        p.vy += 0.5f; if (p.vy > 13) p.vy = 13;
    } else if (bouncing) {
        p.vx += moveDir * 0.4f; if (!moveDir) p.vx += (4.2f - p.vx) * 0.05f; if (p.vx > 9) p.vx = 9; if (p.vx < -6) p.vx = -6;
        if (moveDir) p.face = moveDir;
        p.vy += 0.58f; if (p.vy > 15) p.vy = 15;
    } else {
        Runner *r = ch();
        p.vx = moveDir * 3.4f * r->speed; if (moveDir) p.face = moveDir;
        if (p.onGround) airUsed = 0;
        if (jumpQ) {
            if (p.onGround) { p.vy = JUMPV * r->jump; p.onGround = 0; hop(); }
            else if (r->airJumps > airUsed) { airUsed++; p.vy = JUMPV * r->jump * 0.94f; hop(); }   // chickens: one more in the air
        }
        p.vy += 0.58f * r->grav; if (p.vy > 15 * r->grav) p.vy = 15 * r->grav;
    }
    jumpQ = 0;
    float prevBottom = p.y + p.h * 0.5f;
    p.x += p.vx; p.y += p.vy;
    if (!flight && !bouncing && !flappy && !flappyFreeze && p.x < camX + 8) p.x = camX + 8;
    p.onGround = 0;
    flockStep();
    if (p.vy >= 0) { float nb = p.y + p.h * 0.5f;
        for (int i = 0; i < nPl; i++) { Plat *q = &pl[i];
            if (!(p.x + 6 > q->x && p.x - 6 < q->x + q->w && nb >= q->y && prevBottom <= q->y + 9)) continue;
            if (q->chain) { p.y = q->y - p.h * 0.5f - 1; p.vy = -11; if (p.vx < 4) p.vx = 4; bouncing = 1; bounceCombo++; bounceFlash = 32; addCoins(3); coinsWon += 3; hop(); break; }
            if (q->flappy && !flappy && !flappyFreeze) { p.y = q->y - p.h * 0.5f - 60; p.vx = p.vy = 0; flappyFreeze = 1; flappyPassed = 0; zoomT = ch()->cls == 1 ? 0.80f : 0.66f; break; }
            if (q->chair && !flight && !flappy && !flappyFreeze) { p.y = q->y - p.h * 0.5f - 1; p.vy = -16; p.vx = 8; flight = 1; zoomT = 0.62f; hop(); break; }
            p.y = q->y - p.h * 0.5f; p.vy = 0; p.onGround = 1; if (q->moving) p.x += q->dx;
            if (flappy || flappyFreeze) { flappy = flappyFreeze = 0; zoomT = baseZoom(); nPi = 0; resumeRight(); }
            if (flight) { flight = 0; zoomT = baseZoom(); mega.on = 0;
                int k = 0; for (int j = 0; j < nPl; j++) if (!(pl[j].mz && (pl[j].x > p.x + 30 || pl[j].y < p.y - 30))) pl[k++] = pl[j]; nPl = k;
                resumeRight(); }
            if (bouncing) { bouncing = 0; if (bounceCombo >= 2) bounceFlash = 50; bounceCombo = 0; resumeRight(); }
            break;
        } }
    if (p.onGround) lastGroundY = p.y;
    if (flight || flappy || flappyFreeze) { camX += ((p.x - W * 0.5f) - camX) * 0.12f; camY += ((p.y - H * 0.5f) - camY) * 0.12f; }
    else {
        float topB = camY + H * 0.36f, botB = camY + H * 0.60f, leftB = camX + W * 0.30f, rightB = camX + W * 0.48f;
        if (p.y < topB) camY += (p.y - topB) * 0.18f; else if (p.y > botB) camY += (p.y - botB) * 0.18f;
        if (p.x < leftB) camX += (p.x - leftB) * 0.18f; else if (p.x > rightB) camX += (p.x - rightB) * 0.18f;
    }
    if (p.x > bestX) bestX = p.x;
    if (p.y < bestUp) bestUp = p.y;
    progress = ((startY - bestUp) + (bestX - startX)) / 13;
    score = flapEndless ? flappyPassed : (progress > 0 ? (int)progress : 0);
    genPath();
    for (int i = 0; i < nCo; i++) { Coin *c = &co[i]; if (!c->got && fabsf_(p.x - c->x) < 16 && fabsf_(p.y - c->y) < 16) { c->got = 1; addCoins(1); coinsWon++; } }
    for (int i = 0; i < nOb; i++) { Obs *o = &ob[i];
        if (p.x + 7 > o->x - o->w * 0.5f && p.x - 7 < o->x + o->w * 0.5f && p.y + p.h * 0.5f > o->y - o->h && p.y - p.h * 0.5f < o->y - 2) { gameOver(); return; } }
    if (mega.on && !mega.hit && fabsf_(p.x - mega.x) < mega.w * 0.5f + 5 && fabsf_(p.y - mega.y) < mega.h * 0.5f + 5) {
        mega.hit = 1; megaFlash = 75; sfxPlop();
        for (int k = 0; k < 14; k++) addCoin(mega.x + (rnd() - 0.5f) * W * 0.3f, mega.y + (rnd() - 0.5f) * H * 0.2f);
    }
    if (mega.on && (mega.x < camX - W * 0.8f || mega.y > camY + H * 1.9f)) mega.on = 0;
    if (flappy) {
        if (flapEndless) { float vh = (H * 0.5f) / zoom; flappyCeilY = camY + H * 0.5f - vh + 18; flappyFloorY = camY + H * 0.5f + vh - 18; }
        for (int i = 0; i < nPi; i++) { Pipe *q = &pi[i]; float hg = q->gapH * 0.5f;
            if (p.x + 6 > q->x && p.x - 6 < q->x + q->w && (p.y - p.h * 0.35f < q->gapY - hg || p.y + p.h * 0.35f > q->gapY + hg)) { zoomT = baseZoom(); gameOver(); return; }
            if (!q->passed && p.x > q->x + q->w) { q->passed = 1; flappyPassed++; addCoins(2); coinsWon += 2; } }
        { float vh = (H * 0.5f) / zoom, top = camY + H * 0.5f - vh + 10, bot = camY + H * 0.5f + vh;
          if (!flapEndless) { if (flappyCeilY < top) flappyCeilY = top; if (flappyFloorY > bot) flappyFloorY = bot; }
          if (p.y < flappyCeilY + p.h * 0.5f) { p.y = flappyCeilY + p.h * 0.5f; if (p.vy < 0) p.vy = 0; }   // bump the top, don't die
          if (p.y > flappyFloorY) { zoomT = baseZoom(); gameOver(); return; } }
    }
    if (state == ST_PLAY && jMode != M_COMBO && !flappy && !flappyFreeze && !flight && !bouncing && p.onGround) {   // never a dead end ahead (COMBO's zones are always joined up)
        float pb = p.y + p.h * 0.5f; int ahead = 0;
        for (int i = 0; i < nPl; i++) { Plat *q = &pl[i]; if (q->x + q->w > p.x + p.w && q->x < p.x + W * 1.05f && q->y - pb > -H * 0.5f && q->y - pb < H * 1.1f) { ahead = 1; break; } }
        if (!ahead) { Plat *q = addPlat(p.x + W * 0.6f, pb, W * 0.5f); if (q) q->ground = 1;
            if (genMode != 0) { goRight(p.x + W * 1.1f, pb, 0.9f + rnd() * 0.7f, p.x + W * 1.5f); lastPlatX = genX - W * 0.2f; } }
    }
    if (!flappy && !flappyFreeze) { float pit = (flight || bouncing) ? H * 1.7f : H * 1.0f; if (p.y - lastGroundY > pit) { flight = 0; zoomT = baseZoom(); gameOver(); } }
}
void updateJump(void) {
#ifdef SIMDEBUG
    if (jMode == M_COMBO && state == ST_PLAY && frame == 5) {      // dump the course: build 14 zones ahead and write them out
        for (int k = 0; k < 14; k++) comboZone();
        FILE *f = fopen("/tmp/course.txt", "w");
        for (int i = 0; i < nPl; i++) fprintf(f, "P %f %f %f %d %d\n", pl[i].x, pl[i].y, pl[i].w, pl[i].chair, pl[i].flappy);
        for (int i = 0; i < nOb; i++) fprintf(f, "O %f %f\n", ob[i].x, ob[i].y);
        for (int i = 0; i < nPi; i++) fprintf(f, "I %f %f %f %f\n", pi[i].x, pi[i].w, pi[i].gapY, pi[i].gapH);
        if (mega.on) fprintf(f, "M %f %f\n", mega.x, mega.y);
        fclose(f);
    }
#endif
    if (state != ST_PLAY) return;
    step();
    if (state == ST_PLAY && (++speedAcc & 1)) step();   // 3 steps every 2 frames: quicker than the website's 1.14x, to suit the DS
}

// ── input ─────────────────────────────────────────────────────────────────
void inputJump(void) {
    if (screen == S_JUMP_PAUSE) { if (kDown & KEY_START) screen = S_JUMP; if (kDown & KEY_SELECT) goScreen(S_ARCADE); return; }
    if (kDown & KEY_START) { screen = S_JUMP_PAUSE; return; }
    if (kDown & KEY_X) musicToggle();
    if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    int l = (kHeld & KEY_LEFT) || ((kHeld & KEY_TOUCH) && tX < 85), r = (kHeld & KEY_RIGHT) || ((kHeld & KEY_TOUCH) && tX > 171);
    moveDir = l && !r ? -1 : r && !l ? 1 : 0;
    if (kDown & (KEY_A | KEY_B | KEY_UP)) jumpQ = 1;
    if ((kDown & KEY_TOUCH) && tX >= 85 && tX <= 171) jumpQ = 1;
}

// ── drawing ───────────────────────────────────────────────────────────────
static float SXf(float x) { return (x - camX - W / 2) * zoom + W / 2; }
static float SYf(float y) { return (y - camY - H / 2) * zoom + H / 2; }
FAST static void sky(void) {
    int t = sv.theme;
    u16 top = t == 1 ? COL(27, 27, 25) : t == 4 ? COL(0, 1, 1) : t == 2 ? COL(4, 2, 9) : (t == 3 || t == 5) ? COL(14, 8, 2) : COL(3, 4, 11);
    u16 bot = t == 1 ? COL(22, 22, 20) : t == 4 ? COL(1, 4, 3) : t == 2 ? COL(9, 4, 16) : (t == 3 || t == 5) ? COL(26, 15, 4) : COL(9, 7, 20);
    for (int gy = 0; gy < 2 * SH; gy += 4) {
        int k = gy * 16 / (2 * SH); u16 c = COL((top & 31) + (((bot & 31) - (top & 31)) * k) / 16, ((top >> 5) & 31) + ((((bot >> 5) & 31) - ((top >> 5) & 31)) * k) / 16, ((top >> 10) & 31) + ((((bot >> 10) & 31) - ((top >> 10) & 31)) * k) / 16);
        for (int j = 0; j < 4; j++) { u16 *row = gy + j < SH ? &bufTop[(gy + j) * SW] : &bufBot[(gy + j - SH) * SW]; for (int x = 0; x < SW; x++) row[x] = c; }
    }
    for (int d = 0; d < 44; d++) {                      // the website's drifting deco dots (parallax by depth)
        float z = deco[d][2]; int x = (int)(((deco[d][0] * W * 2 - camX * z * 0.3f) - (int)((deco[d][0] * W * 2 - camX * z * 0.3f) / (W * 2)) * W * 2));
        if (x < 0) x += 2 * (int)W; int y = (int)(deco[d][1] * H * 2 - camY * z * 0.2f) % (int)(2 * H); if (y < 0) y += 2 * (int)H;
        u16 c = t == 1 ? COL(12, 12, 12) : t == 4 ? COL(8, 31, 15) : (t == 3 || t == 5) ? COL(31, 25, 10) : COL(26, 26, 31);
        gpx(x % SW, y % (2 * SH), c); if (deco[d][3] > 1.4f) gpx(x % SW + 1, y % (2 * SH), c);
    }
    if (t == 4) for (int gy = 0; gy < 2 * SH; gy += 24) for (int x = 0; x < SW; x++) gpx(x, gy, COL(1, 9, 5));
}
static const u16 *mkSpr(int c, int *w) { *w = c == 0 ? JM_0_W : c == 1 ? JM_1_W : JM_2_W; return c == 0 ? jm_0 : c == 1 ? jm_1 : jm_2; }
static void slab(Plat *q) {                             // a platform: a marker lying down, coloured by position (as the website)
    int c = ((int)fabsf_(q->x / 37 + q->y / 53)) % 3, w; const u16 *s = mkSpr(c, &w);
    float x0 = SXf(q->x), x1 = SXf(q->x + q->w), y = SYf(q->y) + 4 * zoom;
    if (x1 < -30 || x0 > SW + 30 || y < -20 || y > 2 * SH + 20) return;
    int len = (int)(x1 - x0), th = (int)(w * (x1 - x0) / JM_0_H + 0.5f); if (th < 2) th = 2;
    blitScaledR90(s, w, JM_0_H, (int)((x0 + x1) / 2), (int)y, len, th);
    if (sv.theme == 1 || sv.theme == 4) {              // CARTOON / NEON finish: crisp edge lines (cheap)
        u16 e = sv.theme == 1 ? BLACK : COL(8, 31, 15); int yy = (int)y - th / 2;
        grect((int)x0, yy - 1, len, 1, e); grect((int)x0, yy + th, len, 1, e); }
}
void drawJump(void) {
    char s[32];
    sky(); gClipLo = 0; gClipHi = 2 * SH;
    for (int i = 0; i < nPi; i++) {                     // flappy pipes: stacked markers above and below the gap
        Pipe *q = &pi[i]; int w; const u16 *sp = mkSpr(q->col, &w); float hg = q->gapH * 0.5f, x = SXf(q->x + q->w / 2), sc = q->w * zoom / w;
        if (x < -40 || x > SW + 40) continue;
        // pipes are upright, so they're drawn with the quick straight scaler (no rotation): each
        // segment costs only the pixels it covers. (Rotating them, with the theme's glow passes,
        // was most of FLAP's frame time.)
        int dw = (int)(w * sc + 0.5f), dh = (int)(JM_0_H * sc + 0.5f); if (dh < 1) dh = 1;
        for (int y = (int)SYf(q->gapY - hg) - dh / 2; y > -dh; y -= dh) blitScaled(sp, w, JM_0_H, (int)x, y, dw, dh);
        for (int y = (int)SYf(q->gapY + hg) + dh / 2; y < 2 * SH + dh; y += dh) blitScaled(sp, w, JM_0_H, (int)x, y, dw, dh);
        if (sv.theme == 1 || sv.theme == 4) {           // CARTOON / NEON: a crisp edge line instead of the per-pixel outline
            u16 e = sv.theme == 1 ? BLACK : COL(8, 31, 15); int x0 = (int)x - dw / 2 - 1, x1 = (int)x + dw / 2;
            grect(x0, 0, 1, (int)SYf(q->gapY - hg), e); grect(x1, 0, 1, (int)SYf(q->gapY - hg), e);
            grect(x0, (int)SYf(q->gapY + hg), 1, 2 * SH, e); grect(x1, (int)SYf(q->gapY + hg), 1, 2 * SH, e);
        }
        u16 g = q->col == 0 ? COL(31, 6, 8) : q->col == 1 ? COL(6, 31, 14) : COL(7, 15, 31);
        grect((int)(x - q->w * zoom / 2) - 3, (int)SYf(q->gapY - hg) - 6, (int)(q->w * zoom) + 6, 6, g);
        grect((int)(x - q->w * zoom / 2) - 3, (int)SYf(q->gapY + hg), (int)(q->w * zoom) + 6, 6, g);
    }
    for (int i = 0; i < nPl; i++) { Plat *q = &pl[i];
        if (q->chair) { float x = SXf(q->x + q->w / 2), y = SYf(q->y); float hh = (q->flappy ? 78 : 34) * zoom;   // the flappy launch chair: big, so you can't miss it
            blitRotScale(chair, CHAIR_W, CHAIR_H, (int)x, (int)(y - hh / 2), 0, hh / CHAIR_H); }
        else slab(q); }
    for (int i = 0; i < nCo; i++) if (!co[i].got) { int x = (int)SXf(co[i].x), y = (int)SYf(co[i].y); if (x > -10 && x < SW + 10 && y > -10 && y < 2 * SH + 10) { int d = (int)(COIN_W * zoom * 0.8f + 0.5f); blitScaled(coinT, COIN_W, COIN_H, x, y, d, d); } }
    for (int i = 0; i < nOb; i++) { Obs *o = &ob[i]; int w; const u16 *sp = mkSpr(o->col, &w); float hgt = o->h * 1.15f * zoom;
        int x = (int)SXf(o->x), y = (int)(SYf(o->y) - hgt / 2); if (x < -20 || x > SW + 20) continue;
        if (o->roll == 0 && sv.theme != 1 && sv.theme != 4) blitScaled(sp, w, JM_0_H, x, y, (int)(w * hgt / JM_0_H + 0.5f), (int)(hgt + 0.5f));   // standing still: no rotation needed
        else drawMarkerFx(sp, w, JM_0_H, x, y, o->roll, hgt / JM_0_H, o->col); }
    if (mega.on && !mega.hit) { int x = (int)SXf(mega.x), y = (int)SYf(mega.y); blitRotScale(slotSymT[1], SLOT_LOGO_W, SLOT_LOGO_H, x, y, 0, mega.w * zoom / SLOT_LOGO_W); }
    if (JCHAR == 6) for (int i = 0; i < nFlock; i++) {    // H3MMINGWAY's flock, retracing her trail
        Chick *c = &flock[i]; float age = runDist - c->born, x, y; if (!trailAt(c->gap, &x, &y)) continue;
        if (!c->init) { c->sx = x; c->sy = y; c->lx = x; c->sf = 1; c->init = 1; } else { c->sx += (x - c->sx) * 0.46f; c->sy += (y - c->sy) * 0.36f; }
        c->vs = c->vs * 0.82f + (c->sx - c->lx) * 0.18f; c->lx = c->sx; if (c->vs > 0.14f) c->sf = 1; else if (c->vs < -0.14f) c->sf = -1;
        const u16 *sp; int w, h, L = c->sf < 0, hatched = 0;
        if (age < c->t1) { sp = L ? jf_egg0l : jf_egg0r; w = JF_EGG0R_W; h = JF_EGG0R_H; }
        else if (age < c->t1 + c->t2) { sp = L ? jf_egg1l : jf_egg1r; w = JF_EGG1R_W; h = JF_EGG1R_H; }
        else if (age < c->t1 + c->t2 + c->t3) { const u16 *P[5][2] = { { jf_peek0r, jf_peek0l }, { jf_peek1r, jf_peek1l }, { jf_peek2r, jf_peek2l }, { jf_peek3r, jf_peek3l }, { jf_peek4r, jf_peek4l } };
            const int WW[5] = { JF_PEEK0R_W, JF_PEEK1R_W, JF_PEEK2R_W, JF_PEEK3R_W, JF_PEEK4R_W }; sp = P[c->col][L]; w = WW[c->col]; h = JF_PEEK0R_H; }
        else { const u16 *P[5][2] = { { jf_chick0r, jf_chick0l }, { jf_chick1r, jf_chick1l }, { jf_chick2r, jf_chick2l }, { jf_chick3r, jf_chick3l }, { jf_chick4r, jf_chick4l } };
            const int WW[5] = { JF_CHICK0R_W, JF_CHICK1R_W, JF_CHICK2R_W, JF_CHICK3R_W, JF_CHICK4R_W }; sp = P[c->col][L]; w = WW[c->col]; h = JF_CHICK0R_H; hatched = 1; }
        float bob = hatched ? fsin(frame * 0.34f + c->born * 0.05f) * 1.5f * (fabsf_(c->vs) * 0.7f > 1 ? 1 : fabsf_(c->vs) * 0.7f) : 0;
        blitScaled(sp, w, h, (int)SXf(c->sx), (int)(SYf(c->sy + p.h * 0.5f + bob) - h * zoom * 0.5f), (int)(w * zoom + 0.5f), (int)(h * zoom + 0.5f));
    }
    {   // the runner (the website's art for whoever you picked)
        Runner *r = ch(); int pose = !p.onGround ? 2 : moveDir ? 1 : 0, L = p.face < 0;
        const u16 *sp = r->s[pose][L]; int w = r->w[pose][L];
        float sc = zoom * 1.05f; int dw = (int)(w * sc + 0.5f), dh = (int)(r->h * sc + 0.5f);
        blitScaled(sp, w, r->h, (int)SXf(p.x), (int)(SYf(p.y + p.h * 0.5f) - dh / 2), dw, dh);   // upright: the quick scaler
    }
    sprintf(s, "%d", score); text(bufTop, 6, 4, s, WHITE, 2);
    sprintf(s, "%s  BEST %d", MODE_NAME[jMode], (int)JBEST(jMode)); text(bufTop, SW - 6 - textW(s, 1), 6, s, GOLD, 1);
    if (megaFlash > 0) textC(bufTop, 80, "MEGA!", COL(31, 26, 9), 2);
    if (bounceFlash > 0 && bounceCombo >= 2) { sprintf(s, "BOUNCE x%d", bounceCombo); textC(bufTop, 110, s, COL(15, 31, 19), 1); }
    if (flappyFreeze) textC(bufTop, 140, "TAP TO JUMP", WHITE, 2);
    else if (flappyHint > 0) textC(bufTop, 150, "tap to flap!", WHITE, 1);
    if (screen == S_JUMP_PAUSE) { textC(bufTop, 90, "PAUSED", YELLOW, 2); textC(bufTop, 126, "START resume - SELECT quit", WHITE, 1); }
}

// ── menu and results ──────────────────────────────────────────────────────
static Btn JB[9]; static int charSel;
static void jmLayout(void) {
    for (int i = 0; i < 4; i++) { JB[i] = (Btn){ 38, 4 + i * 36, 180, 31, "", 0, 0 }; strcpy(JB[i].label, MODE_NAME[i]); }
    JB[4] = (Btn){ 10, 152, 112, 32, "RUNNER", 0, 0 }; JB[5] = (Btn){ 134, 152, 112, 32, "BACK", 0, 0 };
}
static void runnerPic(u16 *buf, int k, int cx, int baseY, float sc, int pose) {
    Runner *r = &RUN[k]; blitRotScale(r->s[pose][0], r->w[pose][0], r->h, cx, baseY - (int)(r->h * sc / 2) + (buf == bufBot ? SH : 0), 0, sc);
}
void drawJumpMenu(void) {
    char s[40];
    runnersInit();
    fillScreen(bufTop, DARK);
    textC(bufTop, 6, "QU33PH JUMP", GOLD, 2);
    jmLayout(); int m = menuSel < 4 ? menuSel : 0;
    textC(bufTop, 42, MODE_NAME[m], WHITE, 2);
    textC(bufTop, 76, MODE_BLURB[m], GREY, 1);
    for (int i = 0; i < 4; i++) { sprintf(s, "%s  best %d", MODE_NAME[i], (int)JBEST(i)); textC(bufTop, 100 + i * 14, s, i == m ? YELLOW : GREY, 1); }
    gClipLo = 0; gClipHi = 2 * SH;
    runnerPic(bufTop, ch() - RUN, SW - 26, 180, 1.0f, (frame++ / 12) % 2 ? 1 : 0);   // your runner, ready
    coinCount(bufTop, 6, 176);
    fillScreen(bufBot, DARK); drawBtns(bufBot, JB, 6, menuSel);
}
void inputJumpMenu(void) {
    jmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(JB, 6, &menuSel, 1);
    if (h >= 0 && h < 4) { coinsWon = 0; startRun(h); }
    if (h == 4) { charSel = (int)JCHAR; goScreen(S_JUMP_CHARS); }
    if (h == 5) goScreen(S_ARCADE);
}
// pick your runner: just the pictures (four across, three below), as on the website
static Btn CB[8];
static void chLayout(void) {
    for (int k = 0; k < 7; k++) { int row = k < 4 ? 0 : 1, col = row ? k - 4 : k, n = row ? 3 : 4, x0 = (SW - n * 58 - (n - 1) * 4) / 2;
        CB[k] = (Btn){ x0 + col * 62, 4 + row * 74, 58, 70, "", 0, 0 }; }
    CB[7] = (Btn){ 68, 156, 120, 30, "OK", 0, 0 };
}
void drawJumpChars(void) {
    runnersInit(); chLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 6, "PICK YOUR RUNNER", GOLD, 2);
    gClipLo = 0; gClipHi = 2 * SH;
    int k = charSel < 7 ? charSel : (int)JCHAR;
    runnerPic(bufTop, k, SW / 2, 120, 2.0f, (frame++ / 10) % 2 ? 1 : 0);   // big, running on the spot
    if (k == 6) blitScaled(jf_egg0r, JF_EGG0R_W, JF_EGG0R_H, SW / 2 + 52, 160, JF_EGG0R_W * 2, JF_EGG0R_H * 2);
    fillScreen(bufBot, DARK);
    for (int i = 0; i < 7; i++) {
        int on = i == charSel, sel = i == (int)JCHAR;
        u16 fr = on ? (sv.theme == 1 ? COL(28, 4, 4) : GOLD) : sel ? uiIcon : 0;
        if (fr) { rect(bufBot, CB[i].x, CB[i].y, CB[i].w, 2, fr); rect(bufBot, CB[i].x, CB[i].y + CB[i].h - 2, CB[i].w, 2, fr); rect(bufBot, CB[i].x, CB[i].y, 2, CB[i].h, fr); rect(bufBot, CB[i].x + CB[i].w - 2, CB[i].y, 2, CB[i].h, fr); }
        runnerPic(bufBot, i, CB[i].x + CB[i].w / 2, CB[i].y + 36, 1.4f * (RUN[i].cls ? 1.0f : 0.85f), 0);
    }
    drawBtns(bufBot, &CB[7], 1, charSel == 7 ? 0 : -1);
}
void inputJumpChars(void) {
    chLayout();
    if (kDown & KEY_B) { goScreen(S_JUMP_MENU); return; }
    if (kDown & KEY_LEFT) charSel = (charSel + 7) % 8;
    if (kDown & KEY_RIGHT) charSel = (charSel + 1) % 8;
    if (kDown & KEY_UP) charSel = charSel >= 4 && charSel < 7 ? charSel - 4 : charSel;
    if (kDown & KEY_DOWN) charSel = charSel < 4 ? (charSel < 3 ? charSel + 4 : 7) : 7;
    int keep = kDown; kDown &= ~(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);
    int s = charSel, h = btnInput(CB, 8, &s, 1); kDown = keep; charSel = s;
    if (h >= 0 && h < 7) { JCHAR = h; saveWrite(); }
    if (h == 7) goScreen(S_JUMP_MENU);
}
void drawJumpOver(void) {
    char s[40];
    drawJump();
    box(bufTop, 28, 40, 200, 110, COL(2, 2, 4), GOLD);
    textC(bufTop, 48, "GAME OVER", COL(31, 7, 7), 2);
    sprintf(s, "%s  %d", MODE_NAME[jMode], score); textC(bufTop, 84, s, WHITE, 1);
    sprintf(s, newBest ? "NEW BEST!" : "BEST %d", (int)JBEST(jMode)); textC(bufTop, 102, s, newBest ? LIME : COL(15, 27, 21), 1);
    sprintf(s, "+%d COINS", coinsWon); textC(bufTop, 122, s, GOLD, 1);
    fillScreen(bufBot, DARK);
    JB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; JB[1] = (Btn){ 38, 98, 180, 36, "MODES", 0, 0 };
    drawBtns(bufBot, JB, 2, overSel);
}
void inputJumpOver(void) {
    JB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; JB[1] = (Btn){ 38, 98, 180, 36, "MODES", 0, 0 };
    int h = btnInput(JB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) { coinsWon = 0; startRun(jMode); }
    if (h == 1) goScreen(S_JUMP_MENU);
}
