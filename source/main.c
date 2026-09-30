// ════════════════════════════════════════════════════════════════════════
//  QU33PH DS — the main Qu33ph game (arcade not included yet)
//
//  Same rules & physics as the website's main game, using its real art and
//  sounds. The two screens are one tall window onto the field: the bottom
//  (touch) screen shows the launch end, the top screen the far end, and the
//  view scrolls to follow your marker like the website's landscape camera.
//
//  Modes: 1 PLAYER (30 s + sudden death) · 2 PLAYER (pass the DS, 3/5/10 rounds)
//
//  CONTROLS
//   Touch ............ swipe up the table to throw (like the website)
//   D-pad ←/→ ........ aim            A (hold) ... charge power, let go to throw
//   B ................ cancel a charge
//   L / R ............ marker orientation (vertical / angled / flat)
//   START ............ pause          SELECT (paused) ... quit to menu
//   X ................ music on/off   Y ......... sound effects on/off
//   Menus: D-pad ↑/↓ + A, or tap
// ════════════════════════════════════════════════════════════════════════
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assets.h"

#define SW 256
#define SH 192
static u16 bufTop[SW * SH], bufBot[SW * SH];
static u16 *vramTop, *vramBot;
#define COL(r, g, b) (RGB15(r, g, b) | BIT(15))
#define WHITE  COL(31, 31, 31)
#define BLACK  COL(0, 0, 0)
#define YELLOW COL(31, 27, 4)
#define RED    COL(31, 6, 6)
#define LIME   COL(8, 31, 8)
#define GREY   COL(18, 18, 18)

// ── the website's world (420 x 900) and the realistic field's layout ──────
#define WORLD_H     900.0f
#define VX0         36.0f                 // world x shown at the left of the DS screen
#define K           (256.0f / 220.0f)     // DS pixels per world unit
#define LEFT_EDGE   77.7f                 // past this: PEEF
#define RIGHT_WALL  211.9f
#define FIELD_CX    144.8f
#define LAUNCH_Y    877.5f
#define END_LINE_Y  99.0f
#define START_LINE_Y 820.0f
#define REDDOT_X    217.9f
#define REDDOT_Y    159.8f
#define TOUCH_DIST  45.0f
#define VIEW_H      (384.0f / K)          // world height visible on both screens together
#define SWIPE_GAIN  2.2f                  // DS swipe → website swipe strength

typedef struct {
    float x, y, vx, vy, curve, rot, spin, life;
    int stopped, fallen, armed, deducted, megaTriggered, col, orient;
} Marker;
static Marker mk[3];
static int current;                        // markers thrown this set (0-3)

// orientation: 0 vertical, 1 angled, 2 flat — changes spin, start angle & throw sound
static int orient = 0;
static const char *ORIENT_NAME[3] = { "VERTICAL", "ANGLED", "FLAT" };

// ── game state ────────────────────────────────────────────────────────────
enum { MENU, PLAY, PAUSED, HANDOFF, RESULTS };
static int state = MENU, mode = 1, twoRounds = 5, menuSel = 0, resultsSel = 0;
static int frames, roundFrames, totalFrames, suddenDeath;
static int score2[2], best2, player, p2Round;   // scores are stored doubled (the game has half points)
static float chairX = FIELD_CX, chairY = 117.0f, chairR = 35.0f;
static float camY, camTarget;
static int musicOn = 1, sfxOn = 1, musicCh = -1;
static int frameCount;

// ── popups ("PEEF", "+3", "QU33PH") float up the field ────────────────────
typedef struct { float x, y; int life; char text[24]; u16 col; int big; } Popup;
static Popup pops[8];
// big centred message on the top screen (round results, MEGA, sudden death)
static char banner[2][24]; static int bannerT; static u16 bannerCol;
static void showBanner(const char *a, const char *b, u16 c) {
    strncpy(banner[0], a, 23); banner[0][23] = 0; strncpy(banner[1], b ? b : "", 23); banner[1][23] = 0;
    bannerT = 75; bannerCol = c;
}
static void popup(float x, float y, const char *t, u16 c, int big) {
    for (int i = 0; i < 8; i++) if (pops[i].life <= 0) {
        pops[i].x = x; pops[i].y = y; pops[i].life = 60; pops[i].col = c; pops[i].big = big;
        strncpy(pops[i].text, t, 23); pops[i].text[23] = 0; return;
    }
}

// ── maths ─────────────────────────────────────────────────────────────────
static float fsqrt(float v) { if (v <= 0) return 0; float x = v > 1 ? v : 1; for (int i = 0; i < 12; i++) x = 0.5f * (x + v / x); return x; }
static float fabsf_(float v) { return v < 0 ? -v : v; }
// sin/cos by table-free polynomial (accurate enough for drawing and aiming)
static float wrapPi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static float fsin(float a) { a = wrapPi(a); float y = 1.2732395f * a - 0.4052847f * a * fabsf_(a); return 0.225f * (y * fabsf_(y) - y) + y; }
static float fcos(float a) { return fsin(a + 1.5707963f); }
static float dist(float ax, float ay, float bx, float by) { return fsqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)); }
static int touching(Marker *a, Marker *b) { return dist(a->x, a->y, b->x, b->y) < TOUCH_DIST; }
static float frand(void) { return (rand() % 1000) / 1000.0f; }

// ── sound ─────────────────────────────────────────────────────────────────
static void play(const signed char *d, int len, int rate, int vol) {
    if (!sfxOn) return;
    soundPlaySample(d, SoundFormat_8Bit, len, rate, vol, 64, false, 0);
}
static void sfxThrow(void) {
    int r = rand();
    if (orient == 0) {
        const signed char *s[4] = { snd_vertical1, snd_vertical2, snd_vertical3, snd_vertical4 };
        int l[4] = { SND_VERTICAL1_LEN, SND_VERTICAL2_LEN, SND_VERTICAL3_LEN, SND_VERTICAL4_LEN };
        play(s[r % 4], l[r % 4], 16000, 110);
    } else if (orient == 1) {
        if (r & 1) play(snd_angled1, SND_ANGLED1_LEN, 16000, 110); else play(snd_angled2, SND_ANGLED2_LEN, 16000, 110);
    } else {
        if (r & 1) play(snd_horizontal1, SND_HORIZONTAL1_LEN, 16000, 110); else play(snd_horizontal2, SND_HORIZONTAL2_LEN, 16000, 110);
    }
}
static void sfxPeef(void) {
    if (orient == 0) play(snd_verticalpeef1, SND_VERTICALPEEF1_LEN, 16000, 120);
    else if (orient == 1) play(snd_angledpeef1, SND_ANGLEDPEEF1_LEN, 16000, 120);
    else play(snd_horizontalpeef1, SND_HORIZONTALPEEF1_LEN, 16000, 120);
}
static void sfxPlop(void) { play(snd_plop, SND_PLOP_LEN, 16000, 100); }
static void musicStart(void) {
    if (musicCh >= 0 || !musicOn) return;
    musicCh = soundPlaySample(snd_music, SoundFormat_8Bit, SND_MUSIC_LEN, SND_MUSIC_RATE, 70, 64, true, 0);
}
static void musicStop(void) { if (musicCh >= 0) { soundKill(musicCh); musicCh = -1; } }

// ── drawing: both screens are one tall canvas, 384 px high ────────────────
static inline void gpx(int x, int gy, u16 c) {              // gy: 0-191 top screen, 192-383 bottom
    if ((unsigned)x >= SW) return;
    if ((unsigned)gy < SH) bufTop[gy * SW + x] = c;
    else if ((unsigned)(gy - SH) < SH) bufBot[(gy - SH) * SW + x] = c;
}
static void grect(int x, int gy, int w, int h, u16 c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) gpx(x + i, gy + j, c); }
static int wsx(float wx) { return (int)((wx - VX0) * K); }        // world → screen x
static int wsy(float wy) { return (int)(wy * K - camY * K); }       // world → global screen y (0-383)

// sprite drawn rotated about its centre (cx, gy centre); transparent pixels skipped.
// Whole-number (16.16 fixed-point) maths only: the DS has no floating-point
// hardware, so the per-pixel work must be integer adds to hold 60 fps.
static void blitRot(const u16 *spr, int w, int h, int cx, int cy, float ang) {
    int ci = (int)(fcos(ang) * 65536.0f), si = (int)(fsin(ang) * 65536.0f);
    int r = (int)(fsqrt((float)(w * w + h * h)) / 2) + 1;
    for (int dy = -r; dy <= r; dy++) {
        int gy = cy + dy; if (gy < 0 || gy >= 2 * SH) continue;
        // source position of the leftmost pixel in this row, then step along it
        int sxf = ci * (-r) + si * dy + (w << 15);
        int syf = -si * (-r) + ci * dy + (h << 15);
        for (int dx = -r; dx <= r; dx++, sxf += ci, syf -= si) {
            int gx = cx + dx; if ((unsigned)gx >= SW) continue;
            int ix = sxf >> 16, iy = syf >> 16;
            if ((unsigned)ix >= (unsigned)w || (unsigned)iy >= (unsigned)h) continue;
            u16 p = spr[iy * w + ix];
            if (p & 0x8000) gpx(gx, gy, p);
        }
    }
}
static void blit(u16 *buf, const u16 *spr, int w, int h, int x, int y) {       // unrotated, one screen
    for (int j = 0; j < h; j++) { int yy = y + j; if (yy < 0 || yy >= SH) continue;
        for (int i = 0; i < w; i++) { int xx = x + i; if (xx < 0 || xx >= SW) continue; u16 p = spr[j * w + i]; if (p & 0x8000) buf[yy * SW + xx] = p; } }
}
// text from the built-in DejaVu Sans Bold glyphs, with a dark outline so it reads on the photo
static int textW(const char *t, int sc) { int w = 0; for (; *t; t++) { int ch = *t; if (ch < 32 || ch > 126) ch = '?'; w += font_w[ch - 32] * sc; } return w; }
static void glyphs(u16 *buf, int gyMode, int x, int y, const char *t, u16 col, int sc) {
    for (; *t; t++) {
        int ch = *t; if (ch < 32 || ch > 126) ch = '?';
        const u16 *rows = &font_rows[(ch - 32) * FONT_H];
        for (int j = 0; j < FONT_H; j++) for (int i = 0; i < 16; i++) if (rows[j] & (1 << i))
            for (int a = 0; a < sc; a++) for (int b = 0; b < sc; b++) {
                int px = x + i * sc + a, py = y + j * sc + b;
                if (gyMode) gpx(px, py, col);
                else if ((unsigned)px < SW && (unsigned)py < SH) buf[py * SW + px] = col;
            }
        x += font_w[ch - 32] * sc;
    }
}
static void text(u16 *buf, int x, int y, const char *t, u16 col, int sc) {       // one screen, outlined
    glyphs(buf, 0, x - 1, y, t, BLACK, sc); glyphs(buf, 0, x + 1, y, t, BLACK, sc);
    glyphs(buf, 0, x, y - 1, t, BLACK, sc); glyphs(buf, 0, x, y + 1, t, BLACK, sc);
    glyphs(buf, 0, x, y, t, col, sc);
}
static void textC(u16 *buf, int y, const char *t, u16 col, int sc) { text(buf, (SW - textW(t, sc)) / 2, y, t, col, sc); }
static void gtext(int x, int gy, const char *t, u16 col, int sc) {               // across both screens
    glyphs(0, 1, x - 1, gy, t, BLACK, sc); glyphs(0, 1, x + 1, gy, t, BLACK, sc);
    glyphs(0, 1, x, gy - 1, t, BLACK, sc); glyphs(0, 1, x, gy + 1, t, BLACK, sc);
    glyphs(0, 1, x, gy, t, col, sc);
}
static void fillScreen(u16 *buf, u16 c) { for (int i = 0; i < SW * SH; i++) buf[i] = c; }
static void box(u16 *buf, int x, int y, int w, int h, u16 fill, u16 edge) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int xx = x + i, yy = y + j; if ((unsigned)xx >= SW || (unsigned)yy >= SH) continue;
        int e = (i < 2 || j < 2 || i >= w - 2 || j >= h - 2);
        buf[yy * SW + xx] = e ? edge : fill;
    }
}
static void scoreStr(char *o, int doubled) {                      // 7 → "3.5"
    if (doubled & 1) sprintf(o, "%d.5", doubled / 2); else sprintf(o, "%d", doubled / 2);
}

// ── rules (ported from the website) ───────────────────────────────────────
static const int ROUND_1P = 30, ROUND_2P = 15;
static void resetSet(void) { current = 0; memset(mk, 0, sizeof mk); }
static void startTurn(void) {
    resetSet(); roundFrames = 0; suddenDeath = 0;
    int rt = (mode == 1) ? ROUND_1P : ROUND_2P;
    totalFrames = (mode == 1) ? (rt * 3 / 2) * 60 : rt * 60;   // 1 player: sudden death for the last third
    chairX = FIELD_CX; chairY = 117.0f;
    camY = WORLD_H - VIEW_H; bannerT = 0;
    for (int i = 0; i < 8; i++) pops[i].life = 0;
    state = PLAY;
}
static void startGame(void) {
    score2[0] = score2[1] = 0; player = 0; p2Round = 1;
    startTurn();
}
static void addScore(int doubled) { score2[player] += doubled; }

static void throwMarker(float dx, float dy) {
    if (state != PLAY || current >= 3) return;
    Marker *m = &mk[current];
    memset(m, 0, sizeof *m);
    m->x = FIELD_CX; m->y = LAUNCH_Y;
    m->vx = dx * 0.2f; m->vy = dy * 0.2f; m->curve = dx * 0.0003f;
    m->rot = orient == 0 ? 1.0472f : orient == 1 ? 1.5708f : 0.0f;
    m->spin = orient == 0 ? 0.08f : orient == 1 ? 0.12f : 0.18f;
    m->col = current; m->orient = orient;
    current++;
    sfxThrow();
}

static void triggerMega(Marker *m) {
    m->x = REDDOT_X; m->y = REDDOT_Y; m->vx = m->vy = 0; m->stopped = 1;
    addScore(20);
    showBanner("MEGA QU33PH", "+10", YELLOW);
    sfxPlop();
}

static int calcRoundScore(void) {                  // doubled points
    Marker *g = &mk[0], *r = &mk[1], *b = &mk[2];
    if (!(g->armed && r->armed && b->armed)) return 0;
    int gr = touching(g, r), rb = touching(r, b), gb = touching(g, b);
    if (suddenDeath) {
        if (gr && rb && gb) return 20;
        if (rb || gb) return 10;
        if (gr) return 6;
        return 0;
    }
    if (gr && rb && gb) return 6;       // QU33PH: all three touching = 3 points
    if (rb || gb) return 4;             // blue touching red or green = 2
    if (gr) return 1;                   // green touching red = 0.5
    return 0;
}

static void endTurn(void);
static void tryAdvanceRound(void) {
    if (current < 3) return;
    for (int i = 0; i < 3; i++) if (!mk[i].stopped && !mk[i].fallen) return;
    Marker *g = &mk[0], *r = &mk[1], *b = &mk[2];
    if (!b->fallen && suddenDeath && b->y > chairY) {
        addScore(-4);
        popup(100, chairY + 60, "PEEF  -2  BLUE MISSED CHAIR", RED, 0);
        sfxPeef();
    }
    int rs = calcRoundScore();
    addScore(rs);
    char t[16], s[10]; scoreStr(s, rs); sprintf(t, "+%s", s);
    if (!g->fallen && !r->fallen && !b->fallen && touching(g, r) && touching(r, b) && touching(g, b)) {
        showBanner("QU33PH!", t, YELLOW);
        sfxPlop();
    } else showBanner(t, 0, WHITE);
    resetSet();
}

static void update(void) {
    roundFrames++;
    int rt = ((mode == 1) ? ROUND_1P : ROUND_2P) * 60;
    if (mode == 1 && roundFrames > rt && !suddenDeath) {
        suddenDeath = 1; chairX = FIELD_CX; chairY = END_LINE_Y + 90;
        showBanner("SUDDEN DEATH", "hit the chair!", RED);
    }
    if (roundFrames > totalFrames) { endTurn(); return; }

    for (int i = 0; i < current; i++) {
        Marker *m = &mk[i];
        if (m->stopped || m->fallen) continue;
        m->life += 1.0f / 60.0f;
        if (m->life >= 1.0f) { m->stopped = 1; m->vx = m->vy = 0; sfxPlop(); }
        m->vx += m->curve;
        m->x += m->vx; m->y += m->vy;
        if (!m->armed && m->y < START_LINE_Y) m->armed = 1;
        m->vx *= 0.94f; m->vy *= 0.94f;
        m->rot += m->spin; m->spin *= 0.96f;
        if (suddenDeath && !m->megaTriggered && dist(m->x, m->y, chairX, chairY) < chairR) {
            if (frand() < 0.25f) triggerMega(m);
            else {                                   // bounce off the chair (the website's gentle version)
                float dx = m->x - chairX, dy = m->y - chairY, d = dist(m->x, m->y, chairX, chairY); if (d < 0.01f) d = 1;
                float nx = dx / d, ny = dy / d;
                m->x = chairX + nx * chairR * 1.5f; m->y = chairY + ny * chairR * 1.5f;
                float relv = m->vx * nx + m->vy * ny;
                if (relv < 0) { m->vx -= 1.6f * relv * nx; m->vy -= 1.6f * relv * ny; }
                float side = (m->x < chairX) ? -1 : 1;
                m->vx += -ny * side * 1.8f; m->vy += nx * side * 1.8f;
                if (m->vy > -1) m->vy = -1 - frand() * 1.2f;
                if (m->vx < -3) m->vx = -3 + frand() * 1.5f;
                m->spin += (nx > 0 ? 1 : -1) * 1.1f;
            }
            m->megaTriggered = 1;
        }
        if (m->x < LEFT_EDGE) {
            if (!m->deducted) {
                int ded = suddenDeath ? 5 : 2;
                addScore(-2 * ded); m->deducted = 1;
                char t[12]; sprintf(t, "PEEF  -%d", ded);
                popup(LEFT_EDGE + 10, m->y < END_LINE_Y + 40 ? END_LINE_Y + 40 : m->y, t, RED, 1);
                sfxPeef();
            }
            m->fallen = 1;
        }
        if (m->x > RIGHT_WALL) { m->x = RIGHT_WALL; m->vx *= -0.4f; }
        if (m->y < END_LINE_Y) { m->y = END_LINE_Y; m->vy *= -0.2f; }
        if (!m->stopped && fabsf_(m->vx) < 0.05f && fabsf_(m->vy) < 0.05f) { m->stopped = 1; sfxPlop(); }
    }
    for (int i = 0; i < 8; i++) if (pops[i].life > 0) { pops[i].life--; pops[i].y -= 0.8f; }
    if (bannerT > 0) bannerT--;
    tryAdvanceRound();
}

static void endTurn(void) {
    if (mode == 1) {
        if (score2[0] > best2) best2 = score2[0];
        state = RESULTS; resultsSel = 0; return;
    }
    // 2 player: P1 then P2 each round
    if (player == 0) { player = 1; state = HANDOFF; return; }
    if (p2Round >= twoRounds) { state = RESULTS; resultsSel = 0; return; }
    p2Round++; player = 0; state = HANDOFF;
}

// ── camera: rests on the launch end, follows the lead marker up the table ─
static void camera(void) {
    float target = WORLD_H - VIEW_H;
    int lead = -1;
    for (int i = 0; i < current; i++) if (!mk[i].stopped && !mk[i].fallen && (lead < 0 || mk[i].y < mk[lead].y)) lead = i;
    if (lead >= 0) target = mk[lead].y - VIEW_H * 0.45f;
    if (target < 0) target = 0;
    if (target > WORLD_H - VIEW_H) target = WORLD_H - VIEW_H;
    camY += (target - camY) * (target > camY ? 0.14f : 0.22f);
    if (fabsf_(target - camY) < 0.4f) camY = target;
}

// ── aiming with the buttons (the website's aim marker + hold-to-charge) ───
static float aimAng = -1.5708f;      // straight up the table
static int charging; static float power; static int chargeT;

static void drawField(void) {
    int top = (int)(camY * K);
    if (top < 0) top = 0;
    if (top > FIELD_H - 2 * SH) top = FIELD_H - 2 * SH;
    memcpy(bufTop, field + top * SW, sizeof bufTop);
    memcpy(bufBot, field + (top + SH) * SW, sizeof bufBot);
    camY = top / K;
    if (suddenDeath) blitRot(chair, CHAIR_W, CHAIR_H, wsx(chairX), wsy(chairY), 0);
    for (int i = 0; i < current; i++) {
        Marker *m = &mk[i];
        if (m->fallen && m->x < LEFT_EDGE - 40) continue;
        const u16 *s = m->col == 0 ? mk_green : m->col == 1 ? mk_red : mk_blue;
        int w = m->col == 2 ? MK_BLUE_W : MK_GREEN_W, h = m->col == 2 ? MK_BLUE_H : MK_GREEN_H;
        blitRot(s, w, h, wsx(m->x), wsy(m->y), m->rot);
    }
    // aim guide (buttons)
    if (state == PLAY && current < 3 && (charging || chargeT > 0)) {
        int n = 14; float len = 40 + power * 110;
        for (int i = 1; i <= n; i++) {
            float t = (float)i / n;
            int x = wsx(FIELD_CX + fcos(aimAng) * len * t), y = wsy(LAUNCH_Y + fsin(aimAng) * len * t);
            grect(x - 1, y - 1, 3, 3, i * 3 <= (int)(power * n * 3) ? YELLOW : WHITE);
        }
    }
    for (int i = 0; i < 8; i++) if (pops[i].life > 0) {
        int sc = pops[i].big ? 2 : 1;
        gtext(wsx(pops[i].x), wsy(pops[i].y), pops[i].text, pops[i].col, sc);
    }
}

static void drawHUD(void) {
    char s[40], a[12], b[12];
    int left = (totalFrames - roundFrames + 59) / 60; if (left < 0) left = 0;
    // time bar across the top of the top screen (turns red in sudden death)
    int bw = SW * roundFrames / (totalFrames ? totalFrames : 1); if (bw > SW) bw = SW;
    for (int j = 0; j < 3; j++) for (int i = 0; i < bw; i++) bufTop[j * SW + i] = suddenDeath ? RED : LIME;
    sprintf(s, "TIME %d", left); text(bufTop, 6, 8, s, suddenDeath ? RED : WHITE, 1);
    if (mode == 1) { scoreStr(a, score2[0]); sprintf(s, "SCORE %s", a); text(bufTop, 6, 24, s, YELLOW, 1); }
    else {
        scoreStr(a, score2[0]); scoreStr(b, score2[1]);
        sprintf(s, "P1 %s   P2 %s", a, b); text(bufTop, 6, 24, s, YELLOW, 1);
        sprintf(s, "ROUND %d/%d  PLAYER %d", p2Round, twoRounds, player + 1); text(bufTop, 6, 40, s, WHITE, 1);
    }
    if (suddenDeath) textC(bufTop, 8, "SUDDEN DEATH", RED, 1);
    if (bannerT > 0) { textC(bufTop, 70, banner[0], bannerCol, 2); if (banner[1][0]) textC(bufTop, 104, banner[1], WHITE, 2); }
    // markers left in this set, and the orientation, on the bottom screen
    for (int i = current; i < 3; i++) {
        const u16 *sp = i == 0 ? mk_green : i == 1 ? mk_red : mk_blue;
        int w = i == 2 ? MK_BLUE_W : MK_GREEN_W, h = i == 2 ? MK_BLUE_H : MK_GREEN_H;
        blitRot(sp, w, h, 20 + (i - current) * 26, SH + 150, 1.5708f);
    }
    text(bufBot, SW - textW(ORIENT_NAME[orient], 1) - 6, SH - 18, ORIENT_NAME[orient], WHITE, 1);
    text(bufBot, SW - textW("L/R", 1) - 6, SH - 32, "L/R", GREY, 1);
}

// ── menus ─────────────────────────────────────────────────────────────────
static const char *MENU_ITEMS[] = { "1 PLAYER", "2 PLAYER", "ROUNDS", "MUSIC", "SOUND FX" };
#define MENU_N 5
static void drawMenu(void) {
    fillScreen(bufTop, COL(2, 2, 3));
    blit(bufTop, logo, LOGO_W, LOGO_H, (SW - LOGO_W) / 2, 4);
    textC(bufTop, 128, "Swipe or D-pad + A to throw", WHITE, 1);
    textC(bufTop, 144, "L / R  marker orientation", GREY, 1);
    textC(bufTop, 160, "START  pause", GREY, 1);
    if (best2) { char s[24], a[10]; scoreStr(a, best2); sprintf(s, "BEST  %s", a); textC(bufTop, 176, s, YELLOW, 1); }
    fillScreen(bufBot, COL(2, 2, 3));
    for (int i = 0; i < MENU_N; i++) {
        int y = 12 + i * 35, sel = (i == menuSel);
        box(bufBot, 28, y, 200, 30, sel ? COL(6, 6, 6) : BLACK, sel ? YELLOW : WHITE);
        char s[32];
        if (i == 2) sprintf(s, "2P ROUNDS:  %d", twoRounds);
        else if (i == 3) sprintf(s, "MUSIC:  %s", musicOn ? "ON" : "OFF");
        else if (i == 4) sprintf(s, "SOUND FX:  %s", sfxOn ? "ON" : "OFF");
        else sprintf(s, "%s", MENU_ITEMS[i]);
        text(bufBot, 128 - textW(s, 1) / 2, y + 8, s, sel ? YELLOW : WHITE, 1);
    }
}
static void menuActivate(int i) {
    if (i == 0) { mode = 1; startGame(); }
    else if (i == 1) { mode = 2; startGame(); state = HANDOFF; }
    else if (i == 2) twoRounds = twoRounds == 3 ? 5 : twoRounds == 5 ? 10 : 3;
    else if (i == 3) { musicOn = !musicOn; if (musicOn) musicStart(); else musicStop(); }
    else if (i == 4) sfxOn = !sfxOn;
}
static void drawResults(void) {
    char s[40], a[10], b[10];
    fillScreen(bufTop, COL(2, 2, 3));
    blit(bufTop, logo, LOGO_W, LOGO_H, (SW - LOGO_W) / 2, 2);
    if (mode == 1) {
        scoreStr(a, score2[0]); sprintf(s, "FINAL SCORE  %s", a); textC(bufTop, 132, s, YELLOW, 2);
        scoreStr(b, best2); sprintf(s, "BEST  %s", b); textC(bufTop, 168, s, WHITE, 1);
    } else {
        scoreStr(a, score2[0]); scoreStr(b, score2[1]);
        sprintf(s, "P1  %s     P2  %s", a, b); textC(bufTop, 132, s, WHITE, 1);
        const char *w = score2[0] > score2[1] ? "PLAYER 1 WINS!" : score2[1] > score2[0] ? "PLAYER 2 WINS!" : "IT'S A TIE!";
        textC(bufTop, 152, w, YELLOW, 2);
    }
    fillScreen(bufBot, COL(2, 2, 3));
    const char *it[2] = { "PLAY AGAIN", "MENU" };
    for (int i = 0; i < 2; i++) {
        int y = 50 + i * 50, sel = (i == resultsSel);
        box(bufBot, 38, y, 180, 36, sel ? COL(6, 6, 6) : BLACK, sel ? YELLOW : WHITE);
        text(bufBot, 128 - textW(it[i], 1) / 2, y + 11, it[i], sel ? YELLOW : WHITE, 1);
    }
}
static void drawHandoff(void) {
    char s[32];
    fillScreen(bufTop, COL(2, 2, 3));
    blit(bufTop, logo, LOGO_W, LOGO_H, (SW - LOGO_W) / 2, 6);
    sprintf(s, "PLAYER %d", player + 1); textC(bufTop, 136, s, YELLOW, 2);
    sprintf(s, "ROUND %d / %d", p2Round, twoRounds); textC(bufTop, 170, s, WHITE, 1);
    fillScreen(bufBot, COL(2, 2, 3));
    textC(bufBot, 70, "Pass the DS", WHITE, 1);
    textC(bufBot, 100, "TAP or A to start", YELLOW, 1);
}

// ── input ─────────────────────────────────────────────────────────────────
static int swiping, sx0, sy0, sx1, sy1;
static int hitBox(int tx, int ty, int x, int y, int w, int h) { return tx >= x && tx < x + w && ty >= y && ty < y + h; }

static void input(void) {
    scanKeys();
    int down = keysDown(), held = keysHeld(), up = keysUp();
    touchPosition t; touchRead(&t);
    if (down & KEY_X) { musicOn = !musicOn; if (musicOn) musicStart(); else musicStop(); }
    if (down & KEY_Y) sfxOn = !sfxOn;

    if (state == MENU) {
        if (down & KEY_UP) menuSel = (menuSel + MENU_N - 1) % MENU_N;
        if (down & KEY_DOWN) menuSel = (menuSel + 1) % MENU_N;
        if (down & (KEY_A | KEY_START)) menuActivate(menuSel);
        if (down & KEY_TOUCH) for (int i = 0; i < MENU_N; i++) if (hitBox(t.px, t.py, 28, 12 + i * 35, 200, 30)) { menuSel = i; menuActivate(i); }
        return;
    }
    if (state == RESULTS) {
        if (down & (KEY_UP | KEY_DOWN)) resultsSel ^= 1;
        int pick = -1;
        if (down & KEY_A) pick = resultsSel;
        if (down & KEY_B) pick = 1;
        if (down & KEY_TOUCH) for (int i = 0; i < 2; i++) if (hitBox(t.px, t.py, 38, 50 + i * 50, 180, 36)) pick = i;
        if (pick == 0) { startGame(); if (mode == 2) state = HANDOFF; }
        if (pick == 1) state = MENU;
        return;
    }
    if (state == HANDOFF) {
        if (down & (KEY_A | KEY_TOUCH | KEY_START)) startTurn();
        return;
    }
    if (state == PAUSED) {
        if (down & KEY_START) state = PLAY;
        if (down & KEY_SELECT) state = MENU;
        return;
    }
    // PLAY
    if (down & KEY_START) { state = PAUSED; return; }
    if (down & KEY_L) orient = (orient + 2) % 3;
    if (down & KEY_R) orient = (orient + 1) % 3;
    if (held & KEY_LEFT)  { aimAng -= 0.03f; chargeT = 90; }
    if (held & KEY_RIGHT) { aimAng += 0.03f; chargeT = 90; }
    if (aimAng < -2.6f) aimAng = -2.6f;
    if (aimAng > -0.55f) aimAng = -0.55f;
    if (down & KEY_A) { charging = 1; power = 0; }
    if (charging) {
        static int ph; ph++;
        float p = (ph % 100) / 50.0f; power = p < 1 ? p : 2 - p;      // fills, empties, fills…
        chargeT = 90;
        if (down & KEY_B) { charging = 0; }
        else if (up & KEY_A) {
            charging = 0; float pw = 70 + power * 160;                 // same range as the website's aim throw
            throwMarker(fcos(aimAng) * pw, fsin(aimAng) * pw);
        }
    }
    if (chargeT > 0 && !charging) chargeT--;
    // swipe: the whole drag from where it started to where you let go (like the website)
    if (down & KEY_TOUCH) { swiping = 1; sx0 = sx1 = t.px; sy0 = sy1 = t.py; }
    if (swiping && (held & KEY_TOUCH)) { sx1 = t.px; sy1 = t.py; }
    if (swiping && (up & KEY_TOUCH)) {
        swiping = 0;
        // the DS touch screen is far shorter than a phone, so a swipe is scaled
        // up: a normal DS flick throws as hard as a normal phone flick
        float dx = (sx1 - sx0) / K * SWIPE_GAIN, dy = (sy1 - sy0) / K * SWIPE_GAIN;
        if (dy < -8) throwMarker(dx, dy);
    }
}

int main(void) {
    // bottom (touch) screen = main engine, top = sub engine; both 16-bit bitmaps
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    int bgm = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int bgs = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vramBot = bgGetGfxPtr(bgm); vramTop = bgGetGfxPtr(bgs);
    lcdMainOnBottom();
    soundEnable();
    musicStart();

    while (1) {
        frameCount++;
        if (frameCount == 1) srand(0x51A);
        input();
        if (state == PLAY) { srand(rand() ^ frameCount); update(); camera(); drawField(); drawHUD(); }
        else if (state == PAUSED) {
            drawField(); drawHUD();
            textC(bufTop, 80, "PAUSED", YELLOW, 2);
            textC(bufBot, 70, "START  resume", WHITE, 1);
            textC(bufBot, 94, "SELECT  quit to menu", WHITE, 1);
        }
        else if (state == MENU) drawMenu();
        else if (state == HANDOFF) drawHandoff();
        else if (state == RESULTS) drawResults();
        swiWaitForVBlank();
        dmaCopy(bufTop, vramTop, sizeof bufTop);
        dmaCopy(bufBot, vramBot, sizeof bufBot);
    }
    return 0;
}
