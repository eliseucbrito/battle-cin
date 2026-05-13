#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct TrainerDefEntry {
    std::string name;
    std::string discipline;
    uint8_t     abilityType;
    std::string abilityName;
    std::string abilityDesc;
    uint8_t     colorR, colorG, colorB;
    std::string portraitPath;
    std::string cardPath;
    std::string cardDeckPath;
};

struct HeroDefEntry {
    std::string name;
    std::string monologue;
    std::string description;
    uint8_t     archetype;
    uint8_t     trainerId;     // 1-based DB trainer id
    std::string className;
    int         hp;
    int         ad;
    int         arm;
    std::string assetPath;
    std::vector<std::string> ultimateNames;
    std::string dyingPhrase;
};

extern std::vector<TrainerDefEntry> g_trainerDefs;
extern std::vector<HeroDefEntry>    g_heroDefs;
