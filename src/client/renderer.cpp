#include "renderer.h"
#include "../../include/game_defs.h"
#include <stdio.h>
#include <algorithm>
#include <math.h>

const float GX = 218.f, GY = 92.f, GW = 500.f, GH = 500.f;
const float CELLW = GW / GRID_COLS;
const float CELLH = GH / GRID_ROWS;

static const Color kPlayerColor[2] = { BLUE, RED };
static const Color kP1Color = {80, 150, 255, 255};
static const Color kP2Color = {255, 100, 80, 255};
static Texture2D trainerTextures[N_TRAINERS];
static Texture2D heroTextures[N_HEROES];
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

    uint8_t texIdx = (hs.heroDefIndex < N_HEROES) ? hs.heroDefIndex : hs.archetype;
    Texture2D& tex = heroTextures[texIdx];
    if (tex.id != 0) {
        float wantedH = CELLH * 0.72f;
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
        DrawCircleLinesV(heroCtr, r + 3, GOLD);
        DrawCircleLinesV(heroCtr, r + 6, { 255, 215, 0, 120 });
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

    float pSize = 80.f;
    uint8_t tId0 = snap.trainers[0].trainerId;
    uint8_t tId1 = snap.trainers[1].trainerId;
    if (tId0 < N_TRAINERS && trainerTextures[tId0].id != 0) {
        DrawTexturePro(trainerTextures[tId0], {0,0,(float)trainerTextures[tId0].width, (float)trainerTextures[tId0].height}, {10,10,pSize,pSize}, {0,0}, 0.f, WHITE);
    }
    if (tId1 < N_TRAINERS && trainerTextures[tId1].id != 0) {
        DrawTexturePro(trainerTextures[tId1], {0,0,(float)trainerTextures[tId1].width, (float)trainerTextures[tId1].height}, {936 - pSize - 10,10,pSize,pSize}, {0,0}, 0.f, WHITE);
    }

    char s0[32], s1[32];
    snprintf(s0, sizeof(s0), "P0: %d", snap.trainers[0].score);
    snprintf(s1, sizeof(s1), "P1: %d", snap.trainers[1].score);
    DrawText(s0, 100, 20, 24, BLUE);
    DrawText(s1, 936 - MeasureText(s1, 24) - 100, 20, 24, RED);

    if (snap.phase == PHASE_POSITIONING || snap.phase == PHASE_BATTLE) {
        char t[16]; snprintf(t, sizeof(t), "%ds", snap.timer);
        DrawText(t, 468 - MeasureText(t, 28)/2, 20, 28, GOLD);
    }

    if (snap.trainers[0].abilityReady || snap.trainers[1].abilityReady) {
        const char* msg = "P1: Q  |  P2: E  = Poder do Treinador";
        DrawText(msg, 468 - MeasureText(msg, 20)/2, 640, 20, YELLOW);
    }

    if (snap.phase == PHASE_POSITIONING) {
        const char* hint = "P1: 1/2/3 heroi  |  WASD mover  |  Space posicionar     |     P2: Numpad 1/2/3 heroi  |  Setas mover  |  Enter posicionar";
        int hw = MeasureText(hint, 18);
        DrawRectangle(468 - hw/2 - 6, 604, hw + 12, 30, {0,0,0,200});
        DrawText(hint, 468 - hw/2, 610, 18, SKYBLUE);
    }
}

void drawOverlays(const GameSnapshot& snap, int myId) {
    (void)myId;
    if (snap.phase == PHASE_ROUND_END) {
        DrawRectangle(0, 0, 936, 684, {0, 0, 0, 100});
        const char* msg = (snap.roundWinner == 0) ? "PONTO PARA P1!" : (snap.roundWinner == 1 ? "PONTO PARA P2!" : "EMPATE!");
        DrawText(msg, 468 - MeasureText(msg, 40)/2, 300, 40, snap.roundWinner == 0 ? BLUE : RED);
    }
    if (snap.phase == PHASE_MATCH_END) {
        DrawRectangle(0, 0, 936, 684, {0, 0, 0, 200});
        const char* res = (snap.matchWinner == 0) ? "P1 VENCEU!" : "P2 VENCEU!";
        DrawText(res, 468 - MeasureText(res, 60)/2, 300, 60, GOLD);
        DrawText("Feche o jogo para reiniciar", 468 - MeasureText("Feche o jogo para reiniciar", 20)/2, 400, 20, LIGHTGRAY);
    }
}

void drawVSScreen(const GameSnapshot& snap, int myId) {
    loadTextures();
    (void)myId;

    ClearBackground({12, 12, 26, 255});
    DrawRectangle(0, 0, 936, 684, {0, 0, 0, 80});

    static const Color kTint[2] = { {80, 160, 230, 255}, {230, 80, 80, 255} };

    auto drawSide = [&](int side, float cx, float tintColor) {
        (void)tintColor;
        uint8_t tId = snap.trainers[side].trainerId;
        if (tId < N_TRAINERS) {
            const TrainerDefEntry& td = TRAINER_DEFS[tId];

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

            int nw = MeasureText(td.name, 22);
            DrawText(td.name, (int)(cx - nw * 0.5f), (int)(py + pSize + 10), 22, WHITE);

            int dw = MeasureText(td.discipline, 14);
            DrawText(td.discipline, (int)(cx - dw * 0.5f), (int)(py + pSize + 36), 14, LIGHTGRAY);
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
            if (hDefIdx >= N_HEROES) continue;
            const HeroDefEntry& hd = HERO_DEFS[hDefIdx];

            float hx = startX + h * (cardW + gap);

            DrawRectangleRounded({hx, hy, cardW, cardH}, 0.06f, 6, {30, 30, 50, 255});
            DrawRectangleRoundedLines({hx, hy, cardW, cardH}, 0.06f, 6, kTint[side]);

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

            int nn = MeasureText(hd.name, 11);
            DrawText(hd.name, (int)(hx + (cardW - nn) * 0.5f), (int)(pyy + ps + 6), 11, WHITE);

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

    drawSide(0, 234.f, 0);
    drawSide(1, 702.f, 1);

    DrawText("VS", 468 - MeasureText("VS", 72)/2, 150, 72, GOLD);

    if (snap.phase == PHASE_VS_INTRO) {
        char timerStr[16];
        snprintf(timerStr, sizeof(timerStr), "%d", snap.timer);
        DrawText(timerStr, 468 - MeasureText(timerStr, 48)/2, 580, 48, {255, 255, 255, 180});
    }

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
//  MELEE ATTACK ANIMATIONS
// ═════════════════════════════════════════════════════════════════════════════

struct AttackAnim {
    Vector2 from;
    Vector2 to;
    Vector2 start;
    Vector2 end;
    float   timer;
    float   maxTimer;
};

static std::vector<AttackAnim> attackAnims;

void spawnAttackAnim(Vector2 from, Vector2 to) {
    attackAnims.push_back({ from, to, from, to, 0.16f, 0.16f });
}

void updateAndDrawAttackAnims(float dt) {
    for (int i = (int)attackAnims.size() - 1; i >= 0; i--) {
        AttackAnim& a = attackAnims[i];
        a.timer -= dt;
        if (a.timer <= 0.f) {
            attackAnims.erase(attackAnims.begin() + i);
            continue;
        }
        float t = 1.f - (a.timer / a.maxTimer);
        float phase = (t < 0.5f) ? t * 2.f : 2.f * (1.f - t);
        Vector2 pos = {
            a.from.x + (a.to.x - a.from.x) * phase * 0.3f,
            a.from.y + (a.to.y - a.from.y) * phase * 0.3f
        };
        float alpha = (t < 0.5f) ? 1.f : 1.f - (t - 0.5f) * 2.f;
        Color c = {255, 220, 100, (unsigned char)(255.f * alpha)};
        DrawCircleV(pos, 6.f, c);
        if (phase > 0.01f) {
            Color lineC = {255, 200, 80, (unsigned char)(180.f * alpha)};
            DrawLineEx(a.from, pos, 2.f, lineC);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  RANGED PROJECTILES
// ═════════════════════════════════════════════════════════════════════════════

struct Projectile {
    Vector2 from;
    Vector2 to;
    float   timer;
    float   maxTimer;
    uint8_t archetype;
};

static std::vector<Projectile> projectiles;

void spawnProjectile(Vector2 from, Vector2 to, uint8_t archetype) {
    projectiles.push_back({ from, to, 0.3f, 0.3f, archetype });
}

void updateAndDrawProjectiles(float dt) {
    for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
        Projectile& p = projectiles[i];
        p.timer -= dt;
        if (p.timer <= 0.f) {
            projectiles.erase(projectiles.begin() + i);
            continue;
        }
        float t = 1.f - (p.timer / p.maxTimer);
        Vector2 pos = {
            p.from.x + (p.to.x - p.from.x) * t,
            p.from.y + (p.to.y - p.from.y) * t
        };
        float alpha = 1.f;
        Color c;
        if (p.archetype == ARCHETYPE_MAGE) {
            c = {160, 80, 255, (unsigned char)(255.f * alpha)};
        } else {
            c = {80, 220, 130, (unsigned char)(255.f * alpha)};
        }
        Color trailC = c; trailC.a = (unsigned char)(100.f * (1.f - t));
        DrawLineEx(p.from, pos, 2.f, trailC);
        DrawCircleV(pos, 5.f, c);
        DrawCircleLinesV(pos, 5.f, WHITE);
        if (t > 0.8f) {
            float flash = (t - 0.8f) / 0.2f;
            Color flashC = WHITE; flashC.a = (unsigned char)(200.f * (1.f - flash));
            DrawCircleV(p.to, 8.f * flash, flashC);
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
    DrawRectangle(0, 0, 936, 684, BG);

    // Title
    const char* title = "SELECIONE SEU TREINADOR";
    DrawText(title, (936 - MeasureText(title, 28))/2, 12, 28, WHITE);

    // Timer
    char timerStr[32];
    snprintf(timerStr, sizeof(timerStr), "%.1f", snap.selectTimer);
    DrawText(timerStr, 468 - MeasureText(timerStr, 36)/2, 48, 36, GOLD);

    // Left portrait (P1)
    {
        float px = 40.f, py = 100.f, pSize = 180.f;
        int tIdx = p1.trainerLocked >= 0 ? p1.trainerLocked : p1.trainerCursor;
        if (tIdx < nT && selTrainerTex && selTrainerTex[tIdx].id) {
            DrawTexturePro(selTrainerTex[tIdx],
                {0,0,(float)selTrainerTex[tIdx].width,(float)selTrainerTex[tIdx].height},
                {px, py, pSize, pSize}, {0,0}, 0.f, WHITE);
        } else {
            DrawRectangle((int)px, (int)py, (int)pSize, (int)pSize, kP1Color);
        }
        DrawRectangleLinesEx({px, py, pSize, pSize}, 3, kP1Color);
        const char* p1lbl = "P1";
        DrawText(p1lbl, (int)(px + (pSize - MeasureText(p1lbl, 20))/2), (int)(py + pSize + 6), 20, kP1Color);
        if (p1.trainerLocked >= 0) {
            const char* ready = "READY!";
            DrawText(ready, (int)(px + (pSize - MeasureText(ready, 24))/2), (int)(py + pSize + 30), 24, GREEN);
        } else if (tIdx < nT) {
            DrawText(trainers[tIdx].name, (int)(px + (pSize - MeasureText(trainers[tIdx].name, 14))/2), (int)(py + pSize + 30), 14, WHITE);
        }
    }

    // Right portrait (P2)
    {
        float px = 936 - 40 - 180.f, py = 100.f, pSize = 180.f;
        int tIdx = p2.trainerLocked >= 0 ? p2.trainerLocked : p2.trainerCursor;
        if (tIdx < nT && selTrainerTex && selTrainerTex[tIdx].id) {
            DrawTexturePro(selTrainerTex[tIdx],
                {0,0,(float)selTrainerTex[tIdx].width,(float)selTrainerTex[tIdx].height},
                {px, py, pSize, pSize}, {0,0}, 0.f, WHITE);
        } else {
            DrawRectangle((int)px, (int)py, (int)pSize, (int)pSize, kP2Color);
        }
        DrawRectangleLinesEx({px, py, pSize, pSize}, 3, kP2Color);
        const char* p2lbl = "P2";
        DrawText(p2lbl, (int)(px + (pSize - MeasureText(p2lbl, 20))/2), (int)(py + pSize + 6), 20, kP2Color);
        if (p2.trainerLocked >= 0) {
            const char* ready = "READY!";
            DrawText(ready, (int)(px + (pSize - MeasureText(ready, 24))/2), (int)(py + pSize + 30), 24, GREEN);
        } else if (tIdx < nT) {
            DrawText(trainers[tIdx].name, (int)(px + (pSize - MeasureText(trainers[tIdx].name, 14))/2), (int)(py + pSize + 30), 14, WHITE);
        }
    }

    // Trainer cards at bottom
    const float CW = 188.f, CH = 200.f, PAD = 16.f;
    float totalW = nT * CW + (nT-1) * PAD;
    float startX = (936.f - totalW) / 2.f;
    float startY = 400.f;

    for (int i = 0; i < nT; i++) {
        float x = startX + i * (CW + PAD);
        float y = startY;
        bool hov1 = (i == p1.trainerCursor && p1.trainerLocked < 0);
        bool hov2 = (i == p2.trainerCursor && p2.trainerLocked < 0);

        Color bg = (hov1 || hov2) ? Color{50,50,90,255} : Color{30,30,58,255};
        DrawRectangleRounded({x, y, CW, CH}, 0.08f, 6, bg);
        DrawRectangleRoundedLines({x, y, CW, CH}, 0.08f, 6, {60,60,90,255});

        float ps = 100.f, px2 = x + (CW-ps)/2.f, py2 = y + 14.f;
        if (selTrainerTex && selTrainerTex[i].id) {
            DrawTexturePro(selTrainerTex[i],
                {0,0,(float)selTrainerTex[i].width,(float)selTrainerTex[i].height},
                {px2, py2, ps, ps}, {}, 0.f, WHITE);
        } else {
            DrawRectangleRounded({px2,py2,ps,ps}, 0.2f, 6, trainers[i].color);
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
    DrawText(ctrlHint, (int)(468 - MeasureText(ctrlHint, 16)/2), (int)cy, 16, {160,160,190,255});
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
    DrawRectangle(0, 0, 936, 684, BG);

    // Timer
    char timerStr[32];
    snprintf(timerStr, sizeof(timerStr), "Escolha 3 Herois  —  %.1f", snap.selectTimer);
    int tw = MeasureText(timerStr, 24);
    DrawRectangle(468 - tw/2 - 8, 6, tw + 16, 36, {0,0,0,160});
    DrawText(timerStr, 468 - tw/2, 12, 24, GOLD);

    const float CARD_W = 78.f, CARD_H = 110.f, PAD = 6.f;

    auto drawPlayerSide = [&](int pid, const PlayerInput& inp, Color pCol, float sideX, bool isLeft) {
        int tIdx = snap.trainerChoice[pid];

        // ── Trainer standing on side (large portrait) ──
        float tW = 200.f, tH = 360.f;
        float tX = isLeft ? 30.f : (936 - 30 - tW);
        float tY = 50.f;

        // Shadow/ground effect under trainer
        DrawEllipse((int)(tX + tW/2), (int)(tY + tH + 10), tW*0.45f, 12.f, {0,0,0,120});

        if (tIdx < N_TRAINERS && selTrainerTex && selTrainerTex[tIdx].id) {
            DrawTexturePro(selTrainerTex[tIdx],
                {0,0,(float)selTrainerTex[tIdx].width,(float)selTrainerTex[tIdx].height},
                {tX, tY, tW, tH}, {0,0}, 0.f, WHITE);
        } else {
            DrawRectangle((int)tX, (int)tY, (int)tW, (int)tH, pCol);
        }
        DrawRectangleLinesEx({tX, tY, tW, tH}, 3, pCol);

        // Trainer name below portrait
        const char* tName = (tIdx < N_TRAINERS) ? TRAINER_DEFS[tIdx].name : "???";
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
        float slotsX = sideX + (464.f - slotsTotalW) / 2.f;
        float slotsY = 440.f;

        for (int s = 0; s < 3; s++) {
            float sx = slotsX + s * (slotSize + slotGap);
            bool filled = s < (int)inp.heroPicks.size();
            Color slotBg = filled ? Color{40,60,40,220} : Color{30,30,50,180};
            DrawRectangleRounded({sx, slotsY, slotSize, slotSize}, 0.1f, 4, slotBg);
            DrawRectangleRoundedLines({sx, slotsY, slotSize, slotSize}, 0.1f, 4, filled ? GREEN : Color{60,60,80,200});
            if (filled) {
                int hIdx = inp.heroPicks[s];
                if (selHeroTex && selHeroTex[hIdx].id) {
                    DrawTexturePro(selHeroTex[hIdx],
                        {0,0,(float)selHeroTex[hIdx].width,(float)selHeroTex[hIdx].height},
                        {sx+2, slotsY+2, slotSize-4, slotSize-4}, {}, 0.f, WHITE);
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
        float cardsX = sideX + (464.f - cardsTotalW) / 2.f;
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
            if (selHeroTex && selHeroTex[i].id) {
                DrawTexturePro(selHeroTex[i],
                    {0,0,(float)selHeroTex[i].width,(float)selHeroTex[i].height},
                    {x+3, y+4, CARD_W-6, imgH}, {}, 0.f, WHITE);
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
        float cx = sideX + (464.f - cw) / 2.f;
        DrawRectangle((int)(cx - 4), 640, cw + 8, 24, {0,0,0,180});
        DrawText(ctrl, (int)cx, 642, 16, pCol);
    };

    drawPlayerSide(0, p1, kP1Color, 0.f, true);
    drawPlayerSide(1, p2, kP2Color, 472.f, false);
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

// ── drawPlacementCursors ─────────────────────────────────────────────────────
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
                // Find name from HERO_DEFS
                uint8_t hdi = snap.heroes[i].heroDefIndex;
                if (hdi < N_HEROES) outName = HERO_DEFS[hdi].name;
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
    DrawRectangle(0, 650, w1 + 16, 30, {0,0,0,200});
    DrawText(lbl1, 8, 654, 18, kP1Color);

    uint8_t arch2 = 0xFF; bool placed2 = false; const char* name2 = "???";
    findHeroInfo(1, p2.moveHeroIdx, arch2, placed2, name2);
    char lbl2[128];
    snprintf(lbl2, sizeof(lbl2), "P2: [%d/3] %s  |  Numpad 1/2/3 heroi  |  Setas mover  |  Enter posicionar", p2.moveHeroIdx + 1, name2);
    int w2 = MeasureText(lbl2, 18);
    DrawRectangle(936 - w2 - 16, 650, w2 + 16, 30, {0,0,0,200});
    DrawText(lbl2, 936 - w2 - 8, 654, 18, kP2Color);
}
