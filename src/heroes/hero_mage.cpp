#include "../include/hero_mage.h"
#include <cstdio>
#include <cmath>

// Override: ignores 50% of target's armor
int HeroMage::calculateDamage(const Hero& target) const
{
    int reducedArm = target.arm() / 2;
    int dmg = (ad_ * 100 / (100 + reducedArm)) / DAMAGE_REDUCTION_FACTOR;
    return dmg < 1 ? 1 : dmg;
}

void HeroMage::activateUltimate(Hero** /*allies*/, int /*allyCount*/,
                                 Hero** enemies, int enemyCount)
{
    printf("  [ULT] Mage Arcane Nova! AoE %d enemies within 2 cells\n", enemyCount);
    for (int i = 0; i < enemyCount; ++i) {
        if (!enemies[i] || !enemies[i]->alive()) continue;

        int dx = (int)x_ - (int)enemies[i]->x();
        int dy = (int)y_ - (int)enemies[i]->y();
        float dist = sqrtf((float)(dx*dx + dy*dy));

        if (dist <= 2.0f) {
            // AoE hit: reduced damage, ignores armor completely
            int dmg = (ad_ / 2) / DAMAGE_REDUCTION_FACTOR;
            if (dmg < 1) dmg = 1;
            // We can't call attackTarget here (resets our timer), apply directly via hero method
            enemies[i]->healHp(-dmg);   // negative heal = damage
            printf("    AoE hit enemy(arch=%d): %ddmg\n",
                   enemies[i]->archetype(), dmg);
        }
    }
    attackCount_ = 0;   // reset attack counter after nova
    ultActive_      = true;
    ultActiveTimer_ = ultimateDuration();
}
