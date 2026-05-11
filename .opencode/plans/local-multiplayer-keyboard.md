# Plano de Implementação: Conversão para Local Multiplayer (Teclado Compartilhado)

**Branch**: `feat/local-multiplayer-keyboard`
**Base**: `main`

---

## Visão Geral

**Situação atual**: Cliente (`meu_projeto`) e servidor (`servidor`) são executáveis separados comunicando via UDP (127.0.0.1:7777). Dois jogadores precisam de duas instâncias do cliente e um servidor rodando.

**Situação desejada**: Um único executável. A lógica do Game roda in-process. Dois jogadores no mesmo teclado:
- **P1**: Setas direcionais + Enter
- **P2**: WASD + Space

Tela de seleção estilo MK11 com retratos grandes nas laterais. Posicionamento de heróis por teclado (substituindo mouse drag). Target focus mantido mas desabilitado por enquanto.

---

## Arquitetura

```
ANTES:
  Cliente 0 ──UDP──► Servidor ──UDP──► Cliente 1

DEPOIS:
  ┌─────────────────────────────────────────┐
  │           Cliente Único                  │
  │  ┌──────────┐    ┌──────────────────┐   │
  │  │ Game     │    │ Raylib UI        │   │
  │  │ (logic)  │◄───┤ Input P1+P2      │   │
  │  ├──────────┤    ├──────────────────┤   │
  │  │ Database │    │ MK-style Select  │   │
  │  │ SQLite   │    │ Keyboard Position│   │
  │  └──────────┘    └──────────────────┘   │
  └─────────────────────────────────────────┘
```

---

## Fase 1 — Fundir Servidor no Cliente (Remover UDP)

### 1.1 — CMakeLists.txt

**Mudança**: Incorporar `game.cpp` e `database.cpp` no target do cliente, remover target `servidor`.

```cmake
add_executable(meu_projeto
    src/client/main.cpp
    src/client/renderer.cpp
    src/server/game.cpp         # antes era só do servidor
    src/database.cpp            # antes era só do servidor
    src/heroes/hero.cpp
    src/heroes/hero_tank.cpp
    src/heroes/hero_fighter.cpp
    src/heroes/hero_mage.cpp
    src/heroes/hero_assassin.cpp
    src/heroes/hero_support.cpp
    src/hero_factory.cpp
    src/trainer.cpp
    src/tournament_tree.cpp
)
target_link_libraries(meu_projeto raylib nlohmann_json::nlohmann_json SQLite::SQLite3)
# add_executable(servidor ...)  ← REMOVIDO
```

**Ficheiros**: `CMakeLists.txt`

### 1.2 — main.cpp: Loop Local sem UDP

Substituir toda a lógica de socket UDP por instância local de `Game`:

```cpp
// ANTES:
int sock = socket(AF_INET, SOCK_DGRAM, 0);
sendto(sock, &packet, ..., &serverAddr, ...);
recvfrom(sock, &snapshot, ...);

// DEPOIS:
Game game;
game.registerPlayerLocal(0);
game.registerPlayerLocal(1);

float accumulator = 0.f;
while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    accumulator += dt;

    while (accumulator >= 1.f/20.f) {
        accumulator -= 1.f/20.f;
        processInputLocal(game);   // input P1 + P2 -> game handlers
        game.update(1.f/20.f);
    }

    GameSnapshot snap;
    game.buildSnapshot(snap);
    renderAll(snap, inputs);       // renderização unificada
}
```

**Detalhe**: Manter tick rate de 20Hz para o game.update() (como o servidor atual), mas renderizar a 60fps com Raylib.

**Ficheiros**: `src/client/main.cpp`

### 1.3 — game.h/cpp: Remover Dependência de sockaddr_in

```cpp
// trainer.h — remover:
sockaddr_in addr_;
bool matchesAddr(const sockaddr_in& a) const;
const sockaddr_in& addr() const;
void setAddr(const sockaddr_in& a);

// game.h — adicionar:
void registerPlayerLocal(int pid);   // marca como conectado sem socket
```

O `Game::handleSelect()`, `handlePlaceHero()`, `handleUseAbility()`, `handleTarget()` mantêm as mesmas assinaturas (sem sockaddr).

**Ficheiros**: `include/game.h`, `include/trainer.h`, `src/server/game.cpp`, `src/trainer.cpp`

### 1.4 — src/server/main.cpp

Remover do build. Manter ficheiro no repositório para referência mas não compilar.

**Ficheiros**: `CMakeLists.txt`

### 1.5 — Makefile

Atualizar targets:
```makefile
# ANTIGOS (remover):
# run-server, run-client0, run-client1, run-2players

# NOVOS:
run-local: build
	@cd $(BUILD_DIR) && ./meu_projeto

run-solo: build
	@cd $(BUILD_DIR) && ./meu_projeto --solo
```

**Ficheiros**: `Makefile`

---

## Fase 2 — Input Dual (P1: Setas+Enter, P2: WASD+Space)

### 2.1 — Struct PlayerInputState

```cpp
struct PlayerInput {
    // Seleção
    int  trainerCursor = 0;
    int  trainerLocked  = -1;         // -1 = não travado
    int  heroCursor     = 0;
    std::vector<int> heroPicks;
    bool herosLocked    = false;

    // Posicionamento
    int   moveHeroIdx   = 0;          // qual herói mover (0-2)
    bool  isPositioning = false;
    uint8_t cursorX     = 0;
    uint8_t cursorY     = 0;

    // Batalha
    bool abilityReady   = true;       // pode usar habilidade este round
};

PlayerInput inputs[2];  // [0] = P1, [1] = P2
```

**Ficheiros**: `src/client/main.cpp`

### 2.2 — Input Seleção de Trainer

```cpp
// P1 (Setas + Enter)
if (IsKeyPressed(KEY_RIGHT)) inputs[0].trainerCursor = (inputs[0].trainerCursor + 1) % N_TRAINERS;
if (IsKeyPressed(KEY_LEFT))  inputs[0].trainerCursor = (inputs[0].trainerCursor + N_TRAINERS - 1) % N_TRAINERS;
if (IsKeyPressed(KEY_ENTER) && inputs[0].trainerLocked < 0) {
    inputs[0].trainerLocked = inputs[0].trainerCursor;
    game.handleLocalTrainerLock(0, inputs[0].trainerLocked);
}

// P2 (WASD + Space)
if (IsKeyPressed(KEY_D))      inputs[1].trainerCursor = (inputs[1].trainerCursor + 1) % N_TRAINERS;
if (IsKeyPressed(KEY_A))      inputs[1].trainerCursor = (inputs[1].trainerCursor + N_TRAINERS - 1) % N_TRAINERS;
if (IsKeyPressed(KEY_SPACE) && inputs[1].trainerLocked < 0) {
    inputs[1].trainerLocked = inputs[1].trainerCursor;
    game.handleLocalTrainerLock(1, inputs[1].trainerLocked);
}
```

**Ficheiros**: `src/client/main.cpp`

### 2.3 — Input Seleção de Heróis

```cpp
// Navegação P1 — grid de heróis do trainer escolhido
int heroGridCols = 5;
if (IsKeyPressed(KEY_RIGHT)) moveHeroCursor(inputs[0], +1, heroGridCols);
if (IsKeyPressed(KEY_LEFT))  moveHeroCursor(inputs[0], -1, heroGridCols);
if (IsKeyPressed(KEY_UP))    moveHeroCursor(inputs[0], -heroGridCols);
if (IsKeyPressed(KEY_DOWN))  moveHeroCursor(inputs[0], +heroGridCols);
if (IsKeyPressed(KEY_ENTER)) toggleHeroPick(inputs[0], inputs[0].heroCursor);

// Se P2 travou os heróis também
// Navegação P2
if (IsKeyPressed(KEY_D))  moveHeroCursor(inputs[1], +1, heroGridCols);
...
if (IsKeyPressed(KEY_SPACE)) toggleHeroPick(inputs[1], inputs[1].heroCursor);
```

Quando ambos os jogadores têm 3 heróis selecionados:
```cpp
if (inputs[0].herosLocked && inputs[1].herosLocked) {
    game.handleLocalHeroPick(0, inputs[0].heroPicks);
    game.handleLocalHeroPick(1, inputs[1].heroPicks);
    game.initFromSelections();
}
```

**Ficheiros**: `src/client/main.cpp`

### 2.4 — Input Posicionamento por Teclado

Substituir mouse drag por navegação no grid com teclado:

```cpp
if (snap.phase == PHASE_POSITIONING) {
    // P1 — selecionar herói para mover (1, 2, 3)
    if (IsKeyPressed(KEY_ONE))   inputs[0].moveHeroIdx = 0;
    if (IsKeyPressed(KEY_TWO))   inputs[0].moveHeroIdx = 1;
    if (IsKeyPressed(KEY_THREE)) inputs[0].moveHeroIdx = 2;

    // P1 — mover cursor no grid
    if (IsKeyPressed(KEY_UP))    inputs[0].cursorY = max(0, inputs[0].cursorY - 1);
    if (IsKeyPressed(KEY_DOWN))  inputs[0].cursorY = min(7, inputs[0].cursorY + 1);
    if (IsKeyPressed(KEY_LEFT))  inputs[0].cursorX = max(0, inputs[0].cursorX - 1);
    if (IsKeyPressed(KEY_RIGHT)) inputs[0].cursorX = min(7, inputs[0].cursorX + 1);

    // P1 — confirmar posição
    if (IsKeyPressed(KEY_ENTER)) {
        game.handlePlaceHero(0, inputs[0].moveHeroIdx,
                            inputs[0].cursorX, inputs[0].cursorY);
    }

    // P2 — análogo com WASD + Space, but espelhado para colunas 4-7
}
```

Renderizar cursor de posicionamento para cada jogador com sua cor característica.

**Ficheiros**: `src/client/main.cpp`, `src/client/renderer.cpp`, `src/client/renderer.h`

### 2.5 — Input Batalha (Ability + Target Focus)

```cpp
if (snap.phase == PHASE_BATTLE) {
    // P1 ability: Q
    if (IsKeyPressed(KEY_Q)) game.handleUseAbility(0);
    // P2 ability: E
    if (IsKeyPressed(KEY_E)) game.handleUseAbility(1);

    // Target focus: DESABILITADO por agora
    // Manter código existente, mas sem enviar INPUT_TARGET
}
```

**Ficheiros**: `src/client/main.cpp`

---

## Fase 3 — Renderização MK-Style

### 3.1 — Layout Visual

Ambos jogadores veem a mesma tela. Layout:

```
┌─────────────────────────────────────────────────────────┐
│                    ⏱ 08.5                              │
│  ┌──────────┐                           ┌──────────┐  │
│  │          │                           │          │  │
│  │ TRAINER  │                           │ TRAINER  │  │
│  │   P1     │                           │   P2     │  │
│  │ (grande) │                           │ (grande) │  │
│  │          │                           │          │  │
│  │  READY?  │                           │  READY?  │  │
│  └──────────┘                           └──────────┘  │
│                                                         │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐                   │
│  │ Tk │ │ Fg │ │ Mg │ │ As │ │ Sp │ ← 5 herois       │
│  └────┘ └────┘ └────┘ └────┘ └────┘                   │
│  P1: ←→↑↓ + Enter     P2: WASD + Space               │
└─────────────────────────────────────────────────────────┘
```

**Cores**:
- P1: Azul ({80, 150, 255, 255})
- P2: Vermelho ({255, 100, 80, 255})

### 3.2 — drawTrainerSelectMK()

```cpp
void drawTrainerSelectMK(
    const TrainerDef* trainers, int nTrainers,
    const PlayerInput& p1, const PlayerInput& p2,
    float timer);
```

- Fundo: overlay escuro sobre a arena
- Timer central (8s countdown)
- Portrait P1 esquerda (cursor se não travou, ou trainer escolhido)
- Portrait P2 direita (cursor se não travou, ou trainer escolhido)
- Grid de 2 trainers no centro-inferior
- Dois cursores (P1 azul, P2 vermelho)
- Labels "READY!" ao travar
- Controles no rodapé

**Ficheiros**: `src/client/renderer.h`, `src/client/renderer.cpp`

### 3.3 — drawHeroSelectMK()

```cpp
void drawHeroSelectMK(
    const TrainerDef* trainers,
    const HeroDef* heroes, int nHeroes,
    const PlayerInput& p1, const PlayerInput& p2,
    float timer);
```

- Portrait fixo do trainer escolhido nas laterais
- 3 slots de time abaixo de cada portrait
- Grid de 5 heróis (filtrados por trainerIndex)
- Dois cursores independentes
- Timer de 15 segundos

**Ficheiros**: `src/client/renderer.h`, `src/client/renderer.cpp`

### 3.4 — drawPlacementCursors()

```cpp
void drawPlacementCursors(const GameSnapshot& snap,
                           const PlayerInput& p1, const PlayerInput& p2);
```

- Cursor azul na célula selecionada por P1
- Cursor vermelho na célula selecionada por P2
- Destaque no herói selecionado para mover
- Indicador de zona válida (colunas do arquétipo)

**Ficheiros**: `src/client/renderer.h`, `src/client/renderer.cpp`

---

## Fase 4 — Selecção com Timer e Auto-Pick

### 4.1 — PHASE_SELECT no Game

Novo phase no `protocol.h` e lógica em `Game`:

```cpp
#define PHASE_SELECT 6  // novo, antes de PHASE_VS_INTRO

// Campos em Game:
uint8_t  selectSubphase_ = 0;   // 0 = trainer, 1 = hero
float    selectTimer_    = 0.f;
bool     trainerLocked_[2] = {false, false};
uint8_t  trainerChoice_[2] = {0};
bool     herosLocked_[2]   = {false, false};
uint8_t  heroChoices_[2][3] = {{0}};
```

**Lógica em `Game::update()`**:
```cpp
case PHASE_SELECT:
    selectTimer_ -= dt;
    if (selectSubphase_ == 0) {
        if (selectTimer_ <= 0 || (trainerLocked_[0] && trainerLocked_[1])) {
            for (int i = 0; i < 2; i++)
                if (!trainerLocked_[i]) trainerChoice_[i] = rand() % N_TRAINERS;
            selectSubphase_ = 1;
            selectTimer_ = 15.f;
        }
    } else {
        if (selectTimer_ <= 0 || (herosLocked_[0] && herosLocked_[1])) {
            for (int i = 0; i < 2; i++)
                if (!herosLocked_[i]) autoPickHeroes(i);
            initFromSelections();
        }
    }
    break;
```

**Ficheiros**: `include/protocol.h`, `include/game.h`, `src/server/game.cpp`

### 4.2 — Auto-Pick

```cpp
void Game::autoPickHeroes(int pid) {
    if (herosLocked_[pid]) return;
    uint8_t tIdx = trainerChoice_[pid];
    std::vector<int> available;
    for (int i = 0; i < N_HEROES; i++)
        if (HERO_DEFS[i].trainerIndex == tIdx)
            available.push_back(i);
    std::random_shuffle(available.begin(), available.end());
    for (int h = 0; h < 3; h++)
        heroChoices_[pid][h] = (uint8_t)available[h];
    herosLocked_[pid] = true;
}
```

**Ficheiros**: `include/game.h`, `src/server/game.cpp`

### 4.3 — Campos no GameSnapshot

```cpp
// Em protocol.h, adicionar ao GameSnapshot:
uint8_t  selectSubphase;       // 0 = trainer, 1 = hero
float    selectTimer;           // segundos restantes
uint8_t  trainerLocked[2];     // 0/1
uint8_t  trainerChoice[2];     // índice escolhido
uint8_t  herosLocked[2];
uint8_t  heroPicks[2][3];     // índices (0xFF = vazio)
```

**Ficheiros**: `include/protocol.h`

---

## Fase 5 — Atualizar Definições (2 Trainers + Herois Vinculados)

### 5.1 — game_defs.h

```cpp
static constexpr int N_TRAINERS = 2;  // era 4

struct HeroDefEntry {
    const char* name;
    uint8_t     archetype;
    uint8_t     trainerIndex;  // NOVO
    int         hp, ad, arm;
    const char* assetPath;
};

// 5 heróis por trainer (1 de cada classe)
static constexpr HeroDefEntry HERO_DEFS[N_HEROES] = {
    // Trainer 0
    { "O Construto de Busca",   ARCHETYPE_TANK,     0, 350, 15, 18, "..." },
    { "O Guardiao dos Discos",  ARCHETYPE_FIGHTER,   0, 280, 22, 10, "..." },
    { "O Mestre Parser",        ARCHETYPE_MAGE,      0, 200, 35,  5, "..." },
    { "O Cientista Polarizado", ARCHETYPE_ASSASSIN,  0, 220, 32,  3, "..." },
    { "O Chip-Mestre",          ARCHETYPE_SUPPORT,   0, 240, 12, 10, "..." },
    // Trainer 1
    { "A Burocrata do UML",     ARCHETYPE_TANK,      1, 360, 13, 20, "..." },
    { "O Filosofo do Dilema",   ARCHETYPE_FIGHTER,   1, 270, 24, 12, "..." },
    { "O Artista Vectorial",    ARCHETYPE_MAGE,      1, 190, 38,  4, "..." },
    { "O Inspetor Flaky",       ARCHETYPE_ASSASSIN,  1, 215, 30,  2, "..." },
    { "O Treinador Python",     ARCHETYPE_SUPPORT,   1, 250, 14,  8, "..." },
};
```

Sincronizar arrays locais em `main.cpp` (`TRAINERS[]`, `HEROES[]`).

**Ficheiros**: `include/game_defs.h`, `src/client/main.cpp`, `src/client/renderer.h`

---

## Fase 6 — Solo Mode (Bot)

### 6.1 — Flag --solo

```cpp
int main(int argc, char* argv[]) {
    bool soloMode = (argc >= 2 && strcmp(argv[1], "--solo") == 0);
    // Em solo mode, P2 é bot
    if (soloMode) {
        game.createBot(1);  // bot auto-seleciona trainer + heroes
    }
}
```

Em modo solo, a tela de seleção mostra apenas P1. P2 é auto-pickado.

**Ficheiros**: `src/client/main.cpp`

---

## Fase 7 — Database (Sem Mudanças)

O `SQLite` funciona localmente. O `Game` já tem `Database db_` que cria `battle_cin.db`. Nenhuma mudança necessária em `database.h` ou `database.cpp`.

---

## Ordem de Implementação Sugerida

| # | Tarefa | Complexidade | Ficheiros |
|---|--------|-------------|-----------|
| 1 | game_defs.h: 2 trainers + trainerIndex | Média | game_defs.h, main.cpp, renderer.h |
| 2 | CMakeLists.txt: fundir targets | Baixa | CMakeLists.txt |
| 3 | game.h/cpp: remover sockaddr | Média | game.h, trainer.h, game.cpp, trainer.cpp |
| 4 | game.h/cpp: adicionar PHASE_SELECT | Alta | protocol.h, game.h, game.cpp |
| 5 | Makefile: novos targets | Baixa | Makefile |
| 6 | main.cpp: loop local sem UDP | Alta | main.cpp |
| 7 | main.cpp: input P1+P2 seleção + posicionamento | Alta | main.cpp |
| 8 | renderer.h/cpp: drawTrainerSelectMK() | Alta | renderer.h, renderer.cpp |
| 9 | renderer.h/cpp: drawHeroSelectMK() | Alta | renderer.h, renderer.cpp |
| 10 | renderer.h/cpp: drawPlacementCursors() | Média | renderer.h, renderer.cpp |
| 11 | Solo mode flag + bot | Baixa | main.cpp |

---

## Resumo de Ficheiros

| Ficheiro | Mudanças |
|----------|----------|
| `CMakeLists.txt` | Remover target servidor, adicionar game.cpp + database.cpp ao cliente, linkar SQLite |
| `Makefile` | run-local + run-solo (remover run-2players, run-server, etc.) |
| `include/game_defs.h` | N_TRAINERS=2, trainerIndex em HeroDefEntry |
| `include/protocol.h` | PHASE_SELECT, campos selectSubphase/selectTimer no GameSnapshot |
| `include/game.h` | add registerPlayerLocal, handleLocalTrainerLock, handleLocalHeroPick, campos PHASE_SELECT |
| `include/trainer.h` | Remover sockaddr_in, matchesAddr, addr |
| `src/server/game.cpp` | Lógica PHASE_SELECT, auto-pick, remover dependência rede |
| `src/trainer.cpp` | Remover matchesAddr |
| `src/client/main.cpp` | Reescrever loop: Game local, input P1+P2, sem UDP |
| `src/client/renderer.h` | drawTrainerSelectMK, drawHeroSelectMK, drawPlacementCursors, drawSelectTimer |
| `src/client/renderer.cpp` | Implementar renderização MK-style com 2 cursores |

**Inalterados**: database.h, database.cpp, src/heroes/*, hero.h, linked_list.h, graph.h, priority_queue.h, tournament_tree.h
