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

#include "../../include/protocol.h"
#include "renderer.h"

int main(int argc, char *argv[])
{
    int myId = 0;
    if (argc >= 2) myId = atoi(argv[1]) & 1;

    // ── UDP socket (non-blocking) ─────────────────────────────────────────
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in serverAddr{};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port        = htons(SERVER_PORT);

    // ── Window ────────────────────────────────────────────────────────────
    char title[64]; snprintf(title, sizeof(title), "Battle-CIn Arena — Treinador %d", myId);
    InitWindow(936, 684, title);
    SetTargetFPS(60);

    Texture2D arena = LoadTexture("assets/arena.png");

    GameSnapshot snap{};
    snap.phase = PHASE_WAITING;
    snap.roundWinner = 0xFF;
    snap.matchWinner = 0xFF;

    // Visual state for smoothing
    struct HeroVis {
        Vector2 pos;
        uint16_t prevHp;
        bool active;
    } heroVis[MAX_HEROES_TOTAL];
    for(int i=0; i<MAX_HEROES_TOTAL; i++) { heroVis[i].active = false; }

    int draggingHeroIdx = -1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ── Input: Ability (Q) ────────────────────────────────────────────
        if (IsKeyPressed(KEY_Q)) {
            InputPacket inp{};
            inp.playerId = (uint8_t)myId;
            inp.type = INPUT_USE_ABILITY;
            sendto(sock, &inp, sizeof(inp), 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
        }

        // ── Networking ────────────────────────────────────────────────────
        {
            InputPacket heart{};
            heart.playerId = (uint8_t)myId;
            heart.type = INPUT_HEARTBEAT;
            sendto(sock, &heart, sizeof(heart), 0, (sockaddr *)&serverAddr, sizeof(serverAddr));

            GameSnapshot buf{};
            ssize_t bytes;
            while ((bytes = recvfrom(sock, &buf, sizeof(buf), 0, nullptr, nullptr)) > 0) {
                if (bytes == (ssize_t)sizeof(GameSnapshot)) snap = buf;
            }
        }

        // ── Visual Update ─────────────────────────────────────────────────
        float k = fminf(1.f, 15.f * dt);
        for (int i = 0; i < snap.heroCount; i++) {
            const auto& hs = snap.heroes[i];
            Vector2 target = cellCenter(hs.x, hs.y);
            if (!heroVis[i].active) {
                heroVis[i].pos = target;
                heroVis[i].active = true;
            } else {
                heroVis[i].pos.x += (target.x - heroVis[i].pos.x) * k;
                heroVis[i].pos.y += (target.y - heroVis[i].pos.y) * k;
            }
            // Could spawn hit effects here using hs.hp vs heroVis[i].prevHp
            heroVis[i].prevHp = hs.hp;
        }

        // ── Drag & Drop (Positioning) ─────────────────────────────────────
        if (snap.phase == PHASE_POSITIONING) {
            Vector2 mouse = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                for (int i = 0; i < snap.heroCount; i++) {
                    if (snap.heroes[i].ownerId == (uint8_t)myId && snap.heroes[i].alive) {
                        float d = sqrtf(powf(mouse.x - heroVis[i].pos.x, 2) + powf(mouse.y - heroVis[i].pos.y, 2));
                        if (d < CELLW * 0.4f) { draggingHeroIdx = i; break; }
                    }
                }
            }

            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && draggingHeroIdx != -1) {
                heroVis[draggingHeroIdx].pos = mouse;
            }

            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && draggingHeroIdx != -1) {
                int mx = (int)((mouse.x - GX) / CELLW);
                int my = (int)((mouse.y - GY) / CELLH);
                if (mx >= 0 && mx < GRID_COLS && my >= 0 && my < GRID_ROWS) {
                    // Send place command
                    InputPacket inp{};
                    inp.playerId = (uint8_t)myId;
                    inp.type = INPUT_PLACE;
                    inp.placeX = (uint8_t)mx;
                    inp.placeY = (uint8_t)my;
                    // Find hero index relative to trainer's roster
                    int localIdx = 0;
                    for(int j=0; j<draggingHeroIdx; j++) if(snap.heroes[j].ownerId == (uint8_t)myId) localIdx++;
                    inp.heroIndex = (uint8_t)localIdx;
                    sendto(sock, &inp, sizeof(inp), 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
                }
                draggingHeroIdx = -1;
            }
        }

        // ── Render ────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);
        
        // Background
        DrawTexturePro(arena, {0,0,(float)arena.width, (float)arena.height}, {0,0,936,684}, {0,0}, 0.f, WHITE);

        drawBuffZones(snap);
        drawGrid();

        // Highlight valid deployment zones if dragging
        if (draggingHeroIdx != -1 && snap.phase == PHASE_POSITIONING) {
            uint8_t arch = snap.heroes[draggingHeroIdx].archetype;
            bool isLeft = (myId == 0);
            Color hl = { 0, 255, 0, 40 };
            for (int r = 0; r < GRID_ROWS; r++) {
                for (int c = 0; c < GRID_COLS; c++) {
                    bool valid = false;
                    if (isLeft && c <= 3) valid = true;
                    if (!isLeft && c >= 4) valid = true;
                    
                    if (valid) {
                        if (arch == ARCHETYPE_TANK) {
                            valid = (isLeft ? (c == 3) : (c == 4));
                        } else if (arch == ARCHETYPE_FIGHTER) {
                            valid = (isLeft ? (c >= 2) : (c <= 5));
                        } else if (arch == ARCHETYPE_ASSASSIN) {
                            valid = (isLeft ? (c >= 2) : (c <= 5)) && (r <= 1 || r >= 6);
                        } else if (arch == ARCHETYPE_MAGE || arch == ARCHETYPE_SUPPORT) {
                            valid = (isLeft ? (c <= 1) : (c >= 6));
                        }
                    }
                    if (valid) {
                        DrawRectangleRec(cellRect(c, r), hl);
                        DrawRectangleLinesEx(cellRect(c, r), 2, {0, 255, 0, 100});
                    }
                }
            }
        }

        for (int i = 0; i < snap.heroCount; i++) {
            if (!snap.heroes[i].alive) continue;
            drawHero(snap.heroes[i], heroVis[i].pos, myId, (draggingHeroIdx == i));
        }

        drawHUD(snap, myId);
        drawOverlays(snap, myId);

        EndDrawing();
    }

    UnloadTexture(arena);
    // unloadTextures(); // in renderer.cpp
    CloseWindow();
    return 0;
}
