# Mapeamento do Repositório: Battle-CIn

Este documento descreve a estrutura atual do projeto "Battle-CIn", um MVP de jogo auto-battle desenvolvido em C++ com Raylib para a disciplina de EDOO.

## Estrutura de Arquivos

| Arquivo | Descrição | Papel no Projeto |
| :--- | :--- | :--- |
| `protocol.h` | Header de Protocolo | Define as estruturas de dados compartilhadas (`GameSnapshot`, `InputPacket`), constantes de rede, fases do jogo e tipos de buffs. É a "ponte" entre o cliente e o servidor. |
| `server.cpp` | Servidor do Jogo | Contém a lógica principal (Game Engine). Gerencia o estado dos jogadores, a máquina de estados das fases (Waiting, Positioning, Battle) e a simulação de combate/movimento. |
| `src/main.cpp` | Cliente (Raylib) | Responsável pela renderização gráfica, feedback visual (efeitos de dano, partículas) e captura de inputs do jogador (drag-and-drop na fase de posicionamento). |
| `src/arena.png` | Asset de Fundo | Imagem da arena de combate. |
| `CMakeLists.txt` | Build System | Configuração do CMake para compilar os dois executais: `meu_projeto` (cliente) e `servidor`. |
| `conanfile.txt` | Dependências | Gerenciador de pacotes Conan para incluir `raylib` e `nlohmann_json`. |

---

## Arquitetura Atual (MVP)

### Servidor (`server.cpp`)
Atualmente, a lógica de orientação a objetos é básica:
- **`class Player`**: Representa uma unidade no tabuleiro. Possui atributos como `hp`, `ad`, `arm`, `x`, `y` e métodos para `resetStats`, `applyBuff` e `attackTarget`.
- **`class Game`**: Gerencia a lista de jogadores, o loop de atualização (`update`) e a geração de zonas de buff.
- **Combate**: Melee (adjacência) com cálculo simples de dano.
- **Networking**: UDP operando a 20Hz.

### Cliente (`src/main.cpp`)
- **Renderização**: Desenha o grid, unidades e HUD usando Raylib.
- **Interpolação**: Usa `lerp` para suavizar o movimento das unidades recebido do servidor.
- **Fases**: Mostra overlays específicos para cada estado do jogo enviado pelo servidor.

---

## O que falta (Requisitos de EDOO)

Com base na sua descrição, o projeto precisa evoluir para utilizar conceitos avançados de OO:

1.  **Herança e Polimorfismo**:
    - Criar uma classe base `Hero` e subclasses para diferentes tipos (ex: `Guerreiro`, `Mago`, `Arqueiro`).
    - Cada classe de herói deve ter multiplicadores e comportamentos de ataque distintos.
2.  **Treinadores (Professores)**:
    - Implementar a classe `Trainer` que "possui" heróis.
    - O treinador pode influenciar os atributos iniciais ou ter habilidades especiais baseadas na disciplina que leciona.
3.  **Gerenciamento de Memória (Ponteiros/Referências)**:
    - Uso de `std::vector<Hero*>` ou `std::unique_ptr<Hero>` para gerenciar as unidades dinamicamente.
    - Passagem por referência em métodos de combate para evitar cópias desnecessárias de objetos grandes.
4.  **Classes de Disciplinas**:
    - Mapear heróis às disciplinas dos professores, possivelmente usando o padrão *Factory* ou *Strategy* para definir os multiplicadores.

## Próximas Tasks Sugeridas

1.  **Refatorar `Player` para `Hero`**: Transformar a classe genérica em uma hierarquia.
2.  **Implementar o Sistema de Treinadores**: Criar a lógica onde o jogador escolhe um Professor que determina seu herói/disciplina.
3.  **Melhorar o Protocolo**: Adaptar o `GameSnapshot` para suportar diferentes tipos de heróis (visuais e stats variados).
