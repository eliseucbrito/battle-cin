#include "../include/database.h"
#include "../include/game_defs.h"
#include <cstdio>
#include <ctime>

// abre ou cria o arquivo de banco e inicializa as tabelas
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
    seedHeroes();
}

// fecha a conexao quando o servidor encerra
Database::~Database()
{
    if (db_) {
        sqlite3_close(db_);
        printf("[Database] Banco fechado.\n");
    }
}

// executa um SQL simples sem retorno
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

// cria as tabelas do banco caso ainda nao existam
void Database::createTables()
{
    execute(R"(
        CREATE TABLE IF NOT EXISTS heroes (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT    NOT NULL,
            discipline  TEXT    NOT NULL,
            archetype   INTEGER NOT NULL,
            hp          INTEGER NOT NULL,
            ad          INTEGER NOT NULL,
            arm         INTEGER NOT NULL,
            asset_path  TEXT    NOT NULL
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

void Database::seedHeroes()
{
    if (!db_) return;

    // verifica se ja existem herois
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM heroes;", -1, &stmt, nullptr);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (count > 0) return;

    printf("[Database] Inserindo herois...\n");

    const char* disciplines[] = {
        "Estrutura de Dados",
        "Orient. a Objetos",
        "Computacao Grafica",
        "Algoritmos",
        "Sist. Operacionais"
    };

    for (int i = 0; i < N_HEROES; i++) {
        const HeroDefEntry& h = HERO_DEFS[i];

        sqlite3_stmt* ins = nullptr;
        const char* sql =
            "INSERT INTO heroes (name, discipline, archetype, hp, ad, arm, asset_path) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);";

        sqlite3_prepare_v2(db_, sql, -1, &ins, nullptr);
        sqlite3_bind_text(ins, 1, h.name,                      -1, SQLITE_STATIC);
        sqlite3_bind_text(ins, 2, disciplines[h.archetype],    -1, SQLITE_STATIC);
        sqlite3_bind_int (ins, 3, h.archetype);
        sqlite3_bind_int (ins, 4, h.hp);
        sqlite3_bind_int (ins, 5, h.ad);
        sqlite3_bind_int (ins, 6, h.arm);
        sqlite3_bind_text(ins, 7, h.assetPath,                 -1, SQLITE_STATIC);
        sqlite3_step(ins);
        sqlite3_finalize(ins);
    }

    printf("[Database] %d herois inseridos.\n", N_HEROES);
}

// salva o resultado de uma partida no historico
bool Database::saveMatch(const std::string& winner_name,
                         const std::string& loser_name,
                         int winner_score,
                         int loser_score)
{
    if (!db_) return false;

    // gera o timestamp atual
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

// retorna as ultimas N partidas
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

// retorna o ranking agregado pelo historico de partidas
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
        "SELECT id, name, discipline, archetype, hp, ad, arm, asset_path FROM heroes;",
        -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HeroRecord h;
        h.id         = sqlite3_column_int (stmt, 0);
        h.name       = (const char*)sqlite3_column_text(stmt, 1);
        h.discipline = (const char*)sqlite3_column_text(stmt, 2);
        h.archetype  = sqlite3_column_int (stmt, 3);
        h.hp         = sqlite3_column_int (stmt, 4);
        h.ad         = sqlite3_column_int (stmt, 5);
        h.arm        = sqlite3_column_int (stmt, 6);
        h.asset_path = (const char*)sqlite3_column_text(stmt, 7);
        results.push_back(h);
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
        "SELECT id, name, discipline, archetype, hp, ad, arm, asset_path FROM heroes WHERE id = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        h.id         = sqlite3_column_int (stmt, 0);
        h.name       = (const char*)sqlite3_column_text(stmt, 1);
        h.discipline = (const char*)sqlite3_column_text(stmt, 2);
        h.archetype  = sqlite3_column_int (stmt, 3);
        h.hp         = sqlite3_column_int (stmt, 4);
        h.ad         = sqlite3_column_int (stmt, 5);
        h.arm        = sqlite3_column_int (stmt, 6);
        h.asset_path = (const char*)sqlite3_column_text(stmt, 7);
    }
    sqlite3_finalize(stmt);
    return h;
}

// atualiza os stats de um heroi pelo id
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

// apaga todo o historico de partidas
bool Database::clearHistory()
{
    bool ok = execute("DELETE FROM match_history;");
    if (ok) printf("[Database] Historico apagado.\n");
    return ok;
}

