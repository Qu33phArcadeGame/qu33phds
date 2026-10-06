// ════════════════════════════════════════════════════════════════════════
//  QU33PH DS — main menu and every screen (the arcade's games are in arcade.c)
//
//  CONTROLS: touch or D-pad + A everywhere, B = back.
//  In a match: swipe to throw, or D-pad aim + hold/release A; L/R marker
//  orientation; START pause (SELECT then quits); X music; Y sound effects.
//  Progress saves to the flash cart's microSD card (qu33ph_ds.sav).
// ════════════════════════════════════════════════════════════════════════
#include "qu.h"

int screen = S_TITLE, mode = M_SINGLE, frameCount;
int kDown, kHeld, kUp, tX, tY;
static u16 *vramTop, *vramBot;
static int sel, page, confirmT, confirmWhich = -1, highTab, lastCoinsGain, resultsNewBest;
static Btn B[40];

void goScreen(int s) { screen = s; sel = 0; page = 0; confirmWhich = -1; if (s == S_PLINKO) plinkoEnter(); saveWrite(); }

// ── starting and ending matches ───────────────────────────────────────────
static void beginTurn(void) { orient = sv.orient; startMatch(); screen = S_PLAY; }
static void startSingle(void) { mode = M_SINGLE; score2[0] = score2[1] = 0; player = 0; beginTurn(); }
static void startTwo(void) { mode = M_TWO; score2[0] = score2[1] = 0; player = 0; p2Round = 1; startMatch(); screen = S_HANDOFF; }
void startOlympicMatch(void) { mode = M_OLYMPICS; score2[0] = 0; player = 0; beginTurn(); }

// name entry → where to go after it, and which table gets the score
static int nameReturn = S_TITLE, nameScore; static HighEntry *nameList; static char nameBuf[9];
void nameEntry(int ret, HighEntry *list, int s2) { nameReturn = ret; nameList = list; nameScore = s2; strcpy(nameBuf, sv.name); screen = S_NAME; sel = 0; }
// put a score on a Top 10 (asks for a name the first time)
static void submitHigh(HighEntry *list, int s2, int ret) {
    if (!highQualifies(list, s2)) { screen = ret; return; }
    if (!sv.name[0]) { nameEntry(ret, list, s2); return; }
    highInsert(list, sv.name, s2);
    toast("NEW HIGH SCORE!", list == sv.high1p ? "1 PLAYER TOP 10" : "OLYMPICS TOP 10");
    screen = ret;
}
void olympicsDone(void) { trackGameEnd(0); saveWrite(); goScreen(S_TITLE); submitHigh(sv.highOly, olyTournamentTotal(), S_TITLE); }


static void matchFinished(void) {
    if (mode == M_SINGLE) {
        int before = sv.coins, best = sv.high1p[0].name[0] ? sv.high1p[0].score2 : 0;
        trackGameEnd(score2[0]);
        resultsNewBest = score2[0] > best && score2[0] > 0;
        if (resultsNewBest) addCoins(25);                       // new personal best: +25 coins (as on the website)
        lastCoinsGain = sv.coins - before;
        saveWrite();
        goScreen(S_RESULTS);
        submitHigh(sv.high1p, score2[0], S_RESULTS);
    } else if (mode == M_OLYMPICS) {
        trackGameEnd(score2[0]);
        olyAfterMatch(score2[0]);
        goScreen(S_OLY_BRACKET);
    } else {
        if (player == 0) { player = 1; startMatch(); screen = S_HANDOFF; return; }
        if (p2Round >= sv.twoRounds) { trackGameEnd(score2[0] > score2[1] ? score2[0] : score2[1]); saveWrite(); goScreen(S_RESULTS); return; }
        p2Round++; player = 0; startMatch(); screen = S_HANDOFF;
    }
}

// ── title ─────────────────────────────────────────────────────────────────
#define TITLE_N 12
static const char *TITLE_ITEMS[TITLE_N] = { "1 PLAYER", "2 PLAYER", "OLYMPICS", "HIGH SCORES", "SHOP", "ACHIEVEMENTS",
                                        "CAREER RECORD", "SETTINGS", "THEMES", "SLOT", "PLINQU33PH", "ARCADE" };
static void layoutGrid(int n, int cols, int y0, int h, int gap) {
    int w = (SW - 8 - (cols - 1) * 4) / cols;
    for (int i = 0; i < n; i++) { B[i].x = 4 + (i % cols) * (w + 4); B[i].y = y0 + (i / cols) * (h + gap); B[i].w = w; B[i].h = h; B[i].col = 0; B[i].dim = 0; }
}
// The website's title: the main buttons down the middle, with the icon buttons at the sides:
// SLOT, PLINQU33PH and ARCADE on the left, SETTINGS and THEMES on the right.
// (Button numbers stay as before: 0-6 the middle, 7 settings, 8 themes, 9 slot, 10 plinko, 11 arcade.)
static void logoBlit(int x, int y) {        // NEON: the logo glows like a lit sign
    if (sv.theme == 4) blitGlow(bufTop, logoT, LOGO_W, LOGO_H, x, y, COL(8, 31, 15)); else blit(bufTop, logoT, LOGO_W, LOGO_H, x, y);
}
static void titleLayout(void) {
    for (int i = 0; i < 7; i++) { B[i] = (Btn){ 58, 4 + i * 27, 140, 24, "", 0, 0 }; strcpy(B[i].label, TITLE_ITEMS[i]); }
    B[7]  = (Btn){ 200, 4, 56, 60, "SETTINGS", 0, 0 };   B[8]  = (Btn){ 200, 66, 56, 60, "THEMES", 0, 0 };
    B[9]  = (Btn){ 0, 4, 56, 60, "SLOT", 0, 0 };         B[10] = (Btn){ 0, 66, 56, 60, "PLINQU33PH", 0, 0 };
    B[11] = (Btn){ 0, 128, 56, 60, "ARCADE", 0, 0 };
    B[12] = (Btn){ 200, 128, 56, 60, "COINS", 0, 0 };     // the coin count, under THEMES (opens the coin record)
}
static void drawIconBtn(int i, int on) {
    Btn *b = &B[i];
    int cart = sv.theme == 1;
    u16 c = on ? (cart ? COL(28, 4, 4) : YELLOW) : uiIcon;
    int ix = b->x + b->w / 2, iy = b->y + 4, s = 38, pressed = on && (kHeld & KEY_A) ? 1 : 0;
    iy += pressed;
    if (on) { rect(bufBot, b->x + 3, b->y + 1, b->w - 6, 2, c); rect(bufBot, b->x + 3, b->y + b->h - 3, b->w - 6, 2, c);
              rect(bufBot, b->x + 1, b->y + 3, 2, b->h - 6, c); rect(bufBot, b->x + b->w - 3, b->y + 3, 2, b->h - 6, c); }
    // every icon wears the theme (CARTOON grey like the logo) and the same ink outline
    if (i == 9) blitInk(bufBot, icSlotT, IC_SLOT_W, IC_SLOT_H, ix - IC_SLOT_W / 2, iy, uiInk);
    else if (i == 11) blitInk(bufBot, icArcadeT, IC_ARCADE_W, IC_ARCADE_H, ix - IC_ARCADE_W / 2, iy, uiInk);
    else if (i == 12) {                                  // the coin and your balance
        char v[12]; sprintf(v, "%d", sv.coins);
        blitInk(bufBot, icCoinT, IC_COIN_W, IC_COIN_H, ix - IC_COIN_W / 2, iy + 2, uiInk);
        text(bufBot, ix - textW(v, 1) / 2, b->y + b->h - 19 + pressed, v, on ? YELLOW : GOLD, 1);
        return;
    } else {                                             // the drawn icons: an ink pass, then the colour
        void (*f)(u16 *, int, int, int, u16) = i == 10 ? iconPlinko : i == 7 ? iconGear : iconPalette;
        iconFat = 1; f(bufBot, ix - s / 2, iy + 1, s, uiInk); iconFat = 0;
        f(bufBot, ix - s / 2, iy + 1, s, c);
    }
    { int lx = ix - textSW(b->label) / 2, ly = b->y + b->h - 13 + pressed;   // caption, with a 1px ink shadow so it reads on any theme
      textS(bufBot, lx + 1, ly + 1, b->label, uiInk);
      textS(bufBot, lx, ly, b->label, on ? c : (i == 11 ? (cart ? COL(24, 10, 0) : GOLD) : uiIcon)); }
}
static void drawTitle(void) {
    fillScreen(bufTop, DARK);
    logoBlit( (SW - LOGO_W) / 2, 2);
    char s[32], a[10];
    if (sv.high1p[0].name[0]) { scoreStr(a, sv.high1p[0].score2); sprintf(s, "HIGH SCORE  %s", a); textC(bufTop, 128, s, WHITE, 1); }
    textC(bufTop, 150, "swipe or D-pad + A to throw", GREY, 1);
    textC(bufTop, 166, saveOK ? "your progress saves automatically" : "no SD card: progress not saved", saveOK ? GREY : RED, 1);
    fillScreen(bufBot, DARK);
    titleLayout();
    drawBtns(bufBot, B, 7, sel < 7 ? sel : -1);
    for (int i = 7; i <= 12; i++) drawIconBtn(i, i == sel);
}
// D-pad: up/down within a column; left/right hops between the icons and the middle buttons
static int titleNav(int cur, int dx, int dy) {
    int cx = B[cur].x + B[cur].w / 2, cy = B[cur].y + B[cur].h / 2, best = cur, bd = 1 << 30;
    for (int i = 0; i < 13; i++) { if (i == cur) continue;
        int x = B[i].x + B[i].w / 2 - cx, y = B[i].y + B[i].h / 2 - cy;
        int along = dx ? x * dx : y * dy, across = dx ? (y < 0 ? -y : y) : (x < 0 ? -x : x);
        if (along <= 0 || (dy && across > 30)) continue;
        int d = along + across * 3; if (d < bd) { bd = d; best = i; } }
    if (best == cur && dy) {                              // nothing further that way: wrap round the column
        int far = 0;
        for (int i = 0; i < 13; i++) { int x = B[i].x - B[cur].x, y = (B[cur].y - B[i].y) * dy;
            if (i != cur && x > -30 && x < 30 && y > far) { far = y; best = i; } }
    }
    return best;
}
static void inputTitle(void) {
    titleLayout();
    if (sel < 0 || sel >= 13) sel = 0;
    if (kDown & KEY_UP) sel = titleNav(sel, 0, -1);
    if (kDown & KEY_DOWN) sel = titleNav(sel, 0, 1);
    if (kDown & KEY_LEFT) sel = titleNav(sel, -1, 0);
    if (kDown & KEY_RIGHT) sel = titleNav(sel, 1, 0);
    int keep = kDown; kDown &= ~(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT);
    int h = btnInput(B, 13, &sel, 1);
    kDown = keep;
    switch (h) {
        case 0: startSingle(); break;
        case 1: startTwo(); break;
        case 2: goScreen(S_OLY_SELECT); break;
        case 3: goScreen(S_HIGHS); break;
        case 4: goScreen(S_SHOP); break;
        case 5: goScreen(S_ACH); break;
        case 6: goScreen(S_CAREER); break;
        case 7: goScreen(S_SETTINGS); break;
        case 8: goScreen(S_THEMES); break;
        case 9: goScreen(S_SLOT); break;
        case 10: goScreen(S_PLINKO); break;
        case 11: goScreen(S_ARCADE); break;
        case 12: goScreen(S_COINREC); break;
    }
}

// ── shop ──────────────────────────────────────────────────────────────────
static const char *SHOP_SHORT[SH_COUNT] = { "Doubler", "Magnet", "Extra Time", "Mega Boost", "Peef Guard", "Glow", "Trails", "Confetti", "Gold Slot" };
static void shopLayout(void) {
    layoutGrid(SH_COUNT + 1, 2, 4, 30, 5);
    for (int i = 0; i < SH_COUNT; i++) {
        int own = sv.shopOwned & (1u << i), on = sv.shopOn & (1u << i);
        if (own) sprintf(B[i].label, "%s  %s", SHOP_SHORT[i], on ? "ON" : "OFF");
        else sprintf(B[i].label, "%s  %d", SHOP_SHORT[i], SHOP_PRICE[i]);
        B[i].col = own ? (on ? LIME : GREY) : 0;
        B[i].dim = !own && sv.coins < SHOP_PRICE[i];
    }
    strcpy(B[SH_COUNT].label, "BACK");
}
static void drawShop(void) {
    shopLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 8, "SHOP", GOLD, 2);
    coinCount(bufTop, 8, 44);
    if (sel < SH_COUNT) {
        textC(bufTop, 80, SHOP_NAME[sel], WHITE, 2);
        textC(bufTop, 116, SHOP_DESC[sel], GREY, 1);
        int own = sv.shopOwned & (1u << sel);
        textC(bufTop, 150, own ? "owned: tap to switch on/off" : "tap to buy", own ? LIME : YELLOW, 1);
    }
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, B, SH_COUNT + 1, sel);
}
static void inputShop(void) {
    shopLayout();
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    int h = btnInput(B, SH_COUNT + 1, &sel, 2);
    if (h == SH_COUNT) { goScreen(S_TITLE); return; }
    if (h >= 0) {
        if (sv.shopOwned & (1u << h)) sv.shopOn ^= 1u << h;
        else if (sv.coins >= SHOP_PRICE[h]) { spendCoins(SHOP_PRICE[h]); sv.shopOwned |= 1u << h; sv.shopOn |= 1u << h; toast("PURCHASED", SHOP_NAME[h]); }
        saveWrite();
    }
}

// ── achievements (2 pages of 12; L/R or the arrows change page) ───────────
static void achLayout(void) {
    layoutGrid(14, 2, 3, 23, 3);
    for (int i = 0; i < 12; i++) { int a = page * 12 + i; strcpy(B[i].label, ACH_NAME[a]); B[i].col = (sv.ach & (1u << a)) ? GOLD : GREY; }
    strcpy(B[12].label, page ? "< PAGE 1" : "PAGE 2 >"); strcpy(B[13].label, "BACK");
}
static void drawAch(void) {
    achLayout();
    int n = 0; for (int i = 0; i < ACH_COUNT; i++) if (sv.ach & (1u << i)) n++;
    fillScreen(bufTop, DARK);
    textC(bufTop, 8, "ACHIEVEMENTS", GOLD, 2);
    char s[24]; sprintf(s, "%d / %d unlocked", n, ACH_COUNT); textC(bufTop, 44, s, WHITE, 1);
    if (sel < 12) {
        int a = page * 12 + sel, got = sv.ach & (1u << a);
        textC(bufTop, 84, ACH_NAME[a], got ? GOLD : WHITE, 2);
        textC(bufTop, 120, ACH_DESC[a], GREY, 1);
        textC(bufTop, 146, got ? "UNLOCKED  +1 coin" : "locked", got ? LIME : GREY, 1);
    }
    textC(bufTop, 172, "L / R  change page", GREY, 1);
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, B, 14, sel);
}
static void inputAch(void) {
    achLayout();
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    if (kDown & (KEY_L | KEY_R)) page ^= 1;
    int h = btnInput(B, 14, &sel, 2);
    if (h == 12) page ^= 1;
    if (h == 13) goScreen(S_TITLE);
}

// ── career record ─────────────────────────────────────────────────────────
static void drawCareer(void) {
    char s[40], a[10];
    fillScreen(bufTop, DARK);
    textC(bufTop, 4, "CAREER RECORD", GOLD, 1);
    int n = 0; for (int i = 0; i < ACH_COUNT; i++) if (sv.ach & (1u << i)) n++;
    scoreStr(a, sv.high1p[0].name[0] ? sv.high1p[0].score2 : 0);
    const char *L[10] = { "Games played", "High score", "QU33PHs landed", "MEGA QU33PHs", "PEEFs", "Coins earned (lifetime)", "Slot spins", "Slot wins", "Achievements", "Coins now" };
    int V[10] = { sv.games, 0, sv.qu33phs, sv.megas, sv.peefs, sv.coinsEarned, sv.slotSpins, sv.slotWins, n, sv.coins };
    for (int i = 0; i < 10; i++) {
        text(bufTop, 10, 22 + i * 17, L[i], WHITE, 1);
        if (i == 1) strcpy(s, a); else if (i == 8) sprintf(s, "%d / %d", n, ACH_COUNT); else sprintf(s, "%d", V[i]);
        text(bufTop, SW - 10 - textW(s, 1), 22 + i * 17, s, GOLD, 1);
    }
    fillScreen(bufBot, DARK);
    textC(bufBot, 8, "OLYMPIC MEDALS", GOLD, 1);
    const char *M[3] = { "Gold", "Silver", "Bronze" }; int MV[3] = { sv.gold, sv.silver, sv.bronze };
    const u16 MC[3] = { GOLD, COL(24, 24, 26), COL(26, 15, 7) };
    for (int i = 0; i < 3; i++) {
        for (int dy = -9; dy <= 9; dy++) for (int dx = -9; dx <= 9; dx++) if (dx * dx + dy * dy <= 81) rect(bufBot, 50 + i * 78 + dx, 50 + dy, 1, 1, MC[i]);
        sprintf(s, "%s %d", M[i], MV[i]); text(bufBot, 50 + i * 78 - textW(s, 1) / 2, 66, s, WHITE, 1);
    }
    B[0] = (Btn){ 68, 140, 120, 32, "BACK", 0, 0 };
    drawBtns(bufBot, B, 1, 0);
}
static void inputCareer(void) {
    B[0] = (Btn){ 68, 140, 120, 32, "BACK", 0, 0 };
    int s0 = 0;
    if ((kDown & KEY_B) || btnInput(B, 1, &s0, 1) == 0) goScreen(S_TITLE);
}

// ── high scores ───────────────────────────────────────────────────────────
static void drawHighs(void) {
    HighEntry *l = highTab ? sv.highOly : sv.high1p;
    fillScreen(bufTop, DARK);
    textC(bufTop, 2, highTab ? "OLYMPICS TOP 10 (tournament total)" : "1 PLAYER TOP 10", GOLD, 1);
    for (int i = 0; i < 10; i++) {
        char s[24], a[10];
        sprintf(s, "%d", i + 1); text(bufTop, 20, 20 + i * 17, s, i < 3 ? GOLD : GREY, 1);
        text(bufTop, 50, 20 + i * 17, l[i].name[0] ? l[i].name : "---", WHITE, 1);
        if (l[i].name[0]) { scoreStr(a, l[i].score2); text(bufTop, SW - 20 - textW(a, 1), 20 + i * 17, a, YELLOW, 1); }
    }
    fillScreen(bufBot, DARK);
    B[0] = (Btn){ 8, 30, 116, 40, "1 PLAYER", highTab ? 0 : YELLOW, 0 };
    B[1] = (Btn){ 132, 30, 116, 40, "OLYMPICS", highTab ? YELLOW : 0, 0 };
    B[2] = (Btn){ 68, 120, 120, 34, "BACK", 0, 0 };
    drawBtns(bufBot, B, 3, sel);
}
static void inputHighs(void) {
    B[0] = (Btn){ 8, 30, 116, 40, "", 0, 0 }; B[1] = (Btn){ 132, 30, 116, 40, "", 0, 0 }; B[2] = (Btn){ 68, 120, 120, 34, "", 0, 0 };
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    int h = btnInput(B, 3, &sel, 2);
    if (h == 0) highTab = 0;
    if (h == 1) highTab = 1;
    if (h == 2) goScreen(S_TITLE);
}

// ── settings ──────────────────────────────────────────────────────────────
static const char *ORIENT_SHORT[3] = { "VERTICAL", "ANGLED", "FLAT" };
static void settingsLayout(void) {
    layoutGrid(10, 2, 4, 30, 6);
    sprintf(B[0].label, "MUSIC  %s", sv.musicOn ? "ON" : "OFF");
    sprintf(B[1].label, "SOUND FX  %s", sv.sfxOn ? "ON" : "OFF");
    sprintf(B[2].label, "1P TIMER  %ds", sv.timer1p);
    sprintf(B[3].label, "2P TIMER  %ds", sv.timer2p);
    sprintf(B[4].label, "2P ROUNDS  %d", sv.twoRounds);
    sprintf(B[5].label, "START  %s", ORIENT_SHORT[sv.orient]);
    sprintf(B[6].label, "NAME  %s", sv.name[0] ? sv.name : "---");
    strcpy(B[7].label, "BACK");
    strcpy(B[8].label, confirmWhich == 8 ? "SURE? TAP AGAIN" : "RESET SCORES"); B[8].col = RED;
    strcpy(B[9].label, confirmWhich == 9 ? "SURE? TAP AGAIN" : "RESET ALL"); B[9].col = RED;
}
static void drawSettings(void) {
    settingsLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 20, "SETTINGS", GOLD, 2);
    textC(bufTop, 70, saveOK ? "Saving to the SD card: ON" : "No SD card found:", saveOK ? LIME : RED, 1);
    if (!saveOK) textC(bufTop, 88, "progress lasts until you switch off", GREY, 1);
    textC(bufTop, 118, "in a match: X music, Y sound", GREY, 1);
    textC(bufTop, 140, "RESET ALL wipes coins,", GREY, 1);
    textC(bufTop, 156, "shop items and records", GREY, 1);
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, B, 10, sel);
}
static void inputSettings(void) {
    settingsLayout();
    if (confirmT > 0 && --confirmT == 0) confirmWhich = -1;
    if (kDown & KEY_B) { saveWrite(); goScreen(S_TITLE); return; }
    int h = btnInput(B, 10, &sel, 2);
    if (h < 0) return;
    switch (h) {
        case 0: musicToggle(); break;
        case 1: sv.sfxOn = !sv.sfxOn; break;
        case 2: sv.timer1p = sv.timer1p == 30 ? 45 : sv.timer1p == 45 ? 60 : 30; break;
        case 3: sv.timer2p = sv.timer2p == 15 ? 30 : sv.timer2p == 30 ? 60 : 15; break;
        case 4: sv.twoRounds = sv.twoRounds == 3 ? 5 : sv.twoRounds == 5 ? 10 : 3; break;
        case 5: sv.orient = (sv.orient + 1) % 3; break;
        case 6: nameEntry(S_SETTINGS, 0, 0); return;
        case 7: saveWrite(); goScreen(S_TITLE); return;
        case 8: case 9:
            if (confirmWhich == h) { if (h == 8) resetHighScores(); else { resetEverything(); musicStop(); musicStart(); } toast("DONE", h == 8 ? "high scores cleared" : "everything reset"); confirmWhich = -1; }
            else { confirmWhich = h; confirmT = 180; }
            break;
    }
    saveWrite();
}

// ── themes ────────────────────────────────────────────────────────────────
static const char *THEME_DESC[THEME_COUNT] = { "the real room photo", "comic-book halftone", "moonlit blue", "golden hour", "glowing green neon", "golden hour" };
// SUNSET and GOLDEN were the same look, so the list shows five: REALISTIC, CARTOON, NIGHT, NEON, GOLDEN
#define TVIS 5
static const int TV[TVIS] = { 0, 1, 2, 4, 5 };
static void themesLayout(void) {
    layoutGrid(TVIS + 1, 2, 8, 34, 8);
    for (int k = 0; k < TVIS; k++) { int i = TV[k];
        int own = sv.themesOwned & (1u << i);
        if (i == sv.theme) sprintf(B[k].label, "%s  IN USE", THEME_NAME[i]);
        else if (own) sprintf(B[k].label, "%s", THEME_NAME[i]);
        else sprintf(B[k].label, "%s  %d", THEME_NAME[i], THEME_COST);
        B[k].col = i == sv.theme ? LIME : 0; B[k].dim = !own && sv.coins < THEME_COST;
    }
    strcpy(B[TVIS].label, "BACK");
}
static void drawThemes(void) {
    themesLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 10, "THEMES", GOLD, 2);
    coinCount(bufTop, 8, 44);
    if (sel < TVIS) { textC(bufTop, 90, THEME_NAME[TV[sel]], WHITE, 2); textC(bufTop, 126, THEME_DESC[TV[sel]], GREY, 1); }
    textC(bufTop, 160, "tap an owned theme to use it", GREY, 1);
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, B, TVIS + 1, sel);
}
static void inputThemes(void) {
    themesLayout();
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    int h = btnInput(B, TVIS + 1, &sel, 2);
    if (h == TVIS) { goScreen(S_TITLE); return; }
    if (h >= 0) { int t = TV[h];
        if (!(sv.themesOwned & (1u << t))) {
            if (sv.coins < THEME_COST) return;
            spendCoins(THEME_COST); sv.themesOwned |= 1u << t; unlockAch(A_UNLOCK_THEME);
        }
        sv.theme = t; applyTheme(); saveWrite();
    }
}

// ── coin record (the website's: tap the coins under THEMES) ────────────────
static void drawCoinRecord(void) {
    char v[16];
    fillScreen(bufTop, DARK);
    textC(bufTop, 6, "COIN RECORD", WHITE, 2);
    gClipLo = 0; gClipHi = 2 * SH;
    blitInk(bufTop, icCoinBigT, IC_COINBIG_W, IC_COINBIG_H, (SW - IC_COINBIG_W) / 2, 110 - IC_COINBIG_H / 2, uiInk);
    sprintf(v, "%d", sv.coins); textC(bufTop, 156, v, GOLD, 2);
    fillScreen(bufBot, DARK);
    const char *L[7] = { "Current balance", "Coins earned (all time)", "Coins spent (all time)", "Net saved", "Slot machine wins", "Coins lost in slots", "Games forfeited" };
    int V[7] = { sv.coins, sv.coinsEarned, sv.coinsSpent, sv.coinsEarned - sv.coinsSpent, sv.slotWins, sv.slotLost, sv.forfeits };
    for (int i = 0; i < 7; i++) {
        int y = 4 + i * 21;
        box(bufBot, 6, y, 244, 19, COL(2, 2, 2), COL(5, 5, 5));
        text(bufBot, 12, y + 3, L[i], COL(23, 23, 23), 1);
        sprintf(v, "%d", V[i]); text(bufBot, 244 - textW(v, 1), y + 3, v, COL(31, 27, 0), 1);
    }
    B[0] = (Btn){ 68, 156, 120, 30, "BACK", 0, 0 };
    drawBtns(bufBot, B, 1, 0);
}
static void inputCoinRecord(void) {
    B[0] = (Btn){ 68, 156, 120, 30, "BACK", 0, 0 };
    int s0 = 0;
    if ((kDown & KEY_B) || btnInput(B, 1, &s0, 1) == 0) goScreen(S_TITLE);
}

// ── name entry: on-screen keyboard (D-pad + A, or tap; B deletes; START done)
static const char KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static void nameLayout(void) {
    for (int i = 0; i < 38; i++) { B[i].x = 3 + (i % 10) * 25; B[i].y = 60 + (i / 10) * 32; B[i].w = 24; B[i].h = 29; B[i].col = 0; B[i].dim = 0; }
    for (int i = 0; i < 36; i++) { B[i].label[0] = KEYS[i]; B[i].label[1] = 0; }
    strcpy(B[36].label, "DEL"); strcpy(B[37].label, "OK");
    B[36].w = 36; B[37].x = B[36].x + 40; B[37].w = 36; B[37].col = LIME;
}
static void drawName(void) {
    nameLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 20, nameList ? "NEW HIGH SCORE!" : "PLAYER NAME", GOLD, 2);
    box(bufTop, 48, 80, 160, 40, BLACK, WHITE);
    char s[12]; sprintf(s, "%s%s", nameBuf, (frameCount / 20) % 2 ? "_" : " "); textC(bufTop, 88, s, WHITE, 2);
    textC(bufTop, 150, "B delete  -  START done", GREY, 1);
    fillScreen(bufBot, DARK);
    textC(bufBot, 20, "type your name (up to 8)", WHITE, 1);
    drawBtns(bufBot, B, 38, sel);
}
static void inputName(void) {
    nameLayout();
    int len = strlen(nameBuf), done = 0;
    int h = btnInput(B, 38, &sel, 10);
    if (h >= 0 && h < 36 && len < 8) { nameBuf[len] = KEYS[h]; nameBuf[len + 1] = 0; }
    if ((h == 36 || (kDown & KEY_B)) && len > 0) nameBuf[len - 1] = 0;
    if (h == 37 || (kDown & KEY_START)) done = 1;
    if (done && nameBuf[0]) {
        strcpy(sv.name, nameBuf);
        if (nameList) { highInsert(nameList, sv.name, nameScore); toast("NEW HIGH SCORE!", sv.name); }
        saveWrite();
        int r = nameReturn; nameList = 0; screen = r; sel = 0;
    }
}

// ── results / hand-over / pause ───────────────────────────────────────────
static void drawResults(void) {
    char s[40], a[10], b[10];
    fillScreen(bufTop, DARK);
    logoBlit( (SW - LOGO_W) / 2, 0);
    if (mode == M_SINGLE) {
        scoreStr(a, score2[0]); sprintf(s, "FINAL SCORE  %s", a); textC(bufTop, 124, s, YELLOW, 2);
        if (resultsNewBest) textC(bufTop, 158, "NEW PERSONAL BEST!", LIME, 1);
        if (lastCoinsGain > 0) { sprintf(s, "+%d coins", lastCoinsGain); textC(bufTop, 174, s, GOLD, 1); }
    } else {
        scoreStr(a, score2[0]); scoreStr(b, score2[1]);
        sprintf(s, "P1  %s     P2  %s", a, b); textC(bufTop, 124, s, WHITE, 1);
        textC(bufTop, 146, score2[0] > score2[1] ? "PLAYER 1 WINS!" : score2[1] > score2[0] ? "PLAYER 2 WINS!" : "IT'S A TIE!", YELLOW, 2);
    }
    fillScreen(bufBot, DARK);
    B[0] = (Btn){ 38, 50, 180, 36, "PLAY AGAIN", 0, 0 }; B[1] = (Btn){ 38, 100, 180, 36, "MENU", 0, 0 };
    drawBtns(bufBot, B, 2, sel);
}
static void inputResults(void) {
    B[0] = (Btn){ 38, 50, 180, 36, "", 0, 0 }; B[1] = (Btn){ 38, 100, 180, 36, "", 0, 0 };
    int h = btnInput(B, 2, &sel, 1);
    if (kDown & KEY_B) h = 1;
    if (h == 0) { if (mode == M_TWO) startTwo(); else startSingle(); }
    if (h == 1) goScreen(S_TITLE);
}
static void drawHandoff(void) {
    char s[32];
    fillScreen(bufTop, DARK);
    logoBlit( (SW - LOGO_W) / 2, 4);
    sprintf(s, "PLAYER %d", player + 1); textC(bufTop, 132, s, YELLOW, 2);
    sprintf(s, "ROUND %d / %d", p2Round, sv.twoRounds); textC(bufTop, 168, s, WHITE, 1);
    fillScreen(bufBot, DARK);
    textC(bufBot, 70, "Pass the DS", WHITE, 1);
    textC(bufBot, 100, "TAP or A to start", YELLOW, 1);
}

// ── main loop ─────────────────────────────────────────────────────────────
// frames really shown: counted by the screen's own refresh (60 a second). If a busy frame takes
// longer than 1/60 s, the games catch up by running extra steps, so they keep full speed.
static volatile int vbCount; static int vbLast;
static void vbIrq(void) { vbCount++; }
int main(void) {
    videoSetMode(MODE_5_2D); videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
    int bgm = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int bgs = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vramBot = bgGetGfxPtr(bgm); vramTop = bgGetGfxPtr(bgs);
    lcdMainOnBottom();
    soundEnable();
    irqSet(IRQ_VBLANK, vbIrq); irqEnable(IRQ_VBLANK);
    if (isDSiMode()) setCpuClock(true);       // on a DSi / 3DS running in DSi mode: double the CPU speed (134 MHz)
    saveInit(); applyTheme();
    srand(0x51A);
    musicStart();

    while (1) {
        frameCount++;
        int vbNow = vbCount, steps = vbNow - vbLast; vbLast = vbNow;
        if (steps < 1) steps = 1;
        if (steps > 3) steps = 3;                                   // (never more than 3 catch-up steps)
        scanKeys();
        kDown = keysDown(); kHeld = keysHeld(); kUp = keysUp();
        touchPosition t; touchRead(&t); tX = t.px; tY = t.py;
        if (kDown) srand(rand() ^ frameCount);
        switch (screen) {
            case S_TITLE: inputTitle(); break;
            case S_PLAY:
                if (kDown & KEY_START) { screen = S_PAUSE; break; }
                if (kDown & KEY_X) musicToggle();
                if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
                matchInput(kDown, kHeld, kUp, tX, tY); for (int k = 0; k < steps && !matchOver; k++) matchUpdate();
                if (matchOver) matchFinished();
                break;
            case S_PAUSE:
                if (kDown & KEY_START) screen = S_PLAY;
                if (kDown & KEY_SELECT) { sv.forfeits++; saveWrite(); goScreen(S_TITLE); }
                break;
            case S_HANDOFF: if (kDown & (KEY_A | KEY_TOUCH | KEY_START)) screen = S_PLAY; break;
            case S_RESULTS: inputResults(); break;
            case S_SHOP: inputShop(); break;
            case S_ACH: inputAch(); break;
            case S_CAREER: inputCareer(); break;
            case S_HIGHS: inputHighs(); break;
            case S_SETTINGS: inputSettings(); break;
            case S_THEMES: inputThemes(); break;
            case S_SLOT: inputSlot(kDown, tX, tY); updateSlot(); break;
            case S_PLINKO: inputPlinko(kDown, kHeld, tX, tY); updatePlinko(); break;
            case S_OLY_SELECT: inputOlySelect(kDown, tX, tY); break;
            case S_OLY_BRACKET: inputOlyBracket(kDown, tX, tY); break;
            case S_NAME: inputName(); break;
            case S_ARCADE: inputArcade(); break;
            case S_MINI_MENU: inputMiniMenu(); break;
            case S_MINI: case S_MINI_PAUSE: inputMini(); for (int k = 0; k < steps && screen == S_MINI; k++) updateMini(); break;
            case S_MINI_OVER: inputMiniOver(); break;
            case S_BALL_MENU: inputBallMenu(); break;
            case S_BALL: case S_BALL_PAUSE: inputBall(); for (int k = 0; k < steps && screen == S_BALL; k++) updateBall(); break;
            case S_BALL_OVER: inputBallOver(); break;
            case S_FIDGET_MENU: inputFidgetMenu(); break;
            case S_FIDGET: case S_FIDGET_PAUSE: inputFidget(); for (int k = 0; k < steps && screen == S_FIDGET; k++) updateFidget(); break;
            case S_FIDGET_OVER: inputFidgetOver(); break;
            case S_COINREC: inputCoinRecord(); break;
            case S_BOWL_MENU: inputBowlMenu(); break;
            case S_BOWL: case S_BOWL_PAUSE: inputBowl(); for (int k = 0; k < steps && screen == S_BOWL; k++) updateBowl(); break;
            case S_BOWL_OVER: inputBowlOver(); break;
            case S_STACK_MENU: inputStackMenu(); break;
            case S_STACK: case S_STACK_PAUSE: inputStack(); for (int k = 0; k < steps && screen == S_STACK; k++) updateStack(); break;
            case S_STACK_OVER: inputStackOver(); updateStack(); break;
            case S_FLIP_MENU: inputFlipMenu(); break;
            case S_FLIP_LEVELS: inputFlipLevels(); break;
            case S_FLIP: case S_FLIP_PAUSE: inputFlip(); for (int k = 0; k < steps && screen == S_FLIP; k++) updateFlip(); break;
            case S_FLIP_OVER: inputFlipOver(); break;
            case S_DOZER_MENU: inputDozerMenu(); break;
            case S_DOZER: case S_DOZER_PAUSE: inputDozer(); for (int k = 0; k < steps && screen == S_DOZER; k++) updateDozer(); break;
            case S_DOZER_OVER: inputDozerOver(); updateDozer(); break;
            case S_PIN_MENU: inputPinMenu(); break;
            case S_PIN: case S_PIN_PAUSE: inputPin(); for (int k = 0; k < steps && screen == S_PIN; k++) updatePin(); break;
            case S_PIN_OVER: inputPinOver(); break;
            case S_JUMP_MENU: inputJumpMenu(); break;
            case S_JUMP: case S_JUMP_PAUSE: inputJump(); for (int k = 0; k < steps && screen == S_JUMP; k++) updateJump(); break;
            case S_JUMP_OVER: inputJumpOver(); break;
            case S_JUMP_CHARS: inputJumpChars(); break;
        }
        // Mini Qu33ph has its own music, from its menu to its results (as on the website)
        // the arcade games have their own music, from their menu to their results (as on the website)
        musicSet(screen >= S_MINI_MENU && screen <= S_MINI_OVER ? MUS_MINI : screen >= S_BALL_MENU && screen <= S_BALL_OVER ? MUS_BALL :
                 screen >= S_FIDGET_MENU && screen <= S_FIDGET_OVER ? MUS_FIDGET :
                 screen >= S_BOWL_MENU && screen <= S_BOWL_OVER ? MUS_BOWL :
                 screen >= S_STACK_MENU && screen <= S_STACK_OVER ? MUS_STACK :
                 screen >= S_FLIP_MENU && screen <= S_FLIP_OVER ? MUS_FLIP :
                 screen >= S_DOZER_MENU && screen <= S_DOZER_OVER ? MUS_DOZER :
                 screen >= S_PIN_MENU && screen <= S_PIN_OVER ? MUS_PIN : MUS_MAIN);
        musicTick();
        switch (screen) {
            case S_TITLE: drawTitle(); break;
            case S_PLAY: matchDraw(); break;
            case S_PAUSE: matchDraw(); textC(bufTop, 80, "PAUSED", YELLOW, 2); textC(bufBot, 70, "START  resume", WHITE, 1); textC(bufBot, 94, "SELECT  quit to menu", WHITE, 1); break;
            case S_HANDOFF: drawHandoff(); break;
            case S_RESULTS: drawResults(); break;
            case S_SHOP: drawShop(); break;
            case S_ACH: drawAch(); break;
            case S_CAREER: drawCareer(); break;
            case S_HIGHS: drawHighs(); break;
            case S_SETTINGS: drawSettings(); break;
            case S_THEMES: drawThemes(); break;
            case S_SLOT: drawSlot(); break;
            case S_PLINKO: drawPlinko(); break;
            case S_OLY_SELECT: drawOlySelect(); break;
            case S_OLY_BRACKET: drawOlyBracket(); break;
            case S_NAME: drawName(); break;
            case S_ARCADE: drawArcade(); break;
            case S_MINI_MENU: drawMiniMenu(); break;
            case S_MINI: case S_MINI_PAUSE: drawMini(); break;
            case S_MINI_OVER: drawMiniOver(); break;
            case S_BALL_MENU: drawBallMenu(); break;
            case S_BALL: case S_BALL_PAUSE: drawBall(); break;
            case S_BALL_OVER: drawBallOver(); break;
            case S_FIDGET_MENU: drawFidgetMenu(); break;
            case S_FIDGET: case S_FIDGET_PAUSE: drawFidget(); break;
            case S_FIDGET_OVER: drawFidgetOver(); break;
            case S_COINREC: drawCoinRecord(); break;
            case S_BOWL_MENU: drawBowlMenu(); break;
            case S_BOWL: case S_BOWL_PAUSE: drawBowl(); break;
            case S_BOWL_OVER: drawBowlOver(); break;
            case S_STACK_MENU: drawStackMenu(); break;
            case S_STACK: case S_STACK_PAUSE: drawStack(); break;
            case S_STACK_OVER: drawStackOver(); break;
            case S_FLIP_MENU: drawFlipMenu(); break;
            case S_FLIP_LEVELS: drawFlipLevels(); break;
            case S_FLIP: case S_FLIP_PAUSE: drawFlip(); break;
            case S_FLIP_OVER: drawFlipOver(); break;
            case S_DOZER_MENU: drawDozerMenu(); break;
            case S_DOZER: case S_DOZER_PAUSE: drawDozer(); break;
            case S_DOZER_OVER: drawDozerOver(); break;
            case S_PIN_MENU: drawPinMenu(); break;
            case S_PIN: case S_PIN_PAUSE: drawPin(); break;
            case S_PIN_OVER: drawPinOver(); break;
            case S_JUMP_MENU: drawJumpMenu(); break;
            case S_JUMP: case S_JUMP_PAUSE: drawJump(); break;
            case S_JUMP_OVER: drawJumpOver(); break;
            case S_JUMP_CHARS: drawJumpChars(); break;
        }
        drawToast();
        // Hand both finished frames to the screens. The copy (192 KB) used to keep the CPU waiting
        // for over half a frame on an original DS; now the DMA copies this pair in the background
        // while the CPU gets on with the next frame in the other pair. First the CPU's cache is
        // written out (DMA reads memory directly): flushing the whole 8 KB cache is far quicker
        // than walking 192 KB of frame line by line.
        DC_FlushAll();
        swiWaitForVBlank();
        while (dmaBusy(3) || dmaBusy(2)) ;                       // (last frame's copy: long finished by now)
        dmaCopyWordsAsynch(3, bufTop, vramTop, SCREEN_BYTES);
        dmaCopyWordsAsynch(2, bufBot, vramBot, SCREEN_BYTES);
        bufSwap();
    }
    return 0;
}
