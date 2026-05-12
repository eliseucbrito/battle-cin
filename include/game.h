#pragma once
#include "trainer.h"
#include "protocol.h"
#include "game_defs.h"
#include "database.h"
#include "graph.h"
#include "tournament_tree.h"
#include "shop.h"

class Game {
public:
    Game();

    void registerPlayerLocal(int pid);
    bool handlePlaceHero(int pid, int heroIdx, uint8_t tx, uint8_t ty);
    void handleUseAbility(int pid);
    void handleTarget(int pid, int heroIdx, int targetIdx);
    void handleLocalTrainerLock(int pid, uint8_t trainerIdx);
    void handleLocalHeroPick(int pid, const uint8_t heroIndices[3]);
    void handleBuyItem(int pid, int stockIdx, int heroIdx, int slotIdx);
    void handleRefreshShop(int pid);
    void handleConfirmShop(int pid);
    void handleUseGeneralItem(int pid, int slot);
    void initFromSelections();

    bool isInitialized() const { return initialized_; }
    bool isBot(int pid) const { return isBot_[pid]; }

    void createBot(int pid);
    void updateBot(float dt);

    void update(float dt);
    void buildSnapshot(GameSnapshot& snap) const;

private:
    void startPositioning();
    void startBattle();
    void endRound(uint8_t winner);

    void autoBattleMove();
    void runCombat();
    void resolveTimeLimit();
    void generateBuffZones();
    void tickUltimates(float dt);
    void autoPickHeroes(int pid);
    void enterShopPhase();

    Trainer trainers_[2];
    int     connectedCount_;
    uint8_t phase_;
    float   phaseTimer_;

    BuffZoneInfo buffZones_[4];
    int          buffZoneCount_;
    uint8_t      roundWinner_;
    uint8_t      matchWinner_;
    int          roundNumber_;

    // Selection state
    bool             selected_[2];
    SelectionPacket  selections_[2];
    bool             initialized_;
    Database db_;

    bool             isBot_[2];
    bool             botPlaced_;

    Graph graph_;

    TournamentTree tournament_;

    Shop shop_;

    // PHASE_SELECT state
    uint8_t  selectSubphase_;
    float    selectTimer_;
    bool     trainerLocked_[2];
    uint8_t  trainerChoice_[2];
    bool     herosLocked_[2];
    uint8_t  heroChoices_[2][3];
};
