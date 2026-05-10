# Battle-CIn: Auto-Battle Game (EDOO)

> **Plataforma**: Linux | **Engine**: Custom C++ + Raylib | **Rede**: UDP Autoritativo | **Gênero**: Auto-Battle Tático 1v1

---

## O que é o Battle-CIn?

**Battle-CIn** é um jogo **auto-battle tático 1v1** desenvolvido para a disciplina de **Estrutura de Dados e Orientação a Objetos (EDOO)** no CIn-UFPE. Dois jogadores escolhem um Treinador (professor) e montam uma equipe de 3 heróis com classes distintas. Após o posicionamento inicial, a batalha ocorre **automaticamente**: heróis se movem, atacam e ativam ultimates por conta própria. O jogador pode apenas **direcionar o foco de ataque** de seus heróis (arrastando uma seta de um herói para um inimigo adjacente) e usar a **habilidade do treinador** (1 vez por round).

O jogo roda em **best-of-5 rounds** — quem vencer 3 rounds primeiro leva a partida. Cada round dura no máximo 120 segundos; se o tempo esgotar, vence quem tiver mais % de HP total restante.

---

## Controles

| Ação | Entrada |
|------|---------|
| Selecionar treinador/heróis | `WASD` / `Setas` + `Enter` / `Espaço` |
| Posicionar herói | Clique e arraste (fase de posicionamento) |
| Usar habilidade do treinador | `Q` (1 vez por round) |
| Direcionar foco de ataque | Clique e arraste de herói próprio → inimigo adjacente |

---

## Features Implementadas

### Core Gameplay
- [x] **5 Classes de Heróis**: Tank, Fighter, Mage, Assassin, Support — cada uma com stats, ultimate e comportamento únicos via polimorfismo.
- [x] **Treinadores (Professores)**: 4 treinadores com habilidades especiais distintas (Rally, Shield Wall, Battle Heal, Frenzy).
- [x] **Auto-Battle**: Heróis se movem e atacam automaticamente durante a fase de batalha.
- [x] **Ultimates Automáticas**: Cada classe tem uma ultimate com condição de ativação e probabilidade por tick.
- [x] **Sistema de Rounds**: Best-of-5 com timer de 120s por round, resolução por HP% em caso de empate.
- [x] **Zonas de Buff**: 2-3 zonas aleatórias surgem no grid central a cada round (AD, HP, ARM).
- [x] **Bot Solo**: Se apenas 1 jogador conectar, um bot é criado automaticamente após 5s.

### Visual e Feedback
- [x] **Barras de HP**: Exibidas acima de cada herói (verde > 50%, amarelo > 25%, vermelho ≤ 25%).
- [x] **Números de Dano Flutuantes**: Texto que sobe e desaparece ao receber dano (vermelho) ou cura (verde).
- [x] **Efeitos Visuais**: Expansão de anel branco na morte; explosão dourada na ultimate.
- [x] **Seta de Foco de Ataque**: Linha/seta indicando qual inimigo o herói está focando.
- [x] **Destaque de Inimigos Adjacentes**: Borda pulsante em inimigos válidos durante o drag de targeting.
- [x] **Tela VS**: Tela de transição entre seleção e posicionamento.

### Rede e Protocolo
- [x] **Servidor Autoritativo UDP**: 20Hz tick rate, snapshots broadcast para clientes.
- [x] **Seleção via Rede**: Clientes enviam `SelectionPacket` com treinador + 3 heróis escolhidos.
- [x] **Target Focus via Rede**: `TargetPacket` (4 bytes) para definir foco de ataque durante batalha.
- [x] **Heartbeat**: Keep-alive do cliente para manter conexão ativa.

### Estruturas de Dados (EDOO)
- [x] **LinkedList<T>**: Lista encadeada genérica usada para efeitos ativos (buffs/debuffs com duração) e lista de adjacência do grafo.
- [x] **Graph + BFS**: Grafo 8x8 com BFS para pathfinding de heróis, evitando obstáculos (outros heróis).
- [x] **PriorityQueue (Max-Heap)**: Fila de prioridade para ordem de ataque por attack speed (`as_rate`).
- [x] **TournamentTree**: Árvore binária de torneio para rastrear histórico de rounds (best-of-5).

---

## Features Planejadas / Em Aberto

### Mecânicas de Jogo
- [ ] **Efeitos com Duração Real**: Buffs e debuffs com duração de verdade (stacks, múltiplos efeitos simultâneos).
- [ ] **Novos Tipos de Buff**: Debuffs (redução de AD/ARM), status effects (stun, slow, poison).
- [ ] **IA do Bot com Target Focus**: Bot também deve usar target focus em vez de atacar o primeiro adjacente.
- [ ] **Dificuldade do Bot**: Níveis de dificuldade (agressivo, defensivo, aleatório).

### Qualidade de Vida
- [ ] **Efeitos Sonoros**: Sons para ataques, ultimates, morte, buff zone.
- [ ] **Reconexão**: Suporte a desconectar e reconectar sem perder a partida.
- [ ] **Conflito de Treinador**: Impedir que ambos os jogadores escolham o mesmo treinador (ou adicionar UI de aviso).
- [ ] **Espectador**: Permitir 3º cliente assistir sem jogar.

### Persistência e Meta
- [ ] **Salvar/Recarregar**: Persistir histórico de partidas para arquivo (JSON).
- [ ] **Estatísticas de Partida**: Win rate, heróis mais usados, média de duração de rounds.
- [ ] **Modo Campanha / Tutorial**: Single-player contra bot com progressão.

### Técnicos / Refatoração
- [ ] **QuadTree / Spatial Hash**: Otimizar detecção de proximidade em grids maiores.
- [ ] **Replay System**: Gravar inputs do servidor e reproduzir partidas.
- [ ] **Testes Automatizados**: Unit tests para `LinkedList`, `Graph`, `PriorityQueue`, `TournamentTree`.

---

## Arquitetura Técnica

```
Servidor (src/server/)
├── game.cpp          → Loop principal, fases, auto-battle, pathfinding, combate
├── main.cpp          → Socket UDP, deserialização de pacotes, broadcast de snapshots

Cliente (src/client/)
├── main.cpp          → Loop de jogo, input, interpolação visual, envio de pacotes
├── renderer.cpp      → Renderização de grid, heróis, HUD, efeitos, targeting

Heróis (src/heroes/)
├── hero.cpp          → Lógica base: stats, ataque, movimento, buffs, ultimate
├── hero_factory.cpp  → Criação polimórfica (TankHero, MageHero, etc.)

EDOO (include/)
├── linked_list.h     → Lista encadeada genérica + iterador
├── graph.h           → Grafo do grid + BFS shortest path
├── priority_queue.h  → Max-heap para ordem de combate
└── tournament_tree.h → Árvore binária de torneio

Protocolo (include/)
└── protocol.h        → Pacotes UDP: InputPacket, SelectionPacket, TargetPacket, GameSnapshot
```

---

## Como Rodar

### Build

```bash
# Build completo (Makefile wrapper)
make

# Ou via CMake/Conan
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Execução (Solo — Human vs Bot)

```bash
# Terminal 1
make run-server

# Terminal 2 (esperar 5s para o bot entrar automaticamente)
make run-client0
```

### Execução (2 Jogadores)

```bash
# Terminal 1
make run-server

# Terminal 2
make run-client0

# Terminal 3
make run-client1
```

---

## Requisitos

- **C++17**
- **CMake** 3.15+
- **Raylib** (instalada no sistema ou via Conan)
- **Conan** (opcional, para gerenciamento de dependências)
- **Linux** (desenvolvido e testado em ambiente Linux)

---

## Estrutura de Pastas

```
battle-cin/
├── include/            → Headers (.h) das classes e protocolo
│   ├── linked_list.h
│   ├── graph.h
│   ├── priority_queue.h
│   ├── tournament_tree.h
│   ├── protocol.h
│   ├── game.h
│   └── hero.h
├── src/
│   ├── heroes/         → Implementação das classes herdeiras de Hero
│   ├── server/         → Game Engine + Servidor UDP
│   └── client/         → Renderização Raylib + Input
├── assets/             → Imagens da arena e retratos dos heróis/treinadores
├── .opencode/plans/    → Roadmaps e planos de correção
└── Makefile            → Build simplificado (make, make run-server, etc.)
```

---

## Créditos

Desenvolvido para a disciplina **Estrutura de Dados e Orientação a Objetos (EDOO)** — Centro de Informática (CIn), Universidade Federal de Pernambuco (UFPE).
