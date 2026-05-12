#pragma once
#include <cstdint>

#include "../include/protocol.h"
#include "../include/linked_list.h"

// ─────────────────────────────────────────────────────────────────────────────
// StatProfile — base stats defined by each Hero subclass
// ─────────────────────────────────────────────────────────────────────────────
struct StatProfile {
    int   hp;
    int   ad;
    int   arm;
    float as_rate;    // attacks per second
    float ms_delay;   // seconds per cell (movement speed)
};

// ─────────────────────────────────────────────────────────────────────────────
// Hero — abstract base class
// All combat units inherit from this class.
// ─────────────────────────────────────────────────────────────────────────────
class Hero {
protected:
    // ── Combat stats ─────────────────────────────────────────────────────
    int     hp_,  maxHp_;
    int     ad_,  arm_;
    float   as_rate_;   // attacks/second
    float   ms_delay_;  // seconds/cell

    // ── Position ──────────────────────────────────────────────────────────
    uint8_t x_, y_;
    bool    alive_;

    // ── Timers ────────────────────────────────────────────────────────────
    float moveTimer_;
    float attackTimer_;

    // ── Active effects (buffs/debuffs with duration) ──────────────────────
    LinkedList<ActiveEffect> effects_;

    // ── Ultimate state ────────────────────────────────────────────────────
    float ultCooldownTimer_;   // counts down; ult available when <= 0
    float ultActiveTimer_;     // counts down while ult effect is active
    bool  ultActive_;
    uint8_t ultPhraseIdx_;     // current phrase index [0..2]
    int   attackCount_;        // total attacks performed (used by Mage)

    // ── Custom stats override (per-named-hero from HERO_DEFS) ─────────────
    bool  hasCustomStats_;
    int   customHp_;
    int   customAd_;
    int   customArm_;

    // ── Owner ─────────────────────────────────────────────────────────────
    uint8_t ownerId_;          // 0 or 1 (which trainer this hero belongs to)

    // ── Identity ──────────────────────────────────────────────────────────
    uint8_t heroDefIndex_;     // index into HERO_DEFS[] (for client lookup)

    // ── Target focus ──────────────────────────────────────────────────────
    int8_t targetFocus_;       // enemy index to focus attack, -1 = no focus

    // ── Shop items ────────────────────────────────────────────────────────
    uint8_t items_[MAX_HERO_ITEMS];
    uint8_t itemCount_;
    float   tempItemTimers_[MAX_HERO_ITEMS];

public:
    explicit Hero(uint8_t ownerId);
    virtual ~Hero() = default;

    // ── Pure virtual: each subclass MUST implement ────────────────────────

    /// Returns the archetype constant (ARCHETYPE_TANK, etc.)
    virtual uint8_t archetype() const = 0;

    /// Returns the base stat profile for this hero class
    virtual StatProfile baseStats() const = 0;

    /// Returns true if this hero can be deployed on the given column.
    /// isLeft: true if this trainer owns the left half (cols 0-3).
    virtual bool canDeployAt(int col, bool isLeft) const = 0;

    /// Returns the asset path for this hero's sprite.
    virtual const char* assetPath() const = 0;

    // ── Virtual with default: subclasses may override ─────────────────────

    /// Damage formula — default: ad * 100 / (100 + target.arm)
    virtual int calculateDamage(const Hero& target) const;

    /// Choose movement target cell. Default: move toward nearest enemy.
    /// Returns false if no move should be made.
    virtual bool chooseMove(int enemyX, int enemyY, int& outX, int& outY) const;

    // ── Ultimate (auto-activation) ────────────────────────────────────────

    /// Returns true if the conditions for triggering the ultimate are met.
    virtual bool shouldTriggerUltimate() const = 0;

    /// Activates the ultimate effect. Called by Game when prob. check passes.
    /// allies/enemies are raw arrays; count is the number of valid entries.
    virtual void activateUltimate(Hero** allies, int allyCount,
                                  Hero** enemies, int enemyCount) = 0;

    /// Returns probability (0.0–1.0) per tick of activating ult when conditions met.
    virtual float ultimateProbabilityPerTick() const = 0;

    /// Returns the ultimate cooldown in seconds (after activation ends).
    virtual float ultimateCooldown() const = 0;

    /// Returns how many seconds the ultimate effect lasts.
    virtual float ultimateDuration() const = 0;

    // ── Concrete methods (shared by all heroes) ───────────────────────────

    /// Resets all stats to baseStats() values. Called at round start.
    void resetStats();

    /// Applies a buff zone bonus (adds to effects list with duration).
    void applyBuff(uint8_t type, float duration = 9999.f);

    /// Tick all active effects (decrement duration, remove expired).
    void tickEffects(float dt);

    /// Recalculate stats from base + custom + active effects.
    void recalcStats();

    /// Deals damage to target. Returns true if target died.
    bool attackTarget(Hero& target);

    /// Returns true if this hero is adjacent (<=1 cell in any direction) to other.
    bool isAdjacentTo(const Hero& other) const;

    /// Returns true if this hero is at the same cell as other.
    bool isAtSameCell(const Hero& other) const;

    /// Advances move and attack cooldown timers.
    void tickTimers(float dt);

    /// Advances ultimate cooldown and active timers.
    void tickUltimate(float dt);

    // ── Getters ───────────────────────────────────────────────────────────
    int     hp()       const { return hp_; }
    int     maxHp()    const { return maxHp_; }
    int     ad()       const { return ad_; }
    int     arm()      const { return arm_; }
    float   asRate()   const { return as_rate_; }
    uint8_t x()        const { return x_; }
    uint8_t y()        const { return y_; }
    bool    alive()    const { return alive_; }
    uint8_t buff()     const {
        // Return first effect type, or BUFF_NONE if no effects
        for (const auto& e : effects_) return e.effectType;
        return BUFF_NONE;
    }
    size_t  effectCount() const { return effects_.size(); }
    const LinkedList<ActiveEffect>& effects() const { return effects_; }
    bool    ultActive()  const { return ultActive_; }
    uint8_t ultPhraseIdx() const { return ultPhraseIdx_; }
    uint8_t ownerId()  const { return ownerId_; }
    float   moveTimer()  const { return moveTimer_; }
    float   attackTimer() const { return attackTimer_; }
    int     attackCount() const { return attackCount_; }
    bool    ultReady()   const { return ultCooldownTimer_ <= 0.f && !ultActive_; }
    float   ultCooldownTimer() const { return ultCooldownTimer_; }

    // ── Setters (used by Game and abilities) ──────────────────────────────
    void setPosition(uint8_t x, uint8_t y) { x_ = x; y_ = y; }
    void setAd(int v)    { ad_  = v; }
    void setArm(int v)   { arm_ = v; }
    void setAsRate(float v) { as_rate_ = v; }
    void healHp(int amount);
    void startMoveTimer() { moveTimer_ = ms_delay_; }
    void startAttackTimer() { attackTimer_ = 1.f / as_rate_; }

    // ── Custom stats (per-named-hero) ─────────────────────────────────────
    void setCustomStats(int hp, int ad, int arm);

    // ── Identity ──────────────────────────────────────────────────────────
    uint8_t heroDefIndex() const { return heroDefIndex_; }
    void setHeroDefIndex(uint8_t idx) { heroDefIndex_ = idx; }
    void setUltPhraseIdx(uint8_t idx) { ultPhraseIdx_ = idx; }

    // ── Target focus ──────────────────────────────────────────────────────
    int8_t targetFocus() const { return targetFocus_; }
    void setTargetFocus(int8_t idx) { targetFocus_ = idx; }
    void clearTargetFocus() { targetFocus_ = -1; }

    // ── Item inventory ────────────────────────────────────────────────────
    uint8_t itemCount()              const { return itemCount_; }
    uint8_t itemInSlot(int slot)     const { return (slot >= 0 && slot < MAX_HERO_ITEMS) ? items_[slot] : (uint8_t)0xFF; }
    bool    equipItem(int slot, uint8_t itemId);
    void    unequipItem(int slot);
    void    tickTempItems(int currentRound);
};
