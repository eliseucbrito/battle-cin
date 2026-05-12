# Plano de Implementação: Hero Cards e Layout Dinâmico

**Branch**: `feat/hero-cards` (criar a partir de `feat/shop-economy`)
**Depende de**: Nenhuma fase anterior (pode ser implementado independentemente)

---

## Visão Geral

Redesenhar o layout do jogo para tela cheia com posições dinâmicas, e adicionar cards de herói na parte inferior mostrando nome, vida, poder (carga da ultimate), stats e slots de itens (preparação para a loja).

### Layout Proposto (tela cheia)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ [P1 Portrait  Score]              [Timer]              [Score  P2 Portrait] │
│                                                                         │
│ ┌─────────┐  ┌──────────────────────────────────┐  ┌─────────────┐   │
│ │ P1       │  │                                  │  │          P2 │   │
│ │ Items    │  │           GRID 8x8               │  │  Items      │   │
│ │ ┌──┐     │  │                                  │  │      ┌──┐   │   │
│ │ │ 1│     │  │                                  │  │      │ 1│   │   │
│ │ └──┘     │  │                                  │  │      └──┘   │   │
│ │ ┌──┐     │  │                                  │  │      ┌──┐   │   │
│ │ │ 2│     │  └──────────────────────────────────┘  │      │ 2│   │   │
│ │ └──┘     │                                        │      └──┘   │   │
│ │ Trainer  │                                        │  Trainer    │   │
│ │ Poder    │                                        │  Poder      │   │
│ └─────────┘                                         └─────────────┘   │
│                                                                         │
│ ┌───────────────────────────────────────────────────────────────────────┐ │
│ │ [Card P1-0]  [Card P1-1]  [Card P1-2] | [Card P2-0]  [Card P2-1]  [Card P2-2] │ │
│ └───────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

### Cada Hero Card (inferior)

```
┌──────────────────────────┐
│  [Portrait] Nome Heroi   │
│             Tank          │
│  HP ████████░░░ 230/350  │
│  PWR ██████░░░░  75%      │
│  AD:15  ARM:18  AS:0.60  │
│  Items: [□][□][□][□]      │
└──────────────────────────┘
```

---

## Tarefas

### T1 — Layout Dinâmico / Tela Cheia

**Objetivo**: Converter o renderer de posições fixas (936x684) para layout dinâmico baseado no tamanho da tela.

**Detalhes**:
- Alterar `InitWindow(936, 684, ...)` para `SetConfigFlags(FLAG_FULLSCREEN_MODE); InitWindow(...)` ou permitir toggle com F11
- Criar struct `Layout` com posições calculadas:
  ```cpp
  struct Layout {
      float screenW, screenH;
      float topBarH;
      float bottomCardsH;
      float sidePanelW;
      float gridX, gridY, gridW, gridH;
      float cellW, cellH;
      // Cards region
      float cardsY, cardW, cardH;
      // Side panels
      float leftPanelX, leftPanelW;
      float rightPanelX, rightPanelW;
  };
  ```
- Criar função `Layout computeLayout()` que calcula tudo baseado em `GetScreenWidth()` e `GetScreenHeight()`
- Substituir todas as constantes fixas por campos da struct `Layout`
- A grade deve manter aspect ratio 1:1 (quadrada) e ser centralizada

**Mudanças no renderer**:
- `GX`, `GY`, `GW`, `GH`, `CELLW`, `CELLH` viram variáveis calculadas por `computeLayout()`
- Todas as funções de desenho recebem `const Layout& layout` como parâmetro
- Funções afetadas: `cellCenter`, `cellRect`, `drawGrid`, `drawBuffZones`, `drawPedestal`, `drawHero`, `drawHUD`, `drawOverlays`, `drawVSScreen`, `drawPlacementCursors`, `drawTrainerSelectMK`, `drawHeroSelectMK`, e funções de efeitos visuais

**Arquivos**:
- `src/client/renderer.h` — struct `Layout`, declaração `computeLayout()`, atualizar assinaturas
- `src/client/renderer.cpp` — implementar `computeLayout()`, refatorar coordenadas fixas
- `src/client/main.cpp` — calcular layout no loop, fullscreen toggle (F11)

---

### T2 — Extensão do Protocolo (Dados do Herói)

**Objetivo**: Adicionar campos necessários ao `HeroNetState` e `TrainerNetState` para transmitir stats completos e info da ultimate.

**Adições ao `HeroNetState`**:
```cpp
struct HeroNetState {
    uint8_t  x, y;
    uint16_t hp;
    uint16_t maxHp;
    uint8_t  ad;
    uint8_t  arm;
    uint8_t  asRate_x10;    // NOVO: attack speed * 10 (0.6 → 6, 1.2 → 12)
    uint8_t  archetype;
    uint8_t  heroDefIndex;
    uint8_t  buff;
    uint8_t  alive;
    uint8_t  ultActive;
    uint8_t  ultPct;         // NOVO: 0-100 carga %, 255 = ult ativa
    uint8_t  ownerId;
    int8_t   targetFocus;
    uint8_t  itemCount;      // NOVO: qtd de itens (0 por agora, até loja)
    uint8_t  items[4];       // NOVO: IDs dos itens (255 = vazio, por agora tudo 255)
};
```

**Adições ao `TrainerNetState`**:
```cpp
struct TrainerNetState {
    uint8_t  trainerId;
    uint8_t  score;
    uint8_t  heroCount;
    uint8_t  abilityReady;
    uint16_t gold;           // NOVO: ouro do treinador (0 até loja)
};
```

**Hero — novos getters**:
```cpp
// hero.h
float ultCooldownTimer() const { return ultCooldownTimer_; }
// asRate() já existe
```

**Game::buildSnapshot() — preencher novos campos**:
```cpp
// Para cada herói:
heroNet.asRate_x10 = (uint8_t)(hero.asRate() * 10.f);
if (hero.ultActive()) {
    heroNet.ultPct = 255;
} else if (hero.ultReady()) {
    heroNet.ultPct = 100;
} else {
    float pct = 1.0f - hero.ultCooldownTimer() / hero.ultimateCooldown();
    if (pct < 0.f) pct = 0.f;
    heroNet.ultPct = (uint8_t)(pct * 100.f);
}
heroNet.itemCount = 0; // placeholder até loja
for (int s = 0; s < 4; s++) heroNet.items[s] = 0xFF; // vazio

// Para cada trainer:
trainerNet.gold = (uint16_t)trainers_[i].gold(); // 0 até loja
```

**Arquivos**:
- `include/protocol.h` — novos campos em `HeroNetState` e `TrainerNetState`
- `include/hero.h` — novo getter `ultCooldownTimer()`
- `src/server/game.cpp` — preencher novos campos em `buildSnapshot()`

---

### T3 — Cards de Herói (Renderização)

**Objetivo**: Implementar `drawHeroCards()` que renderiza 6 cards na parte inferior da tela.

**Detalhes do card**:
- Largura: ~200px (escala com tela), Altura: ~130px
- Cor de fundo: escuro com borda na cor do time (azul P1, vermelho P2)
- Herói morto: card escurecido (alpha reduzido), HP mostrado como 0
- HP bar: verde/acima de 50%, amarelo/25-50%, vermelho/abaixo de 25%
- PWR bar (ultimate): azul escuro enquanto carrega, dourada quando pronta, brilhante quando ativa
- Stats line: "AD:15 ARM:18 AS:0.6"
- Item slots: 4 quadrados vazios (cinza escuro, 255 = vazio), ocupados com mini-ícone quando loja for implementada

**Layout dos cards**:
- P1 (3 cards) na metade esquerda do painel inferior
- P2 (3 cards) na metade direita do painel inferior
- Separador vertical entre P1 e P2
- Labels "P1" e "P2" acima dos cards

**Arquivos**:
- `src/client/renderer.h` — declarar `drawHeroCards()`
- `src/client/renderer.cpp` — implementar `drawHeroCards()`
- `src/client/main.cpp` — chamar `drawHeroCards()` no render loop (durante POSITIONING, BATTLE, ROUND_END)

---

### T4 — Painel Lateral de Itens (Placeholder)

**Objetivo**: Renderizar o painel lateral para cada jogador (P1 esquerda, P2 direita), com slots de itens e info do treinador.

**Detalhes**:
- Painel à esquerda (P1): x=0, largura=sidePanelW
- Painel à direita (P2): x=screenW-sidePanelW, largura=sidePanelW
- Conteúdo atual (sem loja ainda):
  - Nome do treinador
  - Indicador de poder do treinador (Ready / Cooldown)
  - Seção "ITENS" com "Nenhum item" ou slots vazios
  - Score
- Futuramente (loja): mostrará itens comprados com ícone e descrição

**Arquivos**:
- `src/client/renderer.h` — declarar `drawSidePanels()`
- `src/client/renderer.cpp` — implementar `drawSidePanels()`
- `src/client/main.cpp` — chamar `drawSidePanels()` no render loop

---

### T5 — HUD Atualizado com Layout Dinâmico

**Objetivo**: Refatorar o `drawHUD()` existente e `drawOverlays()` para usar layout dinâmico.

**Mudanças**:
- `drawHUD()` usa `Layout` para posicionar portraits de treinador, scores, timer e dicas de controle
- Scores agora incluem Gold (quando loja for implementada, mostrando 0)
- Timer centralizado no topo
- Dicas de controle reposicionadas no fundo dos painéis laterais
- `drawOverlays()` usa dimensões dinâmicas para overlays de fim de round/match

**Arquivos**:
- `src/client/renderer.h` — atualizar assinatura de `drawHUD()` e `drawOverlays()`
- `src/client/renderer.cpp` — refatorar com `Layout`
- `src/client/main.cpp` — atualizar chamadas

---

### T6 — Fullscreen Toggle

**Objetivo**: Permitir alternância entre janela e tela cheia com F11.

**Detalhes**:
- `InitWindow()` começa em janela (1280x720 como padrão')
- Tecla F11 faz toggle para fullscreen e vice-versa
- No `main.cpp`:
  ```cpp
  if (IsKeyPressed(KEY_F11)) {
      ToggleFullscreen();
  }
  ```
- Layout se recalcula automaticamente a cada frame via `computeLayout()`

**Arquivos**:
- `src/client/main.cpp` — F11 toggle, tamanho de janela inicial

---

## Ordem de Implementação

| # | Tarefa | Complexidade | Arquivos | Depends |
|---|--------|-------------|----------|---------|
| T1 | Layout dinâmico | Alta | renderer.h, renderer.cpp, main.cpp | — |
| T2 | Protocolo extendido | Média | protocol.h, hero.h, game.cpp | — |
| T3 | Cards de herói | Alta | renderer.h, renderer.cpp, main.cpp | T1, T2 |
| T4 | Painel lateral (placeholder) | Média | renderer.h, renderer.cpp, main.cpp | T1 |
| T5 | HUD atualizado | Média | renderer.h, renderer.cpp, main.cpp | T1 |
| T6 | Fullscreen toggle | Baixa | main.cpp | T1 |

### Ordem recomendada:

```
T2 (Protocolo)       ← sem dependência, pode ser feito em paralelo
      ↓
T1 (Layout dinâmico) ← sem dependência de T2, mas二者 precisam antes de T3-T5
      ↓
T6 (Fullscreen)      ← depende de T1
      ↓
T5 (HUD atualizado)  ← depende de T1
      ↓
T3 (Cards de herói)  ← depende de T1 + T2
      ↓
T4 (Painel lateral)  ← depende de T1
```

**Recomendação**: Fazer T1 e T2 primeiro (podem ser paralelos), depois T5 e T6, depois T3 e T4.

---

## Layout Dinâmico — Especificação Detalhada

### `computeLayout()`

```cpp
Layout computeLayout() {
    Layout l;
    l.screenW = (float)GetScreenWidth();
    l.screenH = (float)GetScreenHeight();

    // Margens
    float margin = 8.f;

    // Barra superior: scores, timer, portraits
    l.topBarH = 72.f;

    // Painel inferior: cards dos herois
    l.bottomCardsH = 148.f;

    // Painéis laterais: itens e info do treinador
    l.sidePanelW = 180.f;

    // Região do grid (centro)
    float availW = l.screenW - 2 * (l.sidePanelW + margin) - 2 * margin;
    float availH = l.screenH - l.topBarH - l.bottomCardsH - 3 * margin;

    // Grid quadrado
    l.gridW = fminf(availW, availH);
    l.gridH = l.gridW;
    l.gridX = l.sidePanelW + margin + (availW - l.gridW) * 0.5f;
    l.gridY = l.topBarH + margin + (availH - l.gridH) * 0.5f;

    l.cellW = l.gridW / GRID_COLS;
    l.cellH = l.gridH / GRID_ROWS;

    // Cards
    l.cardW = 195.f;
    l.cardH = 130.f;
    l.cardsY = l.screenH - l.bottomCardsH + margin;

    // Side panels
    l.leftPanelX = margin;
    l.leftPanelW = l.sidePanelW - 2 * margin;
    l.rightPanelX = l.screenW - l.sidePanelW + margin;
    l.rightPanelW = l.leftPanelW;

    return l;
}
```

### Card Layout — `drawHeroCards()`

```
Para P1 (3 cards, metade esquerda):
  cardsStartX = margin + (screenW/2 - 3*cardW - 2*gap) / 2
  card[0] = (cardsStartX, cardsY)
  card[1] = (cardsStartX + cardW + gap, cardsY)
  card[2] = (cardsStartX + 2*(cardW + gap), cardsY)

Para P2 (3 cards, metade direita):
  cardsStartX = screenW/2 + (screenW/2 - 3*cardW - 2*gap) / 2
  (mesma distribuição)
```

Cada card (195x130):
```
Y+0    ┌──────────────────────────────┐
       │ [Portrait 50x50] Nome       │  ← nome do herói (do HEROES local)
Y+16   │                  Archetype    │  ← badge colorido da classe
Y+26   │                              │
Y+36   │ HP ████████░░░ 230/350       │  ← barra de HP com números
Y+52   │ PWR ██████░░░░░ 75%          │  ← barra de carga da ultimate
Y+68   │ AD:15  ARM:18  AS:0.60       │  ← linha de stats
Y+82   │                              │
Y+96   │ ITENS: [□][□][□][□]          │  ← 4 slots de itens
Y+116  │                              │
Y+130  └──────────────────────────────┘
```

### Painel Lateral — `drawSidePanels()`

```
Left Panel (P1):
┌──────────────────┐
│  [Portrait]      │
│  Prof. Paulo      │
│  Score: 2         │
│                   │
│  ── PODER ──     │
│  [Rally] READY ✓ │
│                   │
│  ── ITENS ──     │
│  Nenhum item      │
│  [□][□][□][□]    │
│                   │
│  ── GOLD ──      │
│  🪙 200          │
└──────────────────┘
```

---

## Notas de Implementação

1. **Compatibilidade com loja**: Os slots de itens nos cards e painéisLaterais são placeholders que serão preenchidos quando a loja for implementada. O campo `items[4]` no protocolo já prepara isso.

2. **Herois mortos**: Cards de heróis mortos devem ter visual escurecido (alpha ~50%) e HP mostrando "0/maxHp" com barra vazia.

3. **Escala de UI**: Em telas muito pequenas, os cards podem ficar sobrepostos. Considerar tamanho mínimo de janela de 1024x768.

4. **Seleção de herói**: Os cards na parte inferior também podem ser usados durante POSITIONING para mostrar qual herói está selecionado (borda pulsante).

5. **Barra PWR (ultimate)**: Usa `ultPct` do protocolo:
   - 0-99%: azul escuro com gradiente crescente
   - 100%: dourado brilhante com glow ("PRONTO!")
   - 255 (ativa): dourado com animação pulsante

6. **Ataque Speed display**: Como `asRate_x10` é um uint8_t, o client converte: `asRate = heroNet.asRate_x10 / 10.f` e exibe como "0.6" ou "1.2".

7. **Gold**: Será 0 até a loja ser implementada. O display já fica no painel lateral pronto para quando gold for adicionado ao `Trainer`.