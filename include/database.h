#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>

struct MatchRecord {
    int         id;
    std::string winner_name;
    std::string loser_name;
    int         winner_score;
    int         loser_score;
    std::string played_at;
};

struct HeroRecord {
    int         id;
    std::string name;
    std::string monologue;
    std::string description;
    int         archetype;
    std::string class_name;
    int         trainer_id;
    int         hp;
    int         ad;
    int         arm;
    std::string asset_path;
    std::string ultimate_name_1;
    std::string ultimate_name_2;
    std::string ultimate_name_3;
    std::string dying_phrase;
};

struct TrainerRecord {
    int         id;
    std::string name;
    std::string discipline;
    int         ability_type;
    std::string ability_name;
    std::string ability_desc;
    int         color_r, color_g, color_b;
    std::string portrait_path;
    std::string card_path;
};

struct ShopItemRecord {
    int         id;
    std::string name;
    std::string description;
    int         base_price;
    int         rarity;
    int         type;
    int         category;
    int         max_rounds;
    std::string icon_path;
    std::string effect_type;
    float       effect_value;
    std::string effect_target;
    int         trainer_id;
    int         hero_id;
};

class Database {
private:
    sqlite3*    db_;
    std::string path_;

    bool execute(const std::string& sql);
    void createTables();
    void seedAll();

public:
    explicit Database(const std::string& path = "battle_cin.db");
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // CREATE
    bool saveMatch(const std::string& winner_name,
                   const std::string& loser_name,
                   int winner_score,
                   int loser_score);

    // READ
    std::vector<MatchRecord> getRecentMatches(int limit = 10);

    struct RankEntry { std::string name; int wins; int losses; };
    std::vector<RankEntry> getRanking();

    std::vector<HeroRecord>    getAllHeroes();
    std::vector<TrainerRecord> getAllTrainers();
    std::vector<ShopItemRecord> getAllShopItems();

    HeroRecord getHeroById(int id);
    TrainerRecord getTrainerById(int id);
    ShopItemRecord getShopItemById(int id);

    // UPDATE
    bool updateHeroStats(int id, int hp, int ad, int arm);

    // DELETE
    bool clearHistory();
};
