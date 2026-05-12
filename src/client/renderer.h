#pragma once
#include "raylib.h"
#include "../../include/protocol.h"
#include <vector>

enum IconId {
    ICON_COIN,
    ICON_HELP,
    ICON_SWORD,
    ICON_SHIELD,
    ICON_HEART,
    ICON_BOOT,
    ICON_WAND,
    ICON_POTION,
    ICON_RING,
    ICON_ARROW,
    ICON_COUNT
};

void loadIcons();
void drawIcon(int iconId, Rectangle rect, Color tint);
int itemIdToIcon(uint8_t itemId);

struct Layout {
    float screenW, screenH;
    float topBarH;
    float bottomCardsH;
    float sidePanelW;
    float gridX, gridY, gridW, gridH;
    float cellW, cellH;
    float cardsY, cardW, cardH;
    float leftPanelX, leftPanelW;
    float rightPanelX, rightPanelW;
    float trainerAbilityBtnY;
    float controlsHintY;
};

Layout computeLayout();
void  applyLayout(const Layout& l);

extern Layout  g_layout;
extern float   GX, GY, GW, GH, CELLW, CELLH;

struct TrainerDef {
    const char* name;
    const char* discipline;
    Color       color;
    uint8_t     abilityType;
    const char* abilityName;
    const char* portraitPath;
    const char* cardPath;
};

struct HeroDef {
    const char* name;
    uint8_t     archetype;
    uint8_t     trainerIndex;
    const char* className;
    int         hp, ad, arm;
    const char* assetPath;
};

struct PlayerInput {
    int  trainerCursor = 0;
    int  trainerCursorRow = 0;
    int  trainerLocked  = -1;
    int  heroCursor     = 0;
    std::vector<int> heroPicks;
    bool herosLocked    = false;
    int   moveHeroIdx   = 0;
    bool  isPositioning = false;
    uint8_t cursorX     = 0;
    uint8_t cursorY     = 0;
    bool  abilityReady  = true;
    int   shopCursorX    = 0;
    int   shopCursorY    = 0;
    int   generalItemCursor = 0;
    int   shopSelectedHero = 0;
    bool  targetingMode   = false;
    int   targetingHeroIdx = -1;
    uint8_t targetCursorX = 0;
    uint8_t targetCursorY = 0;
};

void initSelectionAssets(const TrainerDef* trainers, int nTrainers,
                         const HeroDef*   heroes,   int nHeroes);
void freeSelectionAssets(int nTrainers, int nHeroes);

void drawTrainerSelectMK(const GameSnapshot& snap,
                         const TrainerDef* trainers, int nTrainers,
                         const PlayerInput& p1, const PlayerInput& p2);

void drawHeroSelectMK(const GameSnapshot& snap,
                      const TrainerDef* trainers,
                      const HeroDef* heroes, int nHeroes,
                      const PlayerInput& p1, const PlayerInput& p2);

void drawPlacementCursors(const GameSnapshot& snap,
                          const PlayerInput& p1, const PlayerInput& p2);

bool isValidDeployCell(int col, int row, uint8_t archetype, bool isLeft);

void loadTextures();
void unloadTextures();

Vector2   cellCenter(int cx, int cy);
Rectangle cellRect(int cx, int cy);

void drawGrid();
void drawBuffZones(const GameSnapshot& snap);
void drawPedestal(Vector2 ctr, int ownerId);
void drawHero(const HeroNetState& hs, Vector2 pos, int myId, bool dragging,
              float breathScale = 1.0f, float tiltAngle = 0.0f);
void drawHUD(const GameSnapshot& snap, int myId);
void drawOverlays(const GameSnapshot& snap, int myId);
void drawVSScreen(const GameSnapshot& snap, int myId);

void spawnFloatingText(Vector2 pos, int value, Color color);
void updateAndDrawFloatingTexts(float dt);

void spawnDeathEffect(Vector2 pos);
void spawnUltimateEffect(Vector2 pos);
void updateAndDrawVisualEffects(float dt);



void spawnHitFlash(Vector2 pos);
void updateAndDrawHitFlashes(float dt);

// ═════════════════════════════════════════════════════════════════════════════
//  SPRITE SHEET FX SYSTEM
// ═════════════════════════════════════════════════════════════════════════════

struct SpriteSheet {
    Texture2D texture;
    int cols;
    int rows;
    int frameWidth;
    int frameHeight;
};

struct FxAnim {
    Vector2 from;
    Vector2 to;
    Vector2 pos;
    int     sheetIndex;
    int     spriteRow;
    int     currentFrame;
    float   frameTimer;
    float   frameDuration;
    float   travelTimer;
    float   travelDuration;
    float   rotation;      // ângulo em graus para apontar na direção do alvo
    bool    isTraveling;
    bool    isExploding;
};

void initFxSystem();
void shutdownFxSystem();
void spawnRangedFx(Vector2 from, Vector2 to, uint8_t archetype);
void spawnMeleeFx(Vector2 from, Vector2 to, uint8_t archetype);
void updateAndDrawFxAnims(float dt);

void drawTargetArrow(Vector2 from, Vector2 to);
void drawTargetHighlight(Vector2 pos, float radius, Color color);
void drawAdjacentEnemyHighlights(const GameSnapshot& snap, int myId, int heroIdx);

// ── Targeting ────────────────────────────────────────────────────────────────
void drawTargetingVisuals(const GameSnapshot& snap, const PlayerInput& p1, const PlayerInput& p2);

// Convenience helper: returns global snapshot index for player's Nth hero slot
static inline int heroSlotToGlobal(const GameSnapshot& snap, int pid, int slot) {
    int count = 0;
    for (int i = 0; i < snap.heroCount; i++) {
        if (snap.heroes[i].ownerId == (uint8_t)pid) {
            if (count == slot) return i;
            count++;
        }
    }
    return -1;
}

void drawHeroCards(const GameSnapshot& snap, int myId);
void drawSidePanels(const GameSnapshot& snap, const PlayerInput& p1, const PlayerInput& p2);
void drawShop(const GameSnapshot& snap, const PlayerInput& p1, const PlayerInput& p2);
