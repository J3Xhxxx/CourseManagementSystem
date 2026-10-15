#include "core/structures/TimeBitSet.h"

#include <stdexcept>

using namespace std;

namespace planner {

// 周次、星期、节次都相同，才是同一个上课时段。
bool TimeSlot::operator==(const TimeSlot& other) const {
    return week == other.week && weekday == other.weekday && period == other.period;
}

// 一天 6 个大节、一周 7 天，共 42 个时段。20 周就是 840 位。
TimeBitSet::TimeBitSet(int semesterWeeks) : semesterWeeks_(semesterWeeks) {
    bitCount_ = static_cast<size_t>(semesterWeeks * daysPerWeek * periodsPerDay);
    // 每 64 位放进一个 uint64_t，字数向上取整。尾部补齐位始终为 0，不算有效时间。
    words_.resize((bitCount_ + 63) / 64, 0);
}

// 第 1 周周一第 1 节是第 0 位。公式：(周次-1)*42 + (星期-1)*6 + (节次-1)。
size_t TimeBitSet::index(TimeSlot slot) const {
    if (slot.week < 1 || slot.week > semesterWeeks_ || slot.weekday < 1 ||
        slot.weekday > daysPerWeek || slot.period < 1 || slot.period > periodsPerDay) {
        throw out_of_range("上课时间超出学期或节次范围");
    }
    return static_cast<size_t>((slot.week - 1) * daysPerWeek * periodsPerDay
                               + (slot.weekday - 1) * periodsPerDay + (slot.period - 1));
}

// index 的逆运算：整除 42 得到周次，余数再拆成星期和节次。结果都从 1 开始。
TimeSlot TimeBitSet::decode(size_t position) const {
    const int value = static_cast<int>(position);
    const int slotsPerWeek = daysPerWeek * periodsPerDay;
    return {value / slotsPerWeek + 1, value % slotsPerWeek / periodsPerDay + 1, value % periodsPerDay + 1};
}

// position/64 找到所在的字，position%64 是字内的位。按位或把这一位置成 1。
void TimeBitSet::set(TimeSlot slot) {
    const size_t position = index(slot);
    words_[position / 64] |= 1ULL << (position % 64);
}

// 把目标位移到最低位，再和 1 相与，判断这一位是不是 1。
bool TimeBitSet::contains(TimeSlot slot) const {
    const size_t position = index(slot);
    return (words_[position / 64] >> (position % 64) & 1ULL) != 0;
}

// 任何一个字非 0，就说明至少有一个时段被占用。
bool TimeBitSet::empty() const {
    for (uint64_t word : words_) {
        if (word != 0) { return false; }
    }
    return true;
}

// 统计占用了多少个时段。word &= word-1 每次去掉最低的那个 1，只按占用次数循环。
size_t TimeBitSet::count() const {
    size_t result = 0;
    for (uint64_t word : words_) {
        while (word != 0) {
            word &= word - 1;
            ++result;
        }
    }
    return result;
}

// 以下运算一次处理 64 个时段。调用时两边的学期周数必须相同，否则会按本集合的字数越界。
bool TimeBitSet::intersects(const TimeBitSet& other) const {
    for (size_t i = 0; i < words_.size(); ++i) {
        // 按位与后还有 1，说明这个字里至少有一个时段两边都占用。
        if ((words_[i] & other.words_[i]) != 0) {
            return true;
        }
    }
    return false;
}

// 新集合里，只有两边都是 1 的位才保留。
TimeBitSet TimeBitSet::intersection(const TimeBitSet& other) const {
    TimeBitSet result(semesterWeeks_);
    for (size_t i = 0; i < words_.size(); ++i) {
        result.words_[i] = words_[i] & other.words_[i];
    }
    return result;
}

// 把对方占用的时段并进自己。
void TimeBitSet::unite(const TimeBitSet& other) {
    for (size_t i = 0; i < words_.size(); ++i) {
        words_[i] |= other.words_[i];
    }
}

// 从自己这里清掉对方占用的位。先把对方的 0/1 对调，再按位与。
void TimeBitSet::subtract(const TimeBitSet& other) {
    for (size_t i = 0; i < words_.size(); ++i) {
        words_[i] &= ~other.words_[i];
    }
}

// 只扫有效位数，跳过尾部补齐位；为 1 的位置再 decode 成周次、星期、节次。
vector<TimeSlot> TimeBitSet::occupiedSlots() const {
    vector<TimeSlot> result;
    for (size_t i = 0; i < bitCount_; ++i) {
        if ((words_[i / 64] >> (i % 64) & 1ULL) != 0) {
            result.push_back(decode(i));
        }
    }
    return result;
}

// 学期周数相同，并且每个字都相同，两个集合才相等。
bool TimeBitSet::operator==(const TimeBitSet& other) const {
    return semesterWeeks_ == other.semesterWeeks_ && words_ == other.words_;
}

}