#include "../include/hero.h"
#include "../include/protocol.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>

static int iabs_hero(int v) { return v < 0 ? -v : v; }

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

Hero::Hero(uint8_t ownerId)
    : hp_(0), maxHp_(0), ad_(0), arm_(0),
      as_rate_(0.f), ms_delay_(0.f),
      x_(0), y_(0), alive_(false),
      moveTimer_(0.f), attackTimer_(0.f),
      ultCooldownTimer_(0.f), ultActiveTimer_(0.f),
      ultActive_(false), attackCount_(0),
      hasCustomStats_(false), customHp_(0), customAd_(0), customArm_(0),
      ownerId_(ownerId), heroDefIndex_(0xFF),
      targetFocus_(-1),
      items_{0xFF, 0xFF, 0xFF, 0xFF}, itemCount_(0),
      tempItemTimers_{0.f, 0.f, 0.f, 0.f}
{}

// ─────────────────────────────────────────────────────────────────────────────
// Concrete methods
// ─────────────────────────────────────────────────────────────────────────────

void Hero::resetStats()
{
    StatProfile p = baseStats();
    hp_       = hasCustomStats_ ? customHp_  : p.hp;
    maxHp_    = hasCustomStats_ ? customHp_  : p.hp;
    ad_       = hasCustomStats_ ? customAd_  : p.ad;
    arm_      = hasCustomStats_ ? customArm_ : p.arm;
    as_rate_  = p.as_rate;
    ms_delay_ = p.ms_delay;
    alive_    = true;
    moveTimer_  = 0.f;
    attackTimer_ = 0.f;
    ultCooldownTimer_ = 0.f;
    ultActiveTimer_   = 0.f;
    ultActive_  = false;
    attackCount_ = 0;
    targetFocus_ = -1;
    effects_.clear();
}

void Hero::setCustomStats(int hp, int ad, int arm)
{
    hasCustomStats_ = true;
    customHp_  = hp;
    customAd_  = ad;
    customArm_ = arm;
}

void Hero::applyBuff(uint8_t type, float duration)
{
    int magnitude = 0;
    switch (type) {
        case BUFF_AD:  magnitude = BUFF_AD_BONUS;  break;
        case BUFF_HP:  magnitude = BUFF_HP_BONUS;  break;
        case BUFF_ARM: magnitude = BUFF_ARM_BONUS; break;
    }
    effects_.pushBack({ type, magnitude, duration, duration });
    recalcStats();
    // HP buff grants immediate heal
    if (type == BUFF_HP) {
        hp_ += magnitude;
        if (hp_ > maxHp_) hp_ = maxHp_;
    }
    const char* names[] = { "none", "AD", "HP", "ARM" };
    printf("  Hero(owner=%d) received buff %s (+%d, %.0fs)\n", ownerId_,
           names[type < 4 ? type : 0], magnitude, duration);
}

void Hero::tickEffects(float dt)
{
    bool anyExpired = false;
    effects_.forEach([&](ActiveEffect& e) {
        e.duration -= dt;
        if (e.duration <= 0.f) anyExpired = true;
    });
    if (anyExpired) {
        effects_.removeAll([](const ActiveEffect& e) { return e.duration <= 0.f; });
        recalcStats();
    }
}

void Hero::recalcStats()
{
    // Start from base stats (preserve current hp_)
    StatProfile p = baseStats();
    maxHp_    = hasCustomStats_ ? customHp_  : p.hp;
    ad_       = hasCustomStats_ ? customAd_  : p.ad;
    arm_      = hasCustomStats_ ? customArm_ : p.arm;
    as_rate_  = p.as_rate;
    ms_delay_ = p.ms_delay;

    // Apply all active effects (only maxHp_ gets buff bonuses; hp_ stays intact)
    for (const auto& e : effects_) {
        switch (e.effectType) {
            case BUFF_AD:  ad_    += e.magnitude; break;
            case BUFF_HP:  maxHp_ += e.magnitude; break;
            case BUFF_ARM: arm_   += e.magnitude; break;
        }
    }

    // Clamp current HP to new max (e.g. if a HP buff expired)
    if (hp_ > maxHp_) hp_ = maxHp_;
}

int Hero::calculateDamage(const Hero& target) const
{
    // Default formula: physical damage reduced by armor
    int dmg = (ad_ * 100 / (100 + target.arm_)) / DAMAGE_REDUCTION_FACTOR;
    return dmg < 1 ? 1 : dmg;
}

bool Hero::attackTarget(Hero& target)
{
    int dmg = calculateDamage(target);   // virtual dispatch
    target.hp_ -= dmg;
    startAttackTimer();
    ++attackCount_;
    printf("  Hero(arch=%d,owner=%d) → Hero(arch=%d,owner=%d): %ddmg  HP:%d/%d\n",
           archetype(), ownerId_, target.archetype(), target.ownerId_,
           dmg,
           target.hp_ < 0 ? 0 : target.hp_, target.maxHp_);
    if (target.hp_ <= 0) {
        target.hp_    = 0;
        target.alive_ = false;
        return true;
    }
    return false;
}

bool Hero::isAdjacentTo(const Hero& other) const
{
    return iabs_hero((int)x_ - (int)other.x_) <= 1 &&
           iabs_hero((int)y_ - (int)other.y_) <= 1;
}

bool Hero::isAtSameCell(const Hero& other) const
{
    return x_ == other.x_ && y_ == other.y_;
}

void Hero::tickTimers(float dt)
{
    if (moveTimer_   > 0.f) moveTimer_   -= dt;
    if (attackTimer_ > 0.f) attackTimer_ -= dt;
}

void Hero::tickUltimate(float dt)
{
    if (ultActive_) {
        ultActiveTimer_ -= dt;
        if (ultActiveTimer_ <= 0.f) {
            ultActive_ = false;
            ultCooldownTimer_ = ultimateCooldown();
        }
    } else if (ultCooldownTimer_ > 0.f) {
        ultCooldownTimer_ -= dt;
    }
}

void Hero::healHp(int amount)
{
    hp_ += amount;
    if (hp_ > maxHp_) hp_ = maxHp_;
}

bool Hero::equipItem(int slot, uint8_t itemId)
{
    if (slot < 0 || slot >= MAX_HERO_ITEMS) return false;
    if (items_[slot] != 0xFF) return false;
    items_[slot] = itemId;
    itemCount_++;
    return true;
}

void Hero::unequipItem(int slot)
{
    if (slot < 0 || slot >= MAX_HERO_ITEMS) return;
    if (items_[slot] == 0xFF) return;
    items_[slot] = 0xFF;
    if (itemCount_ > 0) itemCount_--;
}

void Hero::tickTempItems(int currentRound)
{
    (void)currentRound;
    for (int i = 0; i < MAX_HERO_ITEMS; i++) {
        if (items_[i] == 0xFF) continue;
        if (tempItemTimers_[i] > 0.f) {
            items_[i] = 0xFF;
            tempItemTimers_[i] = 0.f;
            if (itemCount_ > 0) itemCount_--;
        }
    }
}

bool Hero::chooseMove(int enemyX, int enemyY, int& outX, int& outY) const
{
    // Default: move diagonally toward enemy
    int ddx = enemyX - (int)x_;
    int ddy = enemyY - (int)y_;
    int sx = (ddx > 0) ? 1 : (ddx < 0) ? -1 : 0;
    int sy = (ddy > 0) ? 1 : (ddy < 0) ? -1 : 0;

    int candidates[3][2] = {
        { (int)x_ + sx, (int)y_ + sy },   // diagonal (preferred)
        { (int)x_ + sx, (int)y_      },   // X only
        { (int)x_,      (int)y_ + sy },   // Y only
    };
    for (auto& c : candidates) {
        int nx = c[0], ny = c[1];
        if (nx < 0 || nx >= GRID_COLS || ny < 0 || ny >= GRID_ROWS) continue;
        if (nx == enemyX && ny == enemyY) continue;
        outX = nx;
        outY = ny;
        return true;
    }
    return false;
}
