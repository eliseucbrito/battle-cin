# Plano de Implementação: Expansão Tática e Visual

**Branch alvo**: a criar (`feat/tactical-expansion`)
**Base**: `feat/visual-feedback-and-gameplay` (branch atual)

---

## Visão Geral — Fases e Dependências

```
Fase 1 (Visual Juice)          ↪ Sem dependência, client-only
Fase 2 (Estatísticas de Round)  ↪ Sem dependência, protocolo mínimo
Fase 3 (IA com Behavior Tree)  ↪ Sem dependência, server-only
Fase 4 (Sinergias de Classe)   ↪ Depende de Fase 2 (mostra sinergias no stats HUD)
Fase 5 (Draft e Banimento)     ↪ Depende de Fase 4 (sinergias influenciam banimentos)
Fase 6 (Economia e Loja)      ↪ Depende de Fase 5 (draft precede loja no fluxo)
```

---

## Fase 1 — Animação Procedural e Juice Visual

**Sem mudanças de protocolo.** Tudo é client-side, usando dados já existentes no `GameSnapshot`.

### 1.1 — Pedestais de Time (Bases 3D Falsas)

**Objetivo**: Desenhar um cilindro achatado colorido abaixo de cada herói, como miniaturas de tabuleiro.

**Detalhes**:
- Cor do pedestal: azul (`{60, 120, 230, 180}`) para time 0, vermelho (`{230, 60, 60, 180}`) para time 1
- Format: elipse horizontal de `CELLW * 0.55` x `CELLH * 0.55`, com borda de 2px mais escura
- Desenhar ANTES do sprite do herói (z-order inferior)

**Arquivos**:
- `src/client/renderer.cpp` — adicionar `drawPedestal(pos, ownerId)` chamado antes de `drawHero()`
- `src/client/renderer.h` — declarar `drawPedestal()`

### 1.2 — Idle Breathing (Respiração)

**Objetivo**: Escalar o sprite do herói suavemente no eixo Y entre `1.0` e `1.05` usando `sin(time)`.

**Detalhes**:
- Período: ~2 segundos (`2 * PI / 2.0`)
- Scale Y: `1.0 + 0.05 * sinf(GetTime() * PI)` (varia de 0.95 a 1.05)
- Não aplicar se o herói estiver morto ou sendo arrastado
- Integrar no `drawHero()` existente, modificando o `DrawTexturePro()` scale

**Arquivos**:
- `src/client/renderer.cpp` — modificar `drawHero()` para aceitar `dt` e calcular `scaleY`

### 1.3 — Inclinação de Movimento (Move Tilt)

**Objetivo**: Rotacionar levemente o sprite na direção do movimento (-5° a +5°).

**Detalhes**:
- Detectar direção do movimento comparando posição interpolada atual com a anterior
- Tilt no eixo Z: `angle = atan2(deltaY, deltaX) * 0.1f` (suavizado)
- Zerar tilt quando parado
- Adicionar `Vector2 prevPos` ao `HeroVis` no `main.cpp`

**Arquivos**:
- `src/client/main.cpp` — adicionar `prevPos` ao `HeroVis`, calcular `delta` antes de interpolar
- `src/client/renderer.cpp` — modificar `drawHero()` para aceitar parâmetro de `tiltAngle`

### 1.4 — Animação de Ataque Melee (Estilingue)

**Objetivo**: Heróis melee (Tank, Fighter, Assassin) avançam rapidamente na direção do alvo e retornam.

**Detalhes**:
- Detectar no cliente quando `hero.hp` de um HERÓI INIMIGO diminui e nosso herói está adjacente
- Animação: herói se move 30% da distância ao alvo em 0.08s, depois volta em 0.08s
- Implementar como estado visual: `AttackAnim { int attackerIdx; Vector2 startPos; Vector2 targetPos; float timer; }`
- Manter `vector<AttackAnim>` no `main.cpp`
- Trigger: quando `snap.heroes[i].hp` diminui e `heroVis[i]` tem `ownerId != myId`, procurar o adjacente dono

**Arquivos**:
- `src/client/renderer.h` — adicionar `struct AttackAnim`, `spawnAttackAnim()`, `drawAttackAnims()`
- `src/client/renderer.cpp` — implementar animação de estilingue
- `src/client/main.cpp` — detectar ataques e spawnar animações

### 1.5 — Animação de Ataque Ranged (Projétil)

**Objetivo**: Heróis ranged (Mage, Support) disparam um projétil visual (linha/círculo) que viaja até o alvo.

**Detalhes**:
- Mesmo trigger que melee, mas verificar `archetype == MAGE || archetype == SUPPORT`
- `Projectile { Vector2 from, to; float timer; float maxTimer; Color color; }`
- Mage: projétil roxo/azul,直线 com trail fade-out
- Support: projétil verde, círculos pequenos que viajam
- Velocidade: 0.3s do atacante ao alvo

**Arquivos**:
- `src/client/renderer.h` — adicionar `struct Projectile`, `spawnProjectile()`, `drawProjectiles()`
- `src/client/renderer.cpp` — implementar projéteis
- `src/client/main.cpp` — detectar ataques ranged e spawnar projéteis

### 1.6 — Hit Flash

**Objetivo**: Círculo branco intenso por 2 frames quando um herói recebe dano.

**Detalhes**:
- Integrar com o sistema de `FloatingText` já existente
- Quando `spawnFloatingText()` é chamado com cor RED, também spawnar um hit flash
- `HitFlash { Vector2 pos; float timer; }` com `timer = 0.066f` (2 frames a 30fps lógico)
- Desenhar `DrawCircleV(pos, radius * 1.2f, {255, 255, 255, (unsigned char)(255 * timer / maxTimer)})`

**Arquivos**:
- `src/client/renderer.h` — adicionar `struct HitFlash`, `spawnHitFlash()`, `drawHitFlashes()`
- `src/client/renderer.cpp` — implementar hit flash renderização e update

### Ordem de Implementação — Fase 1

| # | Tarefa | Complexidade | Arquivos Afetados |
|---|--------|-------------|-------------------|
| 1.1 | Pedestais de time | Baixa | renderer.h, renderer.cpp |
| 1.2 | Idle breathing | Baixa | renderer.cpp, renderer.h |
| 1.3 | Move tilt | Média | main.cpp, renderer.cpp |
| 1.4 | Melee attack anim | Alta | renderer.h, renderer.cpp, main.cpp |
| 1.5 | Ranged projectile | Alta | renderer.h, renderer.cpp, main.cpp |
| 1.6 | Hit flash | Baixa | renderer.h, renderer.cpp |

---

## Fase 2 — Estatísticas de Round

**Mudança de protocolo: adição de campos ao `GameSnapshot`.**

### 2.1 — Coleta de Estatísticas no Servidor

**Objetivo**: Rastrear dano causado, dano recebido e cura realizada por herói em cada round.

**Detalhes**:
- Adicionar `RoundStats` ao `Game`:
  ```cpp
  struct RoundStats {
      int damageDealt[MAX_HEROES_TOTAL];   // dano causado
      int damageReceived[MAX_HEROES_TOTAL]; // dano recebido
      int healingDone[MAX_HEROES_TOTAL];    // cura realizada
  };
  ```
- Em `Game::runCombat()`, após `hero.attackTarget(*target)`:
  - Incrementar `damageDealt[attackerIdx]` e `damageReceived[targetIdx]`
- Em `Hero::healHp()` (chamado por Support ult, Battle Heal, etc.):
  - Incrementar `healingDone[healerIdx]`
- Resetar stats ao início de cada round (`startPositioning()`)

**Arquivos**:
- `include/game.h` — adicionar `RoundStats` struct e `roundStats_` membro
- `src/server/game.cpp` — instrumentation de `runCombat()`, `tickUltimates()`, `startPositioning()`

### 2.2 — Transmissão via Protocolo

**Objetivo**: Enviar as estatísticas no `GameSnapshot` para que o cliente possa exibi-las.

**Detalhes**:
- Adicionar ao `GameSnapshot`:
  ```cpp
  struct HeroRoundStats {
      uint16_t damageDealt;
      uint16_t damageReceived;
      uint16_t healingDone;
  };
  HeroRoundStats heroStats[MAX_HEROES_TOTAL];
  ```
- Em `Game::buildSnapshot()`, preencher `heroStats[]` a partir de `roundStats_`
- Incrementar `sizeof(GameSnapshot)` — ambos client e server precisam ser recompilados

**Arquivos**:
- `include/protocol.h` — adicionar `HeroRoundStats` e campo no `GameSnapshot`
- `src/server/game.cpp` — preencher stats em `buildSnapshot()`
- `src/client/main.cpp` — ler stats do snapshot

### 2.3 — Visualização de Estatísticas (HUD pós-round)

**Objetivo**: Ao final de cada round (PHASE_ROUND_END), exibir um overlay com barras horizontais mostrando stats por herói.

**Detalhes**:
- Posição: centro da tela, overlay semi-transparente escuro
- Layout: 6 linhas (uma por herói), 3 barras por linha (damage dealt / damage received / healing)
- Barras: vermelho para dano, laranja para dano recebido, verde para cura
- Labels: nome do herói à esquerda, números à direita
- Máximo da barra = max de todos os valores (escala relativa)
- Duração: enquanto `phase == PHASE_ROUND_END` (3 segundos)

**Arquivos**:
- `src/client/renderer.h` — declarar `drawRoundStatsOverlay()`
- `src/client/renderer.cpp` — implementar overlay com `DrawRectangle`, `DrawText`
- `src/client/main.cpp` — chamar overlay durante `PHASE_ROUND_END`

### Ordem de Implementação — Fase 2

| # | Tarefa | Complexidade | Arquivos Afetados |
|---|--------|-------------|-------------------|
| 2.1 | Coleta de stats no servidor | Média | game.h, game.cpp |
| 2.2 | Transmissão via protocolo | Baixa | protocol.h, game.cpp, main.cpp |
| 2.3 | Visualização pós-round | Média | renderer.h, renderer.cpp, main.cpp |

---

## Fase 3 — IA com Behavior Tree

**Server-side only.** Sem mudanças de protocolo.

### 3.1 — Estrutura de Behavior Tree

**Objetivo**: Substituir a lógica simples do bot por uma árvore de comportamento.

**Detalhes**:
- Criar `include/behavior_tree.h` com as classes:
  ```cpp
  class BTNode {
  public:
      virtual ~BTNode() = default;
      enum Status { SUCCESS, FAILURE, RUNNING };
      virtual Status tick(float dt) = 0;
  };

  class BTSelector : public BTNode {  // OR: executa filhos até um ter SUCCESS
      std::vector<std::unique_ptr<BTNode>> children_;
  public:
      Status tick(float dt) override;
  };

  class BTSequence : public BTNode {  // AND: executa filhos até um ter FAILURE
      std::vector<std::unique_ptr<BTNode>> children_;
  public:
      Status tick(float dt) override;
  };

  class BTCondition : public BTNode {  // Folha: verifica condição
      std::function<bool()> condition_;
  public:
      Status tick(float dt) override;
  };

  class BTAction : public BTNode {  // Folha: executa ação
      std::function<Status(float)> action_;
  public:
      Status tick(float dt) override;
  };
  ```
- Esta é uma estrutura de dados exigida pela disciplina EDOO (Árvore)

**Arquivos**:
- `include/behavior_tree.h` — novo arquivo
- `src/behavior_tree.cpp` — novo arquivo (implementação dos nós)

### 3.2 — Construir a BT do Bot

**Objetivo**: Montar uma BT que dá comportamento tático ao bot.

**Árvore de comportamento**:
```
[Selector] Root
├── [Sequence] Use Ability
│   ├── [Condition] Ability available?
│   └── [Action] Use ability
├── [Sequence] Focus Low-HP Enemy
│   ├── [Condition] Enemy with HP < 30% exists?
│   ├── [Condition] My hero adjacent to that enemy?
│   └── [Action] Set target focus on low-HP enemy
├── [Sequence] Target Enemy Support
│   ├── [Condition] Enemy has Support alive?
│   ├── [Condition] My hero adjacent to that Support?
│   └── [Action] Set target focus on Support
├── [Sequence] Move to Buff Zone
│   ├── [Condition] Buff zone exists on grid?
│   ├── [Condition] My hero not on a buff zone?
│   └── [Action] Position hero on buff zone (during positioning phase)
└── [Action] Default behavior (current logic)
```

**Arquivos**:
- `src/server/game.cpp` — `updateBot()` refatorado para usar BT
- Construir a árvore em `Game::createBot()` ou em função dedicada

### 3.3 — Chat Narrativo do Bot

**Objetivo**: Exibir no log do servidor (e opcionalmente no cliente) mensagens explicando as decisões do bot.

**Detalhes**:
- Adicionar ao `Game` um buffer de mensagens do bot: `std::string botMessages_[2];`
- Quando o bot toma uma decisão na BT, registrar uma mensagem:
  - `"🎯 [Bot]: Focando no Mago para cortar o dano!"`
  - `"💉 [Bot]: Meu time precisa de vida! Habilidade ativada!"`
  - `"🏃 [Bot]: Movendo para a Zona de Buff de AD!"`
- Adicionar ao `GameSnapshot`:
  ```cpp
  char botMessage[64];  // última mensagem do bot
  ```
- No cliente, renderizar como chat box temporário no canto inferior

**Arquivos**:
- `include/game.h` — adicionar buffer de mensagens
- `src/server/game.cpp` — registrar mensagens na BT
- `include/protocol.h` — adicionar campo no `GameSnapshot`
- `src/client/renderer.h/cpp` — `drawBotChat()`
- `src/client/main.cpp` — exibir mensagens do bot

### Ordem de Implementação — Fase 3

| # | Tarefa | Complexidade | Arquivos Afetados |
|---|--------|-------------|-------------------|
| 3.1 | Behavior Tree estrutura | Média | include/behavior_tree.h, src/behavior_tree.cpp |
| 3.2 | BT do Bot | Alta | game.cpp, game.h |
| 3.3 | Chat narrativo | Baixa | game.h, game.cpp, protocol.h, renderer, main.cpp |

---

## Fase 4 — Sinergias de Classe

**Depende de Fase 2** (usa stats overlay para mostrar sinergias ativas).

### 4.1 — Definição das Sinergias

**Objetivo**: Bonificar composições com 2+ heróis do mesmo arquétipo.

**Regras**:
| Condição | Bônus | Nome |
|----------|-------|------|
| 2+ Tanks | +20% Max HP para Tanks | Fortaleza |
| 2+ Fighters | +15% AD para Fighters | Duelo |
| 2+ Mages | +10 AD + penetrar 50% arm para Mages | Arcano |
| 2+ Assassins | +15% chance de crítico (2x dano) | Emboscada |
| 2+ Supports | Cura passiva de 1% HP/s para Supports | Esperança |

**Detalhes**:
- Verificar composição na inicialização do round (`startBattle()`)
- Aplicar sinergias usando o sistema existente `LinkedList<ActiveEffect>`
- Sinergias duram o round inteiro (duração = `BATTLE_MAX_TIME`)

### 4.2 — Implementação da Verificação

**Objetivo**: Ao iniciar a batalha, contar arquétipos e aplicar bônus.

**Detalhes**:
- Adicionar `applySynergies()` ao `Game`:
  ```cpp
  void Game::applySynergies() {
      for (int i = 0; i < 2; i++) {
          int archetypeCount[5] = {0};
          for (int h = 0; h < trainers_[i].heroCount(); h++)
              archetypeCount[trainers_[i].heroAt(h).archetype()]++;

          for (int h = 0; h < trainers_[i].heroCount(); h++) {
              Hero& hero = trainers_[i].heroAt(h);
              uint8_t arch = hero.archetype();
              if (archetypeCount[arch] >= 2) {
                  switch (arch) {
                      case ARCHETYPE_TANK:
                          hero.applyBuff(BUFF_SYNERGY_HP, BATTLE_MAX_TIME);
                          break;
                      // ... etc
                  }
              }
          }
      }
  }
  ```
- Adicionar novos tipos de buff: `BUFF_SYNERGY_HP = 4`, `BUFF_SYNERGY_AD = 5`, etc. no `protocol.h`
- Estender `Hero::applyBuff()` e `recalcStats()` para lidar com novos tipos

**Arquivos**:
- `include/protocol.h` — novos constantes BUFF_SYNERGY_*
- `include/game.h` — declarar `applySynergies()`
- `src/server/game.cpp` — implementar `applySynergies()`, chamar em `startBattle()`
- `src/heroes/hero.cpp` — tratar novos buff types em `recalcStats()`

### 4.3 — Indicador Visual de Sinergia

**Objetivo**: Exibir no HUD quais sinergias estão ativas para cada time.

**Detalhes**:
- Adicionar ao `GameSnapshot`:
  ```cpp
  uint8_t synergies[2][5];  // [team][archetype] = count
  ```
- No cliente, renderizar ícones pequenos (ou texto) ao lado do nome do treinador
- Exemplo: "Fortaleza (+20% HP Tank)" com ícone brilhante dourado

**Arquivos**:
- `include/protocol.h` — adicionar campo de sinergias ao `GameSnapshot`
- `src/server/game.cpp` — preencher sinergias em `buildSnapshot()`
- `src/client/renderer.cpp` — `drawSynergyIndicators()`
- `src/client/main.cpp` — renderizar durante HUD

### 4.4 — Chance de Crítico (Sinergia Assassin)

**Objetivo**: Implementar mecânica de crítico para a sinergia "Emboscada".

**Detalhes**:
- Adicionar `critChance_` ao `Hero`: chance de crítico (0 por padrão, 0.15 com sinergia)
- Em `Hero::attackTarget()`, se `(float)rand() / RAND_MAX < critChance_`, multiplicar dano por 2
- Adicionar `BUFF_SYNERGY_CRIT` como tipo de buff que concede `critChance_ = 0.15`
- Feedback visual: números de dano amarelos para críticos (já existe sistema de floating text)

**Arquivos**:
- `include/hero.h` — adicionar `critChance_`, `float critChance() const`
- `src/heroes/hero.cpp` — modificar `attackTarget()` e `recalcStats()`
- `include/protocol.h` — `BUFF_SYNERGY_CRIT = 7`

### Ordem de Implementação — Fase 4

| # | Tarefa | Complexidade | Arquivos Afetados |
|---|--------|-------------|-------------------|
| 4.1 | Definição das sinergias | Baixa | documentação |
| 4.2 | Verificação e aplicação | Média | protocol.h, game.h, game.cpp, hero.cpp |
| 4.3 | Indicador visual | Baixa | protocol.h, game.cpp, renderer.cpp |
| 4.4 | Chance de crítico | Média | hero.h, hero.cpp, protocol.h |

---

## Fase 5 — Draft e Banimento (Ban-Pick Alternado)

**Depende de Fase 4** (sinergias influenciam decisões de banimento). Mudança maior de protocolo e UI.

### 5.1 — Novo Fluxo de Rede para Draft

**Objetivo**: Substituir a seleção simultânea por um draft alternado com bans.

**Fluxo de Draft**:
```
PHASE_BAN_PICK (novo):
  1. P1 bane 1 herói (dos 10 disponíveis)
  2. P2 bane 1 herói
  3. P1 escolhe trainer + 1º herói
  4. P2 escolhe trainer + 1º herói
  5. P1 escolhe 2º herói
  6. P2 escolhe 2º herói
  7. P1 escolhe 3º herói
  8. P2 escolhe 3º herói
```

**Detalhes de protocolo**:
- Adicionar `PHASE_BAN_PICK = 6` ao `protocol.h`
- Novo pacote: `DraftPacket` (tamanho variável ou fixo):
  ```cpp
  #pragma pack(push, 1)
  struct DraftPacket {
      uint8_t playerId;    // 0 ou 1
      uint8_t type;        // INPUT_BAN ou INPUT_DRAFT
      uint8_t index;       // índice banido ou índice do hero/trainer escolhido
      uint8_t slot;        // 0-2 para hero slot (DRAFT only)
  };
  #pragma pack(pop)
  ```
- Adicionar ao `GameSnapshot`:
  ```cpp
  uint8_t draftPhase;      // 0-7: qual passo do draft estamos
  uint8_t bannedHeroes[10]; // -1 se não banido, índice se banido
  uint8_t draftTurn;        // 0 ou 1: de quem é a vez
  ```

### 5.2 — Lógica do Draft no Servidor

**Objetivo**: Gerenciar o fluxo de ban-pick no servidor.

**Detalhes**:
- Adicionar ao `Game`:
  ```cpp
  uint8_t draftPhase_ = 0;       // 0-7
  int8_t  bannedHeroes_[10];      // -1 = disponível, índice = banido
  bool    draftReady_[2] = {false, false};
  ```
- `handleBan(int pid, int heroIndex)`: registra banimento, avança `draftPhase_`
- `handleDraft(int pid, int heroIndex, int slot)`: registra escolha, avança `draftPhase_`
- Quando `draftPhase_` atinge 8 (todos escolheram), transicionar para `PHASE_VS_INTRO`

**Arquivos**:
- `include/game.h` — campos de draft e métodos `handleBan()`, `handleDraft()`
- `src/server/game.cpp` — implementar lógica de draft
- `src/server/main.cpp` — deserializar `DraftPacket` e chamar handlers

### 5.3 — UI de Draft no Cliente

**Objetivo**: Renderizar tela de banimento e seleção alternada.

**Detalhes**:
- Novo `ClientPhase::DRAFTING` no cliente
- Tela dividida: grid de heróis disponíveis à esquerda, composição do time à direita
- Heróis banidos aparecem escurecidos e não selecionáveis
- Indicador visual de de quem é a vez: "Sua vez!" ou "Aguardando oponente..."
- Cursor navega com WASD, confirma com Enter/Espaço

**Arquivos**:
- `src/client/main.cpp` — novo `ClientPhase::DRAFTING`, input handling
- `src/client/renderer.h/cpp` — `drawDraftScreen()`, `drawBanHighlight()`
- `include/protocol.h` — constantes e packet

### Ordem de Implementação — Fase 5

| # | Tarefa | Complexidade | Arquivos Afetados |
|---|--------|-------------|-------------------|
| 5.1 | Protocolo de draft | Média | protocol.h |
| 5.2 | Lógica server-side | Alta | game.h, game.cpp, main.cpp (server) |
| 5.3 | UI de draft no cliente | Alta | main.cpp (client), renderer.h/cpp |
| 5.4 | Integração e testes | Média | — |

---

## Fase 6 — Economia e Loja entre Rounds

**Depende de Fase 5** (draft precede loja no fluxo). Mudança de protocolo significativa.

### 6.1 — Sistema de Gold

**Objetivo**: Introduzir moeda de ouro para progressão entre rounds.

**Detalhes**:
- Adicionar ao `Trainer`:
  ```cpp
  int gold_ = 0;
  int gold() const { return gold_; }
  void addGold(int amount) { gold_ += amount; }
  void spendGold(int amount) { gold_ -= amount; }
  ```
- Recompensas de gold por round:
  - Vitória: 100 gold
  - Derrota: 60 gold
  - Empate: 80 gold
  - Bonus por vitória consecutiva: +20 por streak (máx +60)
- Adicionar ao `GameSnapshot`: `uint16_t gold[2];`

**Arquivos**:
- `include/trainer.h` — campo `gold_`
- `src/server/game.cpp` — adicionar gold em `endRound()`, resetar em `startPositioning()` (apenas gold inicial)
- `include/protocol.h` — campo `gold` no `GameSnapshot`

### 6.2 — Fase de Loja (Shop Phase)

**Objetivo**: Nova fase entre `PHASE_ROUND_END` e `PHASE_POSITIONING`.

**Fluxo atualizado**:
```
BATTLE → ROUND_END → SHOP → POSITIONING → BATTLE → ...
```

**Detalhes**:
- Adicionar `PHASE_SHOP = 6` (renumerar `PHASE_BAN_PICK` se necessário, ou usar 7)
- Duração: 15 segundos
- Durante esta fase, jogadores podem comprar itens/upgrades

### 6.3 — Catálogo de Itens (Hash Table)

**Objetivo**: Definir itens disponíveis na loja usando uma Tabela Hash (estrutura de dados EDOO).

**Itens**:

| Nome | Tipo | Custo | Efeito |
|------|------|-------|--------|
| Espada Curta | Passivo | 80 | +8 AD permanente ao herói |
| Escudo de Ferro | Passivo | 80 | +5 ARM permanente ao herói |
| Vitalidade | Passivo | 100 | +40 Max HP permanente ao herói |
| Botas Ágeis | Passivo | 120 | +20% Attack Speed permanente |
| Poção de Cura | Ativável | 50 | Restaura 25% HP de um herói (1 uso) |
| Poção de Fúria | Ativável | 60 | +50% AD por 8s no herói (1 uso, início do round) |
| Elixir de Escudo | Ativável | 70 | +15 ARM por 8s no herói (1 uso, início do round) |

**Detalhes**:
- Criar `include/hash_table.h` — estrutura de dados EDOO (tabela hash com encadeamento)
  ```cpp
  template<typename K, typename V>
  class HashTable {
      struct Entry { K key; V value; Entry* next; };
      // insert(), find(), remove(), forEach()
  };

  struct ShopItem {
      uint8_t id;
      const char* name;
      uint8_t cost;
      uint8_t type;    // ITEM_PASSIVE, ITEM_ACTIVABLE
      uint8_t effect;  // STAT_AD, STAT_ARM, STAT_HP, STAT_AS, EFFECT_HEAL, etc.
      uint8_t magnitude;
  };
  ```
- `HashTable<uint8_t, ShopItem>` para lookup rápido por ID

**Arquivos**:
- `include/hash_table.h` — novo arquivo
- `include/shop_defs.h` — definição dos itens e catálogo global

### 6.4 — Compra e Aplicação de Itens

**Objetivo**: Permitir que jogadores comprem itens durante a Shop Phase.

**Protocolo**:
```cpp
struct ShopPacket {
    uint8_t playerId;
    uint8_t type;        // INPUT_SHOP_BUY = 5
    uint8_t heroIndex;   // qual herói recebe o item (0-2)
    uint8_t itemId;      // ID do item na loja
};
```

**Lógica no servidor**:
- `handleShopBuy(pid, heroIndex, itemId)`: verifica gold suficiente, deduz gold, aplica efeito
- Itens passivos: aplicam `setCustomStats()` ou `applyBuff()` permanente
- Itens ativáveis: armazenados no herói, ativados automaticamente no início do próximo round

**Arquivos**:
- `include/protocol.h` — `INPUT_SHOP_BUY`, `ShopPacket`
- `include/hero.h` — campo `items_` (lista de itens ativáveis comprados)
- `include/game.h` — `handleShopBuy()`, `PHASE_SHOP`
- `src/server/game.cpp` — lógica de compra, item catalog, aplicar efeitos

### 6.5 — UI da Loja no Cliente

**Objetivo**: Renderizar a loja durante a Shop Phase.

**Detalhes**:
- Grid de itens disponíveis com nome, custo e descrição
- Gold atual do jogador no canto
- Heróis do time à direita para selecionar quem recebe o item
- Itens já comprados ficam escurecidos
- Input: WASD para navegar, Enter para comprar, Q para herói anterior, E para próximo

**Arquivos**:
- `src/client/main.cpp` — novo `ClientPhase::SHOPPING`, input, enviar `ShopPacket`
- `src/client/renderer.h/cpp` — `drawShopScreen()`

### Ordem de Implementação — Fase 6

| # | Tarefa | Complexidade | Arquivos Afetados |
|---|--------|-------------|-------------------|
| 6.1 | Sistema de gold | Baixa | trainer.h, game.cpp, protocol.h |
| 6.2 | Fase de loja | Média | game.h, game.cpp, protocol.h |
| 6.3 | Hash Table + Catálogo | Alta | hash_table.h, shop_defs.h, CMakeLists.txt |
| 6.4 | Compra e aplicação | Alta | protocol.h, hero.h, game.h, game.cpp |
| 6.5 | UI da loja | Alta | main.cpp (client), renderer.h/cpp |

---

## Estruturas de Dados EDOO — Resumo

| # | Estrutura | Arquivo | Fase | Uso |
|---|-----------|---------|------|-----|
| 1 | `LinkedList<T>` | include/linked_list.h | ✅ Existente | Efeitos ativos, adjacência do grafo |
| 2 | `Graph + BFS` | include/graph.h | ✅ Existente | Pathfinding 8x8 |
| 3 | `PriorityQueue` | include/priority_queue.h | ✅ Existente | Ordem de combate |
| 4 | `TournamentTree` | include/tournament_tree.h | ✅ Existente | Histórico de rounds |
| 5 | `BehaviorTree` | include/behavior_tree.h | **Fase 3** | IA do bot |
| 6 | `HashTable<K,V>` | include/hash_table.h | **Fase 6** | Catálogo de itens da loja |

---

## Arquivos Novos Previstos

| Arquivo | Fase | Propósito |
|---------|------|-----------|
| `include/behavior_tree.h` | 3 | Árvore de comportamento para IA |
| `src/behavior_tree.cpp` | 3 | Implementação dos nós BT |
| `include/hash_table.h` | 6 | Tabela hash genérica |
| `include/shop_defs.h` | 6 | Definição dos itens da loja |

## Arquivos Modificados Previstos (por Fase)

| Fase | Arquivos |
|------|----------|
| 1 | renderer.h, renderer.cpp, main.cpp (client) |
| 2 | game.h, game.cpp, protocol.h, renderer.h, renderer.cpp, main.cpp |
| 3 | game.h, game.cpp, protocol.h, renderer.h, renderer.cpp, main.cpp, CMakeLists.txt |
| 4 | protocol.h, game.h, game.cpp, hero.h, hero.cpp, renderer.h, renderer.cpp, main.cpp |
| 5 | protocol.h, game.h, game.cpp, main.cpp (server), main.cpp (client), renderer.h/cpp |
| 6 | protocol.h, game.h, game.cpp, hero.h, trainer.h, main.cpp (client), renderer.h/cpp, CMakeLists.txt |

---

## Ordem de Execução Recomendada

```
Fase 1 (Visual)        ← Sem blockers, client-only, pode ser testado visualmente
Fase 2 (Stats)          ← Sem blockers, protocolo mínimo
Fase 3 (IA/BT)          ← Sem blockers, server-only
Fase 4 (Sinergias)      ← Depende de Fase 2 (mostra no stats overlay)
Fase 5 (Draft/Ban)      ← Depende de Fase 4 (sinergias influenciam bans)
Fase 6 (Loja/Economia)  ← Depende de Fase 5 (draft precede loja)
```

Cada fase pode ser implementada e commitada independentemente (exceto pelos dependências indicadas). Entre fases, é recomendado:
1. Fazer build e testar (`make run-2players` ou `make run-solo`)
2. Commitar com mensagem descritiva
3. Atualizar este plano com checkmarks