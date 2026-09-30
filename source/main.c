// ════════════════════════════════════════════════════════════════════════
//  QU33PH DS — the main Qu33ph game for the Nintendo DS
//  Bottom screen (touch): the table. Flick a marker up it with the stylus.
//  Top screen: round, score, best, and what just happened.
//
//  5 rounds x 3 markers.
//   zone points ... 1 near / 2 middle / 3 far, for each marker still on the table
//   PEEF ......... slid off the far edge: -2
//   MEGA ......... stopped on the red dot on the index card: +10
//   QU33PH ....... all three markers touching at the end of a round: +5
// ════════════════════════════════════════════════════════════════════════
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>

#define W 256
#define H 192
static u16 back[W * H];                 // draw here, copy to the screen once per frame
static u16 *vram;
#define COL(r, g, b) (RGB15(r, g, b) | BIT(15))

// ── table layout (pixels) ─────────────────────────────────────────────────
#define TX0     22          // left rail
#define TX1     234         // right rail
#define TY_EDGE 10          // far edge: past this = PEEF
#define ZONE3   66          // above this line = 3 points
#define ZONE2   122         // above this line = 2 points
#define START_X 128
#define START_Y 172
#define MR      5           // marker radius (for collisions)
#define ROUNDS  5

typedef struct { float x, y, vx, vy; int alive, thrown; u16 col; } Marker;
static Marker mk[3];
static int cur, round_no, score, best, card_x, card_y, frame;
static int round_pts, round_mega, round_qu33ph;
static char msg[40];

enum { TITLE, AIM, ROLLING, ROUND_END, GAME_OVER };
static int state = TITLE;

// ── tiny maths (no libm needed) ───────────────────────────────────────────
static float fsqrt(float v) {
    if (v <= 0) return 0;
    float x = v > 1 ? v : 1;
    for (int i = 0; i < 10; i++) x = 0.5f * (x + v / x);
    return x;
}
static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by; return dx * dx + dy * dy;
}

// ── drawing ───────────────────────────────────────────────────────────────
static void rect(int x, int y, int w, int h, u16 c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) { u16 *p = back + (y + j) * W + x; for (int i = 0; i < w; i++) p[i] = c; }
}
static void disc(int cx, int cy, int r, u16 c) {
    for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++)
        if (i * i + j * j <= r * r) { int x = cx + i, y = cy + j; if (x >= 0 && x < W && y >= 0 && y < H) back[y * W + x] = c; }
}
static void dotline(int x0, int y0, int x1, int y1, u16 c) {
    int n = 24;
    for (int i = 0; i <= n; i += 2) rect(x0 + (x1 - x0) * i / n - 1, y0 + (y1 - y0) * i / n - 1, 2, 2, c);
}
// a marker seen from above: coloured body, black cap at the far end, dark outline
static void draw_marker(int x, int y, u16 body) {
    rect(x - 4, y - 10, 8, 20, COL(2, 2, 2));
    rect(x - 3, y - 9, 6, 18, body);
    rect(x - 3, y - 9, 6, 5, COL(1, 1, 1));
    rect(x - 2, y - 2, 2, 8, COL(31, 31, 31));      // little highlight
}
static void draw_table(void) {
    rect(0, 0, W, H, COL(3, 3, 5));                             // floor beyond the table
    rect(TX0, TY_EDGE, TX1 - TX0, H - TY_EDGE, COL(20, 12, 6)); // wood
    for (int x = TX0 + 16; x < TX1; x += 16) rect(x, TY_EDGE, 1, H - TY_EDGE, COL(15, 8, 4));  // planks
    rect(TX0, TY_EDGE, TX1 - TX0, 2, COL(8, 4, 2));             // far edge lip
    rect(TX0 - 4, TY_EDGE, 4, H - TY_EDGE, COL(10, 5, 2));      // rails
    rect(TX1, TY_EDGE, 4, H - TY_EDGE, COL(10, 5, 2));
    for (int x = TX0; x < TX1; x += 6) { rect(x, ZONE3, 3, 1, COL(28, 22, 14)); rect(x, ZONE2, 3, 1, COL(28, 22, 14)); }
    // the index card with the red MEGA dot
    rect(card_x - 16, card_y - 10, 32, 20, COL(2, 2, 2));
    rect(card_x - 15, card_y - 9, 30, 18, COL(31, 31, 30));
    for (int k = -5; k <= 5; k += 5) rect(card_x - 13, card_y + k, 26, 1, COL(20, 24, 31));
    disc(card_x, card_y, 4, COL(29, 3, 3));
}

// ── top screen ────────────────────────────────────────────────────────────
static int top_dirty = 1;
static void draw_top(void) {
    if (!top_dirty) return;
    top_dirty = 0;
    consoleClear();
    printf("\n\n        Q U 3 3 P H\n");
    printf("            DS\n\n");
    if (state == TITLE) {
        printf("   Flick markers up the table\n");
        printf("   with the stylus.\n\n");
        printf("   Far zone 3  Middle 2  Near 1\n");
        printf("   Off the edge: PEEF  -2\n");
        printf("   Red dot: MEGA  +10\n");
        printf("   All 3 touching: QU33PH +5\n\n");
        printf("        TAP TO START\n");
    } else {
        printf("   ROUND   %d / %d\n", round_no, ROUNDS);
        printf("   SCORE   %d\n", score);
        printf("   BEST    %d\n\n", best);
        if (state == AIM || state == ROLLING) printf("   Marker  %d / 3\n\n", cur + 1);
        if (msg[0]) printf("   %s\n", msg);
        if (state == ROUND_END) {
            printf("\n   Round points  %d\n", round_pts);
            if (round_mega)   printf("   MEGA!  +%d\n", 10 * round_mega);
            if (round_qu33ph) printf("   QU33PH!  +5\n");
            printf("\n        TAP TO CONTINUE\n");
        }
        if (state == GAME_OVER) {
            printf("\n   FINAL SCORE  %d\n", score);
            if (score >= best && score > 0) printf("   NEW BEST!\n");
            printf("\n        TAP TO PLAY AGAIN\n");
        }
    }
}

// ── game flow ─────────────────────────────────────────────────────────────
static const u16 *marker_cols(void) {
    static u16 c[3];
    c[0] = COL(28, 4, 4); c[1] = COL(4, 22, 6); c[2] = COL(6, 10, 30);
    return c;
}
static void new_round(void) {
    const u16 *c = marker_cols();
    for (int i = 0; i < 3; i++) { mk[i].alive = 0; mk[i].thrown = 0; mk[i].vx = mk[i].vy = 0; mk[i].col = c[i]; }
    cur = 0;
    card_x = 70 + rand() % 116;
    card_y = 26 + rand() % 22;
    mk[0].x = START_X; mk[0].y = START_Y; mk[0].alive = 1;
    msg[0] = 0; state = AIM; top_dirty = 1;
}
static void new_game(void) { score = 0; round_no = 1; new_round(); }

static int all_stopped(void) {
    for (int i = 0; i < 3; i++) if (mk[i].alive && (mk[i].vx != 0 || mk[i].vy != 0)) return 0;
    return 1;
}
static void score_round(void) {
    round_pts = 0; round_mega = 0; round_qu33ph = 0;
    int on = 0;
    for (int i = 0; i < 3; i++) {
        if (!mk[i].alive) { round_pts -= 2; continue; }
        on++;
        round_pts += mk[i].y < ZONE3 ? 3 : (mk[i].y < ZONE2 ? 2 : 1);
        if (dist2(mk[i].x, mk[i].y, card_x, card_y) <= 7 * 7) round_mega++;
    }
    if (on == 3) {
        int pairs = 0; float t = (2 * MR + 4) * (2 * MR + 4);
        for (int i = 0; i < 2; i++) for (int j = i + 1; j < 3; j++) if (dist2(mk[i].x, mk[i].y, mk[j].x, mk[j].y) <= t) pairs++;
        if (pairs >= 2) round_qu33ph = 1;
    }
    round_pts += 10 * round_mega + 5 * round_qu33ph;
    score += round_pts;
    if (score > best) best = score;
}

// ── physics: slide with friction, bounce off rails, knock into each other ─
static void physics(void) {
    for (int i = 0; i < 3; i++) {
        Marker *m = &mk[i];
        if (!m->alive || !m->thrown) continue;
        m->x += m->vx; m->y += m->vy;
        m->vx *= 0.955f; m->vy *= 0.955f;
        float sp = fsqrt(m->vx * m->vx + m->vy * m->vy);
        if (sp < 0.12f) { m->vx = m->vy = 0; }
        if (m->x < TX0 + MR) { m->x = TX0 + MR; m->vx = -m->vx * 0.6f; }
        if (m->x > TX1 - MR) { m->x = TX1 - MR; m->vx = -m->vx * 0.6f; }
        if (m->y > H - MR)   { m->y = H - MR;   m->vy = -m->vy * 0.5f; }
        if (m->y < TY_EDGE) {                       // off the far edge
            m->alive = 0; m->vx = m->vy = 0;
            snprintf(msg, sizeof msg, "PEEF!  -2");
            top_dirty = 1;
        }
    }
    for (int i = 0; i < 2; i++) for (int j = i + 1; j < 3; j++) {
        Marker *a = &mk[i], *b = &mk[j];
        if (!a->alive || !b->alive || !a->thrown || !b->thrown) continue;
        float d2 = dist2(a->x, a->y, b->x, b->y), min = 2 * MR;
        if (d2 >= min * min || d2 < 0.0001f) continue;
        float d = fsqrt(d2), nx = (b->x - a->x) / d, ny = (b->y - a->y) / d;
        float push = (min - d) * 0.5f;                 // separate them
        a->x -= nx * push; a->y -= ny * push; b->x += nx * push; b->y += ny * push;
        float rel = (a->vx - b->vx) * nx + (a->vy - b->vy) * ny;
        if (rel > 0) {                                 // moving toward each other: trade momentum
            float k = rel * 0.95f;
            a->vx -= k * nx; a->vy -= k * ny; b->vx += k * nx; b->vy += k * ny;
        }
    }
}

// ── touch: drag from the marker and let go to flick it ───────────────────
static int dragging, hx[6], hy[6], hn;
static void input(void) {
    scanKeys();
    int down = keysDown(), held = keysHeld(), up = keysUp();
    touchPosition t;
    if (state == TITLE || state == ROUND_END || state == GAME_OVER) {
        if (down & (KEY_TOUCH | KEY_A | KEY_START)) {
            if (state == TITLE) { srand(frame); new_game(); }
            else if (state == ROUND_END) {
                if (round_no >= ROUNDS) { state = GAME_OVER; top_dirty = 1; }
                else { round_no++; new_round(); }
            } else new_game();
        }
        return;
    }
    if (state != AIM) return;
    if (down & KEY_TOUCH) {
        touchRead(&t);
        if (dist2(t.px, t.py, mk[cur].x, mk[cur].y) < 30 * 30) { dragging = 1; hn = 0; }
    }
    if (dragging && (held & KEY_TOUCH)) {
        touchRead(&t);
        for (int k = 5; k > 0; k--) { hx[k] = hx[k - 1]; hy[k] = hy[k - 1]; }
        hx[0] = t.px; hy[0] = t.py; if (hn < 6) hn++;
    }
    if (dragging && (up & KEY_TOUCH)) {
        dragging = 0;
        if (hn >= 3) {
            int k = hn - 1 < 4 ? hn - 1 : 4;           // speed over the last few frames of the swipe
            float vx = (float)(hx[0] - hx[k]) / k, vy = (float)(hy[0] - hy[k]) / k;
            if (vy < -0.8f) {                          // only upward flicks throw
                // a real flick moves the stylus ~10-30 px a frame: scale it so a
                // gentle flick lands near, a medium one mid-table, and only a
                // hard one risks sliding off the far edge
                vx *= 0.33f; vy *= 0.33f;
                float sp = fsqrt(vx * vx + vy * vy), cap = 8.5f;
                if (sp > cap) { vx *= cap / sp; vy *= cap / sp; }
                mk[cur].vx = vx; mk[cur].vy = vy; mk[cur].thrown = 1;
                msg[0] = 0; state = ROLLING; top_dirty = 1;
            }
        }
    }
}

static void render(void) {
    draw_table();
    // markers still waiting to be thrown, lined up in the bottom-left corner
    for (int i = cur + 1; i < 3; i++) if (state == AIM || state == ROLLING) draw_marker(8, 150 + (i - cur) * 22 - 22, mk[i].col);
    for (int i = 0; i < 3; i++) if (mk[i].alive) draw_marker((int)mk[i].x, (int)mk[i].y, mk[i].col);
    if (state == AIM && dragging && hn > 0) dotline((int)mk[cur].x, (int)mk[cur].y, hx[0], hy[0], COL(31, 31, 31));
    dmaCopy(back, vram, sizeof back);
}

int main(void) {
    // main engine (the bitmap table) on the touch screen, text console on the top screen
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vram = bgGetGfxPtr(bg);
    lcdMainOnBottom();
    consoleDemoInit();
    new_round(); state = TITLE; top_dirty = 1;

    while (1) {
        frame++;
        input();
        if (state == ROLLING) {
            physics();
            if (all_stopped()) {
                cur++;
                if (cur >= 3) { score_round(); state = ROUND_END; top_dirty = 1; }
                else { mk[cur].x = START_X; mk[cur].y = START_Y; mk[cur].alive = 1; state = AIM; top_dirty = 1; }
            }
        }
        render();
        draw_top();
        swiWaitForVBlank();
    }
    return 0;
}
