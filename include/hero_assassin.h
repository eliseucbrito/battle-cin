#pragma once
#include "hero.h"

class HeroAssassin : public Hero {
public:
    explicit HeroAssassin(uint8_t ownerId) : Hero(ownerId) {}

    uint8_t     archetype() const override { return ARCHETYPE_ASSASSIN; }
    const char* assetPath() const override { return "assets/heroes/assassin.png"; }

    StatProfile baseStats() const override {
        return { 150, 30, 5, 1.2f, 0.20f };
    }

    bool canDeployAt(int col, bool isLeft) const override {
        // Assassin logic: row is used for flank (not col, but we keep the interface)
        return isLeft ? (col <= 3) : (col >= 4);
    }

    // Horizontal: flank means top/bottom rows (0-1 or 6-7)
    bool canDeployAtCell(int col, int row, bool isLeft) const;

    bool chooseMove(int enemyX, int enemyY, int& outX, int& outY) const override;
    bool shouldTriggerUltimate() const override;

    float ultimateProbabilityPerTick() const override { return 0.20f; }
    float ultimateCooldown()           const override { return 8.f;  }
    float ultimateDuration()           const override { return 0.5f; }

    void activateUltimate(Hero** allies, int allyCount, Hero** enemies, int enemyCount) override;

    mutable float lowestEnemyHpPct_ = 1.f;
};
