#include "core/algorithms/ConflictDetector.h"

#include <array>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

using namespace planner;

namespace {
void show(const TeachingClass& first, const TeachingClass& second) {
    constexpr array<const char*, 7> days{"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    const auto result = ConflictDetector::analyze(first, second);
    cout << first.name() << " / " << first.teacher() << " 与 "
              << second.name() << " / " << second.teacher() << ": "
              << (result.hasConflict() ? "不能同时选择" : "可以同时选择") << '\n';
    for (auto reason : result.reasons) {
        cout << "  原因：" << (reason == ConflictReason::TimeOverlap ? "上课时间重叠" : "同一课程的教学班互斥") << '\n';
    }
    for (const auto& slot : result.overlapSlots) {
        cout << "  第" << slot.week << "周 " << days.at(static_cast<size_t>(slot.weekday - 1))
                  << " 第" << slot.period << "节（" << PERIOD_TIMES[slot.period - 1] << "）\n";
    }
    cout << '\n';
}
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const TeachingClass osZhang("CS101", "操作系统", 3.5, "张老师", {
        Meeting(parseWeeks("[1-5,7-9周]"), 1, 1, 1, "H415"),
        Meeting(parseWeeks("[1-5,7-9周]"), 4, 3, 3, "H416")
    });
    const TeachingClass osLi("CS101", "操作系统", 3.5, "李老师", {Meeting(parseWeeks("1-16周"), 2, 2, 2, "H205")});
    const TeachingClass dbWang("CS102", "数据库", 3.0, "王老师", {Meeting(parseWeeks("[5-7周]"), 1, 1, 1, "H301")});
    const TeachingClass peChen("PE101", "体育", 1.0, "陈老师", {Meeting(parseWeeks("1-20周", 20, WeekPattern::EvenWeek), 7, 6, 6, "体育馆")});

    cout << "个人选课系统 v0.9 — 核心模型与冲突检测演示\n"
              << "默认学期：20周；每天6个固定时段；支持一周多次上课。\n\n";
    show(osZhang, dbWang);
    show(osZhang, osLi);
    show(osZhang, peChen);
    return 0;
}
