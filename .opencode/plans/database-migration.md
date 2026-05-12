# Plano: Migração de Dados Hardcoded → Banco de Dados

**Branch**: `feat/shop-economy` (atual)

---

## Visão Geral

Atualmente os dados de treinadores, heróis e itens estão duplicados em 3 lugares:
- `include/game_defs.h` — usado por `renderer.cpp` e `game.cpp`
- `src/client/main.cpp` — usado pela tela de seleção
- `src/database.cpp` — seed do DB (nunca lido de volta)

**Objetivo**: O banco SQLite vira a fonte única da verdade. Tudo carrega de lá no startup.

---

## 1. Schema do Banco de Dados

### 1.1 Tabela `trainers` (nova)

```sql
CREATE TABLE IF NOT EXISTS trainers (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT    NOT NULL,
    discipline    TEXT    NOT NULL,
    ability_type  INTEGER NOT NULL,   -- ABILITY_RALLY, ABILITY_SHIELD_WALL, etc.
    ability_name  TEXT    NOT NULL,   -- "Rally (+AD)", "Shield (+ARM)", etc.
    ability_desc  TEXT    NOT NULL,   -- descrição longa (placeholder por enquanto)
    color_r       INTEGER NOT NULL DEFAULT 80,
    color_g       INTEGER NOT NULL DEFAULT 160,
    color_b       INTEGER NOT NULL DEFAULT 230,
    portrait_path TEXT    NOT NULL,   -- imagem de apresentação
    card_path     TEXT    NOT NULL    -- imagem do card na seleção
);
```

### 1.2 Tabela `heroes` (recriada — DROP + CREATE)

```sql
CREATE TABLE IF NOT EXISTS heroes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,
    monologue   TEXT    NOT NULL DEFAULT '',
    archetype   INTEGER NOT NULL,     -- ARCHETYPE_TANK, FIGHTER, MAGE, ASSASSIN, SUPPORT
    trainer_id  INTEGER NOT NULL,     -- FK → trainers.id
    hp          INTEGER NOT NULL,
    ad          INTEGER NOT NULL,
    arm         INTEGER NOT NULL,
    asset_path  TEXT    NOT NULL,
    FOREIGN KEY (trainer_id) REFERENCES trainers(id)
);
```

Mudanças vs schema antigo:
- Remove `discipline` (agora está no trainer via FK)
- Adiciona `monologue` (será preenchido com placeholder e depois com frases dos personagens)
- Adiciona `trainer_id` FK

### 1.3 Tabela `shop_items` (nova)

```sql
CREATE TABLE IF NOT EXISTS shop_items (
    id              INTEGER PRIMARY KEY,      -- itemId fixo (0-10)
    name            TEXT    NOT NULL,
    description     TEXT    NOT NULL,
    base_price      INTEGER NOT NULL,
    rarity          INTEGER NOT NULL,         -- ITEM_RARITY_COMMON/UNCOMMON/RARE/EPIC
    type            INTEGER NOT NULL,         -- ITEM_TYPE_CONSUMABLE/EQUIPMENT/TEMPORARY/UNIQUE
    category        INTEGER NOT NULL,         -- ITEM_CATEGORY_HERO/GENERAL
    max_rounds      INTEGER NOT NULL DEFAULT -1,
    icon_path       TEXT    NOT NULL DEFAULT '',
    effect_type     TEXT    NOT NULL,         -- "heal_pct", "buff_ad_flat", "buff_ad_pct",
                                              -- "buff_arm_flat", "buff_as_pct", "revive", "gold_bonus"
    effect_value    REAL    NOT NULL,         -- valor numérico (0.3 = 30%, 15 = +15 AD, etc.)
    effect_target   TEXT    NOT NULL DEFAULT 'self'  -- "self" ou "all_allies"
);
```

Os 11 itens cabem em 7 tipos de efeito genéricos:

| effect_type      | valor     | target     | Exemplo             |
|------------------|-----------|------------|---------------------|
| `heal_pct`       | 0.30      | self       | HealthPotion (30%)  |
| `buff_ad_flat`   | 15        | self       | StrengthGem (+15)   |
| `buff_arm_flat`  | 10        | self       | SteelArmor (+10)    |
| `buff_as_pct`    | 0.30      | self       | SpeedScroll (+30%)  |
| `buff_ad_pct`    | 0.50      | self       | BerserkerElixir     |
| `buff_ad_flat`   | 10        | all_allies | RallyAll (+10)      |
| `heal_pct`       | 0.25      | all_allies | HealWave (25%)      |
| `revive`         | 0.50      | self       | PhoenixFeather      |
| `gold_bonus`     | 50        | self       | GoldRush            |

---

## 2. Seed Data

### 2.1 INSERT INTO trainers

```sql
INSERT INTO trainers (name, discipline, ability_type, ability_name, ability_desc, color_r, color_g, color_b, portrait_path, card_path)
VALUES
('Abel Guilhermino',  'Estrutura de Dados',    0, 'Rally (+AD)',      '', 80,  160, 230,
 'assets/trainers/presentation/Abel_Guilhermino_presentation.png',
 'assets/trainers/card/Abel_Guilhermino_card.png'),
('Alex Sandro',       'Orient. a Objetos',     1, 'Shield (+ARM)',    '', 230, 80,  130,
 'assets/trainers/presentation/Alex_Sandro_presentation.png',
 'assets/trainers/card/Alex_Sandro_card.png'),
('David Junior',      'Algoritmos',            2, 'Heal (+HP)',       '', 80,  230, 160,
 'assets/trainers/presentation/David_presentation.png',
 'assets/trainers/card/David_Junior_card.png'),
('Francisco Paulo',   'Banco de Dados',        3, 'Frenzy (+AS)',     '', 230, 180, 80,
 'assets/trainers/presentation/Francisco_Paulo_presentation.png',
 'assets/trainers/card/Francisco_Paulo_card.png'),
('Juliano Lyoda',     'Redes de Computadores', 0, 'Rally (+AD)',      '', 160, 80,  230,
 'assets/trainers/presentation/Juliano_lyoda_presentation.png',
 'assets/trainers/card/Juliano_lyoda_card.png'),
('Valeria Cesario',   'Engenharia de Software', 1, 'Shield (+ARM)',   '', 230, 80,  230,
 'assets/trainers/presentation/Valeria_Cesario_presentation.png',
 'assets/trainers/card/Valeria_Cesario_card.png');
```

### 2.2 INSERT INTO heroes

```sql
INSERT INTO heroes (name, monologue, archetype, trainer_id, hp, ad, arm, asset_path)
VALUES
-- Abel Guilhermino (id=1)
('O Construto de Busca',     '', 0, 1, 350, 15, 18, 'assets/heroes/O_Construto_de_Busca.png'),
('O Guardiao dos Discos',    '', 1, 1, 280, 22, 10, 'assets/heroes/O_Guardiao_dos_Discos.png'),
('O Mestre Parser',          '', 2, 1, 200, 35,  5, 'assets/heroes/O_Mestre_Parser.png'),
('O Cientista Polarizado',   '', 3, 1, 220, 32,  3, 'assets/heroes/O_Cientista_Polarizado.png'),
('O Chip-Mestre',            '', 4, 1, 240, 12, 10, 'assets/heroes/O_Chip-Mestre.png'),
-- Alex Sandro (id=2)
('A Burocrata do UML',       '', 0, 2, 360, 13, 20, 'assets/heroes/A_Burocrata_do_UML.png'),
('O Filosofo do Dilema',     '', 1, 2, 270, 24, 12, 'assets/heroes/O_Filosofo_do_Dilema.png'),
('O Artista Vectorial',      '', 2, 2, 190, 38,  4, 'assets/heroes/O_Artista_Vectorial.png'),
('O Inspetor Flaky',         '', 3, 2, 215, 30,  2, 'assets/heroes/O_Inspetor_Flaky.png'),
('O Treinador Python',       '', 4, 2, 250, 14,  8, 'assets/heroes/O_Treinador_Python.png');
```

Monólogos vazios (`''`) — placeholder. Serão preenchidos quando os dados dos personagens forem definidos.

### 2.3 INSERT INTO shop_items

```sql
INSERT INTO shop_items (id, name, description, base_price, rarity, type, category, max_rounds, effect_type, effect_value, effect_target)
VALUES
(0,  'Pocao de Vida',     'Restaura 30% da HP maxima',         25,  0, 0, 1, -1, 'heal_pct',      0.30, 'self'),
(1,  'Gema de Forca',     '+15 AD permanente',                 45,  1, 1, 1, -1, 'buff_ad_flat',   15,  'self'),
(2,  'Armadura de Aco',   '+10 ARM permanente',                50,  1, 1, 1, -1, 'buff_arm_flat',  10,  'self'),
(3,  'Pergaminho Veloz',  '+30% AS por 2 rodadas',             60,  2, 2, 1,  2, 'buff_as_pct',    0.30, 'self'),
(4,  'Elixir Berserker',  '+50% AD por 3 rodadas',             70,  2, 2, 1,  3, 'buff_ad_pct',    0.50, 'self'),
(5,  'Orbe Arcano',       '+25% de dano magico',               100, 3, 3, 1, -1, 'buff_ad_pct',    0.25, 'self'),
(6,  'Pena da Fenix',     'Revive com 50% HP se morto',        130, 3, 3, 1, -1, 'revive',         0.50, 'self'),
(7,  'Escudo Espelhado',  '+20 ARM permanente',                120, 3, 3, 1, -1, 'buff_arm_flat',  20,  'self'),
(8,  'Rally Total',       '+10 AD para todos aliados (1 rodada)', 60, 1, 0, 0, 1, 'buff_ad_flat',  10,  'all_allies'),
(9,  'Onda Curativa',     'Cura 25% HP maxima de todos aliados',  70, 1, 0, 0, -1, 'heal_pct',     0.25, 'all_allies'),
(10, 'Corrida do Ouro',   '+50 gold bonus no proximo round',   40,  0, 0, 0, -1, 'gold_bonus',    50,   'self');
```

---

## 3. Mudanças no Código

### 3.1 `include/database.h` — Novos structs e métodos

```cpp
// NOVO: dados de treinador do banco
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

// NOVO: dados de item no banco
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
};
```

Novos métodos na classe `Database`:
```cpp
std::vector<TrainerRecord> getAllTrainers();
std::vector<HeroRecord>    getAllHeroes();         // já existe
std::vector<ShopItemRecord> getAllShopItems();     // NOVO
```

### 3.2 `src/database.cpp` — Criação de tabelas e seed

Mudanças:
- `createTables()`: adiciona `trainers` e `shop_items`, recria `heroes` (DROP se schema velho)
- `seedHeroes()` → renomeado para `seedAll()`: insere trainers, heroes e shop_items
- Novos métodos: `getAllTrainers()`, `getAllShopItems()`
- `getAllHeroes()` atualizado para novo schema (agora inclui `monologue` e `trainer_id`)

### 3.3 `include/game_defs.h` — Substituir arrays por vetores carregados

**Remove**: `TRAINER_DEFS[]` e `HERO_DEFS[]` constexpr (e as structs velhas `TrainerDefEntry`, `HeroDefEntry`)

**Adiciona**:
```cpp
// Vetores carregados do DB no startup
extern std::vector<TrainerDefEntry> g_trainerDefs;
extern std::vector<HeroDefEntry>    g_heroDefs;
```

As structs podem ser mantidas (compatibilidade), mas populadas do DB.

Ou, mais limpo: mantém as structs, remove os `constexpr` arrays, define os vetores em `game.cpp` com `extern` no header.

### 3.4 `src/client/renderer.h` — Popula do vetor carregado

Remove os `const` arrays `TRAINERS[]` e `HEROES[]` de `main.cpp`, substitui por referência aos vetores globais carregados.

As structs `TrainerDef` e `HeroDef` em `renderer.h` ganham um campo extra ou são substituídas pelas structs do `database.h`.

**Abordagem**: Mudar as definições locais de `main.cpp` para usar os vetores carregados via `game_defs.h`.

### 3.5 `src/client/main.cpp` — Remove arrays hardcoded

Remove `TRAINERS[]` e `HEROES[]` (linhas 16-37). Os loops que iteram sobre trainers/heroes passam a usar `g_trainerDefs.size()` e `g_heroDefs.size()` e acessar via índice.

Funções como `heroesForTrainer()` e `heroFilteredToGlobal()` mudam de `HERO_DEFS[i].trainerIndex` para `g_heroDefs[i].trainerId`.

### 3.6 `src/client/renderer.cpp` — Múltiplos pontos de uso

Todos os lugares que referenciam `TRAINER_DEFS[x]` ou `HERO_DEFS[x]` mudam para `g_trainerDefs[x]` ou `g_heroDefs[x]`. O padrão de acesso continua igual (índice), só muda a origem.

Pontos específicos:
- Linha 553-554: `TRAINER_DEFS[i].portraitPath` → `g_trainerDefs[i].portraitPath`
- Linha 558: `HERO_DEFS[i].assetPath` → `g_heroDefs[i].assetPath`
- Linha 730: `TRAINER_DEFS[tId]` → `g_trainerDefs[tId]`
- Linha 769: `HERO_DEFS[hDefIdx]` → `g_heroDefs[hDefIdx]`
- Linha 1398: `TRAINER_DEFS[tIdx].name` → `g_trainerDefs[tIdx].name`
- Linha 1672: `HERO_DEFS[hero.heroDefIndex].name` → `g_heroDefs[hero.heroDefIndex].name`
- Linha 1815: `HERO_DEFS[hs.heroDefIndex].name` → `g_heroDefs[hs.heroDefIndex].name`
- Linha 1954: `TRAINER_DEFS[tId].name` → `g_trainerDefs[tId].name`
- Linha 2024: `TRAINER_DEFS[tId].abilityType` → `g_trainerDefs[tId].abilityType`
- Linha 2093: `HERO_DEFS[hdi].name` → `g_heroDefs[hdi].name`

E as verificações `tId < N_TRAINERS` / `hDefIdx < N_HEROES` viram `.size()` checks.

### 3.7 `src/server/game.cpp` — Init via vetores

- `initFromSelections()` (linha 68-80): `TRAINER_DEFS[tIdx]` → `g_trainerDefs[tIdx]`, `HERO_DEFS[hIdx]` → `g_heroDefs[hIdx]`
- Loops que filtram heróis por trainer: `HERO_DEFS[i].trainerIndex` → `g_heroDefs[i].trainerId`

Além disso, `Game::Game()` deve chamar `db_.getAllTrainers()` e `db_.getAllHeroes()` para popular `g_trainerDefs` e `g_heroDefs`.

### 3.8 `include/shop.h` + `src/shop.cpp` — Refatorar para itens genéricos

**Abordagem**: Substituir as 11 classes de Item concretas por uma única classe `GenericItem` que carrega seus atributos de um `ShopItemRecord`.

```cpp
class GenericItem : public Item {
    ShopItemRecord record_;
public:
    explicit GenericItem(const ShopItemRecord& rec);
    std::string name()        const override { return record_.name; }
    std::string description() const override { return record_.description; }
    int         basePrice()   const override { return record_.base_price; }
    uint8_t     rarity()      const override { return (uint8_t)record_.rarity; }
    uint8_t     type()        const override { return (uint8_t)record_.type; }
    uint8_t     itemId()      const override { return (uint8_t)record_.id; }
    uint8_t     category()    const override { return (uint8_t)record_.category; }
    int         maxRounds()   const override { return record_.max_rounds; }
    std::unique_ptr<Item> clone() const override { return std::make_unique<GenericItem>(record_); }
    void apply(Hero& hero, int currentRound) const override;
    bool canApply(const Hero& hero) const override;
};
```

O método `apply()` vira um switch/case baseado em `effect_type`:
```
heal_pct       → hero.healHp(hero.maxHp() * value)
buff_ad_flat   → hero.setAd(hero.ad() + (int)value)
buff_ad_pct    → hero.setAd(hero.ad() * (1.0f + value))
buff_arm_flat  → hero.setArm(hero.arm() + (int)value)
buff_as_pct    → hero.setAsRate(hero.asRate() * (1.0f + value))
revive         → (marcado como revive — lógica no Game)
gold_bonus     → (marcado como gold — lógica no Shop)
```

**`ItemCatalog`**: Em vez de registrar 11 protótipos hardcoded, carrega do DB via `db.getAllShopItems()`.

**`canApply()` para PhoenixFeather**: Retorna false (só ativa quando o herói morre — lógica no Game.cpp). O `canApply` genérico verifica o `effect_type`.

---

## 4. Ordem de Execução

1. **`database.h`** — Adicionar `TrainerRecord`, `ShopItemRecord`, novos métodos
2. **`database.cpp`** — `createTables()` com as 3 tabelas novas, `seedAll()`, `getAllTrainers()`, `getAllShopItems()`
3. **`game_defs.h`** — Remover arrays constexpr, declarar vetores globais `extern`
4. **`game.h` / `game.cpp`** — `Game::Game()` carrega vetores do DB; trocar referências hardcoded
5. **`shop.h` / `shop.cpp`** — Implementar `GenericItem`, refatorar `ItemCatalog` para carregar do DB
6. **`renderer.h`** — Ajustar structs `TrainerDef`/`HeroDef` se necessário
7. **`main.cpp`** — Remover arrays locais, usar vetores globais
8. **`renderer.cpp`** — Trocar todas as referências `TRAINER_DEFS`/`HERO_DEFS` → `g_trainerDefs`/`g_heroDefs`
9. Apagar `battle_cin.db` existente (ou o código detecta schema velho e recria)
10. Compilar e testar

---

## 5. Verificação

```bash
cd build && cmake .. && make -j$(nproc)
./meu_projeto                  # modo 2P — seleção deve mostrar trainers/heroes do DB
./meu_projeto --solo           # modo solo — bot deve funcionar
# Verificar: tela de seleção, cards, VS screen, loja, fim de partida
```
