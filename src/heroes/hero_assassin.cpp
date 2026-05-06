#include "../include/hero_assassin.h"
#include <cstdio>

bool HeroAssassin::canDeployAtCell(int col, int row, bool isLeft) const
{
    if (!canDeployAt(col, isLeft)) return false;
    return (row <= 1) || (row >= 6);   // flank rows in horizontal layout
}

bool HeroAssassin::chooseMove(int enemyX, int enemyY, int& outX, int& outY) const
{
    // Same movement logic as default (diagonal preferred toward target)
    return Hero::chooseMove(enemyX, enemyY, outX, outY);
    // Note: the Game selects which enemy is the target (lowest HP),
    // so the assassin's special targeting comes from Game::chooseTarget()
}

bool HeroAssassin::shouldTriggerUltimate() const
{
    return alive_ && ultReady() && lowestEnemyHpPct_ < 0.25f;
}

void HeroAssassin::activateUltimate(Hero** /*allies*/, int /*allyCount*/,
                                     Hero** enemies, int enemyCount)
{
    // Find lowest HP enemy and teleport to it
    Hero* target = nullptr;
    float minPct = 1.1f;
    for (int i = 0; i < enemyCount; ++i) {
        if (!enemies[i] || !enemies[i]->alive()) continue;
        float pct = (float)enemies[i]->hp() / (float)enemies[i]->maxHp();
        if (pct < minPct) { minPct = pct; target = enemies[i]; }
    }

    if (!target) return;

    // Teleport adjacent to target
    int tx = (int)target->x() > 0 ? target->x() - 1 : target->x() + 1;
    int ty = (int)target->y();
    setPosition((uint8_t)tx, (uint8_t)ty);

    // Burst damage: 2× calculateDamage
    int dmg = calculateDamage(*target) * 2;
    target->healHp(-dmg);   // negative heal = damage
    printf("  [ULT] Assassin Shadow Strike! Teleport + %ddmg to enemy(arch=%d) HP:%d/%d\n",
           dmg, target->archetype(), target->hp(), target->maxHp());

    if (target->hp() <= 0) {
        printf("    Enemy killed by Shadow Strike!\n");
    }

    ultActive_      = true;
    ultActiveTimer_ = ultimateDuration();
}
