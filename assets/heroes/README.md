# Assets de Heróis — Guia

## Como adicionar um novo herói

1. Coloque o arquivo PNG nesta pasta: `assets/heroes/<nome>.png`
   - Tamanho recomendado: **64×64** ou **128×128** pixels
   - Fundo **transparente** (PNG com canal alpha)
   - Nome do arquivo em **minúsculas**, sem espaços (use underscores)

2. No código (`include/hero.h`), adicione o mapeamento de nome de asset:
   ```cpp
   // Em HeroArchetype ou na subclasse:
   virtual const char* assetPath() const { return "assets/heroes/meu_heroi.png"; }
   ```

3. O `CMakeLists.txt` copia automaticamente toda a pasta `assets/` para o diretório de build.
   Nenhuma alteração no build system é necessária.

## Assets atuais

| Arquivo | Archetype | Status |
| :--- | :--- | :---: |
| `golem.png` | Tank | ✅ |

## Archetypes disponíveis

| Archetype | Papel | Sugestão de visual |
| :--- | :--- | :--- |
| Tank | Front-line, absorve dano | Grande, pesado, armadura |
| Fighter | Equilibrado, melee | Guerreiro com espada/escudo |
| Mage | Back-line, dano alto | Mago com cajado/livro |
| Assassin | Flanco, velocidade | Ágil, adagas, encapuzado |
| Support | Cura aliados | Healers, bastão de luz |
