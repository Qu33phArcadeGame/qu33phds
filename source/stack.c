// stack.c — QU33PH STACK (the website's stack.html): drag & drop markers into a tower.
//
// The website uses the matter.js physics library. The DS has no such thing, so this file
// carries its own small rigid-body engine: a C port of Box2D-Lite (box-vs-box contacts with
// clipping, and a sequential-impulse solver with friction and warm starting). Settings follow
// the website's: gravity, friction 1, no bounce, air drag 0.03, the same marker box sizes and the
// same "settled" test. To stay quick on the DS only the top few markers of the tower stay loose
// (everything lower is locked in place once it's buried); when the tower falls, the top of it
// comes down.
//
// CONTROLS: touch and drag the marker, lift to drop it. Or: D-pad to move it, A to drop.
// L/R (or the round button bottom-left) turns it upright / flat. START pauses (SELECT quits).
#include "qu.h"
#include "assets_stack.h"

#define CW 256.0f
#define CH 384.0f
#define ML 92.0f                                   // marker length (the website's WW*0.36)
#define MW 29.0f                                   // WW*0.115
#define PLATE_W 102.0f                             // WW*0.40
#define PLATE_Y 346.0f                             // CH*0.90
#define GRAV 1050.0f                                // px/s^2: about twice the website's pull, so the DS plays snappier
#define LOOSE 4                                    // how many of the top placed markers stay loose
#define MAXB 160

// ── a little Box2D-Lite ──────────────────────────────────────────────────
typedef struct { float x, y; } V2;
static V2 v2(float x, float y) { V2 r = { x, y }; return r; }
static V2 vadd(V2 a, V2 b) { return v2(a.x + b.x, a.y + b.y); }
static V2 vsub(V2 a, V2 b) { return v2(a.x - b.x, a.y - b.y); }
static V2 vmul(V2 a, float s) { return v2(a.x * s, a.y * s); }
static float dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
static float crossVV(V2 a, V2 b) { return a.x * b.y - a.y * b.x; }
static V2 crossSV(float s, V2 a) { return v2(-s * a.y, s * a.x); }
static float fabs_(float v) { return v < 0 ? -v : v; }
typedef struct { V2 c1, c2; } M22;                // columns
static M22 rotm(float a) { float c = fcos(a), s = fsin(a); M22 m = { { c, s }, { -s, c } }; return m; }
static M22 tr(M22 m) { M22 t = { { m.c1.x, m.c2.x }, { m.c1.y, m.c2.y } }; return t; }
static V2 mv(M22 m, V2 v) { return v2(m.c1.x * v.x + m.c2.x * v.y, m.c1.y * v.x + m.c2.y * v.y); }
static M22 mm(M22 a, M22 b) { M22 r = { mv(a, b.c1), mv(a, b.c2) }; return r; }
static M22 mabs(M22 a) { M22 r = { { fabs_(a.c1.x), fabs_(a.c1.y) }, { fabs_(a.c2.x), fabs_(a.c2.y) } }; return r; }

typedef struct {
    V2 p, v; float a, w, hw, hh, invM, invI, fric; int col, isStatic, used, settle, dropped, held;
} Body;
typedef union { struct { u8 in1, out1, in2, out2; } e; u32 key; } Feat;
typedef struct { V2 pos, n, r1, r2; float sep, Pn, Pt, mN, mT, bias; Feat f; } Contact;
typedef struct { int a, b, n; Contact c[2]; float fric; } Arb;
static Body B[MAXB]; static int nB;
static Arb arbs[96], arbsOld[96]; static int nArb, nArbOld;
enum { NOE, E1, E2, E3, E4 };
typedef struct { V2 v; Feat f; } CV;

static void setMass(Body *b, int isStatic) {
    b->isStatic = isStatic;
    if (isStatic) { b->invM = b->invI = 0; return; }
    float m = (b->hw * 2) * (b->hh * 2) * 0.004f;              // the website's density
    b->invM = 1 / m; b->invI = 1 / (m * ((b->hw * 2) * (b->hw * 2) + (b->hh * 2) * (b->hh * 2)) / 12);
}
static int clipLine(CV out[2], CV in[2], V2 n, float off, u8 edge) {
    int k = 0; float d0 = dot(n, in[0].v) - off, d1 = dot(n, in[1].v) - off;
    if (d0 <= 0) out[k++] = in[0];
    if (d1 <= 0) out[k++] = in[1];
    if (d0 * d1 < 0) {
        float t = d0 / (d0 - d1);
        out[k].v = vadd(in[0].v, vmul(vsub(in[1].v, in[0].v), t));
        if (d0 > 0) { out[k].f = in[0].f; out[k].f.e.in1 = edge; out[k].f.e.in2 = NOE; }
        else { out[k].f = in[1].f; out[k].f.e.out1 = edge; out[k].f.e.out2 = NOE; }
        k++;
    }
    return k;
}
static void incident(CV c[2], V2 h, V2 pos, M22 R, V2 n0) {
    V2 n = mv(tr(R), n0); n.x = -n.x; n.y = -n.y;
    if (fabs_(n.x) > fabs_(n.y)) {
        if (n.x > 0) { c[0].v = v2(h.x, -h.y); c[0].f.e.in2 = E3; c[0].f.e.out2 = E4; c[1].v = v2(h.x, h.y); c[1].f.e.in2 = E4; c[1].f.e.out2 = E1; }
        else { c[0].v = v2(-h.x, h.y); c[0].f.e.in2 = E1; c[0].f.e.out2 = E2; c[1].v = v2(-h.x, -h.y); c[1].f.e.in2 = E2; c[1].f.e.out2 = E3; }
    } else {
        if (n.y > 0) { c[0].v = v2(h.x, h.y); c[0].f.e.in2 = E4; c[0].f.e.out2 = E1; c[1].v = v2(-h.x, h.y); c[1].f.e.in2 = E1; c[1].f.e.out2 = E2; }
        else { c[0].v = v2(-h.x, -h.y); c[0].f.e.in2 = E2; c[0].f.e.out2 = E3; c[1].v = v2(h.x, -h.y); c[1].f.e.in2 = E3; c[1].f.e.out2 = E4; }
    }
    c[0].f.e.in1 = c[0].f.e.out1 = c[1].f.e.in1 = c[1].f.e.out1 = NOE;
    c[0].v = vadd(pos, mv(R, c[0].v)); c[1].v = vadd(pos, mv(R, c[1].v));
}
static int collide(Contact *out, Body *A, Body *Bb) {
    V2 hA = v2(A->hw, A->hh), hB = v2(Bb->hw, Bb->hh);
    M22 RA = rotm(A->a), RB = rotm(Bb->a), RAT = tr(RA), RBT = tr(RB);
    V2 dp = vsub(Bb->p, A->p), dA = mv(RAT, dp), dB = mv(RBT, dp);
    M22 C = mm(RAT, RB), aC = mabs(C), aCT = tr(aC);
    V2 t1 = mv(aC, hB), fA = v2(fabs_(dA.x) - hA.x - t1.x, fabs_(dA.y) - hA.y - t1.y);
    if (fA.x > 0 || fA.y > 0) return 0;
    V2 t2 = mv(aCT, hA), fB = v2(fabs_(dB.x) - t2.x - hB.x, fabs_(dB.y) - t2.y - hB.y);
    if (fB.x > 0 || fB.y > 0) return 0;
    int axis = 0; float sep = fA.x; V2 n = dA.x > 0 ? RA.c1 : vmul(RA.c1, -1);
    const float rt = 0.95f, at = 0.01f;
    if (fA.y > rt * sep + at * hA.y) { axis = 1; sep = fA.y; n = dA.y > 0 ? RA.c2 : vmul(RA.c2, -1); }
    if (fB.x > rt * sep + at * hB.x) { axis = 2; sep = fB.x; n = dB.x > 0 ? RB.c1 : vmul(RB.c1, -1); }
    if (fB.y > rt * sep + at * hB.y) { axis = 3; sep = fB.y; n = dB.y > 0 ? RB.c2 : vmul(RB.c2, -1); }
    V2 fn, sn; CV ie[2]; float front, negS, posS, side; u8 negE, posE;
    if (axis == 0) { fn = n; front = dot(A->p, fn) + hA.x; sn = RA.c2; side = dot(A->p, sn); negS = -side + hA.y; posS = side + hA.y; negE = E3; posE = E1; incident(ie, hB, Bb->p, RB, fn); }
    else if (axis == 1) { fn = n; front = dot(A->p, fn) + hA.y; sn = RA.c1; side = dot(A->p, sn); negS = -side + hA.x; posS = side + hA.x; negE = E2; posE = E4; incident(ie, hB, Bb->p, RB, fn); }
    else if (axis == 2) { fn = vmul(n, -1); front = dot(Bb->p, fn) + hB.x; sn = RB.c2; side = dot(Bb->p, sn); negS = -side + hB.y; posS = side + hB.y; negE = E3; posE = E1; incident(ie, hA, A->p, RA, fn); }
    else { fn = vmul(n, -1); front = dot(Bb->p, fn) + hB.y; sn = RB.c1; side = dot(Bb->p, sn); negS = -side + hB.x; posS = side + hB.x; negE = E2; posE = E4; incident(ie, hA, A->p, RA, fn); }
    CV c1[2], c2[2];
    if (clipLine(c1, ie, vmul(sn, -1), negS, negE) < 2) return 0;
    if (clipLine(c2, c1, sn, posS, posE) < 2) return 0;
    int k = 0;
    for (int i = 0; i < 2; i++) {
        float s = dot(fn, c2[i].v) - front;
        if (s <= 0) {
            out[k].sep = s; out[k].n = n; out[k].pos = vsub(c2[i].v, vmul(fn, s)); out[k].f = c2[i].f;
            if (axis >= 2) { u8 t = out[k].f.e.in1; out[k].f.e.in1 = out[k].f.e.in2; out[k].f.e.in2 = t; t = out[k].f.e.out1; out[k].f.e.out1 = out[k].f.e.out2; out[k].f.e.out2 = t; }
            out[k].Pn = out[k].Pt = 0; k++;
        }
    }
    return k;
}
static void applyP(Body *b1, Body *b2, V2 r1, V2 r2, V2 P) {
    b1->v = vsub(b1->v, vmul(P, b1->invM)); b1->w -= b1->invI * crossVV(r1, P);
    b2->v = vadd(b2->v, vmul(P, b2->invM)); b2->w += b2->invI * crossVV(r2, P);
}
static float physGrav = GRAV;
FAST static void step(float dt) {
    // broad phase: only pairs with a loose body, whose boxes come near each other
    memcpy(arbsOld, arbs, sizeof(Arb) * nArb); nArbOld = nArb; nArb = 0;
    // (the same pairs as before, found faster: a pair needs a loose body, and only a handful are
    //  loose, so loop over those instead of every pair of up to 160 bodies)
    static u8 loose[MAXB];
    for (int i = 0; i < nB; i++) loose[i] = B[i].used && B[i].invM != 0;
    for (int i = 0; i < nB; i++) { Body *a = &B[i]; if (!a->used) continue;
        for (int j = i + 1; j < nB; j++) { if (!loose[i] && !loose[j]) continue; Body *b = &B[j]; if (!b->used) continue;
            float ra = a->hw + a->hh, rb = b->hw + b->hh;
            if (fabs_(a->p.x - b->p.x) > ra + rb || fabs_(a->p.y - b->p.y) > ra + rb) continue;
            Contact cs[2]; int n = collide(cs, a, b);
            if (!n || nArb >= 96) continue;
            Arb *ar = &arbs[nArb++]; ar->a = i; ar->b = j; ar->n = n; ar->fric = fsqrt(a->fric * b->fric);
            for (int k = 0; k < n; k++) {                    // warm start from last step's matching contact
                ar->c[k] = cs[k];
                for (int o = 0; o < nArbOld; o++) if (arbsOld[o].a == i && arbsOld[o].b == j)
                    for (int q = 0; q < arbsOld[o].n; q++) if (arbsOld[o].c[q].f.key == cs[k].f.key) { ar->c[k].Pn = arbsOld[o].c[q].Pn; ar->c[k].Pt = arbsOld[o].c[q].Pt; }
            }
        } }
    for (int i = 0; i < nB; i++) { Body *b = &B[i]; if (!b->used || b->invM == 0) continue; b->v.y += dt * physGrav; }
    float inv = 1 / dt;
    for (int k = 0; k < nArb; k++) { Arb *ar = &arbs[k]; Body *b1 = &B[ar->a], *b2 = &B[ar->b];
        for (int i = 0; i < ar->n; i++) { Contact *c = &ar->c[i];
            c->r1 = vsub(c->pos, b1->p); c->r2 = vsub(c->pos, b2->p);
            float rn1 = dot(c->r1, c->n), rn2 = dot(c->r2, c->n);
            c->mN = 1 / (b1->invM + b2->invM + b1->invI * (dot(c->r1, c->r1) - rn1 * rn1) + b2->invI * (dot(c->r2, c->r2) - rn2 * rn2));
            V2 t = v2(c->n.y, -c->n.x); float rt1 = dot(c->r1, t), rt2 = dot(c->r2, t);
            c->mT = 1 / (b1->invM + b2->invM + b1->invI * (dot(c->r1, c->r1) - rt1 * rt1) + b2->invI * (dot(c->r2, c->r2) - rt2 * rt2));
            float s = c->sep + 0.5f; c->bias = s < 0 ? -0.2f * inv * s : 0;
            applyP(b1, b2, c->r1, c->r2, vadd(vmul(c->n, c->Pn), vmul(t, c->Pt)));
        } }
    for (int it = 0; it < 7; it++)                                    // 7 solver passes keep stacks steady and the DS quick
        for (int k = 0; k < nArb; k++) { Arb *ar = &arbs[k]; Body *b1 = &B[ar->a], *b2 = &B[ar->b];
            for (int i = 0; i < ar->n; i++) { Contact *c = &ar->c[i];
                V2 dv = vsub(vadd(b2->v, crossSV(b2->w, c->r2)), vadd(b1->v, crossSV(b1->w, c->r1)));
                float dPn = c->mN * (-dot(dv, c->n) + c->bias), P0 = c->Pn;
                c->Pn = P0 + dPn > 0 ? P0 + dPn : 0; dPn = c->Pn - P0;
                applyP(b1, b2, c->r1, c->r2, vmul(c->n, dPn));
                dv = vsub(vadd(b2->v, crossSV(b2->w, c->r2)), vadd(b1->v, crossSV(b1->w, c->r1)));
                V2 t = v2(c->n.y, -c->n.x);
                float dPt = c->mT * (-dot(dv, t)), mx = ar->fric * c->Pn, T0 = c->Pt, Tn = T0 + dPt;
                c->Pt = Tn < -mx ? -mx : Tn > mx ? mx : Tn; dPt = c->Pt - T0;
                applyP(b1, b2, c->r1, c->r2, vmul(t, dPt));
            } }
    for (int i = 0; i < nB; i++) { Body *b = &B[i]; if (!b->used || b->invM == 0) continue;
        b->v = vmul(b->v, 0.97f); b->w *= 0.97f;                       // the website's air drag (0.03)
        b->p = vadd(b->p, vmul(b->v, dt)); b->a += b->w * dt; }
}

// ── the game ──────────────────────────────────────────────────────────────
enum { ST_PLAY, ST_OVER };
static int state, score, overT, cur = -1, placed[MAXB], nPlaced, horizontal, coinsWon, newBest, overSel, menuSel;
static float camY, camScale = 1; static V2 target; static int haveTarget;
static u16 palT[256];
static const u8 *stBgImg(void) { return (sv.theme == 1 || sv.theme == 4) ? st_winp : st_win; }
void stackThemeChanged(void) {
    if (!pakIs(STACK_PAK)) return;
    // the website draws its window tile over the theme's backdrop, then darkens it so the tower
    // pops: those fixed steps are folded into the palette here
    int t = sv.theme; const u16 *p = (t == 1 || t == 4) ? st_winp_pal : st_win_pal;
    float al = t == 1 ? 0.55f : 0.80f, dk = t == 1 ? 0.94f : 0.70f;
    u16 bg = t == 1 ? COL(31, 31, 31) : t == 4 ? COL(2, 0, 3) : t == 2 ? COL(1, 1, 4) : (t == 3 || t == 5) ? COL(13, 8, 2) : COL(2, 3, 5);
    for (int i = 0; i < 256; i++) {
        u16 c = t == 1 ? themeTint(p[i], 1) : (t == 2 || t == 3 || t == 5) ? themeTint(p[i], t) : p[i];
        int r = (int)(((c & 31) * al + (bg & 31) * (1 - al)) * dk), g = (int)((((c >> 5) & 31) * al + ((bg >> 5) & 31) * (1 - al)) * dk),
            b = (int)((((c >> 10) & 31) * al + ((bg >> 10) & 31) * (1 - al)) * dk);
        palT[i] = COL(r, g, b);
    }
}
int stackEnter(void) { if (!pakUse(STACK_PAK, STACK_PAK_SIZE, STACK_PAK_ID)) return 0; stackThemeChanged(); return 1; }

static int newBody(float x, float y, float w, float h, int isStatic, int col) {
    int i = nB < MAXB ? nB++ : MAXB - 1;
    Body *b = &B[i]; memset(b, 0, sizeof *b);
    b->used = 1; b->p = v2(x, y); b->hw = w / 2; b->hh = h / 2; b->fric = 1.0f; b->col = col;
    setMass(b, isStatic);
    return i;
}
static float baseAngle(void) { return horizontal ? 1.5707963f : 0; }
static float topY(void) { float t = PLATE_Y; for (int i = 0; i < nPlaced; i++) if (B[placed[i]].p.y < t) t = B[placed[i]].p.y; return t; }
static void spawn(void) {
    int col = nPlaced % 3;
    float y = topY() - ML * 1.4f;
    // the website shrinks the box onto the visible marker (94% of its width, 98% of its length)
    float sw = col == 0 ? ST_M0_W : col == 1 ? ST_M1_W : ST_M2_W, bw = sw * 0.94f; if (bw < 11.5f) bw = 11.5f;
    cur = newBody(CW / 2, y, bw, ML * 0.98f, 1, col);
    B[cur].a = baseAngle(); B[cur].held = 0; B[cur].dropped = 0;
    haveTarget = 0;
}
static void startGame(void) {
    nB = 0; nArb = 0; nPlaced = 0; score = 0; camY = 0; camScale = 1; state = ST_PLAY; overT = 0; physGrav = GRAV;
    newBody(CW / 2, PLATE_Y + MW * 0.4f, PLATE_W, MW * 0.8f, 1, -1);     // the plate
    spawn();
    screen = S_STACK;
}
static void gameOver(void) {
    if (state == ST_OVER) return;
    state = ST_OVER; overT = 50;
    // let the top of the tower come down (the rest stays where it was buried)
    int from = nPlaced - 8 < 0 ? 0 : nPlaced - 8;
    for (int i = from; i < nPlaced; i++) setMass(&B[placed[i]], 0);
    if (cur >= 0 && !B[cur].dropped) setMass(&B[cur], 0);
    for (int i = 0; i < 4; i++) sfxPlop();
    newBest = score > sv.arcadeBest[ARC_STACK];
    if (newBest) sv.arcadeBest[ARC_STACK] = score;
    sv.arcadePlays[ARC_STACK]++;
    int c = score / 2; if (c > 20) c = 20;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite();
}
static float clampX(float x) { float h = (horizontal ? ML : MW) * 0.5f; return x < h ? h : x > CW - h ? CW - h : x; }
void updateStack(void) {
    if (state == ST_PLAY && cur >= 0 && B[cur].held && haveTarget) {        // the held marker follows your finger
        Body *b = &B[cur];
        b->p.x += (target.x - b->p.x) * 0.45f; b->p.y += (target.y - b->p.y) * 0.45f;   // follows your finger quickly
        b->a = baseAngle(); b->v = v2(0, 0); b->w = 0;
    }
    physGrav = state == ST_OVER ? GRAV * 2.5f : GRAV;
    step(1.0f / 60);
    if (state == ST_OVER) step(1.0f / 60);                                     // the website's snappier collapse
    if (state == ST_PLAY && cur >= 0 && B[cur].dropped) {
        Body *b = &B[cur];
        if (b->p.y > PLATE_Y + ML * 0.8f) gameOver();
        else {
            float sp = fsqrt(b->v.x * b->v.x + b->v.y * b->v.y) / 21.6f + fabs_(b->w) * 0.5f;
            if (sp < 0.4f) b->settle++; else b->settle = 0;
            if (b->settle > 4) {                                   // settled: next marker straight away
                placed[nPlaced++] = cur; score = nPlaced; sfxPlop();
                if (nPlaced > LOOSE) setMass(&B[placed[nPlaced - LOOSE - 1]], 1);   // buried: lock it in place
                cur = -1; spawn();
            }
        }
    }
    if (state == ST_PLAY)
        for (int i = 0; i < nPlaced; i++) { Body *b = &B[placed[i]];
            if (b->p.y > PLATE_Y + ML * 1.1f || b->p.x < CW / 2 - PLATE_W - ML || b->p.x > CW / 2 + PLATE_W + ML) { gameOver(); break; } }
    float ty = topY(); if (cur >= 0 && B[cur].dropped && B[cur].p.y < ty) ty = B[cur].p.y;
    if (state == ST_OVER) {                                                    // pull back to see the whole fall
        float mn = ty, mx = PLATE_Y;
        for (int i = 0; i < nB; i++) if (B[i].used) { if (B[i].p.y < mn) mn = B[i].p.y; if (B[i].p.y > mx && B[i].p.y < PLATE_Y + 300) mx = B[i].p.y; }
        float Hf = mx - mn + ML * 2.4f; if (Hf < CH * 0.5f) Hf = CH * 0.5f;
        float ts = CH * 0.82f / Hf;      // (pulls back faster too) ts = ts < 0.4f ? 0.4f : ts > 1 ? 1 : ts;
        camScale += (ts - camScale) * 0.15f; camY += ((CH * 0.5f - (mn + mx) / 2 * camScale) - camY) * 0.15f;
        if (overT > 0 && --overT == 0) screen = S_STACK_OVER;
    } else {
        float t = CH * 0.28f - ty; if (t < 0) t = 0;
        camScale += (1 - camScale) * 0.2f; camY += (t - camY) * 0.2f;
    }
}

// ── input ─────────────────────────────────────────────────────────────────
#define ORB_X 22
#define ORB_Y 168
static V2 toWorld(int sx, int gy) { return v2(CW / 2 + (sx - CW / 2) / camScale, (gy - camY) / camScale); }
void inputStack(void) {
    if (screen == S_STACK_PAUSE) {
        if (kDown & KEY_START) screen = S_STACK;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_STACK_PAUSE; return; }
    if (kDown & KEY_X) musicToggle();
    if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    if (state != ST_PLAY || cur < 0 || B[cur].dropped) return;
    Body *b = &B[cur];
    int turn = (kDown & (KEY_L | KEY_R)) != 0;
    if (kDown & KEY_TOUCH) { int dx = tX - ORB_X, dy = tY - ORB_Y; if (dx * dx + dy * dy <= 18 * 18) turn = 1; }
    if (turn) { horizontal = !horizontal; b->a = baseAngle(); return; }
    // never let the held marker be pushed down into the tower (it's solid while held, and
    // pressing it into the stack would shove everything): keep its underside above the top
    float lowest = PLATE_Y - ML * 0.42f, half = horizontal ? b->hw : b->hh;
    for (int i = 0; i < nPlaced; i++) { Body *p = &B[placed[i]];
        float ext = fabs_(fcos(p->a)) * p->hh + fabs_(fsin(p->a)) * p->hw, top = p->p.y - ext - half - 2;
        if (fabs_(p->p.x - b->p.x) < ext + half + (horizontal ? b->hh : b->hw) && top < lowest) lowest = top; }
    if (kHeld & KEY_TOUCH) {
        int dx = tX - ORB_X, dy = tY - ORB_Y; if (dx * dx + dy * dy <= 18 * 18) return;
        V2 p = toWorld(tX, tY + SH);
        b->held = 1; target = v2(clampX(p.x), p.y < lowest ? p.y : lowest); haveTarget = 1;
    } else if (kHeld & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)) {          // buttons: steer it
        if (!haveTarget) { target = b->p; haveTarget = 1; }
        b->held = 1;
        if (kHeld & KEY_LEFT) target.x -= 4.5f;
        if (kHeld & KEY_RIGHT) target.x += 4.5f;
        if (kHeld & KEY_UP) target.y -= 4.5f;
        if (kHeld & KEY_DOWN) target.y += 4.5f;
        target.x = clampX(target.x); if (target.y > lowest) target.y = lowest;
    }
    if ((kUp & KEY_TOUCH) || (kDown & KEY_A)) {                                 // let go: it drops
        b->held = 0; b->dropped = 1; setMass(b, 0); b->v = v2(0, 0); b->w = 0;
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
FAST static void drawBg(void) {
    // the window tile, scrolling at the website's slow parallax (0.16 of the camera)
    const u8 *img = stBgImg(); int H = ST_WIN_H;
    int off = ((int)(camY * 0.16f) % H + H) % H;
    for (int gy = 0; gy < 2 * SH; gy++) {
        int sy = ((gy - off) % H + H) % H; const u8 *s = &img[sy * SW];
        u16 *d = gy < SH ? &bufTop[gy * SW] : &bufBot[(gy - SH) * SW];
        for (int x = 0; x < SW; x += 2) { d[x] = palT[s[x]]; d[x + 1] = palT[s[x + 1]]; }
    }
    if (sv.theme == 1)                                                         // CARTOON: ben-day dots
        for (int gy = 4; gy < 2 * SH; gy += 8) for (int x = (gy / 8 % 2) * 4; x < SW; x += 8) { gpx(x, gy, COL(10, 10, 10)); gpx(x + 1, gy, COL(10, 10, 10)); gpx(x, gy + 1, COL(10, 10, 10)); gpx(x + 1, gy + 1, COL(10, 10, 10)); }
    else if (sv.theme == 4)                                                    // NEON: a faint grid
        for (int gy = 0; gy < 2 * SH; gy++) for (int x = 0; x < SW; x++) if (x % 26 == 0 || gy % 26 == 0) gpx(x, gy, COL(2, 10, 9));
}
static void sx_(float x, float y, int *ox, int *oy) { *ox = (int)(CW / 2 + (x - CW / 2) * camScale); *oy = (int)(y * camScale + camY); }
static void drawPlate(void) {
    int px, py; sx_(CW / 2, PLATE_Y, &px, &py);
    int rx = (int)(PLATE_W / 2 * camScale), ry = (int)(MW * 0.55f * camScale);
    int t = sv.theme;
    u16 fill = t == 4 ? COL(5, 1, 4) : (t == 3 || t == 5) ? COL(29, 22, 9) : t == 2 ? COL(5, 4, 11) : t == 1 ? WHITE : COL(25, 26, 28);
    u16 edge = t == 4 ? COL(31, 5, 22) : (t == 3 || t == 5) ? COL(31, 30, 24) : t == 2 ? COL(22, 13, 31) : t == 1 ? BLACK : COL(18, 19, 21);
    for (int j = -ry - 1; j <= ry + 1; j++) for (int i = -rx - 1; i <= rx + 1; i++) {
        float e = (float)(i * i) / (rx * rx) + (float)(j * j) / (ry * ry);
        if (e <= 1) gpx(px + i, py + j, e > 0.82f ? edge : fill);
    }
}
static const u16 *stSpr(int col, int *w) {
    int c = sv.theme == 1;
    if (col == 0) { *w = c ? ST_C0_W : ST_M0_W; return c ? st_c0 : st_m0; }
    if (col == 1) { *w = c ? ST_C1_W : ST_M1_W; return c ? st_c1 : st_m1; }
    *w = c ? ST_C2_W : ST_M2_W; return c ? st_c2 : st_m2;
}
void drawStack(void) {
    char s[24];
    drawBg();
    gClipLo = 0; gClipHi = 2 * SH;
    drawPlate();
    for (int i = 0; i < nB; i++) { Body *b = &B[i]; if (!b->used || b->col < 0) continue;
        int x, y; sx_(b->p.x, b->p.y, &x, &y);
        if (y < -60 || y > 2 * SH + 60) continue;
        int w; const u16 *sp = stSpr(b->col, &w);
        drawMarkerFx(sp, w, (int)ML, x, y, b->a, camScale, b->col);
    }
    sprintf(s, "%d", score); textC(bufTop, 10, s, sv.theme == 1 ? BLACK : WHITE, 2);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_STACK]); textC(bufTop, 44, s, COL(15, 27, 21), 1);
    if (state == ST_PLAY && cur >= 0 && !B[cur].dropped) textC(bufBot, SH - 16, "drag the marker - release to drop", sv.theme == 1 ? BLACK : WHITE, 1);
    // the turn button (bottom-left, as on the website)
    if (state == ST_PLAY) {
        for (int j = -15; j <= 15; j++) for (int i = -15; i <= 15; i++) if (i * i + j * j <= 225) { gdark(ORB_X + i, ORB_Y + SH + j); gdark(ORB_X + i, ORB_Y + SH + j); }
        for (int k = -6; k <= 6; k++) for (int q = -1; q <= 1; q++) gpx(ORB_X + (horizontal ? k : q), ORB_Y + SH + (horizontal ? q : k), COL(15, 27, 21));
    }
    if (state == ST_OVER) { textC(bufTop, 140, "TIMBER!", COL(31, 26, 9), 2); }
    if (screen == S_STACK_PAUSE) {
        textC(bufTop, 90, "PAUSED", YELLOW, 2);
        textC(bufBot, 60, "START  resume", WHITE, 1);
        textC(bufBot, 80, "SELECT  quit to the arcade", WHITE, 1);
    }
}

// ── menu and results ──────────────────────────────────────────────────────
static Btn SB[3];
void drawStackMenu(void) {
    char s[32];
    fillScreen(bufTop, DARK);
    textC(bufTop, 20, "QU33PH", GOLD, 2);
    textC(bufTop, 52, "STACK", GOLD, 2);
    textC(bufTop, 96, "drag & drop - build the tower", WHITE, 1);
    textC(bufTop, 114, "L/R turns the marker", GREY, 1);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_STACK]); textC(bufTop, 140, s, COL(15, 27, 21), 1);
    coinCount(bufTop, 6, 176);
    fillScreen(bufBot, DARK);
    SB[0] = (Btn){ 38, 40, 180, 40, "START", 0, 0 }; SB[1] = (Btn){ 68, 100, 120, 30, "BACK", 0, 0 };
    drawBtns(bufBot, SB, 2, menuSel);
}
void inputStackMenu(void) {
    SB[0] = (Btn){ 38, 40, 180, 40, "START", 0, 0 }; SB[1] = (Btn){ 68, 100, 120, 30, "BACK", 0, 0 };
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(SB, 2, &menuSel, 1);
    if (h == 0) startGame();
    if (h == 1) goScreen(S_ARCADE);
}
void drawStackOver(void) {
    char s[32];
    drawStack();
    box(bufTop, 28, 70, 200, 100, COL(2, 2, 4), GOLD);
    textC(bufTop, 78, "TIMBER!", COL(31, 26, 9), 2);
    sprintf(s, "%d markers high", score); textC(bufTop, 112, s, WHITE, 1);
    sprintf(s, newBest ? "NEW BEST!  %d coins" : "BEST %d   %d coins", newBest ? coinsWon : sv.arcadeBest[ARC_STACK], coinsWon);
    if (newBest) sprintf(s, "NEW BEST!  +%d coins", coinsWon); else sprintf(s, "BEST %d   +%d coins", sv.arcadeBest[ARC_STACK], coinsWon);
    textC(bufTop, 132, s, newBest ? LIME : COL(15, 27, 21), 1);
    fillScreen(bufBot, DARK);
    SB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; SB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    drawBtns(bufBot, SB, 2, overSel);
}
void inputStackOver(void) {
    SB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; SB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    int h = btnInput(SB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) startGame();
    if (h == 1) goScreen(S_ARCADE);
}
