#include "../include/shop.h"
#include <cstdlib>
#include <cstdio>
#include <algorithm>

// ═════════════════════════════════════════════════════════════════════════════
//  ItemCatalog
// ═════════════════════════════════════════════════════════════════════════════

ItemCatalog::ItemCatalog() {
    registerPrototype(std::make_unique<HealthPotion>());
    registerPrototype(std::make_unique<StrengthGem>());
    registerPrototype(std::make_unique<SteelArmor>());
    registerPrototype(std::make_unique<SpeedScroll>());
    registerPrototype(std::make_unique<BerserkerElixir>());
    registerPrototype(std::make_unique<ArcaneOrb>());
    registerPrototype(std::make_unique<PhoenixFeather>());
    registerPrototype(std::make_unique<MirrorShield>());
}

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
    std::string n = item.name();
    int ni = 0;
    for (; ni < 31 && ni < (int)n.size(); ni++) info.name[ni] = n[ni];
    info.name[ni] = '\0';
    std::string d = item.description();
    int di = 0;
    for (; di < 63 && di < (int)d.size(); di++) info.desc[di] = d[di];
    info.desc[di] = '\0';
    return info;
}

// ═════════════════════════════════════════════════════════════════════════════
//  SupplyDemandPricing
// ═════════════════════════════════════════════════════════════════════════════

int SupplyDemandPricing::calculate(const Item& item, int round,
                                    int surviving, int wins) const
{
    int price = item.basePrice();
    price += round * 5;
    price += surviving * 8;
    price -= wins * 3;
    switch (item.rarity()) {
        case ITEM_RARITY_COMMON:   break;
        case ITEM_RARITY_UNCOMMON: price = price * 130 / 100; break;
        case ITEM_RARITY_RARE:     price = price * 180 / 100; break;
        case ITEM_RARITY_EPIC:     price = price * 250 / 100; break;
    }
    return price < 10 ? 10 : price;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Shop
// ═════════════════════════════════════════════════════════════════════════════

Shop::Shop()
    : currentRound_(0), isOpen_(false),
      pricing_(std::make_unique<SupplyDemandPricing>())
{
    gold_[0] = gold_[1] = 0;
    survivingHeroes_[0] = survivingHeroes_[1] = 0;
    wins_[0] = wins_[1] = 0;
    confirmed_[0] = confirmed_[1] = false;
}

void Shop::enterShopPhase(int roundNumber, const Trainer& t0, const Trainer& t1) {
    currentRound_ = roundNumber;
    survivingHeroes_[0] = countSurviving(t0);
    survivingHeroes_[1] = countSurviving(t1);
    wins_[0] = t0.score();
    wins_[1] = t1.score();
    isOpen_ = true;
    confirmed_[0] = confirmed_[1] = false;

    // Gold por jogador: base + performance individual
    for (int p = 0; p < 2; p++) {
        int baseGold = 30 + roundNumber * 10;
        int perfGold = survivingHeroes_[p] * 10;
        gold_[p] += baseGold + perfGold;
    }

    generateStock(6);
    printf("[Shop] Round %d — P1 Gold: %d, P2 Gold: %d, Stock: %d items\n",
           roundNumber, gold_[0], gold_[1], (int)currentStock_.size());
}

void Shop::generateStock(int count) {
    currentStock_.clear();
    uint8_t maxRar = (currentRound_ < 3) ? (uint8_t)ITEM_RARITY_UNCOMMON
                   : (currentRound_ < 6) ? (uint8_t)ITEM_RARITY_RARE
                   : (uint8_t)ITEM_RARITY_EPIC;
    currentStock_ = catalog_.generateStock(count, maxRar, currentRound_);
}

void Shop::refreshStock(int pid) {
    int cost = 30 + currentRound_ * 5;
    if (gold_[pid] < cost) {
        printf("[Shop] P%d Gold insuficiente para refresh (%d)\n", pid, cost);
        return;
    }
    spendGold(pid, cost);
    generateStock(6);
    printf("[Shop] P%d deu refresh por %dg\n", pid, cost);
}

bool Shop::buy(int pid, int stockIndex, Trainer& buyer, int heroIndex, int slotIndex) {
    if (stockIndex < 0 || stockIndex >= (int)currentStock_.size()) return false;
    if (!isOpen_) return false;

    Item& item = *currentStock_[stockIndex];
    Hero& hero = buyer.heroAt(heroIndex);
    int price = calculatePrice(item);

    if (gold_[pid] < price) {
        printf("[Shop] P%d Gold insuficiente: tem %d, precisa %d\n", pid, gold_[pid], price);
        return false;
    }

    if (hero.itemInSlot(slotIndex) != 0xFF) {
        printf("[Shop] Slot %d ja esta ocupado\n", slotIndex);
        return false;
    }

    spendGold(pid, price);
    bool ok = item.use(hero, currentRound_);
    if (ok) {
        hero.equipItem(slotIndex, item.itemId());
        printf("[Shop] P%d: %s para heroi %d (slot %d) por %dg\n",
               pid, item.name().c_str(), heroIndex, slotIndex, price);
    } else {
        refundGold(pid, price);
    }
    removeFromStock(stockIndex);
    return ok;
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
    return pricing_->calculate(item, currentRound_,
                               survivingHeroes_[0] + survivingHeroes_[1],
                               wins_[0] + wins_[1]);
}

bool Shop::confirm(int pid) {
    confirmed_[pid] = true;
    printf("[Shop] P%d confirmou\n", pid);
    if (confirmed_[0] && confirmed_[1]) {
        isOpen_ = false;
        currentStock_.clear();
        printf("[Shop] Ambos confirmaram — saindo da loja\n");
        return true;
    }
    return false;
}

void Shop::botShop(int pid, Trainer& bot, float) {
    if (!isOpen_) return;
    for (int attempt = 0; attempt < 2 && (int)currentStock_.size() > 0; attempt++) {
        int h = rand() % bot.heroCount();
        for (int s = 0; s < MAX_HERO_ITEMS; s++) {
            if (bot.heroAt(h).itemInSlot(s) == 0xFF) {
                int idx = rand() % (int)currentStock_.size();
                buy(pid, idx, bot, h, s);
                break;
            }
        }
    }
    confirm(pid);
}

void Shop::buildShopSnapshot(ShopSnapshot& snap) const {
    for (int p = 0; p < 2; p++) {
        snap.players[p].gold      = (uint8_t)(gold_[p] < 255 ? gold_[p] : 255);
        snap.players[p].confirmed = confirmed_[p] ? 1 : 0;
    }
    snap.stockCount = (uint8_t)currentStock_.size();
    for (int i = 0; i < (int)currentStock_.size() && i < MAX_SHOP_STOCK; i++) {
        snap.stock[i] = ItemCatalog::toShopItemInfo(
            *currentStock_[i], calculatePrice(*currentStock_[i]));
    }
    for (int i = (int)currentStock_.size(); i < MAX_SHOP_STOCK; i++) {
        ShopItemInfo& si = snap.stock[i];
        si.itemId = 0xFF; si.itemType = 0xFF; si.rarity = 0xFF; si.price = 0;
        si.name[0] = '\0'; si.desc[0] = '\0';
    }
}

int Shop::countSurviving(const Trainer& t) const {
    int count = 0;
    for (int i = 0; i < t.heroCount(); i++)
        if (t.heroAt(i).alive()) count++;
    return count;
}
