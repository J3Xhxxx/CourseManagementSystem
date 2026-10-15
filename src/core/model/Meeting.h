#pragma once

#include "core/structures/TimeBitSet.h"
#include <string>
#include <vector>

namespace planner {
using namespace std;

enum class WeekPattern { EveryWeek, OddWeek, EvenWeek };

// 支持 [1-5,7-9周]、1-16周、1,3,5，区间包含两端。
// 先展开实际周次，再按单双周筛选；返回排序去重后的非空集合。
vector<int> parseWeeks(const string& expression, int semesterWeeks = 20,
                            WeekPattern pattern = WeekPattern::EveryWeek);

// 一条安排固定星期、起止大节和教室，weeks 保存真实上课周次。
// 一周多次课、不同周换教室时，由教学班持有多条 Meeting 表示。
class Meeting {
public:
    Meeting(vector<int> weeks, int weekday, int startPeriod,
            int endPeriod, string classroom);
    const vector<int>& weeks() const noexcept { return weeks_; }
    int weekday() const noexcept { return weekday_; }
    int startPeriod() const noexcept { return startPeriod_; }
    int endPeriod() const noexcept { return endPeriod_; }
    const string& classroom() const noexcept { return classroom_; }
    TimeBitSet timeBitmap(int semesterWeeks) const;
    bool operator==(const Meeting& other) const noexcept;

private:
    vector<int> weeks_;
    int weekday_;
    int startPeriod_;
    int endPeriod_;
    string classroom_;
};

}
