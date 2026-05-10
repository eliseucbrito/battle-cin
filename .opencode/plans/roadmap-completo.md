# Roadmap: Completar o Jogo + Estruturas de Dados

## Ordem de Implementação

As tarefas estão ordenadas por prioridade: primeiro finalizamos o jogo funcional,
depois adicionamos as estruturas de dados exigidas pela disciplina EDOO.

---

## Fase 1 — Completar o Jogo (funcionalidade crítica)

### 1.1 — `resolveTimeLimit()` (server/game.cpp)

**Problema**: Função vazia. Quando o timer de batalha (120s) esgota, nada acontece.

**Solução**: Comparar porcentagem total de HP restante de cada time. Quem tiver mais ganha o round. Empate raro possível.

```cpp
void Game::resolveTimeLimit() {
    float hpPct[2] = { 0.f, 0.f };
    for (int i = 0; i < 2; i++) {
        int totalHp = 0, totalMaxHp = 0;
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            const Hero& hero = trainers_[i].heroAt(h);
            totalHp    += hero.hp();
            totalMaxHp += hero.maxHp();
        }
        hpPct[i] = (totalMaxHp > 0) ? (float)totalHp / totalMaxHp : 0.f;
    }
    if (hpPct[0] > hpPct[1])      endRound(0);
    else if (hpPct[1] > hpPct[0]) endRound(1);
    else                           endRound(0xFF);  // empate
}
```

---

### 1.2 — `generateBuffZones()` (server/game.cpp)

**Problema**: Função vazia. Nenhuma zona de buff aparece no grid.

**Solução**: Gerar 2-3 zonas aleatórias nas colunas centrais (2-5) do grid, cada uma com tipo BUFF_AD, BUFF_HP ou BUFF_ARM.

```cpp
void Game::generateBuffZones() {
    buffZoneCount_ = 0;
    int desired = 2 + (rand() % 2);  // 2 ou 3 zonas
    for (int attempt = 0; attempt < 20 && buffZoneCount_ < desired; attempt++) {
        uint8_t bx = (uint8_t)(2 + rand() % 4);   // cols 2-5
        uint8_t by = (uint8_t)(rand() % GRID_ROWS);
        // evitar duplicatas
        bool dup = false;
        for (int i = 0; i < buffZoneCount_; i++) {
            if (buffZones_[i].x == bx && buffZones_[i].y == by) { dup = true; break; }
        }
        if (dup) continue;
        uint8_t type = (uint8_t)(1 + rand() % 3);  // BUFF_AD, BUFF_HP ou BUFF_ARM
        buffZones_[buffZoneCount_++] = { bx, by, type };
    }
}
```

---

### 1.3 — Feedback Visual: Barras de HP e Números de Dano Flutuantes

**Problema**: Não há indicação visual de HP dos heróis nem números de dano.

**Plano**:

#### `drawHero()` — adicionar barra de HP
- Desenhar uma pequena barra acima de cada herói: `hp/maxHp`
- Verde se > 50%, amarelo se > 25%, vermelho se <= 25%
- Posição: acima do sprite do herói

#### `FloatingText` — sistema de números de dano flutuantes
- Criar struct simples: `{ int x, y; int value; float timer; Color color; }`
- No client: manter `vector<FloatingText>` local
- Quando detectar que `hero.hp()` diminuiu entre frames, criar um FloatingText
- Renderizar: texto que sobe e fade out ao longo de ~1 segundo
- Dano normal = vermelho, cura = verde, crítico/ult = amarelo

**Arquivos afetados**:
- `src/client/renderer.h` — adicionar `struct FloatingText`, array, `spawnDamageText()`
- `src/client/renderer.cpp` — `drawHero()` com barra, `drawFloatingTexts()`
- `src/client/main.cpp` — detectar mudanças de HP e spawnar textos

---

### 1.4 — Feedback Visual: Efeitos de Morte e Ultimate

**Problema**: Heróis morrem sem feedback visual. Ultimates ativam sem indicação.

**Plano**:

#### Morte
- Quando `hero.alive` muda de true→false: expandir sprite (scale pulse) por 0.3s, depois desaparecer
- Adicionar estado visual `dyingTimer` no client para cada herói

#### Ultimate
- Quando `hero.ultActive` muda de false→true: flash branco no sprite por 0.2s
- Mostrar nome da ult como texto flutuante ("Iron Fortress!", "Arcane Nova!")

**Arquivos afetados**:
- `src/client/renderer.h` — adicionar arrays de estado visual
- `src/client/renderer.cpp` — efeitos de renderização
- `src/client/main.cpp` — detectar transições de estado

---

### 1.5 — Direcionamento de Ataque (Target Focus)

**Problema**: Durante a batalha, heróis atacam o primeiro inimigo adjacente encontrado (`runCombat()` itera linearmente e faz `break` no primeiro). O jogador não tem controle sobre qual inimigo seu herói ataca.

**Funcionamento desejado**: Durante `PHASE_BATTLE`, o jogador pode arrastar uma seta de um herói seu para um inimigo adjacente a esse herói. O herói então para de atacar quem quer que estivesse atacando e foca no novo alvo. O foco persiste até o alvo morrer, sair da adjacência, ou o jogador redirecionar.

#### Mecânica

1. **Cliente detecta drag de targeting**:
   - Durante `PHASE_BATTLE`, se o jogador clica e arrasta a partir de um HERÓI SEU (ownerId == myId):
     - Detecta qual herói foi clicado (similar ao drag de posicionamento)
     - Identifica inimigos adjacentes a esse herói usando `isAdjacentTo()`
     - Mostra destacação visual nos inimigos adjacentes válidos
     - Ao soltar em cima de um inimigo adjacente: envia `INPUT_TARGET` com `{playerId, heroIndex, targetIndex}`
   - O drag é visualmente uma **linha/seta** esticável do herói até o cursor

2. **Protocolo** — novo input type:
   ```cpp
   #define INPUT_TARGET 4   // focus attack target
   
   // Adicionar ao InputPacket (ou criar TargetPacket separado):
   // playerId: 0 ou 1
   // type: INPUT_TARGET
   // heroIndex: índice local do herói (0-2 no time)
   // targetIndex: índice do inimigo no time adversário (0-2)
   // placeX é reutilizado como heroIndex, placeY como targetIndex
   ```
   
   Ou mais clean: criar `TargetPacket` com 4 bytes:
   ```cpp
   #pragma pack(push, 1)
   struct TargetPacket {
       uint8_t playerId;      // 0 ou 1
       uint8_t type;          // INPUT_TARGET = 4
       uint8_t heroIndex;    // qual herói do jogador
       uint8_t targetIndex;  // qual inimigo focar (índice no time adversário)
   };
   #pragma pack(pop)
   ```
   Distinguir do InputPacket pelo tamanho (4 vs 5 bytes).

3. **Servidor — Hero ganha campo `targetFocus_`**:
   ```cpp
   // Em hero.h:
   int targetFocus_ = -1;  // índice no time adversário, -1 = sem foco
   
   void setTargetFocus(int idx) { targetFocus_ = idx; }
   int  targetFocus() const     { return targetFocus_; }
   ```

4. **Servidor — `Game::handleTarget(pid, heroIdx, targetIdx)`**:
   - Valida que `targetIdx` é um inimigo adjacente ao herói em `heroIdx`
   - Se válido, seta `hero.targetFocus_ = targetIdx`
   - Se `-1`, remove foco (herói volta ao comportamento padrão)

5. **Servidor — `Game::runCombat()` modificado**:
   ```cpp
   // Lógica atual (primeiro adjacente):
   for (int eh = 0; eh < trainers_[1-i].heroCount(); eh++) {
       Hero& enemy = trainers_[1-i].heroAt(eh);
       if (enemy.alive() && hero.isAdjacentTo(enemy)) {
           hero.attackTarget(enemy);
           break;
       }
   }
   
   // Nova lógica:
   Hero* target = nullptr;
   if (hero.targetFocus() >= 0) {
       // Tentar atacar o alvo focado
       Hero& focused = trainers_[1-i].heroAt(hero.targetFocus());
       if (focused.alive() && hero.isAdjacentTo(focused))
           target = &focused;
       else
           hero.setTargetFocus(-1);  // alvo morreu ou não está mais adjacente
   }
   if (!target) {
       // Comportamento padrão: primeiro adjacente
       for (int eh = 0; eh < trainers_[1-i].heroCount(); eh++) {
           Hero& enemy = trainers_[1-i].heroAt(eh);
           if (enemy.alive() && hero.isAdjacentTo(enemy)) {
               target = &enemy;
               break;
           }
       }
   }
   if (target) hero.attackTarget(*target);
   ```

6. **Protocolo — GameSnapshot**:
   - Adicionar `int8_t targetFocus` ao `HeroNetState` (índice no time adversário, -1 se sem foco)
   ```cpp
   struct HeroNetState {
       uint8_t  x, y;
       uint16_t hp;
       uint16_t maxHp;
       uint8_t  ad;
       uint8_t  arm;
       uint8_t  archetype;
       uint8_t  heroDefIndex;
       uint8_t  buff;
       uint8_t  alive;
       uint8_t  ultActive;
       uint8_t  ownerId;
       int8_t   targetFocus;   // NOVO: índice no time adversário, -1 = sem foco
   };
   ```

7. **Cliente — Detecção de drag de targeting** (em `main.cpp`):
   - Durante `PHASE_BATTLE`, se mouse click em um herói próprio:
     - Entrar em modo "targeting"
     - Calcular quais inimigos são adjacentes a esse herói
     - Desenhar seta do herói ao cursor enquanto arrasta
     - Se soltar sobre inimigo adjacente: enviar `TargetPacket`
     - Se soltar fora: cancelar

8. **Cliente — Renderização** (em `renderer.cpp`):
   - **Seta de foco**: quando `hero.targetFocus >= 0`, desenhar linha/seta do herói focado ao inimigo alvo
   - **Brilho no alvo**: destacar o inimigo focado com brilho/borda animada
   - **Highlight de targeting**: durante drag, destacar inimigos adjacentes válidos com borda pulsante
   - Adicionar `drawTargetArrow(from, to)` e `drawTargetHighlight(hero)`:
   ```cpp
   void drawTargetArrow(Vector2 from, Vector2 to);      // seta entre herói e alvo
   void drawTargetHighlight(Vector2 pos, float radius);  // brilho pulsante no alvo
   ```

#### Arquivos afetados

| Arquivo | Mudanças |
|---------|----------|
| `include/protocol.h` | `INPUT_TARGET`, `TargetPacket`, `targetFocus` em `HeroNetState` |
| `include/hero.h` | `targetFocus_` field, `setTargetFocus()`, `targetFocus()` |
| `src/server/game.cpp` | `handleTarget()`, `runCombat()` com foco, validação de adjacência |
| `include/game.h` | Declaração de `handleTarget()` |
| `src/server/main.cpp` | Deserializar `TargetPacket` (4 bytes) e chamar `handleTarget()` |
| `src/client/main.cpp` | Detecção de drag targeting durante `PHASE_BATTLE` |
| `src/client/renderer.h` | `drawTargetArrow()`, `drawTargetHighlight()` |
| `src/client/renderer.cpp` | Renderização da seta, brilho e highlight |

#### Edge cases

- **Alvo morre**: `runCombat()` detecta `!focused.alive()` e reseta `targetFocus_ = -1`. Herói volta ao ataque automático.
- **Alvo sai de adjacência**: Se o herói ou alvo se move e não estão mais adjacentes, resetar foco.
- **Round ends**: `resetForRound()` deve resetar `targetFocus_ = -1` para todos os heróis.
- **Herói morre**: Heróis mortos não podem ter foco nem ser alvo.
- **Multiplicidade**: Todos os inimigos adjacentes são visíveis (borda normal), mas só o focado recebe destaque especial.

---

## Fase 2 — Estruturas de Dados (EDOO)

### 2.1 — Lista Encadeada (sistema de efeitos com duração)

**Problema atual**: Buffs de zona são aplicados uma vez no início da batalha. Não há sistema de efeitos com duração — ultimates modificam stats diretamente e dependem de timers manuais (`ultActiveTimer_`).

**Solução**: Criar uma Lista Encadeada genérica (`LinkedList<T>`) e usá-la para gerenciar efeitos ativos (buffs, debuffs) com duração. Cada efeito tem `duration`, `tick()`, e remove-se automaticamente.

#### Arquivo novo: `include/linked_list.h`
```cpp
template<typename T>
class LinkedList {
    struct Node { T data; Node* next; };
    // insert(), remove(), find(), forEach()
    // Iterador customizado
};

// Efeito ativo
struct ActiveEffect {
    uint8_t effectType;   // BUFF_AD, BUFF_HP, BUFF_ARM, etc.
    int magnitude;
    float duration;       // segundos restantes
};
```

#### Modificações:
- `Hero` → manter `LinkedList<ActiveEffect> effects_` (substituir `uint8_t buff_`)
- `Hero::applyBuff()` → adicionar efeito na lista com duração específica
- `Hero::tickEffects()` → decrementar duração, remover expirados, recalcular stats
- Permite múltiplos efeitos simultâneos, stacks, e debuffs futuros
- **Reutilizada no Graph (2.2) como lista de adjacência**

---

### 2.2 — Grafo + BFS Pathfinding (maior impacto visual/gameplay)

**Problema atual**: `Hero::chooseMove()` move em linha reta na diagonal. `Game::autoBattleMove()` verifica `isAdjacentTo()` e `chooseMove()`. Não há pathfinding real — heróis emperram se um aliado bloqueia o caminho.

**Solução**: Modelar o grid 8x8 como um grafo com arestas entre células adjacentes (8 direções). BFS encontra o caminho mais curto evitando obstáculos (heróis vivos no caminho).

#### Arquivo novo: `include/graph.h`
```cpp
class Graph {
    // Adjacency list using LinkedList<GridCell> (ties into 2.1)
    // BFS shortest path from (sx,sy) to (tx,ty) avoiding blocked cells
    // Consider hero positions as blocked nodes
};
```

#### Modificações:
- `Game::autoBattleMove()` → usar BFS para calcular próximo passo do caminho
- Eliminar `Hero::chooseMove()` (ou simplificar para apenas retornar destino)
- Assassin pode usar BFS para caminhos de flanco diferentes

---

### 2.3 — Fila de Prioridade (ordem de combate)

**Problema atual**: `runCombat()` itera linearmente por todos os heróis. Na prática isso significa que o player 0 sempre ataca primeiro.

**Solução**: Usar uma Priority Queue (heap) ordenada por attack speed (`as_rate`). Heróis com maior attack speed atacam primeiro.

#### Arquivo novo: `include/priority_queue.h`
```cpp
template<typename T>
class PriorityQueue {
    // Max-heap implementado com array dinamico
    // insert(), extractMax(), peek(), isEmpty()
    // Demonstração: heapify, siftUp, siftDown
};
```

#### Modificações:
- `Game::runCombat()` → construir PriorityQueue de heróis ordenados por `as_rate`
- Processar ataques em ordem de prioridade
- Re-inserir herói com timer atualizado (cooldown)

---

### 2.4 — Árvore de Torneio (sistema de rounds)

**Problema atual**: Sistema de rounds é linear (best of 5), sem estrutura de dados representacional.

**Solução**: Modelar como árvore binária de torneio. Cada nó armazena o resultado do round. Demonstra criação, travessia e busca em árvore.

#### Arquivo novo: `include/tournament_tree.h`
```cpp
struct MatchNode {
    int roundNumber;
    uint8_t winner;       // 0, 1, ou 0xFF (ainda não jogado)
    uint8_t scores[2];    // [p0_wins, p1_wins]
    MatchNode* left;
    MatchNode* right;
    MatchNode* parent;
};

class TournamentTree {
    // build(), recordResult(), getWinner()
    // in-order traversal para mostrar histórico
};
```

#### Modificações:
- `Game` → manter `TournamentTree matchTree_`
- Após cada `endRound()`: registrar resultado na árvore
- `buildSnapshot()` → incluir estado do torneio (para o HUD mostrar progresso best-of-5)
- Renderer → mostrar bracket visual simplificado

---

## Fase 3 — Bot/AI (para demonstração solo)

### 3.1 — Bot Simples no Servidor

**Problema**: Para testar sozinho, precisa abrir dois clientes.

**Solução**: Se Player 1 não conectar em X segundos, o servidor cria um bot que:
- Na positioning phase: coloca heróis em posições padrão (baseadas em archetype)
- Na battle phase: usa ability automaticamente quando disponível
- Seleção: escolhe trainer + heróis aleatórios

#### Modificações:
- `src/server/main.cpp` — se só 1 jogador após timeout, criar segundo treinador com heróis aleatórios, chamar `handleSelect()` e `initFromSelections()`
- `src/server/game.cpp` — se `pid == 1` é bot, auto-posicionar heróis e auto-ability

---

## Ordem de Execução

| # | Tarefa | Prioridade | Complexidade |
|---|--------|-----------|-------------|
| 1 | `resolveTimeLimit()` | Alta | Baixa |
| 2 | `generateBuffZones()` | Alta | Baixa |
| 3 | Direcionamento de ataque (target focus) | Alta | Alta |
| 4 | Barras de HP + texto de dano flutuante | Alta | Média |
| 5 | Efeitos visuais de morte + ult | Média | Média |
| 6 | Lista Encadeada (efeitos com duração) | Média | Média |
| 7 | Grafo + BFS pathfinding | Média | Alta |
| 8 | Fila de Prioridade (ordem combate) | Média | Média |
| 9 | Árvore de Torneio | Baixa | Média |
| 10 | Bot/AI solo | Baixa | Baixa |

**Nota**: 1-2 podem ser feitas rápido. 3 é a mecânica de targeting. 4-5 são visuais. 6-9 são as ED exigidas pela disciplina. 10 é o bot.

---

## Arquivos Novos Previstos

| Arquivo | Propósito |
|---------|-----------|
| `include/linked_list.h` | Lista encadeada genérica + iterador |
| `include/graph.h` | Grafo do grid + BFS |
| `include/priority_queue.h` | Heap para ordem de combate |
| `include/tournament_tree.h` | Árvore binária de torneio |

## Arquivos Modificados Previstos

| Arquivo | Mudanças |
|---------|----------|
| `include/protocol.h` | `INPUT_TARGET`, `TargetPacket`, `targetFocus` em `HeroNetState` |
| `include/hero.h` | `targetFocus_`, `LinkedList<ActiveEffect>` effects_ ao invés de `buff_` |
| `include/game.h` | `handleTarget()` declaration |
| `src/server/game.cpp` | `resolveTimeLimit`, `generateBuffZones`, `handleTarget`, `runCombat` com foco, BFS pathfinding, PriorityQueue |
| `src/heroes/hero.cpp` | `tickEffects()`, recalcular stats a partir da lista |
| `src/server/main.cpp` | Deserializar `TargetPacket` (4 bytes) e chamar `handleTarget()` |
| `src/client/renderer.h` | `FloatingText`, `drawTargetArrow()`, `drawTargetHighlight()`, efeitos visuais |
| `src/client/renderer.cpp` | `drawFloatingTexts`, hp bar, seta de target, brilho, death/ult effects |
| `src/client/main.cpp` | Detecção de drag targeting, mudanças de HP/estado |