#include "../include/tournament_tree.h"

// ═════════════════════════════════════════════════════════════════════════════
//  TournamentTree Implementation
// ═════════════════════════════════════════════════════════════════════════════

TournamentTree::TournamentTree(int maxRounds)
    : root_(nullptr), roundNodes_(nullptr), maxRounds_(maxRounds)
{
    if (maxRounds <= 0) return;

    roundNodes_ = new MatchNode*[maxRounds];
    for (int i = 0; i < maxRounds; i++) {
        roundNodes_[i] = new MatchNode(i + 1);  // round numbers are 1-based
    }

    // Build a balanced binary tree from leaf nodes
    // For simplicity with arbitrary N: build a chain-like tree
    // (more balanced trees require next-power-of-2 padding)
    root_ = roundNodes_[0];
    for (int i = 1; i < maxRounds; i++) {
        MatchNode* internal = new MatchNode(-1);  // internal node
        internal->left = root_;
        internal->right = roundNodes_[i];
        root_->parent = internal;
        roundNodes_[i]->parent = internal;
        root_ = internal;
    }
}

TournamentTree::~TournamentTree() {
    clear(root_);
    delete[] roundNodes_;
}

void TournamentTree::clear(MatchNode* node) {
    if (!node) return;
    clear(node->left);
    clear(node->right);
    delete node;
}

void TournamentTree::recordRoundResult(int roundNumber, uint8_t winner) {
    if (roundNumber < 1 || roundNumber > maxRounds_) return;
    MatchNode* node = roundNodes_[roundNumber - 1];
    node->winner = winner;

    // Update cumulative scores up the tree
    if (winner < 2) {
        node->scores[winner]++;
    }

    // Propagate scores upward
    MatchNode* curr = node->parent;
    while (curr) {
        curr->scores[0] = curr->scores[1] = 0;
        if (curr->left) {
            curr->scores[0] += curr->left->scores[0];
            curr->scores[1] += curr->left->scores[1];
        }
        if (curr->right) {
            curr->scores[0] += curr->right->scores[0];
            curr->scores[1] += curr->right->scores[1];
        }
        curr = curr->parent;
    }
}

uint8_t TournamentTree::getMatchWinner() const {
    if (!root_) return 0xFF;
    int winsNeeded = (maxRounds_ / 2) + 1;
    if (root_->scores[0] >= winsNeeded) return 0;
    if (root_->scores[1] >= winsNeeded) return 1;
    return 0xFF;
}

uint8_t TournamentTree::getPlayerWins(int playerId) const {
    if (!root_ || playerId < 0 || playerId > 1) return 0;
    return root_->scores[playerId];
}

const MatchNode* TournamentTree::getRoundNode(int roundNumber) const {
    if (roundNumber < 1 || roundNumber > maxRounds_) return nullptr;
    return roundNodes_[roundNumber - 1];
}

void TournamentTree::inOrderTraversal(const std::function<void(const MatchNode&)>& fn) const {
    // Iterative in-order traversal using parent pointers
    if (!root_) return;
    MatchNode* curr = root_;
    while (curr->left) curr = curr->left;  // go to leftmost

    while (curr) {
        fn(*curr);
        // Find next node (in-order successor)
        if (curr->right) {
            curr = curr->right;
            while (curr->left) curr = curr->left;
        } else {
            while (curr->parent && curr == curr->parent->right)
                curr = curr->parent;
            curr = curr->parent;
        }
    }
}

void TournamentTree::print() const {
    printf("TournamentTree (best-of-%d):\n", maxRounds_);
    printNode(root_, 0);
}

void TournamentTree::printNode(MatchNode* node, int depth) const {
    if (!node) return;
    printNode(node->right, depth + 1);
    for (int i = 0; i < depth; i++) printf("  ");
    if (node->roundNumber > 0)
        printf("Round %d: winner=%d  [P0:%d P1:%d]\n",
               node->roundNumber, node->winner,
               node->scores[0], node->scores[1]);
    else
        printf("Internal: [P0:%d P1:%d]\n", node->scores[0], node->scores[1]);
    printNode(node->left, depth + 1);
}
