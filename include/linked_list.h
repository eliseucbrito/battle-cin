#pragma once
#include <cstddef>
#include <functional>

// ═════════════════════════════════════════════════════════════════════════════
//  LinkedList<T> — Generic singly-linked list for EDOO demonstration
//  Used for: ActiveEffect management (Hero buffs/debuffs with duration)
//            and Graph adjacency lists (Phase 2.2)
// ═════════════════════════════════════════════════════════════════════════════

template<typename T>
class LinkedList {
public:
    struct Node {
        T     data;
        Node* next;
        Node(const T& d, Node* n = nullptr) : data(d), next(n) {}
    };

    // ── Iterator for range-based for loops ───────────────────────────────────
    class Iterator {
        Node* current_;
    public:
        Iterator(Node* n) : current_(n) {}
        T& operator*() const { return current_->data; }
        T* operator->() const { return &current_->data; }
        Iterator& operator++() { current_ = current_->next; return *this; }
        bool operator!=(const Iterator& other) const { return current_ != other.current_; }
    };

    LinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}
    ~LinkedList() { clear(); }

    // Move constructor / move assignment (rule of 5)
    LinkedList(LinkedList&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_)
    {
        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;
    }

    LinkedList& operator=(LinkedList&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_; tail_ = other.tail_; size_ = other.size_;
            other.head_ = other.tail_ = nullptr; other.size_ = 0;
        }
        return *this;
    }

    // ── Core operations ──────────────────────────────────────────────────────

    /// Append to tail — O(1)
    void pushBack(const T& value) {
        Node* n = new Node(value);
        if (!tail_) {
            head_ = tail_ = n;
        } else {
            tail_->next = n;
            tail_ = n;
        }
        ++size_;
    }

    /// Prepend to head — O(1)
    void pushFront(const T& value) {
        Node* n = new Node(value, head_);
        head_ = n;
        if (!tail_) tail_ = n;
        ++size_;
    }

    /// Remove first node matching predicate — O(n)
    template<typename Pred>
    bool removeFirst(const Pred& pred) {
        Node* prev = nullptr;
        Node* curr = head_;
        while (curr) {
            if (pred(curr->data)) {
                if (prev) prev->next = curr->next;
                else      head_ = curr->next;
                if (curr == tail_) tail_ = prev;
                delete curr;
                --size_;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    /// Remove all nodes matching predicate — O(n)
    template<typename Pred>
    int removeAll(const Pred& pred) {
        int removed = 0;
        Node* prev = nullptr;
        Node* curr = head_;
        while (curr) {
            if (pred(curr->data)) {
                Node* toDel = curr;
                curr = curr->next;
                if (prev) prev->next = curr;
                else      head_ = curr;
                if (toDel == tail_) tail_ = prev;
                delete toDel;
                ++removed;
                --size_;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
        return removed;
    }

    /// Apply function to each node — O(n)
    void forEach(const std::function<void(T&)>& fn) {
        for (Node* curr = head_; curr; curr = curr->next)
            fn(curr->data);
    }

    void forEach(const std::function<void(const T&)>& fn) const {
        for (Node* curr = head_; curr; curr = curr->next)
            fn(curr->data);
    }

    /// Clear all nodes — O(n)
    void clear() {
        while (head_) {
            Node* n = head_;
            head_ = head_->next;
            delete n;
        }
        tail_ = nullptr;
        size_ = 0;
    }

    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }

    Iterator begin() const { return Iterator(head_); }
    Iterator end()   const { return Iterator(nullptr); }

    Node* head() const { return head_; }

private:
    Node*  head_;
    Node*  tail_;
    size_t size_;

    // Disable copy (singly-linked list deep copy is expensive; use move)
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ActiveEffect — data structure for timed buffs/debuffs on Heroes
// ═════════════════════════════════════════════════════════════════════════════

struct ActiveEffect {
    uint8_t effectType;   // BUFF_AD, BUFF_HP, BUFF_ARM, or future debuff types
    int     magnitude;    // stat change amount
    float   duration;     // seconds remaining
    float   maxDuration;  // for visual progress

    bool operator==(const ActiveEffect& other) const {
        return effectType == other.effectType && magnitude == other.magnitude;
    }
};
