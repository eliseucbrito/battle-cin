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
      buff_(BUFF_NONE),
      ultCooldownTimer_(0.f), ultActiveTimer_(0.f),
      ultActive_(false), attackCount_(0),
      hasCustomStats_(false), customHp_(0), customAd_(0), customArm_(0),
      ownerId_(ownerId), heroDefIndex_(0xFF)
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
    buff_     = BUFF_NONE;
    alive_    = true;
    moveTimer_  = 0.f;
    attackTimer_ = 0.f;
    ultCooldownTimer_ = 0.f;
    ultActiveTimer_   = 0.f;
    ultActive_  = false;
    attackCount_ = 0;
}

void Hero::setCustomStats(int hp, int ad, int arm)
{
    hasCustomStats_ = true;
    customHp_  = hp;
    customAd_  = ad;
    customArm_ = arm;
}

void Hero::applyBuff(uint8_t type)
{
    buff_ = type;
    switch (type) {
        case BUFF_AD:  ad_    += BUFF_AD_BONUS;                    break;
        case BUFF_HP:  maxHp_ += BUFF_HP_BONUS; hp_ = maxHp_;     break;
        case BUFF_ARM: arm_   += BUFF_ARM_BONUS;                   break;
    }
    const char* names[] = { "none", "AD", "HP", "ARM" };
    printf("  Hero(owner=%d) received buff %s\n", ownerId_,
           names[type < 4 ? type : 0]);
}

int Hero::calculateDamage(const Hero& target) const
{
    // Default formula: physical damage reduced by armor
    int dmg = ad_ * 100 / (100 + target.arm_);
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
