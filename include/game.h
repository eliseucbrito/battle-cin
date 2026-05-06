#pragma once
#include "trainer.h"
#include "protocol.h"
#include <netinet/in.h>

class Game {
public:
    Game();

    void registerPlayer(int pid, const sockaddr_in& from);
    void handlePlaceHero(int pid, int heroIdx, uint8_t tx, uint8_t ty);
    void handleUseAbility(int pid);
    
    void update(float dt);
    void buildSnapshot(GameSnapshot& snap) const;

    bool isConnected(int pid) const;
    bool playerMatchesAddr(int pid, const sockaddr_in& a) const;
    const sockaddr_in& playerAddr(int pid) const;

private:
    void startPositioning();
    void startBattle();
    void endRound(uint8_t winner);
    
    void autoBattleMove();
    void runCombat();
    void resolveTimeLimit();
    void generateBuffZones();

    void tickUltimates(float dt);

    Trainer trainers_[2];
    int     connectedCount_;
    uint8_t phase_;
    float   phaseTimer_;
    
    BuffZoneInfo buffZones_[4];
    int          buffZoneCount_;
    uint8_t      roundWinner_;
    uint8_t      matchWinner_;
};
