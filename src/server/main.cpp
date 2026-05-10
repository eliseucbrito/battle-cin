#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "../../include/game.h"
#include "../../include/protocol.h"

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
    printf("Servidor Battle-CIn (Polimorfismo) — porta %d\nAguardando treinadores...\n\n", SERVER_PORT);

    Game game;
    static constexpr float DT = 1.f / 20;

    while (true) {
        timespec tick_start{};
        clock_gettime(CLOCK_MONOTONIC, &tick_start);

        // ── Receive inputs ────────────────────────────────────────────────
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        ssize_t     received;
        char        buf[64];

        while ((received = recvfrom(sock, buf, sizeof(buf), 0,
                                    (sockaddr *)&clientAddr, &clientLen)) > 0)
        {
            if (received == (ssize_t)sizeof(SelectionPacket)) {
                // Selection packet
                SelectionPacket sel;
                memcpy(&sel, buf, sizeof(sel));
                int pid = sel.playerId;
                if (pid < 0 || pid > 1) continue;
                if (!game.isConnected(pid))
                    game.registerPlayer(pid, clientAddr);
                if (!game.playerMatchesAddr(pid, clientAddr)) continue;
                game.handleSelect(pid, sel);
            }
            else if (received == (ssize_t)sizeof(TargetPacket)) {
                // Target focus packet
                TargetPacket tgt;
                memcpy(&tgt, buf, sizeof(tgt));
                int pid = tgt.playerId;
                if (pid < 0 || pid > 1) continue;

                if (!game.isConnected(pid))
                    game.registerPlayer(pid, clientAddr);

                if (!game.playerMatchesAddr(pid, clientAddr)) continue;

                game.handleTarget(pid, tgt.heroIndex, (int)tgt.targetIndex);
            }
            else if (received == (ssize_t)sizeof(InputPacket)) {
                // Game input packet
                InputPacket inp;
                memcpy(&inp, buf, sizeof(inp));
                int pid = inp.playerId;
                if (pid < 0 || pid > 1) continue;

                if (!game.isConnected(pid))
                    game.registerPlayer(pid, clientAddr);

                if (!game.playerMatchesAddr(pid, clientAddr)) continue;

                if (inp.type == INPUT_PLACE)
                    game.handlePlaceHero(pid, inp.heroIndex, inp.placeX, inp.placeY);
                else if (inp.type == INPUT_USE_ABILITY)
                    game.handleUseAbility(pid);
            }
        }

        // ── Update ────────────────────────────────────────────────────────
        game.update(DT);

        // ── Broadcast Snapshot ────────────────────────────────────────────
        if (game.isInitialized()) {
            GameSnapshot snap;
            game.buildSnapshot(snap);
            for (int i = 0; i < 2; i++) {
                if (game.isConnected(i))
                    sendto(sock, &snap, sizeof(snap), 0,
                           reinterpret_cast<const sockaddr *>(&game.playerAddr(i)),
                           sizeof(sockaddr_in));
            }
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
