// draw.c — screens, sprites, text, buttons, flags
#include "qu.h"

u16 bufTop[SW * SH] __attribute__((aligned(32))), bufBot[SW * SH] __attribute__((aligned(32)));

// ── maths (the DS has no floating-point hardware: keep these light) ──────
float fsqrt(float v) {                         // bit-trick first guess + 3 Newton steps (was 12)
    if (v <= 0) return 0;
    union { float f; u32 i; } u = { v }; u.i = 0x1FBD1DF5 + (u.i >> 1);
    float x = u.f; x = 0.5f * (x + v / x); x = 0.5f * (x + v / x); return 0.5f * (x + v / x);
}
float fabsf_(float v) { return v < 0 ? -v : v; }
static float wrapPi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
float fsin(float a) { a = wrapPi(a); float y = 1.2732395f * a - 0.4052847f * a * fabsf_(a); return 0.225f * (y * fabsf_(y) - y) + y; }
float fcos(float a) { return fsin(a + 1.5707963f); }
float frand(void) { return (rand() % 1000) / 1000.0f; }
float fatan2r(float y, float x) {               // radians
    float ax = fabsf_(x), ay = fabsf_(y);
    if (ax < 1e-6f && ay < 1e-6f) return 0;
    float a = (ax < ay ? ax / ay : ay / ax), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    return y < 0 ? -r : r;
}

// ── both screens as one tall canvas: gy 0-191 top, 192-383 bottom ────────
int gClipLo = 0, gClipHi = 2 * SH;           // rows of the tall canvas a drawing pass may touch
static inline u16 *growp(int gy) { return gy < SH ? &bufTop[gy * SW] : &bufBot[(gy - SH) * SW]; }
void gpx(int x, int gy, u16 c) {
    if ((unsigned)x >= SW || gy < gClipLo || gy >= gClipHi) return;
    growp(gy)[x] = c;
}
void grect(int x, int gy, int w, int h, u16 c) {
    int x0 = x < 0 ? 0 : x, x1 = x + w > SW ? SW : x + w;
    for (int j = 0; j < h; j++) { int y = gy + j; if (y < gClipLo || y >= gClipHi || x0 >= x1) continue;
        u16 *r = growp(y); for (int i = x0; i < x1; i++) r[i] = c; }
}
// the whole tall canvas from a 256-colour picture (arcade tables): 2 pixels per store
void drawIndexed(const u8 *idx, const u16 *pal) {
    u32 *d = (u32 *)bufTop; const u32 *s = (const u32 *)idx;
    for (int half = 0; half < 2; half++, d = (u32 *)bufBot)
        for (int i = 0; i < SW * SH / 4; i++) {
            u32 q = *s++;
            *d++ = pal[q & 255] | ((u32)pal[(q >> 8) & 255] << 16);
            *d++ = pal[(q >> 16) & 255] | ((u32)pal[q >> 24] << 16);
        }
}

// rotated (and scaled) sprite, whole-number maths in the loop (16.16 fixed point). Only the
// stretch of each row that actually lands inside the sprite is walked, and pixels are written
// straight into the row instead of through gpx().
u16 gSil;                                    // draw a sprite's silhouette in one colour (outlines, glows)
int gStip;                                   // see-through: skip every other pixel (a cheap 50% fade)
static void blitCore(const u16 *spr, int w, int h, int cx, int cy, float ang, float scale) {
    if (scale <= 0.01f) return;
    float inv = 1.0f / scale;
    int ci = (int)(fcos(ang) * inv * 65536.0f), si = (int)(fsin(ang) * inv * 65536.0f);
    int r = (int)(fsqrt((float)(w * w + h * h)) * scale / 2) + 1;
    int W = w << 16, H = h << 16;
    for (int dy = -r; dy <= r; dy++) {
        int gy = cy + dy; if (gy < gClipLo || gy >= gClipHi) continue;
        int sxf = ci * (-r) + si * dy + (w << 15), syf = -si * (-r) + ci * dy + (h << 15);
        int a = -r, b = r;
        // narrow [a,b] to where 0 <= sx < W and 0 <= sy < H (both are straight lines in dx)
        #define NARROW(v0, dv, LIM) if (dv == 0) { if (v0 < 0 || v0 >= LIM) continue; } \
            else { int lo, hi; if (dv > 0) { lo = (-(v0)) / dv - 1; hi = (LIM - 1 - (v0)) / dv + 1; } \
                   else { lo = (LIM - 1 - (v0)) / dv - 1; hi = (-(v0)) / dv + 1; } \
                   if (lo - r > a) a = lo - r; if (hi - r < b) b = hi - r; }
        NARROW(sxf, ci, W) NARROW(syf, -si, H)
        #undef NARROW
        if (cx + a < 0) a = -cx;
        if (cx + b > SW - 1) b = SW - 1 - cx;
        if (a > b) continue;
        u16 *row = growp(gy);
        int k = a + r; sxf += ci * k; syf -= si * k;
        for (int dx = a; dx <= b; dx++, sxf += ci, syf -= si) {
            int ix = sxf >> 16, iy = syf >> 16;
            if ((unsigned)ix >= (unsigned)w || (unsigned)iy >= (unsigned)h) continue;
            u16 p = spr[iy * w + ix];
            if ((p & 0x8000) && !(gStip && (((cx + dx) ^ gy) & 1))) row[cx + dx] = gSil ? gSil : p;
        }
    }
}
void blitRot(const u16 *spr, int w, int h, int cx, int cy, float ang) { blitCore(spr, w, h, cx, cy, ang, 1.0f); }
void blitRotScale(const u16 *spr, int w, int h, int cx, int cy, float ang, float scale) { blitCore(spr, w, h, cx, cy, ang, scale); }
// darken one pixel of the tall canvas (soft shadows)
void gdark(int x, int gy) {
    if ((unsigned)x >= SW || gy < gClipLo || gy >= gClipHi) return;
    u16 *p = &growp(gy)[x];
    *p = ((*p >> 1) & 0x3DEF) | 0x8000;
}
void blit(u16 *buf, const u16 *spr, int w, int h, int x, int y) {
    int i0 = x < 0 ? -x : 0, i1 = x + w > SW ? SW - x : w;
    for (int j = 0; j < h; j++) { int yy = y + j; if ((unsigned)yy >= SH) continue;
        const u16 *s = &spr[j * w]; u16 *d = &buf[yy * SW + x];
        for (int i = i0; i < i1; i++) { u16 p = s[i]; if (p & 0x8000) d[i] = p; } }
}
static void hfill(u16 *row, int x0, int x1, u16 c) {          // [x0, x1) already clipped
    if (x0 >= x1) return;
    if (x0 & 1) row[x0++] = c;
    u32 cc = c | ((u32)c << 16), *d = (u32 *)&row[x0]; int n = (x1 - x0) >> 1;
    while (n >= 4) { d[0] = cc; d[1] = cc; d[2] = cc; d[3] = cc; d += 4; n -= 4; }
    while (n--) *d++ = cc;
    if ((x1 - x0) & 1) row[x1 - 1] = c;
}
void rect(u16 *buf, int x, int y, int w, int h, u16 c) {
    int x0 = x < 0 ? 0 : x, x1 = x + w > SW ? SW : x + w, y0 = y < 0 ? 0 : y, y1 = y + h > SH ? SH : y + h;
    for (int yy = y0; yy < y1; yy++) hfill(&buf[yy * SW], x0, x1, c);
}
static u16 themeBgImg[2][SW * SH] __attribute__((aligned(32)));   // this theme's menu backgrounds (top, bottom)
static int themeBgOK;
void fillScreen(u16 *buf, u16 c) {
    if (c == DARK && themeBgOK) { memcpy(buf, buf == bufTop ? themeBgImg[0] : themeBgImg[1], SW * SH * 2); return; }
    u32 cc = c | ((u32)c << 16), *d = (u32 *)buf;
    for (int i = 0; i < SW * SH / 16; i++) { d[0] = cc; d[1] = cc; d[2] = cc; d[3] = cc; d[4] = cc; d[5] = cc; d[6] = cc; d[7] = cc; d += 8; }
}
void box(u16 *buf, int x, int y, int w, int h, u16 fill, u16 edge) {
    rect(buf, x, y, w, h, edge); rect(buf, x + 2, y + 2, w - 4, h - 4, fill);
}

// ── the website's marker shape (body + cap), used by buttons and the power marker ──
// rounded-end inset for row j of an h-tall shape whose end has radius r
static int inset(int j, int h, int r) {
    int d = j < r ? r - j : (j >= h - r ? j - (h - 1 - r) : 0);
    if (d <= 0) return 0;
    int i = 0; while (i < r && (r - i) * (r - i) + d * d > r * r + r) i++;
    return i;
}
// fill [x, x+w) x [y, y+h) with rounded left (rl) / right (rr) ends; row colour from grad[] or c
static void shape(u16 *buf, int x, int y, int w, int h, int rl, int rr, const u16 *grad, u16 c) {
    for (int j = 0; j < h; j++) { int yy = y + j; if ((unsigned)yy >= SH) continue;
        int x0 = x + inset(j, h, rl), x1 = x + w - inset(j, h, rr);
        if (x0 < 0) x0 = 0;
        if (x1 > SW) x1 = SW;
        hfill(&buf[yy * SW], x0, x1, grad ? grad[j * 32 / h] : c); }
}
// ── text (DejaVu Sans Bold glyphs from assets.c), dark outline for reading on photos
static int squeeze;                          // pixels taken off each letter's spacing (long button labels)
int textW(const char *t, int sc) { int w = 0; for (; *t; t++) { int ch = *t; if (ch < 32 || ch > 126) ch = '?'; w += (font_w[ch - 32] - squeeze) * sc; } return w; }
// One pass per glyph row: the outline is the row's pixels spread one step left/right plus the
// rows above and below, minus the letter itself, and only set bits are visited. (It used to be
// five full passes over every bit of every row — the single biggest cost on menu screens.)
static void glyphs(u16 *buf, int gyMode, int x, int y, const char *t, u16 col, int sc) {
    for (; *t; t++) {
        int ch = *t; if (ch < 32 || ch > 126) ch = '?';
        const u16 *rows = &font_rows[(ch - 32) * FONT_H];
        for (int j = -1; j <= FONT_H; j++) {
            u32 r = (j >= 0 && j < FONT_H) ? rows[j] : 0, up = j > 0 ? rows[j - 1] : 0, dn = j < FONT_H - 1 ? rows[j + 1] : 0;
            u32 r2 = r << 1, out = ((r2 << 1) | (r2 >> 1) | (up << 1) | (dn << 1)) & ~r2, m = out | r2;   // bit i+1 = column i
            if (!m) continue;
            for (int b2 = 0; b2 < sc; b2++) {
                int py = y + j * sc + b2;
                u16 *row;
                if (gyMode) { if (py < gClipLo || py >= gClipHi || py < 0 || py >= 2 * SH) continue; row = growp(py); }
                else { if ((unsigned)py >= SH) continue; row = &buf[py * SW]; }
                for (u32 mm = m; mm; mm &= mm - 1) {
                    int i = __builtin_ctz(mm) - 1; u16 c = (r2 >> (i + 1)) & 1 ? col : BLACK;
                    for (int a2 = 0; a2 < sc; a2++) { int px = x + i * sc + a2; if ((unsigned)px < SW) row[px] = c; }
                }
            }
        }
        x += (font_w[ch - 32] - squeeze) * sc;
    }
}
void text(u16 *buf, int x, int y, const char *t, u16 col, int sc) { glyphs(buf, 0, x, y, t, col, sc); }
// centred text that always fits the screen: too wide at double size drops to normal size
// (kept vertically centred where the big text would have been), then tightens its letters
void textC(u16 *buf, int y, const char *t, u16 col, int sc) {
    if (sc > 1 && textW(t, sc) > SW - 6) { y += (FONT_H * (sc - 1)) / 2; sc = 1; }
    int old = squeeze;
    while (textW(t, sc) > SW - 4 && squeeze < 3) squeeze++;
    text(buf, (SW - textW(t, sc)) / 2, y, t, col, sc);
    squeeze = old;
}
void gtext(int x, int gy, const char *t, u16 col, int sc) { glyphs(0, 1, x, gy, t, col, sc); }
void scoreStr(char *o, int doubled) {
    if (doubled < 0 && (doubled & 1)) sprintf(o, "-%d.5", (-doubled) / 2);
    else if (doubled & 1) sprintf(o, "%d.5", doubled / 2);
    else sprintf(o, "%d", doubled / 2);
}
void coinCount(u16 *buf, int x, int y) {
    char s[16]; blitInk(buf, coinT, COIN_W, COIN_H, x, y, uiInk); sprintf(s, "%d", sv.coins);
    text(buf, x + COIN_W + 4, y + 2, s, GOLD, 1);
}


// ── themes on every screen ──────────────────────────────────────────────────
// The website recolours the whole page per theme; here each theme gets its own menu
// background (drawn once when the theme changes, then copied), button colours, accent
// colours for titles and highlights, and a recoloured logo. The tables and fields are
// recoloured by the same themeTint the website-style filters are modelled on.
u16 uiGold = COL(31, 24, 2), uiYellow = COL(31, 27, 4), uiGrey = COL(18, 18, 18), uiIcon = COL(19, 19, 20);
u16 logoT[LOGO_W * LOGO_H];
u16 coinT[COIN_W * COIN_H], icCoinBigT[IC_COINBIG_W * IC_COINBIG_H], slotSymT[7][SLOT_LOGO_W * SLOT_LOGO_H];
u16 icSlotT[IC_SLOT_W * IC_SLOT_H], icArcadeT[IC_ARCADE_W * IC_ARCADE_H], icCoinT[IC_COIN_W * IC_COIN_H], uiInk, uiDotRed;
int iconFat;                                 // extra thickness for the icons' outline pass
static u16 uiEdge = COL(31, 31, 31), uiSel = COL(31, 26, 4), uiGradTop = COL(7, 7, 7), uiGradMid = COL(2, 2, 2), uiGradEnd = COL(0, 0, 0);
static u16 BTN_GRAD[32];
static u32 rs = 1;
static int rnd(int n) { rs = rs * 1103515245u + 12345u; return (int)((rs >> 16) % (unsigned)n); }
static u16 mix(u16 a, u16 b, int k, int n) {          // a -> b, k of n
    int ar = a & 31, ag = (a >> 5) & 31, ab = (a >> 10) & 31, br = b & 31, bg = (b >> 5) & 31, bb = (b >> 10) & 31;
    return COL(ar + (br - ar) * k / n, ag + (bg - ag) * k / n, ab + (bb - ab) * k / n);
}
static void themePx(int x, int gy, u16 c) { if ((unsigned)x < SW && (unsigned)gy < 2 * SH) themeBgImg[gy / SH][(gy % SH) * SW + x] = c; }
static void themeGrad(u16 top, u16 mid, u16 bot) {        // one gradient down both screens
    for (int gy = 0; gy < 2 * SH; gy++) {
        u16 c = gy < SH ? mix(top, mid, gy, SH) : mix(mid, bot, gy - SH, SH);
        for (int x = 0; x < SW; x++) themeBgImg[gy / SH][(gy % SH) * SW + x] = c;
    }
}
void themeUI(int t) {
    rs = 12345 + t;
    switch (t) {
    default:                                            // REALISTIC: as it always was
        uiGold = COL(31, 24, 2); uiYellow = COL(31, 27, 4); uiGrey = COL(18, 18, 18);
        uiEdge = WHITE; uiSel = COL(31, 26, 4); uiGradTop = COL(7, 7, 7); uiGradMid = COL(2, 2, 2); uiGradEnd = BLACK;
        themeGrad(DARK, DARK, DARK);
        break;
    case 1:                                             // CARTOON: newsprint with halftone dots, ink outlines
        uiGold = COL(31, 22, 0); uiYellow = COL(31, 29, 6); uiGrey = COL(23, 23, 22);
        uiEdge = BLACK; uiSel = COL(31, 6, 6); uiGradTop = COL(14, 14, 14); uiGradMid = COL(6, 6, 6); uiGradEnd = COL(2, 2, 2);
        themeGrad(COL(28, 27, 24), COL(26, 25, 22), COL(24, 23, 20));
        for (int gy = 0; gy < 2 * SH; gy += 6) for (int x = (gy / 6 % 2) * 3; x < SW; x += 6) {
            int r = 1 + (gy * 2 / (2 * SH));            // dots grow down the screens, like a comic panel's shading
            for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r) themePx(x + i, gy + j, COL(19, 18, 16));
        }
        for (int gy = 0; gy < 2 * SH; gy++) { themePx(0, gy, BLACK); themePx(1, gy, BLACK); themePx(SW - 1, gy, BLACK); themePx(SW - 2, gy, BLACK); }
        for (int x = 0; x < SW; x++) { themePx(x, 0, BLACK); themePx(x, 1, BLACK); themePx(x, 2 * SH - 1, BLACK); themePx(x, 2 * SH - 2, BLACK); }
        break;
    case 2:                                             // NIGHT: deep blue sky, stars, a moon
        uiGold = COL(29, 27, 14); uiYellow = COL(31, 30, 20); uiGrey = COL(15, 17, 23);
        uiEdge = COL(20, 24, 31); uiSel = COL(31, 29, 14); uiGradTop = COL(5, 7, 15); uiGradMid = COL(1, 2, 6); uiGradEnd = COL(0, 0, 2);
        themeGrad(COL(0, 1, 4), COL(2, 3, 9), COL(1, 1, 5));
        for (int i = 0; i < 220; i++) { int x = rnd(SW), y = rnd(2 * SH), b = 14 + rnd(18); u16 c = COL(b, b, b > 28 ? 31 : b + 3);
            themePx(x, y, c); if (b > 27) { themePx(x + 1, y, c); themePx(x - 1, y, c); themePx(x, y + 1, c); themePx(x, y - 1, c); } }
        for (int j = -14; j <= 14; j++) for (int i = -14; i <= 14; i++) { int d = i * i + j * j, e = (i + 6) * (i + 6) + (j - 4) * (j - 4);
            if (d <= 196 && e > 160) themePx(222 + i, 30 + j, COL(29, 29, 24)); }
        break;
    case 4:                                             // NEON: the website's glowing green, on black
        uiGold = COL(8, 31, 15); uiYellow = COL(20, 31, 24); uiGrey = COL(13, 24, 17);
        uiEdge = COL(6, 31, 15); uiSel = COL(26, 31, 28); uiGradTop = COL(1, 11, 5); uiGradMid = COL(0, 4, 2); uiGradEnd = COL(0, 1, 0);
        themeGrad(COL(0, 3, 1), COL(0, 1, 0), COL(0, 3, 1));
        for (int gy = 0; gy < 2 * SH; gy++) for (int x = 0; x < SW; x++) {
            int g = (x % 24 == 0 || gy % 24 == 0);
            if (g) themePx(x, gy, COL(1, 13, 6));
            int e = x < 4 ? 4 - x : x >= SW - 4 ? x - (SW - 5) : 0;                  // glowing tubes down both edges
            if (e) themePx(x, gy, e >= 3 ? COL(4, 20, 10) : COL(14, 31, 20));
        }
        for (int x = 0; x < SW; x++) for (int k = 0; k < 3; k++) { themePx(x, SH - 2 + k, COL(8, 31, 16)); }   // a tube along the screen gap
        break;
    case 3:                                             // (SUNSET shows GOLDEN)
    case 5:                                             // GOLDEN: warm dark gold with sparkle
        uiGold = COL(31, 27, 6); uiYellow = COL(31, 31, 16); uiGrey = COL(27, 22, 12);
        uiEdge = COL(31, 26, 6); uiSel = COL(31, 31, 24); uiGradTop = COL(20, 14, 2); uiGradMid = COL(8, 5, 0); uiGradEnd = COL(3, 2, 0);
        themeGrad(COL(16, 10, 1), COL(7, 4, 0), COL(18, 12, 2));
        for (int gy = 0; gy < 2 * SH; gy += 3) for (int x = (gy / 3 % 2) * 3; x < SW; x += 6) themePx(x, gy, COL(24, 17, 3));   // gold-leaf grain
        for (int i = 0; i < 140; i++) { int x = rnd(SW), y = rnd(2 * SH), b = rnd(3);
            themePx(x, y, COL(31, 28, 12)); if (!b) for (int k = 1; k < 4; k++) { u16 c = COL(28 - k * 4, 22 - k * 4, 6); themePx(x + k, y, c); themePx(x - k, y, c); themePx(x, y + k, c); themePx(x, y - k, c); } }
        break;
    }
    uiIcon = t == 1 ? COL(3, 3, 3) : t == 2 ? COL(18, 21, 27) : t == 4 ? COL(9, 31, 15) : (t == 3 || t == 5) ? COL(31, 24, 7) : COL(19, 19, 20);
    for (int i = 0; i < 32; i++) BTN_GRAD[i] = i < 15 ? mix(uiGradTop, uiGradMid, i, 15) : mix(uiGradMid, uiGradEnd, i - 15, 17);
    // the logo and every menu icon take the theme's colour (CARTOON: grey) but stay bright enough to read
    #define TINT(dst, src, n) for (int i = 0; i < (n); i++) dst[i] = (src[i] & 0x8000) ? (t <= 1 ? themeTint(src[i], t) : mix(src[i], themeTint(src[i], t), 1, 2)) : 0;
    TINT(logoT, logo, LOGO_W * LOGO_H)
    TINT(icSlotT, ic_slot, IC_SLOT_W * IC_SLOT_H)
    TINT(icArcadeT, ic_arcade, IC_ARCADE_W * IC_ARCADE_H)
    for (int i = 0; i < IC_COIN_W * IC_COIN_H; i++)                // the coin keeps more of its gold so it still reads as a coin
        icCoinT[i] = (ic_coin[i] & 0x8000) ? (t <= 1 ? themeTint(ic_coin[i], t) : mix(ic_coin[i], themeTint(ic_coin[i], t), 1, 3)) : 0;
    // every coin picture and the slot machine's symbols take the theme too (CARTOON: grey)
    #define CTINT(dst, src, n) for (int i = 0; i < (n); i++) dst[i] = (src[i] & 0x8000) ? (t <= 1 ? themeTint(src[i], t) : mix(src[i], themeTint(src[i], t), 1, 3)) : 0;
    CTINT(coinT, coin, COIN_W * COIN_H)
    CTINT(icCoinBigT, ic_coinbig, IC_COINBIG_W * IC_COINBIG_H)
    { const u16 *S[7] = { slot_logo, slot_mega, slot_coin, slot_chair, slot_red, slot_green, slot_blue };
      for (int k = 0; k < 7; k++) { CTINT(slotSymT[k], S[k], SLOT_LOGO_W * SLOT_LOGO_H) } }
    #undef CTINT
    #undef TINT
    uiDotRed = t <= 0 ? COL(31, 10, 10) : themeTint(COL(31, 10, 10), t);
    uiInk = t == 1 ? BLACK : t == 4 ? COL(0, 6, 2) : t == 2 ? COL(1, 2, 7) : (t == 3 || t == 5) ? COL(7, 4, 0) : COL(1, 1, 1);
    themeBgOK = 1;
}

// ── buttons ───────────────────────────────────────────────────────────────
// The website's marker buttons: a dark gradient body with a white outline and a rounded cap
// on the left end, like a marker pen. The highlighted one gets a gold outline and sinks 2 px
// while it's held, as the website's buttons press down.
static void btnGrad(void) { if (!(BTN_GRAD[0] & 0x8000)) themeUI(0); }
void markerShapeCap(u16 *buf, int x, int y, int w, int h, int cap, u16 edge, const u16 *grad) {
    int rc = h / 2 - 1, rb = h / 4; if (rc > cap) rc = cap;
    u16 shine = mix(grad[0], WHITE, 1, 3);
    shape(buf, x, y, w, h, rc, rb, 0, edge);                                      // outline
    shape(buf, x + 2, y + 2, cap - 3, h - 4, rc - 2 > 1 ? rc - 2 : 1, 1, grad, 0); // the cap
    shape(buf, x + cap + 1, y + 2, w - cap - 3, h - 4, 1, rb - 2 > 1 ? rb - 2 : 1, grad, 0);   // the body
    if (cap > 8) rect(buf, x + 3 + rc / 2, y + 2, cap - 5 - rc / 2, 1, shine);  // the shine along the top
    rect(buf, x + cap + 2, y + 2, w - cap - 3 - rb, 1, shine);
}
void markerShape(u16 *buf, int x, int y, int w, int h, u16 edge, const u16 *grad) {
    int cap = h * 45 / 100; if (cap < 10) cap = 10;
    markerShapeCap(buf, x, y, w, h, cap, edge, grad);
}
void drawBtns(u16 *buf, Btn *b, int n, int sel) {
    btnGrad();
    for (int i = 0; i < n; i++) {
        int on = (i == sel), dn = on && (kHeld & KEY_A) ? 2 : 0;
        u16 edge = b[i].dim ? mix(uiEdge, BLACK, 1, 2) : (on ? uiSel : uiEdge);
        u16 c = b[i].col ? b[i].col : (b[i].dim ? GREY : (on ? YELLOW : WHITE));
        if (b[i].w < 40) {                                // little keys (the name keyboard): plain rounded keys
            shape(buf, b[i].x, b[i].y + dn, b[i].w, b[i].h, 3, 3, 0, edge);
            shape(buf, b[i].x + 1, b[i].y + dn + 1, b[i].w - 2, b[i].h - 2, 2, 2, BTN_GRAD, 0);
            text(buf, b[i].x + (b[i].w - textW(b[i].label, 1)) / 2, b[i].y + dn + (b[i].h - FONT_H) / 2 + 1, b[i].label, c, 1);
            continue;
        }
        int cap = b[i].h * 45 / 100; if (cap < 10) cap = 10;
        // the label must sit inside the body: tighten the letters, then shrink the cap, until it does
        squeeze = 0; int tw = textW(b[i].label, 1);
        while (tw > b[i].w - cap - 8 && squeeze < 2) { squeeze++; tw = textW(b[i].label, 1); }
        while (tw > b[i].w - cap - 8 && cap > 7) cap--;
        markerShapeCap(buf, b[i].x, b[i].y + dn, b[i].w, b[i].h, cap, edge, BTN_GRAD);
        int bx = b[i].x + cap + 1, bw = b[i].w - cap - 3;
        text(buf, bx + (bw - tw) / 2, b[i].y + dn + (b[i].h - FONT_H) / 2 + 1, b[i].label, c, 1);
        squeeze = 0;
    }
}
// The power marker: a marker pointing where the throw goes, growing with power and filling
// from white through gold to red. (x0,gy0) is the back of the cap, (ux,uy) the direction.
void powerMarker(int x0, int gy0, float ux, float uy, float len, float power) {
    static u16 spr[200 * 14];
    int L = (int)len; if (L < 24) L = 24; if (L > 200) L = 200;
    int H = 14, cap = 10, nib = 9;
    int pr = (int)(power * 31);
    u16 fill = power <= 0 ? COL(5, 5, 5) : COL(31, 31 - pr * 2 / 3, pr < 16 ? 31 - pr * 2 : 0);
    btnGrad();
    for (int j = 0; j < H; j++) for (int i = 0; i < L; i++) {
        u16 c = 0; int dy = j - H / 2; if (dy < 0) dy = -dy - 1;
        if (i >= L - nib) {                                  // the nib: a point
            int half = (L - 1 - i) * (H / 2) / nib;
            if (dy <= half) c = dy >= half - 1 ? WHITE : (power > 0 ? fill : COL(10, 10, 10));
            if (i >= L - 3 && dy <= 1) c = COL(31, 28, 6);
        } else if (i < cap) {                                // the cap, rounded at the back
            int in = inset(j, H, 6); if (i >= in) c = (i == in || j < 2 || j >= H - 2 || i == cap - 1) ? WHITE : BTN_GRAD[j * 32 / H];
        } else c = (j < 2 || j >= H - 2) ? WHITE : (power > 0 && (i - cap) < (L - nib - cap) * power + 1 ? fill : BTN_GRAD[j * 32 / H]);
        spr[j * L + i] = c ? (c | 0x8000) : 0;
    }
    float cx = x0 + ux * L / 2, cy = gy0 + uy * L / 2;
    blitCore(spr, L, H, (int)cx, (int)cy, fatan2r(uy, ux), 1.0f);
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

// ── the main menu's side icons (the website's SLOT / PLINQU33PH / ARCADE and SETTINGS / THEMES) ──
int textSW(const char *t) { int w = 0; for (; *t; t++) { int ch = *t; if (ch < 32 || ch > 126) ch = '?'; w += fonts_w[ch - 32]; } return w; }
void textS(u16 *buf, int x, int y, const char *t, u16 col) {                     // small caption font, no outline
    for (; *t; t++) {
        int ch = *t; if (ch < 32 || ch > 126) ch = '?';
        const u16 *rows = &fonts_rows[(ch - 32) * FONTS_H];
        for (int j = 0; j < FONTS_H; j++) { int py = y + j; if ((unsigned)py >= SH) continue;
            for (u32 m = rows[j]; m; m &= m - 1) { int px = x + __builtin_ctz(m); if ((unsigned)px < SW) buf[py * SW + px] = col; } }
        x += fonts_w[ch - 32];
    }
}
static void dot(u16 *buf, int cx, int cy, int r, u16 c) {
    for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r + r / 2) { int x = cx + i, y = cy + j; if ((unsigned)x < SW && (unsigned)y < SH) buf[y * SW + x] = c; }
}
static void thickLine(u16 *buf, float x0, float y0, float x1, float y1, int r, u16 c) {
    int n = (int)(fabsf_(x1 - x0) + fabsf_(y1 - y0)) + 1;
    for (int i = 0; i <= n; i++) dot(buf, (int)(x0 + (x1 - x0) * i / n + 0.5f), (int)(y0 + (y1 - y0) * i / n + 0.5f), r, c);
}
// The website's icons are 24x24 SVGs; here each unit is s/24 of the icon's size.
void iconPlinko(u16 *buf, int x, int y, int s, u16 c) {
    static const signed char P[8][2] = { {6,6},{12,6},{18,6},{9,11},{15,11},{6,16},{12,16},{18,16} };
    for (int i = 0; i < 8; i++) dot(buf, x + P[i][0] * s / 24, y + P[i][1] * s / 24, s / 13 + iconFat, c);
    dot(buf, x + 12 * s / 24, y + 21 * s / 24, s / 9 + iconFat, iconFat ? c : uiDotRed);
}
void iconGear(u16 *buf, int x, int y, int s, u16 c) {
    float k = s / 24.0f; int r = 1 + iconFat;
    for (int j = -3 * s / 24 - 3; j <= 3 * s / 24 + 3; j++) for (int i = -3 * s / 24 - 3; i <= 3 * s / 24 + 3; i++) {
        int d = i * i + j * j, R = 3 * s / 24; if (d <= (R + 1 + iconFat) * (R + 1 + iconFat) && d >= (R - 1 - iconFat) * (R - 1 - iconFat)) dot(buf, x + 12 * s / 24 + i, y + 12 * s / 24 + j, 0, c); }
    static const float L[8][4] = { {12,3,12,5},{12,19,12,21},{3,12,5,12},{19,12,21,12},{5.6f,5.6f,7,7},{17,17,18.4f,18.4f},{18.4f,5.6f,17,7},{7,17,5.6f,18.4f} };
    for (int i = 0; i < 8; i++) thickLine(buf, x + L[i][0] * k, y + L[i][1] * k, x + L[i][2] * k, y + L[i][3] * k, r, c);
}
static void bez(float *o, int *n, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
    for (int i = 1; i <= 6; i++) { float t = i / 6.0f, u = 1 - t;
        o[*n * 2] = u*u*u*x0 + 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t*x3; o[*n * 2 + 1] = u*u*u*y0 + 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t*y3; (*n)++; }
}
void iconPalette(u16 *buf, int x, int y, int s, u16 c) {     // the website's palette path, traced
    static float P[100]; static int n;
    if (!n) {
        for (int i = 0; i <= 16; i++) { float t = -1.5707963f - i * 3.1415927f / 16; P[n * 2] = 12 + 9 * fcos(t); P[n * 2 + 1] = 12 + 9 * fsin(t); n++; }
        bez(P, &n, 12, 21, 13.4f, 21, 14, 20, 14, 19);
        bez(P, &n, 14, 19, 14, 17.6f, 15, 17, 16.4f, 17);
        P[n * 2] = 19; P[n * 2 + 1] = 17; n++;
        for (int i = 1; i <= 4; i++) { float t = 1.5707963f - i * 1.5707963f / 4; P[n * 2] = 19 + 3 * fcos(t); P[n * 2 + 1] = 14 + 3 * fsin(t); n++; }
        bez(P, &n, 22, 14, 22, 9, 17.5f, 5, 12, 5);   // (the website's path ends at 12,5 and closes to 12,3)
        P[n * 2] = 12; P[n * 2 + 1] = 3; n++;
    }
    float k = s / 24.0f; int r = 1 + iconFat;
    for (int i = 1; i < n; i++) thickLine(buf, x + P[i * 2 - 2] * k, y + P[i * 2 - 1] * k, x + P[i * 2] * k, y + P[i * 2 + 1] * k, r, c);
    dot(buf, x + 8 * s / 24, y + 10 * s / 24, s / 18 + 1 + iconFat, c); dot(buf, x + 12 * s / 24, y + (int)(7.5f * k), s / 18 + 1 + iconFat, c); dot(buf, x + 16 * s / 24, y + 10 * s / 24, s / 18 + 1 + iconFat, c);
}

// a sprite with a solid ink outline round it (so pictures pop on CARTOON's light paper)
void blitInk(u16 *buf, const u16 *spr, int w, int h, int x, int y, u16 ink) {
    static const signed char O[8][2] = { {-2,0},{2,0},{0,-2},{0,2},{-1,-1},{1,-1},{-1,1},{1,1} };
    for (int k = 0; k < 8; k++) for (int j = 0; j < h; j++) { int yy = y + j + O[k][1]; if ((unsigned)yy >= SH) continue;
        for (int i = 0; i < w; i++) { int xx = x + i + O[k][0]; if ((unsigned)xx < SW && (spr[j * w + i] & 0x8000)) buf[yy * SW + xx] = ink; } }
    blit(buf, spr, w, h, x, y);
}

// ── markers with the theme's finish ─────────────────────────────────────────
// CARTOON: a bold outline in the marker's own colour. NEON (or the shop's GLOW): a soft glow
// round it. col: 0 red, 1 green, 2 blue (-1 = no colour of its own).
int gGlowShop;
static const u16 MK_COLS[3] = { COL(31, 7, 7), COL(7, 29, 9), COL(9, 14, 31) };
void drawMarkerFx(const u16 *spr, int w, int h, int cx, int cy, float ang, float scale, int col) {
    int neon = sv.theme == 4, cart = sv.theme == 1, st = gStip;
    if (neon || gGlowShop) {
        u16 g = neon ? COL(8, 31, 15) : (col >= 0 ? mix(MK_COLS[col], WHITE, 1, 2) : COL(31, 31, 20));
        static const signed char R3[12][2] = { {-3,0},{3,0},{0,-3},{0,3},{-2,-2},{2,-2},{-2,2},{2,2},{-3,-1},{3,1},{-1,3},{1,-3} };
        gSil = mix(g, BLACK, 1, 2); gStip = 1;
        for (int k = 0; k < 12; k++) blitCore(spr, w, h, cx + R3[k][0], cy + R3[k][1], ang, scale);
        gSil = g; gStip = 0;
        for (int k = 0; k < 4; k++) blitCore(spr, w, h, cx + (k == 0) - (k == 1), cy + (k == 2) - (k == 3), ang, scale);
    }
    if (cart && col >= 0) {
        static const signed char R2[8][2] = { {-2,0},{2,0},{0,-2},{0,2},{-1,-1},{1,-1},{-1,1},{1,1} };
        gSil = MK_COLS[col]; gStip = 0;
        for (int k = 0; k < 8; k++) blitCore(spr, w, h, cx + R2[k][0], cy + R2[k][1], ang, scale);
    }
    gSil = 0; gStip = st;
    blitCore(spr, w, h, cx, cy, ang, scale);
}
