#pragma once
#include <cstddef>
#include <utility>
#include <vector>

namespace planner {
using namespace std;

// 固定容量的二叉堆，只保留调用方认为“最好”的前 K 个元素。
// 比较器 heapAbove(a, b) 表示 a 在堆中应排在 b 上面（即 a 比 b 更差）：
// 这样堆顶就是“已保留元素里最差的一个”，装满后只要新元素优于堆顶就替换它，
// 不需要把所有候选同时留在内存里，也不需要维护一份完整结果集。
// 例如要比较“数值越小越优”，就传 heapAbove(a, b) = a > b。
template <typename T, typename HeapAbove>
class BoundedMaxHeap {
public:
    using value_type = T;
    // capacity 为 0 时按 1 处理，避免出现放不进任何元素的堆。
    BoundedMaxHeap(size_t capacity, HeapAbove heapAbove)
        : capacity_(capacity ? capacity : 1), heapAbove_(move(heapAbove)) { heap_.reserve(capacity_); }

    // 堆未满直接插入；已满时新元素优于堆顶（当前最差的一个）才替换。
    // 返回 true 表示新元素最终留在堆中。
    bool offer(T value) {
        if (heap_.size() < capacity_) {
            heap_.push_back(move(value));
            bubbleUp(heap_.size() - 1);
            return true;
        }
        if (!better_(value, heap_.front())) { return false; }
        heap_.front() = move(value);
        sinkDown(0);
        return true;
    }

    // 把堆内元素按从优到差取出。堆顶是最差元素，逐个取出得到从差到优的序列，
    // 因此反向填充；取出后堆为空，不能再使用。
    vector<T> drainSorted() {
        vector<T> result(heap_.size());
        for (size_t index = result.size(); index > 0; --index) {
            result[index - 1] = move(heap_.front());
            heap_.front() = move(heap_.back());
            heap_.pop_back();
            if (!heap_.empty()) { sinkDown(0); }
        }
        return result;
    }

    const T& worstKept() const { return heap_.front(); }
    bool empty() const noexcept { return heap_.empty(); }
    bool full() const noexcept { return heap_.size() >= capacity_; }
    size_t size() const noexcept { return heap_.size(); }
    size_t capacity() const noexcept { return capacity_; }

private:
    // a 是否优于 b：“a 比 b 更差”的取反。
    bool better_(const T& left, const T& right) const { return heapAbove_(right, left); }

    // 下标从 0 开始：父结点 (i-1)/2，左孩子 2i+1，右孩子 2i+2。
    // 性质：父结点不比子结点更优，因此堆顶是整堆最差元素。
    void bubbleUp(size_t index) {
        while (index > 0) {
            const size_t parent = (index - 1) / 2;
            if (!heapAbove_(heap_[index], heap_[parent])) { break; }
            swap(heap_[index], heap_[parent]);
            index = parent;
        }
    }
    void sinkDown(size_t index) {
        while (true) {
            const size_t left = index * 2 + 1;
            if (left >= heap_.size()) { return; }
            size_t above = left;
            const size_t right = left + 1;
            if (right < heap_.size() && heapAbove_(heap_[right], heap_[left])) { above = right; }
            if (!heapAbove_(heap_[above], heap_[index])) { return; }
            swap(heap_[index], heap_[above]);
            index = above;
        }
    }

    vector<T> heap_;
    size_t capacity_;
    HeapAbove heapAbove_;
};
}
