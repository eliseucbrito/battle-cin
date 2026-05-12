# Battle-CIn — Contexto do Projeto

Este documento fornece um mapeamento completo da arquitetura e implementação do **Battle-CIn**, um jogo de auto-battle multiplayer desenvolvido em C++17. O objetivo é servir como guia de contexto para desenvolvedores e modelos de IA.

---

## 1. Visão Geral do Projeto

O **Battle-CIn** é um jogo de estratégia "Auto-Battler" (estilo Teamfight Tactics) onde dois treinadores selecionam heróis, compram itens e os posicionam em um grid. O combate é resolvido automaticamente pelo servidor seguindo regras de inteligência artificial e atributos de RPG.

### 1.1 Stack Tecnológica
- **Linguagem:** C++17 (ênfase em OOP e EDOO).
- **Gráficos:** [Raylib](https://www.raylib.com/) (Client-side).
- **Banco de Dados:** [SQLite3](https://www.sqlite.org/) (Server-side persistência).
- **Rede:** Sockets UDP customizados (comunicação binária).
- **Build System:** CMake / Makefile.

### 1.2 Arquitetura de Rede (Client-Server)
O projeto utiliza uma arquitetura baseada em **Snapshots**:
- **Servidor (`src/server`):** Autoridade máxima. Mantém o estado do jogo (`Game.cpp`), processa o combate, gerencia a loja e persiste dados no SQLite. Envia um `GameSnapshot` binário via UDP para todos os clientes conectados.
- **Cliente (`src/client`):** "Burro" em termos de lógica de jogo. Recebe o snapshot, interpola posições se necessário e renderiza a cena usando Raylib. Envia `InputPacket` com comandos do usuário (seleção, posicionamento, compra).

---

## 2. Estrutura de Diretórios

```bash
.
├── assets/             # Texturas (sprites), sons e fontes
├── build/              # Binários e arquivos de configuração (.ini)
├── include/            # Headers (.h) - Definições de classes e Protocolo
│   ├── protocol.h      # CRÍTICO: Definição dos pacotes de rede e estados
│   ├── linked_list.h   # Implementação manual de lista ligada genérica
│   └── ...
├── src/                # Implementações (.cpp)
│   ├── client/         # Loop principal do cliente e renderizador
│   ├── server/         # Loop principal do servidor e lógica de jogo
│   ├── heroes/         # Especializações dos heróis (Tank, Mage, etc.)
│   ├── database.cpp    # Camada de persistência SQLite
│   └── ...
└── CMakeLists.txt      # Configuração de build
```

---

## 3. Máquina de Estados do Jogo (`PHASE_`)

O fluxo do jogo é controlado por uma State Machine no servidor (`src/server/game.cpp`):

1.  **PHASE_SELECT:** Escolha de treinadores e composição inicial de 3 heróis.
2.  **PHASE_VS_INTRO:** Tela de "Versus" apresentando os jogadores.
3.  **PHASE_POSITIONING:** Jogadores posicionam heróis no grid 8x8.
4.  **PHASE_BATTLE:** Combate automático. IA decide movimentos e alvos.
5.  **PHASE_ROUND_END:** Resultado da rodada e atribuição de dano.
6.  **PHASE_SHOP:** Compra de itens e upgrades com ouro acumulado.
7.  **PHASE_MATCH_END:** Fim de jogo e salvamento no histórico (Database).

---

## 4. Mapeamento OOP (Orientação a Objetos)

### 4.1 Hierarquia de Classes (Herança)

#### Hero — Classe Base Abstrata
Define o comportamento base de combate e atributos (HP, AD, ARM).
```
Hero (abstrata)                     include/hero.h
├── HeroTank   : public Hero        include/hero_tank.h
├── HeroFighter : public Hero       include/hero_fighter.h
├── HeroMage   : public Hero        include/hero_mage.h
├── HeroAssassin : public Hero      include/hero_assassin.h
└── HeroSupport : public Hero       include/hero_support.h
```

#### Item — Prototype + Template Method
Sistema de itens aplicáveis a heróis ou treinadores.
```
Item (abstrata)                     include/shop.h
├── HealthPotion / StrengthGem / SteelArmor
├── ArcaneOrb / PhoenixFeather / MirrorShield
└── RallyAll / HealWave (Itens de suporte)
```

### 4.2 Design Patterns Implementados

| Padrão | Localização | Propósito |
|---|---|---|
| **Factory Method** | `hero_factory.h` | Instancia a subclasse correta de `Hero` via ID. |
| **Strategy** | `shop.h` | `PricingStrategy` muda como os preços da loja escalam. |
| **Prototype** | `shop.h` | Itens são clonados (`clone()`) a partir de um catálogo base. |
| **State Machine** | `game.cpp` | Gerencia a transição entre fases de jogo. |
| **Observer-like** | `renderer.cpp` | Cliente observa o snapshot e reage com efeitos visuais. |
| **Template Method**| `shop.h` | `Item::use()` define o esqueleto de aplicação do item. |

---

## 5. Estruturas de Dados (EDOO)

O projeto evita `std::list` ou `std::queue` em pontos críticos para demonstrar domínio de estruturas manuais:

-   **Linked List (`linked_list.h`):** Usada para gerenciar `ActiveEffects` (buffs/debuffs) em cada herói.
-   **Priority Queue (`priority_queue.h`):** Max-heap usada para determinar a ordem de ataque baseada em `Attack Speed`.
-   **Graph (`graph.h`):** O grid é tratado como um grafo para pathfinding via **BFS** (Busca em Largura).
-   **Tournament Tree (`tournament_tree.h`):** Árvore binária usada para organizar o chaveamento de vitórias.

---

## 6. Persistência e CRUD (SQLite3)

O servidor gerencia um arquivo `battle_cin.db` via `Database.cpp`:
-   **Create:** `saveMatch()` salva resultados ao fim da partida.
-   **Read:** `getRanking()` e `getRecentMatches()` alimentam o placar.
-   **Update:** `updateHeroStats()` permite persistir buffs permanentes.
-   **Delete:** `clearHistory()` limpa os logs.

---

## 7. Protocolo de Comunicação (`protocol.h`)

Os pacotes são binários e compactos usando `#pragma pack(1)`:
-   `InputPacket`: Envia `x, y, heroIndex, type` do cliente para o servidor.
-   `GameSnapshot`: O "deus ex machina" que contém todo o estado (2 treinadores, todos os heróis, loja, timer, fase).

---

## 8. Memória e Semântica de C++

-   **Smart Pointers:** Uso extensivo de `std::unique_ptr` para heróis dentro do `Trainer`.
-   **Move Semantics:** Implementada em `LinkedList` e `PriorityQueue` (Rule of 5).
-   **Const Correctness:** Getters e métodos de cálculo marcados como `const`.
-   **Explicit:** Construtores de classe única (ex: `Hero(uint8_t ownerId)`) são `explicit`.

---

## 9. Como Rodar e Debugar

### 9.1 Build
```bash
mkdir build && cd build
cmake ..
make
```

### 9.2 Execução
1.  Inicie o servidor primeiro: `./battle_cin_server`
2.  Inicie dois clientes: `./battle_cin_client`
3.  Configure IP/Porta no `debug_config.ini` se necessário (padrão `127.0.0.1:7777`).

---

*Este documento é a "fonte da verdade" para a estrutura do Battle-CIn. Qualquer alteração arquitetural deve ser refletida aqui.*

