// draw.c — screens, sprites, text, buttons, flags
#include "qu.h"

u16 bufTop[SW * SH], bufBot[SW * SH];

// ── maths (the DS has no floating-point hardware: keep these light) ──────
float fsqrt(float v) { if (v <= 0) return 0; float x = v > 1 ? v : 1; for (int i = 0; i < 12; i++) x = 0.5f * (x + v / x); return x; }
float fabsf_(float v) { return v < 0 ? -v : v; }
static float wrapPi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
float fsin(float a) { a = wrapPi(a); float y = 1.2732395f * a - 0.4052847f * a * fabsf_(a); return 0.225f * (y * fabsf_(y) - y) + y; }
float fcos(float a) { return fsin(a + 1.5707963f); }
float frand(void) { return (rand() % 1000) / 1000.0f; }

// ── both screens as one tall canvas: gy 0-191 top, 192-383 bottom ────────
void gpx(int x, int gy, u16 c) {
    if ((unsigned)x >= SW) return;
    if ((unsigned)gy < SH) bufTop[gy * SW + x] = c;
    else if ((unsigned)(gy - SH) < SH) bufBot[(gy - SH) * SW + x] = c;
}
void grect(int x, int gy, int w, int h, u16 c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) gpx(x + i, gy + j, c); }

// rotated sprite, whole-number maths only (16.16 fixed point) so it keeps 60 fps
void blitRot(const u16 *spr, int w, int h, int cx, int cy, float ang) {
    int ci = (int)(fcos(ang) * 65536.0f), si = (int)(fsin(ang) * 65536.0f);
    int r = (int)(fsqrt((float)(w * w + h * h)) / 2) + 1;
    for (int dy = -r; dy <= r; dy++) {
        int gy = cy + dy; if (gy < 0 || gy >= 2 * SH) continue;
        int sxf = ci * (-r) + si * dy + (w << 15), syf = -si * (-r) + ci * dy + (h << 15);
        for (int dx = -r; dx <= r; dx++, sxf += ci, syf -= si) {
            int gx = cx + dx; if ((unsigned)gx >= SW) continue;
            int ix = sxf >> 16, iy = syf >> 16;
            if ((unsigned)ix >= (unsigned)w || (unsigned)iy >= (unsigned)h) continue;
            u16 p = spr[iy * w + ix];
            if (p & 0x8000) gpx(gx, gy, p);
        }
    }
}
void blit(u16 *buf, const u16 *spr, int w, int h, int x, int y) {
    for (int j = 0; j < h; j++) { int yy = y + j; if (yy < 0 || yy >= SH) continue;
        for (int i = 0; i < w; i++) { int xx = x + i; if (xx < 0 || xx >= SW) continue; u16 p = spr[j * w + i]; if (p & 0x8000) buf[yy * SW + xx] = p; } }
}
void rect(u16 *buf, int x, int y, int w, int h, u16 c) {
    for (int j = 0; j < h; j++) { int yy = y + j; if ((unsigned)yy >= SH) continue;
        for (int i = 0; i < w; i++) { int xx = x + i; if ((unsigned)xx < SW) buf[yy * SW + xx] = c; } }
}
void fillScreen(u16 *buf, u16 c) { for (int i = 0; i < SW * SH; i++) buf[i] = c; }
void box(u16 *buf, int x, int y, int w, int h, u16 fill, u16 edge) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
        int xx = x + i, yy = y + j; if ((unsigned)xx >= SW || (unsigned)yy >= SH) continue;
        int e = (i < 2 || j < 2 || i >= w - 2 || j >= h - 2);
        buf[yy * SW + xx] = e ? edge : fill;
    }
}

// ── text (DejaVu Sans Bold glyphs from assets.c), dark outline for reading on photos
int textW(const char *t, int sc) { int w = 0; for (; *t; t++) { int ch = *t; if (ch < 32 || ch > 126) ch = '?'; w += font_w[ch - 32] * sc; } return w; }
static void glyphs(u16 *buf, int gyMode, int x, int y, const char *t, u16 col, int sc) {
    for (; *t; t++) {
        int ch = *t; if (ch < 32 || ch > 126) ch = '?';
        const u16 *rows = &font_rows[(ch - 32) * FONT_H];
        for (int j = 0; j < FONT_H; j++) { u16 r = rows[j]; if (!r) continue;
            for (int i = 0; i < 16; i++) if (r & (1 << i))
                for (int a = 0; a < sc; a++) for (int b = 0; b < sc; b++) {
                    int px = x + i * sc + a, py = y + j * sc + b;
                    if (gyMode) gpx(px, py, col);
                    else if ((unsigned)px < SW && (unsigned)py < SH) buf[py * SW + px] = col;
                } }
        x += font_w[ch - 32] * sc;
    }
}
void text(u16 *buf, int x, int y, const char *t, u16 col, int sc) {
    glyphs(buf, 0, x - 1, y, t, BLACK, sc); glyphs(buf, 0, x + 1, y, t, BLACK, sc);
    glyphs(buf, 0, x, y - 1, t, BLACK, sc); glyphs(buf, 0, x, y + 1, t, BLACK, sc);
    glyphs(buf, 0, x, y, t, col, sc);
}
void textC(u16 *buf, int y, const char *t, u16 col, int sc) { text(buf, (SW - textW(t, sc)) / 2, y, t, col, sc); }
void gtext(int x, int gy, const char *t, u16 col, int sc) {
    glyphs(0, 1, x - 1, gy, t, BLACK, sc); glyphs(0, 1, x + 1, gy, t, BLACK, sc);
    glyphs(0, 1, x, gy - 1, t, BLACK, sc); glyphs(0, 1, x, gy + 1, t, BLACK, sc);
    glyphs(0, 1, x, gy, t, col, sc);
}
void scoreStr(char *o, int doubled) {
    if (doubled < 0 && (doubled & 1)) sprintf(o, "-%d.5", (-doubled) / 2);
    else if (doubled & 1) sprintf(o, "%d.5", doubled / 2);
    else sprintf(o, "%d", doubled / 2);
}
void coinCount(u16 *buf, int x, int y) {
    char s[16]; blit(buf, coin, COIN_W, COIN_H, x, y); sprintf(s, "%d", sv.coins);
    text(buf, x + COIN_W + 4, y + 2, s, GOLD, 1);
}

// ── buttons ───────────────────────────────────────────────────────────────
void drawBtns(u16 *buf, Btn *b, int n, int sel) {
    for (int i = 0; i < n; i++) {
        int on = (i == sel);
        u16 edge = b[i].dim ? GREY : (on ? YELLOW : WHITE);
        box(buf, b[i].x, b[i].y, b[i].w, b[i].h, on ? COL(6, 6, 6) : BLACK, edge);
        u16 c = b[i].col ? b[i].col : (b[i].dim ? GREY : (on ? YELLOW : WHITE));
        text(buf, b[i].x + (b[i].w - textW(b[i].label, 1)) / 2, b[i].y + (b[i].h - FONT_H) / 2 + 1, b[i].label, c, 1);
    }
}
// D-pad moves the highlight (cols = buttons per row), A presses it, or tap a button
int btnInput(Btn *b, int n, int *sel, int cols) {
    if (n <= 0) return -1;
    if (*sel < 0 || *sel >= n) *sel = 0;
    if (kDown & KEY_RIGHT) *sel = (*sel + 1) % n;
    if (kDown & KEY_LEFT) *sel = (*sel + n - 1) % n;
    if (kDown & KEY_DOWN) *sel = (*sel + cols) % n;
    if (kDown & KEY_UP) *sel = (*sel + n - cols % n) % n;
    if (kDown & KEY_A) return *sel;
    if (kDown & KEY_TOUCH)
        for (int i = 0; i < n; i++)
            if (tX >= b[i].x && tX < b[i].x + b[i].w && tY >= b[i].y && tY < b[i].y + b[i].h) { *sel = i; return i; }
    return -1;
}

// ── nation flags (simplified, drawn from shapes) ──────────────────────────
static void hstripes(u16 *buf, int x, int y, int w, int h, const u16 *c, int n) { for (int i = 0; i < n; i++) rect(buf, x, y + h * i / n, w, h * (i + 1) / n - h * i / n, c[i]); }
static void vstripes(u16 *buf, int x, int y, int w, int h, const u16 *c, int n) { for (int i = 0; i < n; i++) rect(buf, x + w * i / n, y, w * (i + 1) / n - w * i / n, h, c[i]); }
static void disc(u16 *buf, int cx, int cy, int r, u16 c) { for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r) rect(buf, cx + i, cy + j, 1, 1, c); }
static void nordic(u16 *buf, int x, int y, int w, int h, u16 bg, u16 outer, u16 inner) {
    rect(buf, x, y, w, h, bg);
    int cx = x + w * 3 / 8, t = h / 4;
    rect(buf, x, y + h / 2 - t / 2 - 1, w, t + 2, outer); rect(buf, cx - t / 2 - 1, y, t + 2, h, outer);
    if (inner != outer) { rect(buf, x, y + h / 2 - t / 4, w, t / 2, inner); rect(buf, cx - t / 4, y, t / 2, h, inner); }
}
void drawFlag(u16 *buf, int x, int y, int w, int h, int n) {
    u16 R = COL(26, 3, 5), W = WHITE, B = COL(2, 6, 18), G = COL(2, 16, 8), Y = COL(31, 26, 0), K = BLACK, O = COL(31, 16, 2), L = COL(4, 12, 28);
    switch (n) {
        case 0: { u16 c[3] = { G, W, O }; vstripes(buf, x, y, w, h, c, 3); } break;                          // Ireland
        case 1: rect(buf, x, y, w, h, W); rect(buf, x, y + h / 6, w, h / 8, L); rect(buf, x, y + h * 5 / 6 - h / 8, w, h / 8, L);   // Israel
                disc(buf, x + w / 2, y + h / 2, h / 5, L); disc(buf, x + w / 2, y + h / 2, h / 5 - 2, W); break;
        case 2: for (int i = 0; i < 7; i++) rect(buf, x, y + h * i / 7, w, h / 14 + 1, i % 2 ? W : R);            // USA
                for (int i = 0; i < 7; i++) rect(buf, x, y + h * i / 7 + h / 14, w, h / 14 + 1, W);
                for (int i = 0; i < 13; i++) rect(buf, x, y + h * i / 13, w, h / 13 + 1, i % 2 ? W : R);
                rect(buf, x, y, w * 2 / 5, h * 7 / 13, B); break;
        case 3: { u16 c[3] = { R, W, B }; hstripes(buf, x, y, w, h, c, 3); }                                      // South Africa
                rect(buf, x, y + h * 2 / 5, w, h / 5, G);
                for (int j = 0; j < h; j++) { int e = (j < h / 2 ? j : h - 1 - j) * w / h; rect(buf, x, y + j, e, 1, j > h / 5 && j < h * 4 / 5 ? K : Y); } break;
        case 4: nordic(buf, x, y, w, h, W, R, R); rect(buf, x + w / 2 - h / 8, y, h / 4, h, R); break;           // England (centred cross)
        case 5: rect(buf, x, y, w, h, L); for (int i = 0; i < w; i++) { int yy = i * h / w; rect(buf, x + i, y + yy - 1, 1, 3, W); rect(buf, x + i, y + h - 1 - yy - 1, 1, 3, W); } break;   // Scotland
        case 6: nordic(buf, x, y, w, h, R, W, B); break;                                                         // Norway
        case 7: rect(buf, x, y, w, h, W); disc(buf, x + w / 2, y + h / 2, h * 3 / 10, R); break;                // Japan
        case 8: { u16 c[3] = { K, R, Y }; hstripes(buf, x, y, w, h, c, 3); } break;                            // Germany
        case 9: rect(buf, x, y, w, h, G);                                                                        // Brazil
                for (int j = 0; j < h; j++) { int e = (j < h / 2 ? j : h - 1 - j) * w / h; rect(buf, x + w / 2 - e, y + j, 2 * e, 1, Y); }
                disc(buf, x + w / 2, y + h / 2, h / 4, B); break;
        case 10: rect(buf, x, y, w, h, B); rect(buf, x, y, w / 2, h / 2, B); rect(buf, x + w / 4 - 1, y, 2, h / 2, R); rect(buf, x, y + h / 4 - 1, w / 2, 2, R);   // Australia
                 disc(buf, x + w / 4, y + h * 3 / 4, 2, W); disc(buf, x + w * 3 / 4, y + h / 3, 1, W); disc(buf, x + w * 3 / 4, y + h * 3 / 4, 1, W); break;
        case 11: { u16 c[4] = { R, W, W, R }; vstripes(buf, x, y, w, h, c, 4); } disc(buf, x + w / 2, y + h / 2, h / 4, R); break;   // Canada
        case 12: { u16 c[3] = { B, W, R }; vstripes(buf, x, y, w, h, c, 3); } break;                           // France
        case 13: { u16 c[3] = { R, W, B }; hstripes(buf, x, y, w, h, c, 3); } break;                           // Netherlands
        case 14: nordic(buf, x, y, w, h, L, Y, Y); break;                                                       // Sweden
        case 15: { u16 c[3] = { W, L, R }; hstripes(buf, x, y, w, h, c, 3); } break;                           // Russia
        case 16: { u16 c[3] = { G, W, R }; hstripes(buf, x, y, w, h, c, 3); } disc(buf, x + w / 2, y + h / 2, h / 8, R); break;   // Iran
        case 17: rect(buf, x, y, w, h, R); disc(buf, x + w / 5, y + h / 3, h / 7, Y); break;                   // China
    }
    // thin border so light flags don't vanish on dark screens
    rect(buf, x, y, w, 1, GREY); rect(buf, x, y + h - 1, w, 1, GREY); rect(buf, x, y, 1, h, GREY); rect(buf, x + w - 1, y, 1, h, GREY);
}

// ── toasts: "ACHIEVEMENT UNLOCKED", coins won ─────────────────────────────
static char toastA[28], toastB[28]; static int toastT;
void toast(const char *a, const char *b) { strncpy(toastA, a, 27); toastA[27] = 0; strncpy(toastB, b ? b : "", 27); toastB[27] = 0; toastT = 150; }
void drawToast(void) {
    if (toastT <= 0) return;
    toastT--;
    box(bufTop, 24, 140, 208, 44, COL(3, 3, 1), GOLD);
    textC(bufTop, 146, toastA, GOLD, 1);
    if (toastB[0]) textC(bufTop, 163, toastB, WHITE, 1);
}
