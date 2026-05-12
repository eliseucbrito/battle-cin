#pragma once
#include "hero.h"
#include "hero_factory.h"
#include "protocol.h"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

class Trainer {
private:
    std::string name_;
    std::string discipline_;
    uint8_t     playerId_ = 0;
    uint8_t     trainerId_ = 0;
    uint8_t     abilityType_ = 0;

    std::vector<std::unique_ptr<Hero>> heroes_;

    bool  abilityUsed_ = false;
    float abilityActiveTimer_ = 0.f;

    bool        connected_ = false;
    uint8_t     score_     = 0;

    uint8_t generalItems_[MAX_GENERAL_ITEMS];
    uint8_t generalItemCount_ = 0;

public:
    Trainer() = default;
    Trainer(const std::string& name, const std::string& discipline,
            uint8_t playerId, uint8_t trainerId, uint8_t abilityType);

    void addHero(uint8_t archetype);
    void addHero(uint8_t archetype, int hp, int ad, int arm, uint8_t heroDefIndex);
    void addHero(std::unique_ptr<Hero> hero);

    Hero&       heroAt(int index);
    const Hero& heroAt(int index) const;

    int  heroCount()     const { return (int)heroes_.size(); }
    bool hasLiveHeroes() const;

    bool canUseAbility()  const { return !abilityUsed_; }
    void useAbility();
    void tickAbility(float dt);

    void resetForRound();
    bool placeHero(int heroIndex, uint8_t x, uint8_t y, bool isLeft);

    void         setConnected(bool c) { connected_ = c; }
    bool         isConnected() const  { return connected_; }
    void         addScore()           { ++score_; }
    uint8_t      score() const        { return score_; }
    void         resetScore()         { score_ = 0; }

    uint8_t playerId()     const { return playerId_; }
    uint8_t trainerId()    const { return trainerId_; }
    uint8_t abilityType()  const { return abilityType_; }
    const std::string& name() const { return name_; }

    uint8_t generalItemCount()  const { return generalItemCount_; }
    uint8_t generalItemInSlot(int s) const;
    bool    addGeneralItem(uint8_t itemId);
    void    removeGeneralItem(int slot);
    void    clearGeneralItems();

private:
    void applyAbilityEffect();
};
