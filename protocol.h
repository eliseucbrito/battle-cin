#pragma once
#include <stdint.h>

// ── Game phases ───────────────────────────────────────────────────────────────
#define PHASE_WAITING     0   // waiting for 2nd player
#define PHASE_POSITIONING 1   // drag-and-drop positioning + buff zone choice (10 s)
#define PHASE_BATTLE      2   // autobattle — no player input
#define PHASE_ROUND_END   3   // brief pause after a point is scored
#define PHASE_MATCH_END   4   // match over

// ── Buff types ────────────────────────────────────────────────────────────────
#define BUFF_NONE  0
#define BUFF_AD    1   // +15 AD  — aggressive
#define BUFF_HP    2   // +50 HP  — survival
#define BUFF_ARM   3   // +10 ARM — defensive

// ── Base stats ────────────────────────────────────────────────────────────────
#define BASE_HP          200
#define BASE_AD          20
#define BASE_ARM         8
#define BASE_AS          0.8f    // attacks / second
#define BASE_MS_DELAY    0.30f   // seconds per cell during autobattle

#define BUFF_AD_BONUS    15
#define BUFF_HP_BONUS    50
#define BUFF_ARM_BONUS   10

// ── Match rules ───────────────────────────────────────────────────────────────
#define WIN_SCORE        5
#define POSITIONING_TIME 10
#define BATTLE_MAX_TIME  120
#define ROUND_END_TIME   3

// ── Network ───────────────────────────────────────────────────────────────────
#define SERVER_PORT  7777
#define GRID_COLS    8
#define GRID_ROWS    8

// ── Input types ───────────────────────────────────────────────────────────────
#define INPUT_HEARTBEAT  0   // keep-alive / no action
#define INPUT_PLACE      1   // drag-and-drop: move unit to (placeX, placeY)

// ── Packets ───────────────────────────────────────────────────────────────────
#pragma pack(push, 1)

struct InputPacket {
    uint8_t playerId;   // 0 or 1
    uint8_t type;       // INPUT_HEARTBEAT | INPUT_PLACE
    uint8_t placeX;     // target cell X (INPUT_PLACE only)
    uint8_t placeY;     // target cell Y (INPUT_PLACE only)
};

struct BuffZoneInfo {
    uint8_t x, y, type;
};

struct PlayerNetState {
    uint8_t  x, y;
    uint16_t hp;
    uint16_t maxHp;
    uint8_t  buff;
    uint8_t  score;
    uint8_t  alive;
};

struct GameSnapshot {
    uint8_t       phase;
    uint8_t       timer;           // seconds remaining in current phase
    PlayerNetState players[2];
    uint8_t       buffZoneCount;
    BuffZoneInfo  buffZones[4];
    uint8_t       roundWinner;     // 0xFF = ongoing
    uint8_t       matchWinner;     // 0xFF = ongoing
};

#pragma pack(pop)
