#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <vector>
#include <algorithm>

#include "../../include/protocol.h"
#include "../../include/game.h"
#include "../../include/game_defs.h"
#include "renderer.h"

// ─── Local Trainer/Hero defs built from global DB vectors ────────────────────
static std::vector<TrainerDef> g_localTrainerDefs;
static std::vector<HeroDef>    g_localHeroDefs;

static void buildLocalDefs() {
    g_localTrainerDefs.resize(g_trainerDefs.size());
    for (size_t i = 0; i < g_trainerDefs.size(); i++) {
        g_localTrainerDefs[i] = {
            g_trainerDefs[i].name.c_str(),
            g_trainerDefs[i].discipline.c_str(),
            { (unsigned char)g_trainerDefs[i].colorR,
              (unsigned char)g_trainerDefs[i].colorG,
              (unsigned char)g_trainerDefs[i].colorB, 255 },
            g_trainerDefs[i].abilityType,
            g_trainerDefs[i].abilityName.c_str(),
            g_trainerDefs[i].portraitPath.c_str(),
            g_trainerDefs[i].cardPath.c_str()
        };
    }
    g_localHeroDefs.resize(g_heroDefs.size());
    for (size_t i = 0; i < g_heroDefs.size(); i++) {
        g_localHeroDefs[i] = {
            g_heroDefs[i].name.c_str(),
            g_heroDefs[i].archetype,
            (uint8_t)(g_heroDefs[i].trainerId - 1),
            g_heroDefs[i].className.c_str(),
            g_heroDefs[i].hp,
            g_heroDefs[i].ad,
            g_heroDefs[i].arm,
            g_heroDefs[i].assetPath.c_str()
        };
    }
}

static int heroFilteredToGlobal(int trainerIdx, int cursor) {
    int count = 0;
    for (int i = 0; i < (int)g_heroDefs.size(); i++)
        if (g_heroDefs[i].trainerId == (uint8_t)(trainerIdx + 1)) {
            if (count == cursor) return i;
            count++;
        }
    return cursor;
}

static int heroesForTrainer(int trainerIdx) {
    int count = 0;
    for (int i = 0; i < (int)g_heroDefs.size(); i++)
        if (g_heroDefs[i].trainerId == (uint8_t)(trainerIdx + 1)) count++;
    return count;
}

// ─── main ────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    srand((unsigned)time(nullptr));

    bool soloMode = (argc >= 2 && strcmp(argv[1], "--solo") == 0);

    InitWindow(1640, 1060, "Battle-CIn");
    SetTargetFPS(60);

    Game game;
    game.registerPlayerLocal(0);
    game.registerPlayerLocal(1);
    buildLocalDefs();

    PlayerInput inputs[2];

    if (soloMode) {
        game.createBot(1);
        GameSnapshot botSnap{};
        game.buildSnapshot(botSnap);
        inputs[1].trainerLocked = botSnap.trainerChoice[1];
        inputs[1].trainerCursor = botSnap.trainerChoice[1];
        inputs[1].herosLocked = true;
        for (int h = 0; h < 3; h++)
            inputs[1].heroPicks.push_back(botSnap.heroPicks[1][h]);
    }
    GameSnapshot snap{};
    memset(&snap, 0, sizeof(snap));
    snap.phase = PHASE_SELECT;
    snap.roundWinner = 0xFF;
    snap.matchWinner = 0xFF;

    Texture2D arena = LoadTexture("assets/arena.png");

    struct HeroVis {
        Vector2 pos;
        Vector2 prevPos;
        uint16_t prevHp;
        bool active;
        bool prevAlive;
        bool prevUltActive;
    };
    HeroVis heroVis[MAX_HEROES_TOTAL];
    for (int i = 0; i < MAX_HEROES_TOTAL; i++) {
        heroVis[i].active = false;
        heroVis[i].prevHp = 0;
        heroVis[i].prevPos = {0, 0};
        heroVis[i].prevAlive = false;
        heroVis[i].prevUltActive = false;
    }

    initSelectionAssets(g_localTrainerDefs.data(), (int)g_localTrainerDefs.size(),
                        g_localHeroDefs.data(), (int)g_localHeroDefs.size());
    initFxSystem();

    float accumulator = 0.f;
    uint8_t prevPhase = PHASE_SELECT;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ════════════════════════════════════════════════════════════════════
        //  INPUT
        // ════════════════════════════════════════════════════════════════════
        if (IsKeyPressed(KEY_F10)) game.handleDebugReduceHP(0);

        if (snap.phase == PHASE_SELECT) {
            if (snap.selectSubphase == 0) {
                // ── Trainer Select ─────────────────────────────────────────
                int nTrainers = (int)g_localTrainerDefs.size();
                if (nTrainers == 0) nTrainers = 1; // safeguard

                // P1 (esquerda): A/D move cursor, Space confirma
                if (IsKeyPressed(KEY_D))      inputs[0].trainerCursor = (inputs[0].trainerCursor + 1) % nTrainers;
                if (IsKeyPressed(KEY_A))      inputs[0].trainerCursor = (inputs[0].trainerCursor + nTrainers - 1) % nTrainers;
                if (IsKeyPressed(KEY_SPACE) && inputs[0].trainerLocked < 0) {
                    inputs[0].trainerLocked = inputs[0].trainerCursor;
                    game.handleLocalTrainerLock(0, (uint8_t)inputs[0].trainerCursor);
                }

                // P2 (direita): Arrow keys move cursor, Enter confirma
                if (IsKeyPressed(KEY_RIGHT)) inputs[1].trainerCursor = (inputs[1].trainerCursor + 1) % nTrainers;
                if (IsKeyPressed(KEY_LEFT))  inputs[1].trainerCursor = (inputs[1].trainerCursor + nTrainers - 1) % nTrainers;
                if (IsKeyPressed(KEY_ENTER) && inputs[1].trainerLocked < 0) {
                    inputs[1].trainerLocked = inputs[1].trainerCursor;
                    game.handleLocalTrainerLock(1, (uint8_t)inputs[1].trainerCursor);
                }
            } else {
                // ── Hero Select ───────────────────────────────────────────
                int nHerosPerTrainer = heroesForTrainer(snap.trainerChoice[0]);

                // P1 (esquerda): A/D + Space
                if (nHerosPerTrainer > 0) {
                    if (IsKeyPressed(KEY_D))      inputs[0].heroCursor = (inputs[0].heroCursor + 1) % nHerosPerTrainer;
                    if (IsKeyPressed(KEY_A))      inputs[0].heroCursor = (inputs[0].heroCursor + nHerosPerTrainer - 1) % nHerosPerTrainer;
                    if (IsKeyPressed(KEY_SPACE) && !inputs[0].herosLocked) {
                        int globalIdx = heroFilteredToGlobal(snap.trainerChoice[0], inputs[0].heroCursor);
                        auto it = std::find(inputs[0].heroPicks.begin(), inputs[0].heroPicks.end(), globalIdx);
                        if (it != inputs[0].heroPicks.end()) {
                            inputs[0].heroPicks.erase(it);
                        } else if ((int)inputs[0].heroPicks.size() < 3) {
                            inputs[0].heroPicks.push_back(globalIdx);
                        }
                    }
                }

                // P2 (direita): Arrow keys + Enter
                if (nHerosPerTrainer > 0) {
                    if (IsKeyPressed(KEY_RIGHT)) inputs[1].heroCursor = (inputs[1].heroCursor + 1) % nHerosPerTrainer;
                    if (IsKeyPressed(KEY_LEFT))  inputs[1].heroCursor = (inputs[1].heroCursor + nHerosPerTrainer - 1) % nHerosPerTrainer;
                    if (IsKeyPressed(KEY_ENTER) && !inputs[1].herosLocked) {
                        int globalIdx = heroFilteredToGlobal(snap.trainerChoice[1], inputs[1].heroCursor);
                        auto it = std::find(inputs[1].heroPicks.begin(), inputs[1].heroPicks.end(), globalIdx);
                        if (it != inputs[1].heroPicks.end()) {
                            inputs[1].heroPicks.erase(it);
                        } else if ((int)inputs[1].heroPicks.size() < 3) {
                            inputs[1].heroPicks.push_back(globalIdx);
                        }
                    }
                }

                // When both have 3 picks, lock in
                if (!inputs[0].herosLocked && (int)inputs[0].heroPicks.size() == 3) {
                    inputs[0].herosLocked = true;
                    uint8_t picks[3] = {
                        (uint8_t)inputs[0].heroPicks[0],
                        (uint8_t)inputs[0].heroPicks[1],
                        (uint8_t)inputs[0].heroPicks[2]
                    };
                    game.handleLocalHeroPick(0, picks);
                }
                if (!inputs[1].herosLocked && (int)inputs[1].heroPicks.size() == 3) {
                    inputs[1].herosLocked = true;
                    uint8_t picks[3] = {
                        (uint8_t)inputs[1].heroPicks[0],
                        (uint8_t)inputs[1].heroPicks[1],
                        (uint8_t)inputs[1].heroPicks[2]
                    };
                    game.handleLocalHeroPick(1, picks);
                }
            }
        }

        else if (snap.phase == PHASE_POSITIONING) {
            // P1 (esquerda): 1/2/3 select hero, WASD move, Space place
            if (IsKeyPressed(KEY_ONE))   inputs[0].moveHeroIdx = 0;
            if (IsKeyPressed(KEY_TWO))   inputs[0].moveHeroIdx = 1;
            if (IsKeyPressed(KEY_THREE)) inputs[0].moveHeroIdx = 2;

            if (IsKeyPressed(KEY_W))      inputs[0].cursorY = (uint8_t)((int)inputs[0].cursorY > 0 ? inputs[0].cursorY - 1 : 0);
            if (IsKeyPressed(KEY_S))      inputs[0].cursorY = (uint8_t)((int)inputs[0].cursorY < GRID_ROWS - 1 ? inputs[0].cursorY + 1 : GRID_ROWS - 1);
            if (IsKeyPressed(KEY_A))      inputs[0].cursorX = (uint8_t)((int)inputs[0].cursorX > 0 ? inputs[0].cursorX - 1 : 0);
            if (IsKeyPressed(KEY_D))      inputs[0].cursorX = (uint8_t)((int)inputs[0].cursorX < GRID_COLS - 1 ? inputs[0].cursorX + 1 : GRID_COLS - 1);

            if (IsKeyPressed(KEY_SPACE)) {
                bool ok = game.handlePlaceHero(0, inputs[0].moveHeroIdx,
                                               inputs[0].cursorX, inputs[0].cursorY);
                if (ok && inputs[0].moveHeroIdx < 2) inputs[0].moveHeroIdx++;
            }

            // P2 (direita): KP_1/KP_2/KP_3 select hero, arrow keys move, Enter place
            if (IsKeyPressed(KEY_KP_1))   inputs[1].moveHeroIdx = 0;
            if (IsKeyPressed(KEY_KP_2))   inputs[1].moveHeroIdx = 1;
            if (IsKeyPressed(KEY_KP_3))   inputs[1].moveHeroIdx = 2;

            if (IsKeyPressed(KEY_UP))     inputs[1].cursorY = (uint8_t)((int)inputs[1].cursorY > 0 ? inputs[1].cursorY - 1 : 0);
            if (IsKeyPressed(KEY_DOWN))   inputs[1].cursorY = (uint8_t)((int)inputs[1].cursorY < GRID_ROWS - 1 ? inputs[1].cursorY + 1 : GRID_ROWS - 1);
            if (IsKeyPressed(KEY_LEFT))   inputs[1].cursorX = (uint8_t)((int)inputs[1].cursorX > 0 ? inputs[1].cursorX - 1 : 0);
            if (IsKeyPressed(KEY_RIGHT))  inputs[1].cursorX = (uint8_t)((int)inputs[1].cursorX < GRID_COLS - 1 ? inputs[1].cursorX + 1 : GRID_COLS - 1);
            if (IsKeyPressed(KEY_ENTER)) {
                bool ok = game.handlePlaceHero(1, inputs[1].moveHeroIdx,
                                               inputs[1].cursorX, inputs[1].cursorY);
                if (ok && inputs[1].moveHeroIdx < 2) inputs[1].moveHeroIdx++;
            }
        }

        else if (snap.phase == PHASE_BATTLE) {
            if (IsKeyPressed(KEY_Q)) game.handleUseAbility(0);
            if (IsKeyPressed(KEY_E)) game.handleUseAbility(1);

            // ── General item usage ─────────────────────────────────
            if (!inputs[0].targetingMode) {
                if (IsKeyPressed(KEY_W)) inputs[0].generalItemCursor = (inputs[0].generalItemCursor + 2) % 3;
                if (IsKeyPressed(KEY_S)) inputs[0].generalItemCursor = (inputs[0].generalItemCursor + 1) % 3;
                if (IsKeyPressed(KEY_F)) game.handleUseGeneralItem(0, inputs[0].generalItemCursor);
            }
            if (!inputs[1].targetingMode) {
                if (IsKeyPressed(KEY_UP))   inputs[1].generalItemCursor = (inputs[1].generalItemCursor + 2) % 3;
                if (IsKeyPressed(KEY_DOWN)) inputs[1].generalItemCursor = (inputs[1].generalItemCursor + 1) % 3;
                if (IsKeyPressed(KEY_KP_ENTER)) game.handleUseGeneralItem(1, inputs[1].generalItemCursor);
            }

            // ── P1 Targeting ─────────────────────────────────────────
            if (IsKeyPressed(KEY_LEFT_SHIFT)) {
                if (inputs[0].targetingMode) {
                    inputs[0].targetingMode = false;
                } else {
                    for (int slot = 0; slot < 3; slot++) {
                        int gIdx = heroSlotToGlobal(snap, 0, slot);
                        if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                            inputs[0].targetingHeroIdx = slot;
                            inputs[0].targetCursorX = snap.heroes[gIdx].x;
                            inputs[0].targetCursorY = snap.heroes[gIdx].y;
                            inputs[0].targetingMode = true;
                            break;
                        }
                    }
                }
            }
            if (inputs[0].targetingMode) {
                if (IsKeyPressed(KEY_ONE)) {
                    int gIdx = heroSlotToGlobal(snap, 0, 0);
                    if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                        inputs[0].targetingHeroIdx = 0;
                        inputs[0].targetCursorX = snap.heroes[gIdx].x;
                        inputs[0].targetCursorY = snap.heroes[gIdx].y;
                    }
                }
                if (IsKeyPressed(KEY_TWO)) {
                    int gIdx = heroSlotToGlobal(snap, 0, 1);
                    if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                        inputs[0].targetingHeroIdx = 1;
                        inputs[0].targetCursorX = snap.heroes[gIdx].x;
                        inputs[0].targetCursorY = snap.heroes[gIdx].y;
                    }
                }
                if (IsKeyPressed(KEY_THREE)) {
                    int gIdx = heroSlotToGlobal(snap, 0, 2);
                    if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                        inputs[0].targetingHeroIdx = 2;
                        inputs[0].targetCursorX = snap.heroes[gIdx].x;
                        inputs[0].targetCursorY = snap.heroes[gIdx].y;
                    }
                }

                if (IsKeyPressed(KEY_W)) inputs[0].targetCursorY = (uint8_t)((int)inputs[0].targetCursorY > 0 ? inputs[0].targetCursorY - 1 : 0);
                if (IsKeyPressed(KEY_S)) inputs[0].targetCursorY = (uint8_t)((int)inputs[0].targetCursorY < GRID_ROWS - 1 ? inputs[0].targetCursorY + 1 : GRID_ROWS - 1);
                if (IsKeyPressed(KEY_A)) inputs[0].targetCursorX = (uint8_t)((int)inputs[0].targetCursorX > 0 ? inputs[0].targetCursorX - 1 : 0);
                if (IsKeyPressed(KEY_D)) inputs[0].targetCursorX = (uint8_t)((int)inputs[0].targetCursorX < GRID_COLS - 1 ? inputs[0].targetCursorX + 1 : GRID_COLS - 1);

                if (IsKeyPressed(KEY_SPACE)) {
                    int heroG = heroSlotToGlobal(snap, 0, inputs[0].targetingHeroIdx);
                    int targetLocal = -1;
                    if (heroG >= 0) {
                        int enemyIdx = 0;
                        for (int i = 0; i < snap.heroCount; i++) {
                            if (snap.heroes[i].ownerId == 1) {
                                if (snap.heroes[i].alive &&
                                    snap.heroes[i].x == inputs[0].targetCursorX &&
                                    snap.heroes[i].y == inputs[0].targetCursorY)
                                {
                                    int dx = abs((int)snap.heroes[heroG].x - snap.heroes[i].x);
                                    int dy = abs((int)snap.heroes[heroG].y - snap.heroes[i].y);
                                    if (dx <= 1 && dy <= 1)
                                        targetLocal = enemyIdx;
                                    break;
                                }
                                enemyIdx++;
                            }
                        }
                    }
                    game.handleTarget(0, inputs[0].targetingHeroIdx, targetLocal);
                    inputs[0].targetingMode = false;
                }
            }

            // ── P2 Targeting ─────────────────────────────────────────
            if (IsKeyPressed(KEY_RIGHT_SHIFT)) {
                if (inputs[1].targetingMode) {
                    inputs[1].targetingMode = false;
                } else {
                    for (int slot = 0; slot < 3; slot++) {
                        int gIdx = heroSlotToGlobal(snap, 1, slot);
                        if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                            inputs[1].targetingHeroIdx = slot;
                            inputs[1].targetCursorX = snap.heroes[gIdx].x;
                            inputs[1].targetCursorY = snap.heroes[gIdx].y;
                            inputs[1].targetingMode = true;
                            break;
                        }
                    }
                }
            }
            if (inputs[1].targetingMode) {
                if (IsKeyPressed(KEY_KP_1)) {
                    int gIdx = heroSlotToGlobal(snap, 1, 0);
                    if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                        inputs[1].targetingHeroIdx = 0;
                        inputs[1].targetCursorX = snap.heroes[gIdx].x;
                        inputs[1].targetCursorY = snap.heroes[gIdx].y;
                    }
                }
                if (IsKeyPressed(KEY_KP_2)) {
                    int gIdx = heroSlotToGlobal(snap, 1, 1);
                    if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                        inputs[1].targetingHeroIdx = 1;
                        inputs[1].targetCursorX = snap.heroes[gIdx].x;
                        inputs[1].targetCursorY = snap.heroes[gIdx].y;
                    }
                }
                if (IsKeyPressed(KEY_KP_3)) {
                    int gIdx = heroSlotToGlobal(snap, 1, 2);
                    if (gIdx >= 0 && snap.heroes[gIdx].alive) {
                        inputs[1].targetingHeroIdx = 2;
                        inputs[1].targetCursorX = snap.heroes[gIdx].x;
                        inputs[1].targetCursorY = snap.heroes[gIdx].y;
                    }
                }

                if (IsKeyPressed(KEY_UP))    inputs[1].targetCursorY = (uint8_t)((int)inputs[1].targetCursorY > 0 ? inputs[1].targetCursorY - 1 : 0);
                if (IsKeyPressed(KEY_DOWN))  inputs[1].targetCursorY = (uint8_t)((int)inputs[1].targetCursorY < GRID_ROWS - 1 ? inputs[1].targetCursorY + 1 : GRID_ROWS - 1);
                if (IsKeyPressed(KEY_LEFT))  inputs[1].targetCursorX = (uint8_t)((int)inputs[1].targetCursorX > 0 ? inputs[1].targetCursorX - 1 : 0);
                if (IsKeyPressed(KEY_RIGHT)) inputs[1].targetCursorX = (uint8_t)((int)inputs[1].targetCursorX < GRID_COLS - 1 ? inputs[1].targetCursorX + 1 : GRID_COLS - 1);

                if (IsKeyPressed(KEY_ENTER)) {
                    int heroG = heroSlotToGlobal(snap, 1, inputs[1].targetingHeroIdx);
                    int targetLocal = -1;
                    if (heroG >= 0) {
                        int enemyIdx = 0;
                        for (int i = 0; i < snap.heroCount; i++) {
                            if (snap.heroes[i].ownerId == 0) {
                                if (snap.heroes[i].alive &&
                                    snap.heroes[i].x == inputs[1].targetCursorX &&
                                    snap.heroes[i].y == inputs[1].targetCursorY)
                                {
                                    int dx = abs((int)snap.heroes[heroG].x - snap.heroes[i].x);
                                    int dy = abs((int)snap.heroes[heroG].y - snap.heroes[i].y);
                                    if (dx <= 1 && dy <= 1)
                                        targetLocal = enemyIdx;
                                    break;
                                }
                                enemyIdx++;
                            }
                        }
                    }
                    game.handleTarget(1, inputs[1].targetingHeroIdx, targetLocal);
                    inputs[1].targetingMode = false;
                }
            }
        }

        else if (snap.phase == PHASE_SHOP) {
            // ── P1 Shop (WASD) ──
            int maxHeroes1 = 0;
            for (int i = 0; i < snap.heroCount; i++)
                if (snap.heroes[i].ownerId == 0) maxHeroes1++;
            int maxRows1 = 1 + maxHeroes1;  // row 0 = general, rows 1..max = heroes

            if (IsKeyPressed(KEY_D)) inputs[0].shopCursorX = (inputs[0].shopCursorX + 1) % 3;
            if (IsKeyPressed(KEY_A)) inputs[0].shopCursorX = (inputs[0].shopCursorX + 2) % 3;
            if (IsKeyPressed(KEY_S)) inputs[0].shopCursorY = (inputs[0].shopCursorY + 1) % maxRows1;
            if (IsKeyPressed(KEY_W)) inputs[0].shopCursorY = (inputs[0].shopCursorY + maxRows1 - 1) % maxRows1;

            if (IsKeyPressed(KEY_SPACE)) {
                if (inputs[0].shopCursorY == 0) {
                    int idx = -1, cnt = 0;
                    for (int i = 0; i < snap.shop.stockCount; i++) {
                        if (snap.shop.stock[i].category == ITEM_CATEGORY_GENERAL) {
                            if (cnt == inputs[0].shopCursorX) { idx = i; break; }
                            cnt++;
                        }
                    }
                    if (idx >= 0) game.handleBuyItem(0, idx, 0, 0);
                } else {
                    int idx = -1, cnt = 0;
                    for (int i = 0; i < snap.shop.stockCount; i++) {
                        if (snap.shop.stock[i].category == ITEM_CATEGORY_HERO) {
                            if (cnt == inputs[0].shopCursorX) { idx = i; break; }
                            cnt++;
                        }
                    }
                    if (idx >= 0) game.handleBuyItem(0, idx, inputs[0].shopCursorY - 1, -1);
                }
            }
            if (IsKeyPressed(KEY_F)) game.handleConfirmShop(0);

            // ── P2 Shop (arrows) ──
            int maxHeroes2 = 0;
            for (int i = 0; i < snap.heroCount; i++)
                if (snap.heroes[i].ownerId == 1) maxHeroes2++;
            int maxRows2 = 1 + maxHeroes2;

            if (IsKeyPressed(KEY_RIGHT))  inputs[1].shopCursorX = (inputs[1].shopCursorX + 1) % 3;
            if (IsKeyPressed(KEY_LEFT))   inputs[1].shopCursorX = (inputs[1].shopCursorX + 2) % 3;
            if (IsKeyPressed(KEY_DOWN))   inputs[1].shopCursorY = (inputs[1].shopCursorY + 1) % maxRows2;
            if (IsKeyPressed(KEY_UP))     inputs[1].shopCursorY = (inputs[1].shopCursorY + maxRows2 - 1) % maxRows2;

            if (IsKeyPressed(KEY_ENTER)) {
                if (inputs[1].shopCursorY == 0) {
                    int idx = -1, cnt = 0;
                    for (int i = 0; i < snap.shop.stockCount; i++) {
                        if (snap.shop.stock[i].category == ITEM_CATEGORY_GENERAL) {
                            if (cnt == inputs[1].shopCursorX) { idx = i; break; }
                            cnt++;
                        }
                    }
                    if (idx >= 0) game.handleBuyItem(1, idx, 0, 0);
                } else {
                    int idx = -1, cnt = 0;
                    for (int i = 0; i < snap.shop.stockCount; i++) {
                        if (snap.shop.stock[i].category == ITEM_CATEGORY_HERO) {
                            if (cnt == inputs[1].shopCursorX) { idx = i; break; }
                            cnt++;
                        }
                    }
                    if (idx >= 0) game.handleBuyItem(1, idx, inputs[1].shopCursorY - 1, -1);
                }
            }
            if (IsKeyPressed(KEY_PERIOD)) game.handleConfirmShop(1);
        }

        // ════════════════════════════════════════════════════════════════════
        //  UPDATE (fixed timestep 20 Hz)
        // ════════════════════════════════════════════════════════════════════
        accumulator += dt;
        while (accumulator >= 1.f/20.f) {
            accumulator -= 1.f/20.f;
            game.update(1.f/20.f);
        }

        // ── Build snapshot for rendering ───────────────────────────────────
        game.buildSnapshot(snap);

        // Detect phase transition to POSITIONING — reset cursors & hero selection
        if (snap.phase == PHASE_POSITIONING && prevPhase != PHASE_POSITIONING) {
            inputs[0].moveHeroIdx = 0;
            inputs[1].moveHeroIdx = 0;
            inputs[0].cursorX = 1; inputs[0].cursorY = 3;
            inputs[1].cursorX = 6; inputs[1].cursorY = 3;
        }
        prevPhase = snap.phase;

        // Sync authoritative game state back to PlayerInput (for auto-pick
        // scenarios where the game locked choices without input)
        for (int p = 0; p < 2; p++) {
            if (snap.trainerLocked[p] && inputs[p].trainerLocked < 0) {
                inputs[p].trainerLocked = snap.trainerChoice[p];
            }
            if (snap.herosLocked[p] && !inputs[p].herosLocked) {
                inputs[p].herosLocked = true;
                inputs[p].heroPicks.clear();
                for (int h = 0; h < 3; h++)
                    inputs[p].heroPicks.push_back(snap.heroPicks[p][h]);
            }
        }

        // ── Update hero visual state ───────────────────────────────────────
        float k = fminf(1.f, 15.f * dt);
        for (int i = 0; i < snap.heroCount; i++) {
            Vector2 target = cellCenter(snap.heroes[i].x, snap.heroes[i].y);
            if (!heroVis[i].active) {
                heroVis[i].pos    = target;
                heroVis[i].prevPos = target;
                heroVis[i].prevHp = snap.heroes[i].hp;
                heroVis[i].prevAlive = snap.heroes[i].alive;
                heroVis[i].prevUltActive = snap.heroes[i].ultActive;
                heroVis[i].active = true;
            } else {
                heroVis[i].prevPos = heroVis[i].pos;
                heroVis[i].pos.x += (target.x - heroVis[i].pos.x) * k;
                heroVis[i].pos.y += (target.y - heroVis[i].pos.y) * k;
            }
            if (heroVis[i].active && heroVis[i].prevHp != snap.heroes[i].hp) {
                int delta = (int)snap.heroes[i].hp - (int)heroVis[i].prevHp;
                if (delta < 0) {
                    spawnHitFlash(heroVis[i].pos);
                    spawnFloatingText(heroVis[i].pos, delta, RED);
                    for (int j = 0; j < snap.heroCount; j++) {
                        if (snap.heroes[j].ownerId == snap.heroes[i].ownerId) continue;
                        if (!snap.heroes[j].alive) continue;
                        int dx = abs((int)snap.heroes[j].x - (int)snap.heroes[i].x);
                        int dy = abs((int)snap.heroes[j].y - (int)snap.heroes[i].y);
                        if (dx <= 1 && dy <= 1) {
                            uint8_t arch = snap.heroes[j].archetype;
                            if (arch == ARCHETYPE_MAGE || arch == ARCHETYPE_SUPPORT) {
                                spawnRangedFx(heroVis[j].pos, heroVis[i].pos, arch);
                            } else {
                                spawnMeleeFx(heroVis[j].pos, heroVis[i].pos, arch);
                            }
                            break;
                        }
                    }
                } else {
                    spawnFloatingText(heroVis[i].pos, delta, GREEN);
                }
            }
            if (heroVis[i].active && heroVis[i].prevAlive && !snap.heroes[i].alive) {
                spawnDeathEffect(heroVis[i].pos);
            }
            if (heroVis[i].active && !heroVis[i].prevUltActive && snap.heroes[i].ultActive) {
                spawnUltimateEffect(heroVis[i].pos);
            }
            heroVis[i].prevHp = snap.heroes[i].hp;
            heroVis[i].prevAlive = snap.heroes[i].alive;
            heroVis[i].prevUltActive = snap.heroes[i].ultActive;
        }

        // ════════════════════════════════════════════════════════════════════
        //  RENDER
        // ════════════════════════════════════════════════════════════════════
        applyLayout(computeLayout());
        BeginDrawing();
        ClearBackground({12, 12, 26, 255});

        if (snap.phase == PHASE_SELECT) {
            DrawTexturePro(arena,
                {0, 0, (float)arena.width, (float)arena.height},
                {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {}, 0.f, WHITE);
            if (snap.selectSubphase == 0) {
                drawTrainerSelectMK(snap, g_localTrainerDefs.data(), (int)g_localTrainerDefs.size(), inputs[0], inputs[1]);
            } else {
                drawHeroSelectMK(snap, g_localTrainerDefs.data(), g_localHeroDefs.data(), (int)g_localHeroDefs.size(), inputs[0], inputs[1]);
            }
        }
        else if (snap.phase == PHASE_SHOP) {
            DrawTexturePro(arena,
                {0, 0, (float)arena.width, (float)arena.height},
                {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {}, 0.f, WHITE);
            drawShop(snap, inputs[0], inputs[1]);
        }
        else if (snap.phase == PHASE_VS_INTRO) {
            drawVSScreen(snap, 0);
        }
        else if (snap.phase == PHASE_POSITIONING ||
                 snap.phase == PHASE_BATTLE ||
                 snap.phase == PHASE_ROUND_END ||
                 snap.phase == PHASE_MATCH_END)
        {
            float sw = (float)GetScreenWidth();
            float sh = (float)GetScreenHeight();
            float arenaX = 220.f;
            float arenaY = 0.f;
            float arenaW = 1201.f;
            float arenaH = 880.f;

            // Black background for areas outside the arena
            DrawRectangle(0, 0, (int)sw, (int)sh, BLACK);

            // Arena drawn at its actual size, centered between side panels
            DrawTexturePro(arena,
                {0, 0, (float)arena.width, (float)arena.height},
                {arenaX, arenaY, arenaW, arenaH}, {}, 0.f, WHITE);

            drawBuffZones(snap);
            drawGrid();

            for (int i = 0; i < snap.heroCount; i++) {
                if (!snap.heroes[i].alive) continue;
                drawPedestal(heroVis[i].pos, snap.heroes[i].ownerId);
                float breathScale = 0.95f + 0.10f * (0.5f + 0.5f * sinf((float)GetTime() * PI));
                Vector2 delta = {
                    heroVis[i].pos.x - heroVis[i].prevPos.x,
                    heroVis[i].pos.y - heroVis[i].prevPos.y
                };
                float tiltAngle = (fabsf(delta.x) > 0.5f || fabsf(delta.y) > 0.5f)
                    ? atan2f(delta.y, delta.x) * 0.12f
                    : 0.f;
                drawHero(snap.heroes[i], heroVis[i].pos, 0, false, breathScale, tiltAngle);
            }

            if (snap.phase == PHASE_POSITIONING) {
                drawPlacementCursors(snap, inputs[0], inputs[1]);
            }

            updateAndDrawFxAnims(dt);
            updateAndDrawHitFlashes(dt);
            updateAndDrawFloatingTexts(dt);
            updateAndDrawVisualEffects(dt);

            drawTargetingVisuals(snap, inputs[0], inputs[1]);

            drawHUD(snap, 0);
            drawOverlays(snap, 0);
            drawSidePanels(snap, inputs[0], inputs[1]);
            drawHeroCards(snap, 0);
        }

        EndDrawing();
    }

    freeSelectionAssets((int)g_localTrainerDefs.size(), (int)g_localHeroDefs.size());
    unloadTextures();
    UnloadTexture(arena);
    CloseWindow();
    return 0;
}
