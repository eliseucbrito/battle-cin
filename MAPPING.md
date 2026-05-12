# Mapeamento OOP — Battle-CIn

> Projeto desenvolvido para as disciplinas de **Estrutura de Dados** e **Orientação a Objetos**.
> C++17 + Raylib + SQLite3.

---

## 1. Hierarquia de Classes (Herança)

### 1.1 Hero — Classe Base Abstrata

```
Hero  (abstrata)                    include/hero.h:22   (virtual ~Hero = default)
├── HeroTank   : public Hero        include/hero_tank.h
├── HeroFighter : public Hero       include/hero_fighter.h
├── HeroMage   : public Hero        include/hero_mage.h
├── HeroAssassin : public Hero      include/hero_assassin.h
└── HeroSupport : public Hero       include/hero_support.h
```

**Conceitos:** Herança (`: public Hero`), classe abstrata (9 métodos virtual puro `= 0`), destrutor virtual, `virtual` + `override`.

**O que cada subclasse sobrescreve:**
- `HeroMage::calculateDamage()` — dano mágico ignora 50% da armadura (`include/hero_mage.h:20`)
- `HeroAssassin::chooseMove()` — movimento prioritário para inimigo com menor HP (`include/hero_assassin.h:23`)
- `HeroTank::activateUltimate()` — ganha armadura bônus (`src/heroes/hero_tank.cpp:19`)
- `HeroFighter::activateUltimate()` — ganha ataque bônus (`src/heroes/hero_fighter.cpp:19`)
- `HeroSupport::activateUltimate()` — cura aliados próximos (`src/heroes/hero_support.cpp:20`)

### 1.2 Item — Classe Base Abstrata (Prototype + Template Method)

```
Item  (abstrata)                    include/shop.h:13   (virtual ~Item = default)
├── HealthPotion   : public Item    include/shop.h:63
├── StrengthGem    : public Item    include/shop.h:83
├── SteelArmor     : public Item    include/shop.h:99
├── SpeedScroll    : public Item    include/shop.h:115
├── BerserkerElixir : public Item   include/shop.h:132
├── ArcaneOrb      : public Item    include/shop.h:149
├── PhoenixFeather : public Item    include/shop.h:165
├── MirrorShield   : public Item    include/shop.h:181
├── RallyAll       : public Item    include/shop.h:201
├── HealWave       : public Item    include/shop.h:218
└── GoldRush       : public Item    include/shop.h:235
```

**Conceitos:** Herança, polimorfismo, `virtual` puro + `override`, destrutor virtual.

### 1.3 PricingStrategy — Strategy Pattern

```
PricingStrategy  (abstrata)         include/shop.h:254
└── SupplyDemandPricing : public PricingStrategy   include/shop.h:261
```

---

## 2. Polimorfismo

### 2.1 Polimorfismo de Classe (Virtual Dispatch)

**Hero** — 9 métodos virtuais puros definem o "contrato" que cada subclasse deve implementar:

| Método Virtual Puro | Onde é usado | Onde é implementado |
|---|---|---|
| `archetype()` | Em todo o jogo para identificar o tipo | Cada subclasse retorna uma constante diferente |
| `baseStats()` | `HeroFactory` para estatísticas iniciais | Cada arquivo `.h` da subclasse |
| `calculateDamage(const Hero&)` | `Hero::attackTarget()` em `hero.cpp:122` | `HeroMage` anula 50% da armadura |
| `shouldTriggerUltimate()` | `Game::tickUltimates()` em `game.cpp:588` | Lógica específica por arquétipo |
| `activateUltimate(...)` | `Game::tickUltimates()` | Cada herói ativa efeito diferente |

Exemplo concreto em `src/server/game.cpp:596-604`:
```cpp
if (hero.shouldTriggerUltimate() &&                     // dispatch virtual
    hero.ultimateProbabilityPerTick() > randomFloat()) {
    hero.activateUltimate(allies, allyCount,            // dispatch virtual
                          enemies, enemyCount);
}
```

**Item** — polimorfismo via `clone()` (Prototype):
```cpp
// include/shop.h:33
virtual std::unique_ptr<Item> clone() const = 0;

// Exemplo em shop.h:64
std::unique_ptr<Item> clone() const override {
    return std::make_unique<HealthPotion>(*this);
}
```

### 2.2 Polimorfismo via Templates (Generic Programming)

`LinkedList<T>` e `PriorityQueue<T>` são templates que funcionam com qualquer tipo:

```cpp
// include/linked_list.h:12
template<typename T>
class LinkedList { ... };

// Uso em include/hero.h:154
LinkedList<ActiveEffect> effects_;
for (const auto& e : effects_) { ... }  // range-based for via Iterator
```

---

## 3. Modificadores de Acesso

| Modificador | Quem pode acessar | Exemplo no código |
|---|---|---|
| `public:` | Todos | `Hero::hp()`, `Hero::attackTarget()`, `Trainer::heroAt()` |
| `protected:` | Subclasses | `Hero::hp_`, `Hero::ad_`, `Hero::x_`, `Hero::alive_` (23 membros em `include/hero.h:23-65`) |
| `private:` | Só a própria classe | `Trainer::heroes_`, `Game::trainers_`, `Database::db_`, `Shop::gold_` |

Exemplo de encapsulamento em `include/hero.h`:
```cpp
class Hero {
protected:
    int hp_;        // subclasses HERDAM e modificam diretamente
    int ad_;
    bool alive_;
    // ... 20 membros protegidos
public:
    int hp() const { return hp_; }        // getter público (const)
    bool alive() const { return alive_; } // getter público (const)
    virtual int calculateDamage(const Hero& target) const; // polimórfico
};
```

---

## 4. Ponteiros e Referências

### 4.1 Referências (evitam cópia)

```cpp
// include/hero.h:129 — referência mutável
bool attackTarget(Hero& target);

// include/hero.h:89 — referência constante (somente leitura)
int calculateDamage(const Hero& target) const;
```

### 4.2 Ponteiros Inteligentes (`std::unique_ptr`)

```cpp
// include/hero_factory.h:14 — Factory retorna unique_ptr
static std::unique_ptr<Hero> create(uint8_t archetype, uint8_t ownerId);

// include/trainer.h:18 — Trainer "possui" heróis via unique_ptr
std::vector<std::unique_ptr<Hero>> heroes_;

// include/shop.h:278 — Catálogo de protótipos
std::vector<std::unique_ptr<Item>> prototypes_;
```

### 4.3 Ponteiros Raw (acesso não-dono)

```cpp
// src/server/game.cpp:492 — ponteiro raw para alvo
Hero* target = nullptr;

// include/tournament_tree.h:16-18 — árvore binária ligada por ponteiros
MatchNode* left;
MatchNode* right;
MatchNode* parent;
```

---

## 5. Design Patterns

| Padrão | Onde | Descrição |
|---|---|---|
| **Factory Method** | `include/hero_factory.h` | Cria a subclasse correta de `Hero` baseado no `archetype` |
| **Template Method** | `include/shop.h:17-22` | `Item::use()` define esqueleto: `canApply()` → `apply()` → `onApplied()` |
| **Strategy** | `include/shop.h:254` | `PricingStrategy` é injetado em `Shop` para cálculo de preço |
| **Prototype** | `include/shop.h:33` + `ItemCatalog` | Itens são clonados de protótipos via `clone()` virtual |
| **Iterator** | `include/linked_list.h:21-29` | `LinkedList::Iterator` permite `for (auto& e : lista)` |
| **Composition** | `Game` → `Trainer` → `Hero` | Hierarquia de composição: Game tem Trainers, Trainer tem Heroes |
| **State Machine** | `src/server/game.cpp:207-276` | Jogo é uma máquina de estados finitos (`PHASE_SELECT` → `PHASE_BATTLE` → ...) |

---

## 6. CRUD com Banco de Dados (SQLite3)

Todas as operações CRUD estão em `include/database.h` e `src/database.cpp`:

| Operação | Método | SQL |
|---|---|---|
| **CREATE** | `saveMatch()` | `INSERT INTO match_history (...) VALUES (?,?,?,?,?)` |
| **CREATE** | `seedHeroes()` | `INSERT INTO heroes (...) VALUES (?,?,?,?,?,?,?)` |
| **READ** | `getRecentMatches(n)` | `SELECT * FROM match_history ORDER BY id DESC LIMIT ?` |
| **READ** | `getRanking()` | Agregação com `SUM(wins) GROUP BY name` |
| **READ** | `getAllHeroes()` | `SELECT * FROM heroes` |
| **READ** | `getHeroById(id)` | `SELECT * FROM heroes WHERE id = ?` |
| **UPDATE** | `updateHeroStats(id,hp,ad,arm)` | `UPDATE heroes SET hp=?, ad=?, arm=? WHERE id=?` |
| **DELETE** | `clearHistory()` | `DELETE FROM match_history` |

O banco é populado automaticamente na inicialização via `Database::seedHeroes()`, que insere os 10 heróis do jogo na tabela `heroes` (prevenindo duplicatas com verificações de existência).

---

## 7. Membros Estáticos e Constantes

### 7.1 Static Methods

```cpp
// include/hero_factory.h:14
static std::unique_ptr<Hero> create(uint8_t archetype, uint8_t ownerId);

// include/shop.h:275
static ShopItemInfo toShopItemInfo(const Item& item, int price);
```

### 7.2 Static Constexpr (compile-time)

```cpp
// include/game_defs.h
static constexpr int N_HEROES = 10;
static constexpr HeroDefEntry HERO_DEFS[N_HEROES] = { ... };
```

### 7.3 Static File-scope (variáveis globais do módulo)

```cpp
// src/client/renderer.cpp
static std::vector<FloatingText> floatingTexts;  // Renderer.cpp:601
static std::vector<VisualEffect> visualEffects;   // Renderer.cpp:641
```

---

## 8. Métodos Const

Garantem que o método não modifica o objeto:

```cpp
// include/hero.h:144-166 — TODOS os getters são const
int  hp()  const { return hp_; }
int  ad()  const { return ad_; }
bool alive() const { return alive_; }

// include/hero.h:89 — não modifica o herói, só calcula
int calculateDamage(const Hero& target) const;
```

---

## 9. Member Initializer Lists

```cpp
// src/heroes/hero.cpp:13-25
Hero::Hero(uint8_t ownerId)
    : hp_(0), maxHp_(0), ad_(0), arm_(0), as_rate_(0),
      ms_delay_(0), x_(0), y_(0), alive_(true),
      moveTimer_(0.f), attackTimer_(0.f),
      effects_(), ultCooldownTimer_(0.f),
      ultActiveTimer_(0.f), ultActive_(false),
      attackCount_(0), hasCustomStats_(false),
      customHp_(0), customAd_(0), customArm_(0),
      ownerId_(ownerId), heroDefIndex_(0xFF),
      targetFocus_(-1), itemCount_(0) {}
```

---

## 10. `explicit` Keyword

Evita conversões implícitas que poderiam causar bugs:

```cpp
// include/hero.h:68
explicit Hero(uint8_t ownerId);

// include/database.h:39
explicit Database(const std::string& path = "battle_cin.db");
```

---

## 11. Operator Overloading

```cpp
// include/linked_list.h:25-28 — Iterator para range-based for
T& operator*() const;
T* operator->() const;
Iterator& operator++();
bool operator!=(const Iterator& other) const;

// src/server/game.cpp:14 — PriorityQueue max-heap
bool operator>(const Combatant& other) const {
    return asRate > other.asRate;
}
```

---

## 12. Move Semantics (Rule of 5)

`LinkedList`, `PriorityQueue`, e `Database` implementam:

```cpp
// include/linked_list.h:35-49
LinkedList(LinkedList&& other) noexcept;            // move constructor
LinkedList& operator=(LinkedList&& other) noexcept; // move assignment
LinkedList(const LinkedList&) = delete;             // copy deleted
```

---

## 13. Estruturas de Dados (EDOO)

| Estrutura | Arquivo | Implementação |
|---|---|---|
| **Linked List** (genérica) | `include/linked_list.h` | Template, com Iterator, push_back, removeFirst, removeAll, forEach |
| **Priority Queue** (genérica) | `include/priority_queue.h` | Max-heap, template, com siftUp/siftDown, grow dinâmico |
| **Graph** (grid pathfinding) | `include/graph.h` | Lista de adjacência, BFS para encontrar caminho mínimo |
| **Tournament Tree** | `include/tournament_tree.h` | Árvore binária balanceada, nós ligados por ponteiros |

---

## 14. Diagrama de Composição

```
Game
├── Trainer[2]                  (composição por array)
│   ├── std::vector<unique_ptr<Hero>>
│   │   ├── HeroTank            (polimorfismo)
│   │   ├── HeroFighter
│   │   ├── HeroMage
│   │   ├── HeroAssassin
│   │   └── HeroSupport
│   │       └── LinkedList<ActiveEffect>   (linked list genérica)
│   └── generalItems_[]
├── Database                    (SQLite3 CRUD)
├── Graph                       (BFS pathfinding)
├── TournamentTree              (árvore binária)
└── Shop
    ├── ItemCatalog             (Prototype pattern)
    │   └── vector<unique_ptr<Item>>
    ├── PricingStrategy*        (Strategy pattern)
    └── vector<unique_ptr<Item>> (stock atual)
```

---

## 15. Fluxo do Jogo (State Machine)

```
PHASE_SELECT (treinador → heróis)
    ↓
PHASE_VS_INTRO (tela de apresentação)
    ↓
PHASE_POSITIONING (posicionar heróis no grid)
    ↓
PHASE_BATTLE (combate automático com targeting)
    ↓
PHASE_ROUND_END (resultado da rodada)
    ↓
PHASE_SHOP (comprar itens)
    ↓
PHASE_POSITIONING (próxima rodada)
    ↓  (ou)
PHASE_MATCH_END (fim de jogo)
```
