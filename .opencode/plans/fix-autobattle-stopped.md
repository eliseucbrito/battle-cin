# Plano de Correção: Auto-Battle Parou

**Contexto**: Após implementar o roadmap completo (target focus, BFS pathfinding, Priority Queue, Tournament Tree, LinkedList, bot), o auto-battle parou de funcionar. Os heróis não se movem nem atacam.

**Arquivos envolvidos**:
- `src/server/game.cpp` (`autoBattleMove`, `runCombat`)
- `src/heroes/hero.cpp` (`recalcStats`, `applyBuff`)
- `include/protocol.h`

---

## 1. Bug CRITICAL — BFS não encontra caminho para células bloqueadas

**Arquivo**: `src/server/game.cpp:320-366` (`autoBattleMove()`)

**Problema**: `autoBattleMove()` marca TODAS as posições de heróis vivos como bloqueadas no grid (linhas 329-334), incluindo inimigos. Depois chama `graph_.findPath()` para encontrar caminho até o inimigo-alvo. Mas `findPath()` no BFS pula células bloqueadas (linha 81 de `graph.h`). Logo, a célula do alvo está bloqueada e o BFS nunca a alcança — path não encontrado, herói não se move.

**Consequência**: Sem movimento, heróis nunca ficam adjacentes, nunca atacam. Batalha 100% parada.

**Fix**:
```cpp
// Antes de findPath, desbloquear célula do alvo:
blocked[target->y()][target->x()] = false;
int nx, ny;
if (graph_.findPath(hero.x(), hero.y(), target->x(), target->y(), blocked, nx, ny)) {
    hero.setPosition(nx, ny);
    hero.startMoveTimer();
}
// Re-bloquear após pathfinding:
blocked[target->y()][target->x()] = true;
```

---

## 2. Bug HIGH — `recalcStats()` reseta HP para máximo

**Arquivo**: `src/heroes/hero.cpp:85-104`

**Problema**: `recalcStats()` faz `hp_ = maxHp_` (ou `customHp_`) toda vez que é chamado. Isso acontece em `applyBuff()` (início da batalha) e em `tickEffects()` (quando buffs expiram). Resultado: heróis são curados para full HP continuamente, ficando invencíveis.

**Consequência**: Batalhas nunca terminam por eliminação; sempre vai para time limit.

**Fix**:
- Em `recalcStats()`: NUNCA setar `hp_` diretamente. Apenas recalcular `maxHp_`, `ad_`, `arm_`, `as_rate_`, `ms_delay_`.
- Ao final de `recalcStats()`, fazer `if (hp_ > maxHp_) hp_ = maxHp_;` (clamp).
- Em `applyBuff()`, se `type == BUFF_HP`, adicionar `hp_ += magnitude` (cura imediata ao receber buff de HP), depois clamp.

---

## 3. Bug MEDIUM — Array `blocked` fica inconsistente após movimento

**Arquivo**: `src/server/game.cpp:350-363`

**Problema**: Após `hero.setPosition(nx, ny)`, o código faz `blocked[hero.y()][hero.x()] = true`. Mas `hero.y()/x()` agora é a posição NOVA. A posição ANTIGA (desbloqueada na linha 354) nunca é re-bloqueada. Isso permite que outros heróis, no mesmo tick, façam pathfinding através da célula antiga — podendo gerar sobreposição.

**Fix**: Salvar `oldX = hero.x()`, `oldY = hero.y()` antes de desbloquear. Após `setPosition`, re-bloquear `blocked[oldY][oldX] = true`.

---

## 4. Enhancement — Integrar `targetFocus` no movimento

**Arquivo**: `src/server/game.cpp:340-346`

**Problema**: `autoBattleMove()` SEMPRE escolhe o inimigo mais próximo para se mover, ignorando `targetFocus`. Se o jogador direcionou um herói para atacar inimigo X, o herói ainda se move em direção ao inimigo mais próximo (que pode ser Y), não fazendo sentido.

**Fix**:
```cpp
Hero* moveTarget = nullptr;
if (hero.targetFocus() >= 0) {
    Hero& focused = trainers_[1-i].heroAt(hero.targetFocus());
    if (focused.alive()) moveTarget = &focused;
}
if (!moveTarget) {
    // fallback: inimigo mais próximo (lógica atual)
}
```

---

## 5. Enhancement — Heróis mortos não devem atacar

**Arquivo**: `src/server/game.cpp:393-424` (`runCombat()`)

**Problema**: Em `runCombat()`, a fila de combatentes (`PriorityQueue`) é construída no início. Se um herói morre durante o processamento (ex: atacado por outro herói de prioridade maior), ele ainda pode atacar quando for removido da fila, pois não há check de `alive()` após `pop()`.

**Fix**: Adicionar `if (!hero.alive()) continue;` após `Combatant c = pq.pop();`.

---

## Ordem de Execução

| # | Tarefa | Prioridade | Arquivo |
|---|--------|-----------|---------|
| 1 | Desbloquear célula-alvo no BFS | Critical | `src/server/game.cpp` |
| 2 | Corrigir `recalcStats()` / `applyBuff()` | High | `src/heroes/hero.cpp` |
| 3 | Corrigir `blocked` array (posição antiga) | Medium | `src/server/game.cpp` |
| 4 | Integrar `targetFocus` no movimento | Medium | `src/server/game.cpp` |
| 5 | Pular heróis mortos em `runCombat()` | Low | `src/server/game.cpp` |

---

## Validação

Após as correções:
1. Build: `make clean && make`
2. Teste solo: `make run-server` (terminal 1) + `make run-client0` (terminal 2, esperar 5s para bot entrar)
3. Observar: heróis devem se mover em direção uns aos outros, ficar adjacentes, e começar a atacar automaticamente.
4. Testar target focus: durante batalha, clicar e arrastar de um herói próprio para um inimigo adjacente. O herói deve focar ataques nesse inimigo e continuar se movendo se o inimigo se afastar.
5. Verificar que HP não reseta: heróis devem manter HP danificado e morrer quando HP chegar a 0.
