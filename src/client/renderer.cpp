#include "renderer.h"
#include "../../include/game_defs.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <math.h>

float GX = 499.f, GY = 117.f, GW = 640.f, GH = 640.f;
float CELLW = GW / GRID_COLS;
float CELLH = GH / GRID_ROWS;

Layout g_layout = {};

// ── Icon system ──────────────────────────────────────────────────────────────
static Texture2D texGold = {0};
static Texture2D texTrophy = {0};
static Texture2D texHelp = {0};
static Texture2D texShop[5] = {0};

void loadIcons() {
    if (texGold.id != 0) return;
    texGold = LoadTexture("assets/general/gold_coin.png");
    texTrophy = LoadTexture("assets/general/trophy.png");
    texHelp = LoadTexture("assets/general/help_icon.png");

    texShop[0] = LoadTexture("assets/shop/potion_blue.png");
    texShop[1] = LoadTexture("assets/shop/potion_orange.png");
    texShop[2] = LoadTexture("assets/shop/potion_pink.png");
    texShop[3] = LoadTexture("assets/shop/coffee_cup.png");
    texShop[4] = LoadTexture("assets/shop/lamp.png");
}

void drawIcon(int iconId, Rectangle rect, Color tint) {
    Texture2D* t = nullptr;
    if (iconId == ICON_COIN) t = &texGold;
    else if (iconId == ICON_HELP) t = &texHelp;
    
    if (t && t->id != 0) {
        DrawTexturePro(*t, {0, 0, (float)t->width, (float)t->height}, rect, {0, 0}, 0, tint);
    }
}

// Helper to draw text with word wrap
void DrawTextWrapped(Font font, const char* text, Vector2 pos, float fontSize, float maxWidth, Color tint) {
    if (text == nullptr || text[0] == '\0') return;

    std::string str = text;
    std::string currentLine = "";
    Vector2 currentPos = pos;
    float spaceWidth = MeasureTextEx(font, " ", fontSize, 1).x;

    size_t start = 0, end = 0;
    while ((end = str.find(' ', start)) != std::string::npos) {
        std::string word = str.substr(start, end - start);
        float wordWidth = MeasureTextEx(font, word.c_str(), fontSize, 1).x;
        float lineWidth = MeasureTextEx(font, currentLine.c_str(), fontSize, 1).x;

        if (lineWidth + spaceWidth + wordWidth > maxWidth) {
            DrawTextEx(font, currentLine.c_str(), currentPos, fontSize, 1, tint);
            currentPos.y += fontSize + 2;
            currentLine = word;
        } else {
            if (!currentLine.empty()) currentLine += " ";
            currentLine += word;
        }
        start = end + 1;
    }

    // Last word
    std::string lastWord = str.substr(start);
    float wordWidth = MeasureTextEx(font, lastWord.c_str(), fontSize, 1).x;
    float lineWidth = MeasureTextEx(font, currentLine.c_str(), fontSize, 1).x;

    if (lineWidth + spaceWidth + wordWidth > maxWidth) {
        DrawTextEx(font, currentLine.c_str(), currentPos, fontSize, 1, tint);
        currentPos.y += fontSize + 2;
        DrawTextEx(font, lastWord.c_str(), currentPos, fontSize, 1, tint);
    } else {
        if (!currentLine.empty()) currentLine += " ";
        currentLine += lastWord;
        DrawTextEx(font, currentLine.c_str(), currentPos, fontSize, 1, tint);
    }
}

Texture2D* itemIdToTexture(uint8_t itemId) {
    return &texShop[itemId % 5];
}

const char* itemIdToName(uint8_t itemId) {
    static const char* names[] = {
        "Pocao de Cura",
        "Pocao de Vitalidade",
        "Pocao de Resiliencia",
        "Cafe Energizante",
        "Lampada do Conhecimento"
    };
    return names[itemId % 5];
}

static void unloadIcons() {
    if (texGold.id != 0) {
        UnloadTexture(texGold); texGold = {0};
        UnloadTexture(texTrophy); texTrophy = {0};
        UnloadTexture(texHelp); texHelp = {0};
        for(int i=0; i<5; i++) {
            UnloadTexture(texShop[i]);
            texShop[i] = {0};
        }
    }
}


// Arena image dimensions
static const float ARENA_W = 1201.f;
static const float ARENA_H = 880.f;
static const float PANEL_W = 220.f;
static const float GRID_OFF_X = 279.f;   // arenaX (220) + 279 = 499
static const float GRID_OFF_Y = 117.f;   // arenaY (0) + 117 = 117

Layout computeLayout() {
    Layout l;
    l.screenW = (float)GetScreenWidth();
    l.screenH = (float)GetScreenHeight();

    // Arena is centered in the top portion, leaving room for side panels
    float arenaX = PANEL_W;
    float arenaY = 0.f;

    // Grid aligns with the arena image
    l.gridX = arenaX + GRID_OFF_X;
    l.gridY = arenaY + GRID_OFF_Y;
    l.gridW = 640.f;
    l.gridH = 640.f;
    l.cellW = l.gridW / GRID_COLS;
    l.cellH = l.gridH / GRID_ROWS;

    // Side panels outside the arena image
    l.leftPanelX = 0;
    l.leftPanelW = PANEL_W;
    l.rightPanelX = arenaX + ARENA_W;
    l.rightPanelW = l.screenW - l.rightPanelX;
    l.sidePanelW = PANEL_W;

    // Cards below the arena image
    l.cardsY = arenaY + ARENA_H + 12.f;
    l.bottomCardsH = l.screenH - l.cardsY;
    if (l.bottomCardsH < 100.f) l.bottomCardsH = 100.f;

    // Each player gets half the full screen width as their card container.
    float halfScreen = l.screenW * 0.5f;
    float minCardW = 100.f;
    float maxCardW = 218.f;
    l.cardW = fminf(maxCardW, fmaxf(minCardW, halfScreen / 3.f));
    l.cardH = fminf(130.f, l.bottomCardsH - 20.f);

    l.topBarH = l.gridY;
    l.trainerAbilityBtnY = l.screenH - 24.f;
    l.controlsHintY = l.screenH - 20.f;

    return l;
}

void applyLayout(const Layout& l) {
    g_layout = l;
    GX = l.gridX;
    GY = l.gridY;
    CELLW = l.cellW;
    CELLH = l.cellH;
}

static const Color kPlayerColor[2] = { BLUE, RED };
static const Color kP1Color = {80, 150, 255, 255};
static const Color kP2Color = {255, 100, 80, 255};
static std::vector<Texture2D> trainerTextures;
static std::vector<Texture2D> trainerCardTextures;
static std::vector<Texture2D> heroTextures;
static bool texturesLoaded = false;

static const int MAX_FX_SHEETS = 8;
static SpriteSheet fxSheets[MAX_FX_SHEETS];
static int fxSheetCount = 0;
static std::vector<FxAnim> fxAnims;

static constexpr int ARCHETYPE_FX_ROW[5] = {
    0,  // TANK     → Vermelho/Laranja
    2,  // FIGHTER  → Azul
    1,  // MAGE     → Roxo
    3,  // ASSASSIN → Verde
    4   // SUPPORT  → Dourado
};

// Offset de rotação baseado na orientação dos sprites no PNG.
// Se os sprites no PNG apontam para CIMA (topo da imagem), use 90.0f.
// Se apontam para a DIREITA, use 0.0f.
// Se apontam para BAIXO, use -90.0f (ou 270.0f).
// Ajuste conforme necessário para cada sprite sheet.
static constexpr float FX_SPRITE_ROTATION_OFFSET = 90.0f;

void loadTextures() {
    if (texturesLoaded) return;

    trainerTextures.resize(g_trainerDefs.size());
    trainerCardTextures.resize(g_trainerDefs.size());
    for (int i = 0; i < (int)g_trainerDefs.size(); i++) {
        if (!g_trainerDefs[i].portraitPath.empty())
            trainerTextures[i] = LoadTexture(g_trainerDefs[i].portraitPath.c_str());
        if (!g_trainerDefs[i].cardPath.empty())
            trainerCardTextures[i] = LoadTexture(g_trainerDefs[i].cardPath.c_str());
    }

    heroTextures.resize(g_heroDefs.size());
    for (int i = 0; i < (int)g_heroDefs.size(); i++) {
        heroTextures[i] = LoadTexture(g_heroDefs[i].assetPath.c_str());
    }
    texturesLoaded = true;
}

Vector2 cellCenter(int cx, int cy) {
    return { GX + cx * CELLW + CELLW * 0.5f, GY + cy * CELLH + CELLH * 0.5f };
}

Rectangle cellRect(int cx, int cy) {
    return { GX + cx * CELLW, GY + cy * CELLH, CELLW, CELLH };
}

void drawGrid() {
    Color c = { 255, 255, 255, 65 };
    for (int col = 0; col <= GRID_COLS; col++) DrawLineV({ GX + col * CELLW, GY }, { GX + col * CELLW, GY + GH }, c);
    for (int row = 0; row <= GRID_ROWS; row++) DrawLineV({ GX, GY + row * CELLH }, { GX + GW, GY + row * CELLH }, c);
    
    float midX = GX + 4 * CELLW;
    DrawLineEx({ midX, GY }, { midX, GY + GH }, 2.f, { 255, 255, 100, 160 });
}

void drawBuffZones(const GameSnapshot& snap) {
    for (int i = 0; i < snap.buffZoneCount; i++) {
        const auto& bz = snap.buffZones[i];
        if (bz.x >= GRID_COLS) continue;
        Rectangle r = cellRect(bz.x, bz.y);
        Color fill;
        if (bz.type == BUFF_AD) fill = (Color){255,140,0,70};
        else if (bz.type == BUFF_HP) fill = (Color){0,200,80,70};
        else fill = (Color){50,180,255,70};
        DrawRectangleRec(r, fill);
    }
}

void drawPedestal(Vector2 ctr, int ownerId) {
    float rx = CELLW * 0.55f;
    float ry = CELLH * 0.22f;
    Color fill = (ownerId == 0) ? Color{60, 120, 230, 120} : Color{230, 60, 60, 120};
    Color border = (ownerId == 0) ? Color{40, 90, 200, 180} : Color{200, 40, 40, 180};
    float baseY = ctr.y + CELLH * 0.32f;
    DrawEllipse((int)ctr.x, (int)baseY, rx, ry, fill);
    DrawEllipseLines((int)ctr.x, (int)baseY, rx, ry, border);
}

void drawHero(const HeroNetState& hs, Vector2 ctr, int myId, bool dragging,
              float breathScale, float tiltAngle)
{
    float r = CELLW * 0.42f;
    Color pCol = kPlayerColor[hs.ownerId];
    if (dragging) pCol.a = 120;

    float heroShift = -CELLH * 0.12f;
    Vector2 heroCtr = { ctr.x, ctr.y + heroShift };

    uint8_t texIdx = (hs.heroDefIndex < (int)g_heroDefs.size()) ? hs.heroDefIndex : hs.archetype;
    Texture2D& tex = heroTextures[texIdx];
    if (tex.id != 0) {
        float scaleMult = 0.9000f;
        float wantedH = CELLH * scaleMult;
        float scale = wantedH / tex.height;
        float sw = tex.width  * scale;
        float sh = tex.height * scale * breathScale;

        DrawTexturePro(
            tex,
            { 0, 0, (float)tex.width, (float)tex.height },
            { heroCtr.x, heroCtr.y, sw, sh },
            { sw * 0.5f, sh * 0.5f }, tiltAngle * RAD2DEG, WHITE
        );
    } else {
        const char* archNames[] = {"TNK", "FGT", "MAG", "ASN", "SUP"};
        DrawText(archNames[hs.archetype], (int)heroCtr.x - 12, (int)heroCtr.y - 6, 10, WHITE);
    }

    Color rimColor = (hs.ownerId == (uint8_t)myId) ? WHITE : LIGHTGRAY;
    DrawCircleLinesV(heroCtr, r, { rimColor.r, rimColor.g, rimColor.b, 90 });
    DrawCircleLinesV(heroCtr, r - 1, { pCol.r, pCol.g, pCol.b, 90 });

    if (hs.ultActive) {
        float t = (float)GetTime();
        float pulse = (sinf(t * 12.0f) + 1.0f) * 0.5f;
        
        // Flashy rings
        DrawCircleLinesV(heroCtr, r + 4 + pulse * 4, ColorAlpha(GOLD, 0.8f));
        DrawCircleLinesV(heroCtr, r + 7 + pulse * 6, ColorAlpha(YELLOW, 0.5f * pulse));
        
        // Floating label with power name from DB (randomized phrase)
        const char* ultTxt = "PODER ATIVO!";
        if (hs.heroDefIndex < (uint8_t)g_heroDefs.size()) {
            const auto& phrases = g_heroDefs[hs.heroDefIndex].ultimateNames;
            if (hs.ultPhraseIdx < phrases.size()) {
                ultTxt = phrases[hs.ultPhraseIdx].c_str();
            }
        }

        int fs = 11;
        int tw = MeasureText(ultTxt, fs);
        float boxW = (float)tw + 12.f;
        DrawRectangleRounded({ heroCtr.x - boxW * 0.5f, heroCtr.y - r - 34, boxW, 16.f }, 0.5f, 4, { 255, 200, 0, 180 });
        DrawText(ultTxt, (int)(heroCtr.x - tw / 2), (int)(heroCtr.y - r - 32), fs, BLACK);
    } else if (hs.alive && hs.hp > 0 && hs.hp <= (uint16_t)(hs.maxHp * 0.15f)) {
        // Dying phrase
        const char* dieTxt = "...";
        if (hs.heroDefIndex < (uint8_t)g_heroDefs.size()) {
            dieTxt = g_heroDefs[hs.heroDefIndex].dyingPhrase.c_str();
        }

        int fs = 10;
        int tw = MeasureText(dieTxt, fs);
        float boxW = (float)tw + 12.f;
        DrawRectangleRounded({ heroCtr.x - boxW * 0.5f, heroCtr.y - r - 30, boxW, 14.f }, 0.5f, 4, { 200, 40, 40, 200 });
        DrawText(dieTxt, (int)(heroCtr.x - tw / 2), (int)(heroCtr.y - r - 28), fs, WHITE);
    }

    float bw = CELLW * 0.85f, bh = 6.f;
    float bx = heroCtr.x - bw * 0.5f, by = heroCtr.y - r - 14.f;
    float pct = (hs.maxHp > 0) ? (float)hs.hp / hs.maxHp : 0.f;
    DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, DARKGRAY);
    DrawRectangle((int)bx, (int)by, (int)(bw * pct), (int)bh,
                  pct > 0.5f ? GREEN : (pct > 0.25f ? YELLOW : RED));
    DrawRectangleLinesEx({ bx, by, bw, bh }, 1, { 255, 255, 255, 80 });
}

void drawHUD(const GameSnapshot& snap, int myId) {
    loadTextures();
    (void)myId;

    float sw = g_layout.screenW;
    float midX = sw * 0.5f;

    float pSize = 100.f;
    float pSizeH = 80.f;
    uint8_t tId0 = snap.trainers[0].trainerId;
    uint8_t tId1 = snap.trainers[1].trainerId;
    if (tId0 < (int)g_trainerDefs.size() && trainerTextures[tId0].id != 0) {
        DrawTexturePro(trainerTextures[tId0], {0,0,(float)trainerTextures[tId0].width, (float)trainerTextures[tId0].height}, {10,10,pSize,pSizeH}, {0,0}, 0.f, WHITE);
    }
    if (tId1 < (int)g_trainerDefs.size() && trainerTextures[tId1].id != 0) {
        DrawTexturePro(trainerTextures[tId1], {0,0,(float)trainerTextures[tId1].width, (float)trainerTextures[tId1].height}, {sw - pSize - 10,10,pSize,pSizeH}, {0,0}, 0.f, WHITE);
    }

    char s0[32], s1[32];
    snprintf(s0, sizeof(s0), "P0: %d", snap.trainers[0].score);
    snprintf(s1, sizeof(s1), "P1: %d", snap.trainers[1].score);
    DrawText(s0, 100, 20, 24, BLUE);
    DrawText(s1, (int)(sw - MeasureText(s1, 24) - 100), 20, 24, RED);

    if (snap.phase == PHASE_POSITIONING || snap.phase == PHASE_BATTLE) {
        char t[16]; snprintf(t, sizeof(t), "%ds", snap.timer);
        DrawText(t, (int)(midX - MeasureText(t, 28) * 0.5f), 20, 28, GOLD);
    }

    if (snap.trainers[0].abilityReady || snap.trainers[1].abilityReady) {
        const char* msg = "P1: Q  |  P2: E  = Poder do Treinador";
        DrawText(msg, (int)(midX - MeasureText(msg, 20) * 0.5f), (int)g_layout.trainerAbilityBtnY, 20, YELLOW);
    }

    if (snap.phase == PHASE_POSITIONING) {
        const char* hint = "P1: 1/2/3 heroi  |  WASD mover  |  Space posicionar     |     P2: Numpad 1/2/3 heroi  |  Setas mover  |  Enter posicionar";
        int hw = MeasureText(hint, 18);
        DrawRectangle((int)(midX - hw * 0.5f - 6), (int)(g_layout.controlsHintY - 6), hw + 12, 30, {0,0,0,200});
        DrawText(hint, (int)(midX - hw * 0.5f), (int)g_layout.controlsHintY, 18, SKYBLUE);
    }
}

void drawOverlays(const GameSnapshot& snap, int myId) {
    (void)myId;
    float sw = g_layout.screenW, sh = g_layout.screenH;
    float midX = sw * 0.5f;
    if (snap.phase == PHASE_ROUND_END) {
        DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 100});
        const char* msg = (snap.roundWinner == 0) ? "PONTO PARA P1!" : (snap.roundWinner == 1 ? "PONTO PARA P2!" : "EMPATE!");
        DrawText(msg, (int)(midX - MeasureText(msg, 40) * 0.5f), (int)(sh * 0.44f), 40, snap.roundWinner == 0 ? BLUE : RED);
    }
    if (snap.phase == PHASE_MATCH_END) {
        DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 200});
        const char* res = (snap.matchWinner == 0) ? "P1 VENCEU!" : "P2 VENCEU!";
        DrawText(res, (int)(midX - MeasureText(res, 60) * 0.5f), (int)(sh * 0.44f), 60, GOLD);
        const char* restart = "Feche o jogo para reiniciar";
        DrawText(restart, (int)(midX - MeasureText(restart, 20) * 0.5f), (int)(sh * 0.58f), 20, LIGHTGRAY);
    }
}

void drawVSScreen(const GameSnapshot& snap, int myId) {
    loadTextures();
    (void)myId;

    float sw = g_layout.screenW, sh = g_layout.screenH;
    float midX = sw * 0.5f;

    ClearBackground({12, 12, 26, 255});
    DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 80});

    static const Color kTint[2] = { {80, 160, 230, 255}, {230, 80, 80, 255} };

    auto drawSide = [&](int side, float cx, float tintColor) {
        (void)tintColor;
        uint8_t tId = snap.trainers[side].trainerId;
        if (tId < (int)g_trainerDefs.size()) {
            const TrainerDefEntry& td = g_trainerDefs[tId];

            float pSize = 140.f;
            float pSizeH = 140.f;
            float px = cx - pSize * 0.5f;
            float py = 60.f;
            if (trainerTextures[tId].id != 0) {
                DrawTexturePro(trainerTextures[tId],
                    {0, 0, (float)trainerTextures[tId].width, (float)trainerTextures[tId].height},
                    {px, py, pSize, pSizeH}, {0, 0}, 0.f, WHITE);
            } else {
                DrawRectangle((int)px, (int)py, (int)pSize, (int)pSize, kTint[side]);
            }

            int nw = MeasureText(td.name.c_str(), 22);
            DrawText(td.name.c_str(), (int)(cx - nw * 0.5f), (int)(py + pSize + 10), 22, WHITE);

            int dw = MeasureText(td.discipline.c_str(), 14);
            DrawText(td.discipline.c_str(), (int)(cx - dw * 0.5f), (int)(py + pSize + 36), 14, LIGHTGRAY);
        }

        uint8_t heroDefIdxs[3];
        int heroCount = 0;
        for (int i = 0; i < snap.heroCount && heroCount < 3; i++) {
            if (snap.heroes[i].ownerId == (uint8_t)side) {
                heroDefIdxs[heroCount++] = snap.heroes[i].heroDefIndex;
            }
        }

        float cardW = 110.f, cardH = 140.f;
        float gap = 16.f;
        float totalW = heroCount * cardW + (heroCount - 1) * gap;
        float startX = cx - totalW * 0.5f;
        float hy = 280.f;

        for (int h = 0; h < heroCount; h++) {
            uint8_t hDefIdx = heroDefIdxs[h];
            if (hDefIdx >= (int)g_heroDefs.size()) continue;
            const HeroDefEntry& hd = g_heroDefs[hDefIdx];

            float hx = startX + h * (cardW + gap);

            DrawRectangleRounded({hx, hy, cardW, cardH}, 0.06f, 6, {30, 30, 50, 255});
            DrawRectangleRoundedLines({hx, hy, cardW, cardH}, 0.06f, 6, kTint[side]);

            float ps = 64.f;
            float psH = 64.f;
            float pxx = hx + (cardW - ps) * 0.5f;
            float pyy = hy + 10.f;
            if (heroTextures[hDefIdx].id != 0) {
                DrawTexturePro(heroTextures[hDefIdx],
                    {0, 0, (float)heroTextures[hDefIdx].width, (float)heroTextures[hDefIdx].height},
                    {pxx, pyy, ps, psH}, {0, 0}, 0.f, WHITE);
            } else {
                DrawRectangle((int)pxx, (int)pyy, (int)ps, (int)ps, kTint[side]);
            }

            int nn = MeasureText(hd.name.c_str(), 11);
            DrawText(hd.name.c_str(), (int)(hx + (cardW - nn) * 0.5f), (int)(pyy + ps + 6), 11, WHITE);

            static const char* archNames[] = {"Tank", "Fighter", "Mage", "Assassin", "Support"};
            static const Color archColors[] = {
                {80,130,220,255}, {220,80,80,255}, {150,80,220,255},
                {70,70,70,255},   {80,200,130,255}
            };
            const char* cls = archNames[hd.archetype];
            int cw = MeasureText(cls, 10);
            DrawText(cls, (int)(hx + (cardW - cw) * 0.5f), (int)(pyy + ps + 20), 10, archColors[hd.archetype]);

            char stats[48];
            snprintf(stats, sizeof(stats), "HP:%d AD:%d ARM:%d", hd.hp, hd.ad, hd.arm);
            int sw = MeasureText(stats, 10);
            DrawText(stats, (int)(hx + (cardW - sw) * 0.5f), (int)(pyy + ps + 34), 10, GRAY);
        }
    };

    drawSide(0, sw * 0.25f, 0);
    drawSide(1, sw * 0.75f, 1);

    DrawText("VS", (int)(midX - MeasureText("VS", 72) * 0.5f), (int)(sh * 0.22f), 72, GOLD);

    if (snap.phase == PHASE_VS_INTRO) {
        char timerStr[16];
        snprintf(timerStr, sizeof(timerStr), "%d", snap.timer);
        DrawText(timerStr, (int)(midX - MeasureText(timerStr, 48) * 0.5f), (int)(sh * 0.85f), 48, {255, 255, 255, 180});
    }

    const char* hint = "Preparando arena...";
    DrawText(hint, (int)(midX - MeasureText(hint, 18) * 0.5f), (int)(sh * 0.94f), 18, {255, 255, 255, 120});
}

void unloadTextures() {
    if (!texturesLoaded) return;
    unloadIcons();
    for (auto& t : trainerTextures) if (t.id != 0) UnloadTexture(t);
    for (auto& t : trainerCardTextures) if (t.id != 0) UnloadTexture(t);
    for (auto& t : heroTextures)    if (t.id != 0) UnloadTexture(t);
    trainerTextures.clear();
    trainerCardTextures.clear();
    heroTextures.clear();
    shutdownFxSystem();
    texturesLoaded = false;
}

// ═════════════════════════════════════════════════════════════════════════════
//  SPRITE SHEET FX SYSTEM
// ═════════════════════════════════════════════════════════════════════════════

static SpriteSheet loadSpriteSheet(const char* path, int cols, int rows) {
    SpriteSheet s;
    s.texture = LoadTexture(path);
    s.cols = cols;
    s.rows = rows;
    s.frameWidth  = s.texture.width  / cols;
    s.frameHeight = s.texture.height / rows;
    return s;
}

static Rectangle getFrameRect(const SpriteSheet& s, int frame, int row) {
    return {
        (float)(frame % s.cols) * s.frameWidth,
        (float)(row % s.rows)    * s.frameHeight,
        (float)s.frameWidth,
        (float)s.frameHeight
    };
}

void initFxSystem() {
    fxSheets[0] = loadSpriteSheet("assets/fx/attack_05.png", 14, 9);
    fxSheetCount = 1;
}

void shutdownFxSystem() {
    for (int i = 0; i < fxSheetCount; i++) {
        if (fxSheets[i].texture.id != 0) {
            UnloadTexture(fxSheets[i].texture);
            fxSheets[i].texture = Texture2D{};
        }
    }
    fxSheetCount = 0;
    fxAnims.clear();
}

static void spawnFx(Vector2 from, Vector2 to, uint8_t archetype, float travelDur) {
    FxAnim fx;
    fx.from           = from;
    fx.to             = to;
    fx.pos            = from;
    fx.sheetIndex     = 0;
    fx.spriteRow      = ARCHETYPE_FX_ROW[archetype % 5];
    fx.currentFrame   = 0;
    fx.frameTimer     = 0.f;
    fx.frameDuration  = 0.03f;
    fx.travelTimer    = 0.f;
    fx.travelDuration = travelDur;
    fx.isTraveling    = true;
    fx.isExploding    = false;

    // Calcula o ângulo de direção do ataque (em graus)
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    fx.rotation = atan2f(dy, dx) * RAD2DEG + FX_SPRITE_ROTATION_OFFSET;

    fxAnims.push_back(fx);
}

void spawnRangedFx(Vector2 from, Vector2 to, uint8_t archetype) {
    spawnFx(from, to, archetype, 0.30f);
}

void spawnMeleeFx(Vector2 from, Vector2 to, uint8_t archetype) {
    spawnFx(from, to, archetype, 0.15f);
}

void updateAndDrawFxAnims(float dt) {
    for (int i = (int)fxAnims.size() - 1; i >= 0; i--) {
        FxAnim& fx = fxAnims[i];
        fx.frameTimer += dt;

        // Avança frame
        if (fx.frameTimer >= fx.frameDuration) {
            fx.frameTimer -= fx.frameDuration;
            fx.currentFrame++;
        }

        if (fx.isTraveling) {
            // Fase de viagem: move de A → B e loopa frames
            fx.travelTimer += dt;
            float t = fminf(1.f, fx.travelTimer / fx.travelDuration);
            fx.pos.x = fx.from.x + (fx.to.x - fx.from.x) * t;
            fx.pos.y = fx.from.y + (fx.to.y - fx.from.y) * t;

            const SpriteSheet& sheet = fxSheets[fx.sheetIndex];
            if (fx.currentFrame >= sheet.cols) {
                fx.currentFrame = 0; // loop durante viagem
            }

            // Chegou no alvo → troca para explosão
            if (t >= 1.f) {
                fx.isTraveling = false;
                fx.isExploding = true;
                fx.currentFrame = 0;
                fx.frameTimer = 0.f;
            }
        } else if (fx.isExploding) {
            // Fase de explosão: fixo no alvo, toca frames sem loop
            fx.pos = fx.to;
            const SpriteSheet& sheet = fxSheets[fx.sheetIndex];
            if (fx.currentFrame >= sheet.cols) {
                fxAnims.erase(fxAnims.begin() + i);
                continue;
            }
        }

        // Draw
        const SpriteSheet& sheet = fxSheets[fx.sheetIndex];
        Rectangle src = getFrameRect(sheet, fx.currentFrame, fx.spriteRow);
        float spriteSize = CELLW * 0.8f;
        Rectangle dst = {
            fx.pos.x - spriteSize * 0.5f,
            fx.pos.y - spriteSize * 0.5f,
            spriteSize,
            spriteSize
        };
        Vector2 origin = { spriteSize * 0.5f, spriteSize * 0.5f };
        DrawTexturePro(sheet.texture, src, dst, origin, fx.rotation, WHITE);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  FLOATING DAMAGE/HEAL TEXT
// ═════════════════════════════════════════════════════════════════════════════

struct FloatingText {
    Vector2 pos;
    int     value;
    float   timer;
    float   maxTimer;
    Color   color;
};

static std::vector<FloatingText> floatingTexts;

void spawnFloatingText(Vector2 pos, int value, Color color) {
    floatingTexts.push_back({ pos, value, 1.0f, 1.0f, color });
}

void updateAndDrawFloatingTexts(float dt) {
    for (int i = (int)floatingTexts.size() - 1; i >= 0; i--) {
        FloatingText& ft = floatingTexts[i];
        ft.timer -= dt;
        if (ft.timer <= 0.f) {
            floatingTexts.erase(floatingTexts.begin() + i);
            continue;
        }
        ft.pos.y -= 30.f * dt;
        float alpha = ft.timer / ft.maxTimer;
        Color c = ft.color;
        c.a = (unsigned char)(255.f * alpha);
        char buf[16];
        snprintf(buf, sizeof(buf), "%+d", ft.value);
        int fontSize = 16;
        int w = MeasureText(buf, fontSize);
        DrawText(buf, (int)(ft.pos.x - w * 0.5f), (int)ft.pos.y, fontSize, c);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  VISUAL EFFECTS
// ═════════════════════════════════════════════════════════════════════════════

struct VisualEffect {
    Vector2 pos;
    float   timer;
    float   maxTimer;
    float   startRadius;
    float   endRadius;
    Color   color;
    int     type;
};

static std::vector<VisualEffect> visualEffects;

void spawnDeathEffect(Vector2 pos) {
    visualEffects.push_back({
        pos, 0.6f, 0.6f, 5.f, 45.f,
        {200, 200, 220, 255}, 0
    });
}

void spawnUltimateEffect(Vector2 pos) {
    visualEffects.push_back({
        pos, 0.4f, 0.4f, 10.f, 50.f,
        {255, 215, 0, 255}, 1
    });
}

void updateAndDrawVisualEffects(float dt) {
    for (int i = (int)visualEffects.size() - 1; i >= 0; i--) {
        VisualEffect& ve = visualEffects[i];
        ve.timer -= dt;
        if (ve.timer <= 0.f) {
            visualEffects.erase(visualEffects.begin() + i);
            continue;
        }
        float t = 1.f - (ve.timer / ve.maxTimer);
        float radius = ve.startRadius + (ve.endRadius - ve.startRadius) * t;
        float alpha = 1.f - t;
        Color c = ve.color;
        c.a = (unsigned char)(255.f * alpha);

        if (ve.type == 0) {
            DrawCircleLinesV(ve.pos, radius, c);
            DrawCircleLinesV(ve.pos, radius * 0.7f, c);
        } else {
            Color fill = c; fill.a = (unsigned char)(80.f * alpha);
            DrawCircleV(ve.pos, radius, fill);
            DrawCircleLinesV(ve.pos, radius, c);
            int rays = 8;
            for (int r = 0; r < rays; r++) {
                float angle = (float)r * (2.f * PI / rays) + t * PI;
                Vector2 end = {
                    ve.pos.x + cosf(angle) * radius * 1.3f,
                    ve.pos.y + sinf(angle) * radius * 1.3f
                };
                DrawLineV(ve.pos, end, c);
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  HIT FLASH
// ═════════════════════════════════════════════════════════════════════════════

struct HitFlash {
    Vector2 pos;
    float   timer;
    float   maxTimer;
};

static std::vector<HitFlash> hitFlashes;

void spawnHitFlash(Vector2 pos) {
    hitFlashes.push_back({ pos, 0.066f, 0.066f });
}

void updateAndDrawHitFlashes(float dt) {
    for (int i = (int)hitFlashes.size() - 1; i >= 0; i--) {
        HitFlash& hf = hitFlashes[i];
        hf.timer -= dt;
        if (hf.timer <= 0.f) {
            hitFlashes.erase(hitFlashes.begin() + i);
            continue;
        }
        float alpha = hf.timer / hf.maxTimer;
        Color c = {255, 255, 255, (unsigned char)(255.f * alpha)};
        DrawCircleV(hf.pos, CELLW * 0.42f * 1.2f, c);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  TARGETING ARROWS
// ═════════════════════════════════════════════════════════════════════════════

void drawTargetArrow(Vector2 from, Vector2 to) {
    Color c = {255, 50, 50, 200};
    float thickness = 3.f;
    DrawLineEx(from, to, thickness, c);

    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.f) return;

    float nx = dx / len, ny = dy / len;
    float headLen = 10.f;
    float headAngle = 0.5f;

    Vector2 tip1 = {
        to.x - headLen * (nx * cosf(headAngle) - ny * sinf(headAngle)),
        to.y - headLen * (nx * sinf(headAngle) + ny * cosf(headAngle))
    };
    Vector2 tip2 = {
        to.x - headLen * (nx * cosf(-headAngle) - ny * sinf(-headAngle)),
        to.y - headLen * (nx * sinf(-headAngle) + ny * cosf(-headAngle))
    };
    DrawTriangle(to, tip1, tip2, c);
}

void drawTargetHighlight(Vector2 pos, float radius, Color color) {
    float t = (float)GetTime();
    unsigned char alpha = (unsigned char)(150 + 105 * sinf(t * 6.f));
    Color c = color; c.a = alpha;
    DrawCircleLinesV(pos, radius + 5, c);
    DrawCircleLinesV(pos, radius + 8, {c.r, c.g, c.b, (unsigned char)(alpha * 0.5f)});
}

void drawAdjacentEnemyHighlights(const GameSnapshot& snap, int myId, int heroIdx) {
    if (heroIdx < 0 || heroIdx >= snap.heroCount) return;
    const HeroNetState& hero = snap.heroes[heroIdx];
    if (hero.ownerId != (uint8_t)myId || !hero.alive) return;

    Vector2 heroPos = cellCenter(hero.x, hero.y);
    float heroR = CELLW * 0.42f;

    for (int i = 0; i < snap.heroCount; i++) {
        if (snap.heroes[i].ownerId == (uint8_t)myId) continue;
        if (!snap.heroes[i].alive) continue;

        int dx = abs((int)hero.x - (int)snap.heroes[i].x);
        int dy = abs((int)hero.y - (int)snap.heroes[i].y);
        if (dx <= 1 && dy <= 1) {
            Vector2 enemyPos = cellCenter(snap.heroes[i].x, snap.heroes[i].y);
            float t = (float)GetTime();
            unsigned char alpha = (unsigned char)(100 + 80 * sinf(t * 4.f));
            DrawCircleLinesV(enemyPos, heroR + 4, {255, 80, 80, alpha});
        }
    }
}

// ── drawTargetingVisuals ─────────────────────────────────────────────────────
void drawTargetingVisuals(const GameSnapshot& snap, const PlayerInput& p1, const PlayerInput& p2)
{
    const PlayerInput* players[2] = {&p1, &p2};
    for (int p = 0; p < 2; p++) {
        const PlayerInput& inp = *players[p];
        if (!inp.targetingMode) continue;

        int heroG = heroSlotToGlobal(snap, p, inp.targetingHeroIdx);
        if (heroG < 0 || !snap.heroes[heroG].alive) continue;

        Vector2 from = cellCenter(snap.heroes[heroG].x, snap.heroes[heroG].y);
        Vector2 to   = cellCenter(inp.targetCursorX, inp.targetCursorY);

        drawTargetArrow(from, to);
        drawAdjacentEnemyHighlights(snap, p, heroG);

        float t = (float)GetTime();
        unsigned char alpha = (unsigned char)(180 + 75 * sinf(t * 5.f));
        Rectangle r = cellRect(inp.targetCursorX, inp.targetCursorY);
        DrawRectangleLinesEx(r, 2.f, (Color){255, 255, 100, alpha});
        DrawRectangleLinesEx({r.x - 1, r.y - 1, r.width + 2, r.height + 2}, 1.f,
                             (Color){255, 255, 100, (unsigned char)(alpha / 2)});

        char label[32];
        snprintf(label, sizeof(label), "P%d TARGET", p + 1);
        int labelW = MeasureText(label, 10);
        DrawText(label, (int)(r.x + r.width * 0.5f - labelW * 0.5f), (int)(r.y - 14.f), 10,
                 (Color){255, 255, 100, alpha});
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  SELECTION SCREEN RENDERING
// ═════════════════════════════════════════════════════════════════════════════

static const char*  ARCH_NAMES[]   = {"Tank","Fighter","Mage","Assassin","Support"};
static const Color  ARCH_COLORS[]  = {
    {80,130,220,255}, {220,80,80,255}, {150,80,220,255},
    {70,70,70,255},   {80,200,130,255}
};

static Texture2D* selTrainerTex = nullptr;
static Texture2D* selHeroTex    = nullptr;
static int        selNTrainers  = 0;
static int        selNHeroes    = 0;

void initSelectionAssets(const TrainerDef* trainers, int nT,
                         const HeroDef*   heroes,   int nH) {
    selNTrainers   = nT;
    selNHeroes     = nH;
    selTrainerTex  = new Texture2D[nT];
    selHeroTex     = new Texture2D[nH];
    for (int i = 0; i < nT; i++) {
        selTrainerTex[i] = (trainers[i].portraitPath[0] != '\0')
                           ? LoadTexture(trainers[i].portraitPath)
                           : Texture2D{};
    }
    for (int i = 0; i < nH; i++) {
        selHeroTex[i] = LoadTexture(heroes[i].assetPath);
    }
}

void freeSelectionAssets(int nT, int nH) {
    for (int i = 0; i < nT; i++) if (selTrainerTex[i].id) UnloadTexture(selTrainerTex[i]);
    for (int i = 0; i < nH; i++) if (selHeroTex[i].id)    UnloadTexture(selHeroTex[i]);
    delete[] selTrainerTex; selTrainerTex = nullptr;
    delete[] selHeroTex;    selHeroTex    = nullptr;
}

// ── drawTrainerSelectMK ──────────────────────────────────────────────────────
void drawTrainerSelectMK(const GameSnapshot& snap,
                         const TrainerDef* trainers, int nT,
                         const PlayerInput& p1, const PlayerInput& p2)
{
    static const Color BG   = {12,12,26,220};
    float sw = g_layout.screenW, sh = g_layout.screenH;
    float midX = sw * 0.5f;
    DrawRectangle(0, 0, (int)sw, (int)sh, BG);

    const char* title = "SELECIONE SEU TREINADOR";
    DrawText(title, (int)(midX - MeasureText(title, 28) * 0.5f), 12, 28, WHITE);

    char timerStr[32];
    snprintf(timerStr, sizeof(timerStr), "%.1f", snap.selectTimer);
    DrawText(timerStr, (int)(midX - MeasureText(timerStr, 36) * 0.5f), 48, 36, GOLD);

    // Left portrait (P1)
    {
        float px = 40.f, py = 100.f;
        float pSize = 377.f;
        float pSizeH = 610.f;
        int tIdx = p1.trainerLocked >= 0 ? p1.trainerLocked : p1.trainerCursor;
        if (tIdx < nT && selTrainerTex && selTrainerTex[tIdx].id) {
            DrawTexturePro(selTrainerTex[tIdx],
                {0,0,(float)selTrainerTex[tIdx].width,(float)selTrainerTex[tIdx].height},
                {px, py, pSize, pSizeH}, {0,0}, 0.f, WHITE);
        } else {
            DrawRectangle((int)px, (int)py, (int)pSize, (int)pSizeH, kP1Color);
        }
        DrawRectangleLinesEx({px, py, pSize, pSizeH}, 3, kP1Color);
        const char* p1lbl = "P1";
        DrawText(p1lbl, (int)(px + (pSize - MeasureText(p1lbl, 20))/2), (int)(py + pSizeH + 6), 20, kP1Color);
        if (p1.trainerLocked >= 0) {
            const char* ready = "READY!";
            DrawText(ready, (int)(px + (pSize - MeasureText(ready, 24))/2), (int)(py + pSizeH + 30), 24, GREEN);
        } else if (tIdx < nT) {
            DrawText(trainers[tIdx].name, (int)(px + (pSize - MeasureText(trainers[tIdx].name, 14))/2), (int)(py + pSizeH + 30), 14, WHITE);
        }
    }

    // Right portrait (P2)
    {
        float px = sw - 40 - 377.f;
        float py = 100.f;
        float pSize = 377.f;
        float pSizeH = 610.f;
        int tIdx = p2.trainerLocked >= 0 ? p2.trainerLocked : p2.trainerCursor;
        if (tIdx < nT && selTrainerTex && selTrainerTex[tIdx].id) {
            DrawTexturePro(selTrainerTex[tIdx],
                {0,0,(float)selTrainerTex[tIdx].width,(float)selTrainerTex[tIdx].height},
                {px, py, pSize, pSizeH}, {0,0}, 0.f, WHITE);
        } else {
            DrawRectangle((int)px, (int)py, (int)pSize, (int)pSizeH, kP2Color);
        }
        DrawRectangleLinesEx({px, py, pSize, pSizeH}, 3, kP2Color);
        const char* p2lbl = "P2";
        DrawText(p2lbl, (int)(px + (pSize - MeasureText(p2lbl, 20))/2), (int)(py + pSizeH + 6), 20, kP2Color);
        if (p2.trainerLocked >= 0) {
            const char* ready = "READY!";
            DrawText(ready, (int)(px + (pSize - MeasureText(ready, 24))/2), (int)(py + pSizeH + 30), 24, GREEN);
        } else if (tIdx < nT) {
            DrawText(trainers[tIdx].name, (int)(px + (pSize - MeasureText(trainers[tIdx].name, 14))/2), (int)(py + pSizeH + 30), 14, WHITE);
        }
    }

    // Trainer cards at bottom — positioned with ~20% bottom margin
    const float CW = 188.f, CH = 200.f, PAD = 16.f;
    float totalW = nT * CW + (nT-1) * PAD;
    float startX = (sw - totalW) / 2.f;
    float startY = sh - CH - sh * 0.05f;

    for (int i = 0; i < nT; i++) {
        float x = startX + i * (CW + PAD);
        float y = startY;
        bool hov1 = (i == p1.trainerCursor && p1.trainerLocked < 0);
        bool hov2 = (i == p2.trainerCursor && p2.trainerLocked < 0);

        Color bg = (hov1 || hov2) ? Color{50,50,90,255} : Color{30,30,58,255};
        DrawRectangleRounded({x, y, CW, CH}, 0.08f, 6, bg);
        DrawRectangleRoundedLines({x, y, CW, CH}, 0.08f, 6, {60,60,90,255});

        float psH = 100.f;
        float ps = 100.f;
        if (selTrainerTex && selTrainerTex[i].id) {
            ps = psH * ((float)selTrainerTex[i].width / selTrainerTex[i].height);
        }
        float px2 = x + (CW-ps)/2.f, py2 = y + 14.f;
        if (selTrainerTex && selTrainerTex[i].id) {
            DrawTexturePro(selTrainerTex[i],
                {0,0,(float)selTrainerTex[i].width,(float)selTrainerTex[i].height},
                {px2, py2, ps, psH}, {}, 0.f, WHITE);
        } else {
            DrawRectangleRounded({px2,py2,ps,psH}, 0.2f, 6, trainers[i].color);
        }

        int nw = MeasureText(trainers[i].name, 14);
        DrawText(trainers[i].name, (int)(x+(CW-nw)/2), (int)(py2+ps+10), 14, WHITE);

        int dw = MeasureText(trainers[i].discipline, 11);
        DrawText(trainers[i].discipline, (int)(x+(CW-dw)/2), (int)(py2+ps+28), 11, {160,160,190,255});

        char abuf[32]; snprintf(abuf, sizeof(abuf), "Poder: %s", trainers[i].abilityName);
        int aw = MeasureText(abuf, 11);
        DrawText(abuf, (int)(x+(CW-aw)/2), (int)(py2+ps+48), 11, trainers[i].color);

        // P1 cursor
        if (hov1) {
            float t = (float)GetTime();
            unsigned char alpha = (unsigned char)(180 + 75 * sinf(t * 4.f));
            Color bc = kP1Color; bc.a = alpha;
            DrawRectangleLinesEx({x-4, y-4, CW+8, CH+8}, 3, bc);
        }
        // P2 cursor
        if (hov2) {
            float t = (float)GetTime();
            unsigned char alpha = (unsigned char)(180 + 75 * sinf(t * 4.f));
            Color bc = kP2Color; bc.a = alpha;
            DrawRectangleLinesEx({x-2, y-2, CW+4, CH+4}, 2, bc);
        }
    }

    // Controls
    float cy = startY + CH + 16.f;
    const char* ctrlHint = "P1: A / D + Space    |    P2: ← → + Enter";
    DrawText(ctrlHint, (int)(midX - MeasureText(ctrlHint, 16) * 0.5f), (int)cy, 16, {160,160,190,255});
}

// ── drawHeroSelectMK ─────────────────────────────────────────────────────────
// Mortal Kombat style: trainers standing on sides, hero cards at bottom
void drawHeroSelectMK(const GameSnapshot& snap,
                      const TrainerDef* trainers,
                      const HeroDef* heroes, int nH,
                      const PlayerInput& p1, const PlayerInput& p2)
{
    (void)trainers;
    static const Color BG = {12,12,26,180};
    float sw = g_layout.screenW, sh = g_layout.screenH;
    float midX = sw * 0.5f;
    DrawRectangle(0, 0, (int)sw, (int)sh, BG);

    char timerStr[32];
    snprintf(timerStr, sizeof(timerStr), "Escolha 3 Herois  —  %.1f", snap.selectTimer);
    int tw = MeasureText(timerStr, 24);
    DrawRectangle((int)(midX - tw * 0.5f - 8), 6, tw + 16, 36, {0,0,0,160});
    DrawText(timerStr, (int)(midX - tw * 0.5f), 12, 24, GOLD);

    const float CARD_W = 78.f, CARD_H = 110.f, PAD = 6.f;

        auto drawPlayerSide = [&](int pid, const PlayerInput& inp, Color pCol, float sideX, bool isLeft) {
        int tIdx = snap.trainerChoice[pid];

        float areaW = sw * 0.5f;

        // ── Trainer standing on side (large portrait) ──
        float tW = 200.f;
        float tH = 360.f;
        float tX = isLeft ? 30.f : (sw - 30 - tW);
        float tY = 50.f;

        // Shadow/ground effect under trainer
        DrawEllipse((int)(tX + tW/2), (int)(tY + tH + 10), tW*0.45f, 12.f, {0,0,0,120});

        if (tIdx < (int)g_trainerDefs.size() && selTrainerTex && selTrainerTex[tIdx].id) {
            DrawTexturePro(selTrainerTex[tIdx],
                {0,0,(float)selTrainerTex[tIdx].width,(float)selTrainerTex[tIdx].height},
                {tX, tY, tW, tH}, {0,0}, 0.f, WHITE);
        } else {
            DrawRectangle((int)tX, (int)tY, (int)tW, (int)tH, pCol);
        }
        DrawRectangleLinesEx({tX, tY, tW, tH}, 3, pCol);

        // Trainer name below portrait
        const char* tName = (tIdx < (int)g_trainerDefs.size()) ? g_trainerDefs[tIdx].name.c_str() : "???";
        int tnW = MeasureText(tName, 18);
        DrawRectangle((int)(tX + (tW - tnW)/2 - 4), (int)(tY + tH + 16), tnW + 8, 28, {0,0,0,180});
        DrawText(tName, (int)(tX + (tW - tnW)/2), (int)(tY + tH + 20), 18, WHITE);

        // "READY!" or cursor indicator
        if (inp.herosLocked) {
            const char* ready = "PRONTO!";
            int rw = MeasureText(ready, 20);
            DrawRectangle((int)(tX + (tW - rw)/2 - 4), (int)(tY + tH + 48), rw + 8, 28, {0,0,0,180});
            DrawText(ready, (int)(tX + (tW - rw)/2), (int)(tY + tH + 52), 20, GREEN);
        }

        // ── Pick slots (3 small squares between trainer and cards) ──
        float slotSize = 56.f;
        float slotGap = 10.f;
        float slotsTotalW = 3 * slotSize + 2 * slotGap;
        float slotsX = sideX + (areaW - slotsTotalW) / 2.f;
        float slotsY = 440.f;

        for (int s = 0; s < 3; s++) {
            float sx = slotsX + s * (slotSize + slotGap);
            bool filled = s < (int)inp.heroPicks.size();
            Color slotBg = filled ? Color{40,60,40,220} : Color{30,30,50,180};
            DrawRectangleRounded({sx, slotsY, slotSize, slotSize}, 0.1f, 4, slotBg);
            DrawRectangleRoundedLines({sx, slotsY, slotSize, slotSize}, 0.1f, 4, filled ? GREEN : Color{60,60,80,200});
            if (filled) {
                int hIdx = inp.heroPicks[s];
                float slotW = slotSize - 4;
                float slotH = slotSize - 4;
                if (selHeroTex && selHeroTex[hIdx].id) {
                    DrawTexturePro(selHeroTex[hIdx],
                        {0,0,(float)selHeroTex[hIdx].width,(float)selHeroTex[hIdx].height},
                        {sx+2, slotsY+2, slotW, slotH}, {}, 0.f, WHITE);
                } else {
                    DrawRectangle((int)(sx+2), (int)(slotsY+2), (int)(slotSize-4), (int)(slotSize-4), ARCH_COLORS[heroes[hIdx].archetype]);
                }
                // Small archetype label
                const char* an = ARCH_NAMES[heroes[hIdx].archetype];
                int anw = MeasureText(an, 8);
                DrawRectangle((int)(sx + (slotSize-anw)/2 - 2), (int)(slotsY + slotSize - 14), anw + 4, 12, {0,0,0,200});
                DrawText(an, (int)(sx + (slotSize-anw)/2), (int)(slotsY + slotSize - 12), 8, WHITE);
            } else {
                DrawText("?", (int)(sx + (slotSize - MeasureText("?", 20))/2), (int)(slotsY + (slotSize-20)/2), 20, Color{80,80,100,255});
            }
        }

        // ── Hero cards at bottom (single horizontal row) ──
        std::vector<int> filtered;
        for (int i = 0; i < nH; i++)
            if (heroes[i].trainerIndex == (uint8_t)tIdx)
                filtered.push_back(i);

        int nFiltered = (int)filtered.size();
        float cardsTotalW = nFiltered * CARD_W + (nFiltered - 1) * PAD;
        float cardsX = sideX + (areaW - cardsTotalW) / 2.f;
        float cardsY = 520.f;

        for (int fi = 0; fi < nFiltered; fi++) {
            int i = filtered[fi];
            float x = cardsX + fi * (CARD_W + PAD);
            float y = cardsY;

            bool hov = (fi == inp.heroCursor);
            bool picked = (std::find(inp.heroPicks.begin(), inp.heroPicks.end(), i) != inp.heroPicks.end());

            Color bg = picked ? Color{40,80,40,230} : (hov ? Color{60,60,100,230} : Color{35,35,60,230});
            DrawRectangleRounded({x, y, CARD_W, CARD_H}, 0.08f, 6, bg);

            // Hero portrait
            float imgH = CARD_H * 0.50f;
            float imgW = CARD_W - 6;
            if (selHeroTex && selHeroTex[i].id) {
                imgW = imgH * ((float)selHeroTex[i].width / selHeroTex[i].height);
            }
            float hx = x + (CARD_W - imgW) / 2.f;

            if (selHeroTex && selHeroTex[i].id) {
                DrawTexturePro(selHeroTex[i],
                    {0,0,(float)selHeroTex[i].width,(float)selHeroTex[i].height},
                    {hx, y+4, imgW, imgH}, {}, 0.f, WHITE);
            } else {
                DrawRectangleRounded({x+3, y+4, CARD_W-6, imgH}, 0.1f, 4, ARCH_COLORS[heroes[i].archetype]);
            }

            // Name
            float ty = y + 4 + imgH + 3;
            int nw = MeasureText(heroes[i].name, 9);
            DrawText(heroes[i].name, (int)(x+(CARD_W-nw)/2), (int)ty, 9, WHITE); ty += 12;

            // Class badge
            int bw = MeasureText(ARCH_NAMES[heroes[i].archetype], 9);
            DrawRectangleRounded({x+(CARD_W-bw-8)/2, ty, (float)(bw+8), 14}, 0.4f, 4, ARCH_COLORS[heroes[i].archetype]);
            DrawText(ARCH_NAMES[heroes[i].archetype], (int)(x+(CARD_W-bw)/2), (int)(ty+2), 9, WHITE); ty += 16;

            // Stats
            char sb[32]; snprintf(sb, sizeof(sb), "HP:%d AD:%d", heroes[i].hp, heroes[i].ad);
            int sw = MeasureText(sb, 8);
            DrawText(sb, (int)(x+(CARD_W-sw)/2), (int)ty, 8, {160,160,190,255});

            // Cursor highlight
            if (hov) {
                float t = (float)GetTime();
                unsigned char alpha = (unsigned char)(180 + 75 * sinf(t * 5.f));
                Color bc = pCol; bc.a = alpha;
                DrawRectangleLinesEx({x-3, y-3, CARD_W+6, CARD_H+6}, 3, bc);
            }
            if (picked) {
                DrawRectangleLinesEx({x, y, CARD_W, CARD_H}, 2, GREEN);
                DrawText("✓", (int)(x+CARD_W-14), (int)(y+4), 12, GREEN);
            }
        }

        // Controls hint
        const char* ctrl = (pid == 0) ? "P1: A / D + Space" : "P2: ← → + Enter";
        int cw = MeasureText(ctrl, 16);
        float cx = sideX + (areaW - cw) / 2.f;
        DrawRectangle((int)(cx - 4), 640, cw + 8, 24, {0,0,0,180});
        DrawText(ctrl, (int)cx, 642, 16, pCol);
    };

    drawPlayerSide(0, p1, kP1Color, 0.f, true);
    drawPlayerSide(1, p2, kP2Color, sw * 0.5f, false);
}

// ── isValidDeployCell ────────────────────────────────────────────────────────
bool isValidDeployCell(int col, int row, uint8_t archetype, bool isLeft)
{
    if (isLeft && col > 3) return false;
    if (!isLeft && col < 4) return false;
    switch (archetype) {
        case ARCHETYPE_TANK:     return isLeft ? (col == 3) : (col == 4);
        case ARCHETYPE_FIGHTER:  return isLeft ? (col >= 2 && col <= 3) : (col >= 4 && col <= 5);
        case ARCHETYPE_MAGE:     return isLeft ? (col <= 1) : (col >= 6);
        case ARCHETYPE_ASSASSIN: return ((row <= 1) || (row >= 6)) && (isLeft ? (col <= 3) : (col >= 4));
        case ARCHETYPE_SUPPORT:  return isLeft ? (col <= 2) : (col >= 5);
        default: return false;
    }
}

// ── Helper: modern rounded bar ────────────────────────────────────────────────
static void drawModernBar(float x, float y, float w, float h, float fillPct,
                          Color bgColor, Color fillColor, const char* label,
                          Color labelColor, int fontSize)
{
    float r = h * 0.5f;
    DrawRectangleRounded({x, y, w, h}, 1.f, 8, bgColor);
    if (fillPct > 0.01f) {
        DrawRectangleRounded({x, y, w * fillPct, h}, 1.f, 8, fillColor);
    }
    DrawRectangleRoundedLines({x, y, w, h}, 1.f, 8, {255,255,255,30});
    if (label) {
        DrawText(label, (int)(x + 6), (int)(y + (h - fontSize) * 0.5f), fontSize, labelColor);
    }
}

// ── Helper: draw text with word-wrap ──────────────────────────────────────────
static void drawWrappedText(const char* text, float x, float y, float maxWidth,
                            int fontSize, Color color, int* outHeight)
{
    int totalLen = (int)strlen(text);
    int lineStart = 0;
    float cy = y;
    int lineH = fontSize + 2;

    while (lineStart < totalLen) {
        int lineEnd = totalLen;
        for (int i = lineStart; i < totalLen; i++) {
            if (text[i] == ' ') {
                int segLen = i - lineStart;
                if (segLen > 0 && segLen < 256) {
                    char buf[256];
                    memcpy(buf, text + lineStart, segLen);
                    buf[segLen] = '\0';
                    if (MeasureText(buf, fontSize) > maxWidth) {
                        lineEnd = i;
                        break;
                    }
                }
            }
        }
        int segLen = lineEnd - lineStart;
        if (segLen > 255) segLen = 255;
        char buf[256];
        memcpy(buf, text + lineStart, segLen);
        buf[segLen] = '\0';
        DrawText(buf, (int)x, (int)cy, fontSize, color);
        cy += lineH;
        lineStart = (lineEnd < totalLen) ? lineEnd : totalLen;
        if (lineStart < totalLen && text[lineStart] == ' ') lineStart++;
    }
    if (outHeight) *outHeight = (int)(cy - y);
}

// ── drawShop ─────────────────────────────────────────────────────────────────
void drawShop(const GameSnapshot& snap, const PlayerInput& p1, const PlayerInput& p2)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    loadIcons();

    DrawRectangle(0, 0, (int)sw, (int)sh, {10, 10, 30, 240});

    static Color rarityColors[] = {
        {180, 180, 180, 255}, {80, 180, 80, 255}, {80, 80, 255, 255}, {220, 80, 220, 255},
    };

    for (int pid = 0; pid < 2; pid++) {
        PlayerInput& inp = const_cast<PlayerInput&>((pid == 0) ? p1 : p2);
        float sideX = (pid == 0) ? 0.f : sw * 0.5f;
        float sideW = sw * 0.5f;
        Color pCol = (pid == 0) ? (Color){80, 150, 255, 255} : (Color){255, 100, 80, 255};
        const ShopSnapshot& ss = snap.shop;

        DrawLineEx({sideX + sideW, 0}, {sideX + sideW, sh}, 2, {80, 80, 120, 150});

        // ── Calculate total height for vertical centering ────────────────────
        float cardW = (sideW - 30 - 2 * 8) / 3.f;
        float cardH = 90.f;
        int stockShow = ss.stockCount < 6 ? ss.stockCount : 6;
        int genCnt = 0;
        int heroCnt = 0;
        for (int i = 0; i < stockShow; i++) {
            if (ss.stock[i].category == ITEM_CATEGORY_GENERAL && genCnt < 3) genCnt++;
            else if (ss.stock[i].category == ITEM_CATEGORY_HERO && heroCnt < 3) heroCnt++;
        }
        int pHeroes[3] = {-1,-1,-1}, pHCnt = 0;
        for (int i = 0; i < snap.heroCount && pHCnt < 3; i++)
            if (snap.heroes[i].ownerId == (uint8_t)pid) pHeroes[pHCnt++] = i;

        float contentH = 40.f; // Header
        contentH += 30.f; // General title + space
        contentH += cardH + 20.f; // General cards + gap
        for (int h = 0; h < pHCnt; h++) {
            contentH += 25.f; // Hero title/stats
            contentH += cardH + 15.f; // Hero cards + gap
        }
        contentH += 70.f; // Description box area

        float y = (sh - contentH) * 0.5f;
        if (y < 10.f) y = 10.f;

        // Find selected item index for description
        const ShopItemInfo* selItem = nullptr;
        int stockShowActual = ss.stockCount < 6 ? ss.stockCount : 6;
        int genIdxs[3] = {-1,-1,-1}, gCnt = 0;
        int heroIdxs[3] = {-1,-1,-1}, hCnt = 0;
        for (int i = 0; i < stockShowActual; i++) {
            if (ss.stock[i].category == ITEM_CATEGORY_GENERAL && gCnt < 3) genIdxs[gCnt++] = i;
            else if (ss.stock[i].category == ITEM_CATEGORY_HERO && hCnt < 3) heroIdxs[hCnt++] = i;
        }
        if (inp.shopCursorY == 0 && inp.shopCursorX < gCnt) selItem = &ss.stock[genIdxs[inp.shopCursorX]];
        else if (inp.shopCursorY > 0 && inp.shopCursorY <= pHCnt && inp.shopCursorX < hCnt) selItem = &ss.stock[heroIdxs[inp.shopCursorX]];

        // ── Header: gold ──
        char goldStr[32];
        snprintf(goldStr, sizeof(goldStr), "P%d OURO: %d", pid + 1, ss.players[pid].gold);
        drawIcon(ICON_COIN, {sideX + 16, y, 28, 28}, GOLD);
        DrawText(goldStr, (int)(sideX + 50), (int)(y + 2), 24, pCol);
        if (ss.players[pid].confirmed)
            DrawText("PRONTO", (int)(sideX + sideW - 80), (int)(y + 4), 18, GREEN);
        y += 45;

        // Helper: draw an item card
        auto drawCard = [&](float cx, float cy, const ShopItemInfo& item, bool sel) {
            Color bg = sel ? Color{45, 45, 80, 250} : Color{25, 25, 50, 240};
            DrawRectangleRounded({cx, cy, cardW, cardH}, 0.08f, 6, bg);
            if (item.rarity < 4)
                DrawRectangleRoundedLines({cx, cy, cardW, cardH}, 0.08f, 6, rarityColors[item.rarity]);
            
            Texture2D* t = itemIdToTexture(item.itemId);
            if (t && t->id != 0) {
                float texSz = 32.f;
                DrawTexturePro(*t, {0, 0, (float)t->width, (float)t->height}, {cx + 8, cy + 8, texSz, texSz}, {0, 0}, 0, WHITE);
            }
            
            int nameSize = item.price > 0 ? 11 : 12;
            DrawText(item.name, (int)(cx + 45), (int)(cy + 10), nameSize, WHITE);
            
            char pStr[16]; snprintf(pStr, sizeof(pStr), "%dg", item.price);
            DrawText(pStr, (int)(cx + 8), (int)(cy + cardH - 22), 16, GOLD);

            if (sel) {
                DrawRectangleRoundedLines({cx, cy, cardW, cardH}, 0.08f, 6, pCol);
                DrawRectangleRoundedLines({cx-1, cy-1, cardW+2, cardH+2}, 0.08f, 6, pCol);
            }
        };

        // ── Row 0: General items ──
        DrawText("ITENS GERAIS", (int)(sideX + 10), (int)y, 16, GOLD);
        y += 24;
        for (int i = 0; i < gCnt; i++) {
            float cx = sideX + 10 + i * (cardW + 8);
            drawCard(cx, y, ss.stock[genIdxs[i]], inp.shopCursorY == 0 && inp.shopCursorX == i);
        }
        y += cardH + 15;

        // ── Rows 1-3: Heroes ──
        for (int h = 0; h < pHCnt; h++) {
            int hi = pHeroes[h];
            const HeroNetState& hero = snap.heroes[hi];
            const char* hName = "???";
            if (hero.heroDefIndex < (int)g_heroDefs.size()) hName = g_heroDefs[hero.heroDefIndex].name.c_str();
            DrawText(hName, (int)(sideX + 10), (int)y, 14, WHITE);

            float hpPct = (hero.maxHp > 0) ? (float)hero.hp / hero.maxHp : 0;
            Color hpCol = hpPct > 0.5f ? GREEN : (hpPct > 0.25f ? YELLOW : RED);
            float hpX = sideX + MeasureText(hName, 14) + 20;
            DrawRectangle((int)hpX, (int)(y + 4), 60, 8, DARKGRAY);
            DrawRectangle((int)hpX, (int)(y + 4), (int)(60 * hpPct), 8, hpCol);

            float slotSz = 22, slotGap = 4;
            float sx = hpX + 80;
            for (int s = 0; s < 4; s++) {
                float ssx = sx + s * (slotSz + slotGap);
                bool filled = (hero.items[s] != 0xFF);
                Color sBg = filled ? Color{40, 70, 40, 255} : Color{25, 25, 45, 220};
                DrawRectangleRounded({ssx, y, slotSz, slotSz}, 0.25f, 4, sBg);
                DrawRectangleRoundedLines({ssx, y, slotSz, slotSz}, 0.25f, 4, {60, 60, 100, 180});
                if (filled) {
                    Texture2D* t = itemIdToTexture(hero.items[s]);
                    if (t && t->id != 0) {
                        DrawTexturePro(*t, {0, 0, (float)t->width, (float)t->height}, {ssx + 2, y + 2, slotSz - 4, slotSz - 4}, {0, 0}, 0, WHITE);
                    }
                }
            }
            y += 25;

            for (int i = 0; i < hCnt; i++) {
                float cx = sideX + 10 + i * (cardW + 8);
                drawCard(cx, y, ss.stock[heroIdxs[i]], inp.shopCursorY == (h + 1) && inp.shopCursorX == i);
            }
            y += cardH + 10;
        }

        // ── Description Box ──
        if (selItem) {
            DrawRectangleRounded({sideX + 10, y, sideW - 20, 60}, 0.1f, 5, {20, 20, 40, 180});
            DrawRectangleRoundedLines({sideX + 10, y, sideW - 20, 60}, 0.1f, 5, pCol);
            DrawText("DESCRICAO:", (int)(sideX + 18), (int)(y + 8), 11, GOLD);
            DrawTextWrapped(GetFontDefault(), selItem->desc, {sideX + 18, y + 24}, 12, sideW - 36, LIGHTGRAY);
        }
        y += 70;

        // ── Controls ──
        const char* ctrl = (pid == 0)
            ? "WASD: Navegar | Espaco: Comprar | F: Confirmar"
            : "Setas: Navegar | Enter: Comprar | . : Confirmar";
        int cw = MeasureText(ctrl, 11);
        DrawRectangleRounded({sideX + (sideW - cw - 40) * 0.5f, sh - 35, (float)cw + 40, 25}, 0.5f, 6, {20, 20, 40, 150});
        DrawText(ctrl, (int)(sideX + (sideW - cw) * 0.5f), (int)(sh - 28), 11, {160, 160, 180, 255});
    }
}

// ── drawHeroCards ────────────────────────────────────────────────────────────
void drawHeroCards(const GameSnapshot& snap, int myId) {
    (void)myId;
    loadTextures();

    float sw = g_layout.screenW, sh = g_layout.screenH;
    float cardW = g_layout.cardW, cardH = g_layout.cardH;
    float cardsY = g_layout.cardsY;

    static const char* archNames[]  = {"Tank", "Fighter", "Mage", "Assassin", "Support"};
    static Color archColors[] = {
        {80,130,220,255}, {220,80,80,255}, {150,80,220,255},
        {70,70,70,255},   {80,200,130,255}
    };

    // Bottom strip: subtle dark panel
    DrawRectangle(0, (int)cardsY - 8, (int)sw, (int)(sh - cardsY + 8), {6, 6, 14, 255});

    for (int player = 0; player < 2; player++) {
        // Each player's container is exactly half the screen width.
        float containerX = player == 0 ? 0.f : sw * 0.5f;
        float containerW = sw * 0.5f;

        // space-around: 3 cards with equal margins on both edges
        float totalCardW = 3.f * cardW;
        float totalGap = containerW - totalCardW;
        float gap = totalGap / 4.f;

        float cardX[3];
        cardX[0] = containerX + gap * 0.5f;
        cardX[1] = cardX[0] + cardW + gap;
        cardX[2] = cardX[1] + cardW + gap;

        Color pCol = player == 0 ? Color{80,150,255,255} : Color{255,100,80,255};
        Color dimCol = player == 0 ? Color{80,150,255,120} : Color{255,100,80,120};

        int localHeroes[3] = {-1, -1, -1};
        int localCount = 0;
        for (int i = 0; i < snap.heroCount; i++) {
            if (snap.heroes[i].ownerId == (uint8_t)player) {
                if (localCount < 3) localHeroes[localCount++] = i;
            }
        }

        for (int h = 0; h < 3; h++) {
            float cx = cardX[h];
            float cy = cardsY;

            if (localHeroes[h] < 0) {
                // Empty slot placeholder
                DrawRectangleRounded({cx + 4, cy + 4, cardW - 8, cardH - 8}, 0.08f, 8, {16, 16, 30, 180});
                DrawRectangleRoundedLines({cx + 4, cy + 4, cardW - 8, cardH - 8}, 0.08f, 8, {40, 40, 60, 120});
                continue;
            }

            const HeroNetState& hs = snap.heroes[localHeroes[h]];
            bool alive = hs.alive;

            // Card background with player-tinted border
            Color cardBg = alive ? (Color){18, 18, 34, 255} : (Color){12, 12, 22, 255};
            DrawRectangleRounded({cx, cy, cardW, cardH}, 0.08f, 10, cardBg);
            Color borderCol = alive ? dimCol : (Color){60, 60, 70, 100};
            DrawRectangleRoundedLines({cx, cy, cardW, cardH}, 0.08f, 10, borderCol);
            // Top accent line
            DrawRectangle((int)cx + 8, (int)cy, (int)(cardW - 16), 2, pCol);

            // Portrait (left side, fixed size, vertically centered)
            float portraitH = 60.f;
            float portraitSize = portraitH; // square aspect for the box
            float px2 = cx + 8.f;
            float py2 = cy + (cardH - portraitSize) * 0.5f;

            uint8_t texIdx = (hs.heroDefIndex < (int)g_heroDefs.size()) ? hs.heroDefIndex : hs.archetype;
            Texture2D& tex = heroTextures[texIdx];
            if (tex.id != 0) {
                float scale = portraitSize / tex.height;
                float sw2 = tex.width * scale;
                DrawTexturePro(tex,
                    {0, 0, (float)tex.width, (float)tex.height},
                    {px2, py2, sw2, portraitSize}, {0, 0}, 0.f, alive ? WHITE : (Color){120,120,120,180});
            } else {
                DrawRectangleRounded({px2, py2, portraitSize, portraitSize}, 0.1f, 6, archColors[hs.archetype]);
            }
            // Portrait border
            DrawRectangleRoundedLines({px2, py2, portraitSize, portraitSize}, 0.1f, 6, {255,255,255,40});

            // Right side info area — starts at top of card for maximum text space
            float infoX = px2 + portraitSize + 10.f;
            float infoY = cy + 6.f;
            float infoW = cardW - (infoX - cx) - 8.f;

            // Hero name (with word-wrap)
            const char* heroName = "???";
            if (hs.heroDefIndex < (int)g_heroDefs.size())
                heroName = g_heroDefs[hs.heroDefIndex].name.c_str();
            int nameH = 0;
            drawWrappedText(heroName, infoX, infoY, infoW, 12,
                            alive ? WHITE : (Color){150,150,150,180}, &nameH);
            infoY += nameH + 4.f;

            // Class badge
            const char* archName = (hs.archetype < 5) ? archNames[hs.archetype] : "???";
            int aw = MeasureText(archName, 9);
            float badgeW = aw + 10.f;
            DrawRectangleRounded({infoX, infoY, badgeW, 15.f}, 0.4f, 4, archColors[hs.archetype]);
            DrawText(archName, (int)(infoX + 5), (int)(infoY + 2), 9, WHITE);
            infoY += 21.f;

            // HP bar
            {
                float bw = infoW, bh = 12.f;
                float hpPct = (hs.maxHp > 0) ? (float)hs.hp / hs.maxHp : 0.f;
                Color hpFill = hpPct > 0.5f ? (Color){60, 200, 80, 255}
                             : (hpPct > 0.25f ? (Color){220, 180, 40, 255} : (Color){220, 60, 60, 255});
                char hpLabel[32];
                snprintf(hpLabel, sizeof(hpLabel), "%d/%d", hs.hp, hs.maxHp);
                drawModernBar(infoX, infoY, bw, bh, hpPct, {35,35,45,255}, hpFill, hpLabel, WHITE, 8);
                infoY += bh + 6.f;
            }

            // Ultimate bar
            {
                float bw = infoW, bh = 10.f;
                uint8_t pct = hs.ultPct;
                if (pct == 255) {
                    float t = (float)GetTime();
                    unsigned char a = (unsigned char)(160 + 95 * sinf(t * 6.f));
                    Color glow = GOLD; glow.a = a;
                    drawModernBar(infoX, infoY, bw, bh, 1.f, {40,35,20,255}, glow, "ULTIMATE", BLACK, 8);
                } else {
                    float fillPct = pct / 100.f;
                    Color pwrColor = (pct >= 100) ? (Color){255, 200, 60, 255} : (Color){60, 100, 200, 255};
                    if (pct >= 100) {
                        float t = (float)GetTime();
                        unsigned char a = (unsigned char)(180 + 75 * sinf(t * 4.f));
                        pwrColor = GOLD; pwrColor.a = a;
                    }
                    char ultLabel[32];
                    snprintf(ultLabel, sizeof(ultLabel), "ULT %u%%", pct);
                    drawModernBar(infoX, infoY, bw, bh, fillPct, {35,35,45,255}, pwrColor, ultLabel, WHITE, 8);
                }
                infoY += bh + 8.f;
            }

            // Stats row
            {
                float as = hs.asRate_x10 / 10.f;
                char stats[64];
                snprintf(stats, sizeof(stats), "AD %d   ARM %d   AS %.1f", hs.ad, hs.arm, as);
                DrawText(stats, (int)infoX, (int)infoY, 9, {160, 160, 190, 255});
            }

            // Item slots at bottom of card
            {
                float slotSize = 20.f;
                float slotGap = 4.f;
                float sx = infoX;
                float sy = cy + cardH - slotSize - 8.f;
                for (int s = 0; s < 4; s++) {
                    float ssx = sx + s * (slotSize + slotGap);
                    uint8_t itemId = (s < 4) ? hs.items[s] : (uint8_t)0xFF;
                    Color slotBg = (itemId != 0xFF) ? Color{55, 55, 80, 255} : Color{28, 28, 42, 200};
                    DrawRectangleRounded({ssx, sy, slotSize, slotSize}, 0.25f, 4, slotBg);
                    DrawRectangleRoundedLines({ssx, sy, slotSize, slotSize}, 0.25f, 4, {80, 80, 110, 180});
                    if (itemId != 0xFF) {
                        // Small dot indicator for equipped item
                        DrawCircle((int)(ssx + slotSize - 4), (int)(sy + 4), 2, GOLD);
                    }
                }
            }
        }
    }
}

// ── drawSidePanels ───────────────────────────────────────────────────────────
void drawSidePanels(const GameSnapshot& snap, const PlayerInput& p1, const PlayerInput& p2) {
    (void)p1; (void)p2;
    loadTextures();
    (void)snap;

    float topY = 0;
    float panelH = g_layout.cardsY;

    static const char* abilityNames[] = { "Rally (+AD)", "Shield (+ARM)" };

    for (int player = 0; player < 2; player++) {
        float px = player == 0 ? g_layout.leftPanelX : g_layout.rightPanelX;
        float panelW = player == 0 ? g_layout.leftPanelW : g_layout.rightPanelW;
        Color pCol = player == 0 ? Color{80,150,255,255} : Color{255,100,80,255};
        Color pColDim = player == 0 ? Color{80,150,255,80} : Color{255,100,80,80};
        Color bgDark = {10, 10, 22, 255};
        Color bgMid  = {14, 14, 28, 255};

        // Panel background with subtle gradient
        DrawRectangle((int)px, (int)topY, (int)panelW, (int)panelH, bgDark);
        // Top accent bar
        DrawRectangle((int)px, (int)topY, (int)panelW, 3, pCol);
        // Bottom fade
        DrawRectangle((int)px, (int)(panelH - 40), (int)panelW, 40, {10,10,22,200});

        float cy = topY + 14.f;
        float pad = 10.f;

        // Player header badge
        {
            const char* label = player == 0 ? "JOGADOR 1" : "JOGADOR 2";
            int lw = MeasureText(label, 10);
            float badgeW = lw + 16.f;
            float badgeX = px + (panelW - badgeW) * 0.5f;
            DrawRectangleRounded({badgeX, cy, badgeW, 18.f}, 0.5f, 6, pCol);
            DrawText(label, (int)(badgeX + 8), (int)(cy + 3), 10, {0,0,0,255});
            cy += 26.f;
        }

        // Trainer portrait with glow ring
        uint8_t tId = snap.trainers[player].trainerId;
        float maxPs = 226.f;
        float ps = fminf(panelW - 2.f * pad, maxPs);
        float psX = px + (panelW - ps) * 0.5f;
        if (tId < (int)g_trainerDefs.size() && trainerCardTextures[tId].id != 0) {
            float cAspect = (float)trainerCardTextures[tId].width / trainerCardTextures[tId].height;
            float cH = ps;
            float cW = cH * cAspect;
            float cX = px + (panelW - cW) * 0.5f;
            DrawTexturePro(trainerCardTextures[tId],
                {0, 0, (float)trainerCardTextures[tId].width, (float)trainerCardTextures[tId].height},
                {cX, cy, cW, cH}, {0, 0}, 0.f, WHITE);
            DrawRectangleLinesEx({cX, cy, cW, cH}, 3, pColDim);
            cy += cH + 10.f;
        } else if (tId < (int)g_trainerDefs.size() && trainerTextures[tId].id != 0) {
            DrawCircle((int)(psX + ps * 0.5f), (int)(cy + ps * 0.5f), ps * 0.55f, pColDim);
            DrawCircle((int)(psX + ps * 0.5f), (int)(cy + ps * 0.5f), ps * 0.52f, bgDark);
            DrawTexturePro(trainerTextures[tId],
                {0, 0, (float)trainerTextures[tId].width, (float)trainerTextures[tId].height},
                {psX, cy, ps, ps}, {0, 0}, 0.f, WHITE);
            cy += ps + 10.f;
        } else {
            DrawCircle((int)(psX + ps * 0.5f), (int)(cy + ps * 0.5f), ps * 0.5f, {30,30,45,255});
            cy += ps + 10.f;
        }

        // Trainer name
        const char* tName = (tId < (int)g_trainerDefs.size()) ? g_trainerDefs[tId].name.c_str() : "???";
        int nw = MeasureText(tName, 13);
        DrawText(tName, (int)(px + (panelW - nw) * 0.5f), (int)cy, 13, WHITE);
        cy += 20.f;

        // Score & Gold as stat badges
        {
            float badgeH = 22.f;
            float gap2 = 6.f;
            float badgeW = (panelW - 2.f * pad - gap2) * 0.5f;

            // Score badge
            char scoreStr[32];
            snprintf(scoreStr, sizeof(scoreStr), "%d", snap.trainers[player].score);
            DrawRectangleRounded({px + pad, cy, badgeW, badgeH}, 0.4f, 4, {20,20,40,255});
            DrawRectangleRoundedLines({px + pad, cy, badgeW, badgeH}, 0.4f, 4, pColDim);
            
            if (texTrophy.id != 0) {
                DrawTexturePro(texTrophy, {0, 0, (float)texTrophy.width, (float)texTrophy.height}, {px + pad + 4, cy + 3, 16, 16}, {0,0}, 0, WHITE);
            }
            DrawText("SCORE", (int)(px + pad + 24), (int)(cy + 7), 8, {120,120,150,255});
            
            int sw2 = MeasureText(scoreStr, 11);
            DrawText(scoreStr, (int)(px + pad + badgeW - sw2 - 6), (int)(cy + 6), 11, WHITE);

            // Gold badge
            char goldStr[32];
            snprintf(goldStr, sizeof(goldStr), "%d", snap.trainers[player].gold);
            DrawRectangleRounded({px + pad + badgeW + gap2, cy, badgeW, badgeH}, 0.4f, 4, {20,20,40,255});
            DrawRectangleRoundedLines({px + pad + badgeW + gap2, cy, badgeW, badgeH}, 0.4f, 4, {180,150,40,120});
            
            if (texGold.id != 0) {
                DrawTexturePro(texGold, {0, 0, (float)texGold.width, (float)texGold.height}, {px + pad + badgeW + gap2 + 4, cy + 3, 16, 16}, {0,0}, 0, WHITE);
            }
            DrawText("GOLD", (int)(px + pad + badgeW + gap2 + 24), (int)(cy + 7), 8, {180,150,40,200});
            
            int gw = MeasureText(goldStr, 11);
            DrawText(goldStr, (int)(px + pad + badgeW + gap2 + badgeW - gw - 6), (int)(cy + 6), 11, GOLD);

            cy += badgeH + 12.f;
        }

        // Divider
        DrawLineEx({px + pad, cy}, {px + panelW - pad, cy}, 1, {40, 40, 60, 180});
        cy += 10.f;

        // Items section
        DrawText("ITENS", (int)(px + pad), (int)cy, 10, {130, 130, 160, 255});
        cy += 16.f;

        float slotSize = fminf(36.f, (panelW - 2.f * pad - 3.f * 4.f) / 4.f);
        float slotGap = 4.f;
        int itemCount = 0;
        for (int i = 0; i < snap.heroCount; i++) {
            if (snap.heroes[i].ownerId != (uint8_t)player) continue;
            for (int s = 0; s < 4; s++) {
                if (snap.heroes[i].items[s] != 0xFF) itemCount++;
            }
        }

        float slotRowY = cy;
        for (int s = 0; s < 4; s++) {
            float ssx = px + pad + s * (slotSize + slotGap);
            DrawRectangleRounded({ssx, slotRowY, slotSize, slotSize}, 0.2f, 4, {22, 22, 38, 255});
            DrawRectangleRoundedLines({ssx, slotRowY, slotSize, slotSize}, 0.2f, 4, {50, 50, 75, 200});
            if (itemCount > s) {
                // Filled slot indicator
                DrawCircle((int)(ssx + slotSize * 0.5f), (int)(slotRowY + slotSize * 0.5f), slotSize * 0.25f, GOLD);
            }
        }
        cy = slotRowY + slotSize + 10.f;

        // Divider
        DrawLineEx({px + pad, cy}, {px + panelW - pad, cy}, 1, {40, 40, 60, 180});
        cy += 10.f;

        // Power section
        DrawText("HABILIDADE", (int)(px + pad), (int)cy, 10, {130, 130, 160, 255});
        cy += 16.f;

        uint8_t abType = (tId < (int)g_trainerDefs.size()) ? g_trainerDefs[tId].abilityType : 0xFF;
        const char* abName = (abType < 2) ? abilityNames[abType] : "???";
        int abW = MeasureText(abName, 11);
        DrawText(abName, (int)(px + pad), (int)cy, 11, WHITE);
        cy += 18.f;

        // Ability status button
        {
            float btnW = panelW - 2.f * pad;
            float btnH = 24.f;
            float btnX = px + pad;
            if (snap.trainers[player].abilityReady) {
                float t = (float)GetTime();
                unsigned char alpha = (unsigned char)(180 + 75 * sinf(t * 5.f));
                Color glow = pCol; glow.a = alpha;
                DrawRectangleRounded({btnX, cy, btnW, btnH}, 0.5f, 6, glow);
                DrawRectangleRounded({btnX + 2, cy + 2, btnW - 4, btnH - 4}, 0.4f, 6, {10,10,22,255});
                const char* readyTxt = "PRONTO";
                int rw = MeasureText(readyTxt, 11);
                DrawText(readyTxt, (int)(btnX + (btnW - rw) * 0.5f), (int)(cy + 5), 11, GREEN);
            } else {
                DrawRectangleRounded({btnX, cy, btnW, btnH}, 0.5f, 6, {25, 25, 40, 255});
                DrawRectangleRoundedLines({btnX, cy, btnW, btnH}, 0.5f, 6, {50, 50, 70, 200});
                const char* waitTxt = "RECARGA";
                int ww = MeasureText(waitTxt, 11);
                DrawText(waitTxt, (int)(btnX + (btnW - ww) * 0.5f), (int)(cy + 5), 11, {120, 120, 140, 255});
            }
            cy += btnH + 10.f;
        }

        // Controls hint
        const char* ctrl = player == 0 ? "[Q] ATIVAR" : "[E] ATIVAR";
        int cw = MeasureText(ctrl, 9);
        DrawText(ctrl, (int)(px + (panelW - cw) * 0.5f), (int)cy, 9, {100, 100, 130, 255});
        cy += 20.f;

        // Divider
        DrawLineEx({px + pad, cy}, {px + panelW - pad, cy}, 1, {40, 40, 60, 180});
        cy += 10.f;

        // Combat Log section
        DrawText("LOG DE COMBATE", (int)(px + pad), (int)cy, 10, {130, 130, 160, 255});
        cy += 16.f;
        drawCombatLogs(player, px + pad, cy, panelW - 2 * pad);
    }

    // Floating ability indicator near the grid
    for (int player = 0; player < 2; player++) {
        float px = player == 0 ? g_layout.leftPanelX : g_layout.rightPanelX;
        float pw = player == 0 ? g_layout.leftPanelW : g_layout.rightPanelW;
        float by = g_layout.gridY + g_layout.gridH + 12.f;

        if (snap.trainers[player].abilityReady) {
            const char* txt = player == 0 ? "[Q] PRONTO" : "[E] PRONTO";
            int tw = MeasureText(txt, 10);
            float tx = px + (pw - tw) * 0.5f;
            Color pCol = player == 0 ? Color{80,150,255,255} : Color{255,100,80,255};
            DrawRectangleRounded({tx - 4, by - 2, (float)(tw + 8), 16.f}, 0.5f, 4, pCol);
            DrawText(txt, (int)tx, (int)by, 10, {0,0,0,255});
        }
    }
}
void drawPlacementCursors(const GameSnapshot& snap,
                          const PlayerInput& p1, const PlayerInput& p2)
{
    // Helper: find hero info for a player by moveHeroIdx
    auto findHeroInfo = [&](int pid, int moveIdx, uint8_t& outArch, bool& outPlaced, const char*& outName) {
        outArch = 0xFF;
        outPlaced = false;
        outName = "???";
        int localIdx = 0;
        for (int i = 0; i < snap.heroCount; i++) {
            if (snap.heroes[i].ownerId != (uint8_t)pid) continue;
            if (localIdx == moveIdx) {
                outArch = snap.heroes[i].archetype;
                // Check if already placed (not at default position)
                outPlaced = (snap.heroes[i].x != (pid == 0 ? 1 : 6)) || (snap.heroes[i].y != (2 + localIdx));
                // Find name from g_heroDefs
                uint8_t hdi = snap.heroes[i].heroDefIndex;
                if (hdi < (int)g_heroDefs.size()) outName = g_heroDefs[hdi].name.c_str();
                return;
            }
            localIdx++;
        }
    };

    // Draw valid cells for each player
    auto drawValidCells = [&](int pid, int moveIdx, Color pCol) {
        uint8_t arch = 0xFF;
        bool placed = false;
        const char* name = "???";
        findHeroInfo(pid, moveIdx, arch, placed, name);
        if (arch == 0xFF) return;
        bool isLeft = (pid == 0);
        // Valid cells highlight
        for (int r = 0; r < GRID_ROWS; r++) {
            for (int c = 0; c < GRID_COLS; c++) {
                if (isValidDeployCell(c, r, arch, isLeft)) {
                    Color fill = isLeft ? Color{80, 150, 255, 90} : Color{255, 100, 80, 90};
                    Color border = isLeft ? Color{80, 150, 255, 200} : Color{255, 100, 80, 200};
                    DrawRectangleRec(cellRect(c, r), fill);
                    DrawRectangleLinesEx(cellRect(c, r), 2, border);
                }
            }
        }
    };

    drawValidCells(0, p1.moveHeroIdx, kP1Color);
    drawValidCells(1, p2.moveHeroIdx, kP2Color);

    // P1 cursor
    {
        float alpha = 0.3f + 0.2f * sinf((float)GetTime() * 4.f);
        Color c = kP1Color; c.a = (unsigned char)(255.f * alpha);
        Rectangle r = cellRect(p1.cursorX, p1.cursorY);
        DrawRectangleRec(r, c);
        DrawRectangleLinesEx(r, 3, kP1Color);
        // Hero number in cursor
        char num[4]; snprintf(num, sizeof(num), "%d", p1.moveHeroIdx + 1);
        DrawText(num, (int)(r.x + (r.width - MeasureText(num, 14))/2), (int)(r.y + (r.height - 14)/2), 14, WHITE);
    }

    // P2 cursor
    {
        float alpha = 0.3f + 0.2f * sinf((float)GetTime() * 4.f + 1.f);
        Color c = kP2Color; c.a = (unsigned char)(255.f * alpha);
        Rectangle r = cellRect(p2.cursorX, p2.cursorY);
        DrawRectangleRec(r, c);
        DrawRectangleLinesEx(r, 3, kP2Color);
        char num[4]; snprintf(num, sizeof(num), "%d", p2.moveHeroIdx + 1);
        DrawText(num, (int)(r.x + (r.width - MeasureText(num, 14))/2), (int)(r.y + (r.height - 14)/2), 14, WHITE);
    }

    // Labels at bottom
    uint8_t arch1 = 0xFF; bool placed1 = false; const char* name1 = "???";
    findHeroInfo(0, p1.moveHeroIdx, arch1, placed1, name1);
    char lbl1[128];
    snprintf(lbl1, sizeof(lbl1), "P1: [%d/3] %s  |  1/2/3 heroi  |  WASD mover  |  Space posicionar", p1.moveHeroIdx + 1, name1);
    int w1 = MeasureText(lbl1, 18);
    DrawRectangle(0, (int)(g_layout.screenH - 34), w1 + 16, 30, {0,0,0,200});
    DrawText(lbl1, 8, (int)(g_layout.screenH - 30), 18, kP1Color);

    uint8_t arch2 = 0xFF; bool placed2 = false; const char* name2 = "???";
    findHeroInfo(1, p2.moveHeroIdx, arch2, placed2, name2);
    char lbl2[128];
    snprintf(lbl2, sizeof(lbl2), "P2: [%d/3] %s  |  Numpad 1/2/3 heroi  |  Setas mover  |  Enter posicionar", p2.moveHeroIdx + 1, name2);
    int w2 = MeasureText(lbl2, 18);
    DrawRectangle((int)(g_layout.screenW - w2 - 16), (int)(g_layout.screenH - 34), w2 + 16, 30, {0,0,0,200});
    DrawText(lbl2, (int)(g_layout.screenW - w2 - 8), (int)(g_layout.screenH - 30), 18, kP2Color);
}

// ── Combat Log Implementation ────────────────────────────────────────────────
static std::vector<LogEntry> g_logs[2];

void addCombatLog(int side, const char* text, Color color) {
    if (side < 0 || side > 1) return;
    LogEntry e;
    strncpy(e.text, text, sizeof(e.text) - 1);
    e.text[sizeof(e.text) - 1] = '\0';
    e.color = color;
    e.timer = 8.0f;
    g_logs[side].insert(g_logs[side].begin(), e);
    if (g_logs[side].size() > 8) g_logs[side].pop_back();
}

void updateCombatLogs(float dt) {
    for (int i = 0; i < 2; i++) {
        for (auto it = g_logs[i].begin(); it != g_logs[i].end(); ) {
            it->timer -= dt;
            if (it->timer <= 0) {
                // it = g_logs[i].erase(it); // Or keep them until next round?
                // The user didn't specify auto-clear, but it's good for long battles.
                // Let's keep them for now, or just fade.
                it++;
            } else {
                it++;
            }
        }
    }
}

void drawCombatLogs(int side, float x, float y, float w) {
    (void)w;
    float cy = y;
    for (const auto& e : g_logs[side]) {
        Color c = e.color;
        if (e.timer < 1.0f && e.timer > 0) {
            c.a = (unsigned char)(e.timer * 255);
        } else if (e.timer <= 0) {
            c.a = 50; // Very faded
        }
        DrawText(e.text, (int)x, (int)cy, 10, c);
        cy += 14;
    }
}

void clearCombatLogs() {
    g_logs[0].clear();
    g_logs[1].clear();
}
