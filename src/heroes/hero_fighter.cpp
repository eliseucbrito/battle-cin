#include "../include/hero_fighter.h"
#include <cstdio>

void HeroFighter::activateUltimate(Hero** /*allies*/, int /*allyCount*/,
                                    Hero** /*enemies*/, int /*enemyCount*/)
{
    printf("  [ULT] Fighter Berserker Rage! +50%% AD for %.0fs\n", ultimateDuration());
    savedAd_ = ad_;
    ad_      = ad_ * 3 / 2;   // +50%
    ultActive_      = true;
    ultActiveTimer_ = ultimateDuration();
}
