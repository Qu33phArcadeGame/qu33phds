// fidget.c — FIDGET QU33PH (the website's fidget.html): spinner air hockey.
//
// The field fills both screens with its centre line on the gap between them, so the top
// screen is the CPU's (or player 2's) half and the bottom (touch) screen is yours. Knock the
// ball off the far end to score; first to 3, 5 or 7. Physics, CPU, kicks and timings are the
// website's, in the website's own units (UN = the field's width).
//
// CONTROLS: drag your spinner with the stylus, or move it with the D-pad. In 2 PLAYER the top
// spinner moves with X (up) B (down) Y (left) A (right). START pauses (SELECT quits).
#include "qu.h"
#include "assets_fidget.h"

#define CW 256.0f
#define CH 384.0f
#define CY 192.0f                                  // the centre line: the gap between the screens
#define UN 256.0f
#define R_BALL (UN * 0.052f)
#define R_SPIN (UN * 0.13f)
enum { ST_PLAY, ST_GOAL, ST_WIN };
typedef struct { float x, y, px, py, vx, vy, spin; int touching; } Spin;
static Spin s1, s2;
static struct { float x, y, vx, vy, rot; } ball;
static int state, goalT, fScore1, fScore2, mode2p, winGoals = 3, c1, menuSel, overSel, coinsWon, youWon;
static char msg[16], winner[16];
static u16 palT[256];

static const u8 *fdField(void) {
    switch (sv.theme) { case 1: return fd_field1; case 2: return fd_field2; case 3: return fd_field3; case 4: return fd_field4; case 5: return fd_field5; default: return fd_field0; }
}
static const u16 *fpal(void) {
    switch (sv.theme) { case 1: return fd_pal1; case 2: return fd_pal2; case 3: return fd_pal3; case 4: return fd_pal4; case 5: return fd_pal5; default: return fd_pal0; }
}
// every theme has its own field art on the website, so the palette is used as it is
void fidgetThemeChanged(void) { if (!pakIs(FIDGET_PAK)) return; const u16 *p = fpal(); for (int i = 0; i < 256; i++) palT[i] = p[i] | 0x8000; }
int fidgetEnter(void) { if (!pakUse(FIDGET_PAK, FIDGET_PAK_SIZE, FIDGET_PAK_ID)) return 0; fidgetThemeChanged(); return 1; }

static void sfx(int which) {                          // 0 your spinner, 1 theirs, 2 out of the field
    int r = rand();
    if (which == 0) { const u8 *d[3] = { fs_hitc1, fs_hitc2, fs_hitc3 }; int l[3] = { FS_HITC1_LEN, FS_HITC2_LEN, FS_HITC3_LEN }; playAdpcm(d[r % 3], l[r % 3], 16000, 80); }
    else if (which == 1) { const u8 *d[3] = { fs_hitf1, fs_hitf2, fs_hitf3 }; int l[3] = { FS_HITF1_LEN, FS_HITF2_LEN, FS_HITF3_LEN }; playAdpcm(d[r % 3], l[r % 3], 16000, 80); }
    else { if (r & 1) playAdpcm(fs_oob1, FS_OOB1_LEN, 16000, 80); else playAdpcm(fs_oob2, FS_OOB2_LEN, 16000, 80); }
}
static void resetPositions(void) {
    ball.x = CW / 2; ball.y = CY; ball.vx = ball.vy = 0;
    s1 = (Spin){ CW / 2, CH * 0.80f, CW / 2, CH * 0.80f, 0, 0, s1.spin, 0 };
    s2 = (Spin){ CW / 2, CH * 0.20f, CW / 2, CH * 0.20f, 0, 0, s2.spin, 0 };
}
static void startGame(void) { fScore1 = fScore2 = 0; resetPositions(); state = ST_PLAY; msg[0] = 0; screen = S_FIDGET; }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static void place(Spin *sp, float x, float y, int bottom) {
    sp->x = clampf(x, R_SPIN, CW - R_SPIN);
    sp->y = bottom ? clampf(y, CY + R_SPIN * 0.6f, CH - R_SPIN) : clampf(y, R_SPIN, CY - R_SPIN * 0.6f);
}
static void collide(Spin *sp, int close) {
    float dx = ball.x - sp->x, dy = ball.y - sp->y, d = fsqrt(dx * dx + dy * dy), rr = R_BALL + R_SPIN * 0.82f;
    if (d < rr && d > 0.001f) {
        if (!sp->touching) { sp->touching = 1; sfx(close ? 0 : 1); }
        float nx = dx / d, ny = dy / d; ball.x = sp->x + nx * rr; ball.y = sp->y + ny * rr;
        float relv = sp->vx * nx + sp->vy * ny; if (relv < 0) relv = 0;
        float kick = relv + UN * 0.010f;
        ball.vx = nx * kick + sp->vx * 0.6f; ball.vy = ny * kick + sp->vy * 0.6f;
        float s = fsqrt(ball.vx * ball.vx + ball.vy * ball.vy), MX = UN * 0.075f;
        if (s > MX) { ball.vx *= MX / s; ball.vy *= MX / s; }
    } else sp->touching = 0;
}
static void scoreGoal(int who) {
    if (who == 1) { fScore1++; strcpy(msg, "GOAL!"); } else { fScore2++; strcpy(msg, mode2p ? "P2 SCORES" : "CPU SCORES"); }
    if (fScore1 >= winGoals || fScore2 >= winGoals) {
        youWon = fScore1 >= winGoals;
        strcpy(winner, mode2p ? (youWon ? "P1 WINS!" : "P2 WINS!") : (youWon ? "YOU WIN!" : "CPU WINS"));
        state = ST_WIN;
        sv.arcadePlays[ARC_FIDGET]++;
        int before = sv.coins;
        if (!mode2p && youWon) { sv.arcadeBest[ARC_FIDGET]++; addCoins(winGoals); }   // best = wins against the CPU
        coinsWon = sv.coins - before;
        saveWrite();
        screen = S_FIDGET_OVER;
        return;
    }
    state = ST_GOAL; goalT = 70;
}
void updateFidget(void) {
    float m1 = fsqrt(s1.vx * s1.vx + s1.vy * s1.vy), m2 = fsqrt(s2.vx * s2.vx + s2.vy * s2.vy);
    s1.spin += 0.25f + m1 * 0.02f; s2.spin += 0.25f + m2 * 0.02f;
    if (state == ST_GOAL) { if (--goalT <= 0) { resetPositions(); state = ST_PLAY; msg[0] = 0; } return; }
    if (state != ST_PLAY) return;
    s1.vx = s1.x - s1.px; s1.vy = s1.y - s1.py; s1.px = s1.x; s1.py = s1.y;
    if (mode2p) { s2.vx = s2.x - s2.px; s2.vy = s2.y - s2.py; s2.px = s2.x; s2.py = s2.y; }
    else {                                              // the website's CPU
        float tx, ty;
        if (ball.y < CY) { tx = ball.x; ty = ball.y - R_SPIN * 0.7f; } else { tx = CW / 2 + (ball.x - CW / 2) * 0.5f; ty = CH * 0.14f; }
        float adx = tx - s2.x, ady = ty - s2.y, ad = fsqrt(adx * adx + ady * ady); if (ad < 1e-3f) ad = 1;
        float step = UN * 0.0105f < ad ? UN * 0.0105f : ad;
        s2.px = s2.x; s2.py = s2.y; s2.x += adx / ad * step; s2.y += ady / ad * step;
        place(&s2, s2.x, s2.y, 0);
        s2.vx = s2.x - s2.px; s2.vy = s2.y - s2.py;
    }
    ball.x += ball.vx; ball.y += ball.vy; ball.rot += fsqrt(ball.vx * ball.vx + ball.vy * ball.vy) * 0.03f;
    ball.vx *= 0.99f; ball.vy *= 0.99f;
    if (fabsf_(ball.vx) < 0.03f) ball.vx = 0;
    if (fabsf_(ball.vy) < 0.03f) ball.vy = 0;
    if (ball.x < R_BALL) { ball.x = R_BALL; ball.vx = fabsf_(ball.vx) * 0.9f; }
    if (ball.x > CW - R_BALL) { ball.x = CW - R_BALL; ball.vx = -fabsf_(ball.vx) * 0.9f; }
    if (ball.y < R_BALL) { sfx(2); scoreGoal(1); return; }        // off the top end: you score
    if (ball.y > CH - R_BALL) { sfx(2); scoreGoal(2); return; }   // off the bottom end: they score
    collide(&s1, 1); collide(&s2, 0);
}

// ── input ─────────────────────────────────────────────────────────────────
void inputFidget(void) {
    if (screen == S_FIDGET_PAUSE) {
        if (kDown & KEY_START) screen = S_FIDGET;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_FIDGET_PAUSE; return; }
    if (!mode2p) {
        if (kDown & KEY_X) { sv.musicOn = !sv.musicOn; if (sv.musicOn) musicStart(); else musicStop(); }
        if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    }
    if (state != ST_PLAY && state != ST_GOAL) return;
    if (kHeld & KEY_TOUCH) place(&s1, tX, tY + SH, 1);           // the stylus: your spinner follows it
    else {
        float sp = 4.0f, dx = 0, dy = 0;                      // the D-pad: a steady glide
        if (kHeld & KEY_LEFT) dx -= sp;
        if (kHeld & KEY_RIGHT) dx += sp;
        if (kHeld & KEY_UP) dy -= sp;
        if (kHeld & KEY_DOWN) dy += sp;
        if (dx || dy) place(&s1, s1.x + dx, s1.y + dy, 1);
    }
    if (mode2p) {
        float sp = 4.0f, dx = 0, dy = 0;
        if (kHeld & KEY_Y) dx -= sp;
        if (kHeld & KEY_A) dx += sp;
        if (kHeld & KEY_X) dy -= sp;
        if (kHeld & KEY_B) dy += sp;
        if (dx || dy) place(&s2, s2.x + dx, s2.y + dy, 0);
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
static const u16 *SPR(int c) { return c ? fd_spin1 : fd_spin0; }
static u16 lineCol(int c) { return c ? COL(9, 18, 31) : COL(8, 28, 11); }
static void drawSpinner(Spin *sp, int c) { blitRot(SPR(c), FD_SPIN_W, FD_SPIN_H, (int)sp->x, (int)sp->y, sp->spin); }
static void drawField(void) {
    drawIndexed(fdField(), palT);
    gClipLo = 0; gClipHi = 2 * SH;
    grect(0, 0, SW, 5, lineCol(!c1));                      // top end: the top spinner's colour
    grect(0, 2 * SH - 5, SW, 5, lineCol(c1));              // bottom end: yours
    for (int x = 0; x < SW; x += 13) for (int k = 0; k < 8 && x + k < SW; k++) gdark(x + k, (int)CY);   // the dashed centre line
    for (int a = 0; a < 96; a += 2) { float t = a * 6.2831853f / 96; gdark((int)(CW / 2 + 31 * fcos(t)), (int)(CY + 31 * fsin(t))); }
}
void drawFidget(void) {
    char s[24];
    drawField();
    blitRot(fd_ball, FD_BALL_W, FD_BALL_H, (int)ball.x, (int)ball.y, ball.rot);
    drawSpinner(&s2, !c1); drawSpinner(&s1, c1);
    sprintf(s, "%d - %d", fScore1, fScore2);
    textC(bufTop, SH - 26, s, WHITE, 1);
    text(bufTop, 128 - 36 - textW(mode2p ? "P1" : "YOU", 1), SH - 26, mode2p ? "P1" : "YOU", COL(25, 31, 29), 1);
    text(bufTop, 128 + 36, SH - 26, mode2p ? "P2" : "CPU", COL(31, 25, 25), 1);
    if (state == ST_GOAL) textC(bufTop, 70, msg, WHITE, 2);
    if (screen == S_FIDGET_PAUSE) {
        textC(bufTop, 70, "PAUSED", YELLOW, 2);
        textC(bufBot, 70, "START  resume", WHITE, 1);
        textC(bufBot, 94, "SELECT  quit to the arcade", WHITE, 1);
    }
}

// ── the pick screen (the website's: mode, spinner colour, first to) ───────
static Btn FB[8];
static void menuLayout(void) {
    FB[0] = (Btn){ 6, 6, 120, 30, "1 PLAYER", mode2p ? 0 : LIME, 0 };   FB[1] = (Btn){ 130, 6, 120, 30, "2 PLAYER", mode2p ? LIME : 0, 0 };
    for (int i = 0; i < 3; i++) { FB[2 + i] = (Btn){ 20 + i * 76, 44, 70, 28, "", winGoals == 3 + i * 2 ? LIME : 0, 0 }; sprintf(FB[2 + i].label, "TO %d", 3 + i * 2); }
    FB[5] = (Btn){ 6, 82, 120, 52, "GREEN", COL(15, 31, 18), 0 };       FB[6] = (Btn){ 130, 82, 120, 52, "BLUE", COL(17, 25, 31), 0 };
    FB[7] = (Btn){ 68, 146, 120, 30, "BACK", 0, 0 };
}
void drawFidgetMenu(void) {
    char s[40];
    fillScreen(bufTop, DARK);
    textC(bufTop, 4, "FIDGET QU33PH", GOLD, 2);
    textC(bufTop, 36, mode2p ? "P1 - PICK YOUR SPINNER" : "PICK YOUR SPINNER", WHITE, 1);
    static float a; a += 0.03f;
    blitRot(fd_spin0, FD_SPIN_W, FD_SPIN_H, 76, 108, a);
    blitRot(fd_spin1, FD_SPIN_W, FD_SPIN_H, 180, 108, -a);
    textC(bufTop, 164, mode2p ? "P1 stylus/D-pad, P2 uses X B Y A" : "knock it off the far end to score", GREY, 1);
    sprintf(s, "WINS VS CPU %d", sv.arcadeBest[ARC_FIDGET]); text(bufTop, 6, 178, s, GREY, 1);
    coinCount(bufTop, SW - 50, 178);
    fillScreen(bufBot, DARK);
    menuLayout();
    drawBtns(bufBot, FB, 8, menuSel);
}
void inputFidgetMenu(void) {
    menuLayout();
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(FB, 8, &menuSel, 2);
    if (h == 0) mode2p = 0;
    if (h == 1) mode2p = 1;
    if (h >= 2 && h <= 4) winGoals = 3 + (h - 2) * 2;
    if (h == 5 || h == 6) { c1 = h - 5; startGame(); }
    if (h == 7) goScreen(S_ARCADE);
}
static Btn OB[3];
void drawFidgetOver(void) {
    char s[32];
    drawFidget();
    box(bufTop, 28, 40, 200, 104, COL(2, 2, 4), GOLD);
    textC(bufTop, 50, winner, COL(31, 26, 9), 2);
    sprintf(s, "%d - %d", fScore1, fScore2); textC(bufTop, 86, s, WHITE, 1);
    if (!mode2p) { sprintf(s, "%d COINS EARNED", coinsWon); textC(bufTop, 106, s, GOLD, 1); }
    OB[0] = (Btn){ 38, 30, 180, 34, "PLAY AGAIN", 0, 0 }; OB[1] = (Btn){ 38, 74, 180, 34, "CHANGE SETUP", 0, 0 }; OB[2] = (Btn){ 38, 118, 180, 34, "ARCADE", 0, 0 };
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, OB, 3, overSel);
}
void inputFidgetOver(void) {
    int h = btnInput(OB, 3, &overSel, 1);
    if (kDown & KEY_B) h = 2;
    if (h == 0) startGame();
    if (h == 1) goScreen(S_FIDGET_MENU);
    if (h == 2) goScreen(S_ARCADE);
}
