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

#include "../protocol.h"

// ── Grid layout ───────────────────────────────────────────────────────────────
static constexpr float GX    = 218.f;
static constexpr float GY    =  92.f;
static constexpr float GW    = 500.f;
static constexpr float GH    = 500.f;
static constexpr float CELLW = GW / GRID_COLS;
static constexpr float CELLH = GH / GRID_ROWS;

static Vector2 cellCenter(int cx, int cy)
{
    return { GX + cx * CELLW + CELLW * 0.5f, GY + cy * CELLH + CELLH * 0.5f };
}

static Rectangle cellRect(int cx, int cy)
{
    return { GX + cx * CELLW, GY + cy * CELLH, CELLW, CELLH };
}

// ── Colors ────────────────────────────────────────────────────────────────────
static const Color kPlayer[2] = { BLUE, RED };

static Color buffFill(uint8_t type) {
    switch (type) {
        case BUFF_AD:  return { 255, 140,   0, 70  };   // orange
        case BUFF_HP:  return {   0, 200,  80, 70  };   // green
        case BUFF_ARM: return {  50, 180, 255, 70  };   // sky blue
        default:       return BLANK;
    }
}
static Color buffRim(uint8_t type) {
    Color c = buffFill(type); c.a = 210; return c;
}
static const char *buffLabel[4] = { "", "AD", "HP", "ARM" };
static const char *buffDesc[4]  = { "", "AD +15", "HP +50", "ARM +10" };

// ── Hit ring effect ───────────────────────────────────────────────────────────
struct HitRing {
    float x, y, elapsed, duration;
    Color color;
};
static HitRing hitRings[8];
static int     hitRingCount = 0;

static void spawnHitRing(float x, float y, Color c)
{
    if (hitRingCount >= 8) hitRingCount = 0;
    hitRings[hitRingCount++] = { x, y, 0.f, 0.40f, c };
}

static void updateDrawHitRings(float dt)
{
    int alive = 0;
    for (int i = 0; i < hitRingCount; i++) {
        HitRing &r = hitRings[i];
        r.elapsed += dt;
        if (r.elapsed >= r.duration) continue;
        hitRings[alive++] = r;

        float t     = r.elapsed / r.duration;          // 0 → 1
        float rad   = CELLW * 0.2f + CELLW * 0.55f * t; // expands
        Color c     = r.color;
        c.a         = (unsigned char)((1.f - t) * 220);
        DrawCircleLinesV({ r.x, r.y }, rad, c);
        DrawCircleLinesV({ r.x, r.y }, rad * 0.6f, c);
    }
    hitRingCount = alive;
}

// ── Damage popup ──────────────────────────────────────────────────────────────
struct DmgPop {
    float x, y, elapsed, duration;
    int   amount;
};
static DmgPop dmgPops[16];
static int    dmgPopCount = 0;

static void spawnDmgPop(float x, float y, int amount)
{
    if (dmgPopCount >= 16) dmgPopCount = 0;
    dmgPops[dmgPopCount++] = { x, y, 0.f, 0.85f, amount };
}

static void updateDrawDmgPops(float dt)
{
    int alive = 0;
    for (int i = 0; i < dmgPopCount; i++) {
        DmgPop &p = dmgPops[i];
        p.elapsed += dt;
        if (p.elapsed >= p.duration) continue;
        dmgPops[alive++] = p;

        float t    = p.elapsed / p.duration;
        float yOff = -55.f * t;            // floats up
        float alpha = (t < 0.6f) ? 1.f : (1.f - (t - 0.6f) / 0.4f);
        Color c    = { 255, 80, 0, (unsigned char)(alpha * 255) };
        char  txt[12];
        snprintf(txt, sizeof(txt), "-%d", p.amount);
        DrawText(txt,
                 (int)(p.x - MeasureText(txt, 18) * 0.5f),
                 (int)(p.y + yOff), 18, c);
    }
    dmgPopCount = alive;
}

// ── Arena / grid drawing ──────────────────────────────────────────────────────

static void drawGrid()
{
    Color c = { 255, 255, 255, 65 };
    for (int col = 0; col <= GRID_COLS; col++) {
        float x = GX + col * CELLW;
        DrawLineV({ x, GY }, { x, GY + GH }, c);
    }
    for (int row = 0; row <= GRID_ROWS; row++) {
        float y = GY + row * CELLH;
        DrawLineV({ GX, y }, { GX + GW, y }, c);
    }
    // North/south dividing line
    float midY = GY + 4 * CELLH;
    DrawLineEx({ GX, midY }, { GX + GW, midY }, 2.f, { 255, 255, 100, 160 });
}

static void drawBuffZones(const GameSnapshot &snap)
{
    for (int i = 0; i < snap.buffZoneCount; i++) {
        const BuffZoneInfo &bz = snap.buffZones[i];
        if (bz.x >= GRID_COLS || bz.y >= GRID_ROWS) continue;

        Rectangle r = cellRect(bz.x, bz.y);
        DrawRectangleRec(r, buffFill(bz.type));
        DrawRectangleLinesEx(r, 2.f, buffRim(bz.type));

        Vector2 ctr = cellCenter(bz.x, bz.y);
        const char *lbl = buffLabel[bz.type];
        DrawText(lbl, (int)(ctr.x - MeasureText(lbl, 11) * 0.5f),
                 (int)(ctr.y - 6), 11, WHITE);
    }
}

// Dims the half the player cannot position in
static void drawPositioningRestriction(int myId)
{
    // myId==0 → north (rows 0-3) is mine → dim south (rows 4-7)
    // myId==1 → south (rows 4-7) is mine → dim north (rows 0-3)
    float dimY     = (myId == 0) ? GY + 4 * CELLH : GY;
    Rectangle dimR = { GX, dimY, GW, 4 * CELLH };
    DrawRectangleRec(dimR, { 0, 0, 0, 110 });
}

// Ghost cell highlight during drag
static void drawGhostCell(int cx, int cy, int playerId)
{
    Color c = kPlayer[playerId]; c.a = 80;
    DrawRectangleRec(cellRect(cx, cy), c);
    c.a = 200;
    DrawRectangleLinesEx(cellRect(cx, cy), 2.f, c);
}

// ── Unit rendering ────────────────────────────────────────────────────────────

static void drawUnit(Vector2 ctr, int id, const PlayerNetState &ps, bool isMe, bool dragging)
{
    float r = CELLW * 0.37f;

    if (dragging) {
        // Faded at original position while dragging
        Color c = kPlayer[id]; c.a = 80;
        DrawCircleV(ctr, r, c);
        DrawCircleLinesV(ctr, r, { 255,255,255,60 });
        return;
    }

    DrawCircleV({ ctr.x + 2, ctr.y + 2 }, r, { 0,0,0,90 }); // shadow
    DrawCircleV(ctr, r, kPlayer[id]);
    DrawCircleLinesV(ctr, r, isMe ? WHITE : LIGHTGRAY);

    // Buff glow
    if (ps.buff != BUFF_NONE) {
        Color gc = buffRim(ps.buff); gc.a = 180;
        DrawCircleLinesV(ctr, r + 3, gc);
        DrawCircleLinesV(ctr, r + 6, gc);
    }

    // HP bar
    float bw = CELLW * 0.86f, bh = 7.f;
    float bx = ctr.x - bw * 0.5f, by = ctr.y - r - 13.f;
    float pct = ps.maxHp > 0 ? (float)ps.hp / ps.maxHp : 0.f;
    DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, DARKGRAY);
    Color hc = pct > 0.5f ? GREEN : pct > 0.25f ? YELLOW : RED;
    DrawRectangle((int)bx, (int)by, (int)(bw * pct), (int)bh, hc);
    DrawRectangleLinesEx({ bx, by, bw, bh }, 1.f, { 255,255,255,100 });

    char hpTxt[12];
    snprintf(hpTxt, sizeof(hpTxt), "%d", ps.hp);
    DrawText(hpTxt, (int)(ctr.x - MeasureText(hpTxt,9)*0.5f), (int)(by-11), 9, WHITE);

    char lbl[4] = { 'P', (char)('0'+id), '\0' };
    DrawText(lbl, (int)(ctr.x - MeasureText(lbl,13)*0.5f), (int)(ctr.y-7), 13, WHITE);
}

// ── HUD ───────────────────────────────────────────────────────────────────────

static void drawHUD(const GameSnapshot &snap, int myId)
{
    // Scores
    char s0[24], s1[24];
    snprintf(s0, sizeof(s0), "P0 (Azul): %d", snap.players[0].score);
    snprintf(s1, sizeof(s1), "P1 (Verm): %d", snap.players[1].score);
    DrawText(s0,  20, 8, 22, myId == 0 ? BLUE : LIGHTGRAY);
    DrawText(s1, 936 - MeasureText(s1,22) - 20, 8, 22, myId == 1 ? RED : LIGHTGRAY);

    // Phase + timer (center)
    const char *phNames[] = {
        "Aguardando 2º Jogador...", "POSICIONAMENTO", "BATALHA",
        "FIM DE RODADA", "FIM DE PARTIDA"
    };
    int ph = snap.phase < 5 ? snap.phase : 0;
    Color phCol = ph == PHASE_BATTLE      ? RED    :
                  ph == PHASE_POSITIONING ? ORANGE : WHITE;
    DrawText(phNames[ph], 468 - MeasureText(phNames[ph],22)/2, 8, 22, phCol);

    if (ph == PHASE_POSITIONING || ph == PHASE_BATTLE) {
        char tim[12]; snprintf(tim, sizeof(tim), "%ds", snap.timer);
        DrawText(tim, 468 - MeasureText(tim,18)/2, 33, 18, WHITE);
    }

    // "Você é Player X"
    char myLbl[24]; snprintf(myLbl, sizeof(myLbl), "Você: Player %d", myId);
    DrawText(myLbl, 20, 655, 15, kPlayer[myId]);

    // Controls
    const char *ctrl = ph == PHASE_POSITIONING ? "Arraste seu personagem" : "Autobattle";
    DrawText(ctrl, 936 - MeasureText(ctrl,15) - 16, 655, 15, LIGHTGRAY);

    // Buff legend
    int lx = 300;
    for (int b = 1; b <= 3; b++) {
        Color bc = buffFill(b); bc.a = 220;
        DrawRectangle(lx, 622, 13, 13, bc);
        DrawText(buffDesc[b], lx+16, 622, 13, LIGHTGRAY);
        lx += 16 + MeasureText(buffDesc[b], 13) + 22;
    }

    // Positioning hint
    if (ph == PHASE_POSITIONING) {
        const char *hint = myId == 0
            ? "Posicione-se no NORTE (metade superior)"
            : "Posicione-se no SUL (metade inferior)";
        DrawText(hint, 468 - MeasureText(hint,14)/2, 638, 14, YELLOW);
    }
}

static void drawRoundEndOverlay(const GameSnapshot &snap, int myId)
{
    DrawRectangle(186, 215, 564, 175, {0,0,0,190});
    DrawRectangleLinesEx({186,215,564,175}, 2.f, WHITE);

    if (snap.roundWinner == 0xFF) {
        const char *m = "EMPATE!";
        DrawText(m, 468-MeasureText(m,36)/2, 244, 36, YELLOW);
    } else {
        bool won = snap.roundWinner == (uint8_t)myId;
        const char *m = won ? "VOCÊ GANHOU O PONTO!" : "OPONENTE GANHOU O PONTO";
        DrawText(m, 468-MeasureText(m,28)/2, 244, 28, won ? GREEN : RED);
    }
    char sc[48]; snprintf(sc, sizeof(sc), "Placar: %d — %d",
                          snap.players[0].score, snap.players[1].score);
    DrawText(sc, 468-MeasureText(sc,22)/2, 296, 22, WHITE);
    const char *nx = "Próxima rodada em breve...";
    DrawText(nx, 468-MeasureText(nx,16)/2, 340, 16, LIGHTGRAY);
}

static void drawMatchEndOverlay(const GameSnapshot &snap, int myId)
{
    DrawRectangle(140, 185, 656, 240, {0,0,0,215});
    DrawRectangleLinesEx({140,185,656,240}, 3.f, GOLD);
    bool won = snap.matchWinner == (uint8_t)myId;
    const char *res = won ? "VITÓRIA!" : "DERROTA";
    DrawText(res, 468-MeasureText(res,52)/2, 210, 52, won ? GOLD : RED);
    char sc[48]; snprintf(sc, sizeof(sc), "Placar final: %d — %d",
                          snap.players[0].score, snap.players[1].score);
    DrawText(sc, 468-MeasureText(sc,22)/2, 292, 22, WHITE);
    const char *bye = "Feche e reabra para jogar novamente.";
    DrawText(bye, 468-MeasureText(bye,16)/2, 336, 16, LIGHTGRAY);
}

static void drawWaitingOverlay()
{
    DrawRectangle(0, 282, 936, 52, {0,0,0,165});
    const char *m = "Aguardando 2º jogador...";
    DrawText(m, 468-MeasureText(m,28)/2, 294, 28, WHITE);
}

// ── Main ──────────────────────────────────────────────────────────────────────

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
    char title[48]; snprintf(title, sizeof(title), "Arena — Player %d", myId);
    InitWindow(936, 684, title);
    SetTargetFPS(60);

    Texture2D arena = LoadTexture("assets/arena.png");

    // Initial snapshot
    GameSnapshot snap{};
    snap.phase       = PHASE_WAITING;
    snap.roundWinner = 0xFF;
    snap.matchWinner = 0xFF;

    // ── Client-side visual state ──────────────────────────────────────────

    // Smooth positions (screen pixels, interpolated toward server cell)
    Vector2 visPos[2] = {
        cellCenter(3, 1),   // P0 default
        cellCenter(4, 6)    // P1 default
    };

    // HP from previous frame — for detecting hits
    uint16_t prevHp[2]    = { BASE_HP, BASE_HP };
    uint8_t  prevPhase    = PHASE_WAITING;

    // Drag state (POSITIONING only)
    bool dragging         = false;
    int  ghostCellX       = -1;
    int  ghostCellY       = -1;

    // ── Game loop ─────────────────────────────────────────────────────────
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ── Send heartbeat every frame (keeps connection alive) ───────────
        {
            InputPacket inp{};
            inp.playerId = (uint8_t)myId;
            inp.type     = INPUT_HEARTBEAT;
            sendto(sock, &inp, sizeof(inp), 0,
                   (sockaddr *)&serverAddr, sizeof(serverAddr));
        }

        // ── Receive latest snapshot ───────────────────────────────────────
        {
            GameSnapshot buf{};
            ssize_t bytes;
            while ((bytes = recvfrom(sock, &buf, sizeof(buf),
                                     0, nullptr, nullptr)) > 0) {
                if (bytes == (ssize_t)sizeof(GameSnapshot))
                    snap = buf;
            }
        }

        // ── Phase change: reset effects & prevHp ─────────────────────────
        if (snap.phase != prevPhase) {
            if (snap.phase == PHASE_POSITIONING) {
                hitRingCount = dmgPopCount = 0;
                for (int i = 0; i < 2; i++) prevHp[i] = snap.players[i].hp;
            }
            dragging   = false;
            ghostCellX = ghostCellY = -1;
            prevPhase  = snap.phase;
        }

        // ── Detect hits → spawn effects ───────────────────────────────────
        for (int i = 0; i < 2; i++) {
            uint16_t curHp = snap.players[i].hp;
            if (curHp < prevHp[i] && prevHp[i] > 0) {
                int dmg = prevHp[i] - curHp;
                spawnHitRing(visPos[i].x, visPos[i].y, kPlayer[i]);
                spawnDmgPop(visPos[i].x, visPos[i].y - CELLH * 0.3f, dmg);
            }
            prevHp[i] = curHp;
        }

        // ── Smooth position interpolation (lerp toward server cell) ───────
        float k = fminf(1.f, 12.f * dt);
        for (int i = 0; i < 2; i++) {
            Vector2 target = cellCenter(snap.players[i].x, snap.players[i].y);
            visPos[i].x += (target.x - visPos[i].x) * k;
            visPos[i].y += (target.y - visPos[i].y) * k;
        }

        // ── Drag-and-drop input (POSITIONING only) ────────────────────────
        if (snap.phase == PHASE_POSITIONING && snap.players[myId].alive) {
            Vector2 mouse   = GetMousePosition();
            Vector2 myCtr   = visPos[myId];
            float   unitR   = CELLW * 0.37f;

            bool mouseInGrid = mouse.x >= GX && mouse.x < GX + GW &&
                               mouse.y >= GY && mouse.y < GY + GH;

            // Cell under mouse
            int mx = (int)((mouse.x - GX) / CELLW);
            int my = (int)((mouse.y - GY) / CELLH);

            // Check my half restriction
            bool myZone = (myId == 0) ? (my <= 3) : (my >= 4);

            float dx = mouse.x - myCtr.x, dy = mouse.y - myCtr.y;
            float dist = sqrtf(dx*dx + dy*dy);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && dist <= unitR + 6.f)
                dragging = true;

            if (dragging && mouseInGrid && myZone) {
                ghostCellX = mx;
                ghostCellY = my;
            } else if (dragging) {
                ghostCellX = ghostCellY = -1;
            }

            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                if (dragging && ghostCellX >= 0) {
                    InputPacket inp{};
                    inp.playerId = (uint8_t)myId;
                    inp.type     = INPUT_PLACE;
                    inp.placeX   = (uint8_t)ghostCellX;
                    inp.placeY   = (uint8_t)ghostCellY;
                    sendto(sock, &inp, sizeof(inp), 0,
                           (sockaddr *)&serverAddr, sizeof(serverAddr));
                    // Snap visPos immediately for responsiveness
                    visPos[myId] = cellCenter(ghostCellX, ghostCellY);
                }
                dragging   = false;
                ghostCellX = ghostCellY = -1;
            }
        } else {
            dragging   = false;
            ghostCellX = ghostCellY = -1;
        }

        // ── Render ────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(BLACK);

        // Arena background
        {
            float sx = 936.f / arena.width,  sy = 684.f / arena.height;
            float s  = (sx > sy) ? sx : sy;
            float sw = 936.f / s, sh = 684.f / s;
            float ox = (arena.width  - sw) * 0.5f;
            float oy = (arena.height - sh) * 0.5f;
            DrawTexturePro(arena, {ox,oy,sw,sh}, {0,0,936,684}, {0,0}, 0.f, WHITE);
        }

        drawBuffZones(snap);
        drawGrid();

        if (snap.phase == PHASE_POSITIONING)
            drawPositioningRestriction(myId);

        // Ghost cell during drag
        if (ghostCellX >= 0)
            drawGhostCell(ghostCellX, ghostCellY, myId);

        // Hit rings (drawn under units so they appear around them)
        updateDrawHitRings(dt);

        // Units
        for (int i = 0; i < 2; i++) {
            if (!snap.players[i].alive) continue;
            bool beingDragged = (dragging && i == myId);
            drawUnit(visPos[i], i, snap.players[i], i == myId, beingDragged);
        }

        // Drag cursor: follow mouse with ghost circle
        if (dragging) {
            Vector2 mouse = GetMousePosition();
            Color c = kPlayer[myId]; c.a = 140;
            DrawCircleV(mouse, CELLW * 0.35f, c);
            DrawCircleLinesV(mouse, CELLW * 0.35f, WHITE);
        }

        // Damage popups (drawn on top of units)
        updateDrawDmgPops(dt);

        drawHUD(snap, myId);

        switch (snap.phase) {
            case PHASE_WAITING:   drawWaitingOverlay();              break;
            case PHASE_ROUND_END: drawRoundEndOverlay(snap, myId);  break;
            case PHASE_MATCH_END: drawMatchEndOverlay(snap, myId);  break;
            default: break;
        }

        EndDrawing();
    }

    UnloadTexture(arena);
    CloseWindow();
    close(sock);
    return 0;
}
