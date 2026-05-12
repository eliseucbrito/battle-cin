#include "../include/database.h"
#include "../include/game_defs.h"
#include <cstdio>
#include <ctime>

Database::Database(const std::string& path)
    : db_(nullptr), path_(path)
{
    int rc = sqlite3_open(path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        printf("[Database] Erro ao abrir banco '%s': %s\n",
               path_.c_str(), sqlite3_errmsg(db_));
        db_ = nullptr;
        return;
    }
    printf("[Database] Banco aberto: %s\n", path_.c_str());
    createTables();
    seedAll();
}

Database::~Database()
{
    if (db_) {
        sqlite3_close(db_);
        printf("[Database] Banco fechado.\n");
    }
}

bool Database::execute(const std::string& sql)
{
    if (!db_) return false;
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        printf("[Database] Erro SQL: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

void Database::createTables()
{
    execute("DROP TABLE IF EXISTS heroes;");
    execute("DROP TABLE IF EXISTS trainers;");
    execute("DROP TABLE IF EXISTS shop_items;");

    execute(R"(
        CREATE TABLE trainers (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            name          TEXT    NOT NULL,
            discipline    TEXT    NOT NULL,
            ability_type  INTEGER NOT NULL,
            ability_name  TEXT    NOT NULL DEFAULT '',
            ability_desc  TEXT    NOT NULL DEFAULT '',
            color_r       INTEGER NOT NULL DEFAULT 80,
            color_g       INTEGER NOT NULL DEFAULT 160,
            color_b       INTEGER NOT NULL DEFAULT 230,
            portrait_path TEXT    NOT NULL DEFAULT '',
            card_path     TEXT    NOT NULL DEFAULT ''
        );
    )");

    execute(R"(
        CREATE TABLE heroes (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            name          TEXT    NOT NULL,
            monologue     TEXT    NOT NULL DEFAULT '',
            archetype     INTEGER NOT NULL,
            class_name    TEXT    NOT NULL DEFAULT '',
            trainer_id    INTEGER NOT NULL,
            hp            INTEGER NOT NULL,
            ad            INTEGER NOT NULL,
            arm           INTEGER NOT NULL,
            asset_path    TEXT    NOT NULL DEFAULT '',
            ultimate_name TEXT    DEFAULT 'Poder Ativo',
            FOREIGN KEY (trainer_id) REFERENCES trainers(id)
        );
    )");

    execute(R"(
        CREATE TABLE shop_items (
            id              INTEGER PRIMARY KEY,
            name            TEXT    NOT NULL,
            description     TEXT    NOT NULL DEFAULT '',
            base_price      INTEGER NOT NULL,
            rarity          INTEGER NOT NULL,
            type            INTEGER NOT NULL,
            category        INTEGER NOT NULL,
            max_rounds      INTEGER NOT NULL DEFAULT -1,
            icon_path       TEXT    NOT NULL DEFAULT '',
            effect_type     TEXT    NOT NULL DEFAULT '',
            effect_value    REAL    NOT NULL DEFAULT 0,
            effect_target   TEXT    NOT NULL DEFAULT 'self',
            trainer_id      INTEGER NOT NULL DEFAULT -1,
            hero_id         INTEGER NOT NULL DEFAULT -1
        );
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS match_history (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            winner_name  TEXT    NOT NULL,
            loser_name   TEXT    NOT NULL,
            winner_score INTEGER NOT NULL,
            loser_score  INTEGER NOT NULL,
            played_at    TEXT    NOT NULL
        );
    )");

    printf("[Database] Tabelas prontas.\n");
}

void Database::seedAll()
{
    if (!db_) return;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM trainers;", -1, &stmt, nullptr);
    int trainCount = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        trainCount = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    if (trainCount > 0) return;

    printf("[Database] Inserindo treinadores, herois e itens...\n");

    // ── Treinadores ──
    struct TrainerSeed { const char* name; const char* disc; int abType; const char* abName;
                         int r, g, b; const char* pp; const char* cp; };
    TrainerSeed tdata[] = {
        { "Abel Guilhermino",  "Estrutura de Dados",     0, "Rally (+AD)",  80, 160, 230,
          "assets/trainers/presentation/Abel_Guilhermino_presentation.png",
          "assets/trainers/card/Abel_Guilhermino_card.png" },
        { "Alex Sandro",       "Orient. a Objetos",      1, "Shield (+ARM)", 230, 80, 130,
          "assets/trainers/presentation/Alex_Sandro_presentation.png",
          "assets/trainers/card/Alex_Sandro_card.png" },
        { "David Junior",       "Algoritmos",             2, "Heal (+HP)",  80, 230, 160,
          "assets/trainers/presentation/David_presentation.png",
          "assets/trainers/card/David_Junior_card.png" },
        { "Francisco Paulo",   "Banco de Dados",         3, "Frenzy (+AS)", 230, 180, 80,
          "assets/trainers/presentation/Francisco_Paulo_presentation.png",
          "assets/trainers/card/Francisco_Paulo_card.png" },
        { "Juliano Lyoda",     "Redes de Computadores",  0, "Rally (+AD)",  160, 80, 230,
          "assets/trainers/presentation/Juliano_lyoda_presentation.png",
          "assets/trainers/card/Juliano_lyoda_card.png" },
        { "Valeria Cesario",   "Engenharia de Software", 1, "Shield (+ARM)", 230, 80, 230,
          "assets/trainers/presentation/Valeria_Cesario_presentation.png",
          "assets/trainers/card/Valeria_Cesario_card.png" },
    };

    for (const auto& t : tdata) {
        const char* sql = "INSERT INTO trainers (name,discipline,ability_type,ability_name,"
                          "color_r,color_g,color_b,portrait_path,card_path)"
                          "VALUES (?,?,?,?,?,?,?,?,?);";
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, t.name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, t.disc, -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 3, t.abType);
        sqlite3_bind_text(stmt, 4, t.abName, -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 5, t.r);
        sqlite3_bind_int (stmt, 6, t.g);
        sqlite3_bind_int (stmt, 7, t.b);
        sqlite3_bind_text(stmt, 8, t.pp, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, t.cp, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    printf("[Database] %zu treinadores inseridos.\n", sizeof(tdata)/sizeof(tdata[0]));

    // ── Herois ──
    struct HeroSeed { const char* name; int arch; int trainId; const char* cls;
                      int hp, ad, arm; const char* asset; const char* ultName; };
    HeroSeed hdata[] = {
        { "O Construto de Busca",    0, 1, "Tank",     350, 15, 18, "assets/heroes/O_Construto_de_Busca.png",   "Fortaleza de Ferro" },
        { "O Guardiao dos Discos",   1, 1, "Fighter",  280, 22, 10, "assets/heroes/O_Guardiao_dos_Discos.png",  "Furia do Disco" },
        { "O Mestre Parser",         2, 1, "Mage",     200, 35,  5, "assets/heroes/O_Mestre_Parser.png",        "Chuva de Tokens" },
        { "O Cientista Polarizado",  3, 1, "Assassin", 220, 32,  3, "assets/heroes/O_Cientista_Polarizado.png", "Corte Magnetico" },
        { "O Chip-Mestre",           4, 1, "Support",  240, 12, 10, "assets/heroes/O_Chip-Mestre.png",          "Overclock de Vida" },
        { "A Burocrata do UML",      0, 2, "Tank",     360, 13, 20, "assets/heroes/A_Burocrata_do_UML.png",     "Diagrama de Defesa" },
        { "O Filosofo do Dilema",    1, 2, "Fighter",  270, 24, 12, "assets/heroes/O_Filosofo_do_Dilema.png",   "Golpe do Absurdo" },
        { "O Artista Vectorial",     2, 2, "Mage",     190, 38,  4, "assets/heroes/O_Artista_Vectorial.png",    "Pincelada Critica" },
        { "O Inspetor Flaky",        3, 2, "Assassin", 215, 30,  2, "assets/heroes/O_Inspetor_Flaky.png",       "Bug Fatal" },
        { "O Treinador Python",      4, 2, "Support",  250, 14,  8, "assets/heroes/O_Treinador_Python.png",     "Script de Cura" },
    };

    for (const auto& h : hdata) {
        const char* sql = "INSERT INTO heroes (name,monologue,archetype,class_name,trainer_id,hp,ad,arm,asset_path,ultimate_name)"
                          "VALUES (?,?,?,?,?,?,?,?,?,?);";
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, h.name,  -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, "",   -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 3, h.arch);
        sqlite3_bind_text(stmt, 4, h.cls,   -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 5, h.trainId);
        sqlite3_bind_int (stmt, 6, h.hp);
        sqlite3_bind_int (stmt, 7, h.ad);
        sqlite3_bind_int (stmt, 8, h.arm);
        sqlite3_bind_text(stmt, 9, h.asset, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 10, h.ultName, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    printf("[Database] %zu herois inseridos.\n", sizeof(hdata)/sizeof(hdata[0]));

    // ── Itens da loja ──
    // Definimos itens base e os replicamos para cada treinador/herói
    int currentItemId = 0;

    auto trainers = getAllTrainers();
    for (const auto& t : trainers) {
        struct ItemData { std::string name; const char* desc; int price; int rarity; const char* eff; float val; int idOff; };
        ItemData trainerItems[] = {
            { "Cafe Energizante", "Um cafe forte que desperta o potencial do treinador, concedendo um bonus de 10 de Dano de Ataque (AD) para todos os herois durante o round atual.", 40, 0, "buff_ad_flat", 10.0f, 3 },
            { "Lampada do Conhecimento",  "Uma lampada que ilumina a estrategia, aumentando a Armadura (ARM) de todos os herois em 15 pontos durante o round atual.",    45, 0, "buff_arm_flat", 15.0f, 4 }
        };
        for (auto& it : trainerItems) {
            int finalId = (currentItemId / 5) * 5 + it.idOff;
            if (finalId <= currentItemId) finalId += 5;
            currentItemId = finalId;

            const char* sql = "INSERT INTO shop_items (id,name,description,base_price,rarity,type,category,effect_type,effect_value,trainer_id)"
                              "VALUES (?,?,?,?,?,?,?,?,?,?);";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            sqlite3_bind_int (stmt, 1, currentItemId);
            sqlite3_bind_text(stmt, 2, it.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, it.desc, -1, SQLITE_STATIC);
            sqlite3_bind_int (stmt, 4, it.price);
            sqlite3_bind_int (stmt, 5, it.rarity);
            sqlite3_bind_int (stmt, 6, 0); // type
            sqlite3_bind_int (stmt, 7, 0); // category: general
            sqlite3_bind_text(stmt, 8, it.eff, -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 9, it.val);
            sqlite3_bind_int (stmt, 10, t.id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    auto heroes = getAllHeroes();
    for (const auto& h : heroes) {
        struct ItemData { std::string name; const char* desc; int price; int rarity; const char* eff; float val; int idOff; };
        ItemData heroItems[] = {
            { "Pocao de Cura (25% HP)",   "Uma pocao magica que restaura instantaneamente 25% dos Pontos de Vida (HP) maximos do heroi que a consome.",            30, 0, "heal_pct", 0.25f, 0 },
            { "Pocao de Vitalidade (+20 AD)", "Um elixir potente que fortalece os musculos do heroi, concedendo um bonus permanente de 20 de Dano de Ataque (AD).",    60, 0, "buff_ad_flat", 20.0f, 1 },
            { "Pocao de Resiliencia (+15 ARM)",     "Uma pocao protetora que reforca a resistencia do heroi, concedendo um bonus permanente de 15 de Armadura (ARM).",    55, 0, "buff_arm_flat", 15.0f, 2 }
        };
        for (auto& it : heroItems) {
            int finalId = (currentItemId / 5) * 5 + it.idOff;
            if (finalId <= currentItemId) finalId += 5;
            currentItemId = finalId;

            const char* sql = "INSERT INTO shop_items (id,name,description,base_price,rarity,type,category,effect_type,effect_value,hero_id)"
                              "VALUES (?,?,?,?,?,?,?,?,?,?);";
            sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            sqlite3_bind_int (stmt, 1, currentItemId);
            sqlite3_bind_text(stmt, 2, it.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, it.desc, -1, SQLITE_STATIC);
            sqlite3_bind_int (stmt, 4, it.price);
            sqlite3_bind_int (stmt, 5, it.rarity);
            sqlite3_bind_int (stmt, 6, 1); // type
            sqlite3_bind_int (stmt, 7, 1); // category: hero
            sqlite3_bind_text(stmt, 8, it.eff, -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 9, it.val);
            sqlite3_bind_int (stmt, 10, h.id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    printf("[Database] Itens inseridos vinculados a treinadores e herois.\n");
}

bool Database::saveMatch(const std::string& winner_name,
                         const std::string& loser_name,
                         int winner_score,
                         int loser_score)
{
    if (!db_) return false;
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO match_history (winner_name, loser_name, winner_score, loser_score, played_at) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, winner_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, loser_name.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, winner_score);
    sqlite3_bind_int (stmt, 4, loser_score);
    sqlite3_bind_text(stmt, 5, buf,                 -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        printf("[Database] Partida salva: %s %d x %d %s\n",
               winner_name.c_str(), winner_score, loser_score, loser_name.c_str());
        return true;
    }
    return false;
}

std::vector<MatchRecord> Database::getRecentMatches(int limit)
{
    std::vector<MatchRecord> results;
    if (!db_) return results;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, winner_name, loser_name, winner_score, loser_score, played_at "
        "FROM match_history ORDER BY id DESC LIMIT ?;";

    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MatchRecord r;
        r.id           = sqlite3_column_int (stmt, 0);
        r.winner_name  = (const char*)sqlite3_column_text(stmt, 1);
        r.loser_name   = (const char*)sqlite3_column_text(stmt, 2);
        r.winner_score = sqlite3_column_int (stmt, 3);
        r.loser_score  = sqlite3_column_int (stmt, 4);
        r.played_at    = (const char*)sqlite3_column_text(stmt, 5);
        results.push_back(r);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Database::RankEntry> Database::getRanking()
{
    std::vector<RankEntry> results;
    if (!db_) return results;

    const char* sql = R"(
        SELECT name,
               SUM(wins)   AS total_wins,
               SUM(losses) AS total_losses
        FROM (
            SELECT winner_name AS name, COUNT(*) AS wins, 0 AS losses
              FROM match_history GROUP BY winner_name
            UNION ALL
            SELECT loser_name AS name, 0 AS wins, COUNT(*) AS losses
              FROM match_history GROUP BY loser_name
        )
        GROUP BY name
        ORDER BY total_wins DESC;
    )";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RankEntry e;
        e.name   = (const char*)sqlite3_column_text(stmt, 0);
        e.wins   = sqlite3_column_int(stmt, 1);
        e.losses = sqlite3_column_int(stmt, 2);
        results.push_back(e);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<HeroRecord> Database::getAllHeroes()
{
    std::vector<HeroRecord> results;
    if (!db_) return results;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT id, name, monologue, archetype, class_name, trainer_id, hp, ad, arm, asset_path, ultimate_name "
        "FROM heroes ORDER BY id;",
        -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HeroRecord h;
        h.id          = sqlite3_column_int (stmt, 0);
        h.name        = (const char*)sqlite3_column_text(stmt, 1);
        h.monologue   = (const char*)sqlite3_column_text(stmt, 2);
        h.archetype   = sqlite3_column_int (stmt, 3);
        h.class_name  = (const char*)sqlite3_column_text(stmt, 4);
        h.trainer_id  = sqlite3_column_int (stmt, 5);
        h.hp          = sqlite3_column_int (stmt, 6);
        h.ad          = sqlite3_column_int (stmt, 7);
        h.arm         = sqlite3_column_int (stmt, 8);
        h.asset_path  = (const char*)sqlite3_column_text(stmt, 9);
        h.ultimate_name = (const char*)sqlite3_column_text(stmt, 10);
        results.push_back(h);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<TrainerRecord> Database::getAllTrainers()
{
    std::vector<TrainerRecord> results;
    if (!db_) return results;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT id, name, discipline, ability_type, ability_name, ability_desc, "
        "color_r, color_g, color_b, portrait_path, card_path FROM trainers ORDER BY id;",
        -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TrainerRecord t;
        t.id            = sqlite3_column_int (stmt, 0);
        t.name          = (const char*)sqlite3_column_text(stmt, 1);
        t.discipline    = (const char*)sqlite3_column_text(stmt, 2);
        t.ability_type  = sqlite3_column_int (stmt, 3);
        t.ability_name  = (const char*)sqlite3_column_text(stmt, 4);
        t.ability_desc  = (const char*)sqlite3_column_text(stmt, 5);
        t.color_r       = sqlite3_column_int (stmt, 6);
        t.color_g       = sqlite3_column_int (stmt, 7);
        t.color_b       = sqlite3_column_int (stmt, 8);
        t.portrait_path = (const char*)sqlite3_column_text(stmt, 9);
        t.card_path     = (const char*)sqlite3_column_text(stmt, 10);
        results.push_back(t);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<ShopItemRecord> Database::getAllShopItems()
{
    std::vector<ShopItemRecord> results;
    if (!db_) return results;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT id,name,description,base_price,rarity,type,category,max_rounds,"
        "icon_path,effect_type,effect_value,effect_target,trainer_id,hero_id "
        "FROM shop_items ORDER BY id;",
        -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ShopItemRecord r;
        r.id           = sqlite3_column_int (stmt, 0);
        r.name         = (const char*)sqlite3_column_text(stmt, 1);
        r.description  = (const char*)sqlite3_column_text(stmt, 2);
        r.base_price   = sqlite3_column_int (stmt, 3);
        r.rarity       = sqlite3_column_int (stmt, 4);
        r.type         = sqlite3_column_int (stmt, 5);
        r.category     = sqlite3_column_int (stmt, 6);
        r.max_rounds   = sqlite3_column_int (stmt, 7);
        r.icon_path    = (const char*)sqlite3_column_text(stmt, 8);
        r.effect_type  = (const char*)sqlite3_column_text(stmt, 9);
        r.effect_value = (float)sqlite3_column_double(stmt, 10);
        r.effect_target = (const char*)sqlite3_column_text(stmt, 11);
        r.trainer_id   = sqlite3_column_int (stmt, 12);
        r.hero_id      = sqlite3_column_int (stmt, 13);
        results.push_back(r);
    }
    sqlite3_finalize(stmt);
    return results;
}

HeroRecord Database::getHeroById(int id)
{
    HeroRecord h{};
    if (!db_) return h;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT id, name, monologue, archetype, class_name, trainer_id, hp, ad, arm, asset_path "
        "FROM heroes WHERE id = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        h.id          = sqlite3_column_int (stmt, 0);
        h.name        = (const char*)sqlite3_column_text(stmt, 1);
        h.monologue   = (const char*)sqlite3_column_text(stmt, 2);
        h.archetype   = sqlite3_column_int (stmt, 3);
        h.class_name  = (const char*)sqlite3_column_text(stmt, 4);
        h.trainer_id  = sqlite3_column_int (stmt, 5);
        h.hp          = sqlite3_column_int (stmt, 6);
        h.ad          = sqlite3_column_int (stmt, 7);
        h.arm         = sqlite3_column_int (stmt, 8);
        h.asset_path  = (const char*)sqlite3_column_text(stmt, 9);
    }
    sqlite3_finalize(stmt);
    return h;
}

TrainerRecord Database::getTrainerById(int id)
{
    TrainerRecord t{};
    if (!db_) return t;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT id,name,discipline,ability_type,ability_name,ability_desc,"
        "color_r,color_g,color_b,portrait_path,card_path FROM trainers WHERE id = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        t.id            = sqlite3_column_int (stmt, 0);
        t.name          = (const char*)sqlite3_column_text(stmt, 1);
        t.discipline    = (const char*)sqlite3_column_text(stmt, 2);
        t.ability_type  = sqlite3_column_int (stmt, 3);
        t.ability_name  = (const char*)sqlite3_column_text(stmt, 4);
        t.ability_desc  = (const char*)sqlite3_column_text(stmt, 5);
        t.color_r       = sqlite3_column_int (stmt, 6);
        t.color_g       = sqlite3_column_int (stmt, 7);
        t.color_b       = sqlite3_column_int (stmt, 8);
        t.portrait_path = (const char*)sqlite3_column_text(stmt, 9);
        t.card_path     = (const char*)sqlite3_column_text(stmt, 10);
    }
    sqlite3_finalize(stmt);
    return t;
}

ShopItemRecord Database::getShopItemById(int id)
{
    ShopItemRecord r{};
    if (!db_) return r;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "SELECT id,name,description,base_price,rarity,type,category,max_rounds,"
        "icon_path,effect_type,effect_value,effect_target,trainer_id,hero_id FROM shop_items WHERE id = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        r.id           = sqlite3_column_int (stmt, 0);
        r.name         = (const char*)sqlite3_column_text(stmt, 1);
        r.description  = (const char*)sqlite3_column_text(stmt, 2);
        r.base_price   = sqlite3_column_int (stmt, 3);
        r.rarity       = sqlite3_column_int (stmt, 4);
        r.type         = sqlite3_column_int (stmt, 5);
        r.category     = sqlite3_column_int (stmt, 6);
        r.max_rounds   = sqlite3_column_int (stmt, 7);
        r.icon_path    = (const char*)sqlite3_column_text(stmt, 8);
        r.effect_type  = (const char*)sqlite3_column_text(stmt, 9);
        r.effect_value = (float)sqlite3_column_double(stmt, 10);
        r.effect_target = (const char*)sqlite3_column_text(stmt, 11);
        r.trainer_id   = sqlite3_column_int (stmt, 12);
        r.hero_id      = sqlite3_column_int (stmt, 13);
    }
    sqlite3_finalize(stmt);
    return r;
}

bool Database::updateHeroStats(int id, int hp, int ad, int arm)
{
    if (!db_) return false;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
        "UPDATE heroes SET hp = ?, ad = ?, arm = ? WHERE id = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, hp);
    sqlite3_bind_int(stmt, 2, ad);
    sqlite3_bind_int(stmt, 3, arm);
    sqlite3_bind_int(stmt, 4, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        printf("[Database] Hero id=%d atualizado: HP=%d AD=%d ARM=%d\n", id, hp, ad, arm);
        return true;
    }
    return false;
}

bool Database::clearHistory()
{
    bool ok = execute("DELETE FROM match_history;");
    if (ok) printf("[Database] Historico apagado.\n");
    return ok;
}
