#pragma once
#include "hero.h"

class HeroSupport : public Hero {
public:
    explicit HeroSupport(uint8_t ownerId) : Hero(ownerId) {}

    uint8_t     archetype() const override { return ARCHETYPE_SUPPORT; }
    const char* assetPath() const override { return "assets/heroes/support.png"; }

    StatProfile baseStats() const override {
        return { 180, 12, 8, 0.8f, 0.30f };
    }

    bool canDeployAt(int col, bool isLeft) const override {
        // Mid or Back: cols 0-2 left, cols 5-7 right
        return isLeft ? (col <= 2) : (col >= 5);
    }

    bool shouldTriggerUltimate() const override;

    float ultimateProbabilityPerTick() const override { return 0.10f; }
    float ultimateCooldown()           const override { return 18.f; }
    float ultimateDuration()           const override { return 1.f;  }

    void activateUltimate(Hero** allies, int allyCount, Hero** enemies, int enemyCount) override;

    mutable bool anyAllyLowHp_ = false;
};
