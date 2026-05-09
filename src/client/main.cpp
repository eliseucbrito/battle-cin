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
    { "Prof. Paulo",   "Estrutura de Dados",  {80, 160, 230, 255}, ABILITY_RALLY,       "Rally (+AD)",   "assets/trainer0.png" },
    { "Prof. Eliseu",  "Orient. a Objetos",   {230, 80, 130, 255}, ABILITY_SHIELD_WALL, "Shield (+ARM)", "assets/trainer1.png" },
    { "Prof. Fabio",   "Computacao Grafica",  {80, 200, 130, 255}, ABILITY_FRENZY,      "Frenzy (+AS)",  "" },
    { "Prof. Leila",   "Algoritmos",          {220, 180, 50, 255}, ABILITY_BATTLE_HEAL, "Heal (HP)",     "" },
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
    struct HeroVis { Vector2 pos; uint16_t prevHp; bool active; };
    HeroVis heroVis[MAX_HEROES_TOTAL];
    for (int i = 0; i < MAX_HEROES_TOTAL; i++) heroVis[i].active = false;

    int draggingHeroIdx = -1;

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

            // Transition: free selection assets and move to PLAYING
            if ((int)heroPicks.size() == MAX_HEROES_SIDE) {
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
                    heroVis[i].active = true;
                } else {
                    heroVis[i].pos.x += (target.x - heroVis[i].pos.x) * k;
                    heroVis[i].pos.y += (target.y - heroVis[i].pos.y) * k;
                }
                heroVis[i].prevHp = snap.heroes[i].hp;
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
                drawHero(snap.heroes[i], heroVis[i].pos, myId, (draggingHeroIdx == i));
            }

            drawHUD(snap, myId);
            drawOverlays(snap, myId);
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
