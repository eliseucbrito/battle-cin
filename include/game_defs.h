#pragma once
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Shared game data tables — used by both client (for display) and server
// (for game initialization). These are indices into the definition tables,
// NOT network IDs. Client selection indices must match these exactly.
// ─────────────────────────────────────────────────────────────────────────────

// ── Trainer Definitions ──────────────────────────────────────────────────────
// client renderer.h TrainerDef includes extra display fields (color, portrait).
// This table is the minimal server-side view.

static constexpr int N_TRAINERS = 4;

struct TrainerDefEntry {
    const char* name;
    const char* discipline;
    uint8_t     abilityType;   // ABILITY_* from protocol.h
    const char* portraitPath;  // asset path for client renderer
};

static constexpr TrainerDefEntry TRAINER_DEFS[N_TRAINERS] = {
    { "Prof. Juliano",   "Introducao a Programacao",   0, "" },  // Cyan
    { "Prof. Valeria",   "Banco de Dados",             1, "" },  // Purple
    { "Prof. Francisco", "Estrutura de Dados",         3, "" },  // Green
    { "Prof. David",     "Redes de Computadores",      2, "" },  // Orange
};

// ── Hero Definitions ─────────────────────────────────────────────────────────
// client renderer.h HeroDef includes extra display fields (className, assetPath).
// This table is the minimal server-side view with per-named-hero stats.

static constexpr int N_HEROES = 10;

struct HeroDefEntry {
    const char* name;
    uint8_t     archetype;     // ARCHETYPE_* from protocol.h
    int         hp;
    int         ad;
    int         arm;
    const char* assetPath;     // sprite path for client renderer
};

static constexpr HeroDefEntry HERO_DEFS[N_HEROES] = {
    // name                        archetype    hp   ad  arm  assetPath
    { "O Construto de Busca",      0,          350, 15, 18, "assets/heroes/O_Construto_de_Busca.png"    },
    { "O Guardiao dos Discos",     1,          280, 22, 10, "assets/heroes/O_Guardiao_dos_Discos.png"   },
    { "O Mestre Parser",           2,          200, 35,  5, "assets/heroes/O_Mestre_Parser.png"         },
    { "O Cientista Polarizado",    3,          220, 32,  3, "assets/heroes/O_Cientista_Polarizado.png"  },
    { "O Chip-Mestre",             4,          240, 12, 10, "assets/heroes/O_Chip-Mestre.png"           },
    { "A Burocrata do UML",        0,          360, 13, 20, "assets/heroes/A_Burocrata_do_UML.png"      },
    { "O Artista Vectorial",       2,          190, 38,  4, "assets/heroes/O_Artista_Vectorial.png"     },
    { "O Inspetor Flaky",          3,          215, 30,  2, "assets/heroes/O_Inspetor_Flaky.png"        },
    { "O Treinador Python",        4,          250, 14,  8, "assets/heroes/O_Treinador_Python.png"      },
    { "O Filosofo do Dilema",      1,          270, 24, 12, "assets/heroes/O_Filosofo_do_Dilema.png"    },
};

// ── Trainer Colors (visual identity for each professor) ─────────────────────
struct TrainerColor {
    uint8_t r, g, b;
};

static constexpr TrainerColor TRAINER_COLORS[N_TRAINERS] = {
    { 0,   255, 255 },  // Prof. Juliano   - Cyan
    { 128, 0,   128 },  // Prof. Valeria   - Purple (Roxo)
    { 0,   255, 0   },  // Prof. Francisco - Green
    { 255, 165, 0   },  // Prof. David     - Orange
};