#pragma once
#include "raylib.h"
#include "../include/protocol.h"

// Grid constants (shared with main.cpp)
extern const float GX, GY, GW, GH, CELLW, CELLH;

void drawGrid();
void drawBuffZones(const GameSnapshot& snap);
void drawHero(const HeroNetState& hs, Vector2 pos, int myId, bool dragging);
void drawHUD(const GameSnapshot& snap, int myId);
void drawOverlays(const GameSnapshot& snap, int myId);

Vector2 cellCenter(int cx, int cy);
Rectangle cellRect(int cx, int cy);
