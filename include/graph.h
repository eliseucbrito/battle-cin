#pragma once
#include "linked_list.h"
#include "protocol.h"
#include <cstdint>
#include <cstring>

// ═════════════════════════════════════════════════════════════════════════════
//  Graph — 8x8 grid adjacency graph with BFS shortest path
//  Used for: Hero pathfinding avoiding obstacles (other heroes)
//  Demonstrates: adjacency list (LinkedList), BFS algorithm, queue
// ═════════════════════════════════════════════════════════════════════════════

struct GridCell {
    int x, y;
    GridCell() : x(0), y(0) {}
    GridCell(int xx, int yy) : x(xx), y(yy) {}
    bool operator==(const GridCell& other) const {
        return x == other.x && y == other.y;
    }
};

class Graph {
public:
    static constexpr int MAX_NODES = GRID_COLS * GRID_ROWS;
    static constexpr int MAX_NEIGHBORS = 8;  // 8 directions

    Graph() {
        buildAdjacency();
    }

    /// Rebuild adjacency list (call when grid dimensions change)
    void buildAdjacency() {
        for (int y = 0; y < GRID_ROWS; y++) {
            for (int x = 0; x < GRID_COLS; x++) {
                int idx = nodeIndex(x, y);
                adj_[idx].clear();
                // 8 directions
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < GRID_COLS && ny >= 0 && ny < GRID_ROWS) {
                            adj_[idx].pushBack(GridCell(nx, ny));
                        }
                    }
                }
            }
        }
    }

    /// BFS shortest path from (sx,sy) to (tx,ty), avoiding blocked cells.
    /// blocked[y][x] = true if cell is occupied.
    /// Returns: true if path found, nextStep set to the first cell to move to.
    bool findPath(int sx, int sy, int tx, int ty,
                  const bool blocked[GRID_ROWS][GRID_COLS],
                  int& outNextX, int& outNextY) const
    {
        if (sx == tx && sy == ty) return false;

        bool visited[GRID_ROWS][GRID_COLS];
        std::memset(visited, 0, sizeof(visited));

        int parentX[GRID_ROWS][GRID_COLS];
        int parentY[GRID_ROWS][GRID_COLS];
        std::memset(parentX, -1, sizeof(parentX));
        std::memset(parentY, -1, sizeof(parentY));

        // BFS queue (circular buffer for efficiency)
        GridCell queue[MAX_NODES];
        int qHead = 0, qTail = 0;

        queue[qTail++] = GridCell(sx, sy);
        visited[sy][sx] = true;

        while (qHead < qTail) {
            GridCell curr = queue[qHead++];
            int idx = nodeIndex(curr.x, curr.y);

            for (const GridCell& neighbor : adj_[idx]) {
                if (visited[neighbor.y][neighbor.x]) continue;
                if (blocked[neighbor.y][neighbor.x]) continue;

                visited[neighbor.y][neighbor.x] = true;
                parentX[neighbor.y][neighbor.x] = curr.x;
                parentY[neighbor.y][neighbor.x] = curr.y;
                queue[qTail++] = neighbor;

                if (neighbor.x == tx && neighbor.y == ty) {
                    // Reconstruct first step
                    int cx = tx, cy = ty;
                    while (parentX[cy][cx] != sx || parentY[cy][cx] != sy) {
                        int px = parentX[cy][cx];
                        int py = parentY[cy][cx];
                        cx = px; cy = py;
                    }
                    outNextX = cx;
                    outNextY = cy;
                    return true;
                }
            }
        }
        return false;  // no path found
    }

    static int nodeIndex(int x, int y) {
        return y * GRID_COLS + x;
    }

private:
    // Adjacency list: one LinkedList per grid cell
    mutable LinkedList<GridCell> adj_[MAX_NODES];
};
