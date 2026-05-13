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
            description   TEXT    NOT NULL DEFAULT '',
            archetype     INTEGER NOT NULL,
            class_name    TEXT    NOT NULL DEFAULT '',
            trainer_id    INTEGER NOT NULL,
            hp            INTEGER NOT NULL,
            ad            INTEGER NOT NULL,
            arm           INTEGER NOT NULL,
            asset_path      TEXT    NOT NULL DEFAULT '',
            ultimate_name_1 TEXT    DEFAULT 'Poder Ativo',
            ultimate_name_2 TEXT    DEFAULT 'Poder Ativo',
            ultimate_name_3 TEXT    DEFAULT 'Poder Ativo',
            dying_phrase    TEXT    DEFAULT '...',
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
            archetype_id    INTEGER NOT NULL DEFAULT -1
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
        { "Abel Guilhermino",  "Engenharia de Software", 0, "Rally (+AD)",  0, 120, 215,
          "assets/trainers/presentation/Abel_Guilhermino_presentation.png",
          "assets/trainers/card/Abel_Guilhermino_card.png" },
        { "Alex Sandro",       "Algoritmos",             1, "Shield (+ARM)", 200, 50, 50,
          "assets/trainers/presentation/Alex_Sandro_presentation.png",
          "assets/trainers/card/Alex_Sandro_card.png" },
        { "David Junior",       "Redes de Computadores",  2, "Heal (+HP)",  255, 165, 0,
          "assets/trainers/presentation/David_presentation.png",
          "assets/trainers/card/David_Junior_card.png" },
        { "Francisco Paulo",   "Estrutura de Dados e Orientação a Objetos", 3, "Frenzy (+AS)", 0, 255, 0,
          "assets/trainers/presentation/Francisco_Paulo_presentation.png",
          "assets/trainers/card/Francisco_Paulo_card.png" },
        { "Juliano Lyoda",     "Introdução a Programação", 0, "Rally (+AD)",  0, 255, 255,
          "assets/trainers/presentation/Juliano_lyoda_presentation.png",
          "assets/trainers/card/Juliano_lyoda_card.png" },
        { "Valeria Cesario",   "Banco de Dados",         1, "Shield (+ARM)", 128, 0, 128,
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
    struct HeroSeed {
        const char* name; int arch; int trainId; const char* cls;
        int hp; int ad; int arm;
        const char* asset; const char* monologue; const char* desc;
        const char* u1; const char* u2; const char* u3;
        const char* dying;
    };
    HeroSeed hdata[] = {
        // Abel's Heroes (ID 1)
        { "O Construto de Busca",    0, 1, "Tank",     350, 15, 18, "assets/heroes/O_Construto_de_Busca.png",   "A ineficiência é uma falha. A otimização é inevitável. Prepare-se para ser percorrido de ponta a ponta.", "Forjado nas profundezas do código legado, este golem não entende o conceito de 'força bruta', apenas 'complexidade de tempo O(1)'.", "Busca em Largura (AoE)", "Busca Bloqueada!", "Escudo de Dados", "Timeout atingido..." },
        { "O Guardiao dos Discos",   1, 1, "Fighter",  280, 22, 10, "assets/heroes/O_Guardiao_dos_Discos.png",  "Você tentou um DROP TABLE no meu domínio... agora enfrentará o peso de todo o meu ROLLBACK.", "Uma entidade colossal que guarda os registros e as temidas planilhas da SecGrad. Ele odeia queries lentas.", "Inner Join", "Setor Defeituoso!", "Giro de Leitura", "Estou Brickando..." },
        { "O Mestre Parser",         2, 1, "Mage",     200, 35,  5, "assets/heroes/O_Mestre_Parser.png",        "Erro fatal na linha da sua vida. Falha ao gerar o código executável do seu futuro.", "O pesadelo final e absoluto. Uma monstruosidade multi-cabeças que devora código imperfeito e cospe otimização.", "Syntax Error", "Erro de Sintaxe!", "Recursao Infinita", "Stack Overflow!" },
        { "O Cientista Polarizado",  2, 1, "Mage",     220, 32,  3, "assets/heroes/O_Cientista_Polarizado.png", "O fluxo magnético através de uma superfície fechada é zero... mas o impacto do meu campo no seu sistema será absoluto.", "Flutuando sobre as leis da física clássica, este cientista vive em um estado de instabilidade absoluta.", "Lei de Faraday", "Curto-Circuito!", "Pulso de Faraday", "Polaridade invertida..." },
        { "O Chip-Mestre",           4, 1, "Support",  240, 12, 10, "assets/heroes/O_Chip-Mestre.png",          "Tudo se resume a 0 e 1, alto e baixo, verdadeiro e falso. O seu erro lógico não tem lugar no meu clock.", "A personificação do hardware de baixo nível. Dizem que ele foi montado usando velhos kits de laboratório esquecidos.", "Clock Overdrive", "Cache Renovado!", "Sinal Estavel", "Sobreaquecimento!" },

        // Alex's Heroes (ID 2)
        { "A Burocrata do UML",      0, 2, "Tank",     360, 13, 20, "assets/heroes/A_Burocrata_do_UML.png",     "O cliente não sabe o que quer, vocês não sabem o que programar, mas eu sei exatamente como documentar a ruína de vocês.", "Ela carrega o peso de todos os projetos mal especificados. Armou-se com pranchetas flamejantes.", "Mudança de Escopo", "Heranca Pesada!", "Classe Abstrata", "Processo cancelado." },
        { "O Filosofo do Dilema",    1, 2, "Fighter",  270, 24, 12, "assets/heroes/O_Filosofo_do_Dilema.png",   "Se P implica Q, e você é P, então a sua aniquilação, Q, é uma mera tautologia.", "Duas cabeças de pedra que nunca concordam, vivendo na eterna tabela-verdade do CIn.", "Paradoxo Lógico", "Crise Existencial!", "Dúvida Metódica", "Isso e real?" },
        { "O Artista Vectorial",     2, 2, "Mage",     190, 38,  4, "assets/heroes/O_Artista_Vectorial.png",    "Vocês não estão alinhados ao centro! Que dor visual... deixem-me agrupá-los na dor!", "Ele não vê um campo de batalha, apenas um grande canvas com grids desalinhados.", "Auto-Layout", "Renderizacao!", "Malha de Pontos", "Corrompendo canvas..." },
        { "O Inspetor Flaky",        0, 2, "Tank",     215, 30,  2, "assets/heroes/O_Inspetor_Flaky.png",       "Passou na minha máquina... mas será que passa no seu escudo?", "Um ser paranoico, assombrado pelos bugs que escaparam para a produção.", "Assert.Throws()", "Teste Falhou!", "Corrupcao de Pilha", "Falha intermitente..." },
        { "O Treinador Python",      4, 2, "Support",  250, 14,  8, "assets/heroes/O_Treinador_Python.png",     "Olá, mundo! Vejo que você esqueceu dois espaços na linha 4 de sua estratégia... pereça.", "O primeiro grande desafio dos calouros. Ele parece incrivelmente amigável e acessível.", "Import Antigravity", "Import Vital!", "Thread Segura", "Indentacao errada!" },

        // Juliano's Heroes (ID 5)
        { "O Escudo IF",           0, 5, "Tank",     355, 14, 19, "assets/heroes/O_Escudo_IF.png",          "Se o ataque for maior que 0, proteja!", "Uma estrutura condicional que bloqueia o fluxo de ataques.", "Condição de Proteção", "Then Defensivo", "Else de Ferro", "Condição não atendida..." },
        { "O Loop WHILE",          1, 5, "Fighter",  275, 23, 11, "assets/heroes/O_Loop_WHILE.png",         "Enquanto houver inimigos, ataque!", "Um ciclo infinito de golpes que não para até a vitória.", "Ataque Infinito", "Iteração Fatal", "Break de Dano", "Loop interrompido..." },
        { "O Feiticeiro PRINT",    2, 5, "Mage",     195, 37,  4, "assets/heroes/O_Feiticeiro_PRINT.png",     "Exibindo sua derrota no terminal...", "O primeiro comando de todo aprendiz, manifestando realidade.", "Mensagem Mágica", "Log de Fogo", "Debug Arcano", "Erro na saída..." },
        { "O BREAK Furtivo",       3, 5, "Assassin", 218, 31,  2, "assets/heroes/O_BREAK_Furtivo.png",      "Interrompendo sua existência agora.", "Uma saída abrupta que pega todos de surpresa.", "Interrupção Letal", "Saída Forçada", "Pulo de Escopo", "Execução finalizada." },
        { "O TRYCATCH",            4, 5, "Support",  245, 13,  9, "assets/heroes/O_TRYCATCH.png",           "Não se preocupe, eu trato esse erro.", "O mestre da resiliência, tratando falhas como oportunidades.", "Tratamento de Erros", "Bloco de Cura", "Finally Vital", "Exception não tratada!" },

        // Valeria's Heroes (ID 6)
        { "PRIMARY KEY",           0, 6, "Tank",     365, 12, 21, "assets/heroes/PRIMARY_KEY.png",          "Eu sou único. Você é redundante.", "O pilar de integridade que sustenta toda a base.", "Identificador Único", "Restrição Forte", "Índice de Defesa", "Conflito de chave!" },
        { "INNER JOIN",            1, 6, "Fighter",  285, 21, 13, "assets/heroes/INNER_JOIN.png",           "Estamos conectados pelo mesmo ID.", "A força da união que combina o melhor de cada lado.", "Combate Combinado", "União de Tabelas", "On Clause Hit", "Relação perdida..." },
        { "SELECT * FROM",         2, 6, "Mage",     185, 39,  3, "assets/heroes/SELECT_ALL_FROM.png",      "Eu vejo tudo o que você esconde.", "A visão absoluta que extrai cada detalhe do inimigo.", "Invocação de Dados", "Query Devastadora", "Fetch Arcano", "Tabela não encontrada!" },
        { "DROP TABLE",            3, 6, "Assassin", 212, 33,  1, "assets/heroes/DROP_TABLE.png",           "Sua existência foi deletada.", "O aniquilador de estruturas, apagando o passado sem deixar rastros.", "Apagão Silencioso", "Delete sem Where", "Truncate Letal", "Commit falhou..." },
        { "ROLLBACK",              4, 6, "Support",  255, 11, 10, "assets/heroes/ROLLBACK.png",             "Voltando ao estado anterior à sua chegada.", "A chance de desfazer o inevitável e restaurar a ordem.", "Restauração Vital", "Undo de Dano", "Transação Segura", "Erro no Log!" },

        // Francisco's Heroes (ID 4)
        { "A Classe Abstrata",     0, 4, "Tank",     350, 15, 18, "assets/heroes/A_Classe_Abstrata.png",    "Sou apenas o molde da sua destruição.", "O conceito puro da defesa, esperando ser implementado.", "Molde de Proteção", "Interface Rígida", "Contrato de Ferro", "Instanciação impossível." },
        { "O Objeto Herança",      1, 4, "Fighter",  280, 22, 10, "assets/heroes/O_Objeto_Heranca.png",     "Meu poder vem da minha super-classe.", "Aquele que herda o poder das gerações passadas.", "Atributos do Pai", "Super Chamada", "Extensão de Dano", "Base corrompida..." },
        { "O Construtor",          2, 4, "Mage",     200, 35,  5, "assets/heroes/O_Construtor.png",         "New Hero(this_victory);", "A faísca inicial que dá vida a novas instâncias de combate.", "Criação Mágica", "New Instance!", "Init Arcano", "Destruído..." },
        { "O Polimorfismo",        3, 4, "Assassin", 220, 32,  3, "assets/heroes/O_Polimorfismo.png",       "Eu sou o que eu precisar ser.", "A capacidade de assumir qualquer forma para vencer.", "Múltiplas Formas", "Override Letal", "Dynamic Binding", "Tipo inválido..." },
        { "O Encapsulamento",      4, 4, "Support",  240, 12, 10, "assets/heroes/O_Encapsulamento.png",     "Meus métodos são privados, sua dor é pública.", "O segredo bem guardado que protege o núcleo vital.", "Proteção de Dados", "Setter de Cura", "Getter de Vida", "Acesso negado!" },

        // David's Heroes (ID 3)
        { "Firewall",              0, 3, "Tank",     358, 14, 19, "assets/heroes/Firewall.png",             "Acesso negado. Pacote descartado.", "A barreira de fogo que filtra quem entra e quem sai.", "Bloqueio Externo", "Regra de Negativa", "Filtro de Pacotes", "Porta invadida!" },
        { "Pacote TCP",            1, 3, "Fighter",  278, 24, 11, "assets/heroes/Pacote_TCP.png",           "Receba cada bit da minha fúria.", "A garantia de que o golpe chegará intacto e em ordem.", "Conexão Confiável", "Three-Way Handshake", "Ack de Dano", "Conexão resetada..." },
        { "Roteador",              2, 3, "Mage",     192, 36,  4, "assets/heroes/Roteador.png",             "Próximo salto: sua derrota.", "O guia dos dados, sempre encontrando o caminho mais rápido.", "Direcionamento Mágico", "Salto de Rede", "Tabela de Rotas", "Sem rota para o host." },
        { "Packet Sniffer",        3, 3, "Assassin", 217, 31,  2, "assets/heroes/Packet_Sniffer.png",       "Eu li seus pacotes antes de você enviá-los.", "O espião silencioso que conhece cada movimento seu.", "Espionagem de Rede", "Captura de Dados", "Wireshark Hit", "Criptografia forte!" },
        { "Access Point",          4, 3, "Support",  248, 13,  9, "assets/heroes/Access_Point.png",         "Sinal forte, aliados prontos.", "O farol de conectividade que fortalece os aliados.", "Conexão Aliada", "Sinal Wi-Fi", "DHCP de Cura", "Fora de alcance..." },
    };

    for (const auto& h : hdata) {
        const char* sql = "INSERT INTO heroes (name,monologue,description,archetype,class_name,trainer_id,hp,ad,arm,asset_path,ultimate_name_1,ultimate_name_2,ultimate_name_3,dying_phrase)"
                          "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, h.name,  -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, h.monologue, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, h.desc,   -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 4, h.arch);
        sqlite3_bind_text(stmt, 5, h.cls,   -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 6, h.trainId);
        sqlite3_bind_int (stmt, 7, h.hp);
        sqlite3_bind_int (stmt, 8, h.ad);
        sqlite3_bind_int (stmt, 9, h.arm);
        sqlite3_bind_text(stmt, 10, h.asset, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, h.u1, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 12, h.u2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 13, h.u3, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 14, h.dying, -1, SQLITE_STATIC);
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

    struct NewItemData { std::string name; const char* desc; int price; int rarity; const char* eff; float val; int archId; std::string icon; };
    NewItemData archetypeItems[] = {
        { "Abraco do Arcanjo", "Cura um aliado automaticamente quando ele cai abaixo de 20% de vida (cooldown 30s, sem custo de mana)", 100, 1, "auto_heal", 0.2f, 4, "assets/shop/abraco_do_arcanjo.png" },
        { "Adaga Envenenada", "Ataques basicos envenenam, causando 3% de vida maxima como dano verdadeiro por 4s (nao acumula)", 80, 1, "poison_hit", 0.03f, 3, "assets/shop/adaga_envenenada.png" },
        { "Botas da Agilidade", "Aumenta o movespeed base em +20", 50, 0, "buff_speed", 20.0f, -1, "assets/shop/botas_da_agilidade.png" },
        { "Cajado Sagrado", "Ao usar ultimate, reduz o cooldown de todas as outras habilidades em 4s.", 120, 2, "ult_cd_red", 4.0f, 2, "assets/shop/cajado_sagrado.png" },
        { "Egide do Guardiao", "Move-se 20% mais devagar, mas reduz todo dano recebido em 15%", 90, 1, "dmg_reduction", 0.15f, 0, "assets/shop/egide_do_guardiao.png" },
        { "Elmo do Esquecimento", "Imunidade temporaria", 110, 2, "temp_immunity", 0.f, 0, "assets/shop/elmo_do_esquecimento.png" },
        { "Escudo do Pacto", "Proteger um aliado transfere 20% do dano recebido para voce (limitado a 30% da sua vida).", 100, 1, "dmg_transfer", 0.2f, 0, "assets/shop/escudo_do_pacto.png" },
        { "Excalibur", "Seu proximo ataque apos habilidade causa 100% de dano extra em area.", 150, 2, "aoe_next_hit", 1.0f, 1, "assets/shop/excalibur.png" },
        { "Foice do Enforcado", "Executa inimigos com menos de 15% de vida se acertados pelas costas", 130, 2, "execute_backstab", 0.15f, 3, "assets/shop/foice_do_enforcado.png" },
        { "Lamina Dupla", "Acertar um mesmo inimigo duas vezes seguidas se cura baseado no dano causado", 95, 1, "lifesteal_combo", 0.f, 1, "assets/shop/lamina_dupla.png" },
        { "Luvas da Paciencia", "Reduz o tempo de recarga das skills em 10%", 60, 0, "cd_reduction", 0.1f, -1, "assets/shop/luvas_da_paciencia.png" },
        { "Machado do Berserker", "Cada ataque basico aumenta o dano do proximo em 5% (acumula ate 30%). Focado em abate sustentado.", 110, 2, "stack_ad", 0.05f, 1, "assets/shop/machado_do_berserker.png" },
        { "Manto do Espreitador", "Ao ficar 3s sem tomar dano, entra em furtividade por 4s (primeiro ataque causa 50% mais dano).", 105, 1, "stealth_ooc", 0.5f, 3, "assets/shop/manto_do_espreitador.png" },
        { "Martelo do Gigante", "Habilidades de impacto causam stun de 0.75s (cooldown 8s por alvo)", 100, 1, "stun_on_hit", 0.75f, 1, "assets/shop/martelo_do_gigante.png" },
        { "Olho de Sauron", "Habilidades de area deixam um rastro que causa 40% do dano original por 2s.", 140, 2, "aoe_trail", 0.4f, 2, "assets/shop/olho_de_sauron.png" },
        { "Tomo Amaldicoado", "Habilidades aplicam queimadura que causa 2% de vida maxima como dano magico por 3s.", 85, 1, "burn_skill", 0.02f, 2, "assets/shop/tomo_amaldicoado.png" },
        { "Tomo da Sabedoria", "Ao curar um aliado com menos de 30% de vida, a cura e 50% mais eficaz.", 95, 1, "bonus_heal_low", 0.5f, 4, "assets/shop/tomo_da_sabedoria.png" },
        { "Tomo Inspirador", "Habilidades de cura também concedem 15% de velocidade de ataque por 3s.", 85, 1, "heal_buff_as", 0.15f, 4, "assets/shop/tomo_inspirador.png" }
    };

    for (auto& it : archetypeItems) {
        currentItemId++;

        const char* sql = "INSERT INTO shop_items (id,name,description,base_price,rarity,type,category,effect_type,effect_value,icon_path,archetype_id)"
                          "VALUES (?,?,?,?,?,?,?,?,?,?,?);";
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        sqlite3_bind_int (stmt, 1, currentItemId);
        sqlite3_bind_text(stmt, 2, it.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, it.desc, -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 4, it.price);
        sqlite3_bind_int (stmt, 5, it.rarity);
        sqlite3_bind_int (stmt, 6, 1); // type
        sqlite3_bind_int (stmt, 7, 1); // category: hero/archetype
        sqlite3_bind_text(stmt, 8, it.eff, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 9, it.val);
        sqlite3_bind_text(stmt, 10, it.icon.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int (stmt, 11, it.archId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    printf("[Database] Itens inseridos vinculados a treinadores e arquetipos.\n");
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
        "SELECT id, name, monologue, description, archetype, class_name, trainer_id, hp, ad, arm, asset_path, ultimate_name_1, ultimate_name_2, ultimate_name_3, dying_phrase "
        "FROM heroes ORDER BY id;",
        -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HeroRecord h;
        h.id          = sqlite3_column_int (stmt, 0);
        h.name        = (const char*)sqlite3_column_text(stmt, 1);
        h.monologue   = (const char*)sqlite3_column_text(stmt, 2);
        h.description = (const char*)sqlite3_column_text(stmt, 3);
        h.archetype   = sqlite3_column_int (stmt, 4);
        h.class_name  = (const char*)sqlite3_column_text(stmt, 5);
        h.trainer_id  = sqlite3_column_int (stmt, 6);
        h.hp          = sqlite3_column_int (stmt, 7);
        h.ad          = sqlite3_column_int (stmt, 8);
        h.arm         = sqlite3_column_int (stmt, 9);
        h.asset_path  = (const char*)sqlite3_column_text(stmt, 10);
        h.ultimate_name_1 = (const char*)sqlite3_column_text(stmt, 11);
        h.ultimate_name_2 = (const char*)sqlite3_column_text(stmt, 12);
        h.ultimate_name_3 = (const char*)sqlite3_column_text(stmt, 13);
        h.dying_phrase    = (const char*)sqlite3_column_text(stmt, 14);
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
        "icon_path,effect_type,effect_value,effect_target,trainer_id,archetype_id "
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
        r.archetype_id = sqlite3_column_int (stmt, 13);
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
        "SELECT id, name, monologue, description, archetype, class_name, trainer_id, hp, ad, arm, asset_path "
        "FROM heroes WHERE id = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        h.id          = sqlite3_column_int (stmt, 0);
        h.name        = (const char*)sqlite3_column_text(stmt, 1);
        h.monologue   = (const char*)sqlite3_column_text(stmt, 2);
        h.description = (const char*)sqlite3_column_text(stmt, 3);
        h.archetype   = sqlite3_column_int (stmt, 4);
        h.class_name  = (const char*)sqlite3_column_text(stmt, 5);
        h.trainer_id  = sqlite3_column_int (stmt, 6);
        h.hp          = sqlite3_column_int (stmt, 7);
        h.ad          = sqlite3_column_int (stmt, 8);
        h.arm         = sqlite3_column_int (stmt, 9);
        h.asset_path  = (const char*)sqlite3_column_text(stmt, 10);
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
        "icon_path,effect_type,effect_value,effect_target,trainer_id,archetype_id FROM shop_items WHERE id = ?;",
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
        r.archetype_id = sqlite3_column_int (stmt, 13);
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
