#include "../include/hero_factory.h"
#include "../include/hero_tank.h"
#include "../include/hero_fighter.h"
#include "../include/hero_mage.h"
#include "../include/hero_assassin.h"
#include "../include/hero_support.h"
#include "../include/protocol.h"
#include <cstdio>

std::unique_ptr<Hero> HeroFactory::create(uint8_t archetype, uint8_t ownerId)
{
    switch (archetype) {
        case ARCHETYPE_TANK:     return std::make_unique<HeroTank>(ownerId);
        case ARCHETYPE_FIGHTER:  return std::make_unique<HeroFighter>(ownerId);
        case ARCHETYPE_MAGE:     return std::make_unique<HeroMage>(ownerId);
        case ARCHETYPE_ASSASSIN: return std::make_unique<HeroAssassin>(ownerId);
        case ARCHETYPE_SUPPORT:  return std::make_unique<HeroSupport>(ownerId);
        default:
            printf("[HeroFactory] Archetype desconhecido %d\n", archetype);
            return nullptr;
    }
}

std::unique_ptr<Hero> HeroFactory::create(uint8_t archetype, uint8_t ownerId,
                                           int hp, int ad, int arm, uint8_t heroDefIndex)
{
    auto hero = create(archetype, ownerId);
    if (hero) {
        hero->setCustomStats(hp, ad, arm);
        hero->setHeroDefIndex(heroDefIndex);
        hero->resetStats();   // apply custom stats immediately
    }
    return hero;
}
