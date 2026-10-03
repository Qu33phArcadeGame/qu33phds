// pin.c — QU33PH PINBALL (the website's arcade pinball): the ball is a Qu33ph marker.
//
// The website's table art, cropped to the playfield so it fills both screens: bumpers and the
// MEGA target on top, the chair, slings, flippers and launch chute below. Its physics are the
// website's own (a flat logical table drawn in perspective onto the art), frame for frame:
// bumpers +100, slings +25, the chair +250 and a ride up to MEGA (+1000).
//
// CONTROLS: L / D-pad = left flipper, R / A = right flipper, or touch the left / right half
// of the bottom screen. To launch: A or tap. START pauses (SELECT quits). X music, Y sound.
#include "qu.h"
#include "assets_pin.h"

#define LW 1.0f
#define LH 1.5f
#define G 0.0006f
#define PERSP 0.5f
#define DAMP 0.62f
#define R 0.040f
#define RX (R * 0.62f)
#define RY (R * 1.35f)
static const float QTL[2] = { 0.25f, 0.43f }, QTR[2] = { 0.77f, 0.42f }, QBL[2] = { 0.23f, 0.805f }, QBR[2] = { 0.82f, 0.805f };
typedef struct { float x, y, r; int hit, sling; } Bump;
// an even, square set of four bumpers in two straight rows, centred on the field. The columns
// sit 0.27 apart so there's a clear 0.08 channel (wider than the marker ball) down the middle
// and down each side, so the ball can always get past them. (The website's are deliberately
// staggered; the art's own bumpers are painted out of the table and these are drawn instead.)
static Bump bumps[6] = { {0.355f,-0.05f,0.095f,0,0}, {0.625f,-0.05f,0.095f,0,0}, {0.355f,0.13f,0.095f,0,0}, {0.625f,0.13f,0.095f,0,0}, {0.22f,0.92f,0.09f,0,1}, {0.71f,0.92f,0.09f,0,1} };
static const float CH_X = 0.14f, CH_Y = 0.55f, CH_W = 0.22f, CH_H = 0.09f, MG_X = 0.70f, MG_Y = -0.17f, MG_W = 0.275f, MG_H = 0.16f;
static const float TRIS[2][7][2] = {
    { {0.000f,0.936f},{0.033f,0.789f},{0.092f,0.780f},{0.205f,0.957f},{0.198f,1.035f},{0.125f,1.041f},{0.003f,0.948f} },
    { {0.915f,0.891f},{0.870f,1.059f},{0.785f,1.074f},{0.715f,1.038f},{0.792f,0.822f},{0.877f,0.798f},{0.912f,0.876f} } };
static struct { float x, y, vx, vy, spin; int launched, toMega, inChute, color, upT, megaT, chkT; float chkY; } b;
enum { ST_READY, ST_PLAY, ST_OVER };
static int state, score, balls, seq, megaFlash, chairCD, msgT, coinsWon, newBest, overSel, menuSel, tl, tr;
static float fl, fr; static char msg[16];
static u16 palT[256];

void pinThemeChanged(void) { if (!pakIs(PIN_PAK)) return; for (int i = 0; i < 256; i++) palT[i] = themeTint(pb_pal[i], sv.theme); }
int pinEnter(void) { if (!pakUse(PIN_PAK, PIN_PAK_SIZE, PIN_PAK_ID)) return 0; pinThemeChanged(); return 1; }

// ── the website's perspective: the flat table onto the tilted quad of the art ─────
static float vp(float y) { float v = y / LH; if (v < -0.13f) v = -0.13f; if (v > 1.15f) v = 1.15f; return v / (1 + PERSP * (1 - v)); }
static void proj(float x, float y, float *sx, float *sy) {
    float u = x / LW, v = vp(y);
    float tx = QTL[0] + (QTR[0] - QTL[0]) * u, ty = QTL[1] + (QTR[1] - QTL[1]) * u;
    float bx = QBL[0] + (QBR[0] - QBL[0]) * u, by = QBL[1] + (QBR[1] - QBL[1]) * u;
    *sx = (tx + (bx - tx) * v) * PB_IMG_W - PB_OX; *sy = (ty + (by - ty) * v) * PB_IMG_H - PB_OY;
}
static float sizePx(float l, float y) { return l * PB_IMG_W * 0.52f * (0.45f + vp(y) * 1.15f); }

static void newBall(void) { memset(&b, 0, sizeof b); b.x = 1.10f; b.y = 1.14f; b.inChute = 1; b.color = seq++ % 2; b.chkY = -1; state = ST_READY; }
static void resetGame(void) { score = 0; balls = 3; seq = 0; msgT = 0; newBall(); screen = S_PIN; }
static void launch(void) { if (state == ST_READY && !b.launched) { b.vy = -0.055f; b.vx = -0.015f; b.launched = 1; state = ST_PLAY; } }
static void say(const char *t, int frames) { strcpy(msg, t); msgT = frames; }
static void endGame(void) {
    state = ST_OVER; say("GAME OVER", 999);
    newBest = score > sv.arcadeBest[ARC_PINBALL]; if (newBest) sv.arcadeBest[ARC_PINBALL] = score;
    sv.arcadePlays[ARC_PINBALL]++;
    int c = score / 100; if (c > 25) c = 25;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite(); screen = S_PIN_OVER;
}
static void flipper(float px, float py, float ang, float len, float active) {
    float tx = px + fcos(ang) * len, ty = py + fsin(ang) * len, dx = tx - px, dy = ty - py, L2 = dx * dx + dy * dy;
    float t = ((b.x - px) * dx + (b.y - py) * dy) / L2; t = t < 0 ? 0 : t > 1 ? 1 : t;
    float cx = px + dx * t, cy = py + dy * t, ox = b.x - cx, oy = b.y - cy, d = fsqrt(ox * ox + oy * oy); if (d < 1e-6f) d = 1e-6f;
    float thick = R + 0.022f;
    if (d < thick) {
        float nx = ox / d, ny = oy / d; b.x = cx + nx * thick; b.y = cy + ny * thick;
        float dot = b.vx * nx + b.vy * ny;
        if (dot < 0) { float m = active > 0.35f ? 1.8f : 0.5f; b.vx -= m * dot * nx; b.vy -= m * dot * ny; }
        b.vy -= active * 0.05f;
        if (active > 0.4f) sfxPlop();
    }
}
static int inPoly(float px, float py, const float (*p)[2], int n) {
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) { float xi = p[i][0], yi = p[i][1], xj = p[j][0], yj = p[j][1];
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / ((yj - yi) != 0 ? (yj - yi) : 1e-9f) + xi)) in = !in; }
    return in;
}
static void triCollide(void) {
    for (int t = 0; t < 2; t++) {
        const float (*p)[2] = TRIS[t];
        if (!inPoly(b.x, b.y, p, 7)) continue;
        float best = 1e9f, bcx = 0, bcy = 0;
        for (int i = 0, j = 6; i < 7; j = i++) {
            float ax = p[j][0], ay = p[j][1], dx = p[i][0] - ax, dy = p[i][1] - ay, L2 = dx * dx + dy * dy;
            float u = ((b.x - ax) * dx + (b.y - ay) * dy) / L2; u = u < 0 ? 0 : u > 1 ? 1 : u;
            float qx = ax + dx * u, qy = ay + dy * u, d = fsqrt((b.x - qx) * (b.x - qx) + (b.y - qy) * (b.y - qy));
            if (d < best) { best = d; bcx = qx; bcy = qy; }
        }
        float nx = b.x - bcx, ny = b.y - bcy, nl = fsqrt(nx * nx + ny * ny);
        if (nl < 1e-6f) { nx = b.x < 0.5f ? -1 : 1; ny = 0.3f; nl = fsqrt(nx * nx + ny * ny); }
        nx = -nx / nl; ny = -ny / nl;
        b.x = bcx + nx * R * 1.9f; b.y = bcy + ny * R * 1.9f;
        float dot = b.vx * nx + b.vy * ny; if (dot < 0) { b.vx -= 1.7f * dot * nx; b.vy -= 1.7f * dot * ny; }
        float out = b.vx * nx + b.vy * ny; if (out < 0.010f) { b.vx += nx * (0.010f - out); b.vy += ny * (0.010f - out); }
        b.vy += 0.004f; return;
    }
}
static void pinStep(void);
static int pinAcc;
void updatePin(void) {                                 // 4 physics steps every 3 frames: a livelier table
    pinStep(); if (state == ST_PLAY && ++pinAcc >= 3) { pinAcc = 0; pinStep(); }
}
static void pinStep(void) {
    fl += (tl - fl) * 0.5f; fr += (tr - fr) * 0.5f;
    if (msgT > 0 && msgT < 900) msgT--;
    if (megaFlash > 0) megaFlash--;
    for (int j = 0; j < 6; j++) if (bumps[j].hit > 0) bumps[j].hit--;
    if (state != ST_PLAY) return;
    b.vy += G; b.x += b.vx; b.y += b.vy; b.vx *= 0.99f; b.vy *= 0.996f;
    float mv = b.inChute ? 0.05f : 0.042f;
    if (b.vx > mv) b.vx = mv; if (b.vx < -mv) b.vx = -mv; if (b.vy > mv) b.vy = mv; if (b.vy < -mv) b.vy = -mv;
    b.spin += (fabsf_(b.vx) + fabsf_(b.vy)) * 2.4f;
    if (b.inChute) {
        if (b.x < 0.98f + RX) { b.x = 0.98f + RX; if (b.vx < 0) b.vx *= -0.3f; }
        if (b.x > 1.14f - RX) { b.x = 1.14f - RX; b.vx = -b.vx * DAMP; }
        if (b.y < 0.42f) { b.inChute = 0; b.x = 0.9f; b.vx = -0.02f; }
    } else {
        float lw, rw;
        if (b.y < 0.6f) { lw = 0.04f; rw = 0.94f; }
        else { float tf = (b.y - 0.6f) / 0.58f; tf = tf < 0 ? 0 : tf > 1 ? 1 : tf; float e = tf * tf * (3 - 2 * tf); lw = 0.04f + e * 0.15f; rw = 0.94f - e * 0.23f; }
        if (b.x < lw + RX) { b.x = lw + RX; if (b.vx < 0) b.vx = -b.vx * DAMP; }
        if (b.x > rw - RX) { b.x = rw - RX; if (b.vx > 0) b.vx = -b.vx * DAMP; }
    }
    if (b.y < 0.02f + RY) { b.y = 0.02f + RY; b.vy = -b.vy * DAMP; }
    if (chairCD > 0) chairCD--;
    if (b.toMega) { b.vx += ((MG_X + MG_W / 2) - b.x) * 0.006f; b.upT = 0; b.megaT++;
        if (b.megaT > 70 || (b.y > 0.42f && b.vy > 0.006f)) { b.toMega = 0; b.megaT = 0; } }
    if (b.y < 0.96f && !b.toMega) b.upT++; else b.upT = 0;
    int forceDown = b.upT > 135;                       // the website's anti-trap
    if (forceDown) { b.vy += 0.0035f; b.vx += (0.5f - b.x) * 0.0018f; b.vx *= 0.97f; }
    float spd = fabsf_(b.vx) + fabsf_(b.vy);
    if (!b.toMega && !b.inChute && b.y > 0.24f && b.y < 0.99f && spd < 0.010f) { b.vy += 0.0028f; b.vx += (0.5f - b.x) * 0.0007f; }
    if (!b.toMega && !b.inChute && b.y > 0.84f && (b.x < 0.34f || b.x > 0.62f)) b.vx += (0.5f - b.x) * 0.009f;
    if (++b.chkT >= 16) { if (!b.toMega && !b.inChute && b.y > 0.24f && b.y < 0.94f && (b.y - (b.chkY < 0 ? b.y : b.chkY)) < 0.02f) { b.vx *= 0.4f; if (b.vy < 0.016f) b.vy = 0.016f; b.vx += (0.5f - b.x) * 0.002f; } b.chkY = b.y; b.chkT = 0; }
    for (int i = 0; i < 6 && !forceDown && !b.toMega; i++) { Bump *p = &bumps[i];
        float qx = b.x - RX > p->x ? b.x - RX : b.x + RX < p->x ? b.x + RX : p->x, qy = b.y - RY > p->y ? b.y - RY : b.y + RY < p->y ? b.y + RY : p->y;
        float dx = qx - p->x, dy = qy - p->y, d = fsqrt(dx * dx + dy * dy); if (d < 1e-6f) d = 1;
        if (d < p->r) { float nx = dx / d, ny = dy / d, push = p->r - d; b.x += nx * push; b.y += ny * push;
            if (p->sling) { b.vx = (0.5f - p->x) * 0.05f; b.vy = 0.03f; }
            else { float dot = b.vx * nx + b.vy * ny; b.vx -= 2 * dot * nx; b.vy -= 2 * dot * ny; b.vx = b.vx * 0.5f + nx * 0.006f; b.vy = b.vy * 0.5f + ny * 0.006f; }
            p->hit = 8; score += p->sling ? 25 : 100; } }
    if (b.x > CH_X - RX && b.x < CH_X + CH_W + RX && b.y > CH_Y - RY && b.y < CH_Y + CH_H + RY && b.vy > -0.01f && chairCD <= 0) {
        chairCD = 22; b.y = CH_Y - RY; b.vx = 0.026f; b.vy = -0.078f; b.toMega = 1; b.megaT = 0; score += 250; say("TO MEGA!", 45); }
    if (b.toMega && b.y < 0.11f && b.x > MG_X - 0.05f && b.x < MG_X + MG_W + 0.05f) {
        score += 1000; megaFlash = 50; say("MEGA! +1000", 70); b.toMega = 0; b.megaT = 0; b.vy = 0.014f; b.vx = (0.5f - b.x) * 0.02f; sfxPlop(); }
    triCollide();
    flipper(0.22f, 1.02f, 0.5f - fl, 0.20f, fl);
    flipper(0.70f, 1.05f, (3.14159265f - 0.5f) + fr, 0.20f, fr);
    if (b.y > 1.18f) { if (--balls <= 0) endGame(); else newBall(); }
}

// ── input ─────────────────────────────────────────────────────────────────
void inputPin(void) {
    if (screen == S_PIN_PAUSE) {
        if (kDown & KEY_START) screen = S_PIN;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_PIN_PAUSE; return; }
    if (kDown & KEY_X) musicToggle();
    if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    if (state == ST_READY && (kDown & (KEY_A | KEY_TOUCH | KEY_UP))) { launch(); return; }
    int touchL = (kHeld & KEY_TOUCH) && tX < 128, touchR = (kHeld & KEY_TOUCH) && tX >= 128;
    int nl = (kHeld & (KEY_L | KEY_LEFT | KEY_DOWN)) || touchL, nr = (kHeld & (KEY_R | KEY_A | KEY_B)) || touchR;
    if ((nl && !tl) || (nr && !tr)) playAdpcm(pb_snd, PB_SND_LEN, 12000, 80);
    tl = nl; tr = nr;
}

// ── drawing ───────────────────────────────────────────────────────────────
static void ring(float cx, float cy, float rx, float ry, u16 c, int thick) {
    for (int a = 0; a < 64; a++) { float t = a * 6.2831853f / 64; int x = (int)(cx + rx * fcos(t)), y = (int)(cy + ry * fsin(t));
        for (int k = 0; k < thick; k++) gpx(x, y + k, c); }
}
static void drawFlipper(float px, float py, float ang, float len, int mirror) {
    float ax, ay, bx, by; proj(px, py, &ax, &ay); proj(px + fcos(ang) * len, py + fsin(ang) * len, &bx, &by);
    float dx = bx - ax, dy = by - ay, L = fsqrt(dx * dx + dy * dy) * 1.08f, a = fatan2r(dy, dx);
    int cart = sv.theme == 1; const u16 *s = cart ? pb_padc : pb_pad; int w = PB_PAD_W, h = cart ? PB_PADC_H : PB_PAD_H;
    (void)mirror;
    drawMarkerFx(s, w, h, (int)((ax + bx) / 2), (int)((ay + by) / 2), a, L / w, 2);
}
void drawPin(void) {
    char s[32];
    drawIndexed(pb_table, palT);
    gClipLo = 0; gClipHi = 2 * SH;
    // the bumpers: drawn as the art's glowing cylinders, back row first (cyan / pink, alternating)
    for (int i = 0; i < 4; i++) { Bump *p = &bumps[i];
        float x, y; proj(p->x, p->y, &x, &y); float rx = sizePx(p->r, p->y) * 1.3f, ry = rx * 0.52f, hgt = ry * 1.25f;   // (drawn a little larger than the hit circle, like the art)
        int cyan = (i == 0 || i == 3); u16 rim = cyan ? COL(7, 31, 29) : COL(31, 7, 13), rimD = cyan ? COL(2, 14, 14) : COL(14, 2, 6);
        if (p->hit > 0) rim = WHITE;
        for (int j = (int)-ry; j <= (int)(ry + hgt); j++) {           // body: the cylinder's side, darker toward the bottom
            float yy = j < 0 ? 0 : j > hgt ? hgt : j; float k = j < hgt ? 1 : 1 - ((j - hgt) / ry) * ((j - hgt) / ry);
            if (k < 0) continue; int half = (int)(rx * fsqrt(k));
            u16 c = j > hgt - 2 ? rim : COL(6 - (int)(yy / hgt * 3), 6 - (int)(yy / hgt * 3), 12 - (int)(yy / hgt * 5));   // glossy side, lit rim at its foot
            for (int q = -half; q <= half; q++) gpx((int)x + q, (int)y + j, c);
        }
        for (int j = (int)-ry; j <= (int)ry; j++) {                    // the top: dark cap with a bright ring
            float k = 1 - (j / ry) * (j / ry); if (k < 0) continue; int half = (int)(rx * fsqrt(k)), inner = (int)(rx * 0.78f * fsqrt(k));
            for (int q = -half; q <= half; q++) { int a = q < 0 ? -q : q; gpx((int)x + q, (int)y + j, a > inner ? rim : (a > inner - 2 ? rimD : COL(2, 3, 7))); }
        }
    }
    {   // the MEGA target
        float ax, ay, bx, by, cx, cy, dx, dy; proj(MG_X, MG_Y, &ax, &ay); proj(MG_X + MG_W, MG_Y, &bx, &by); proj(MG_X + MG_W, MG_Y + MG_H, &cx, &cy); proj(MG_X, MG_Y + MG_H, &dx, &dy);
        u16 c = megaFlash > 0 ? COL(31, 30, 22) : COL(31, 26, 9);
        for (int k = 0; k <= 20; k++) { float t = k / 20.0f;
            gpx((int)(ax + (bx - ax) * t), (int)(ay + (by - ay) * t), c); gpx((int)(dx + (cx - dx) * t), (int)(dy + (cy - dy) * t), c);
            gpx((int)(ax + (dx - ax) * t), (int)(ay + (dy - ay) * t), c); gpx((int)(bx + (cx - bx) * t), (int)(by + (cy - by) * t), c); }
        if (megaFlash > 0) for (int y = (int)ay; y < (int)dy; y++) for (int x = (int)ax; x < (int)bx; x++) if ((x ^ y) & 1) gpx(x, y, COL(31, 26, 9));
        gtext((int)((ax + bx) / 2) - textW("MEGA", 1) / 2, (int)ay - 15, "MEGA", COL(31, 26, 9), 1);
    }
    drawFlipper(0.22f, 1.02f, 0.5f - fl, 0.20f, 1);
    drawFlipper(0.70f, 1.05f, (3.14159265f - 0.5f) + fr, 0.20f, 0);
    if (state != ST_OVER) { float x, y; proj(b.x, b.y, &x, &y); float sc = sizePx(RY * 2, b.y) / PB_M0_H;
        drawMarkerFx(b.color ? pb_m1 : pb_m0, PB_M0_W, PB_M0_H, (int)x, (int)y, b.spin, sc, b.color ? 1 : 0); }
    sprintf(s, "SCORE %d", score); text(bufTop, 6, 4, s, WHITE, 1);
    sprintf(s, "BALLS %d", balls); text(bufTop, SW - 6 - textW(s, 1), 4, s, WHITE, 1);
    sprintf(s, "HIGH %d", sv.arcadeBest[ARC_PINBALL]); textC(bufTop, 4, s, COL(17, 28, 31), 1);
    if (msgT > 0 && state != ST_OVER) textC(bufTop, 150, msg, COL(31, 26, 9), 2);
    if (state == ST_READY) textC(bufBot, SH - 16, "A or tap to LAUNCH", WHITE, 1);
    else textS(bufBot, 6, SH - 12, "L / tap left", GREY), textS(bufBot, SW - 6 - textSW("R / tap right"), SH - 12, "R / tap right", GREY);
    if (screen == S_PIN_PAUSE) { textC(bufTop, 90, "PAUSED", YELLOW, 2); textC(bufTop, 126, "START resume - SELECT quit", WHITE, 1); }
}
static Btn PBB[2];
void drawPinMenu(void) {
    char s[32];
    fillScreen(bufTop, DARK);
    textC(bufTop, 6, "QU33PH PINBALL", GOLD, 2);
    textC(bufTop, 42, "The ball is a marker. 3 balls.", WHITE, 1);
    textC(bufTop, 60, "bumpers +100   slings +25", WHITE, 1);
    textC(bufTop, 76, "the chair +250 sends you up to", COL(31, 26, 9), 1);
    textC(bufTop, 92, "the MEGA target: +1000", COL(31, 26, 9), 1);
    textC(bufTop, 112, "L / R or tap a side to flip", GREY, 1);
    sprintf(s, "HIGH %d", sv.arcadeBest[ARC_PINBALL]); textC(bufTop, 140, s, COL(17, 28, 31), 1);
    coinCount(bufTop, 6, 176);
    fillScreen(bufBot, DARK);
    PBB[0] = (Btn){ 38, 40, 180, 40, "PLAY", 0, 0 }; PBB[1] = (Btn){ 68, 100, 120, 30, "BACK", 0, 0 };
    drawBtns(bufBot, PBB, 2, menuSel);
}
void inputPinMenu(void) {
    PBB[0] = (Btn){ 38, 40, 180, 40, "PLAY", 0, 0 }; PBB[1] = (Btn){ 68, 100, 120, 30, "BACK", 0, 0 };
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(PBB, 2, &menuSel, 1);
    if (h == 0) resetGame();
    if (h == 1) goScreen(S_ARCADE);
}
void drawPinOver(void) {
    char s[32];
    drawPin();
    box(bufTop, 28, 40, 200, 104, COL(2, 2, 4), GOLD);
    textC(bufTop, 48, "GAME OVER", GOLD, 2);
    sprintf(s, "SCORE %d", score); textC(bufTop, 84, s, WHITE, 1);
    sprintf(s, newBest ? "NEW HIGH!  +%d coins" : "HIGH %d   +%d coins", newBest ? coinsWon : sv.arcadeBest[ARC_PINBALL], coinsWon);
    if (!newBest) sprintf(s, "HIGH %d   +%d coins", sv.arcadeBest[ARC_PINBALL], coinsWon);
    textC(bufTop, 104, s, newBest ? LIME : COL(17, 28, 31), 1);
    fillScreen(bufBot, DARK);
    PBB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; PBB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    drawBtns(bufBot, PBB, 2, overSel);
}
void inputPinOver(void) {
    PBB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; PBB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    int h = btnInput(PBB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) resetGame();
    if (h == 1) goScreen(S_ARCADE);
}
