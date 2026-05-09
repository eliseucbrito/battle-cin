#include "../include/trainer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

Trainer::Trainer(const std::string& name, const std::string& discipline,
                 uint8_t playerId, uint8_t trainerId, uint8_t abilityType)
    : name_(name), discipline_(discipline), playerId_(playerId), trainerId_(trainerId),
      abilityType_(abilityType), abilityUsed_(false),
      abilityActiveTimer_(0.f), connected_(false), score_(0)
{}

void Trainer::addHero(uint8_t archetype)
{
    if (heroes_.size() >= MAX_HEROES_SIDE) return;
    auto hero = HeroFactory::create(archetype, playerId_);
    if (hero) {
        heroes_.push_back(std::move(hero));
    }
}

void Trainer::addHero(uint8_t archetype, int hp, int ad, int arm, uint8_t heroDefIndex)
{
    if (heroes_.size() >= MAX_HEROES_SIDE) return;
    auto hero = HeroFactory::create(archetype, playerId_, hp, ad, arm, heroDefIndex);
    if (hero) {
        heroes_.push_back(std::move(hero));
    }
}

void Trainer::addHero(std::unique_ptr<Hero> hero)
{
    if (heroes_.size() >= MAX_HEROES_SIDE) return;
    heroes_.push_back(std::move(hero));
}

Hero& Trainer::heroAt(int index)
{
    return *heroes_.at(index);
}

const Hero& Trainer::heroAt(int index) const
{
    return *heroes_.at(index);
}

bool Trainer::hasLiveHeroes() const
{
    for (const auto& h : heroes_) {
        if (h->alive()) return true;
    }
    return false;
}

void Trainer::resetForRound()
{
    abilityUsed_ = false;
    abilityActiveTimer_ = 0.f;
    for (auto& h : heroes_) {
        h->resetStats();
    }
}

bool Trainer::placeHero(int heroIndex, uint8_t x, uint8_t y, bool isLeft)
{
    if (heroIndex < 0 || heroIndex >= (int)heroes_.size()) return false;
    
    // Validate deployment zone using polymorphism
    if (!heroes_[heroIndex]->canDeployAt(x, isLeft)) return false;
    
    // Additional check for Assassin (flank rows in horizontal layout)
    if (heroes_[heroIndex]->archetype() == ARCHETYPE_ASSASSIN) {
        if (y > 1 && y < 6) return false;
    }

    heroes_[heroIndex]->setPosition(x, y);
    return true;
}

void Trainer::useAbility()
{
    if (!canUseAbility()) return;
    
    printf("[Trainer %d] Using ability %d!\n", trainerId_, abilityType_);
    applyAbilityEffect();
    abilityUsed_ = true;
    
    // Some abilities have duration (Rally, Shield Wall, Frenzy)
    if (abilityType_ != ABILITY_BATTLE_HEAL) {
        abilityActiveTimer_ = 5.0f; // default 5s duration
    }
}

void Trainer::applyAbilityEffect()
{
    for (auto& h : heroes_) {
        if (!h->alive()) continue;

        switch (abilityType_) {
            case ABILITY_RALLY:
                h->setAd(h->ad() + 5);
                break;
            case ABILITY_SHIELD_WALL:
                h->setArm(h->arm() + 8);
                break;
            case ABILITY_BATTLE_HEAL:
                h->healHp(h->maxHp() * 25 / 100);
                break;
            case ABILITY_FRENZY:
                h->setAsRate(h->asRate() * 1.4f);
                break;
        }
    }
}

void Trainer::tickAbility(float dt)
{
    if (abilityActiveTimer_ > 0.f) {
        abilityActiveTimer_ -= dt;
        if (abilityActiveTimer_ <= 0.f) {
            // Restore stats (simple way: just reset to base + buffs)
            // A more complex system would track modifiers, but for EDOO 
            // we'll keep it simple: the effect ends and stats are recalculated.
            for (auto& h : heroes_) {
                if (h->alive()) {
                    int currentBuff = h->buff();
                    h->resetStats();
                    if (currentBuff != BUFF_NONE) h->applyBuff(currentBuff);
                }
            }
        }
    }
}

bool Trainer::matchesAddr(const sockaddr_in& a) const
{
    return addr_.sin_addr.s_addr == a.sin_addr.s_addr &&
           addr_.sin_port == a.sin_port;
}
