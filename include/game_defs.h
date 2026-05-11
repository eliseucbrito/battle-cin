#pragma once
#include <cstdint>

static constexpr int N_TRAINERS = 2;

struct TrainerDefEntry {
    const char* name;
    const char* discipline;
    uint8_t     abilityType;
    const char* portraitPath;
};

static constexpr TrainerDefEntry TRAINER_DEFS[N_TRAINERS] = {
    { "Prof. Paulo",   "Estrutura de Dados",   0, "assets/trainer0.png" },
    { "Prof. Eliseu",  "Orient. a Objetos",    1, "assets/trainer1.png" },
};

static constexpr int N_HEROES = 10;

struct HeroDefEntry {
    const char* name;
    uint8_t     archetype;
    uint8_t     trainerIndex;
    int         hp;
    int         ad;
    int         arm;
    const char* assetPath;
};

static constexpr HeroDefEntry HERO_DEFS[N_HEROES] = {
    // Trainer 0
    { "O Construto de Busca",      0, 0, 350, 15, 18, "assets/heroes/O_Construto_de_Busca.png"    },
    { "O Guardiao dos Discos",     1, 0, 280, 22, 10, "assets/heroes/O_Guardiao_dos_Discos.png"   },
    { "O Mestre Parser",           2, 0, 200, 35,  5, "assets/heroes/O_Mestre_Parser.png"         },
    { "O Cientista Polarizado",    3, 0, 220, 32,  3, "assets/heroes/O_Cientista_Polarizado.png"  },
    { "O Chip-Mestre",             4, 0, 240, 12, 10, "assets/heroes/O_Chip-Mestre.png"           },
    // Trainer 1
    { "A Burocrata do UML",        0, 1, 360, 13, 20, "assets/heroes/A_Burocrata_do_UML.png"      },
    { "O Filosofo do Dilema",      1, 1, 270, 24, 12, "assets/heroes/O_Filosofo_do_Dilema.png"    },
    { "O Artista Vectorial",       2, 1, 190, 38,  4, "assets/heroes/O_Artista_Vectorial.png"     },
    { "O Inspetor Flaky",          3, 1, 215, 30,  2, "assets/heroes/O_Inspetor_Flaky.png"        },
    { "O Treinador Python",        4, 1, 250, 14,  8, "assets/heroes/O_Treinador_Python.png"      },
};
