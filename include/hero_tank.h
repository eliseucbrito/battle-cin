#pragma once
#include "hero.h"

class HeroTank : public Hero {
private:
    int savedArm_;
public:
    explicit HeroTank(uint8_t ownerId) : Hero(ownerId), savedArm_(0) {}

    uint8_t     archetype() const override { return ARCHETYPE_TANK; }
    const char* assetPath() const override { return "assets/heroes/golem.png"; }

    StatProfile baseStats() const override {
        return { 350, 15, 18, 0.6f, 0.40f };
    }

    bool canDeployAt(int col, bool isLeft) const override {
        // Front line: col 3 for left side, col 4 for right side
        return isLeft ? (col == 3) : (col == 4);
    }

    bool shouldTriggerUltimate() const override {
        return alive_ && ultReady() && (float)hp_ / (float)maxHp_ < 0.30f;
    }

    float ultimateProbabilityPerTick() const override { return 0.15f; }
    float ultimateCooldown()           const override { return 12.f; }
    float ultimateDuration()           const override { return 5.f;  }

    void activateUltimate(Hero** allies, int allyCount, Hero** enemies, int enemyCount) override;
};
