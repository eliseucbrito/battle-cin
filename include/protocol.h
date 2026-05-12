#pragma once
#include <stdint.h>

#define PHASE_SELECT        0
#define PHASE_POSITIONING   1
#define PHASE_BATTLE        2
#define PHASE_ROUND_END     3
#define PHASE_MATCH_END     4
#define PHASE_VS_INTRO      5
#define PHASE_SHOP          6

#define BUFF_NONE  0
#define BUFF_AD    1
#define BUFF_HP    2
#define BUFF_ARM   3

#define ARCHETYPE_TANK      0
#define ARCHETYPE_FIGHTER   1
#define ARCHETYPE_MAGE      2
#define ARCHETYPE_ASSASSIN  3
#define ARCHETYPE_SUPPORT   4

#define ABILITY_RALLY        0
#define ABILITY_SHIELD_WALL  1
#define ABILITY_BATTLE_HEAL  2
#define ABILITY_FRENZY       3

#define ITEM_TYPE_CONSUMABLE  0
#define ITEM_TYPE_EQUIPMENT   1
#define ITEM_TYPE_TEMPORARY   2
#define ITEM_TYPE_UNIQUE      3

#define ITEM_RARITY_COMMON    0
#define ITEM_RARITY_UNCOMMON  1
#define ITEM_RARITY_RARE      2
#define ITEM_RARITY_EPIC      3

#define ITEM_CATEGORY_GENERAL  0
#define ITEM_CATEGORY_HERO     1

#define MAX_SHOP_STOCK        6
#define MAX_HERO_ITEMS        4
#define MAX_GENERAL_ITEMS     3

#define BUFF_AD_BONUS    15
#define BUFF_HP_BONUS    50
#define BUFF_ARM_BONUS   10

#define WIN_SCORE         5
#define POSITIONING_TIME  20
#define BATTLE_MAX_TIME   3600
#define DAMAGE_REDUCTION_FACTOR 3
#define ROUND_END_TIME    3
#define VS_INTRO_TIME     3
#define MAX_HEROES_SIDE   3
#define MAX_HEROES_TOTAL  (MAX_HEROES_SIDE * 2)
#define SELECT_TRAINER_TIME 45
#define SELECT_HERO_TIME    45

#define SERVER_PORT  7777
#define GRID_COLS    8
#define GRID_ROWS    8

#define INPUT_HEARTBEAT   0
#define INPUT_PLACE       1
#define INPUT_USE_ABILITY 2
#define INPUT_SELECT      3
#define INPUT_TARGET      4

#pragma pack(push, 1)

struct InputPacket {
    uint8_t playerId;
    uint8_t type;
    uint8_t placeX;
    uint8_t placeY;
    uint8_t heroIndex;
};

struct SelectionPacket {
    uint8_t playerId;
    uint8_t type;
    uint8_t trainerIndex;
    uint8_t heroIndices[3];
};

struct TargetPacket {
    uint8_t playerId;
    uint8_t type;
    uint8_t heroIndex;
    uint8_t targetIndex;
};

#pragma pack(pop)

struct BuffZoneInfo {
    uint8_t x, y, type;
};

struct ShopItemInfo {
    uint8_t itemId;
    uint8_t itemType;
    uint8_t rarity;
    uint8_t price;
    uint8_t category;
    char    name[32];
    char    desc[256];
};

struct ShopPlayerInfo {
    uint8_t gold;
    uint8_t confirmed;
};

struct ShopSnapshot {
    ShopPlayerInfo players[2];
    uint8_t        stockCount;
    ShopItemInfo   stock[MAX_SHOP_STOCK];
};

struct HeroNetState {
    uint8_t  x, y;
    uint16_t hp;
    uint16_t maxHp;
    uint8_t  ad;
    uint8_t  arm;
    uint8_t  asRate_x10;
    uint8_t  archetype;
    uint8_t  heroDefIndex;
    uint8_t  buff;
    uint8_t  alive;
    uint8_t  ultActive;
    uint8_t  ultPct;
    uint8_t  ultPhraseIdx;
    uint8_t  ownerId;
    int8_t   targetFocus;
    uint8_t  itemCount;
    uint8_t  items[4];
};

struct TrainerNetState {
    uint8_t  trainerId;
    uint8_t  score;
    uint8_t  heroCount;
    uint8_t  abilityReady;
    uint16_t gold;
    uint8_t  generalItemCount;
    uint8_t  generalItems[MAX_GENERAL_ITEMS];
};

#pragma pack(push, 1)
struct GameSnapshot {
    uint8_t        phase;
    uint8_t        timer;
    TrainerNetState trainers[2];
    uint8_t        heroCount;
    HeroNetState   heroes[MAX_HEROES_TOTAL];
    uint8_t        buffZoneCount;
    BuffZoneInfo   buffZones[4];
    uint8_t        roundWinner;
    uint8_t        matchWinner;

    uint8_t  selectSubphase;
    float    selectTimer;
    uint8_t  trainerLocked[2];
    uint8_t  trainerChoice[2];
    uint8_t  herosLocked[2];
    uint8_t  heroPicks[2][3];
    ShopSnapshot shop;
};
#pragma pack(pop)
