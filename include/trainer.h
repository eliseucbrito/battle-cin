#pragma once
#include "hero.h"
#include "hero_factory.h"
#include "protocol.h"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <netinet/in.h>

// ─────────────────────────────────────────────────────────────────────────────
// Trainer — owns a roster of Heroes and has one manual Ability per round
// ─────────────────────────────────────────────────────────────────────────────
class Trainer {
private:
    std::string name_;
    std::string discipline_;
    uint8_t     playerId_;       // 0 or 1 (which player controls this trainer)
    uint8_t     trainerId_;      // index into TRAINER_DEFS[] (name/ability lookup)
    uint8_t     abilityType_;    // ABILITY_* constant

    // Composition: Trainer owns its heroes via unique_ptr (polymorphic)
    std::vector<std::unique_ptr<Hero>> heroes_;

    // Ability state
    bool  abilityUsed_;          // resets each round
    float abilityActiveTimer_;   // counts down while ability effect is active

    // Network state
    sockaddr_in addr_      = {};
    bool        connected_ = false;
    uint8_t     score_     = 0;

public:
    Trainer() = default;
    Trainer(const std::string& name, const std::string& discipline,
            uint8_t playerId, uint8_t trainerId, uint8_t abilityType);

    // ── Hero roster management ────────────────────────────────────────────

    /// Add a hero using the factory (by archetype, default stats)
    void addHero(uint8_t archetype);

    /// Add a hero with custom stats from HERO_DEFS[]
    void addHero(uint8_t archetype, int hp, int ad, int arm, uint8_t heroDefIndex);

    /// Direct add (for manual setup)
    void addHero(std::unique_ptr<Hero> hero);

    /// Access hero by index — returns reference (no copy)
    Hero&       heroAt(int index);
    const Hero& heroAt(int index) const;

    int  heroCount()     const { return (int)heroes_.size(); }
    bool hasLiveHeroes() const;

    // ── Trainer Ability ───────────────────────────────────────────────────

    bool canUseAbility()  const { return !abilityUsed_; }

    /// Activates ability: applies effect to all alive heroes_
    void useAbility();

    /// Tick ability duration timer
    void tickAbility(float dt);

    // ── Round lifecycle ───────────────────────────────────────────────────

    /// Resets hero stats and ability cooldown for a new round
    void resetForRound();

    /// Place a hero at a cell (called by Game::handlePlace)
    bool placeHero(int heroIndex, uint8_t x, uint8_t y, bool isLeft);

    // ── Network ───────────────────────────────────────────────────────────
    void         setAddr(const sockaddr_in& a)  { addr_ = a; connected_ = true; }
    bool         isConnected() const            { return connected_; }
    bool         matchesAddr(const sockaddr_in& a) const;
    const sockaddr_in& addr() const             { return addr_; }
    void         addScore()                     { ++score_; }
    uint8_t      score() const                  { return score_; }
    void         resetScore()                   { score_ = 0; }

    // ── Snapshot helpers ──────────────────────────────────────────────────
    uint8_t playerId()     const { return playerId_; }
    uint8_t trainerId()    const { return trainerId_; }
    uint8_t abilityType()  const { return abilityType_; }
    const std::string& name() const { return name_; }

private:
    void applyAbilityEffect();
};
