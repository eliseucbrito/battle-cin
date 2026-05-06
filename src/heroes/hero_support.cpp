#include "../include/hero_support.h"
#include <cstdio>

bool HeroSupport::shouldTriggerUltimate() const
{
    return alive_ && ultReady() && anyAllyLowHp_;
}

void HeroSupport::activateUltimate(Hero** allies, int allyCount,
                                    Hero** /*enemies*/, int /*enemyCount*/)
{
    printf("  [ULT] Support Mass Heal! Healing %d allies\n", allyCount);
    for (int i = 0; i < allyCount; ++i) {
        if (!allies[i] || !allies[i]->alive()) continue;
        int healAmt = allies[i]->maxHp() * 30 / 100;   // 30% max HP
        allies[i]->healHp(healAmt);
        printf("    Healed ally(arch=%d): +%d HP → %d/%d\n",
               allies[i]->archetype(), healAmt,
               allies[i]->hp(), allies[i]->maxHp());
    }
    anyAllyLowHp_ = false;
    ultActive_      = true;
    ultActiveTimer_ = ultimateDuration();
}
