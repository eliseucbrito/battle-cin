#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>

// ═════════════════════════════════════════════════════════════════════════════
//  TournamentTree — Binary tree tracking round results (best-of-N)
//  Used for: Match history and bracket visualization
//  Demonstrates: binary tree, traversal (in-order), node allocation
// ═════════════════════════════════════════════════════════════════════════════

struct MatchNode {
    int       roundNumber;    // 1-based round index (-1 for internal nodes)
    uint8_t   winner;         // 0, 1, or 0xFF (not played yet)
    uint8_t   scores[2];      // [p0_wins, p1_wins] at this point
    MatchNode* left;          // previous rounds / subtree
    MatchNode* right;         // next rounds / subtree
    MatchNode* parent;

    MatchNode(int rn = -1, uint8_t w = 0xFF)
        : roundNumber(rn), winner(w), left(nullptr), right(nullptr), parent(nullptr)
    {
        scores[0] = scores[1] = 0;
    }
};

class TournamentTree {
public:
    TournamentTree(int maxRounds);
    ~TournamentTree();

    /// Record result of a round (0 = p0 wins, 1 = p1 wins, 0xFF = draw)
    void recordRoundResult(int roundNumber, uint8_t winner);

    /// Get current match winner, or 0xFF if match ongoing
    uint8_t getMatchWinner() const;

    /// Get wins for a player
    uint8_t getPlayerWins(int playerId) const;

    /// Get the leaf node for a specific round
    const MatchNode* getRoundNode(int roundNumber) const;

    /// In-order traversal: calls fn for each node
    void inOrderTraversal(const std::function<void(const MatchNode&)>& fn) const;

    /// Print tree to console (for debugging)
    void print() const;

    int maxRounds() const { return maxRounds_; }

private:
    MatchNode* root_;
    MatchNode** roundNodes_;  // array of leaf pointers, indexed by roundNumber-1
    int maxRounds_;

    void clear(MatchNode* node);
    void printNode(MatchNode* node, int depth) const;
};
