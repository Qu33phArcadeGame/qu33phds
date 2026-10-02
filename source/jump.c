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
static int jMode, state, score, frame, coinsWon, newBest, menuSel, overSel, upStart, cycles, lastLaunch, megaFlash, bounceCombo, bounceFlash, flappyPassed, flappyHint;
static int flight, bouncing, flappy, flappyFreeze, flapEndless, stairDir, stairLeft, flapPipeN, moveDir, jumpQ, speedAcc;
static float camX, camY, zoom, zoomT, genX, genY, segLeft, lastPlatX, noMarkerUntilX, startX, startY, bestX, bestUp, lastGroundY, progress, flappyFloorY, flappyCeilY, flappyEndX, flapLastX, flapBandY;
static int genMode;                                    // 0 up, 1 right, 2 stairs
static struct { float x, y, w, h; int hit, on; } mega;
static float deco[44][4];
enum { ST_PLAY, ST_OVER };
#define JBEST(m) sv.spare[m]                           // each mode's best (spare save room, so old saves read 0)

int jumpEnter(void) { return pakUse(JUMP_PAK, JUMP_PAK_SIZE, JUMP_PAK_ID); }
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
static void flappyRun(float chx, float gy0) {
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
                if (progress > 26 && roll < 0.12f) flappyRun(genX, genY);
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
static void genPath(void) {
    if (flapEndless) { flapSpawnAhead(); if ((frame & 7) == 0) cull(); return; }
    float za = 1 / (zoom < 0.4f ? 0.4f : zoom);
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
    mega.on = 0; flight = bouncing = flappy = flappyFreeze = flapEndless = 0; zoom = zoomT = BASEZ; cycles = 0; lastLaunch = -99;
    megaFlash = bounceCombo = bounceFlash = flappyPassed = flappyHint = 0; noMarkerUntilX = 0; upStart = 8; flapPipeN = 0;
    camX = p.x - W * 0.4f; camY = p.y - H * 0.55f;
    for (int d = 0; d < 44; d++) { deco[d][0] = rnd(); deco[d][1] = rnd(); deco[d][2] = 0.3f + rnd() * 0.7f; deco[d][3] = 0.6f + rnd() * 1.6f; }
    if (m == M_FLAP) {
        flapEndless = 1; flapBandY = startY - H * 0.34f; flapLastX = startX + W * 0.6f;
        p.y = flapBandY; camY = p.y - H * 0.5f; camX = p.x - W * 0.5f; flappyFloorY = camY + H + 40; flappyCeilY = camY - 40; flappyEndX = 1e9f;
        zoom = zoomT = 0.66f; flappyFreeze = 1;
    } else {
        Plat *q = addPlat(startX - W * 0.28f, startY, W * 0.56f); q->ground = 1;
        if (m == M_RUN) goRight(startX + W * 0.28f, startY, 1.1f + rnd(), startX + W * 0.7f);
        else { genMode = 0; genX = startX; lastPlatX = startX; genY = startY - (48 + rnd() * 18); segLeft = 6 + rand() % 4; }
    }
    genPath(); screen = S_JUMP;
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
static void resumeRight(void) { goRight(p.x + 20, p.y + p.h * 0.5f, 0.9f + rnd() * 0.8f, p.x + 20 + W * 0.7f); lastPlatX = p.x; }

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
        p.vx = moveDir * 3.4f; if (moveDir) p.face = moveDir;
        if (jumpQ && p.onGround) { p.vy = JUMPV; p.onGround = 0; hop(); }
        p.vy += 0.58f; if (p.vy > 15) p.vy = 15;
    }
    jumpQ = 0;
    float prevBottom = p.y + p.h * 0.5f;
    p.x += p.vx; p.y += p.vy;
    if (!flight && !bouncing && !flappy && !flappyFreeze && p.x < camX + 8) p.x = camX + 8;
    p.onGround = 0;
    if (p.vy >= 0) { float nb = p.y + p.h * 0.5f;
        for (int i = 0; i < nPl; i++) { Plat *q = &pl[i];
            if (!(p.x + 6 > q->x && p.x - 6 < q->x + q->w && nb >= q->y && prevBottom <= q->y + 9)) continue;
            if (q->chain) { p.y = q->y - p.h * 0.5f - 1; p.vy = -11; if (p.vx < 4) p.vx = 4; bouncing = 1; bounceCombo++; bounceFlash = 32; addCoins(3); coinsWon += 3; hop(); break; }
            if (q->flappy && !flappy && !flappyFreeze) { p.y = q->y - p.h * 0.5f - 60; p.vx = p.vy = 0; flappyFreeze = 1; flappyPassed = 0; zoomT = 0.66f; break; }
            if (q->chair && !flight && !flappy && !flappyFreeze) { p.y = q->y - p.h * 0.5f - 1; p.vy = -16; p.vx = 8; flight = 1; zoomT = 0.62f; hop(); break; }
            p.y = q->y - p.h * 0.5f; p.vy = 0; p.onGround = 1; if (q->moving) p.x += q->dx;
            if (flappy || flappyFreeze) { flappy = flappyFreeze = 0; zoomT = BASEZ; nPi = 0; resumeRight(); }
            if (flight) { flight = 0; zoomT = BASEZ; mega.on = 0;
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
            if (p.x + 6 > q->x && p.x - 6 < q->x + q->w && (p.y - p.h * 0.35f < q->gapY - hg || p.y + p.h * 0.35f > q->gapY + hg)) { zoomT = BASEZ; gameOver(); return; }
            if (!q->passed && p.x > q->x + q->w) { q->passed = 1; flappyPassed++; addCoins(2); coinsWon += 2; } }
        if (p.y > flappyFloorY || p.y < flappyCeilY) { zoomT = BASEZ; gameOver(); return; }
    }
    if (state == ST_PLAY && !flappy && !flappyFreeze && !flight && !bouncing && p.onGround) {   // never a dead end ahead
        float pb = p.y + p.h * 0.5f; int ahead = 0;
        for (int i = 0; i < nPl; i++) { Plat *q = &pl[i]; if (q->x + q->w > p.x + p.w && q->x < p.x + W * 1.05f && q->y - pb > -H * 0.5f && q->y - pb < H * 1.1f) { ahead = 1; break; } }
        if (!ahead) { Plat *q = addPlat(p.x + W * 0.6f, pb, W * 0.5f); if (q) q->ground = 1;
            if (genMode != 0) { goRight(p.x + W * 1.1f, pb, 0.9f + rnd() * 0.7f, p.x + W * 1.5f); lastPlatX = genX - W * 0.2f; } }
    }
    if (!flappy && !flappyFreeze) { float pit = (flight || bouncing) ? H * 1.7f : H * 1.0f; if (p.y - lastGroundY > pit) { flight = 0; zoomT = BASEZ; gameOver(); } }
}
void updateJump(void) {
    if (state != ST_PLAY) return;
    step();
    if (state == ST_PLAY && ++speedAcc >= 7) { speedAcc = 0; step(); }   // the website runs at 1.14x: 8 steps every 7 frames
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
static void sky(void) {
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
    drawMarkerFx(s, w, JM_0_H, (int)((x0 + x1) / 2), (int)y, 1.5707963f, (x1 - x0) / JM_0_H, c);
}
void drawJump(void) {
    char s[32];
    sky(); gClipLo = 0; gClipHi = 2 * SH;
    for (int i = 0; i < nPi; i++) {                     // flappy pipes: stacked markers above and below the gap
        Pipe *q = &pi[i]; int w; const u16 *sp = mkSpr(q->col, &w); float hg = q->gapH * 0.5f, x = SXf(q->x + q->w / 2), sc = q->w * zoom / w;
        if (x < -40 || x > SW + 40) continue;
        float seg = JM_0_H * sc;
        for (float y = SYf(q->gapY - hg) - seg / 2; y > -seg; y -= seg) drawMarkerFx(sp, w, JM_0_H, (int)x, (int)y, 0, sc, q->col);
        for (float y = SYf(q->gapY + hg) + seg / 2; y < 2 * SH + seg; y += seg) drawMarkerFx(sp, w, JM_0_H, (int)x, (int)y, 0, sc, q->col);
        u16 g = q->col == 0 ? COL(31, 6, 8) : q->col == 1 ? COL(6, 31, 14) : COL(7, 15, 31);
        grect((int)(x - q->w * zoom / 2) - 3, (int)SYf(q->gapY - hg) - 6, (int)(q->w * zoom) + 6, 6, g);
        grect((int)(x - q->w * zoom / 2) - 3, (int)SYf(q->gapY + hg), (int)(q->w * zoom) + 6, 6, g);
    }
    for (int i = 0; i < nPl; i++) { Plat *q = &pl[i];
        if (q->chair) { float x = SXf(q->x + q->w / 2), y = SYf(q->y); float hh = 34 * zoom; blitRotScale(chair, CHAIR_W, CHAIR_H, (int)x, (int)(y - hh / 2), 0, hh / CHAIR_H); }
        else slab(q); }
    for (int i = 0; i < nCo; i++) if (!co[i].got) { int x = (int)SXf(co[i].x), y = (int)SYf(co[i].y); if (x > -10 && x < SW + 10 && y > -10 && y < 2 * SH + 10) blitRotScale(coinT, COIN_W, COIN_H, x, y, 0, 0.8f * zoom / 0.8f); }
    for (int i = 0; i < nOb; i++) { Obs *o = &ob[i]; int w; const u16 *sp = mkSpr(o->col, &w); float hgt = o->h * 1.15f * zoom;
        int x = (int)SXf(o->x), y = (int)(SYf(o->y) - hgt / 2); if (x < -20 || x > SW + 20) continue;
        drawMarkerFx(sp, w, JM_0_H, x, y, o->roll, hgt / JM_0_H, o->col); }
    if (mega.on && !mega.hit) { int x = (int)SXf(mega.x), y = (int)SYf(mega.y); blitRotScale(slotSymT[1], SLOT_LOGO_W, SLOT_LOGO_H, x, y, 0, mega.w * zoom / SLOT_LOGO_W); }
    {   // the player (the website's jump_*.png art)
        const u16 *sp; int w;
        if (!p.onGround) { sp = p.face < 0 ? jg_jumpl : jg_jumpr; w = p.face < 0 ? JG_JUMPL_W : JG_JUMPR_W; }
        else if (moveDir) { sp = p.face < 0 ? jg_runl : jg_runr; w = p.face < 0 ? JG_RUNL_W : JG_RUNR_W; }
        else { sp = jg_stand; w = JG_STAND_W; }
        float sc = zoom * 1.05f; blitRotScale(sp, w, JG_STAND_H, (int)SXf(p.x), (int)(SYf(p.y + p.h * 0.5f) - JG_STAND_H * sc / 2), 0, sc);
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
static Btn JB[5];
static void jmLayout(void) { for (int i = 0; i < 4; i++) { JB[i] = (Btn){ 38, 6 + i * 38, 180, 32, "", 0, 0 }; strcpy(JB[i].label, MODE_NAME[i]); } JB[4] = (Btn){ 68, 160, 120, 28, "BACK", 0, 0 }; }
void drawJumpMenu(void) {
    char s[40];
    fillScreen(bufTop, DARK);
    textC(bufTop, 6, "QU33PH JUMP", GOLD, 2);
    jmLayout(); int m = menuSel < 4 ? menuSel : 0;
    textC(bufTop, 44, MODE_NAME[m], WHITE, 2);
    textC(bufTop, 80, MODE_BLURB[m], GREY, 1);
    for (int i = 0; i < 4; i++) { sprintf(s, "%s  best %d", MODE_NAME[i], (int)JBEST(i)); textC(bufTop, 104 + i * 15, s, i == m ? YELLOW : GREY, 1); }
    coinCount(bufTop, 6, 176);
    textS(bufTop, SW - 6 - textSW("D-pad run - A jump"), 180, "D-pad run - A jump", WHITE);
    fillScreen(bufBot, DARK); drawBtns(bufBot, JB, 5, menuSel);
}
void inputJumpMenu(void) {
    jmLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(JB, 5, &menuSel, 1);
    if (h >= 0 && h < 4) { coinsWon = 0; startRun(h); }
    if (h == 4) goScreen(S_ARCADE);
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
