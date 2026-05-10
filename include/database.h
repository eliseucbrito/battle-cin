#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>

// dados de uma partida salva
struct MatchRecord {
    int         id;
    std::string winner_name;
    std::string loser_name;
    int         winner_score;
    int         loser_score;
    std::string played_at;
};

// dados de um heroi no banco
struct HeroRecord {
    int         id;
    std::string name;
    std::string discipline;
    int         archetype;
    int         hp;
    int         ad;
    int         arm;
    std::string asset_path;
};

// CRUD — persistencia do Battle-CIn com SQLite
class Database {
private:
    sqlite3*    db_;
    std::string path_;

    bool execute(const std::string& sql);
    void createTables();
    void seedHeroes(); // insere os herois no banco se ainda nao tiver nenhum

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

    std::vector<HeroRecord> getAllHeroes();
    HeroRecord getHeroById(int id);

    // UPDATE
    bool updateHeroStats(int id, int hp, int ad, int arm);

    // DELETE
    bool clearHistory();
};