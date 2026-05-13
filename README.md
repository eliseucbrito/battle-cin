# Battle-CIn: Auto-Battle Game 

> **Plataforma**: Linux | **Engine**: Custom C++ + Raylib | **Rede**: UDP Autoritativo | **Gênero**: Auto-Battle Tático 1v1

---

- **Hierarquia de Heróis**: 5 classes (Tank, Fighter, Mage, Assassin, Support) com comportamentos e stats distintos via polimorfismo.
- **Arena Horizontal**: Layout Esquerda (Treinador 0) vs Direita (Treinador 1).
- **Treinadores (Professores)**: Cada treinador gerencia uma equipe de heróis e possui uma habilidade especial manual.
- **Ultimates Automáticos**: Heróis ativam seus poderes baseados em algoritmos probabilísticos e condições de batalha.
- **Multiplayer UDP**: Sincronização em tempo real entre cliente e servidor.
- **Histórico de Partidas**: Resultados salvos em banco de dados SQLite com ranking de treinadores.

---

## Controles

### Fase de Posicionamento

| Ação | P1 | P2 |
|------|----|----|
| Selecionar herói | `1` `2` `3` | `KP_1` `KP_2` `KP_3` |
| Mover cursor no grid | `W` `A` `S` `D` | `Setas` |
| Confirmar posição | `Espaço` | `Enter` |

### Fase de Batalha

| Ação | P1 | P2 |
|------|----|----|
| Usar habilidade do treinador | `Q` | `E` |
| Navegar itens | `W` `S` | `Seta Cima` `Seta Baixo` |
| Usar item | `F` | — |

---

## Imagens do jogo

<img width="1920" height="1080" alt="screenshot-2026-05-12_23-03-30" src="https://github.com/user-attachments/assets/8dd43d10-9876-4ce0-b0f8-19c9b0eea9c5" />


<img width="1640" height="1060" alt="screenshot-2026-05-12_23-04-50" src="https://github.com/user-attachments/assets/842a2614-21b0-4b3e-ba36-62195d93733f" />


<img width="1640" height="1060" alt="screenshot-2026-05-12_23-05-19" src="https://github.com/user-attachments/assets/7ab1ca60-650d-4112-a633-49c0c6c46e3e" />

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
- [x] **Histórico de Partidas**: Banco de dados SQLite salva resultados e exibe ranking ao final da partida.

### Visual e Feedback
- [x] **Barras de HP**: Exibidas acima de cada herói (verde > 50%, amarelo > 25%, vermelho ≤ 25%).
- [x] **Números de Dano Flutuantes**: Texto que sobe e desaparece ao receber dano (vermelho) ou cura (verde).
- [x] **Efeitos Visuais**: Expansão de anel branco na morte; explosão dourada na ultimate.
- [x] **Seta de Foco de Ataque**: Linha/seta indicando qual inimigo o herói está focando.
- [x] **Destaque de Inimigos Adjacentes**: Borda pulsante em inimigos válidos durante o drag de targeting.
- [x] **Tela VS**: Tela de transição entre seleção e posicionamento.
- [x] **Pedestais e Idle Breathing**: Elipse colorida sob cada herói, animação de respiração sutil.
- [x] **Animações de Ataque**: Slingshot melee, projéteis ranged, flash de hit.

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
- [x] **HashTable / Database**: Banco de dados SQLite para persistência de resultados e ranking.

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
├── tournament_tree.h → Árvore binária de torneio
├── database.h        → SQLite database para histórico e ranking

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
make run-solo
```

### Execução (2 Jogadores)

```bash
make run-2players
```

### Execução Manual

```bash
# Terminal 1 — Servidor
make run-server

# Terminal 2 — Jogador 0
make run-client0

# Terminal 3 — Jogador 1
make run-client1
```

> Na primeira execução, o banco de dados `battle_cin.db` será criado automaticamente com os dados dos heróis.

---

## Requisitos

- **C++17**
- **CMake** 3.15+
- **Raylib** (instalada no sistema ou via Conan)
- **SQLite3** (baixado automaticamente pelo Conan, ou instale manualmente: `sudo apt install libsqlite3-dev`)
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
│   ├── database.h
│   ├── protocol.h
│   ├── game.h
│   └── hero.h
├── src/
│   ├── heroes/         → Implementação das classes herdeiras de Hero
│   ├── server/         → Game Engine + Servidor UDP
│   ├── database.cpp    → SQLite database para histórico e ranking
│   └── client/         → Renderização Raylib + Input
├── assets/             → Imagens da arena e retratos dos heróis/treinadores
├── .opencode/plans/    → Roadmaps e planos de correção
└── Makefile            → Build simplificado (make, make run-2players, etc.)
```

---

## Créditos

Desenvolvido para a disciplina **Estrutura de Dados e Orientação a Objetos (EDOO)** — Centro de Informática (CIn), Universidade Federal de Pernambuco (UFPE).

**Integrantes:**

- Eliseu Cordeiro de Brito - ecb2@cin.ufpe.br
- Matheus Victor Alves da Silva - mvas2@cin.ufpe.br
- Cleyton Junior da Silva Cardoso - cjsc@cin.ufpe.br
- Hugo José Bento da Cunha - hjbc@cin.ufpe.br
