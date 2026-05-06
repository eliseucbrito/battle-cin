#pragma once
#include <stdint.h>

// ── Game phases ───────────────────────────────────────────────────────────────
#define PHASE_WAITING      0   // waiting for 2nd player
#define PHASE_POSITIONING  1   // drag-and-drop positioning (20 s)
#define PHASE_BATTLE       2   // autobattle — no player input
#define PHASE_ROUND_END    3   // brief pause after a point is scored
#define PHASE_MATCH_END    4   // match over

// ── Buff types ────────────────────────────────────────────────────────────────
#define BUFF_NONE  0
#define BUFF_AD    1   // +15 AD  — aggressive
#define BUFF_HP    2   // +50 HP  — survival
#define BUFF_ARM   3   // +10 ARM — defensive

// ── Hero archetypes ───────────────────────────────────────────────────────────
#define ARCHETYPE_TANK      0
#define ARCHETYPE_FIGHTER   1
#define ARCHETYPE_MAGE      2
#define ARCHETYPE_ASSASSIN  3
#define ARCHETYPE_SUPPORT   4

// ── Trainer ability types ─────────────────────────────────────────────────────
#define ABILITY_RALLY        0   // +AD for all heroes
#define ABILITY_SHIELD_WALL  1   // +ARM for all heroes
#define ABILITY_BATTLE_HEAL  2   // heal all heroes
#define ABILITY_FRENZY       3   // +AS for all heroes

// ── Buff bonuses (legacy buff zones) ─────────────────────────────────────────
#define BUFF_AD_BONUS    15
#define BUFF_HP_BONUS    50
#define BUFF_ARM_BONUS   10

// ── Match rules ───────────────────────────────────────────────────────────────
#define WIN_SCORE         5
#define POSITIONING_TIME  20
#define BATTLE_MAX_TIME   120
#define ROUND_END_TIME    3
#define MAX_HEROES_SIDE   3
#define MAX_HEROES_TOTAL  (MAX_HEROES_SIDE * 2)

// ── Network ───────────────────────────────────────────────────────────────────
#define SERVER_PORT  7777
#define GRID_COLS    8
#define GRID_ROWS    8

// ── Input types ───────────────────────────────────────────────────────────────
#define INPUT_HEARTBEAT   0   // keep-alive / no action
#define INPUT_PLACE       1   // drag-and-drop: move hero to (placeX, placeY)
#define INPUT_USE_ABILITY 2   // trainer presses Q to use ability

// ── Packets ───────────────────────────────────────────────────────────────────
#pragma pack(push, 1)

struct InputPacket {
    uint8_t playerId;    // 0 or 1
    uint8_t type;        // INPUT_* constants
    uint8_t placeX;      // target cell X (INPUT_PLACE only)
    uint8_t placeY;      // target cell Y (INPUT_PLACE only)
    uint8_t heroIndex;   // which hero to move (INPUT_PLACE only)
};

struct BuffZoneInfo {
    uint8_t x, y, type;
};

// Network state of a single hero (sent in snapshot)
struct HeroNetState {
    uint8_t  x, y;
    uint16_t hp;
    uint16_t maxHp;
    uint8_t  ad;
    uint8_t  arm;
    uint8_t  archetype;   // ARCHETYPE_* constant
    uint8_t  buff;
    uint8_t  alive;
    uint8_t  ultActive;   // 1 if ultimate is currently active
    uint8_t  ownerId;     // trainer index: 0 or 1
};

// Network state of a trainer
struct TrainerNetState {
    uint8_t trainerId;      // trainer type/index (for name/asset lookup)
    uint8_t score;
    uint8_t heroCount;
    uint8_t abilityReady;   // 1 if ability can be used this round
};

struct GameSnapshot {
    uint8_t        phase;
    uint8_t        timer;            // seconds remaining in current phase
    TrainerNetState trainers[2];
    uint8_t        heroCount;        // total heroes in the heroes[] array
    HeroNetState   heroes[MAX_HEROES_TOTAL];
    uint8_t        buffZoneCount;
    BuffZoneInfo   buffZones[4];
    uint8_t        roundWinner;      // 0xFF = ongoing
    uint8_t        matchWinner;      // 0xFF = ongoing
};

#pragma pack(pop)
