#pragma once
#include "hero.h"

class HeroMage : public Hero {
public:
    explicit HeroMage(uint8_t ownerId) : Hero(ownerId) {}

    uint8_t     archetype() const override { return ARCHETYPE_MAGE; }
    const char* assetPath() const override { return "assets/heroes/mage.png"; }

    StatProfile baseStats() const override {
        return { 140, 35, 4, 0.7f, 0.35f };
    }

    bool canDeployAt(int col, bool isLeft) const override {
        // Back line: cols 0-1 left, cols 6-7 right
        return isLeft ? (col <= 1) : (col >= 6);
    }

    int calculateDamage(const Hero& target) const override;

    bool shouldTriggerUltimate() const override {
        return alive_ && ultReady() && attackCount_ >= 3;
    }

    float ultimateProbabilityPerTick() const override { return 0.12f; }
    float ultimateCooldown()           const override { return 15.f; }
    float ultimateDuration()           const override { return 1.f;  }

    void activateUltimate(Hero** allies, int allyCount, Hero** enemies, int enemyCount) override;
};
