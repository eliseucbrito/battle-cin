# Plano de Merge: ranking-screen → feat/shop-economy

**Branch de origem**: `origin/feature/ranking-screen`
**Branch de destino**: `feat/shop-economy` (atual)
**Estratégia**: Manter nossos padrões, adicionar apenas as mudanças da outra branch

---

## O que a branch `feature/ranking-screen` adiciona

| O que | Arquivos | Tipo |
|---|---|---|
| Tela de ranking no fim da partida | `src/client/renderer.cpp` | +34/-5 linhas |
| Sprites coloridos dos heróis | `assets/heroes/*.png` (20 PNGs) | Novos arquivos |

Nenhuma mudança em `game_defs.h`, `main.cpp`, `game.cpp`, etc.

---

## Código a alterar

### `src/client/renderer.cpp` — bloco `PHASE_MATCH_END` (linhas 705-711)

**Antes** (nosso código atual):
```cpp
    if (snap.phase == PHASE_MATCH_END) {
        DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 200});
        const char* res = (snap.matchWinner == 0) ? "P1 VENCEU!" : "P2 VENCEU!";
        DrawText(res, (int)(midX - MeasureText(res, 60) * 0.5f), (int)(sh * 0.44f), 60, GOLD);
        const char* restart = "Feche o jogo para reiniciar";
        DrawText(restart, (int)(midX - MeasureText(restart, 20) * 0.5f), (int)(sh * 0.58f), 20, LIGHTGRAY);
    }
```

**Depois** (adaptado para posições proporcionais):
```cpp
    if (snap.phase == PHASE_MATCH_END) {
        DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 200});

        const char* res = (snap.matchWinner == (uint8_t)myId) ? "VITÓRIA!" : "DERROTA";
        Color resColor = (snap.matchWinner == (uint8_t)myId) ? GOLD : RED;
        DrawText(res, (int)(midX - MeasureText(res, 60) * 0.5f), (int)(sh * 0.26f), 60, resColor);

        uint8_t t0 = snap.trainers[0].trainerId;
        uint8_t t1 = snap.trainers[1].trainerId;

        char placar[64];
        snprintf(placar, sizeof(placar), "%s  %d x %d  %s",
                 TRAINER_DEFS[t0].name, snap.trainers[0].score,
                 snap.trainers[1].score, TRAINER_DEFS[t1].name);
        DrawText(placar, (int)(midX - MeasureText(placar, 24) * 0.5f), (int)(sh * 0.41f), 24, WHITE);

        float sepW = sw * 0.573f;
        DrawRectangle((int)(midX - sepW * 0.5f), (int)(sh * 0.47f), (int)sepW, 1, {255, 255, 255, 60});
        DrawText("RANKING", (int)(midX - MeasureText("RANKING", 18) * 0.5f), (int)(sh * 0.49f), 18, GOLD);

        int first  = (snap.trainers[0].score >= snap.trainers[1].score) ? 0 : 1;
        int second = 1 - first;

        char linha1[64], linha2[64];
        snprintf(linha1, sizeof(linha1), "1. %s - %d pts",
                 TRAINER_DEFS[snap.trainers[first].trainerId].name,
                 snap.trainers[first].score);
        snprintf(linha2, sizeof(linha2), "2. %s - %d pts",
                 TRAINER_DEFS[snap.trainers[second].trainerId].name,
                 snap.trainers[second].score);

        DrawText(linha1, (int)(midX - MeasureText(linha1, 20) * 0.5f), (int)(sh * 0.53f), 20, WHITE);
        DrawText(linha2, (int)(midX - MeasureText(linha2, 20) * 0.5f), (int)(sh * 0.58f), 20, LIGHTGRAY);

        DrawText("Feche o jogo para reiniciar",
                 (int)(midX - MeasureText("Feche o jogo para reiniciar", 16) * 0.5f), (int)(sh * 0.66f), 16, {160, 160, 160, 255});
    }
```

### Diferenças do código original da branch

O código do `origin/feature/ranking-screen` usa posições absolutas (936×684).
Adaptamos para posições proporcionais ao `g_layout`:

| Elemento | Original (px) | Adaptado |
|---|---|---|
| VITÓRIA/DERROTA Y | 180 | `sh * 0.26f` |
| Placar Y | 280 | `sh * 0.41f` |
| Separador X/width | 200/536 | `midX - sepW*0.5f` / `sepW = sw*0.573f` |
| RANKING Y | 335 | `sh * 0.49f` |
| 1º lugar Y | 365 | `sh * 0.53f` |
| 2º lugar Y | 395 | `sh * 0.58f` |
| Restart Y | 450 | `sh * 0.66f` |

---

## Passos de execução

1. Copiar os 20 PNGs da branch de origem para o working tree
2. Editar `src/client/renderer.cpp` substituindo o bloco `PHASE_MATCH_END`
3. Compilar e verificar

---

## Verificação

```bash
cd build && cmake .. && make
```
Testar tela de fim de partida (ranking com nomes de treinadores e placar).
