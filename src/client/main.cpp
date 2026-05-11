// Battle-CIn — Client Entry Point
// OOP Disciplines: Herança, Polimorfismo, Encapsulamento, Smart Pointers
//
// ClientPhase state machine:
//   TRAINER_SELECT → HERO_SELECT → PLAYING (UDP networking loop)
//
// The selection phases are purely local (no server connection needed).
// After selection, the client connects to the authoritative server and enters
// the existing networking loop using the existing protocol and renderer.

#include "raylib.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <vector>
#include <algorithm>

#include "../../include/protocol.h"
#include "renderer.h"

// ─── Client-side State Machine ───────────────────────────────────────────────
// Separate from PHASE_* in protocol.h (those are server-side battle phases).
// This enum class controls the CLIENT UI flow, which happens before networking.
enum class ClientPhase {
    TRAINER_SELECT,   // Local: player picks one of N trainers
    HERO_SELECT,      // Local: player picks 3 heroes from the pool
    PLAYING           // Networked: existing authoritative server loop
};

// ─── Trainer display data (client-side only) ─────────────────────────────────
// Full Trainer OOP objects live on the server (src/server/game.cpp).
// Here we keep lightweight display-only records for the selection UI.
static const TrainerDef TRAINERS[] = {
    //  name              discipline             color               abilityType        abilityName     portraitPath
    { "Prof. Juliano",   "Introducao a Programacao", {0, 255, 255, 255}, ABILITY_RALLY,       "Rally (+AD)",   "assets/trainer0.png" },
    { "Prof. Valeria",   "Banco de Dados",          {128, 0, 128, 255}, ABILITY_SHIELD_WALL, "Shield (+ARM)", "assets/trainer1.png" },
    { "Prof. Francisco", "Estrutura de Dados",      {0, 255, 0, 255},   ABILITY_FRENZY,      "Frenzy (+AS)",  "" },
    { "Prof. David",     "Redes de Computadores",   {255, 165, 0, 255}, ABILITY_BATTLE_HEAL, "Heal (HP)",    "" },
};
static constexpr int N_TRAINERS = 4;

// ─── Hero pool display data (client-side only) ────────────────────────────────
// Actual polymorphic Hero objects (TankHero, MageHero, etc.) are created by
// HeroFactory on the server. These records are just for rendering the grid.
static const HeroDef HEROES[] = {
    // name                        archetype          className    hp   ad  arm  assetPath
    { "O Construto de Busca",    ARCHETYPE_TANK,     "Tank",     350, 15, 18,  "assets/heroes/O_Construto_de_Busca.png"    },
    { "O Guardiao dos Discos",   ARCHETYPE_FIGHTER,  "Fighter",  280, 22, 10,  "assets/heroes/O_Guardiao_dos_Discos.png"   },
    { "O Mestre Parser",         ARCHETYPE_MAGE,     "Mage",     200, 35,  5,  "assets/heroes/O_Mestre_Parser.png"         },
    { "O Cientista Polarizado",  ARCHETYPE_ASSASSIN, "Assassin", 220, 32,  3,  "assets/heroes/O_Cientista_Polarizado.png"  },
    { "O Chip-Mestre",           ARCHETYPE_SUPPORT,  "Support",  240, 12, 10,  "assets/heroes/O_Chip-Mestre.png"           },
    { "A Burocrata do UML",      ARCHETYPE_TANK,     "Tank",     360, 13, 20,  "assets/heroes/A_Burocrata_do_UML.png"      },
    { "O Artista Vectorial",     ARCHETYPE_MAGE,     "Mage",     190, 38,  4,  "assets/heroes/O_Artista_Vectorial.png"     },
    { "O Inspetor Flaky",        ARCHETYPE_ASSASSIN, "Assassin", 215, 30,  2,  "assets/heroes/O_Inspetor_Flaky.png"        },
    { "O Treinador Python",      ARCHETYPE_SUPPORT,  "Support",  250, 14,  8,  "assets/heroes/O_Treinador_Python.png"      },
    { "O Filosofo do Dilema",    ARCHETYPE_FIGHTER,  "Fighter",  270, 24, 12,  "assets/heroes/O_Filosofo_do_Dilema.png"    },
};
static constexpr int N_HEROES        = 10;
static constexpr int HERO_GRID_COLS  = 5;
static constexpr int HERO_GRID_ROWS  = 2;

// ─── main ────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    // Determine player ID from command-line argument (0 or 1)
    int myId = 0;
    if (argc >= 2) myId = atoi(argv[1]) & 1;

    // ── Window ────────────────────────────────────────────────────────────────
    char title[64];
    snprintf(title, sizeof(title), "Battle-CIn — Instancia %d", myId);
    InitWindow(936, 684, title);
    SetTargetFPS(60);

    // ── Selection phase state ─────────────────────────────────────────────────
    ClientPhase      clientPhase   = ClientPhase::TRAINER_SELECT;
    int              selCursor     = 0;   // flat index into current grid
    int              selTrainerIdx = -1;  // which trainer was picked
    std::vector<int> heroPicks;           // indices into HEROES[]

    // Load portrait textures for selection screens (freed before PLAYING)
    initSelectionAssets(TRAINERS, N_TRAINERS, HEROES, N_HEROES);

    // ── Networking (set up now; heartbeat only starts in PLAYING) ─────────────
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);
    sockaddr_in serverAddr{};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port        = htons(SERVER_PORT);

    // ── Battle-phase state (used only in PLAYING) ─────────────────────────────
    Texture2D arena = LoadTexture("assets/arena.png");

    GameSnapshot snap{};
    snap.phase       = PHASE_WAITING;
    snap.roundWinner = 0xFF;
    snap.matchWinner = 0xFF;

    // Visual interpolation state per hero (smooth movement)
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
        heroVis[i].prevPos = {0, 0};
        heroVis[i].prevAlive = true;
        heroVis[i].prevUltActive = false;
    }

    int draggingHeroIdx = -1;

    // Targeting state (during battle)
    int  targetingHeroIdx = -1;    // hero being dragged for targeting
    bool isTargeting      = false; // true while dragging targeting arrow

    // ── Game Loop ─────────────────────────────────────────────────────────────
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ════════════════════════════════════════════════════════════════════
        //  INPUT
        // ════════════════════════════════════════════════════════════════════

        if (clientPhase == ClientPhase::TRAINER_SELECT) {
            // Both WASD and Arrow Keys work (one instance = one player)
            if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))
                selCursor = (selCursor + 1) % N_TRAINERS;
            if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))
                selCursor = (selCursor + N_TRAINERS - 1) % N_TRAINERS;

            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                selTrainerIdx = selCursor;
                selCursor     = 0;         // reset cursor for hero grid
                heroPicks.clear();
                clientPhase   = ClientPhase::HERO_SELECT;
            }
        }

        else if (clientPhase == ClientPhase::HERO_SELECT) {
            int row = selCursor / HERO_GRID_COLS;
            int col = selCursor % HERO_GRID_COLS;

            if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))
                if (col < HERO_GRID_COLS - 1) selCursor++;
            if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))
                if (col > 0) selCursor--;
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
                if (row < HERO_GRID_ROWS - 1) selCursor += HERO_GRID_COLS;
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
                if (row > 0) selCursor -= HERO_GRID_COLS;

            // Toggle pick (prevent duplicates via std::find — O(n), fine for n=3)
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                auto it = std::find(heroPicks.begin(), heroPicks.end(), selCursor);
                if (it != heroPicks.end()) {
                    heroPicks.erase(it);           // deselect
                } else if ((int)heroPicks.size() < 3) {
                    heroPicks.push_back(selCursor); // select
                }
            }
            // Undo last pick
            if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_BACKSPACE))
                if (!heroPicks.empty()) heroPicks.pop_back();

            // Transition: free selection assets, send selection to server, move to PLAYING
            if ((int)heroPicks.size() == MAX_HEROES_SIDE) {
                SelectionPacket sel{};
                sel.playerId     = (uint8_t)myId;
                sel.type         = INPUT_SELECT;
                sel.trainerIndex = (uint8_t)selTrainerIdx;
                for (int i = 0; i < MAX_HEROES_SIDE; i++)
                    sel.heroIndices[i] = (uint8_t)heroPicks[i];
                sendto(sock, &sel, sizeof(sel), 0,
                       (sockaddr*)&serverAddr, sizeof(serverAddr));

                freeSelectionAssets(N_TRAINERS, N_HEROES);
                clientPhase = ClientPhase::PLAYING;
            }
        }

        else /* ClientPhase::PLAYING */ {
            // Trainer ability (Q key)
            if (IsKeyPressed(KEY_Q)) {
                InputPacket inp{};
                inp.playerId = (uint8_t)myId;
                inp.type     = INPUT_USE_ABILITY;
                sendto(sock, &inp, sizeof(inp), 0,
                       (sockaddr*)&serverAddr, sizeof(serverAddr));
            }

            // Heartbeat + receive snapshot
            {
                InputPacket heart{};
                heart.playerId = (uint8_t)myId;
                heart.type     = INPUT_HEARTBEAT;
                sendto(sock, &heart, sizeof(heart), 0,
                       (sockaddr*)&serverAddr, sizeof(serverAddr));

                GameSnapshot buf{};
                ssize_t bytes;
                while ((bytes = recvfrom(sock, &buf, sizeof(buf), 0,
                                         nullptr, nullptr)) > 0)
                    if (bytes == (ssize_t)sizeof(GameSnapshot)) snap = buf;
            }

            // Visual position interpolation (smooth movement)
            float k = fminf(1.f, 15.f * dt);
            for (int i = 0; i < snap.heroCount; i++) {
                Vector2 target = cellCenter(snap.heroes[i].x, snap.heroes[i].y);
                if (!heroVis[i].active) {
                    heroVis[i].pos    = target;
                    heroVis[i].prevPos = target;
                    heroVis[i].active = true;
                } else {
                    heroVis[i].prevPos = heroVis[i].pos;
                    heroVis[i].pos.x += (target.x - heroVis[i].pos.x) * k;
                    heroVis[i].pos.y += (target.y - heroVis[i].pos.y) * k;
                }
                // Detect HP changes for floating text + attack visuals
                if (heroVis[i].active && heroVis[i].prevHp != snap.heroes[i].hp) {
                    int delta = (int)snap.heroes[i].hp - (int)heroVis[i].prevHp;
                    if (delta < 0) {
                        // Damage taken: spawn hit flash + find attacker for animation
                        spawnHitFlash(heroVis[i].pos);
                        spawnFloatingText(heroVis[i].pos, delta, RED);
                        // Find an adjacent enemy to attribute the attack
                        for (int j = 0; j < snap.heroCount; j++) {
                            if (snap.heroes[j].ownerId == snap.heroes[i].ownerId) continue;
                            if (!snap.heroes[j].alive) continue;
                            int dx = abs((int)snap.heroes[j].x - (int)snap.heroes[i].x);
                            int dy = abs((int)snap.heroes[j].y - (int)snap.heroes[i].y);
                            if (dx <= 1 && dy <= 1) {
                                uint8_t arch = snap.heroes[j].archetype;
                                if (arch == ARCHETYPE_MAGE || arch == ARCHETYPE_SUPPORT) {
                                    spawnProjectile(heroVis[j].pos, heroVis[i].pos, arch);
                                } else {
                                    spawnAttackAnim(heroVis[j].pos, heroVis[i].pos);
                                }
                                break;
                            }
                        }
                    } else {
                        spawnFloatingText(heroVis[i].pos, delta, GREEN);
                    }
                }
                // Detect death transition
                if (heroVis[i].active && heroVis[i].prevAlive && !snap.heroes[i].alive) {
                    spawnDeathEffect(heroVis[i].pos);
                }
                // Detect ultimate activation
                if (heroVis[i].active && !heroVis[i].prevUltActive && snap.heroes[i].ultActive) {
                    spawnUltimateEffect(heroVis[i].pos);
                }
                heroVis[i].prevHp = snap.heroes[i].hp;
                heroVis[i].prevAlive = snap.heroes[i].alive;
                heroVis[i].prevUltActive = snap.heroes[i].ultActive;
            }

            // Drag & drop for deployment positioning
            if (snap.phase == PHASE_POSITIONING) {
                Vector2 mouse = GetMousePosition();

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    for (int i = 0; i < snap.heroCount; i++) {
                        if (snap.heroes[i].ownerId == (uint8_t)myId && snap.heroes[i].alive) {
                            float d = sqrtf(
                                powf(mouse.x - heroVis[i].pos.x, 2) +
                                powf(mouse.y - heroVis[i].pos.y, 2));
                            if (d < CELLW * 0.4f) { draggingHeroIdx = i; break; }
                        }
                    }
                }
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && draggingHeroIdx != -1)
                    heroVis[draggingHeroIdx].pos = mouse;

                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && draggingHeroIdx != -1) {
                    Vector2 m = GetMousePosition();
                    int mx = (int)((m.x - GX) / CELLW);
                    int my = (int)((m.y - GY) / CELLH);
                    if (mx >= 0 && mx < GRID_COLS && my >= 0 && my < GRID_ROWS) {
                        InputPacket inp{};
                        inp.playerId = (uint8_t)myId;
                        inp.type     = INPUT_PLACE;
                        inp.placeX   = (uint8_t)mx;
                        inp.placeY   = (uint8_t)my;
                        int localIdx = 0;
                        for (int j = 0; j < draggingHeroIdx; j++)
                            if (snap.heroes[j].ownerId == (uint8_t)myId) localIdx++;
                        inp.heroIndex = (uint8_t)localIdx;
                        sendto(sock, &inp, sizeof(inp), 0,
                               (sockaddr*)&serverAddr, sizeof(serverAddr));
                    }
                    draggingHeroIdx = -1;
                }
            }

            // ── Targeting (during battle) ────────────────────────────────────
            if (snap.phase == PHASE_BATTLE) {
                Vector2 mouse = GetMousePosition();

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    for (int i = 0; i < snap.heroCount; i++) {
                        if (snap.heroes[i].ownerId == (uint8_t)myId && snap.heroes[i].alive) {
                            float d = sqrtf(
                                powf(mouse.x - heroVis[i].pos.x, 2) +
                                powf(mouse.y - heroVis[i].pos.y, 2));
                            if (d < CELLW * 0.4f) {
                                targetingHeroIdx = i;
                                isTargeting = true;
                                break;
                            }
                        }
                    }
                }

                if (isTargeting && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    // Continue dragging - arrow is drawn in render
                }

                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && isTargeting) {
                    // Find which enemy we dropped on
                    int localHeroIdx = -1;
                    int myHeroCount = 0;
                    for (int i = 0; i < snap.heroCount; i++) {
                        if (snap.heroes[i].ownerId == (uint8_t)myId) {
                            if (i == targetingHeroIdx) {
                                localHeroIdx = myHeroCount;
                                break;
                            }
                            myHeroCount++;
                        }
                    }

                    if (localHeroIdx >= 0) {
                        for (int i = 0; i < snap.heroCount; i++) {
                            if (snap.heroes[i].ownerId == (uint8_t)myId) continue;
                            if (!snap.heroes[i].alive) continue;

                            float d = sqrtf(
                                powf(mouse.x - heroVis[i].pos.x, 2) +
                                powf(mouse.y - heroVis[i].pos.y, 2));
                            if (d < CELLW * 0.5f) {
                                // Check adjacency
                                int dx = abs((int)snap.heroes[targetingHeroIdx].x - (int)snap.heroes[i].x);
                                int dy = abs((int)snap.heroes[targetingHeroIdx].y - (int)snap.heroes[i].y);
                                if (dx <= 1 && dy <= 1) {
                                    // Convert global enemy index to local enemy index (0-2)
                                    int localTargetIdx = 0;
                                    for (int j = 0; j < i; j++) {
                                        if (snap.heroes[j].ownerId != (uint8_t)myId)
                                            localTargetIdx++;
                                    }
                                    TargetPacket tgt{};
                                    tgt.playerId    = (uint8_t)myId;
                                    tgt.type        = INPUT_TARGET;
                                    tgt.heroIndex   = (uint8_t)localHeroIdx;
                                    tgt.targetIndex = (uint8_t)localTargetIdx;
                                    sendto(sock, &tgt, sizeof(tgt), 0,
                                           (sockaddr*)&serverAddr, sizeof(serverAddr));
                                }
                                break;
                            }
                        }
                    }
                    targetingHeroIdx = -1;
                    isTargeting = false;
                }
            }
        }

        // ════════════════════════════════════════════════════════════════════
        //  RENDER
        // ════════════════════════════════════════════════════════════════════
        BeginDrawing();
        ClearBackground({12, 12, 26, 255});

        if (clientPhase == ClientPhase::TRAINER_SELECT) {
            drawTrainerSelect(TRAINERS, N_TRAINERS, selCursor, selTrainerIdx, myId);
        }
        else if (clientPhase == ClientPhase::HERO_SELECT) {
            drawHeroSelect(TRAINERS[selTrainerIdx],
                           HEROES, N_HEROES,
                           HERO_GRID_COLS,
                           selCursor, heroPicks, myId);
        }
        else /* PLAYING */ {
            if (snap.phase == PHASE_VS_INTRO) {
                drawVSScreen(snap, myId);
            } else {
                // Arena background
                DrawTexturePro(arena,
                    {0, 0, (float)arena.width, (float)arena.height},
                    {0, 0, 936, 684}, {}, 0.f, WHITE);

                // Deployment zone highlights (only during positioning)
                if (snap.phase == PHASE_POSITIONING && draggingHeroIdx != -1) {
                    uint8_t arch   = snap.heroes[draggingHeroIdx].archetype;
                    bool    isLeft = (myId == 0);
                    Color   hl     = {0, 255, 0, 40};
                    for (int r = 0; r < GRID_ROWS; r++) {
                        for (int c = 0; c < GRID_COLS; c++) {
                            bool valid = isLeft ? (c <= 3) : (c >= 4);
                            if (valid) {
                                if      (arch == ARCHETYPE_TANK)
                                    valid = isLeft ? (c == 3) : (c == 4);
                                else if (arch == ARCHETYPE_FIGHTER)
                                    valid = isLeft ? (c >= 2) : (c <= 5);
                                else if (arch == ARCHETYPE_ASSASSIN)
                                    valid = (isLeft ? (c >= 2) : (c <= 5)) && (r <= 1 || r >= 6);
                                else if (arch == ARCHETYPE_MAGE || arch == ARCHETYPE_SUPPORT)
                                    valid = isLeft ? (c <= 1) : (c >= 6);
                            }
                            if (valid) {
                                DrawRectangleRec(cellRect(c, r), hl);
                                DrawRectangleLinesEx(cellRect(c, r), 2, {0, 255, 0, 100});
                            }
                        }
                    }
                }

                drawBuffZones(snap);
                drawGrid();

                for (int i = 0; i < snap.heroCount; i++) {
                    if (!snap.heroes[i].alive) continue;
                    // Pedestal (team-colored base)
                    drawPedestal(heroVis[i].pos, snap.heroes[i].ownerId);
                    // Idle breathing
                    float breathScale = 0.95f + 0.10f * (0.5f + 0.5f * sinf((float)GetTime() * PI));
                    // Move tilt: derive from delta position
                    Vector2 delta = {
                        heroVis[i].pos.x - heroVis[i].prevPos.x,
                        heroVis[i].pos.y - heroVis[i].prevPos.y
                    };
                    float tiltAngle = (fabsf(delta.x) > 0.5f || fabsf(delta.y) > 0.5f)
                        ? atan2f(delta.y, delta.x) * 0.12f
                        : 0.f;
                    drawHero(snap.heroes[i], heroVis[i].pos, myId,
                             (draggingHeroIdx == i), breathScale, tiltAngle);
                }

                // Draw existing target focus arrows
                for (int i = 0; i < snap.heroCount; i++) {
                    if (!snap.heroes[i].alive) continue;
                    if (snap.heroes[i].ownerId != (uint8_t)myId) continue;
                    if (snap.heroes[i].targetFocus >= 0) {
                        int tf = snap.heroes[i].targetFocus;
                        // Find global index of focused enemy
                        int enemyCount = 0;
                        for (int j = 0; j < snap.heroCount; j++) {
                            if (snap.heroes[j].ownerId != (uint8_t)myId) {
                                if (enemyCount == tf && snap.heroes[j].alive) {
                                    drawTargetArrow(heroVis[i].pos, heroVis[j].pos);
                                    drawTargetHighlight(heroVis[j].pos, CELLW * 0.42f, RED);
                                    break;
                                }
                                enemyCount++;
                            }
                        }
                    }
                }

                // Draw targeting arrow being dragged
                if (isTargeting && targetingHeroIdx >= 0) {
                    Vector2 mouse = GetMousePosition();
                    drawTargetArrow(heroVis[targetingHeroIdx].pos, mouse);
                    drawAdjacentEnemyHighlights(snap, myId, targetingHeroIdx);
                }

                updateAndDrawAttackAnims(dt);
                updateAndDrawProjectiles(dt);
                updateAndDrawHitFlashes(dt);
                updateAndDrawFloatingTexts(dt);
                updateAndDrawVisualEffects(dt);

                drawHUD(snap, myId);
                drawOverlays(snap, myId);
            }
        }

        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    unloadTextures();
    UnloadTexture(arena);
    close(sock);
    CloseWindow();
    return 0;
}
