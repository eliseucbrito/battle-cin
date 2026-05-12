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
#include "renderer.h"

// ─── Client-side Trainer display data ────────────────────────────────────────
static const int N_TRAINERS_LOCAL = 2;
static const TrainerDef TRAINERS[] = {
    { "Prof. Paulo",   "Estrutura de Dados",  {80, 160, 230, 255}, ABILITY_RALLY,       "Rally (+AD)",   "assets/trainer0.png" },
    { "Prof. Eliseu",  "Orient. a Objetos",   {230, 80, 130, 255}, ABILITY_SHIELD_WALL, "Shield (+ARM)", "assets/trainer1.png" },
};

static const int N_HEROES_LOCAL = 10;
static const HeroDef HEROES[] = {
    { "O Construto de Busca",     ARCHETYPE_TANK,     0, "Tank",     350, 15, 18, "assets/heroes/O_Construto_de_Busca.png"    },
    { "O Guardiao dos Discos",    ARCHETYPE_FIGHTER,  0, "Fighter",  280, 22, 10, "assets/heroes/O_Guardiao_dos_Discos.png"   },
    { "O Mestre Parser",          ARCHETYPE_MAGE,     0, "Mage",     200, 35,  5, "assets/heroes/O_Mestre_Parser.png"         },
    { "O Cientista Polarizado",   ARCHETYPE_ASSASSIN, 0, "Assassin", 220, 32,  3, "assets/heroes/O_Cientista_Polarizado.png"  },
    { "O Chip-Mestre",            ARCHETYPE_SUPPORT,  0, "Support",  240, 12, 10, "assets/heroes/O_Chip-Mestre.png"           },
    { "A Burocrata do UML",       ARCHETYPE_TANK,     1, "Tank",     360, 13, 20, "assets/heroes/A_Burocrata_do_UML.png"      },
    { "O Filosofo do Dilema",     ARCHETYPE_FIGHTER,  1, "Fighter",  270, 24, 12, "assets/heroes/O_Filosofo_do_Dilema.png"    },
    { "O Artista Vectorial",      ARCHETYPE_MAGE,     1, "Mage",     190, 38,  4, "assets/heroes/O_Artista_Vectorial.png"     },
    { "O Inspetor Flaky",         ARCHETYPE_ASSASSIN, 1, "Assassin", 215, 30,  2, "assets/heroes/O_Inspetor_Flaky.png"        },
    { "O Treinador Python",       ARCHETYPE_SUPPORT,  1, "Support",  250, 14,  8, "assets/heroes/O_Treinador_Python.png"      },
};

static int heroFilteredToGlobal(int trainerIdx, int cursor) {
    return trainerIdx * 5 + cursor;
}

static int heroesForTrainer(int trainerIdx) {
    int count = 0;
    for (int i = 0; i < N_HEROES_LOCAL; i++)
        if (HEROES[i].trainerIndex == (uint8_t)trainerIdx) count++;
    return count;
}

// ─── main ────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    srand((unsigned)time(nullptr));

    bool soloMode = (argc >= 2 && strcmp(argv[1], "--solo") == 0);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Battle-CIn");
    SetTargetFPS(60);

    Game game;
    game.registerPlayerLocal(0);
    game.registerPlayerLocal(1);

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

    initSelectionAssets(TRAINERS, N_TRAINERS_LOCAL, HEROES, N_HEROES_LOCAL);
    initFxSystem();

    float accumulator = 0.f;
    uint8_t prevPhase = PHASE_SELECT;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ════════════════════════════════════════════════════════════════════
        //  INPUT
        // ════════════════════════════════════════════════════════════════════

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        if (snap.phase == PHASE_SELECT) {
            if (snap.selectSubphase == 0) {
                // ── Trainer Select ─────────────────────────────────────────
                // P1 (esquerda): A/D + Space
                if (IsKeyPressed(KEY_D))      inputs[0].trainerCursor = (inputs[0].trainerCursor + 1) % N_TRAINERS_LOCAL;
                if (IsKeyPressed(KEY_A))      inputs[0].trainerCursor = (inputs[0].trainerCursor + N_TRAINERS_LOCAL - 1) % N_TRAINERS_LOCAL;
                if (IsKeyPressed(KEY_SPACE) && inputs[0].trainerLocked < 0) {
                    inputs[0].trainerLocked = inputs[0].trainerCursor;
                    game.handleLocalTrainerLock(0, (uint8_t)inputs[0].trainerLocked);
                }

                // P2 (direita): Arrow keys + Enter
                if (IsKeyPressed(KEY_RIGHT)) inputs[1].trainerCursor = (inputs[1].trainerCursor + 1) % N_TRAINERS_LOCAL;
                if (IsKeyPressed(KEY_LEFT))  inputs[1].trainerCursor = (inputs[1].trainerCursor + N_TRAINERS_LOCAL - 1) % N_TRAINERS_LOCAL;
                if (IsKeyPressed(KEY_ENTER) && inputs[1].trainerLocked < 0) {
                    inputs[1].trainerLocked = inputs[1].trainerCursor;
                    game.handleLocalTrainerLock(1, (uint8_t)inputs[1].trainerLocked);
                }
            } else {
                // ── Hero Select ───────────────────────────────────────────
                int nHerosPerTrainer = heroesForTrainer(snap.trainerChoice[0]);

                // P1 (esquerda): A/D + Space
                {
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
                {
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
                drawTrainerSelectMK(snap, TRAINERS, N_TRAINERS_LOCAL, inputs[0], inputs[1]);
            } else {
                drawHeroSelectMK(snap, TRAINERS, HEROES, N_HEROES_LOCAL, inputs[0], inputs[1]);
            }
        }
        else if (snap.phase == PHASE_VS_INTRO) {
            drawVSScreen(snap, 0);
        }
        else if (snap.phase == PHASE_POSITIONING ||
                 snap.phase == PHASE_BATTLE ||
                 snap.phase == PHASE_ROUND_END ||
                 snap.phase == PHASE_MATCH_END)
        {
            DrawTexturePro(arena,
                {0, 0, (float)arena.width, (float)arena.height},
                {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {}, 0.f, WHITE);

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

            drawHUD(snap, 0);
            drawOverlays(snap, 0);
            drawSidePanels(snap, 0);
            drawHeroCards(snap, 0);
        }

        EndDrawing();
    }

    freeSelectionAssets(N_TRAINERS_LOCAL, N_HEROES_LOCAL);
    unloadTextures();
    UnloadTexture(arena);
    CloseWindow();
    return 0;
}
