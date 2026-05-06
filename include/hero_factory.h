#pragma once
#include "hero.h"
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// HeroFactory — Factory Method pattern
// Creates the correct Hero subclass given an archetype constant.
// Returns a unique_ptr to transfer ownership to the caller (Trainer).
// ─────────────────────────────────────────────────────────────────────────────
class HeroFactory {
public:
    /// Creates and returns a new Hero of the given archetype, owned by ownerId.
    /// Returns nullptr for unknown archetypes.
    static std::unique_ptr<Hero> create(uint8_t archetype, uint8_t ownerId);
};
