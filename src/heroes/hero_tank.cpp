#include "../include/hero_tank.h"
#include <cstdio>

void HeroTank::activateUltimate(Hero** /*allies*/, int /*allyCount*/,
                                 Hero** /*enemies*/, int /*enemyCount*/)
{
    printf("  [ULT] Tank Iron Fortress! +20 ARM for %.0fs\n", ultimateDuration());
    savedArm_ = arm_;
    arm_     += 20;
    ultActive_      = true;
    ultActiveTimer_ = ultimateDuration();
}
