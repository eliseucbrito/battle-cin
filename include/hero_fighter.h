#pragma once
#include "hero.h"

class HeroFighter : public Hero {
private:
    int savedAd_;
public:
    explicit HeroFighter(uint8_t ownerId) : Hero(ownerId), savedAd_(0) {}

    uint8_t     archetype() const override { return ARCHETYPE_FIGHTER; }
    const char* assetPath() const override { return "assets/heroes/fighter.png"; }

    StatProfile baseStats() const override {
        return { 220, 25, 10, 0.9f, 0.28f };
    }

    bool canDeployAt(int col, bool isLeft) const override {
        // Front/Mid: cols 2-3 left, cols 4-5 right
        return isLeft ? (col >= 2 && col <= 3) : (col >= 4 && col <= 5);
    }

    bool shouldTriggerUltimate() const override {
        return alive_ && ultReady() && (float)hp_ / (float)maxHp_ < 0.50f;
    }

    float ultimateProbabilityPerTick() const override { return 0.10f; }
    float ultimateCooldown()           const override { return 10.f; }
    float ultimateDuration()           const override { return 4.f;  }

    void activateUltimate(Hero** allies, int allyCount, Hero** enemies, int enemyCount) override;
};
