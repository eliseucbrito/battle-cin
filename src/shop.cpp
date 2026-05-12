#include "../include/shop.h"
#include <cstdlib>
#include <cstdio>
#include <algorithm>

// ── GenericItem ──

std::unique_ptr<Item> GenericItem::clone() const {
    return std::make_unique<GenericItem>(record_);
}

bool GenericItem::canApply(const Hero& hero) const {
    if (record_.effect_type == "heal_pct" && record_.effect_target == "self")
        return hero.hp() < hero.maxHp();
    if (record_.effect_type == "revive")
        return false;
    return true;
}

void GenericItem::doApply(Hero& hero) const {
    const std::string& eff = record_.effect_type;
    float val = record_.effect_value;

    if (eff == "heal_pct") {
        hero.healHp((int)(hero.maxHp() * val));
    } else if (eff == "buff_ad_flat") {
        hero.setAd(hero.ad() + (int)val);
    } else if (eff == "buff_ad_pct") {
        hero.setAd((int)(hero.ad() * (1.0 + val)));
    } else if (eff == "buff_arm_flat") {
        hero.setArm(hero.arm() + (int)val);
    } else if (eff == "buff_as_pct") {
        hero.setAsRate(hero.asRate() * (float)(1.0 + val));
    }
}

// ── SupplyDemandPricing ──

int SupplyDemandPricing::calculate(const Item& item, int round,
                                   int surviving, int wins) const {
    (void)wins;
    int price = item.basePrice();
    price += (round / 2) * 5;
    if (surviving >= 2) price += 10;
    if (item.type() == ITEM_TYPE_EQUIPMENT) price += 15;
    if (item.type() == ITEM_TYPE_TEMPORARY) price += 10;
    if (price < 10) price = 10;
    return price;
}

// ── ItemCatalog ──

ItemCatalog::ItemCatalog() {}

void ItemCatalog::registerPrototype(std::unique_ptr<Item> item) {
    prototypes_.push_back(std::move(item));
}

std::unique_ptr<Item> ItemCatalog::createRandom(uint8_t maxRarity) const {
    if (prototypes_.empty()) return nullptr;
    int eligible[ITEM_ID_COUNT];
    int eligibleCount = 0;
    for (int i = 0; i < (int)prototypes_.size(); i++) {
        if (prototypes_[i]->rarity() <= maxRarity) {
            eligible[eligibleCount++] = i;
        }
    }
    if (eligibleCount == 0) return nullptr;
    int idx = eligible[rand() % eligibleCount];
    return prototypes_[idx]->clone();
}

std::vector<std::unique_ptr<Item>> ItemCatalog::generateStock(
    int count, uint8_t maxRarity, int roundNumber) const
{
    (void)roundNumber;
    std::vector<std::unique_ptr<Item>> stock;
    while ((int)stock.size() < count) {
        auto item = createRandom(maxRarity);
        if (item) stock.push_back(std::move(item));
    }
    for (int i = (int)stock.size() - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        if (i != j) std::swap(stock[i], stock[j]);
    }
    return stock;
}

ShopItemInfo ItemCatalog::toShopItemInfo(const Item& item, int price) {
    ShopItemInfo info;
    info.itemId   = item.itemId();
    info.itemType = item.type();
    info.rarity   = item.rarity();
    info.price    = (uint8_t)(price < 255 ? price : 255);
    info.category = item.category();
    snprintf(info.name, sizeof(info.name), "%s", item.name().c_str());
    snprintf(info.desc, sizeof(info.desc), "%s", item.description().c_str());
    return info;
}

const Item* ItemCatalog::getPrototype(uint8_t itemId) const {
    for (const auto& p : prototypes_)
        if (p->itemId() == itemId) return p.get();
    return nullptr;
}

// ── Shop ──

Shop::Shop(const std::vector<ShopItemRecord>& shopRecords)
    : currentRound_(0), isOpen_(false)
{
    gold_[0] = gold_[1] = 0;
    survivingHeroes_[0] = survivingHeroes_[1] = 0;
    wins_[0] = wins_[1] = 0;
    confirmed_[0] = confirmed_[1] = false;
    pricing_ = std::make_unique<SupplyDemandPricing>();

    for (const auto& rec : shopRecords)
        catalog_.registerPrototype(std::make_unique<GenericItem>(rec));
}

void Shop::enterShopPhase(int roundNumber, const Trainer& t0, const Trainer& t1) {
    currentRound_ = roundNumber;
    isOpen_ = true;
    confirmed_[0] = confirmed_[1] = false;

    survivingHeroes_[0] = countSurviving(t0);
    survivingHeroes_[1] = countSurviving(t1);
    wins_[0] = t0.score();
    wins_[1] = t1.score();

    gold_[0] = 60 + roundNumber * 10 + (t0.score() * 15);
    gold_[1] = 60 + roundNumber * 10 + (t1.score() * 15);

    currentStock_.clear();
    generateStock(MAX_SHOP_STOCK);

    printf("[Shop] Round %d — P1 gold=%d, P2 gold=%d\n", roundNumber, gold_[0], gold_[1]);
}

void Shop::generateStock(int count) {
    uint8_t maxRarity = ITEM_RARITY_COMMON;
    if (currentRound_ >= 3) maxRarity = ITEM_RARITY_UNCOMMON;
    if (currentRound_ >= 6) maxRarity = ITEM_RARITY_RARE;
    if (currentRound_ >= 9) maxRarity = ITEM_RARITY_EPIC;

    auto items = catalog_.generateStock(count, maxRarity, currentRound_);
    for (auto& item : items)
        currentStock_.push_back(std::move(item));
}

void Shop::removeFromStock(int idx) {
    if (idx >= 0 && idx < (int)currentStock_.size())
        currentStock_.erase(currentStock_.begin() + idx);
}

const Item* Shop::stockItem(int idx) const {
    if (idx < 0 || idx >= (int)currentStock_.size()) return nullptr;
    return currentStock_[idx].get();
}

int Shop::calculatePrice(const Item& item) const {
    return pricing_->calculate(item, currentRound_, survivingHeroes_[0] + survivingHeroes_[1],
                                wins_[0] + wins_[1]);
}

bool Shop::buy(int pid, int stockIndex, Trainer& buyer, int heroIndex, int slotIndex) {
    if (!isOpen_ || confirmed_[pid]) return false;
    if (stockIndex < 0 || stockIndex >= (int)currentStock_.size()) return false;
    if (heroIndex < 0 || heroIndex >= buyer.heroCount()) return false;

    const Item* item = currentStock_[stockIndex].get();
    if (!item) return false;

    int price = calculatePrice(*item);
    if (gold_[pid] < price) return false;

    uint8_t cat = item->category();
    if (cat == ITEM_CATEGORY_HERO) {
        if (slotIndex < 0) {
            for (int s = 0; s < MAX_HERO_ITEMS; s++) {
                if (buyer.heroAt(heroIndex).itemInSlot(s) == 0xFF) {
                    slotIndex = s;
                    break;
                }
            }
        }
        if (slotIndex < 0 || slotIndex >= MAX_HERO_ITEMS) return false;
        auto purchased = currentStock_[stockIndex]->clone();
        buyer.heroAt(heroIndex).equipItem(slotIndex, purchased->itemId());
    } else {
        buyer.addGeneralItem(item->itemId());
    }

    spendGold(pid, price);
    removeFromStock(stockIndex);
    printf("[Shop] P%d comprou %s por %d gold\n", pid, item->name().c_str(), price);
    return true;
}

void Shop::refreshStock(int pid) {
    if (!isOpen_ || confirmed_[pid]) return;
    int cost = 5;
    if (gold_[pid] < cost) return;
    spendGold(pid, cost);
    generateStock(1);
    printf("[Shop] P%d refreshou estoque\n", pid);
}

bool Shop::confirm(int pid) {
    if (!isOpen_) return false;
    if (confirmed_[pid]) return false;
    confirmed_[pid] = true;
    printf("[Shop] P%d confirmou\n", pid);
    if (confirmed_[0] && confirmed_[1]) {
        isOpen_ = false;
        printf("[Shop] Ambos confirmaram — fase encerrada.\n");
        return true;
    }
    return false;
}

int Shop::countSurviving(const Trainer& t) const {
    int alive = 0;
    for (int i = 0; i < t.heroCount(); i++)
        if (t.heroAt(i).alive()) alive++;
    return alive;
}

void Shop::botShop(int pid, Trainer& bot, float dt) {
    (void)dt;
    if (!isOpen_ || confirmed_[pid]) return;
    int attempts = 0;
    while (attempts < 3) {
        int r = rand() % stockCount();
        const Item* item = stockItem(r);
        if (!item) { attempts++; continue; }
        int hi = rand() % bot.heroCount();
        if (buy(pid, r, bot, hi, -1)) break;
        attempts++;
    }
    confirm(pid);
}

void Shop::buildShopSnapshot(ShopSnapshot& snap) const {
    for (int i = 0; i < 2; i++) {
        snap.players[i].gold = (uint8_t)(gold_[i] < 255 ? gold_[i] : 255);
        snap.players[i].confirmed = confirmed_[i] ? 1 : 0;
    }
    int sc = stockCount();
    snap.stockCount = (uint8_t)(sc > MAX_SHOP_STOCK ? MAX_SHOP_STOCK : sc);
    for (int i = 0; i < snap.stockCount; i++) {
        const Item* item = stockItem(i);
        if (item)
            snap.stock[i] = ItemCatalog::toShopItemInfo(*item, calculatePrice(*item));
    }
}
