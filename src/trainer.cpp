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
    
    if (!heroes_[heroIndex]->canDeployAt(x, isLeft)) return false;
    
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
    
    if (abilityType_ != ABILITY_BATTLE_HEAL) {
        abilityActiveTimer_ = 5.0f;
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

uint8_t Trainer::generalItemInSlot(int s) const {
    if (s < 0 || s >= MAX_GENERAL_ITEMS) return 0xFF;
    return generalItems_[s];
}

bool Trainer::addGeneralItem(uint8_t itemId) {
    if (generalItemCount_ >= MAX_GENERAL_ITEMS) return false;
    for (int i = 0; i < MAX_GENERAL_ITEMS; i++) {
        if (generalItems_[i] == 0xFF) {
            generalItems_[i] = itemId;
            generalItemCount_++;
            return true;
        }
    }
    return false;
}

void Trainer::removeGeneralItem(int slot) {
    if (slot < 0 || slot >= MAX_GENERAL_ITEMS) return;
    if (generalItems_[slot] == 0xFF) return;
    generalItems_[slot] = 0xFF;
    if (generalItemCount_ > 0) generalItemCount_--;
}

void Trainer::clearGeneralItems() {
    for (int i = 0; i < MAX_GENERAL_ITEMS; i++) generalItems_[i] = 0xFF;
    generalItemCount_ = 0;
}
