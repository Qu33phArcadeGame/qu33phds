// game.c — the Qu33ph match: the website's rules & physics, shop effects, themes
#include "qu.h"
#include "assets_mini.h"
#include "assets_ball.h"
#include "assets_fidget.h"
#include "assets_bowl.h"
#include "assets_stack.h"
#include "assets_flip.h"
#include "assets_dozer.h"
#include "assets_pin.h"

// ── the website's world (420 x 900) and the realistic field's layout ──────
#define WORLD_H      900.0f
#define VX0          36.0f
#define K            (256.0f / 220.0f)
#define LEFT_EDGE    77.7f
#define RIGHT_WALL   211.9f
#define FIELD_CX     144.8f
#define LAUNCH_Y     877.5f
#define END_LINE_Y   99.0f
#define START_LINE_Y 820.0f
#define REDDOT_X     217.9f
#define REDDOT_Y     159.8f
#define TOUCH_DIST   45.0f
#define VIEW_H       (384.0f / K)
#define SWIPE_GAIN   2.2f

typedef struct { float x, y, vx, vy, curve, rot, spin, life; int stopped, fallen, armed, deducted, megaTriggered, col; float tx[10], ty[10]; int tn; } Marker;
static Marker mk[3];
static int current;
int orient;
int score2[2], player, p2Round, roundFrames, totalFrames, suddenDeath, matchOver;
static float chairX = FIELD_CX, chairY = 117.0f, chairR = 35.0f;
// The BOTTOM screen is fixed on the launch end (you always see what you're throwing).
// Only the TOP screen scrolls: at rest it shows the stretch just above the bottom screen,
// it follows a marker up the table, and holds on where it landed until the set is scored.
#define BOT_PX   (FIELD_H - SH)                    // strip row shown at the top of the bottom screen
#define TOP_REST (FIELD_H - 2 * SH)                // top screen at rest: directly above the bottom one
static float topPx = TOP_REST;                     // strip row shown at the top of the top screen
static int passOff;                                // the current drawing pass's offset (see matchDraw)
static int lastThrown = -1;
static int magnetAcc;
static const char *ORIENT_NAME[3] = { "VERTICAL", "ANGLED", "FLAT" };

// ── popups, banner, confetti ─────────────────────────────────────────────
typedef struct { float x, y; int life; char text[24]; u16 col; } Popup;
static Popup pops[6];
static void popup(float x, float y, const char *t, u16 c) {
    for (int i = 0; i < 6; i++) if (pops[i].life <= 0) { pops[i].x = x; pops[i].y = y; pops[i].life = 60; pops[i].col = c; strncpy(pops[i].text, t, 23); pops[i].text[23] = 0; return; }
}
static char banner[2][24]; static int bannerT; static u16 bannerCol;
static void showBanner(const char *a, const char *b, u16 c) { strncpy(banner[0], a, 23); banner[0][23] = 0; strncpy(banner[1], b ? b : "", 23); banner[1][23] = 0; bannerT = 75; bannerCol = c; }
typedef struct { float x, y, vx, vy; int life; u16 c; } Bit;
static Bit conf[48];
static void confetti(float x, float y) {
    if (!shopActive(SH_CONFETTI)) return;
    const u16 cs[5] = { RED, LIME, YELLOW, COL(8, 14, 31), COL(31, 10, 28) };
    for (int i = 0; i < 48; i++) { conf[i].x = x; conf[i].y = y; conf[i].vx = (frand() - 0.5f) * 8; conf[i].vy = -frand() * 7 - 2; conf[i].life = 50 + rand() % 30; conf[i].c = cs[i % 5]; }
}

// ── sound ─────────────────────────────────────────────────────────────────
// Everything is IMA-ADPCM now (4 bits a sample, played natively by the DS sound hardware).
static int musicCh = -1, musicMuted;
static void play(const u8 *d, int len, int vol) { playAdpcm(d, len, 16000, vol); }
void sfxThrow(int o) {
    int r = rand();
    if (o == 0) { const u8 *s[4] = { snd_vertical1, snd_vertical2, snd_vertical3, snd_vertical4 };
                  int l[4] = { SND_VERTICAL1_LEN, SND_VERTICAL2_LEN, SND_VERTICAL3_LEN, SND_VERTICAL4_LEN }; play(s[r % 4], l[r % 4], 110); }
    else if (o == 1) { if (r & 1) play(snd_angled1, SND_ANGLED1_LEN, 110); else play(snd_angled2, SND_ANGLED2_LEN, 110); }
    else { if (r & 1) play(snd_horizontal1, SND_HORIZONTAL1_LEN, 110); else play(snd_horizontal2, SND_HORIZONTAL2_LEN, 110); }
}
void sfxPeef(int o) {
    if (o == 0) play(snd_verticalpeef1, SND_VERTICALPEEF1_LEN, 120);
    else if (o == 1) play(snd_angledpeef1, SND_ANGLEDPEEF1_LEN, 120);
    else play(snd_horizontalpeef1, SND_HORIZONTALPEEF1_LEN, 120);
}
void sfxPlop(void) { play(snd_plop, SND_PLOP_LEN, 100); }
// One track plays at a time: the main game's (built in) or the open arcade game's (in its pack).
// Each is restarted by musicTick when it reaches its end.
static int musicTrack = MUS_MAIN, musicT;
static int trackData(int t, const u8 **d, int *len, int *rate, int *frames) {
    if (t == MUS_MINI && pakIs(MINI_PAK)) { *d = ms_music; *len = MS_MUSIC_LEN; *rate = MS_MUSIC_RATE; *frames = MS_MUSIC_FRAMES; return 1; }
    if (t == MUS_BALL && pakIs(BALL_PAK)) { *d = bs_music; *len = BS_MUSIC_LEN; *rate = BS_MUSIC_RATE; *frames = BS_MUSIC_FRAMES; return 1; }
    if (t == MUS_FIDGET && pakIs(FIDGET_PAK)) { *d = fs_music; *len = FS_MUSIC_LEN; *rate = FS_MUSIC_RATE; *frames = FS_MUSIC_FRAMES; return 1; }
    if (t == MUS_BOWL && pakIs(BOWL_PAK)) { *d = bw_music; *len = BW_MUSIC_LEN; *rate = BW_MUSIC_RATE; *frames = BW_MUSIC_FRAMES; return 1; }
    if (t == MUS_STACK && pakIs(STACK_PAK)) { *d = st_music; *len = ST_MUSIC_LEN; *rate = ST_MUSIC_RATE; *frames = ST_MUSIC_FRAMES; return 1; }
    if (t == MUS_FLIP && pakIs(FLIP_PAK)) { *d = fl_music; *len = FL_MUSIC_LEN; *rate = FL_MUSIC_RATE; *frames = FL_MUSIC_FRAMES; return 1; }
    if (t == MUS_DOZER && pakIs(DOZER_PAK)) { *d = dz_music; *len = DZ_MUSIC_LEN; *rate = DZ_MUSIC_RATE; *frames = DZ_MUSIC_FRAMES; return 1; }
    if (t == MUS_PIN && pakIs(PIN_PAK)) { *d = pb_music; *len = PB_MUSIC_LEN; *rate = PB_MUSIC_RATE; *frames = PB_MUSIC_FRAMES; return 1; }
    if (t == MUS_MAIN) { *d = snd_music; *len = SND_MUSIC_LEN; *rate = SND_MUSIC_RATE; *frames = SND_MUSIC_FRAMES; return 1; }
    return 0;
}
static int musicFrames;
void musicStart(void) {
    if (musicCh >= 0 || !sv.musicOn) return;
    const u8 *d; int len, rate;
    if (!trackData(musicTrack, &d, &len, &rate, &musicFrames)) return;
    musicCh = soundPlaySample(d, SoundFormat_ADPCM, len, rate, 70, 64, false, 0); musicT = 0; musicMuted = 0;
}
// X in a game: mute/unmute without restarting the song (it keeps playing silently, so it
// carries on from where it is rather than starting over)
void musicToggle(void) {
    sv.musicOn = !sv.musicOn;
    if (musicCh >= 0) { soundSetVolume(musicCh, sv.musicOn ? 70 : 0); musicMuted = !sv.musicOn; }
    else if (sv.musicOn) musicStart();
}
void musicStop(void) { if (musicCh >= 0) { soundKill(musicCh); musicCh = -1; } }
void musicSet(int t) { if (t != musicTrack) { musicStop(); musicTrack = t; } musicStart(); }
void musicTick(void) {
    if (musicCh >= 0 && ++musicT >= musicFrames) {      // loop; a muted song loops on silently
        int m = musicMuted; musicStop(); sv.musicOn = 1; musicStart(); sv.musicOn = !m;
        if (m && musicCh >= 0) { soundSetVolume(musicCh, 0); musicMuted = 1; }
    }
}
void playAdpcm(const u8 *d, int len, int rate, int vol) {
    if (!sv.sfxOn) return;
    if (vol > 127) vol = 127;
    if (vol < 0) vol = 0;
    soundPlaySample(d, SoundFormat_ADPCM, len, rate, vol, 64, false, 0);
}

// ── maths ─────────────────────────────────────────────────────────────────
static float dist(float ax, float ay, float bx, float by) { return fsqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)); }
static int touching(Marker *a, Marker *b) { return dist(a->x, a->y, b->x, b->y) < TOUCH_DIST; }
static int wsx(float wx) { return (int)((wx - VX0) * K); }
static int wsy(float wy) { return (int)(wy * K) - passOff; }

// ── a turn ────────────────────────────────────────────────────────────────
static void resetSet(void) { current = 0; memset(mk, 0, sizeof mk); }
void startMatch(void) {
    resetSet(); roundFrames = 0; suddenDeath = 0; matchOver = 0; magnetAcc = 0;
    int rt = (mode == M_TWO) ? sv.timer2p : sv.timer1p;
    if (shopActive(SH_XTIME)) rt += 5;
    totalFrames = (mode == M_TWO) ? rt * 60 : (rt * 3 / 2) * 60;     // 1P & Olympics: sudden death for the last third
    chairX = FIELD_CX; chairY = 117.0f;
    topPx = TOP_REST; lastThrown = -1; bannerT = 0;
    for (int i = 0; i < 6; i++) pops[i].life = 0;
    for (int i = 0; i < 48; i++) conf[i].life = 0;
}
static void addScore(int d) {
    score2[player] += d;
    if (d > 0 && shopActive(SH_MAGNET)) {             // +1 coin per 15 points
        magnetAcc += d;
        while (magnetAcc >= 30) { magnetAcc -= 30; addCoins(1); }
    }
}
static void throwMarker(float dx, float dy) {
    if (screen != S_PLAY || current >= 3) return;
    Marker *m = &mk[current];
    memset(m, 0, sizeof *m);
    m->x = FIELD_CX; m->y = LAUNCH_Y;
    m->vx = dx * 0.2f; m->vy = dy * 0.2f; m->curve = dx * 0.0003f;
    m->spin = orient == 0 ? 0.08f : orient == 1 ? 0.12f : 0.18f;
    m->col = current; lastThrown = current;
    // Land in the orientation you picked, whatever the colour or theme: the marker still spins
    // the website's amount in flight (spin x 22.85 over its one second), but it starts turned so
    // it comes to rest VERTICAL / ANGLED / FLAT. Each picture's marker lies at its own angle
    // (red & green diagonal, blue level, the CARTOON ones upright), so that's allowed for too.
    {
        float target = orient == 0 ? 1.5708f : orient == 1 ? 0.7854f : 0.0f;
        float axis = sv.theme == 1 ? 1.5708f : m->col == 2 ? -0.14f : 0.7854f;
        m->rot = target - axis - m->spin * 22.85f;
    }
    current++;
    sfxThrow(orient);
}
static void triggerMega(Marker *m) {
    m->x = REDDOT_X; m->y = REDDOT_Y; m->vx = m->vy = 0; m->stopped = 1;
    addScore(20);
    showBanner("MEGA QU33PH", "+10", YELLOW);
    confetti(REDDOT_X, REDDOT_Y);
    sfxPlop(); trackMega();
}
static int calcRoundScore(void) {
    Marker *g = &mk[0], *r = &mk[1], *b = &mk[2];
    if (!(g->armed && r->armed && b->armed)) return 0;
    int gr = touching(g, r), rb = touching(r, b), gb = touching(g, b);
    if (suddenDeath) {
        if (gr && rb && gb) return 20;
        if (rb || gb) return 10;
        if (gr) return 6;
        return 0;
    }
    if (gr && rb && gb) return 6;       // QU33PH = 3
    if (rb || gb) return 4;             // blue touching = 2
    if (gr) return 1;                   // green-red = 0.5
    return 0;
}
static void tryAdvanceRound(void) {
    if (current < 3) return;
    for (int i = 0; i < 3; i++) if (!mk[i].stopped && !mk[i].fallen) return;
    Marker *g = &mk[0], *r = &mk[1], *b = &mk[2];
    if (!b->fallen && suddenDeath && b->y > chairY) {
        addScore(-4); popup(100, chairY + 60, "PEEF -2 BLUE MISSED CHAIR", RED); sfxPeef(orient); trackPeef();
    }
    int rs = calcRoundScore();
    addScore(rs);
    char t[16], s[10]; scoreStr(s, rs); sprintf(t, "+%s", s);
    lastThrown = -1;                                   // set scored: the top screen glides back to rest
    if (!g->fallen && !r->fallen && !b->fallen && touching(g, r) && touching(r, b) && touching(g, b)) {
        showBanner("QU33PH!", t, YELLOW); sfxPlop(); trackQu33ph(suddenDeath);
        confetti((g->x + r->x + b->x) / 3, (g->y + r->y + b->y) / 3);
    } else showBanner(t, 0, WHITE);
    resetSet();
}

void matchUpdate(void) {
    roundFrames++;
    int rt = ((mode == M_TWO) ? sv.timer2p : sv.timer1p) + (shopActive(SH_XTIME) ? 5 : 0);
    if (mode != M_TWO && roundFrames > rt * 60 && !suddenDeath) {
        suddenDeath = 1; chairX = FIELD_CX; chairY = END_LINE_Y + 90;
        showBanner("SUDDEN DEATH", "hit the chair!", RED);
    }
    if (roundFrames > totalFrames) { matchOver = 1; return; }
    float megaChance = shopActive(SH_MEGABOOST) ? 0.45f : 0.25f;
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
        if ((roundFrames & 1) == 0) { for (int k = 9; k > 0; k--) { m->tx[k] = m->tx[k - 1]; m->ty[k] = m->ty[k - 1]; } m->tx[0] = m->x; m->ty[0] = m->y; if (m->tn < 10) m->tn++; }
        if (suddenDeath && !m->megaTriggered && dist(m->x, m->y, chairX, chairY) < chairR) {
            if (frand() < megaChance) triggerMega(m);
            else {                                   // the website's gentle chair bounce
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
                if (shopActive(SH_PEEFGUARD)) ded = (ded + 1) / 2;
                addScore(-2 * ded); m->deducted = 1;
                char t[12]; sprintf(t, "PEEF -%d", ded);
                popup(LEFT_EDGE + 10, m->y < END_LINE_Y + 40 ? END_LINE_Y + 40 : m->y, t, RED);
                sfxPeef(orient); trackPeef();
            }
            m->fallen = 1;
        }
        if (m->x > RIGHT_WALL) { m->x = RIGHT_WALL; m->vx *= -0.4f; }
        if (m->y < END_LINE_Y) { m->y = END_LINE_Y; m->vy *= -0.2f; }
        if (!m->stopped && fabsf_(m->vx) < 0.05f && fabsf_(m->vy) < 0.05f) { m->stopped = 1; sfxPlop(); }
    }
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) { pops[i].life--; pops[i].y -= 0.8f; }
    for (int i = 0; i < 48; i++) if (conf[i].life > 0) { conf[i].life--; conf[i].vy += 0.25f; conf[i].x += conf[i].vx; conf[i].y += conf[i].vy; }
    if (bannerT > 0) bannerT--;
    tryAdvanceRound();
    // top-screen camera: follow the marker that's moving furthest up the table; once it
    // stops, stay on the last marker thrown so you can see where it landed
    float target = TOP_REST; int lead = -1;
    for (int i = 0; i < current; i++) if (!mk[i].stopped && !mk[i].fallen && (lead < 0 || mk[i].y < mk[lead].y)) lead = i;
    if (lead < 0 && lastThrown >= 0 && lastThrown < current && !mk[lastThrown].fallen) lead = lastThrown;
    if (lead >= 0) target = mk[lead].y * K - SH * 0.55f;
    if (target < 0) target = 0;
    if (target > TOP_REST) target = TOP_REST;
    topPx += (target - topPx) * (target < topPx ? 0.16f : 0.10f);
    if (fabsf_(target - topPx) < 0.5f) topPx = target;
}

// ── aiming with buttons ───────────────────────────────────────────────────
// Every throw is measured the same way, so the first marker of a set flies exactly like the
// others: the swipe's strength is the bigger of its total length and its speed over the last
// few frames (a quick flick counts fully), and a glitchy first touch sample is ignored.
// A full button charge just reaches the back wall; a swipe needs real length or speed to get there.
static float aimAng = -1.5708f, power; static int charging, chargeT, ph;
static int swiping, sx0, sy0, sx1, sy1, hx[4], hy[4], hn;
void matchInput(int down, int held, int up, int tx, int ty) {
    if (down & KEY_L) orient = (orient + 2) % 3;
    if (down & KEY_R) orient = (orient + 1) % 3;
    if (held & KEY_LEFT)  { aimAng -= 0.03f; chargeT = 90; }
    if (held & KEY_RIGHT) { aimAng += 0.03f; chargeT = 90; }
    if (aimAng < -2.6f) aimAng = -2.6f;
    if (aimAng > -0.55f) aimAng = -0.55f;
    if (down & KEY_A) { charging = 1; power = 0; ph = 0; }
    if (charging) {
        ph++; float p = (ph % 64) / 32.0f; power = p < 1 ? p : 2 - p;    // full in about half a second
        chargeT = 90;
        if (down & KEY_B) charging = 0;
        else if (up & KEY_A) { charging = 0; float pw = 70 + power * 180; throwMarker(fcos(aimAng) * pw, fsin(aimAng) * pw); }
    }
    if (chargeT > 0 && !charging) chargeT--;
    if (down & KEY_TOUCH) { swiping = 1; sx0 = sx1 = tx; sy0 = sy1 = ty; hn = 0; }
    if (swiping && (held & KEY_TOUCH)) {
        if (hn == 0 && (tx - sx0) * (tx - sx0) + (ty - sy0) * (ty - sy0) > 3600) { sx0 = tx; sy0 = ty; }   // bad first sample
        sx1 = tx; sy1 = ty;
        for (int k = 3; k > 0; k--) { hx[k] = hx[k - 1]; hy[k] = hy[k - 1]; } hx[0] = tx; hy[0] = ty; if (hn < 4) hn++;
    }
    if (swiping && (up & KEY_TOUCH)) {
        swiping = 0;
        float ddx = sx1 - sx0, ddy = sy1 - sy0;
        if (hn >= 3) {                                // speed over the last frames, scaled to a ~6-frame swipe
            int k = hn - 1; float vx = (hx[0] - hx[k]) * 6.0f / k, vy = (hy[0] - hy[k]) * 6.0f / k;
            if (vy < ddy) { ddy = vy; ddx = ddx * 0.5f + vx * 0.5f; }   // (dy is negative going up)
        }
        float dx = ddx / K * SWIPE_GAIN, dy = ddy / K * SWIPE_GAIN;
        if (dy < -8) throwMarker(dx, dy);
    }
}

// ── drawing ───────────────────────────────────────────────────────────────
static void markerSprite(int col, const u16 **s, int *w, int *h) {
    int cart = (sv.theme == 1);
    if (col == 2) { *s = cart ? mkc_blue : mk_blue; *w = MK_BLUE_W; *h = MK_BLUE_H; }
    else { *s = col == 0 ? (cart ? mkc_green : mk_green) : (cart ? mkc_red : mk_red); *w = MK_GREEN_W; *h = MK_GREEN_H; }
}
static const u16 MCOL[3] = { COL(6, 28, 8), COL(31, 6, 6), COL(8, 12, 31) };
static void drawWorld(void) {
    // markers still waiting this set sit on the table above the launch spot, as on the website
    for (int i = current; i < 3; i++) {
        const u16 *sp; int w, h; markerSprite(i, &sp, &w, &h);
        float a = orient == 0 ? 1.0472f : orient == 1 ? 1.5708f : 0.0f;
        drawMarkerFx(sp, w, h, wsx((LEFT_EDGE + RIGHT_WALL) / 2), wsy(LAUNCH_Y - 30 - i * 46), a, 1.0f, i == 0 ? 1 : i == 1 ? 0 : 2);
    }
    if (suddenDeath) blitRot(chair, CHAIR_W, CHAIR_H, wsx(chairX), wsy(chairY), 0);
    for (int i = 0; i < current; i++) {
        Marker *m = &mk[i];
        if (m->fallen && m->x < LEFT_EDGE - 40) continue;
        if (shopActive(SH_TRAILS)) for (int k = 1; k < m->tn; k++) grect(wsx(m->tx[k]) - 1, wsy(m->ty[k]) - 1, 3, 3, MCOL[m->col]);
        // GLOW (shop) now glows round the marker's own shape; NEON glows green; CARTOON gets a bold
        // outline in the marker's colour
        const u16 *s; int w, h; markerSprite(m->col, &s, &w, &h);
        gGlowShop = shopActive(SH_GLOW);
        // touching another marker: a pulsing gold glow, so you can see the touch that scores
        int touch = 0;
        if (!m->fallen) for (int j = 0; j < current; j++) if (j != i && !mk[j].fallen && touching(m, &mk[j])) touch = 1;
        if (touch) { int pz = (frameCount >> 2) & 7; pz = pz < 4 ? pz : 7 - pz; gTouchGlow = COL(31, 22 + pz * 2, 4 + pz * 3); }
        drawMarkerFx(s, w, h, wsx(m->x), wsy(m->y), m->rot, 1.0f, m->col == 0 ? 1 : m->col == 1 ? 0 : 2);
        gGlowShop = 0; gTouchGlow = 0;
    }
    if (current < 3 && (charging || chargeT > 0)) {   // the power marker grows out of the launch spot
        float len = charging ? (40 + power * 150) * K : 40 * K;
        powerMarker(wsx(FIELD_CX), wsy(LAUNCH_Y), fcos(aimAng), fsin(aimAng), len, charging ? (power < 0.04f ? 0.04f : power) : 0);
    }
    for (int i = 0; i < 48; i++) if (conf[i].life > 0) grect(wsx(conf[i].x), wsy(conf[i].y), 3, 3, conf[i].c);
    for (int i = 0; i < 6; i++) if (pops[i].life > 0) gtext(wsx(pops[i].x), wsy(pops[i].y), pops[i].text, pops[i].col, 1);
}
void matchDraw(void) {
    int tp = (int)topPx;
    memcpy(bufTop, fieldPix + tp * SW, sizeof bufTop);
    memcpy(bufBot, fieldPix + BOT_PX * SW, sizeof bufBot);
    // draw everything in the field twice: once with the top screen's scroll (clipped to it),
    // once with the fixed bottom screen's (clipped to it), so markers cross the gap correctly
    for (int pass = 0; pass < 2; pass++) {
        passOff = pass == 0 ? tp : BOT_PX - SH;        // tall-canvas rows: top 0-191, bottom 192-383
        gClipLo = pass == 0 ? 0 : SH; gClipHi = pass == 0 ? SH : 2 * SH;
        drawWorld();
    }
    gClipLo = 0; gClipHi = 2 * SH;
    // HUD
    char s[40], a[12], b[12];
    int left = (totalFrames - roundFrames + 59) / 60; if (left < 0) left = 0;
    int bw = SW * roundFrames / (totalFrames ? totalFrames : 1); if (bw > SW) bw = SW;
    rect(bufTop, 0, 0, bw, 3, suddenDeath ? RED : LIME);
    sprintf(s, "TIME %d", left); text(bufTop, 6, 8, s, suddenDeath ? RED : WHITE, 1);
    if (mode != M_TWO) {
        scoreStr(a, score2[0]); sprintf(s, "SCORE %s", a); text(bufTop, 6, 24, s, YELLOW, 1);
        if (mode == M_OLYMPICS) { drawFlag(bufTop, SW - 40, 8, 32, 21, olyNation); }
    } else {
        scoreStr(a, score2[0]); scoreStr(b, score2[1]);
        sprintf(s, "P1 %s   P2 %s", a, b); text(bufTop, 6, 24, s, YELLOW, 1);
        sprintf(s, "ROUND %d/%d  PLAYER %d", p2Round, sv.twoRounds, player + 1); text(bufTop, 6, 40, s, WHITE, 1);
    }
    if (suddenDeath) textC(bufTop, 8, "SUDDEN DEATH", RED, 1);
    if (bannerT > 0) { textC(bufTop, 70, banner[0], bannerCol, 2); if (banner[1][0]) textC(bufTop, 104, banner[1], WHITE, 2); }
    text(bufBot, SW - textW(ORIENT_NAME[orient], 1) - 6, SH - 18, ORIENT_NAME[orient], WHITE, 1);
    text(bufBot, SW - textW("L/R", 1) - 6, SH - 32, "L/R", GREY, 1);
}
