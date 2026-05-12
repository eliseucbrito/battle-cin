#pragma once
#include "hero.h"
#include "trainer.h"
#include "protocol.h"
#include "database.h"
#include <string>
#include <vector>
#include <memory>

// ── Item IDs (shared with protocol.h types) ──
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

class Item {
public:
    virtual ~Item() = default;

    bool use(Hero& hero, int currentRound) const {
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
    virtual std::string effectType()  const { return ""; }
    virtual int         trainerId()   const { return -1; }
    virtual int         heroId()      const { return -1; }

    virtual std::unique_ptr<Item> clone() const = 0;

    virtual void apply(Hero& hero, int currentRound) const = 0;

private:
    virtual bool canApply(const Hero& hero) const { (void)hero; return true; }
    virtual void onApplied(Hero& hero, int) const { (void)hero; }
};

class GenericItem : public Item {
    ShopItemRecord record_;
public:
    explicit GenericItem(const ShopItemRecord& rec) : record_(rec) {}

    std::string name()        const override { return record_.name; }
    std::string description() const override { return record_.description; }
    int         basePrice()   const override { return record_.base_price; }
    uint8_t     rarity()      const override { return (uint8_t)record_.rarity; }
    uint8_t     type()        const override { return (uint8_t)record_.type; }
    uint8_t     itemId()      const override { return (uint8_t)record_.id; }
    uint8_t     category()    const override { return (uint8_t)record_.category; }
    int         maxRounds()   const override { return record_.max_rounds; }
    std::string effectType()  const override { return record_.effect_type; }
    int         trainerId()   const override { return record_.trainer_id; }
    int         heroId()      const override { return record_.hero_id; }
    std::unique_ptr<Item> clone() const override;
    void apply(Hero& hero, int currentRound) const override { (void)currentRound; doApply(hero); }

private:
    bool canApply(const Hero& hero) const override;
    void doApply(Hero& hero) const;
};

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

class ItemCatalog {
public:
    ItemCatalog();
    std::unique_ptr<Item> createRandom(uint8_t maxRarity, const std::vector<int>& tIds, const std::vector<int>& hIds) const;
    std::vector<std::unique_ptr<Item>> generateStock(int count, uint8_t maxRarity, int roundNumber, const std::vector<int>& tIds, const std::vector<int>& hIds) const;
    static ShopItemInfo toShopItemInfo(const Item& item, int price);
    const Item* getPrototype(uint8_t itemId) const;
private:
    std::vector<std::unique_ptr<Item>> prototypes_;
public:
    void registerPrototype(std::unique_ptr<Item> item);
private:
};

class Shop {
public:
    Shop(const std::vector<ShopItemRecord>& shopRecords);
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
    std::vector<int> allowedTrainers_;
    std::vector<int> allowedHeroes_;
    void generateStock(int count);
    void removeFromStock(int idx);
};
