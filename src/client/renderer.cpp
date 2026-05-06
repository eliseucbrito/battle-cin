#include "renderer.h"
#include <stdio.h>

const float GX = 218.f, GY = 92.f, GW = 500.f, GH = 500.f;
const float CELLW = GW / GRID_COLS;
const float CELLH = GH / GRID_ROWS;

static const Color kPlayerColor[2] = { BLUE, RED };
static Texture2D trainerTextures[2];
static Texture2D heroTextures[5]; // mapping archetypes to textures
static bool texturesLoaded = false;

void loadTextures() {
    if (texturesLoaded) return;
    trainerTextures[0] = LoadTexture("assets/trainer0.png");
    trainerTextures[1] = LoadTexture("assets/trainer1.png");

    heroTextures[ARCHETYPE_TANK]     = LoadTexture("assets/heroes/O_Construto_de_Busca.png");
    heroTextures[ARCHETYPE_FIGHTER]  = LoadTexture("assets/heroes/O_Filósofo_do_Dilema.png");
    heroTextures[ARCHETYPE_MAGE]     = LoadTexture("assets/heroes/O_Mestre_Parser.png");
    heroTextures[ARCHETYPE_ASSASSIN] = LoadTexture("assets/heroes/O_Cientista_Polarizado.png");
    heroTextures[ARCHETYPE_SUPPORT]  = LoadTexture("assets/heroes/O_Chip-Mestre.png");
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
    Texture2D& tex = heroTextures[hs.archetype];
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
    if (trainerTextures[0].id != 0) {
        DrawTexturePro(trainerTextures[0], {0,0,(float)trainerTextures[0].width, (float)trainerTextures[0].height}, {10,10,pSize,pSize}, {0,0}, 0.f, WHITE);
    }
    if (trainerTextures[1].id != 0) {
        DrawTexturePro(trainerTextures[1], {0,0,(float)trainerTextures[1].width, (float)trainerTextures[1].height}, {936 - pSize - 10,10,pSize,pSize}, {0,0}, 0.f, WHITE);
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

void unloadTextures() {
    if (!texturesLoaded) return;
    UnloadTexture(trainerTextures[0]);
    UnloadTexture(trainerTextures[1]);
    for (int i = 0; i < 5; i++) {
        if (heroTextures[i].id != 0) UnloadTexture(heroTextures[i]);
    }
    texturesLoaded = false;
}
