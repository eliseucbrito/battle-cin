#pragma once
#include <cstddef>
#include <algorithm>

// ═════════════════════════════════════════════════════════════════════════════
//  PriorityQueue<T> — Max-heap based priority queue for EDOO demonstration
//  Used for: Combat turn order (higher attack speed attacks first)
//  Demonstrates: heap array, siftUp, siftDown, heapify
// ═════════════════════════════════════════════════════════════════════════════

template<typename T>
class PriorityQueue {
public:
    PriorityQueue() : data_(nullptr), size_(0), capacity_(0) {}

    ~PriorityQueue() { delete[] data_; }

    // Move semantics
    PriorityQueue(PriorityQueue&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_)
    {
        other.data_ = nullptr; other.size_ = 0; other.capacity_ = 0;
    }

    PriorityQueue& operator=(PriorityQueue&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_; size_ = other.size_; capacity_ = other.capacity_;
            other.data_ = nullptr; other.size_ = 0; other.capacity_ = 0;
        }
        return *this;
    }

    /// Insert element — O(log n)
    void push(const T& value) {
        if (size_ >= capacity_) grow();
        data_[size_] = value;
        siftUp(size_);
        ++size_;
    }

    /// Remove and return highest priority element — O(log n)
    T pop() {
        T result = data_[0];
        data_[0] = data_[size_ - 1];
        --size_;
        if (size_ > 0) siftDown(0);
        return result;
    }

    /// Peek at highest priority element — O(1)
    const T& peek() const { return data_[0]; }

    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }

    /// Build heap from array — O(n)
    void heapify(const T* arr, size_t n) {
        clear();
        if (n > capacity_) {
            delete[] data_;
            capacity_ = n;
            data_ = new T[capacity_];
        }
        for (size_t i = 0; i < n; i++) data_[i] = arr[i];
        size_ = n;
        for (int i = (int)(size_ / 2) - 1; i >= 0; i--) {
            siftDown(i);
        }
    }

    void clear() { size_ = 0; }

private:
    T*     data_;
    size_t size_;
    size_t capacity_;

    void grow() {
        size_t newCap = capacity_ == 0 ? 8 : capacity_ * 2;
        T* newData = new T[newCap];
        for (size_t i = 0; i < size_; i++) newData[i] = data_[i];
        delete[] data_;
        data_ = newData;
        capacity_ = newCap;
    }

    void siftUp(int idx) {
        while (idx > 0) {
            int parent = (idx - 1) / 2;
            if (data_[parent] > data_[idx]) break;  // parent has higher priority
            std::swap(data_[idx], data_[parent]);
            idx = parent;
        }
    }

    void siftDown(int idx) {
        while (true) {
            int left  = 2 * idx + 1;
            int right = 2 * idx + 2;
            int largest = idx;

            if (left < (int)size_ && data_[left] > data_[largest]) largest = left;
            if (right < (int)size_ && data_[right] > data_[largest]) largest = right;

            if (largest == idx) break;
            std::swap(data_[idx], data_[largest]);
            idx = largest;
        }
    }

    // Disable copy
    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;
};
