// bowl.c — QU33PH BOWLING (the website's bowling.html): ten frames, marker pins.
//
// The lane art is exactly the DS's 2:3, so it fills both screens: the pin deck and the
// cabinet's HIGH SCORE and MEGA boxes on the top screen, the foul line, your marker and the
// ball-return queue on the bottom. The lane perspective, physics (hook, carry, the capsule-
// shaped marker, pin chain reactions), scoring, 10th-frame chair and MEGA QU33PH are the
// website's, in its own units.
//
// CONTROLS: slide sideways on the bottom screen to spot your marker, then flick up to roll
// (faster flick = more power, sideways flick = hook). Or: D-pad left/right to spot, hold A for
// power and let go. L/R (or tap the round button) turns the marker: VERT / ANGLED / FLAT.
// START pauses (SELECT quits). X music, Y sound effects.
#include "qu.h"
#include "assets_bowl.h"

#define AWf 256.0f
#define AHf 384.0f
#define NEAR_Y 0.830f
#define NEAR_HW 0.395f
#define LANE_CX 0.4900f
#define FAR_Y 0.360f
#define FAR_HW 0.094f
#define RET_Y 0.866f
#define RET_X0 0.2600f
#define RET_X1 0.7240f
#define HS_X 0.591f
#define HS_Y 0.2585f
#define BOX_L 0.233f
#define BOX_R 0.752f
#define BOX_Y 0.2585f
#define LANE_LEN 7.0f
#define PIN_R 0.080f
#define BALL_R 0.076f
#define BALL_HL 0.165f
#define PIN_D 0.215f
#define ROW_D 0.325f
#define HEAD_Y 5.95f
#define CHAIR_Y (HEAD_Y - 0.62f)
#define CHAIR_R 0.135f
#define MEGA_CHANCE 0.42f
#define BASE_HOOK -0.085f
#define DT (1.3f / 60.0f)                  // the website's real time, played 30% quicker on the DS
static const float KK = NEAR_HW / FAR_HW - 1, S1 = FAR_HW / NEAR_HW;

typedef struct { float X, Y, vx, vy, rot, vrot, sink; int col, down, gone; } Pin;
static Pin pins[10];
static const float LAYOUT[10][2] = { {0,0}, {-PIN_D,1}, {PIN_D,1}, {-2*PIN_D,2}, {0,2}, {2*PIN_D,2}, {-3*PIN_D,3}, {-PIN_D,3}, {PIN_D,3}, {3*PIN_D,3} };
static const int PIN_COL[10] = { 2, 0, 1, 0, 1, 0, 1, 0, 1, 0 };          // red 0, green 1, blue 2
static const int QUEUE[10] = { 0, 1, 0, 1, 0, 1, 0, 1, 0, 2 };            // blue always last
static struct { float X, Y, vx, vy, spin, rot, carry, wob; int col, live, gutter; } ball; static int haveBall;
static const u16 *chairImg(void) { return chair; }       // the main game's chair picture
static struct { float X, Y, vx, vy, rot; int shoved; } chr; static int haveChair;
static struct { float x0, y0, t; } megaFly; static int flying;
enum { PH_AIM, PH_ROLL, PH_MEGA, PH_OVER };
static int phase, frameNo, rollNo, rolls[24], nRolls, megaRight, megaWon, standStart, orientIdx = 1, msgT, coinsWon, newBest;
static float aimX, settleT, power; static int charging, ph;
static char msg[16]; static u16 msgCol;
static u16 palT[256];
static int dragging, dsx, dsy, dnx, dny, dT0, bFrames;

typedef struct { const char *label; float rot, hook, speed, carry; } Ori;
static const Ori ORI[3] = { { "VERT", 0, 1.65f, 0.93f, 0.84f }, { "ANGLED", 0.7853982f, 1.00f, 1.00f, 1.00f }, { "FLAT", 1.5707963f, 0.42f, 1.07f, 1.18f } };

void bowlThemeChanged(void) { if (!pakIs(BOWL_PAK)) return; for (int i = 0; i < 256; i++) palT[i] = themeTint(bw_pal[i], sv.theme); }
int bowlEnter(void) { if (!pakUse(BOWL_PAK, BOWL_PAK_SIZE, BOWL_PAK_ID)) return 0; bowlThemeChanged(); return 1; }

// ── the website's lane perspective ────────────────────────────────────────
static float scaleAt(float v) { return 1 / (1 + v * KK); }
static void proj(float u, float v, float *x, float *y, float *s) {
    float sc = scaleAt(v); *s = sc;
    *x = AWf * (LANE_CX + u * NEAR_HW * sc);
    *y = AHf * (NEAR_Y + (FAR_Y - NEAR_Y) * (1 - sc) / (1 - S1));
}
static int capsuleHit(float cx, float cy, float r, float *nx, float *ny) {
    float ax = fsin(ball.rot), ay = fcos(ball.rot);
    float t = (cx - ball.X) * ax + (cy - ball.Y) * ay; if (t < -BALL_HL) t = -BALL_HL; if (t > BALL_HL) t = BALL_HL;
    float dx = cx - (ball.X + ax * t), dy = cy - (ball.Y + ay * t), d = fsqrt(dx * dx + dy * dy);
    if (d >= r + BALL_R || d <= 0.0001f) return 0;
    *nx = dx / d; *ny = dy / d; return 1;
}
static float ballHalfWidth(float rot) { return fabsf_(fsin(rot)) * BALL_HL + BALL_R; }

// ── sounds ────────────────────────────────────────────────────────────────
static void sfxThrowB(void) {
    const u8 *d[18] = { bw_t0, bw_t1, bw_t2, bw_t3, bw_t4, bw_t5, bw_t6, bw_t7, bw_t8, bw_t9, bw_t10, bw_t11, bw_t12, bw_t13, bw_t14, bw_t15, bw_t16, bw_t17 };
    const int l[18] = { BW_T0_LEN, BW_T1_LEN, BW_T2_LEN, BW_T3_LEN, BW_T4_LEN, BW_T5_LEN, BW_T6_LEN, BW_T7_LEN, BW_T8_LEN, BW_T9_LEN, BW_T10_LEN, BW_T11_LEN,
                        BW_T12_LEN, BW_T13_LEN, BW_T14_LEN, BW_T15_LEN, BW_T16_LEN, BW_T17_LEN };
    int i = rand() % 18; playAdpcm(d[i], l[i], 12000, 80);
}
static int pinSfxT;
static void sfxPin(void) {
    if (pinSfxT > 0) return;                            // a strike's cascade would otherwise flood all the channels
    pinSfxT = 4;
    const u8 *d[6] = { bw_p0, bw_p1, bw_p2, bw_p3, bw_p4, bw_p5 };
    const int l[6] = { BW_P0_LEN, BW_P1_LEN, BW_P2_LEN, BW_P3_LEN, BW_P4_LEN, BW_P5_LEN };
    int i = rand() % 6; playAdpcm(d[i], l[i], 12000, 80);
}
static void say(const char *t, u16 c) { strncpy(msg, t, 15); msg[15] = 0; msgCol = c; msgT = 95; }

// ── the game ──────────────────────────────────────────────────────────────
static void rackPins(void) {
    for (int i = 0; i < 10; i++) pins[i] = (Pin){ LAYOUT[i][0], HEAD_Y + LAYOUT[i][1] * ROW_D, 0, 0, 0, 0, 0, PIN_COL[i], 0, 0 };
}
static int standing(void) { int n = 0; for (int i = 0; i < 10; i++) if (!pins[i].gone && !pins[i].down) n++; return n; }
static int isWashout(void) {
    if (pins[0].down) return 0;
    int l = 0, r = 0;
    for (int i = 1; i < 10; i++) { Pin *p = &pins[i]; if (p->gone || p->down) continue; if (p->X < -0.001f) l = 1; else if (p->X > 0.001f) r = 1; }
    return l && r;
}
static int scoreTotal(void) {
    int s = 0, i = 0;
    for (int f = 0; f < 10 && i < nRolls; f++) {
        int a = rolls[i], b = i + 1 < nRolls ? rolls[i + 1] : 0, c = i + 2 < nRolls ? rolls[i + 2] : 0;
        if (a == 10) { s += 10 + b + c; i += 1; }
        else if (a + b == 10 && i + 1 < nRolls) { s += 10 + c; i += 2; }
        else { s += a + b; i += 2; }
    }
    return s;
}
static int finalScore(void) { return scoreTotal() + (megaWon ? 33 : 0); }
static void newGame(void) {
    frameNo = rollNo = nRolls = 0; aimX = 0; megaRight = rand() & 1; megaWon = 0; flying = 0; haveChair = 0;
    rackPins(); haveBall = 0; phase = PH_AIM; msgT = 0; standStart = 10; charging = dragging = 0;
}
static void setChair(void) {
    haveChair = frameNo == 9 && rollNo == 2 && standing() == 10 && !megaWon;
    if (haveChair) { chr.X = 0; chr.Y = CHAIR_Y; chr.vx = chr.vy = chr.rot = 0; chr.shoved = 0; }
}
static void rackFresh(void) { rackPins(); standStart = 10; setChair(); }
static void sweepAndStand(void) { for (int i = 0; i < 10; i++) if (pins[i].down) pins[i].gone = 1; standStart = standing(); }
static void readyNext(void) { haveBall = 0; phase = PH_AIM; }
static void endGame(void) {
    phase = PH_OVER;
    int s = finalScore();
    newBest = s > sv.arcadeBest[ARC_BOWLING];
    if (newBest) sv.arcadeBest[ARC_BOWLING] = s;
    sv.arcadePlays[ARC_BOWLING]++;
    int c = s / 20; if (c > 20) c = 20;
    int before = sv.coins; if (c > 0) addCoins(c); coinsWon = sv.coins - before;
    saveWrite();
    screen = S_BOWL_OVER;
}
static void endRoll(int fromMega) {
    if (phase == PH_MEGA && !fromMega) return;
    int felled = standStart - standing(), cleared = standing() == 0;
    if (nRolls < 24) rolls[nRolls++] = felled;
    int strike = felled == 10;
    if (!fromMega) {
        if (rollNo == 0 && strike) { say("QU33PH!", COL(31, 26, 9)); sfxPlop(); }
        else if (rollNo > 0 && cleared) { say("SPARE", COL(7, 26, 15)); sfxPlop(); }
        else if (felled == 0 && haveBall && ball.gutter) { say("PEEF", COL(31, 11, 11)); playAdpcm(bw_peef, BW_PEEF_LEN, 12000, 80); }
        else if (isWashout()) { say("WASHOUT", COL(31, 17, 5)); playAdpcm(bw_wash, BW_WASH_LEN, 12000, 80); }
    }
    if (frameNo < 9) {
        if (rollNo == 0 && !strike) { rollNo = 1; sweepAndStand(); setChair(); readyNext(); }
        else { frameNo++; rollNo = 0; rackFresh(); readyNext(); }
        return;
    }
    if (rollNo == 0) { rollNo = 1; if (strike) rackFresh(); else sweepAndStand(); setChair(); readyNext(); }
    else if (rollNo == 1) {
        int openedWithStrike = nRolls >= 2 && rolls[nRolls - 2] == 10;
        if (openedWithStrike || cleared) { rollNo = 2; rackFresh(); readyNext(); }
        else endGame();
    } else endGame();
}
static void throwBall(float dx, float pw) {
    const Ori *o = &ORI[orientIdx];
    ball.X = aimX; ball.Y = 0; ball.vx = dx * 1.7f; ball.vy = 3.5f * pw * o->speed;
    ball.spin = (dx * 1.5f + BASE_HOOK) * o->hook; ball.rot = o->rot; ball.carry = o->carry;
    ball.col = QUEUE[frameNo]; ball.live = 1; ball.wob = 0; ball.gutter = 0; haveBall = 1;
    phase = PH_ROLL; settleT = 0;
    sfxThrowB();
}
static void knock(Pin *p, float nx, float ny, float f, float spinBias) {
    p->vx += nx * f; p->vy += ny * f;
    if (!p->down) { p->down = 1; p->rot = (frand() - 0.5f) * 0.5f; p->vrot = (frand() - 0.5f) * 9 + spinBias; sfxPin(); }
}
void updateBowl(void) {
    if (pinSfxT > 0) pinSfxT--;
    if (msgT > 0) msgT--;
    bFrames++;
    const float dt = DT;
    if (phase == PH_ROLL && haveBall && ball.live) {
        float grip = ball.vy / 2.2f; if (grip > 1) grip = 1;
        ball.vx += ball.spin * grip * dt * 2.4f;
        ball.vy -= ball.vy * 0.13f * dt;
        ball.X += ball.vx * dt; ball.Y += ball.vy * dt;
        ball.wob += ball.vy * dt * 7;
        float hw = ballHalfWidth(ball.rot);
        if (fabsf_(ball.X) > 1.0f - hw) {                     // PEEF: in the gutter, the roll is dead
            ball.X = (ball.X < 0 ? -1 : 1) * (1.0f - hw);
            ball.vx = ball.vy = ball.spin = 0; ball.gutter = 1; ball.live = 0; settleT = 0;
        }
        float nx, ny;
        if (haveChair && !chr.shoved && capsuleHit(chr.X, chr.Y, CHAIR_R, &nx, &ny)) {
            if (frand() < MEGA_CHANCE) {
                float x, y, s; proj(ball.X, ball.Y / LANE_LEN, &x, &y, &s);
                megaFly.x0 = x; megaFly.y0 = y; megaFly.t = 0; flying = 1; ball.live = 0; phase = PH_MEGA;
                chr.shoved = 1; chr.vy = 0.7f; sfxPlop();
            } else {
                chr.shoved = 1; chr.vy = ball.vy * 0.95f > 2.2f ? ball.vy * 0.95f : 2.2f; chr.vx = ball.vx * 0.7f;
                ball.vy *= 0.55f; sfxPin();
            }
        }
        for (int i = 0; i < 10; i++) { Pin *p = &pins[i]; if (p->gone) continue;
            if (capsuleHit(p->X, p->Y, PIN_R, &nx, &ny)) {
                float hit = (ball.vy * 0.80f + fabsf_(ball.vx) * 0.5f) * ball.carry; if (hit < 1.15f) hit = 1.15f;
                p->vx += nx * hit; p->vy += ny * hit;
                p->down = 1; p->rot = (frand() - 0.5f) * 0.5f; p->vrot = (frand() - 0.5f) * 9 - (nx > 0 ? 1 : -1) * 3;
                ball.vx += -nx * hit * 0.09f; ball.vy = ball.vy - 0.16f > 0.55f ? ball.vy - 0.16f : 0.55f;
                sfxPin();
            }
        }
        if (ball.Y > LANE_LEN + 0.6f || ball.vy < 0.22f) { ball.live = 0; settleT = 0; }
    }
    if (haveChair && chr.shoved && phase != PH_MEGA) {
        chr.X += chr.vx * dt; chr.Y += chr.vy * dt; chr.rot += chr.vx * dt * 2;
        chr.vy -= chr.vy * 1.5f * dt; chr.vx -= chr.vx * 1.5f * dt;
        for (int i = 0; i < 10; i++) { Pin *p = &pins[i]; if (p->gone || p->down) continue;
            float dx = p->X - chr.X, dy = p->Y - chr.Y;
            if (fsqrt(dx * dx + dy * dy) < CHAIR_R + PIN_R) {
                if (!dx) dx = 0.01f;
                if (!dy) dy = 0.01f;
                float d = fsqrt(dx * dx + dy * dy), f = chr.vy * 0.8f > 1.2f ? chr.vy * 0.8f : 1.2f;
                knock(p, dx / d, dy / d, f, 0);
            } }
        if (chr.Y > LANE_LEN + 1.2f) haveChair = 0;
    }
    if (phase == PH_MEGA && flying) {
        megaFly.t += dt;
        if (megaFly.t >= 1.05f) { megaWon = 1; say("MEGA QU33PH", COL(31, 9, 7)); sfxPlop(); flying = 0; haveBall = 0; endRoll(1); }
    }
    for (int i = 0; i < 10; i++) { Pin *p = &pins[i]; if (p->gone) continue;
        if (fabsf_(p->vx) > 0.001f || fabsf_(p->vy) > 0.001f) {
            p->X += p->vx * dt; p->Y += p->vy * dt;
            p->vx -= p->vx * 1.9f * dt; p->vy -= p->vy * 1.9f * dt;
            p->rot += p->vrot * dt; p->vrot -= p->vrot * 2.2f * dt;
            if (p->down) { p->sink += dt * 2.4f; if (p->sink > 1) p->sink = 1; }
            for (int j = 0; j < 10; j++) { Pin *q = &pins[j]; if (q == p || q->gone) continue;
                float ddx = q->X - p->X, ddy = q->Y - p->Y, d = fsqrt(ddx * ddx + ddy * ddy);
                if (d < PIN_R * 2 && d > 0.0001f) {
                    float nx = ddx / d, ny = ddy / d, push = (PIN_R * 2 - d) * 0.5f;
                    q->X += nx * push; p->X -= nx * push; q->Y += ny * push; p->Y -= ny * push;
                    float t = fsqrt(p->vx * p->vx + p->vy * p->vy) * 0.92f;    // the website's stronger chain reaction
                    if (t > 0.10f) { q->vx += nx * t; q->vy += ny * t;
                        if (!q->down) { q->down = 1; q->rot = (frand() - 0.5f) * 0.5f; q->vrot = (frand() - 0.5f) * 8; sfxPin(); } }
                } }
            if (fabsf_(p->X) > 1.25f || p->Y > LANE_LEN + 1.1f) p->gone = 1;
        } }
    if (phase == PH_ROLL && haveBall && !ball.live) {
        settleT += dt;
        int moving = 0;
        for (int i = 0; i < 10; i++) if (!pins[i].gone && fsqrt(pins[i].vx * pins[i].vx + pins[i].vy * pins[i].vy) > 0.14f) moving = 1;
        if (!moving || settleT > 1.3f) endRoll(0);
    }
}

// ── input ─────────────────────────────────────────────────────────────────
#define ORI_X 236                                  // (the website puts it bottom-centre; here that's where flicks start)
#define ORI_Y 170
#define ORI_R 13
void inputBowl(void) {
    if (screen == S_BOWL_PAUSE) {
        if (kDown & KEY_START) screen = S_BOWL;
        if (kDown & KEY_SELECT) goScreen(S_ARCADE);
        return;
    }
    if (kDown & KEY_START) { screen = S_BOWL_PAUSE; dragging = charging = 0; return; }
    if (kDown & KEY_X) musicToggle();
    if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
    if (phase != PH_AIM) { dragging = charging = 0; return; }
    if (kDown & KEY_L) orientIdx = (orientIdx + 2) % 3;
    if (kDown & KEY_R) orientIdx = (orientIdx + 1) % 3;
    if (kDown & KEY_TOUCH) {
        int dx = tX - ORI_X, dy = tY - ORI_Y;
        if (dx * dx + dy * dy <= (ORI_R + 4) * (ORI_R + 4)) { orientIdx = (orientIdx + 1) % 3; return; }   // the turn button
        dragging = 1; dsx = dnx = tX; dsy = dny = tY; dT0 = bFrames; charging = 0;
    }
    static float aimAtDown;
    if (kDown & KEY_TOUCH) aimAtDown = aimX;
    if (dragging && (kHeld & KEY_TOUCH)) {                 // sliding spots the marker, as on the website
        dnx = tX; dny = tY;
        float a = aimAtDown + (dnx - dsx) / AWf * 2.1f; aimX = a < -0.72f ? -0.72f : a > 0.72f ? 0.72f : a;
    }
    if (dragging && (kUp & KEY_TOUCH)) {
        dragging = 0;
        float dt = (bFrames - dT0) * 16.67f; if (dt < 40) dt = 40;
        float dx = (dnx - dsx) / AWf, dy = (dsy - dny) / AHf;
        if (dy >= 0.03f) { float pw = dy * 2.6f * (260 / dt) + 0.35f; throwBall(dx, pw < 0.35f ? 0.35f : pw > 1.35f ? 1.35f : pw); }
        return;
    }
    if (!dragging) {
        if (!charging) {
            if (kHeld & KEY_LEFT)  { aimX -= 0.02f; if (aimX < -0.72f) aimX = -0.72f; }
            if (kHeld & KEY_RIGHT) { aimX += 0.02f; if (aimX > 0.72f) aimX = 0.72f; }
        }
        if (kDown & KEY_A) { charging = 1; ph = 0; }
        if (charging) {
            ph++; float p = (ph % 64) / 32.0f; p = p < 1 ? p : 2 - p; power = 0.35f + p;
            if (kDown & KEY_B) charging = 0;
            else if (kUp & KEY_A) {                        // holding left/right as you let go adds hook
                charging = 0;
                throwBall((kHeld & KEY_LEFT) ? -0.12f : (kHeld & KEY_RIGHT) ? 0.12f : 0, power);
            }
        }
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
static const u16 *pinSpr(int c, int *w, int *h) {
    int cart = sv.theme == 1;
    if (cart) { *h = BW_CPIN0_H; if (c == 0) { *w = BW_CPIN0_W; return bw_cpin0; } if (c == 1) { *w = BW_CPIN1_W; return bw_cpin1; } *w = BW_CPIN2_W; return bw_cpin2; }
    *h = BW_PIN0_H; if (c == 0) { *w = BW_PIN0_W; return bw_pin0; } if (c == 1) { *w = BW_PIN1_W; return bw_pin1; } *w = BW_PIN2_W; return bw_pin2;
}
static void marker(int c, float x, float y, float h, float rot, int stip) {   // a marker 'h' pixels tall, centred at x,y
    int w, sh; const u16 *s = pinSpr(c, &w, &sh);
    gStip = stip; blitRotScale(s, w, sh, (int)x, (int)y, rot, h / sh); gStip = 0;
}
static void shadow(float x, float y, float h, float rot) {          // the website's capsule shadow, flattened
    float c = fcos(-rot), s = fsin(-rot);
    int hw = (int)(h * 0.14f) + 1, hl = (int)(h * 0.46f);
    for (int j = -hl; j <= hl; j++) for (int i = -hw; i <= hw; i++) {
        int st = hl - hw; int jj = j > st ? j - st : j < -st ? j + st : 0;
        if (i * i + jj * jj > hw * hw) continue;
        float px = i * c - j * s, py = (i * s + j * c) * 0.34f;
        gdark((int)(x + px), (int)(y + h * 0.09f + py));
    }
}
static void drawPins(void) {
    int order[10]; for (int i = 0; i < 10; i++) order[i] = i;
    for (int i = 0; i < 10; i++) for (int j = i + 1; j < 10; j++) if (pins[order[j]].Y > pins[order[i]].Y) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int k = 0; k < 10; k++) { Pin *p = &pins[order[k]]; if (p->gone) continue;
        float v = p->Y / LANE_LEN; if (v < 0 || v > 1.14f) continue;
        float x, y, s; proj(p->X, v < 1.13f ? v : 1.13f, &x, &y, &s);
        float h = AHf * 0.085f * s * 1.9f;
        if (p->down) { if (p->sink < 0.8f) marker(p->col, x, y + h * 0.16f, h * 0.92f, 1.5707963f + p->rot, p->sink > 0.4f); }
        else marker(p->col, x, y - h * 0.30f, h, 0, 0);
    }
}
static void fmtFrame(int f, char *marks, int *total) {             // the website's frameView, one frame
    int i = 0, run = 0; marks[0] = 0; *total = -1;
    for (int k = 0; k <= f; k++) {
        if (i >= nRolls) { if (k == f) return; else return; }
        int a = rolls[i], hb = i + 1 < nRolls, hc = i + 2 < nRolls, b = hb ? rolls[i + 1] : 0, c = hc ? rolls[i + 2] : 0;
        char m[12] = ""; int tot = -1;
        #define MK(v) ((v) == 0 ? '-' : '0' + (v))
        if (k < 9) {
            if (a == 10) { strcpy(m, "X"); if (hb && hc) { run += 10 + b + c; tot = run; } i += 1; }
            else if (hb && a + b == 10) { sprintf(m, "%c /", MK(a)); if (hc) { run += 10 + c; tot = run; } i += 2; }
            else if (hb) { sprintf(m, "%c %c", MK(a), MK(b)); run += a + b; tot = run; i += 2; }
            else { sprintf(m, "%c", MK(a)); i += 1; }
        } else {
            int n = 0; char t[3] = { 0 };
            t[n++] = a == 10 ? 'X' : MK(a);
            if (hb) t[n++] = b == 10 ? 'X' : (a != 10 && a + b == 10 ? '/' : MK(b));
            if (hc) t[n++] = c == 10 ? 'X' : (b != 10 && a == 10 && b + c == 10 ? '/' : MK(c));
            for (int q = 0; q < n; q++) { m[q * 2] = t[q]; m[q * 2 + 1] = q < n - 1 ? ' ' : 0; }
            int bonus = a == 10 || (hb && a + b == 10), done = bonus ? hc : hb;
            if (done) { run += a + b + c; tot = run; }
        }
        #undef MK
        if (k == f) { strcpy(marks, m); *total = tot; return; }
    }
}
static void drawHUD(void) {
    char s[24];
    // the ten frames along the top of the top screen
    int bw = 25, x0 = 3, y0 = 2, bh = 30;
    for (int f = 0; f < 10; f++) {
        int x = x0 + f * bw, cur = f == frameNo && phase != PH_OVER;
        for (int j = 0; j < bh; j++) for (int i = 0; i < bw - 1; i++) gdark(x + i, y0 + j);
        u16 e = cur ? GOLD : COL(12, 12, 12);
        grect(x, y0, bw - 1, 1, e); grect(x, y0 + bh - 1, bw - 1, 1, e); grect(x, y0, 1, bh, e); grect(x + bw - 2, y0, 1, bh, e);
        char m[12]; int tot; fmtFrame(f, m, &tot);
        textS(bufTop, x + 2, y0 + 1, (sprintf(s, "%d", f + 1), s), COL(14, 14, 14));
        textS(bufTop, x + (bw - textSW(m)) / 2, y0 + 8, m, WHITE);
        if (tot >= 0) { sprintf(s, "%d", tot); textS(bufTop, x + (bw - textSW(s)) / 2, y0 + 19, s, GOLD); }
    }
    sprintf(s, "SCORE %d", finalScore()); text(bufTop, SW - 4 - textW(s, 1), 36, s, WHITE, 1);
    if (megaWon) textS(bufTop, SW - 4 - textSW("+33 MEGA"), 52, "+33 MEGA", COL(31, 9, 7));
    sprintf(s, "%d", sv.arcadeBest[ARC_BOWLING]);
    text(bufTop, (int)(AWf * HS_X), (int)(AHf * HS_Y) - 7, s, COL(31, 9, 7), 1);   // on the cabinet's "High Score:"
}
static void drawQueue(void) {
    float y = AHf * RET_Y, step = (RET_X1 - RET_X0) / 9, h = AHf * 0.030f;
    for (int i = 0; i < 10; i++) {
        float x = AWf * (RET_X0 + step * i); int cur = i == frameNo;
        if (cur && ((bFrames / 12) & 1)) for (int j = -6; j <= 6; j++) for (int k = -6; k <= 6; k++) if (j * j + k * k <= 36) gpx((int)x + k, (int)y + j, GOLD);
        marker(QUEUE[i], x, y, cur ? h * 1.30f : h * 1.06f, 1.5707963f, i < frameNo);
    }
}
static void drawOriBtn(void) {
    int cx = ORI_X, cy = ORI_Y;
    for (int j = -ORI_R; j <= ORI_R; j++) for (int i = -ORI_R; i <= ORI_R; i++) if (i * i + j * j <= ORI_R * ORI_R) { gdark(cx + i, cy + j + SH); gdark(cx + i, cy + j + SH); }
    for (int a = 0; a < 40; a++) { float t = -0.19f + a * (5.08f / 40); gpx(cx + (int)(8 * fcos(t)), cy - 1 + (int)(8 * fsin(t)) + SH, COL(23, 23, 23)); }
    float r = ORI[orientIdx].rot, c = fcos(r), s = fsin(r);
    for (int k = -6; k <= 6; k++) for (int w = -1; w <= 1; w++) gpx(cx + (int)(-s * k + c * w), cy + (int)(c * k + s * w) + SH, COL(9, 19, 31));
}
static void drawLaneAndPlay(void) {
    drawIndexed(bw_lane, palT);
    gClipLo = 0; gClipHi = 2 * SH;
    // the MEGA card in one of the speaker boxes: lit in the tenth frame
    { int cx = (int)(AWf * (megaRight ? BOX_R : BOX_L)), cy = (int)(AHf * BOX_Y);
      int armed = frameNo == 9 && !megaWon;
      blit(bufTop, bw_mega, BW_MEGA_W, BW_MEGA_H, cx - BW_MEGA_W / 2, cy - BW_MEGA_H / 2);
      if (!(armed || megaWon)) for (int j = 0; j < BW_MEGA_H; j++) for (int i = 0; i < BW_MEGA_W; i++) if ((i + j) & 1) gdark(cx - BW_MEGA_W / 2 + i, cy - BW_MEGA_H / 2 + j); }
    drawPins();
    if (haveBall && phase != PH_MEGA) {
        float v = ball.Y / LANE_LEN;
        if (v <= 1.16f) { float x, y, s; proj(ball.X, v < 0 ? 0 : v > 1.15f ? 1.15f : v, &x, &y, &s);
            float h = AHf * 0.085f * s * 1.75f;
            shadow(x, y, h, ball.rot);
            marker(ball.col, x, y - h * 0.06f, h * 0.86f, ball.rot + fsin(ball.wob) * 0.10f, 0); }
    }
    if (haveChair) {
        float v = chr.Y / LANE_LEN;
        if (v >= 0 && v <= 1.16f) { float x, y, s; proj(chr.X, v < 1.15f ? v : 1.15f, &x, &y, &s);
            float h = AHf * 0.085f * s * 2.3f;
            blitRotScale(chairImg(), CHAIR_W, CHAIR_H, (int)x, (int)(y - h * 0.34f), chr.rot, h / CHAIR_H); }
    }
    if (flying) {
        float t = megaFly.t / 1.05f; if (t > 1) t = 1; float e = t * t * (3 - 2 * t);
        float tx = AWf * (megaRight ? BOX_R : BOX_L), ty = AHf * BOX_Y;
        float x = megaFly.x0 + (tx - megaFly.x0) * e, y = megaFly.y0 + (ty - megaFly.y0) * e - fsin(t * 3.14159f) * AHf * 0.10f;
        marker(2, x, y, AHf * 0.045f * (1 - t * 0.45f), t * 10, 0);
    }
    if (phase == PH_AIM) {
        float x, y, s; proj(aimX, 0.012f, &x, &y, &s);
        float h = AHf * 0.085f * s * 1.75f;
        shadow(x, y, h, ORI[orientIdx].rot);
        marker(QUEUE[frameNo], x, y - h * 0.06f, h * 0.86f, ORI[orientIdx].rot, 0);
        if (charging) {                                     // the power marker, pointing up the lane
            powerMarker((int)x, (int)y - 30, 0, -1, 30 + (power - 0.35f) * 90, power - 0.35f);
        }
    }
    drawQueue();
    if (phase == PH_AIM) {
        drawOriBtn();
        textC(bufBot, (int)(AHf * 0.725f) - SH - 7, rollNo == 0 ? "SLIDE TO AIM - FLICK TO ROLL" : "SPARE ATTEMPT", WHITE, 1);
        text(bufBot, ORI_X - ORI_R - 4 - textW(ORI[orientIdx].label, 1), ORI_Y - 7, ORI[orientIdx].label, WHITE, 1);
    }
    drawHUD();
    if (msgT > 0) textC(bufTop, 150, msg, msgCol, 2);
}
void drawBowl(void) {
    drawLaneAndPlay();
    if (screen == S_BOWL_PAUSE) {
        textC(bufTop, 110, "PAUSED", YELLOW, 2);
        textC(bufBot, 40, "START  resume", WHITE, 1);
        textC(bufBot, 60, "SELECT  quit to the arcade", WHITE, 1);
    }
}

// ── menu and results ──────────────────────────────────────────────────────
static Btn BB[3]; static int menuSel, overSel;
void drawBowlMenu(void) {
    char s[40];
    fillScreen(bufTop, DARK);
    textC(bufTop, 4, "BOWLING", GOLD, 2);
    textC(bufTop, 38, "Ten frames. Markers are pins.", WHITE, 1);
    textC(bufTop, 56, "Slide to spot, flick up to roll,", WHITE, 1);
    textC(bufTop, 71, "flick sideways to hook it.", WHITE, 1);
    textC(bufTop, 89, "L/R: VERT hooks most,", WHITE, 1);
    textC(bufTop, 104, "FLAT is fast, carries most.", WHITE, 1);
    textC(bufTop, 122, "10th frame: hit the chair", COL(31, 26, 9), 1);
    textC(bufTop, 137, "for MEGA QU33PH (+33)", COL(31, 26, 9), 1);
    sprintf(s, "BEST %d", sv.arcadeBest[ARC_BOWLING]); text(bufTop, 6, 176, s, GREY, 1);
    coinCount(bufTop, SW - 50, 176);
    fillScreen(bufBot, DARK);
    BB[0] = (Btn){ 38, 40, 180, 40, "BOWL", 0, 0 }; BB[1] = (Btn){ 68, 100, 120, 30, "BACK", 0, 0 };
    drawBtns(bufBot, BB, 2, menuSel);
    textC(bufBot, 150, "or D-pad to spot, hold A, let go", GREY, 1);
}
void inputBowlMenu(void) {
    BB[0] = (Btn){ 38, 40, 180, 40, "BOWL", 0, 0 }; BB[1] = (Btn){ 68, 100, 120, 30, "BACK", 0, 0 };
    if (kDown & KEY_B) { goScreen(S_ARCADE); return; }
    int h = btnInput(BB, 2, &menuSel, 1);
    if (h == 0) { newGame(); screen = S_BOWL; }
    if (h == 1) goScreen(S_ARCADE);
}
void drawBowlOver(void) {
    char s[40];
    drawLaneAndPlay();
    box(bufTop, 28, 44, 200, 110, COL(2, 2, 4), GOLD);
    sprintf(s, "FINAL %d", finalScore()); textC(bufTop, 54, s, GOLD, 2);
    sprintf(s, "HIGH %d", sv.arcadeBest[ARC_BOWLING]); textC(bufTop, 90, s, newBest ? LIME : WHITE, 1);
    if (newBest) textC(bufTop, 106, "NEW BEST!", LIME, 1);
    sprintf(s, "%d COINS EARNED", coinsWon); textC(bufTop, 126, s, GOLD, 1);
    fillScreen(bufBot, DARK);
    BB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; BB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    drawBtns(bufBot, BB, 2, overSel);
}
void inputBowlOver(void) {
    BB[0] = (Btn){ 38, 46, 180, 36, "PLAY AGAIN", 0, 0 }; BB[1] = (Btn){ 38, 98, 180, 36, "ARCADE", 0, 0 };
    int h = btnInput(BB, 2, &overSel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) { newGame(); screen = S_BOWL; }
    if (h == 1) goScreen(S_ARCADE);
}
