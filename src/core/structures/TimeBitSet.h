#pragma once

#include <cstdint>
#include <vector>

namespace planner {
using namespace std;

struct TimeSlot {
    int week;    // 从第 1 周开始。
    int weekday; // 1 表示周一，7 表示周日。
    int period;  // 1～6 表示下列六个完整大节，不是学校小节编号。
    bool operator==(const TimeSlot& other) const;
};

// 六个大节的上课时间，下标 0 对应第 1 大节。
const char* const PERIOD_TIMES[6] = {
    "08:00-09:45", "10:05-11:50", "14:00-15:45",
    "16:05-17:50", "18:40-20:25", "20:45-22:30"
};

// 全学期的时间集合，每一位对应一个 (周次,星期,大节)。
// 集合运算要求两边学期周数相同；同一项目内的位图都按项目周数创建。
class TimeBitSet {
public:
    static const int daysPerWeek = 7;
    static const int periodsPerDay = 6;
    explicit TimeBitSet(int semesterWeeks = 20);

    int semesterWeeks() const { return semesterWeeks_; }
    void set(TimeSlot slot);
    bool contains(TimeSlot slot) const;
    bool empty() const;
    size_t count() const;
    bool intersects(const TimeBitSet& other) const;
    TimeBitSet intersection(const TimeBitSet& other) const;
    void unite(const TimeBitSet& other);
    void subtract(const TimeBitSet& other);
    vector<TimeSlot> occupiedSlots() const;
    bool operator==(const TimeBitSet& other) const;

private:
    // 时间点与位下标互相转换：index = (week-1)*42 + (weekday-1)*6 + (period-1)。
    size_t index(TimeSlot slot) const;
    TimeSlot decode(size_t index) const;

    int semesterWeeks_;
    size_t bitCount_;
    vector<uint64_t> words_; // 每个 uint64_t 存 64 个时间点。
};

} // namespace planner
