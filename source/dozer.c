// dozer.c — QU33PH DOZER (the website's dozer.html): a coin pusher.
//
// The cabinet art is the DS's own 2:3, so it fills both screens: the marquee and the drop slot
// on top, the pusher bed running down onto the bottom screen with its ledge near the bottom.
// Drop markers through the slot; the pusher shoves the pile and whatever goes over the ledge is
// yours: a coin is +1, a marker comes back to drop again. A chair drops onto the bed at 30
// seconds; the field refills when it runs dry. The website's push, shelf, pile and refill rules
// are all here. The pile can hold 130 pieces, so its physics run in whole-number (fixed-point)
// maths: the DS has no floating-point hardware, and this keeps it at full speed.
//
// CONTROLS: drag across the bottom screen to aim along the slot, lift to drop. Or D-pad
// left/right + A. L/R turns the marker (along the push / across). Out of markers: B buys 3 for
// 10 coins. START pauses (SELECT quits). X music, Y sound effects.
#include "qu.h"
#include "assets_dozer.h"

#define FX 65536                                     // 1.0 in fixed point (16.16)
#define F(x) ((int)((x) * FX))
static int fmul(int a, int b) { return (int)(((s64)a * b) >> 16); }
static int fdiv(int a, int b) { return (int)(((s64)a << 16) / b); }
static int isqrt64(s64 v) {                          // integer square root (for 32.32 -> 16.16)
    if (v <= 0) return 0;
    u64 x = (u64)v, r = 0, bit = (u64)1 << 62;
    while (bit > x) bit >>= 2;
    while (bit) { if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return (int)r;
}
// the website's bed, slot and sizes (fractions of the cabinet art and of the bed)
#define BED_BACKY 0.455f
#define BED_FRONTY 0.862f
#define BED_BACKL 0.350f
#define BED_BACKR 0.650f
#define BED_FRONTL 0.205f
#define BED_FRONTR 0.775f
#define SLOT_Y0 0.176f
#define SLOT_Y1 0.211f
#define SLOT_X0 0.140f
#define SLOT_X1 0.860f
#define CHAIR_AT 30
#define PZ_MIN F(0.02)
#define PZ_MAX F(0.43)
#define R_COIN F(0.050)
#define MK_RAD F(0.048)
#define MK_H F(0.062)
#define R_CHAIR F(0.115)
#define MAXI 150
typedef struct { int u, v, du, dv, rad, h, ex, ey, shelf, drop, coin, col; float rot, spin; } Item;   // ex,ey: half-length along rot
static Item it[MAXI]; static int nI;
static int queueC[64], nQ, cycleN, coinsWon, state, pz, pzPrev, chairOn, refillFr, dropping, paidOut, upright = 1, menuSel, overSel;
static float tLeft, tTotal = 60, pzPhase, chairT, dragU = 0.5f, dropT; static int dropCol; static float dropRot;
typedef struct { float x, y, vy, vx, rot, spin, sz; int coin, col; } Fall;
static Fall fall[24]; static int nFall;
typedef struct { char txt[16]; int life; u16 col; float u; int big; } Pop; static Pop pops[6];
enum { ST_PLAY, ST_OVER };
static u16 palT[256];
static int dragging;

void dozerThemeChanged(void) { if (!pakIs(DOZER_PAK)) return; for (int i = 0; i < 256; i++) palT[i] = themeTint(dz_pal[i], sv.theme); }
int dozerEnter(void) { if (!pakUse(DOZER_PAK, DOZER_PAK_SIZE, DOZER_PAK_ID)) return 0; dozerThemeChanged(); return 1; }
static int rndi(int a, int b) { return a + (int)(((s64)(rand() & 0xFFFF) * (b - a)) >> 16); }

static void setEnds(Item *a) { if (a->h) { a->ex = (int)(fcos(a->rot) * a->h); a->ey = (int)(fsin(a->rot) * a->h); } else a->ex = a->ey = 0; }
static void addCoin(int u, int v) { if (nI >= MAXI) return; Item *a = &it[nI++]; memset(a, 0, sizeof *a); a->u = u; a->v = v; a->rad = R_COIN; a->coin = 1; }
static Item *addMarker(int u, int v, int col, float rot) {
    if (nI >= MAXI) return 0; Item *a = &it[nI++]; memset(a, 0, sizeof *a);
    a->u = u; a->v = v; a->rad = MK_RAD; a->h = MK_H; a->col = col; a->rot = rot; setEnds(a); return a;
}
static void grant(int n) { for (int i = 0; i < n && nQ < 64; i++) queueC[nQ++] = cycleN++ % 3; }
static void popAdd(const char *t, u16 c, float u, int big) {
    for (int i = 0; i < 6; i++) if (pops[i].life <= 0) { strncpy(pops[i].txt, t, 15); pops[i].txt[15] = 0; pops[i].col = c; pops[i].u = u; pops[i].big = big; pops[i].life = big ? 80 : 50; return; }
}
static void sfx(int i) {
    const u8 *D[8] = { dz_s0, dz_s1, dz_s2, dz_s3, dz_s4, dz_s5, dz_s6, dz_s7 };
    const int L[8] = { DZ_S0_LEN, DZ_S1_LEN, DZ_S2_LEN, DZ_S3_LEN, DZ_S4_LEN, DZ_S5_LEN, DZ_S6_LEN, DZ_S7_LEN };
    playAdpcm(D[i], L[i], 12000, 80);
}

// ── the website's capsule contacts, in fixed point ───────────────────────
static void onSeg(int px, int py, int x1, int y1, int x2, int y2, int *ox, int *oy) {
    s64 dx = x2 - x1, dy = y2 - y1, L = dx * dx + dy * dy;
    if (L < 4096) { *ox = x1; *oy = y1; return; }
    s64 num = (s64)(px - x1) * dx + (s64)(py - y1) * dy;
    s64 t = (num << 16) / L; if (t < 0) t = 0; if (t > FX) t = FX;
    *ox = x1 + (int)((dx * t) >> 16); *oy = y1 + (int)((dy * t) >> 16);
}
static void closest(Item *a, Item *b, int *ax, int *ay, int *bx, int *by) {
    if (!a->h && !b->h) { *ax = a->u; *ay = a->v; *bx = b->u; *by = b->v; return; }
    if (!b->h) { onSeg(b->u, b->v, a->u - a->ex, a->v - a->ey, a->u + a->ex, a->v + a->ey, ax, ay); *bx = b->u; *by = b->v; return; }
    if (!a->h) { onSeg(a->u, a->v, b->u - b->ex, b->v - b->ey, b->u + b->ex, b->v + b->ey, bx, by); *ax = a->u; *ay = a->v; return; }
    int pax = a->u, pay = a->v, pbx = b->u, pby = b->v;
    for (int i = 0; i < 2; i++) {
        onSeg(pbx, pby, a->u - a->ex, a->v - a->ey, a->u + a->ex, a->v + a->ey, &pax, &pay);
        onSeg(pax, pay, b->u - b->ex, b->v - b->ey, b->u + b->ex, b->v + b->ey, &pbx, &pby);
    }
    *ax = pax; *ay = pay; *bx = pbx; *by = pby;
}
static int uExt(Item *a) { return a->h ? (a->ex < 0 ? -a->ex : a->ex) + a->rad : a->rad; }
static int backV(Item *a) { int y1 = a->v - a->ey, y2 = a->v + a->ey; return (y1 < y2 ? y1 : y2) - a->rad; }
FAST static void separate(void) {
    for (int i = 0; i < nI; i++) {
        Item *a = &it[i];
        for (int j = i + 1; j < nI; j++) {
            Item *b = &it[j];
            if (a->shelf != b->shelf) continue;
            int du = b->u - a->u, dv = b->v - a->v;
            // only pieces that could actually touch: their radii plus the markers' half-lengths
            int reach = a->rad + b->rad + (a->ex < 0 ? -a->ex : a->ex) + (a->ey < 0 ? -a->ey : a->ey) + (b->ex < 0 ? -b->ex : b->ex) + (b->ey < 0 ? -b->ey : b->ey);
            if (du > reach || du < -reach || dv > reach || dv < -reach) continue;
            int ax, ay, bx, by; closest(a, b, &ax, &ay, &bx, &by);
            int dx = bx - ax, dy = by - ay, mn = a->rad + b->rad;
            s64 d2 = (s64)dx * dx + (s64)dy * dy;
            if (d2 >= (s64)mn * mn || d2 == 0) continue;
            int d = isqrt64(d2);
            if (mn - d < F(0.006)) continue;                                      // settled contact: no jitter
            int inv = (1 << 30) / d, push = (mn - d) / 2;                       // one quick divide, then multiplies
            int nx = (int)(((s64)dx * inv) >> 14), ny = (int)(((s64)dy * inv) >> 14);
            a->u -= fmul(nx, push); a->v -= fmul(ny, push); b->u += fmul(nx, push); b->v += fmul(ny, push);
            int rel = fmul(b->du - a->du, nx) + fmul(b->dv - a->dv, ny);
            if (rel < 0) { int imp = fmul(-rel, F(0.48)); a->du -= fmul(nx, imp); a->dv -= fmul(ny, imp); b->du += fmul(nx, imp); b->dv += fmul(ny, imp); }
            if (a->h) a->spin += ((float)(ax - a->u) * (-ny) - (float)(ay - a->v) * (-nx)) / ((float)FX * FX) * 1.4f;
            if (b->h) b->spin += ((float)(bx - b->u) * ny - (float)(by - b->v) * nx) / ((float)FX * FX) * 1.4f;
        }
        if (chairOn && !a->shelf) {
            Item c = { 0 }; c.u = F(0.5); c.v = F(0.5);
            int ax, ay, bx, by; closest(a, &c, &ax, &ay, &bx, &by);
            int dx = ax - F(0.5), dy = ay - F(0.5), mn = a->rad + R_CHAIR;
            s64 d2 = (s64)dx * dx + (s64)dy * dy;
            if (d2 < (s64)mn * mn && d2 > 0) {
                int d = isqrt64(d2), nx = fdiv(dx, d), ny = fdiv(dy, d);
                a->u += fmul(nx, mn - d); a->v += fmul(ny, mn - d);
                float kick = a->h ? 0.020f : 0.006f, jit = a->h ? (frand() - 0.5f) * 1.8f : (frand() - 0.5f) * 0.5f, c2 = fcos(jit), s2 = fsin(jit);
                float fnx = nx / (float)FX, fny = ny / (float)FX;
                a->du += F((fnx * c2 - fny * s2) * kick); a->dv += F((fnx * s2 + fny * c2) * kick);
                if (a->h) a->spin += (frand() - 0.5f) * 0.7f;
            }
        }
        int ex = uExt(a);
        if (a->u < ex) { a->u = ex; a->du = (a->du < 0 ? -a->du : a->du) * 2 / 5; }
        if (a->u > FX - ex) { a->u = FX - ex; a->du = -(a->du < 0 ? -a->du : a->du) * 2 / 5; }
        if (a->v < a->rad / 5) a->v = a->rad / 5;
    }
}
static void reset(void) {
    nI = 0; coinsWon = 0; nQ = 0; cycleN = 0; grant(3);
    tLeft = tTotal; pz = pzPrev = PZ_MIN; pzPhase = 0; dragU = 0.5f; dropping = 0; chairOn = 0; chairT = 0;
    memset(pops, 0, sizeof pops); paidOut = 0; refillFr = 132; nFall = 0;
    int r = 0;
    for (float v = 0.525f; v <= 0.80f; v += 0.108f, r++) {           // rows of coins, as the website lays them
        int n = (r % 2 == 0) ? 9 : 8; float off = (r % 2 == 0) ? 0 : 0.84f / 16;
        for (int i = 0; i < n; i++) { float u = 0.08f + off + i * (0.84f / (n - 1)) + (frand() - 0.5f) * 0.02f;
            u = u < 0.05f ? 0.05f : u > 0.95f ? 0.95f : u; addCoin(F(u), F(v + (frand() - 0.5f) * 0.016f)); }
    }
    for (int i = 0; i < 9; i++) addMarker(F(0.15f + frand() * 0.7f), F(0.56f + frand() * 0.2f), rand() % 3, frand() * 3.14159f);
    const int minBack = PZ_MAX + F(0.025), maxFront = F(0.82);
    for (int pass = 0; pass < 4; pass++) {
        for (int k = 0; k < 6; k++) separate();
        for (int i = 0; i < nI; i++) { Item *a = &it[i]; int bv = backV(a); if (bv < minBack) a->v += minBack - bv; if (a->v > maxFront) a->v = maxFront - rndi(0, F(0.02)); }
    }
    int k = 0; for (int i = 0; i < nI; i++) if (it[i].v <= maxFront + F(0.005)) it[k++] = it[i]; nI = k;
    state = ST_PLAY;
}
static void doRefill(void) {
    int n = 30 + rand() % 8;        // the website tips in 50-60; the DS's smaller bed holds about 30-37 well
    for (int i = 0; i < n && nI < MAXI; i++) {
        int u = F(0.5) + rndi(-F(0.24), F(0.24)); if (u < R_COIN) u = R_COIN; if (u > FX - R_COIN) u = FX - R_COIN;
        addCoin(u, F(0.54) + rndi(-F(0.12), F(0.14))); Item *c = &it[nI - 1];
        c->du = rndi(-F(0.014), F(0.014)); c->dv = rndi(-F(0.012), F(0.016)); c->drop = FX;
    }
    refillFr = 132; popAdd("COIN DROP!", COL(31, 27, 0), 0.5f, 1);
    for (int i = 0; i < 3; i++) sfx(rand() % 5);
}
static void endGame(void) {
    state = ST_OVER;
    if (!paidOut && coinsWon > 0) { addCoins(coinsWon); paidOut = 1; }
    if (coinsWon > sv.arcadeBest[ARC_DOZER]) sv.arcadeBest[ARC_DOZER] = coinsWon;
    sv.arcadePlays[ARC_DOZER]++; saveWrite();
    screen = S_DOZER_OVER;
}
// the bed's perspective, in whole numbers: u,v (16.16) -> screen x,y and the size factor (16.16)
#define BXL ((int)(256 * BED_BACKL * FX))
#define BXR ((int)(256 * BED_BACKR * FX))
#define FXL ((int)(256 * BED_FRONTL * FX))
#define FXR ((int)(256 * BED_FRONTR * FX))
static void bedXYi(int u, int v, int *x, int *y, int *s) {
    int l = BXL + fmul(FXL - BXL, v), r = BXR + fmul(FXR - BXR, v);
    *x = (l + fmul(r - l, u)) >> 16;
    *y = (int)(384 * BED_BACKY) + (fmul((int)(384 * (BED_FRONTY - BED_BACKY) * FX), v) >> 16);
    *s = FX + fmul((int)(((BED_FRONTR - BED_FRONTL) / (BED_BACKR - BED_BACKL) - 1) * FX), v);
}
static void bedXY(int u, int v, float *x, float *y, float *s) { int ix, iy, is; bedXYi(u, v, &ix, &iy, &is); *x = ix; *y = iy; *s = is / (float)FX; }
void updateDozer(void) {
    const float dt = 1.0f / 60;
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) pops[i].life--;
    for (int i = nFall - 1; i >= 0; i--) {                          // tumbling over the front, then gone
        fall[i].vy += 384 * 1.2f * dt; fall[i].y += fall[i].vy * dt; fall[i].x += fall[i].vx * dt; fall[i].rot += fall[i].spin * dt; fall[i].sz *= 1 - dt * 0.16f;
        if (fall[i].y > 384 * 1.14f) fall[i] = fall[--nFall];
    }
    if (state != ST_PLAY) return;
    tLeft -= dt;
    if (!chairOn && tTotal - tLeft >= CHAIR_AT) { chairOn = 1; chairT = 0; popAdd("CHAIR DROPPED!", COL(31, 11, 11), 0.5f, 1); }
    if (chairOn && chairT < 1) { chairT += dt * 2.2f; if (chairT > 1) chairT = 1; }
    if (tLeft <= 0) { tLeft = 0; endGame(); return; }
    if (refillFr <= 0) { int nc = 0; for (int i = 0; i < nI; i++) nc += it[i].coin; if (nc < 5) doRefill(); }
    pzPhase += dt * 2.0f; pzPrev = pz;                               // (the website's 1.55, quickened for the DS)
    pz = PZ_MIN + (int)((PZ_MAX - PZ_MIN) * (0.5f - 0.5f * fcos(pzPhase)));
    int pSpeed = pz - pzPrev > 0 ? pz - pzPrev : 0, dpz = pz - pzPrev;
    if (dropping) {
        dropT += dt * 2.6f;
        if (dropT >= 1) {
            int landV = fmul(MK_RAD, F(1.2)) + fmul(MK_H, F(0.2));
            Item *m = addMarker(F(dragU), landV, dropCol, dropRot);
            if (m) m->shelf = landV < pz;                          // caught on the pusher's shelf
            dropping = 0;
        }
    }
    for (int i = 0; i < nI; i++) {
        Item *a = &it[i];
        a->u += a->du; a->v += a->dv; a->du = a->du * 9 / 10; a->dv = a->dv * 9 / 10;
        if (a->h) { a->rot += a->spin; a->spin *= 0.88f; setEnds(a); }
        if (a->shelf) {
            if (dpz > 0) a->v += dpz;
            if (a->v > pz) { a->shelf = 0; a->drop = FX; if (a->dv < F(0.003)) a->dv = F(0.003); }
        } else { int bv = backV(a); if (bv < pz) { a->v += pz - bv; int k = fmul(pSpeed, F(1.7)); if (a->dv < k) a->dv = k; } }   // the face drives it
        if (a->drop > 0) { a->drop -= F(dt * 3.2f); if (a->drop < 0) a->drop = 0; }
    }
    for (int k = 0; k < 4; k++) separate();
    if (refillFr > 0) { refillFr--; for (int i = 0; i < nI; i++) if (it[i].v > F(0.97)) it[i].v = F(0.97); }
    for (int i = nI - 1; i >= 0; i--) {
        Item *a = &it[i];
        if (a->shelf || refillFr > 0) continue;
        if (a->v > FX - fmul(a->rad, F(0.30))) {                   // over the ledge
            float x, y, s; bedXY(a->u, FX, &x, &y, &s);
            if (nFall < 24) fall[nFall++] = (Fall){ x, y, 384 * 0.10f, (a->u / (float)FX - 0.5f) * 256 * 0.04f, a->rot, (frand() - 0.5f) * 3, s, a->coin, a->col };
            if (a->coin) { coinsWon++; popAdd("+1", COL(31, 27, 0), a->u / (float)FX, 0); sfx(rand() % 5); }
            else { if (nQ < 64) queueC[nQ++] = a->col; popAdd("MARKER BACK", COL(17, 26, 31), a->u / (float)FX, 0); sfx(7); }
            it[i] = it[--nI];
        }
    }
}

// ── input ─────────────────────────────────────────────────────────────────
static void doDrop(void) {
    if (state != ST_PLAY || nQ <= 0 || dropping) return;
    dropCol = queueC[0]; memmove(queueC, queueC + 1, sizeof(int) * (--nQ));
    dropping = 1; dropT = 0; dropRot = upright ? 1.5707963f : 0;      // vertical = along the push
    sfx(5 + rand() % 2);
}
void inputDozer(void) {
    if (screen == S_DOZER_PAUSE) {
        if (kDown & KEY_START) screen = S_DOZER;
        if (kDown & KEY_SELECT) { endGame(); goScreen(S_ARCADE); }
        return;
    }
    if (kDown & KEY_START) { screen = S_DOZER_PAUSE; return; }
    if (kDown & KEY_X) musicToggle();
    if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    if (kDown & (KEY_L | KEY_R)) upright = !upright;
    if (kDown & KEY_B && nQ <= 0 && !dropping && sv.coins >= 10) { spendCoins(10); grant(3); popAdd("+3 MARKERS", COL(15, 31, 19), 0.5f, 1); }
    if (kDown & KEY_TOUCH) dragging = 1;                              // (a touch carried in from the menu doesn't count)
    if (dragging && (kHeld & KEY_TOUCH)) { float u = (tX / 256.0f - SLOT_X0) / (SLOT_X1 - SLOT_X0); dragU = u < 0 ? 0 : u > 1 ? 1 : u; }
    if (dragging && (kUp & KEY_TOUCH)) { dragging = 0; doDrop(); }
    if (kHeld & KEY_LEFT) { dragU -= 0.02f; if (dragU < 0) dragU = 0; }
    if (kHeld & KEY_RIGHT) { dragU += 0.02f; if (dragU > 1) dragU = 1; }
    if (kDown & KEY_A) doDrop();
}

// ── drawing ───────────────────────────────────────────────────────────────
static void markerPic(int col, const u16 **s, int *w, int *h) {
    if (col == 2) { *s = mk_blue; *w = MK_BLUE_W; *h = MK_BLUE_H; }
    else { *s = col == 0 ? mk_red : mk_green; *w = MK_GREEN_W; *h = MK_GREEN_H; }
}
static void drawMk(float x, float y, float len, float rot, int col) {
    const u16 *s; int w, h; markerPic(col, &s, &w, &h);
    // the website lays the square marker pictures across len x len, the blue one across len
    drawMarkerFx(s, w, h, (int)x, (int)y, rot - 0.785f * (col != 2), len / w, col);
}
// coins come in a handful of sizes: each size is scaled once (and again when the theme changes)
// and then copied straight into the screen rows, with no maths per pixel
#define CMAXD 22
static u16 coinCache[CMAXD + 1][CMAXD * CMAXD]; static u8 coinReady[CMAXD + 1]; static int coinTheme = -1;
static u16 *gRow(int gy) { return gy < SH ? &bufTop[gy * SW] : &bufBot[(gy - SH) * SW]; }
static void coinAt(int x, int gy, int rad) {
    int d = rad * 2; if (d < 3) d = 3; if (d > CMAXD) d = CMAXD;
    int dh = d * 82 / 100;
    if (coinTheme != sv.theme) { memset(coinReady, 0, sizeof coinReady); coinTheme = sv.theme; }
    u16 *c = coinCache[d];
    if (!coinReady[d]) { for (int j = 0; j < dh; j++) for (int i = 0; i < d; i++) c[j * d + i] = coinT[(j * COIN_H / dh) * COIN_W + i * COIN_W / d]; coinReady[d] = 1; }
    int x0 = x - d / 2, y0 = gy - dh / 2;
    for (int j = 0; j < dh; j++) { int yy = y0 + j; if (yy < 0 || yy >= 2 * SH) continue;
        u16 *row = gRow(yy); const u16 *src = &c[j * d];
        for (int i = 0; i < d; i++) { int xx = x0 + i; u16 p = src[i]; if ((p & 0x8000) && (unsigned)xx < SW) row[xx] = p; } }
}
static void drawPusher(void) {
    float yTop = 384 * 0.405f, s, xl, xr, yF;
    bedXY(0, pz, &xl, &yF, &s); bedXY(FX, pz, &xr, &yF, &s);
    float bw = (BED_BACKR - BED_BACKL) * 256, lT = 256 * BED_BACKL - bw * 0.16f, rT = 256 * BED_BACKR + bw * 0.16f;
    float lF = xl - bw * 0.14f * s, rF = xr + bw * 0.14f * s;
    for (int y = (int)yTop; y < (int)yF; y++) {
        float t = (y - yTop) / (yF - yTop); int x0 = (int)(lT + (lF - lT) * t), x1 = (int)(rT + (rF - rT) * t);
        u16 c = COL(3 + (int)(t * 3), 3 + (int)(t * 3), 3 + (int)(t * 4));
        if (x0 < 0) x0 = 0;
        if (x1 > SW) x1 = SW;
        u16 *row = gRow(y); for (int x = x0; x < x1; x++) row[x] = c;
    }
    int fh = (int)(384 * 0.022f * s), y0 = (int)yF - fh / 2;           // the yellow-and-black hazard face
    for (int j = 0; j < fh; j++) for (int x = (int)lF; x < (int)rF; x++) gpx(x, y0 + j, ((x + j) / (fh > 2 ? fh : 3)) & 1 ? COL(2, 2, 2) : COL(30, 24, 0));
}
void drawDozer(void) {
    char s[32];
    drawIndexed(dz_cab, palT);
    gClipLo = 0; gClipHi = 2 * SH;
    drawPusher();
    if (chairOn) { float x, y, sc; bedXY(F(0.5), F(0.5), &x, &y, &sc);
        float hh = (R_CHAIR / (float)FX) * 76.8f * sc * 2.3f * (0.35f + 0.65f * chairT);
        blitRotScale(chair, CHAIR_W, CHAIR_H, (int)x, (int)(y - hh * 0.22f), 0, hh / CHAIR_H); }
    // the pile, back to front
    static u8 ord[MAXI]; for (int i = 0; i < nI; i++) ord[i] = i;
    for (int i = 1; i < nI; i++) { u8 k = ord[i]; int j = i - 1; while (j >= 0 && it[ord[j]].v > it[k].v) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = k; }
    for (int n = 0; n < nI; n++) { Item *a = &it[ord[n]]; int x, y, sc; bedXYi(a->u, a->v, &x, &y, &sc);
        y -= fmul(fmul(a->shelf ? FX : a->drop, sc), F(384 * 0.020)) >> 16;   // (lift, in pixels: was left in 16.16, so lifted pieces flew off-screen)
        if (a->coin) { int rad = (fmul(sc, F(0.050 * 76.8 * 0.95)) + FX / 2) >> 16;
            u16 *row = (y + rad / 2 >= 0 && y + rad / 2 < 2 * SH) ? gRow(y + rad / 2) : 0;
            if (row) for (int i = -rad; i <= rad; i++) { int xx = x + i; if ((unsigned)xx < SW) row[xx] = ((row[xx] >> 1) & 0x3DEF) | 0x8000; }
            coinAt(x, y, rad); }
        else drawMk(x, y, ((MK_H + MK_RAD) * 2 / (float)FX) * 76.8f * (sc / (float)FX), a->rot, a->col);
    }
    for (int i = 0; i < nFall; i++) {
        if (fall[i].coin) coinAt((int)fall[i].x, (int)fall[i].y, (int)((R_COIN / (float)FX) * 76.8f * fall[i].sz));
        else drawMk(fall[i].x, fall[i].y, ((MK_H + MK_RAD) * 2 / (float)FX) * 76.8f * fall[i].sz, fall[i].rot, fall[i].col);
    }
    // the slot: where you'll drop, and the marker on its way down
    float sx = 256 * (SLOT_X0 + (SLOT_X1 - SLOT_X0) * dragU), sy = 384 * (SLOT_Y0 + SLOT_Y1) * 0.5f, len = ((MK_H + MK_RAD) * 2 / (float)FX) * 76.8f;
    if (state == ST_PLAY && nQ > 0 && !dropping) { drawMk(sx, sy, len, upright ? 1.5707963f : 0, queueC[0]); for (int k = -2; k <= 2; k++) gpx((int)sx + k, (int)sy + 12, GOLD); }
    if (dropping) { float t = dropT, x, y, sc; bedXY(F(dragU), 0, &x, &y, &sc);
        drawMk(sx + (x - sx) * t, sy + (y - sy) * t, len * (1 - 0.12f * t), dropRot, dropCol); }
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) {
        if (pops[i].big) textC(bufTop, 132, pops[i].txt, pops[i].col, 2);
        else { float x, y, sc; bedXY(F(pops[i].u), FX, &x, &y, &sc); gtext((int)x - textW(pops[i].txt, 1) / 2, (int)y - 30 - (50 - pops[i].life) / 2, pops[i].txt, pops[i].col, 1); }
    }
    // HUD between the slot and the bed
    sprintf(s, "TIME %d", (int)(tLeft + 0.99f)); text(bufTop, 6, 88, s, tLeft < 10 ? RED : WHITE, 1);
    sprintf(s, "+%d", coinsWon); textC(bufTop, 88, s, GOLD, 1);
    sprintf(s, "MARKERS %d", nQ); text(bufTop, SW - 6 - textW(s, 1), 88, s, WHITE, 1);
    if (state == ST_PLAY && nQ <= 0 && !dropping) textC(bufBot, SH - 16, sv.coins >= 10 ? "B: +3 MARKERS (10 COINS)" : "NEED 10 COINS FOR MORE", YELLOW, 1);
    else textS(bufBot, SW - 6 - textSW(upright ? "L/R: ALONG" : "L/R: ACROSS"), SH - 12, upright ? "L/R: ALONG" : "L/R: ACROSS", WHITE);
    if (screen == S_DOZER_PAUSE) { textC(bufTop, 120, "PAUSED", YELLOW, 2); textC(bufTop, 156, "START resume - SELECT quit", WHITE, 1); }
}

// ── menu and results ──────────────────────────────────────────────────────
static Btn DB[4];
void drawDozerMenu(void) {
    char s[32];
    fillScreen(bufTop, DARK);
    textC(bufTop, 6, "QU33PH DOZER", GOLD, 2);
    textC(bufTop, 40, "Drag across the slot, lift to drop.", WHITE, 1);
    textC(bufTop, 58, "Coin off the ledge = +1 coin", GOLD, 1);
    textC(bufTop, 74, "a marker off the ledge comes back", WHITE, 1);
    textC(bufTop, 92, "a chair drops in at 30 seconds", COL(31, 11, 11), 1);
    textC(bufTop, 108, "out of markers? +3 for 10 coins", GREY, 1);
    sprintf(s, "BEST HAUL %d", sv.arcadeBest[ARC_DOZER]); textC(bufTop, 140, s, GREY, 1);
    coinCount(bufTop, 6, 176);
    fillScreen(bufBot, DARK);
    DB[0] = (Btn){ 38, 10, 180, 34, "60 SECONDS", 0, 0 }; DB[1] = (Btn){ 38, 52, 180, 34, "90 SECONDS", 0, 0 };
    DB[2] = (Btn){ 38, 94, 180, 34, "120 SECONDS", 0, 0 }; DB[3] = (Btn){ 68, 142, 120, 30, "BACK", 0, 0 };
    drawBtns(bufBot, DB, 4, menuSel);
}
void inputDozerMenu(void) {
    DB[0] = (Btn){ 38, 10, 180, 34, "60 SECONDS", 0, 0 }; DB[1] = (Btn){ 38, 52, 180, 34, "90 SECONDS", 0, 0 };
    DB[2] = (Btn){ 38, 94, 180, 34, "120 SECONDS", 0, 0 }; DB[3] = (Btn){ 68, 142, 120, 30, "BACK", 0, 0 };
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(DB, 4, &menuSel, 1);
    if (h >= 0 && h <= 2) { tTotal = 60 + h * 30; reset(); screen = S_DOZER; }
    if (h == 3) goScreen(S_ARCADE);
}
void drawDozerOver(void) {
    char s[32];
    drawDozer();
    box(bufTop, 28, 40, 200, 100, COL(2, 2, 4), GOLD);
    textC(bufTop, 48, "TIME UP", GOLD, 2);
    sprintf(s, "+%d COINS WON", coinsWon); textC(bufTop, 84, s, WHITE, 1);
    sprintf(s, "BEST HAUL %d", sv.arcadeBest[ARC_DOZER]); textC(bufTop, 104, s, COL(15, 27, 21), 1);
    fillScreen(bufBot, DARK);
    DB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; DB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    drawBtns(bufBot, DB, 2, overSel);
}
void inputDozerOver(void) {
    DB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; DB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    int h = btnInput(DB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) { reset(); screen = S_DOZER; }
    if (h == 1) goScreen(S_ARCADE);
}
