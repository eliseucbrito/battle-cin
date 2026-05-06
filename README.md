# Battle-CIn: Auto-Battle Game (EDOO)

Este é um jogo auto-battle desenvolvido para a disciplina de **Estrutura de Dados e Orientação a Objetos (EDOO)** no CIn-UFPE. O projeto utiliza C++, Raylib e conceitos avançados de OOP como Herança, Polimorfismo, Encapsulamento e Composição.

## Funcionalidades Atuais (Versão Modular)

- **Hierarquia de Heróis**: 5 classes (Tank, Fighter, Mage, Assassin, Support) com comportamentos e stats distintos via polimorfismo.
- **Arena Horizontal**: Layout Esquerda (Treinador 0) vs Direita (Treinador 1).
- **Treinadores (Professores)**: Cada treinador gerencia uma equipe de heróis e possui uma habilidade especial manual (tecla Q).
- **Ultimates Automáticos**: Heróis ativam seus poderes baseados em algoritmos probabilísticos e condições de batalha.
- **Multiplayer UDP**: Sincronização em tempo real entre cliente e servidor.

## Requisitos

- **C++17**
- **CMake** (3.15+)
- **Raylib**
- **Conan** (opcional, para gerenciamento de dependências)

## Como Rodar o Projeto

### 1. Preparar Dependências

Se estiver usando o ambiente configurado com Conan:

```bash
mkdir -p build
conan install . --output-folder=build --build=missing
cd build
cmake .. -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Release
cmake --build .

```

Ou instale a `raylib` no seu sistema e use o CMake padrão:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. Executar o Servidor

O servidor deve ser iniciado primeiro:

```bash
./servidor
```

### 3. Executar os Clientes

Abra dois terminais adicionais para os jogadores:

**Jogador 1 (Esquerda):**
```bash
./meu_projeto 0
```

**Jogador 2 (Direita):**
```bash
./meu_projeto 1
```

## Controles

- **Fase de Posicionamento**: Clique e arraste seus heróis para as colunas permitidas pela classe deles.
- **Fase de Batalha**: 
    - Os heróis lutam automaticamente.
    - Pressione **Q** para usar a habilidade especial do seu Treinador (1 vez por round).
- **Ultimates**: Ativam sozinhos quando as condições são atingidas (ex: Tank com vida baixa ganha armadura).

## Estrutura de Pastas

- `include/`: Headers (`.h`) das classes e protocolo.
- `src/heroes/`: Implementação das classes herdeiras de `Hero`.
- `src/server/`: Lógica da Game Engine e Servidor UDP.
- `src/client/`: Lógica de renderização Raylib e Input.
- `assets/`: Imagens da arena e retratos dos heróis/treinadores.
