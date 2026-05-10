#include "renderer.h"
#include "../../include/game_defs.h"
#include <stdio.h>
#include <algorithm>
#include <math.h>

const float GX = 218.f, GY = 92.f, GW = 500.f, GH = 500.f;
const float CELLW = GW / GRID_COLS;
const float CELLH = GH / GRID_ROWS;

static const Color kPlayerColor[2] = { BLUE, RED };
static Texture2D trainerTextures[N_TRAINERS];
static Texture2D heroTextures[N_HEROES]; // mapping heroDefIndex to textures
static bool texturesLoaded = false;

void loadTextures() {
    if (texturesLoaded) return;

    for (int i = 0; i < N_TRAINERS; i++) {
        if (TRAINER_DEFS[i].portraitPath[0] != '\0')
            trainerTextures[i] = LoadTexture(TRAINER_DEFS[i].portraitPath);
    }

    for (int i = 0; i < N_HEROES; i++) {
        heroTextures[i] = LoadTexture(HERO_DEFS[i].assetPath);
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
    
    // Vertical dividing line (Horizontal Arena: Left vs Right)
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

void drawHero(const HeroNetState& hs, Vector2 ctr, int myId, bool dragging) {
    float r = CELLW * 0.42f;
    Color pCol = kPlayerColor[hs.ownerId];
    if (dragging) pCol.a = 120;

    // 1. Colored circle as background
    DrawCircleV(ctr, r, pCol);

    // 2. Draw hero sprite clipped inside the circle area
    uint8_t texIdx = (hs.heroDefIndex < N_HEROES) ? hs.heroDefIndex : hs.archetype;
    Texture2D& tex = heroTextures[texIdx];
    if (tex.id != 0) {
        // Scale sprite to fill the circle diameter (2*r x 2*r)
        float diameter = r * 2.0f;
        float scaleX = diameter / tex.width;
        float scaleY = diameter / tex.height;
        float scale  = (scaleX < scaleY) ? scaleX : scaleY;
        float sw = tex.width  * scale;
        float sh = tex.height * scale;

        // Use scissor to clip sprite to the circle's bounding square
        int sx = (int)(ctr.x - r);
        int sy = (int)(ctr.y - r);
        int sd = (int)(diameter);
        BeginScissorMode(sx, sy, sd, sd);
        DrawTexturePro(
            tex,
            { 0, 0, (float)tex.width, (float)tex.height },
            { ctr.x - sw * 0.5f, ctr.y - sh * 0.5f, sw, sh },
            { 0, 0 }, 0.f, WHITE
        );
        EndScissorMode();
    } else {
        // Fallback: archetype label
        const char* archNames[] = {"TNK", "FGT", "MAG", "ASN", "SUP"};
        DrawText(archNames[hs.archetype], (int)ctr.x - 12, (int)ctr.y - 6, 10, WHITE);
    }

    // 3. Circle outline drawn ON TOP to mask sprite corners
    Color rimColor = (hs.ownerId == (uint8_t)myId) ? WHITE : LIGHTGRAY;
    DrawCircleLinesV(ctr, r, rimColor);
    // Thicker colored rim for visual quality
    DrawCircleLinesV(ctr, r - 1, pCol);

    // 4. Ultimate glow
    if (hs.ultActive) {
        DrawCircleLinesV(ctr, r + 3, GOLD);
        DrawCircleLinesV(ctr, r + 6, { 255, 215, 0, 120 });
    }

    // 5. HP bar above the circle
    float bw = CELLW * 0.85f, bh = 6.f;
    float bx = ctr.x - bw * 0.5f, by = ctr.y - r - 14.f;
    float pct = (hs.maxHp > 0) ? (float)hs.hp / hs.maxHp : 0.f;
    DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, DARKGRAY);
    DrawRectangle((int)bx, (int)by, (int)(bw * pct), (int)bh,
                  pct > 0.5f ? GREEN : (pct > 0.25f ? YELLOW : RED));
    DrawRectangleLinesEx({ bx, by, bw, bh }, 1, { 255, 255, 255, 80 });
}

void drawHUD(const GameSnapshot& snap, int myId) {
    loadTextures(); // ensures textures are loaded once

    // Trainer Portraits in top corners
    float pSize = 80.f;
    uint8_t tId0 = snap.trainers[0].trainerId;
    uint8_t tId1 = snap.trainers[1].trainerId;
    if (tId0 < N_TRAINERS && trainerTextures[tId0].id != 0) {
        DrawTexturePro(trainerTextures[tId0], {0,0,(float)trainerTextures[tId0].width, (float)trainerTextures[tId0].height}, {10,10,pSize,pSize}, {0,0}, 0.f, WHITE);
    }
    if (tId1 < N_TRAINERS && trainerTextures[tId1].id != 0) {
        DrawTexturePro(trainerTextures[tId1], {0,0,(float)trainerTextures[tId1].width, (float)trainerTextures[tId1].height}, {936 - pSize - 10,10,pSize,pSize}, {0,0}, 0.f, WHITE);
    }

    // Scores
    char s0[32], s1[32];
    snprintf(s0, sizeof(s0), "P0: %d", snap.trainers[0].score);
    snprintf(s1, sizeof(s1), "P1: %d", snap.trainers[1].score);
    DrawText(s0, 100, 20, 24, BLUE);
    DrawText(s1, 936 - MeasureText(s1, 24) - 100, 20, 24, RED);

    if (snap.phase == PHASE_POSITIONING || snap.phase == PHASE_BATTLE) {
        char t[16]; snprintf(t, sizeof(t), "%ds", snap.timer);
        DrawText(t, 468 - MeasureText(t, 28)/2, 20, 28, GOLD);
    }

    if (snap.trainers[myId].abilityReady) {
        const char* msg = "Q: ACTIVAR PODER TREINADOR";
        DrawText(msg, 468 - MeasureText(msg, 20)/2, 640, 20, YELLOW);
    }

    if (snap.phase == PHASE_POSITIONING) {
        const char* hint = (myId == 0) ? "ESQUERDA (Cols 0-3)" : "DIREITA (Cols 4-7)";
        DrawText(hint, 468 - MeasureText(hint, 20)/2, 610, 20, SKYBLUE);
    }
}

void drawOverlays(const GameSnapshot& snap, int myId) {
    if (snap.phase == PHASE_WAITING) {
        DrawRectangle(0, 0, 936, 684, {0, 0, 0, 150});
        const char* msg = "Aguardando Segundo Treinador...";
        DrawText(msg, 468 - MeasureText(msg, 30)/2, 342, 30, WHITE);
    }
    if (snap.phase == PHASE_ROUND_END) {
        DrawRectangle(0, 0, 936, 684, {0, 0, 0, 100});
        const char* msg = (snap.roundWinner == (uint8_t)myId) ? "PONTO PARA VOCÊ!" : (snap.roundWinner == 0xFF ? "EMPATE!" : "PONTO PARA O OPONENTE");
        DrawText(msg, 468 - MeasureText(msg, 40)/2, 300, 40, (snap.roundWinner == (uint8_t)myId) ? GREEN : RED);
    }
    if (snap.phase == PHASE_MATCH_END) {
        DrawRectangle(0, 0, 936, 684, {0, 0, 0, 200});
        const char* res = (snap.matchWinner == (uint8_t)myId) ? "VITÓRIA!" : "DERROTA";
        DrawText(res, 468 - MeasureText(res, 60)/2, 300, 60, GOLD);
        DrawText("Feche o jogo para reiniciar", 468 - MeasureText("Feche o jogo para reiniciar", 20)/2, 400, 20, LIGHTGRAY);
    }
}

void drawVSScreen(const GameSnapshot& snap, int myId) {
    loadTextures();

    // Background
    ClearBackground({12, 12, 26, 255});
    DrawRectangle(0, 0, 936, 684, {0, 0, 0, 80});

    // Colors for each player
    static const Color kTint[2] = { {80, 160, 230, 255}, {230, 80, 80, 255} };

    // ── Helper to draw one side ──
    auto drawSide = [&](int side, float cx, float tintColor) {
        // Trainer portrait
        uint8_t tId = snap.trainers[side].trainerId;
        if (tId < N_TRAINERS) {
            const TrainerDefEntry& td = TRAINER_DEFS[tId];

            // Portrait
            float pSize = 140.f;
            float px = cx - pSize * 0.5f;
            float py = 60.f;
            if (trainerTextures[tId].id != 0) {
                DrawTexturePro(trainerTextures[tId],
                    {0, 0, (float)trainerTextures[tId].width, (float)trainerTextures[tId].height},
                    {px, py, pSize, pSize}, {0, 0}, 0.f, WHITE);
            } else {
                DrawRectangle((int)px, (int)py, (int)pSize, (int)pSize, kTint[side]);
            }

            // Trainer name
            int nw = MeasureText(td.name, 22);
            DrawText(td.name, (int)(cx - nw * 0.5f), (int)(py + pSize + 10), 22, WHITE);

            // Discipline
            int dw = MeasureText(td.discipline, 14);
            DrawText(td.discipline, (int)(cx - dw * 0.5f), (int)(py + pSize + 36), 14, LIGHTGRAY);
        }

        // Collect this side's heroes from snapshot
        uint8_t heroDefIdxs[3];
        int heroCount = 0;
        for (int i = 0; i < snap.heroCount && heroCount < 3; i++) {
            if (snap.heroes[i].ownerId == (uint8_t)side) {
                heroDefIdxs[heroCount++] = snap.heroes[i].heroDefIndex;
            }
        }

        // Hero cards
        float cardW = 110.f, cardH = 140.f;
        float gap = 16.f;
        float totalW = heroCount * cardW + (heroCount - 1) * gap;
        float startX = cx - totalW * 0.5f;
        float hy = 280.f;

        for (int h = 0; h < heroCount; h++) {
            uint8_t hDefIdx = heroDefIdxs[h];
            if (hDefIdx >= N_HEROES) continue;
            const HeroDefEntry& hd = HERO_DEFS[hDefIdx];

            float hx = startX + h * (cardW + gap);

            // Card background
            DrawRectangleRounded({hx, hy, cardW, cardH}, 0.06f, 6, {30, 30, 50, 255});
            DrawRectangleRoundedLines({hx, hy, cardW, cardH}, 0.06f, 6, kTint[side]);

            // Hero portrait
            float ps = 64.f;
            float pxx = hx + (cardW - ps) * 0.5f;
            float pyy = hy + 10.f;
            if (heroTextures[hDefIdx].id != 0) {
                DrawTexturePro(heroTextures[hDefIdx],
                    {0, 0, (float)heroTextures[hDefIdx].width, (float)heroTextures[hDefIdx].height},
                    {pxx, pyy, ps, ps}, {0, 0}, 0.f, WHITE);
            } else {
                DrawRectangle((int)pxx, (int)pyy, (int)ps, (int)ps, kTint[side]);
            }

            // Hero name
            int nn = MeasureText(hd.name, 11);
            DrawText(hd.name, (int)(hx + (cardW - nn) * 0.5f), (int)(pyy + ps + 6), 11, WHITE);

            // Class name
            static const char* archNames[] = {"Tank", "Fighter", "Mage", "Assassin", "Support"};
            static const Color archColors[] = {
                {80,130,220,255}, {220,80,80,255}, {150,80,220,255},
                {70,70,70,255},   {80,200,130,255}
            };
            const char* cls = archNames[hd.archetype];
            int cw = MeasureText(cls, 10);
            DrawText(cls, (int)(hx + (cardW - cw) * 0.5f), (int)(pyy + ps + 20), 10, archColors[hd.archetype]);

            // Stats
            char stats[48];
            snprintf(stats, sizeof(stats), "HP:%d AD:%d ARM:%d", hd.hp, hd.ad, hd.arm);
            int sw = MeasureText(stats, 10);
            DrawText(stats, (int)(hx + (cardW - sw) * 0.5f), (int)(pyy + ps + 34), 10, GRAY);
        }
    };

    // Draw left side (Player 0)
    drawSide(0, 234.f, 0);

    // Draw right side (Player 1)
    drawSide(1, 702.f, 1);

    // "VS" text in center
    DrawText("VS", 468 - MeasureText("VS", 72)/2, 150, 72, GOLD);

    // Countdown timer at bottom
    if (snap.phase == PHASE_VS_INTRO) {
        char timerStr[16];
        snprintf(timerStr, sizeof(timerStr), "%d", snap.timer);
        DrawText(timerStr, 468 - MeasureText(timerStr, 48)/2, 580, 48, {255, 255, 255, 180});
    }

    // "Preparando arena..." hint
    const char* hint = "Preparando arena...";
    DrawText(hint, 468 - MeasureText(hint, 18)/2, 640, 18, {255, 255, 255, 120});
}

void unloadTextures() {
    if (!texturesLoaded) return;
    for (int i = 0; i < N_TRAINERS; i++) {
        if (trainerTextures[i].id != 0) UnloadTexture(trainerTextures[i]);
    }
    for (int i = 0; i < N_HEROES; i++) {
        if (heroTextures[i].id != 0) UnloadTexture(heroTextures[i]);
    }
    texturesLoaded = false;
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
        ft.pos.y -= 30.f * dt;  // sobe
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
//  SELECTION SCREEN RENDERING
// ═════════════════════════════════════════════════════════════════════════════

// Archetype display helpers
static const char*  ARCH_NAMES[]   = {"Tank","Fighter","Mage","Assassin","Support"};
static const Color  ARCH_COLORS[]  = {
    {80,130,220,255}, {220,80,80,255}, {150,80,220,255},
    {70,70,70,255},   {80,200,130,255}
};

// Per-selection portrait textures (separate from battle heroTextures[])
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

// ── drawTrainerSelect ─────────────────────────────────────────────────────────
// Renders 1xN grid of trainer cards with cursor highlight.
void drawTrainerSelect(const TrainerDef* trainers, int nT,
                       int cursor, int /*selectedIdx*/, int myId) {
    static const Color BG   = {12,12,26,255};
    static const Color CARD = {30,30,58,255};
    DrawRectangle(0, 0, 936, 684, BG);

    // Title
    const char* title = "BATTLE-CIn";
    DrawText(title, (936 - MeasureText(title, 36))/2, 18, 36, WHITE);
    const char* sub = "Selecione seu Treinador (Professor)";
    DrawText(sub,   (936 - MeasureText(sub, 18))/2, 60, 18, {180,180,200,255});

    // Player label
    char plbl[24]; snprintf(plbl, sizeof(plbl), "Instancia %d (P%d)", myId, myId+1);
    Color pidColor = (myId == 0) ? Color{255,100,100,255} : Color{100,150,255,255};
    DrawText(plbl, (936 - MeasureText(plbl,16))/2, 90, 16, pidColor);

    // Card layout
    const float CW = 188.f, CH = 250.f, PAD = 16.f;
    float totalW = nT * CW + (nT-1) * PAD;
    float startX = (936.f - totalW) / 2.f;
    float startY = 118.f;

    for (int i = 0; i < nT; i++) {
        float x = startX + i * (CW + PAD);
        float y = startY;
        bool  hov = (i == cursor);

        // Card background
        Color bg = hov ? Color{42,42,78,255} : CARD;
        DrawRectangleRounded({x, y, CW, CH}, 0.08f, 6, bg);
        DrawRectangleRoundedLines({x, y, CW, CH}, 0.08f, 6, {60,60,90,255});

        // Portrait area
        float ps = 100.f, px = x + (CW-ps)/2.f, py = y + 14.f;
        if (selTrainerTex && selTrainerTex[i].id) {
            DrawTexturePro(selTrainerTex[i],
                {0,0,(float)selTrainerTex[i].width,(float)selTrainerTex[i].height},
                {px, py, ps, ps}, {}, 0.f, WHITE);
        } else {
            DrawRectangleRounded({px,py,ps,ps}, 0.2f, 6, trainers[i].color);
        }
        DrawRectangleRoundedLines({px,py,ps,ps}, 0.2f, 6, trainers[i].color);

        // Name
        int nw = MeasureText(trainers[i].name, 14);
        DrawText(trainers[i].name, (int)(x+(CW-nw)/2), (int)(py+ps+10), 14, WHITE);

        // Discipline
        int dw = MeasureText(trainers[i].discipline, 11);
        DrawText(trainers[i].discipline, (int)(x+(CW-dw)/2), (int)(py+ps+28), 11, {160,160,190,255});

        // Ability badge
        char abuf[32]; snprintf(abuf, sizeof(abuf), "Poder: %s", trainers[i].abilityName);
        int aw = MeasureText(abuf, 11);
        DrawText(abuf, (int)(x+(CW-aw)/2), (int)(py+ps+48), 11, trainers[i].color);

        // Cursor border (pulses)
        if (hov) {
            float t = (float)GetTime();
            unsigned char alpha = (unsigned char)(180 + 75 * sinf(t * 4.f));
            Color bc = pidColor; bc.a = alpha;
            DrawRectangleLinesEx({x-4,y-4,CW+8,CH+8}, 3, bc);
        }
    }

    // Controls
    float cy = startY + CH + 24.f;
    const char* ctrl = (myId == 0)
        ? "A / D = Mover    |    SPACE = Confirmar"
        : "< / > = Mover    |    ENTER = Confirmar";
    DrawText(ctrl, (936 - MeasureText(ctrl,16))/2, (int)cy, 16, {160,160,190,255});
}

// ── drawHeroSelect ────────────────────────────────────────────────────────────
// Renders a 2x5 hero grid. Left sidebar shows trainer info + picks progress.
void drawHeroSelect(const TrainerDef& trainer,
                    const HeroDef* heroes, int nH,
                    int gridCols,
                    int cursor,
                    const std::vector<int>& picks,
                    int myId) {
    static const Color BG   = {12,12,26,255};
    static const Color CARD = {30,30,58,255};
    DrawRectangle(0, 0, 936, 684, BG);

    // ── Left sidebar (trainer info + picks) ──────────────────────────────────
    const float SIDEBAR_W = 200.f;
    DrawRectangle(0, 0, (int)SIDEBAR_W, 684, {22,22,46,240});

    int tw = MeasureText(trainer.name, 13);
    DrawText(trainer.name, (int)((SIDEBAR_W-tw)/2), 16, 13, WHITE);
    int dw = MeasureText(trainer.discipline, 11);
    DrawText(trainer.discipline, (int)((SIDEBAR_W-dw)/2), 34, 11, {160,160,190,255});

    // Picks list
    char prog[24]; snprintf(prog, sizeof(prog), "Herois: %d / 3", (int)picks.size());
    int pw = MeasureText(prog, 13);
    DrawText(prog, (int)((SIDEBAR_W-pw)/2), 68, 13,
             picks.size() == 3 ? GREEN : Color{220,180,50,255});

    for (int p = 0; p < (int)picks.size(); p++) {
        int    idx  = picks[p];
        float  py   = 92.f + p * 72.f;
        uint8_t arc = heroes[idx].archetype;
        Color  ac   = ARCH_COLORS[arc];
        DrawRectangleRounded({8, py, SIDEBAR_W-16, 64}, 0.1f, 4, CARD);
        DrawRectangleRounded({8, py, 4, 64}, 0.1f, 4, ac);   // accent strip
        // mini portrait
        if (selHeroTex && selHeroTex[idx].id)
            DrawTexturePro(selHeroTex[idx],
                {0,0,(float)selHeroTex[idx].width,(float)selHeroTex[idx].height},
                {14, py+4, 54, 56}, {}, 0.f, WHITE);
        else
            DrawRectangleRounded({14,py+4,54,56}, 0.1f, 4, ac);
        int nw2 = MeasureText(heroes[idx].name, 10);
        DrawText(heroes[idx].name, (int)(72), (int)(py+8), 10, WHITE);
        DrawText(ARCH_NAMES[arc], 72, (int)(py+24), 10, ac);
        char sb[24]; snprintf(sb,sizeof(sb),"HP:%d AD:%d",heroes[idx].hp,heroes[idx].ad);
        DrawText(sb, 72, (int)(py+40), 10, {160,160,190,255});
        (void)nw2;
    }

    // ── Hero grid ─────────────────────────────────────────────────────────────
    const float CW = 140.f, CH = 178.f, PAD = 12.f;
    int   gridRows = (nH + gridCols - 1) / gridCols;
    float gridW    = gridCols * CW + (gridCols-1) * PAD;
    float availW   = 936.f - SIDEBAR_W;
    float startX   = SIDEBAR_W + (availW - gridW) / 2.f;
    float startY   = 60.f;

    // Title
    const char* title = "Escolha 3 Herois";
    DrawText(title, (int)(SIDEBAR_W + (availW - MeasureText(title,18))/2), 18, 18, WHITE);

    Color pidColor = (myId==0) ? Color{255,100,100,255} : Color{100,150,255,255};

    for (int i = 0; i < nH; i++) {
        int   row  = i / gridCols,  col = i % gridCols;
        float x    = startX + col * (CW + PAD);
        float y    = startY + row * (CH + PAD);
        bool  hov  = (i == cursor);
        bool  picked = (std::find(picks.begin(), picks.end(), i) != picks.end());
        uint8_t arc = heroes[i].archetype;
        Color   ac  = ARCH_COLORS[arc];

        // Background
        Color bg = picked ? Color{40,50,70,255} : (hov ? Color{42,42,78,255} : CARD);
        DrawRectangleRounded({x,y,CW,CH}, 0.08f, 6, bg);
        DrawRectangleRounded({x,y,CW,4},  0.1f,  4, ac);  // accent bar

        // Portrait
        float imgH = CH * 0.48f;
        if (selHeroTex && selHeroTex[i].id)
            DrawTexturePro(selHeroTex[i],
                {0,0,(float)selHeroTex[i].width,(float)selHeroTex[i].height},
                {x+6, y+8, CW-12, imgH}, {}, 0.f, WHITE);
        else
            DrawRectangleRounded({x+6,y+8,CW-12,imgH}, 0.1f, 4, ac);

        // Name
        float ty = y + 8 + imgH + 5;
        int   nw = MeasureText(heroes[i].name, 10);
        DrawText(heroes[i].name, (int)(x+(CW-nw)/2), (int)ty, 10, WHITE); ty += 14;

        // Class badge
        int bw = MeasureText(ARCH_NAMES[arc],10);
        DrawRectangleRounded({x+(CW-bw-10)/2, ty, (float)(bw+10), 16}, 0.4f, 4, ac);
        DrawText(ARCH_NAMES[arc], (int)(x+(CW-bw)/2), (int)(ty+3), 10, WHITE); ty += 20;

        // Stats
        char sb[32]; snprintf(sb,sizeof(sb),"HP:%d AD:%d ARM:%d",heroes[i].hp,heroes[i].ad,heroes[i].arm);
        int sw = MeasureText(sb,9);
        DrawText(sb,(int)(x+(CW-sw)/2),(int)ty,9,{160,160,190,255});

        // Cursor border
        if (hov)
            DrawRectangleLinesEx({x-3,y-3,CW+6,CH+6}, 3, pidColor);
        // Picked indicator
        if (picked) {
            DrawRectangleLinesEx({x,y,CW,CH}, 2, ac);
            DrawText("✓", (int)(x+CW-16), (int)(y+4), 14, GREEN);
        }
    }

    // Controls
    float cy = startY + gridRows * (CH + PAD) + 8;
    const char* ctrl = "A/W/S/D = Mover  |  SPACE = Selecionar/Desmarcar  |  Q = Desfazer";
    DrawText(ctrl, (int)(SIDEBAR_W+(availW-MeasureText(ctrl,13))/2), (int)cy, 13, {160,160,190,255});

    if ((int)picks.size() == 3) {
        const char* ok = "3 herois prontos! Iniciando...";
        DrawText(ok, (int)(SIDEBAR_W+(availW-MeasureText(ok,16))/2), (int)(cy+20), 16, GREEN);
    }
}
