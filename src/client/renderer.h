#pragma once
#include "raylib.h"
#include "../../include/protocol.h"
#include <vector>

// ─── Grid layout constants (shared with main.cpp) ────────────────────────────
extern const float GX, GY, GW, GH, CELLW, CELLH;

// ─── Selection screen data structures ────────────────────────────────────────

// Lightweight display-only struct for Trainer cards (local, pre-server)
struct TrainerDef {
    const char* name;
    const char* discipline;
    Color       color;
    uint8_t     abilityType;   // ABILITY_* from protocol.h
    const char* abilityName;   // Human-readable ability label
    const char* portraitPath;  // "" = use colored placeholder
};

// Lightweight display-only struct for Hero cards (local, pre-server)
struct HeroDef {
    const char* name;
    uint8_t     archetype;     // ARCHETYPE_* from protocol.h
    const char* className;     // "Tank", "Fighter", etc.
    int         hp, ad, arm;
    const char* assetPath;
};

// ─── Texture lifecycle for selection screens ─────────────────────────────────
void initSelectionAssets(const TrainerDef* trainers, int nTrainers,
                         const HeroDef*   heroes,   int nHeroes);
void freeSelectionAssets(int nTrainers, int nHeroes);

// ─── Selection screen draw calls ─────────────────────────────────────────────
void drawTrainerSelect(const TrainerDef* trainers, int nTrainers,
                       int cursor, int selectedIdx, int myId);

void drawHeroSelect(const TrainerDef& trainer,
                    const HeroDef* heroes, int nHeroes,
                    int gridCols,
                    int cursor,
                    const std::vector<int>& picks,
                    int myId);

// ─── Battle screen draw calls ─────────────────────────────────────────────────
void loadTextures();
void unloadTextures();

Vector2   cellCenter(int cx, int cy);
Rectangle cellRect(int cx, int cy);

void drawGrid();
void drawBuffZones(const GameSnapshot& snap);
void drawHero(const HeroNetState& hs, Vector2 pos, int myId, bool dragging);
void drawHUD(const GameSnapshot& snap, int myId);
void drawOverlays(const GameSnapshot& snap, int myId);
void drawVSScreen(const GameSnapshot& snap, int myId);

// ─── Floating damage/heal text ───────────────────────────────────────────────
void spawnFloatingText(Vector2 pos, int value, Color color);
void updateAndDrawFloatingTexts(float dt);

// ─── Visual effects (death, ultimate) ────────────────────────────────────────
void spawnDeathEffect(Vector2 pos);
void spawnUltimateEffect(Vector2 pos);
void updateAndDrawVisualEffects(float dt);

// ─── Targeting arrows ──────────────────────────────────────────────────────────
void drawTargetArrow(Vector2 from, Vector2 to);
void drawTargetHighlight(Vector2 pos, float radius, Color color);
void drawAdjacentEnemyHighlights(const GameSnapshot& snap, int myId, int heroIdx);
