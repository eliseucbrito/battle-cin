#pragma once
#include "hero.h"
#include "trainer.h"
#include "protocol.h"
#include <string>
#include <vector>
#include <memory>

// ═════════════════════════════════════════════════════════════════════════════
//  Item — abstract base class (Template Method)
// ═════════════════════════════════════════════════════════════════════════════

class Item {
public:
    virtual ~Item() = default;

    bool use(Hero& hero, int currentRound) {
        if (!canApply(hero)) return false;
        apply(hero, currentRound);
        onApplied(hero, currentRound);
        return true;
    }

    virtual std::string name()        const = 0;
    virtual std::string description() const = 0;
    virtual int         basePrice()   const = 0;
    virtual uint8_t     rarity()      const = 0;
    virtual uint8_t     type()        const = 0;
    virtual int         maxRounds()   const { return -1; }
    virtual uint8_t     itemId()      const = 0;
    virtual uint8_t     category()    const { return ITEM_CATEGORY_HERO; }

    virtual std::unique_ptr<Item> clone() const = 0;

    virtual void apply(Hero& hero, int currentRound) const = 0;

private:
    virtual bool canApply(const Hero& hero) const { (void)hero; return true; }
    virtual void onApplied(Hero& hero, int) { (void)hero; }
};

// ═════════════════════════════════════════════════════════════════════════════
//  Item IDs
// ═════════════════════════════════════════════════════════════════════════════

#define ITEM_ID_HEALTH_POTION      0
#define ITEM_ID_STRENGTH_GEM       1
#define ITEM_ID_STEEL_ARMOR        2
#define ITEM_ID_SPEED_SCROLL       3
#define ITEM_ID_BERSERKER_ELIXIR   4
#define ITEM_ID_ARCANE_ORB         5
#define ITEM_ID_PHOENIX_FEATHER    6
#define ITEM_ID_MIRROR_SHIELD      7
#define ITEM_ID_RALLY_ALL          8
#define ITEM_ID_HEAL_WAVE          9
#define ITEM_ID_GOLD_RUSH          10
#define ITEM_ID_COUNT              11

// ═════════════════════════════════════════════════════════════════════════════
//  Concrete Hero Item classes
// ═════════════════════════════════════════════════════════════════════════════

class HealthPotion : public Item {
public:
    std::string name()        const override { return "Pocao de Vida"; }
    std::string description() const override { return "Restaura 30% da HP maxima"; }
    int         basePrice()   const override { return 25; }
    uint8_t     rarity()      const override { return ITEM_RARITY_COMMON; }
    uint8_t     type()        const override { return ITEM_TYPE_CONSUMABLE; }
    uint8_t     itemId()      const override { return ITEM_ID_HEALTH_POTION; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<HealthPotion>(*this);
    }
private:
    bool canApply(const Hero& hero) const override {
        return hero.hp() < hero.maxHp();
    }
    void apply(Hero& hero, int) const override {
        hero.healHp(hero.maxHp() * 30 / 100);
    }
};

class StrengthGem : public Item {
public:
    std::string name()        const override { return "Gema de Forca"; }
    std::string description() const override { return "+15 AD permanente"; }
    int         basePrice()   const override { return 45; }
    uint8_t     rarity()      const override { return ITEM_RARITY_UNCOMMON; }
    uint8_t     type()        const override { return ITEM_TYPE_EQUIPMENT; }
    uint8_t     itemId()      const override { return ITEM_ID_STRENGTH_GEM; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<StrengthGem>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.setAd(hero.ad() + 15);
    }
};

class SteelArmor : public Item {
public:
    std::string name()        const override { return "Armadura de Aco"; }
    std::string description() const override { return "+10 ARM permanente"; }
    int         basePrice()   const override { return 50; }
    uint8_t     rarity()      const override { return ITEM_RARITY_UNCOMMON; }
    uint8_t     type()        const override { return ITEM_TYPE_EQUIPMENT; }
    uint8_t     itemId()      const override { return ITEM_ID_STEEL_ARMOR; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<SteelArmor>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.setArm(hero.arm() + 10);
    }
};

class SpeedScroll : public Item {
public:
    std::string name()        const override { return "Pergaminho Veloz"; }
    std::string description() const override { return "+30% AS por 2 rodadas"; }
    int         basePrice()   const override { return 60; }
    uint8_t     rarity()      const override { return ITEM_RARITY_RARE; }
    uint8_t     type()        const override { return ITEM_TYPE_TEMPORARY; }
    int         maxRounds()   const override { return 2; }
    uint8_t     itemId()      const override { return ITEM_ID_SPEED_SCROLL; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<SpeedScroll>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.setAsRate(hero.asRate() * 1.3f);
    }
};

class BerserkerElixir : public Item {
public:
    std::string name()        const override { return "Elixir Berserker"; }
    std::string description() const override { return "+50% AD por 3 rodadas"; }
    int         basePrice()   const override { return 70; }
    uint8_t     rarity()      const override { return ITEM_RARITY_RARE; }
    uint8_t     type()        const override { return ITEM_TYPE_TEMPORARY; }
    int         maxRounds()   const override { return 3; }
    uint8_t     itemId()      const override { return ITEM_ID_BERSERKER_ELIXIR; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<BerserkerElixir>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.setAd(hero.ad() * 3 / 2);
    }
};

class ArcaneOrb : public Item {
public:
    std::string name()        const override { return "Orbe Arcano"; }
    std::string description() const override { return "+25% de dano magico"; }
    int         basePrice()   const override { return 100; }
    uint8_t     rarity()      const override { return ITEM_RARITY_EPIC; }
    uint8_t     type()        const override { return ITEM_TYPE_UNIQUE; }
    uint8_t     itemId()      const override { return ITEM_ID_ARCANE_ORB; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<ArcaneOrb>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.setAd(hero.ad() * 125 / 100);
    }
};

class PhoenixFeather : public Item {
public:
    std::string name()        const override { return "Pena da Fenix"; }
    std::string description() const override { return "Revive com 50% HP se morto"; }
    int         basePrice()   const override { return 130; }
    uint8_t     rarity()      const override { return ITEM_RARITY_EPIC; }
    uint8_t     type()        const override { return ITEM_TYPE_UNIQUE; }
    uint8_t     itemId()      const override { return ITEM_ID_PHOENIX_FEATHER; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<PhoenixFeather>(*this);
    }
private:
    bool canApply(const Hero&) const override { return false; }
    void apply(Hero&, int) const override {}
};

class MirrorShield : public Item {
public:
    std::string name()        const override { return "Escudo Espelhado"; }
    std::string description() const override { return "+20 ARM permanente"; }
    int         basePrice()   const override { return 120; }
    uint8_t     rarity()      const override { return ITEM_RARITY_EPIC; }
    uint8_t     type()        const override { return ITEM_TYPE_UNIQUE; }
    uint8_t     itemId()      const override { return ITEM_ID_MIRROR_SHIELD; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<MirrorShield>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.setArm(hero.arm() + 20);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
//  General Item classes (affect all allies, stored on trainer)
// ═════════════════════════════════════════════════════════════════════════════

class RallyAll : public Item {
public:
    std::string name()        const override { return "Rally Total"; }
    std::string description() const override { return "+10 AD para todos aliados (1 rodada)"; }
    int         basePrice()   const override { return 60; }
    uint8_t     rarity()      const override { return ITEM_RARITY_UNCOMMON; }
    uint8_t     type()        const override { return ITEM_TYPE_CONSUMABLE; }
    uint8_t     itemId()      const override { return ITEM_ID_RALLY_ALL; }
    uint8_t     category()    const override { return ITEM_CATEGORY_GENERAL; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<RallyAll>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.setAd(hero.ad() + 10);
    }
};

class HealWave : public Item {
public:
    std::string name()        const override { return "Onda Curativa"; }
    std::string description() const override { return "Cura 25% da HP maxima de todos aliados"; }
    int         basePrice()   const override { return 70; }
    uint8_t     rarity()      const override { return ITEM_RARITY_UNCOMMON; }
    uint8_t     type()        const override { return ITEM_TYPE_CONSUMABLE; }
    uint8_t     itemId()      const override { return ITEM_ID_HEAL_WAVE; }
    uint8_t     category()    const override { return ITEM_CATEGORY_GENERAL; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<HealWave>(*this);
    }
    void apply(Hero& hero, int) const override {
        hero.healHp(hero.maxHp() * 25 / 100);
    }
};

class GoldRush : public Item {
public:
    std::string name()        const override { return "Corrida do Ouro"; }
    std::string description() const override { return "+50 gold bonus no proximo round"; }
    int         basePrice()   const override { return 40; }
    uint8_t     rarity()      const override { return ITEM_RARITY_COMMON; }
    uint8_t     type()        const override { return ITEM_TYPE_CONSUMABLE; }
    uint8_t     itemId()      const override { return ITEM_ID_GOLD_RUSH; }
    uint8_t     category()    const override { return ITEM_CATEGORY_GENERAL; }
    std::unique_ptr<Item> clone() const override {
        return std::make_unique<GoldRush>(*this);
    }
    void apply(Hero&, int) const override {}
};

// ═════════════════════════════════════════════════════════════════════════════
//  PricingStrategy (Strategy Pattern)
// ═════════════════════════════════════════════════════════════════════════════

class PricingStrategy {
public:
    virtual int calculate(const Item& item, int roundNumber,
                          int survivingHeroes, int wins) const = 0;
    virtual ~PricingStrategy() = default;
};

class SupplyDemandPricing : public PricingStrategy {
public:
    int calculate(const Item& item, int round, int surviving, int wins) const override;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ItemCatalog (Prototype/Factory Pattern)
// ═════════════════════════════════════════════════════════════════════════════

class ItemCatalog {
public:
    ItemCatalog();
    std::unique_ptr<Item> createRandom(uint8_t maxRarity) const;
    std::vector<std::unique_ptr<Item>> generateStock(int count, uint8_t maxRarity, int roundNumber) const;
    static ShopItemInfo toShopItemInfo(const Item& item, int price);
    const Item* getPrototype(uint8_t itemId) const;
private:
    std::vector<std::unique_ptr<Item>> prototypes_;
    void registerPrototype(std::unique_ptr<Item> item);
};

// ═════════════════════════════════════════════════════════════════════════════
//  Shop (composição central)
// ═════════════════════════════════════════════════════════════════════════════

class Shop {
public:
    Shop();
    void enterShopPhase(int roundNumber, const Trainer& t0, const Trainer& t1);
    bool buy(int pid, int stockIndex, Trainer& buyer, int heroIndex, int slotIndex);
    void refreshStock(int pid);
    bool confirm(int pid);
    void botShop(int pid, Trainer& bot, float dt);

    int     gold(int pid)          const { return gold_[pid]; }
    int     stockCount()           const { return (int)currentStock_.size(); }
    const   Item* stockItem(int idx) const;
    int     currentRound()         const { return currentRound_; }
    bool    isOpen()               const { return isOpen_; }
    bool    playerConfirmed(int pid) const { return confirmed_[pid]; }
    void    buildShopSnapshot(ShopSnapshot& snap) const;
    int     calculatePrice(const Item& item) const;
    void    spendGold(int pid, int amt)   { gold_[pid] -= amt; }
    void    refundGold(int pid, int amt)  { gold_[pid] += amt; }
    int     countSurviving(const Trainer& t) const;
    const   Item* getPrototype(uint8_t itemId) const { return catalog_.getPrototype(itemId); }
    void    addBonusGold(int pid, int amt) { gold_[pid] += amt; }

private:
    int  gold_[2];
    int  currentRound_;
    int  survivingHeroes_[2];
    int  wins_[2];
    bool isOpen_;
    bool confirmed_[2];
    ItemCatalog catalog_;
    std::vector<std::unique_ptr<Item>> currentStock_;
    std::unique_ptr<PricingStrategy> pricing_;
    void generateStock(int count);
    void removeFromStock(int idx);
};
