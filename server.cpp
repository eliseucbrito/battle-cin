#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <algorithm>

#include "protocol.h"

static int iabs(int v) { return v < 0 ? -v : v; }

// ─────────────────────────────────────────────────────────────────────────────
// Player — dados + comportamentos de uma unidade
// ─────────────────────────────────────────────────────────────────────────────

class Player {
public:
    sockaddr_in addr      = {};
    bool        connected = false;

    float   moveTimer    = 0.f;
    float   attackTimer  = 0.f;
    int     ad = 0, arm = 0, hp = 0, maxHp = 0;
    float   as_rate  = 0.f;   // ataques / segundo
    float   ms_delay = 0.f;   // segundos / célula
    uint8_t buff  = BUFF_NONE;
    uint8_t score = 0;
    uint8_t x = 0, y = 0;
    bool    alive = false;

    // Restaura todos os stats de combate sem tocar em score / connected / addr
    void resetStats()
    {
        ad       = BASE_AD;
        arm      = BASE_ARM;
        maxHp    = BASE_HP;
        hp       = BASE_HP;
        as_rate  = BASE_AS;
        ms_delay = BASE_MS_DELAY;
        buff     = BUFF_NONE;
        alive    = true;
        moveTimer = attackTimer = 0.f;
    }

    void applyBuff(uint8_t type)
    {
        buff = type;
        switch (type) {
            case BUFF_AD:  ad    += BUFF_AD_BONUS;                  break;
            case BUFF_HP:  maxHp += BUFF_HP_BONUS; hp = maxHp;      break;
            case BUFF_ARM: arm   += BUFF_ARM_BONUS;                 break;
        }
        const char *names[] = { "none","AD","HP","ARM" };
        printf("  recebeu buff %s\n", names[type < 4 ? type : 0]);
    }

    // Aplica um hit no alvo; retorna true se o alvo morreu
    bool attackTarget(Player &target)
    {
        int dmg = ad * 100 / (100 + target.arm);
        if (dmg < 1) dmg = 1;
        target.hp -= dmg;
        attackTimer = 1.f / as_rate;
        printf("  %ddmg  HP:%d/%d\n", dmg,
               target.hp < 0 ? 0 : target.hp, target.maxHp);
        if (target.hp <= 0) {
            target.hp    = 0;
            target.alive = false;
            return true;
        }
        return false;
    }

    bool isAdjacentTo(const Player &other) const
    {
        return iabs((int)x - (int)other.x) <= 1 &&
               iabs((int)y - (int)other.y) <= 1;
    }

    bool matchesAddr(const sockaddr_in &a) const
    {
        return addr.sin_addr.s_addr == a.sin_addr.s_addr &&
               addr.sin_port        == a.sin_port;
    }

    void tickTimers(float dt)
    {
        if (moveTimer   > 0.f) moveTimer   -= dt;
        if (attackTimer > 0.f) attackTimer -= dt;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Game — state machine completa
// ─────────────────────────────────────────────────────────────────────────────

class Game {
public:
    // ── Interface pública (chamada pelo main) ─────────────────────────────

    // Registra um novo cliente; inicia posicionamento quando os 2 chegam
    void registerPlayer(int pid, const sockaddr_in &from)
    {
        Player &p  = players_[pid];
        p.addr      = from;
        p.connected = true;
        p.score     = 0;
        p.resetStats();
        p.x = (pid == 0) ? 3 : 4;
        p.y = (pid == 0) ? 1 : 6;
        ++connected_;
        printf("Player %d conectado! (%d/2)\n", pid, connected_);
        if (connected_ == 2 && phase_ == PHASE_WAITING)
            startPositioning();
    }

    // Processa comando de posicionamento (drag-and-drop)
    void handlePlace(int pid, uint8_t tx, uint8_t ty)
    {
        if (phase_ != PHASE_POSITIONING)              return;
        if (tx >= GRID_COLS || ty >= GRID_ROWS)       return;
        bool inZone = (pid == 0) ? (ty <= 3) : (ty >= 4);
        if (!inZone)                                   return;
        int other = 1 - pid;
        if (tx == players_[other].x && ty == players_[other].y) return;
        players_[pid].x = tx;
        players_[pid].y = ty;
    }

    // Avança um tick (chamado a 20 Hz)
    void update(float dt)
    {
        for (int i = 0; i < 2; i++) players_[i].tickTimers(dt);

        switch (phase_) {
            case PHASE_POSITIONING:
                phaseTimer_ -= dt;
                if (phaseTimer_ <= 0.f) startBattle();
                break;

            case PHASE_BATTLE:
                phaseTimer_ -= dt;
                autoBattleMove(0);
                autoBattleMove(1);
                runCombat();
                if (phaseTimer_ <= 0.f && phase_ == PHASE_BATTLE)
                    resolveTimeLimit();
                break;

            case PHASE_ROUND_END:
                phaseTimer_ -= dt;
                if (phaseTimer_ <= 0.f) startPositioning();
                break;

            default: break;
        }
    }

    void buildSnapshot(GameSnapshot &snap) const
    {
        snap.phase         = phase_;
        snap.timer         = (uint8_t)(phaseTimer_ < 0 ? 0 : (int)phaseTimer_);
        snap.buffZoneCount = (uint8_t)buffZoneCount_;
        snap.roundWinner   = roundWinner_;
        snap.matchWinner   = matchWinner_;

        for (int i = 0; i < 2; i++) {
            const Player &p = players_[i];
            snap.players[i] = {
                p.x, p.y,
                (uint16_t)(p.hp < 0 ? 0 : p.hp),
                (uint16_t)p.maxHp,
                p.buff,
                p.score,
                (uint8_t)p.alive
            };
        }
        for (int i = 0; i < 4; i++) {
            snap.buffZones[i] = (i < buffZoneCount_)
                                ? buffZones_[i]
                                : BuffZoneInfo{ 0xFF, 0xFF, BUFF_NONE };
        }
    }

    bool             isConnected(int pid)                      const { return players_[pid].connected; }
    bool             playerMatchesAddr(int pid, const sockaddr_in &a) const { return players_[pid].matchesAddr(a); }
    const sockaddr_in &playerAddr(int pid)                     const { return players_[pid].addr; }

private:
    Player      players_[2];
    int         connected_     = 0;
    uint8_t     phase_         = PHASE_WAITING;
    float       phaseTimer_    = 0.f;
    BuffZoneInfo buffZones_[4];
    int          buffZoneCount_ = 0;
    uint8_t     roundWinner_   = 0xFF;
    uint8_t     matchWinner_   = 0xFF;

    // ── Transições de fase ────────────────────────────────────────────────

    void startPositioning()
    {
        phase_       = PHASE_POSITIONING;
        phaseTimer_  = POSITIONING_TIME;
        roundWinner_ = 0xFF;

        for (int i = 0; i < 2; i++) players_[i].resetStats();
        players_[0].x = 3; players_[0].y = 1;
        players_[1].x = 4; players_[1].y = 6;

        generateBuffZones();
        printf("Posicionamento! (%ds)\n", POSITIONING_TIME);
    }

    void startBattle()
    {
        phase_      = PHASE_BATTLE;
        phaseTimer_ = BATTLE_MAX_TIME;

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < buffZoneCount_; j++) {
                if (players_[i].x == buffZones_[j].x &&
                    players_[i].y == buffZones_[j].y) {
                    printf("P%d ", i);
                    players_[i].applyBuff(buffZones_[j].type);
                    break;
                }
            }
        }
        printf("Batalha!\n");
    }

    void endRound(uint8_t winner)
    {
        roundWinner_ = winner;
        if (winner != 0xFF) {
            players_[winner].score++;
            printf("Ponto: P%d — Placar %d-%d\n",
                   winner, players_[0].score, players_[1].score);
            if (players_[winner].score >= WIN_SCORE) {
                matchWinner_ = winner;
                phase_       = PHASE_MATCH_END;
                printf("Partida encerrada! Vencedor: P%d\n", winner);
                return;
            }
        } else {
            printf("Empate de HP — sem ponto.\n");
        }
        phase_      = PHASE_ROUND_END;
        phaseTimer_ = ROUND_END_TIME;
    }

    // ── Autobattle ────────────────────────────────────────────────────────

    // Move a unidade i um passo em direção ao inimigo (diagonal preferida)
    void autoBattleMove(int i)
    {
        Player &me  = players_[i];
        Player &foe = players_[1 - i];
        if (!me.alive || !foe.alive || me.moveTimer > 0.f) return;
        if (me.isAdjacentTo(foe)) return;

        int ddx = (int)foe.x - (int)me.x;
        int ddy = (int)foe.y - (int)me.y;
        int sx  = (ddx > 0) ? 1 : (ddx < 0) ? -1 : 0;
        int sy  = (ddy > 0) ? 1 : (ddy < 0) ? -1 : 0;

        int candidates[3][2] = {
            { (int)me.x + sx, (int)me.y + sy },   // diagonal (preferida)
            { (int)me.x + sx, (int)me.y      },   // só X
            { (int)me.x,      (int)me.y + sy },   // só Y
        };
        for (auto &c : candidates) {
            int nx = c[0], ny = c[1];
            if (nx < 0 || nx >= GRID_COLS || ny < 0 || ny >= GRID_ROWS) continue;
            if (nx == (int)foe.x && ny == (int)foe.y) continue;
            me.x = (uint8_t)nx;
            me.y = (uint8_t)ny;
            me.moveTimer = me.ms_delay;
            return;
        }
    }

    // Auto-ataque melee quando adjacentes
    void runCombat()
    {
        for (int i = 0; i < 2 && phase_ == PHASE_BATTLE; i++) {
            Player &attacker = players_[i];
            Player &defender = players_[1 - i];
            if (!attacker.alive || attacker.attackTimer > 0.f) continue;
            if (!defender.alive || !attacker.isAdjacentTo(defender)) continue;

            printf("P%d→P%d", i, 1 - i);
            if (attacker.attackTarget(defender)) {
                printf("Player %d morreu!\n", 1 - i);
                endRound((uint8_t)i);
            }
        }
    }

    void resolveTimeLimit()
    {
        float p0 = (float)players_[0].hp / players_[0].maxHp;
        float p1 = (float)players_[1].hp / players_[1].maxHp;
        printf("Tempo! HP P0=%.0f%% P1=%.0f%%\n", p0 * 100, p1 * 100);
        if      (p0 > p1 + 0.001f) endRound(0);
        else if (p1 > p0 + 0.001f) endRound(1);
        else                        endRound(0xFF);
    }

    // ── Geração de buff zones ─────────────────────────────────────────────
    // Norte (rows 0-3) = zona P0, Sul (rows 4-7) = zona P1.
    // 2 zonas por metade, tipos variados para criar decisões a cada rodada.

    void generateBuffZones()
    {
        uint8_t northTypes[2] = { BUFF_AD,  BUFF_HP  };
        uint8_t southTypes[2] = { BUFF_ARM, BUFF_HP  };

        if (rand() % 2) std::swap(northTypes[0], northTypes[1]);
        if (rand() % 2) std::swap(southTypes[0], southTypes[1]);
        if (rand() % 3 == 0) northTypes[rand() % 2] = BUFF_ARM;
        if (rand() % 3 == 0) southTypes[rand() % 2] = BUFF_AD;

        const int rowMin[2]     = { 0, 4 };
        const int rowMax[2]     = { 3, 7 };
        uint8_t  *sideTypes[2]  = { northTypes, southTypes };

        buffZoneCount_ = 0;
        for (int side = 0; side < 2; side++) {
            for (int k = 0; k < 2; k++) {
                for (int attempt = 0; attempt < 50; attempt++) {
                    int bx = rand() % GRID_COLS;
                    int by = rowMin[side] + rand() % (rowMax[side] - rowMin[side] + 1);
                    if ((bx == 3 && by == 1) || (bx == 4 && by == 6)) continue;
                    bool dup = false;
                    for (int n = 0; n < buffZoneCount_; n++) {
                        if (buffZones_[n].x == bx && buffZones_[n].y == by) { dup = true; break; }
                    }
                    if (!dup) {
                        buffZones_[buffZoneCount_++] = { (uint8_t)bx, (uint8_t)by, sideTypes[side][k] };
                        break;
                    }
                }
            }
        }
        printf("  %d buff zones geradas\n", buffZoneCount_);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// main — rede + loop de tick
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    srand((unsigned)time(nullptr));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(SERVER_PORT);
    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    printf("Servidor 20 Hz — porta %d\nAguardando 2 jogadores...\n\n", SERVER_PORT);

    Game game;
    static constexpr float DT = 1.f / 20;

    while (true) {
        timespec tick_start{};
        clock_gettime(CLOCK_MONOTONIC, &tick_start);

        // ── Recebe inputs ─────────────────────────────────────────────────
        InputPacket inp;
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        ssize_t     received;

        while ((received = recvfrom(sock, &inp, sizeof(inp), 0,
                                    (sockaddr *)&clientAddr, &clientLen)) > 0)
        {
            if (received < (ssize_t)sizeof(InputPacket)) continue;
            int pid = inp.playerId;
            if (pid < 0 || pid > 1) continue;

            if (!game.isConnected(pid))
                game.registerPlayer(pid, clientAddr);

            if (!game.playerMatchesAddr(pid, clientAddr)) continue;

            if (inp.type == INPUT_PLACE)
                game.handlePlace(pid, inp.placeX, inp.placeY);
        }

        // ── Atualiza estado do jogo ───────────────────────────────────────
        game.update(DT);

        // ── Broadcast ────────────────────────────────────────────────────
        GameSnapshot snap;
        game.buildSnapshot(snap);
        for (int i = 0; i < 2; i++) {
            if (game.isConnected(i))
                sendto(sock, &snap, sizeof(snap), 0,
                       reinterpret_cast<const sockaddr *>(&game.playerAddr(i)),
                       sizeof(sockaddr_in));
        }

        // ── Tick rate (20 Hz) ─────────────────────────────────────────────
        timespec tick_end{};
        clock_gettime(CLOCK_MONOTONIC, &tick_end);
        long elapsed_us = (tick_end.tv_sec  - tick_start.tv_sec)  * 1000000L
                        + (tick_end.tv_nsec - tick_start.tv_nsec) / 1000L;
        long sleep_us = (1000000L / 20) - elapsed_us;
        if (sleep_us > 0) usleep((useconds_t)sleep_us);
    }
}
