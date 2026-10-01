// ════════════════════════════════════════════════════════════════════════
//  QU33PH DS — main menu and every screen (the arcade comes later)
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
static const char *TITLE_ITEMS[11] = { "1 PLAYER", "2 PLAYER", "OLYMPICS", "HIGH SCORES", "SHOP", "ACHIEVEMENTS",
                                        "CAREER RECORD", "SETTINGS", "THEMES", "SLOT", "PLINQU33PH" };
static void layoutGrid(int n, int cols, int y0, int h, int gap) {
    int w = (SW - 8 - (cols - 1) * 4) / cols;
    for (int i = 0; i < n; i++) { B[i].x = 4 + (i % cols) * (w + 4); B[i].y = y0 + (i / cols) * (h + gap); B[i].w = w; B[i].h = h; B[i].col = 0; B[i].dim = 0; }
}
static void drawTitle(void) {
    fillScreen(bufTop, DARK);
    blit(bufTop, logo, LOGO_W, LOGO_H, (SW - LOGO_W) / 2, 2);
    coinCount(bufTop, 8, 8);
    char s[32], a[10];
    if (sv.high1p[0].name[0]) { scoreStr(a, sv.high1p[0].score2); sprintf(s, "HIGH SCORE  %s", a); textC(bufTop, 128, s, WHITE, 1); }
    textC(bufTop, 150, "swipe or D-pad + A to throw", GREY, 1);
    textC(bufTop, 166, saveOK ? "progress saves to your SD card" : "no SD card: progress won't be kept", saveOK ? GREY : RED, 1);
    fillScreen(bufBot, DARK);
    layoutGrid(11, 2, 4, 27, 4);
    for (int i = 0; i < 11; i++) strcpy(B[i].label, TITLE_ITEMS[i]);
    drawBtns(bufBot, B, 11, sel);
}
static void inputTitle(void) {
    layoutGrid(11, 2, 4, 27, 4);
    int h = btnInput(B, 11, &sel, 2);
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
        else if (sv.coins >= SHOP_PRICE[h]) { sv.coins -= SHOP_PRICE[h]; sv.shopOwned |= 1u << h; sv.shopOn |= 1u << h; toast("PURCHASED", SHOP_NAME[h]); }
        saveWrite();
    }
}

// ── achievements (2 pages of 12; L/R or the arrows change page) ───────────
static void achLayout(void) {
    layoutGrid(14, 2, 4, 25, 4);
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
        case 0: sv.musicOn = !sv.musicOn; if (sv.musicOn) musicStart(); else musicStop(); break;
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
static const char *THEME_DESC[THEME_COUNT] = { "the real room photo", "cartoon-style markers", "moonlit blue", "warm evening glow", "electric colours", "gilded sepia" };
static void themesLayout(void) {
    layoutGrid(THEME_COUNT + 1, 2, 8, 34, 8);
    for (int i = 0; i < THEME_COUNT; i++) {
        int own = sv.themesOwned & (1u << i);
        if (i == sv.theme) sprintf(B[i].label, "%s  IN USE", THEME_NAME[i]);
        else if (own) sprintf(B[i].label, "%s", THEME_NAME[i]);
        else sprintf(B[i].label, "%s  %d", THEME_NAME[i], THEME_COST);
        B[i].col = i == sv.theme ? LIME : 0; B[i].dim = !own && sv.coins < THEME_COST;
    }
    strcpy(B[THEME_COUNT].label, "BACK");
}
static void drawThemes(void) {
    themesLayout();
    fillScreen(bufTop, DARK);
    textC(bufTop, 10, "THEMES", GOLD, 2);
    coinCount(bufTop, 8, 44);
    if (sel < THEME_COUNT) { textC(bufTop, 90, THEME_NAME[sel], WHITE, 2); textC(bufTop, 126, THEME_DESC[sel], GREY, 1); }
    textC(bufTop, 160, "tap an owned theme to use it", GREY, 1);
    fillScreen(bufBot, DARK);
    drawBtns(bufBot, B, THEME_COUNT + 1, sel);
}
static void inputThemes(void) {
    themesLayout();
    if (kDown & KEY_B) { goScreen(S_TITLE); return; }
    int h = btnInput(B, THEME_COUNT + 1, &sel, 2);
    if (h == THEME_COUNT) { goScreen(S_TITLE); return; }
    if (h >= 0) {
        if (!(sv.themesOwned & (1u << h))) {
            if (sv.coins < THEME_COST) return;
            sv.coins -= THEME_COST; sv.themesOwned |= 1u << h; unlockAch(A_UNLOCK_THEME);
        }
        sv.theme = h; applyTheme(); saveWrite();
    }
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
    blit(bufTop, logo, LOGO_W, LOGO_H, (SW - LOGO_W) / 2, 0);
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
    blit(bufTop, logo, LOGO_W, LOGO_H, (SW - LOGO_W) / 2, 4);
    sprintf(s, "PLAYER %d", player + 1); textC(bufTop, 132, s, YELLOW, 2);
    sprintf(s, "ROUND %d / %d", p2Round, sv.twoRounds); textC(bufTop, 168, s, WHITE, 1);
    fillScreen(bufBot, DARK);
    textC(bufBot, 70, "Pass the DS", WHITE, 1);
    textC(bufBot, 100, "TAP or A to start", YELLOW, 1);
}

// ── main loop ─────────────────────────────────────────────────────────────
int main(void) {
    videoSetMode(MODE_5_2D); videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
    int bgm = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int bgs = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    vramBot = bgGetGfxPtr(bgm); vramTop = bgGetGfxPtr(bgs);
    lcdMainOnBottom();
    soundEnable();
    saveInit(); applyTheme();
    srand(0x51A);
    musicStart();

    while (1) {
        frameCount++;
        scanKeys();
        kDown = keysDown(); kHeld = keysHeld(); kUp = keysUp();
        touchPosition t; touchRead(&t); tX = t.px; tY = t.py;
        if (kDown) srand(rand() ^ frameCount);
        switch (screen) {
            case S_TITLE: inputTitle(); break;
            case S_PLAY:
                if (kDown & KEY_START) { screen = S_PAUSE; break; }
                if (kDown & KEY_X) { sv.musicOn = !sv.musicOn; if (sv.musicOn) musicStart(); else musicStop(); }
                if (kDown & KEY_Y) sv.sfxOn = !sv.sfxOn;
                matchInput(kDown, kHeld, kUp, tX, tY); matchUpdate();
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
        }
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
        }
        drawToast();
        swiWaitForVBlank();
        dmaCopy(bufTop, vramTop, sizeof bufTop);
        dmaCopy(bufBot, vramBot, sizeof bufBot);
    }
    return 0;
}
