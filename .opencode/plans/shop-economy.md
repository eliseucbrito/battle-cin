# Plano de Implementação: Economia e Loja entre Rounds

**Branch**: `feat/shop-economy`
**Base**: `feat/local-multiplayer-keyboard` (branch atual)

---

## Visão Geral — Alterações em Relação ao Plano Original (Fase 6)

1. **Sem Fase 5 (Draft/Ban)**: A loja entra direto após `PHASE_ROUND_END`, sem draft/banimento.
2. **Itens armazenados no SQLite**: O catálogo de itens fica na tabela `shop_items` do banco existente. Carregado para uma `HashTable` em memória no startup para lookup rápido.
3. **Itens persistem entre rounds**: Itens passivos são permanentes na partida. Itens ativáveis são consumidos ao uso (1 uso por round) e precisam ser recomprados.
4. **Bot compra automaticamente**: Lógica simples de compra priorizando heróis com menor stat base.

---

## Fluxo de Fases Atualizado

```
SELECT → VS_INTRO → POSITIONING → BATTLE → ROUND_END → SHOP → POSITIONING → BATTLE → ...
                                                                         ↑_________________|
(reset do ciclo na volta de SHOP para POSITIONING)

No match end:
... → ROUND_END → MATCH_END
```

O ciclo pós-primeiro round fica: `ROUND_END → SHOP → POSITIONING → BATTLE → ROUND_END → ...`

---

## Estruturas de Dados EDOO

| # | Estrutura | Arquivo | Uso |
|---|-----------|---------|-----|
| 1 | `HashTable<K,V>` | `include/hash_table.h` | Catálogo de itens em memória (lookup O(1) por ID) |

A `HashTable` é a estrutura de dados EDOO exigida. Os dados são persistidos no SQLite, mas o lookup em tempo de jogo usa a HashTable.

---

## Tarefas

### T1 — Sistema de Gold

**Objetivo**: Introduzir moeda de ouro para progressão entre rounds.

**Detalhes**:
- Adicionar ao `Trainer`:
  ```cpp
  int gold_ = 0;
  int gold() const { return gold_; }
  void addGold(int amount) { gold_ += amount; }
  bool spendGold(int amount) {
      if (gold_ < amount) return false;
      gold_ -= amount;
      return true;
  }
  void resetGold() { gold_ = 0; }
  ```
- Recompensas de gold por round (em `endRound()`):
  - Vitória: 100 gold
  - Derrota: 60 gold
  - Empate: 80 gold
  - Bônus por streak consecutivo: +20 por vitória seguida (máx +60)
- Gold inicial: 200 (para permitir compras no primeiro shop fase)
- Adicionar ao `GameSnapshot`: `uint16_t gold[2];`
- Adicionar ao `GameSnapshot`: `uint8_t winStreak[2];` (para bônus de streak)
- Adicionar ao `TrainerNetState`: `uint16_t gold;`

**Arquivos**:
- `include/trainer.h` — campo `gold_`, getters/setters
- `src/trainer.cpp` — implementação
- `src/server/game.cpp` — adicionar gold em `endRound()`, resetar em reset de partida
- `include/protocol.h` — campo `gold` no `GameSnapshot` e `TrainerNetState`

---

### T2 — Estrutura HashTable (EDOO)

**Objetivo**: Criar tabela hash genérica com encadeamento separado para uso como catálogo de itens.

**Detalhes**:
- Criar `include/hash_table.h` — template `HashTable<K, V>`:
  ```cpp
  template<typename K, typename V>
  class HashTable {
      struct Entry {
          K key;
          V value;
          Entry* next;
          Entry(const K& k, const V& v, Entry* n = nullptr)
              : key(k), value(v), next(n) {}
      };

      Entry** buckets_;
      size_t bucketCount_;
      size_t size_;

      size_t hash(const K& key) const;  // especializado para int/uint8_t/string

  public:
      HashTable(size_t bucketCount = 16);
      ~HashTable();

      void insert(const K& key, const V& value);
      V* find(const K& key);
      bool remove(const K& key);
      void forEach(const std::function<void(const K&, const V&)>& fn) const;
      size_t size() const { return size_; }
  };
  ```
- Hash para `uint8_t`: simplesmente `key % bucketCount_`
- Hash para `std::string`: `std::hash<std::string>{}(key) % bucketCount_`

**Arquivos**:
- `include/hash_table.h` — novo arquivo (header-only template)

---

### T3 — Catálogo de Itens (SQLite + HashTable)

**Objetivo**: Definir os itens da loja e armazená-los no banco de dados SQLite.

**Definição dos Itens**:

| ID | Nome | Tipo | Custo | Efeito | Descrição |
|----|------|------|-------|--------|-----------|
| 0 | Espada Curta | PASSIVE | 80 | +8 AD permanente | "Aumenta o ataque base" |
| 1 | Escudo de Ferro | PASSIVE | 80 | +5 ARM permanente | "Aumenta a armadura base" |
| 2 | Vitalidade | PASSIVE | 100 | +40 Max HP permanente | "Aumenta a vida maxima" |
| 3 | Botas Ageis | PASSIVE | 120 | +30% Attack Speed | "Aumenta a velocidade de ataque" |
| 4 | Pocao de Cura | ACTIVABLE | 50 | Restaura 25% HP | "Cura um heroi no inicio do round" |
| 5 | Pocao de Furia | ACTIVABLE | 60 | +50% AD por 8s | "Aumenta o dano no inicio do round" |
| 6 | Elixir de Escudo | ACTIVABLE | 70 | +15 ARM por 8s | "Aumenta a armadura no inicio do round" |

**Detalhes**:
- Definir constantes: `ITEM_PASSIVE = 0`, `ITEM_ACTIVABLE = 1`
- Definir constantes de efeito: `EFFECT_AD = 0`, `EFFECT_ARM = 1`, `EFFECT_HP = 2`, `EFFECT_AS = 3`, `EFFECT_HEAL_PCT = 4`, `EFFECT_AD_PCT_TEMP = 5`, `EFFECT_ARM_TEMP = 6`
- Criar `include/shop_defs.h`:
  ```cpp
  struct ShopItem {
      uint8_t id;
      const char* name;
      uint8_t cost;
      uint8_t type;       // ITEM_PASSIVE ou ITEM_ACTIVABLE
      uint8_t effect;     // EFFECT_AD, EFFECT_ARM, etc.
      uint8_t magnitude;  // valor do efeito
      const char* description;
  };
  ```
- Criar array estático `SHOP_ITEM_DEFS[]` com os 7 itens pré-definidos
- Adicionar tabela `shop_items` no SQLite via `Database::createTables()`
- Adicionar `Database::seedShopItems()` que insere os itens se a tabela estiver vazia
- Adicionar `Database::getAllShopItems()` que retorna `std::vector<ShopItem>`
- No `Game` ou em uma função `initShopCatalog()`: carregar itens do DB e popular `HashTable<uint8_t, ShopItem>`

**Arquivos**:
- `include/shop_defs.h` — novo arquivo
- `include/database.h` — adicionar métodos para shop_items
- `src/database.cpp` — criar tabela, seed, e getAllShopItems
- `src/server/game.cpp` — carregar catálogo no init

---

### T4 — Compra e Aplicação de Itens

**Objetivo**: Permitir que jogadores comprem itens durante a Shop Phase e aplicá-los aos heróis.

**Protocolo**:
```cpp
// Novo input type
#define INPUT_SHOP_BUY  5

#pragma pack(push, 1)
struct ShopPacket {
    uint8_t playerId;
    uint8_t type;        // INPUT_SHOP_BUY
    uint8_t heroIndex;   // qual heroi recebe o item (0-2)
    uint8_t itemId;      // ID do item na loja
};
#pragma pack(pop)
```

**Lógica no servidor** (`Game::handleShopBuy()`):
1. Verificar se `phase_ == PHASE_SHOP`
2. Verificar se `itemId` existe no catálogo
3. Verificar se o jogador tem gold suficiente
4. Verificar se o herói do jogador está no range (0-2)
5. Deduzir gold do trainer
6. Aplicar o efeito:
   - **Passivo**: aplicar diretamente no herói (modificar stat base)
   - **Ativável**: registrar no herói para ativação automática no início do próximo round
7. Adicionar ao `GameSnapshot` os itens comprados por cada jogador para exibição:

**Itens passivos — aplicação imediata**:
- `+8 AD`: `hero.setAd(hero.ad() + 8); hero.recalcStats();`
- `+5 ARM`: `hero.setArm(hero.arm() + 5); hero.recalcStats();`
- `+40 HP`: `hero.setCustomStats(hero.maxHp() + 40, ...)` — precisa de mecânica de custom stats, ou diretamente no reset de round
- `+30% AS`: `hero.setAsRate(hero.asRate() * 1.3f);`

Atenção: Itens passivos precisam persistir entre rounds. A abordagem é **modificar os stats customizados do herói** que são aplicados em `resetStats()` / `resetForRound()`. Isso requer adicionar um sistema de "permanent buffs" ao Trainer ou Hero.

**Estratégia para itens passivos**: Adicionar ao `Hero` uma lista de `PermanentItem`:
```cpp
struct PermanentItem {
    uint8_t itemId;
    uint8_t effect;
    uint8_t magnitude;
};
```
Em `Hero::resetStats()`, após restaurar stats base + custom, aplicar todos os `PermanentItem` passivos.
Em `Hero::addPermanentItem()`, adicionar item e chamar `recalcStats()`.

**Itens ativáveis — ativação no início do round**:
- Adicionar ao `Hero`: `LinkedList<ShopItem> activableItems_;`
- Quando o round começa (`startBattle()`), para cada herói, aplicar os efeitos ativáveis:
  - `Pocao de Cura`: `hero.healHp(hero.maxHp() * 25 / 100);`
  - `Pocao de Furia`: `hero.applyBuff(BUFF_AD, 8.f);` com magnitude 50% (usar novo buff type `BUFF_AD_PCT`)
  - `Elixir de Escudo`: `hero.applyBuff(BUFF_ARM, 8.f);` com magnitude 15
- Após aplicar, **limpar** a lista de ativáveis (são de uso único por round)

**Efeitos nos buff types** — Adicionar a `protocol.h`:
```cpp
#define BUFF_AD_PCT   4  // +50% AD temporário
```
Modificar `Hero::recalcStats()` para tratar `BUFF_AD_PCT` como multiplicador percentual de AD.

**Adicionar ao `GameSnapshot`**:
```cpp
uint8_t shopItemCount[2][3];   // [player][heroSlot] = numero de itens
uint8_t shopItems[2][3][4];    // [player][heroSlot][itemIdx] = itemId (max 4 por heroi)
```
Isso permite ao cliente mostrar quais itens cada herói possui.

**Arquivos**:
- `include/protocol.h` — `ShopPacket`, `INPUT_SHOP_BUY`, `PHASE_SHOP`, `BUFF_AD_PCT`, campos no `GameSnapshot`, constantes de itens
- `include/hero.h` — `PermanentItem`, `activableItems_`, métodos para adicionar/limpar itens, modificação em `resetStats()`
- `src/heroes/hero.cpp` — implementação de `addPermanentItem()`, modificação de `resetStats()` e `recalcStats()`, ativação de itens no início do round
- `include/game.h` — `handleShopBuy()`, `shopCatalog_`, `PHASE_SHOP`, lógica de transição
- `src/server/game.cpp` — lógica de compra, transição de fases, ativação de itens

---

### T5 — Fase de Loja (Shop Phase)

**Objetivo**: Nova fase do jogo entre `PHASE_ROUND_END` e `PHASE_POSITIONING`.

**Fluxo atualizado no `Game::update()`**:
```
CASE PHASE_ROUND_END:
    phaseTimer_ -= dt;
    if (phaseTimer_ <= 0.f) {
        phase_ = PHASE_SHOP;      // <-- novo
        phaseTimer_ = SHOP_TIME;
    }
    break;

CASE PHASE_SHOP:                   // <-- novo
    phaseTimer_ -= dt;
    updateBotShop();               // bot compra itens
    if (phaseTimer_ <= 0.f) {
        applyActivableItems();     // aplica itens ativáveis no início do round
        startPositioning();
    }
    break;
```

**Constantes**:
```cpp
#define SHOP_TIME  15   // segundos para a fase de loja
#define INITIAL_GOLD 200 // gold inicial
```

**Detalhes adicionais**:
- Na primeira vez (após VS_INTRO → POSITIONING), não há loja. Gold é inicializado.
- Após cada `ROUND_END`, vai para `SHOP` (exceto no Match End).
- Em `startPositioning()` (chamada após SHOP), os heróis já terão os efeitos passivos dos itens (pois foram modificados antes) e os ativáveis são aplicados em `startBattle()`.
- No `MATCH_END`, não há shop.

**Itens por herói**: Limite de **4 itens por herói** (passivos + ativáveis combinados). Se já tiver 4, não pode comprar mais.

**Arquivos**:
- `include/protocol.h` — constante `PHASE_SHOP`, `SHOP_TIME`, `INITIAL_GOLD`
- `include/game.h` — novo estado de shop, métodos
- `src/server/game.cpp` — lógica de transição de fases, `update()`, `applyActivableItems()`, `updateBotShop()`

---

### T6 — UI da Loja no Cliente

**Objetivo**: Renderizar a loja durante a Shop Phase.

**Layout**:
```
┌─────────────────────────────────────────────────────────────┐
│                     LOJA  (15s)                             │
│                                                              │
│  ┌──────────────────┐  ┌──────────────────────────────────┐ │
│  │ GOLD: 260 🪙     │  │  SEU TIME:                       │ │
│  │                  │  │  [Hero 0] [Hero 1] [Hero 2]      │ │
│  │ ┌──────────────┐│  │   ↑ selecionar herói (Q/E)       │ │
│  │ │Espada Curta  ││  │  Itens: [Espada Curta]            │ │
│  │ │ 80g  +8 AD   ││  │                                  │ │
│  │ └──────────────┘│  └──────────────────────────────────┘ │
│  │ ┌──────────────┐│                                        │
│  │ │Escudo Ferro  ││  ┌──────────────────────────────────┐ │
│  │ │ 80g  +5 ARM  ││  │  TIME INIMIGO:                    │ │
│  │ └──────────────┘│  │  [Hero 0] [Hero 1] [Hero 2]      │ │
│  │ ┌──────────────┐│  │  (mostra itens, nao interagir)    │ │
│  │ │Vitalidade    ││  └──────────────────────────────────┘ │
│  │ │ 100g +40 HP  ││                                        │
│  │ └──────────────┘│                                        │
│  │ ... mais itens  │  P1: WASD navegar, Enter comprar     │
│  └──────────────────┘  P2: Setas navegar, Enter comprar    │
└─────────────────────────────────────────────────────────────┘
```

**Input**:
- P1 (esquerda): W/S navegar itens, A/D trocar herói alvo, Enter comprar
- P2 (direita): Up/Down navegar itens, Left/Right trocar herói alvo, KP_Enter comprar

**Detalhes**:
- Novo estado no cliente: processar `PHASE_SHOP`
- `drawShopScreen()` — renderiza a loja com:
  - Grid de itens disponíveis (nome, custo, descrição curta)
  - Gold atual de cada jogador no canto
  - Heróis do time para selecionar quem recebe o item
  - Itens já comprados marcados nos heróis
  - Mensagem de "Gold insuficiente" ou "Herói cheio (4 itens)" se compra falhar
  - Timer da fase no topo
- Cores: gold amarelo para moeda, ícones coloridos por tipo de item
- Itens ativáveis marcados com ícone diferente (ex: borda laranja vs azul para passivos)

**Arquivos**:
- `src/client/main.cpp` — novo `PHASE_SHOP` no input/render loop, `ShopInput` state para cada jogador
- `src/client/renderer.h` — declarar `drawShopScreen()`, structs de dados da loja
- `src/client/renderer.cpp` — implementar renderização da loja

---

### T7 — Extensões do Protocolo para Itens

**Objetivo**: Garantir que o `GameSnapshot` transmita todas as informações necessárias para a UI da loja.

**Adições ao `GameSnapshot`**:
```cpp
uint16_t gold[2];                     // gold de cada jogador
uint8_t  shopPhase;                   // 0 = não na loja, 1 = na loja

// Itens comprados por cada jogador (para exibição na HUD)
uint8_t itemCount[2][3];             // [player][heroIdx] = quantidade de itens
uint8_t items[2][3][4];              // [player][heroIdx][slot] = itemId
```

**Catalogo enviado separadamente**: O cliente já tem os `SHOP_ITEM_DEFS[]` hardcoded (iguais ao banco), então não precisa transmitir o catálogo pelo protocolo. A HashTable é server-side.

**Arquivos**:
- `include/protocol.h` — extensões do `GameSnapshot`
- `src/server/game.cpp` — preencher novos campos em `buildSnapshot()`
- `src/client/main.cpp` — ler novos campos

---

### T8 — Banco de Dados: Tabela de Itens

**Objetivo**: Adicionar tabela `shop_items` ao SQLite existente e persistir compras.

**Nova tabela**:
```sql
CREATE TABLE IF NOT EXISTS shop_items (
    id          INTEGER PRIMARY KEY,
    name        TEXT    NOT NULL,
    type        INTEGER NOT NULL,  -- 0 = PASSIVE, 1 = ACTIVABLE
    cost        INTEGER NOT NULL,
    effect      INTEGER NOT NULL,  -- EFFECT_AD, EFFECT_ARM, etc.
    magnitude   INTEGER NOT NULL,
    description TEXT    NOT NULL
);
```

**Adicionar ao Database**:
- `Database::getAllShopItems()` — retorna `std::vector<ShopItem>`
- `Database::seedShopItems()` — popula a tabela se vazia (usando os mesmos dados de `shop_defs.h`)
- Extra (opcional, não essencial para MVP): tabela `purchase_log` para rastrear compras entre partidas

**Arquivos**:
- `include/database.h` — adicionar métodos
- `src/database.cpp` — implementar

---

### T9 — Bot Shop AI

**Objetivo**: O bot compra itens automaticamente durante a Shop Phase.

**Lógica simples** (`Game::updateBotShop()`):
1. Enquanto `phase_ == PHASE_SHOP` e o bot tem gold >= custo mínimo (50):
   - Escolher aleatoriamente um herói (0-2) que ainda não tem 4 itens
   - Escolher aleatoriamente um item que pode comprar (gold suficiente)
   - Comprar usando `handleShopBuy()`
   - Continuar até não poder comprar mais ou até 3 compras por fase
2. Não comprar se ouro < 50 (item mais barato)

**Arquivos**:
- `src/server/game.cpp` — implementar `updateBotShop()`

---

### T10 — HUD: Gold e Itens durante Batalha

**Objetivo**: Exibir gold e itens ativos no HUD padrão (durante batalha e posicionamento).

**Detalhes**:
- Adicionar exibição de gold no canto do HUD de cada jogador
- Ao lado de cada herói no HUD, mostrar mini-ícones dos itens (sequência de quadrados coloridos)
- Itens ativáveis ativos mostram borda brilhante quando em efeito

**Arquivos**:
- `src/client/renderer.h` — declarar `drawGoldDisplay()`, `drawHeroItems()`
- `src/client/renderer.cpp` — implementar

---

## Ordem de Implementação

| # | Tarefa | Complexidade | Arquivos Afetados | Depends On |
|---|--------|-------------|-------------------|------------|
| T1 | Sistema de gold | Média | trainer.h, trainer.cpp, game.cpp, protocol.h | — |
| T2 | HashTable EDOO | Média | hash_table.h (novo) | — |
| T3 | Catálogo de itens (SQLite + HashTable) | Alta | shop_defs.h (novo), database.h, database.cpp, game.cpp | T2 |
| T4 | Compra e aplicação de itens | Alta | protocol.h, hero.h, hero.cpp, game.h, game.cpp | T3, T1 |
| T5 | Fase de Loja (server) | Média | protocol.h, game.h, game.cpp | T4 |
| T6 | UI da Loja (client) | Alta | main.cpp (client), renderer.h, renderer.cpp | T5 |
| T7 | Protocolo extendido | Baixa | protocol.h, game.cpp, main.cpp | T5 |
| T8 | Banco de dados de itens | Média | database.h, database.cpp | T3 |
| T9 | Bot Shop AI | Baixa | game.h, game.cpp | T5 |
| T10 | HUD gold + itens | Baixa | renderer.h, renderer.cpp | T7 |

### Ordem recomendada de execução:

```
T1 (Gold) + T2 (HashTable)        ← independentes, podem ser feitos em paralelo
      ↓
T3 (Catálogo SQLite)
      ↓
T8 (DB tabela shop_items)
      ↓
T4 (Compra e aplicação)
      ↓
T5 (Fase de Loja server-side)
      ↓
T7 (Protocolo extendido)
      ↓
T6 (UI da Loja client-side)
      ↓
T9 (Bot Shop AI)
      ↓
T10 (HUD gold + itens)
```

---

## Arquivos Novos Previstos

| Arquivo | Propósito |
|---------|-----------|
| `include/hash_table.h` | Tabela hash genérica (EDOO) |
| `include/shop_defs.h` | Definição dos itens da loja e constantes |

## Arquivos Modificados Previstos

| Arquivo | Mudanças |
|---------|----------|
| `include/protocol.h` | `PHASE_SHOP`, `INPUT_SHOP_BUY`, `ShopPacket`, extensões no `GameSnapshot`, `BUFF_AD_PCT` |
| `include/trainer.h` | Campo `gold_`, `winStreak_`, métodos |
| `src/trainer.cpp` | Implementação de gold |
| `include/hero.h` | `PermanentItem`, `activableItems_`, métodos de item |
| `src/heroes/hero.cpp` | `addPermanentItem()`, modificação de `resetStats()`, `recalcStats()` para itens |
| `include/game.h` | `handleShopBuy()`, `shopCatalog_`, `updateBotShop()`, `applyActivableItems()` |
| `src/server/game.cpp` | Transição de fases, lógica de compra, bot shop, ativação de itens |
| `include/database.h` | Métodos para shop_items |
| `src/database.cpp` | Tabela shop_items, seed, getAllShopItems |
| `src/client/main.cpp` | Novo `PHASE_SHOP` input/render, `ShopInput` state |
| `src/client/renderer.h` | `drawShopScreen()`, `drawGoldDisplay()`, `drawHeroItems()` |
| `src/client/renderer.cpp` | Implementação da UI da loja, HUD de gold |
| `CMakeLists.txt` | Nenhuma mudança (headers são include-only; hash_table é header-only) |

---

## Notas de Implementação

1. **Première Shop Phase**: O primeiro round NÃO tem shop (após VS_INTRO → POSITIONING direto). A shop phase acontece apenas DEPOIS de ROUND_END, a partir do round 2.
2. **Itens passivos e resetStats()**: O `resetStats()` já restaura dos stats base + custom. Itens passivos precisam ser aplicados *depois* de `resetStats()`, via `addPermanentItem()`.
3. **Itens ativáveis são de uso único por round**: Após ativação em `startBattle()`, são removidos. O jogador pode comprar novamente na próxima shop phase.
4. **Gold persiste entre rounds** mas reseta entre partidas (em `resetScore()` ou novo jogo).
5. **Limite de itens**: Máximo de 4 itens por herói (para simplificar protocolo e UI).
6. **Seleção de herói na loja**: O jogador seleciona qual dos 3 heróis receberá o item. Itens não são transferíveis após compra.