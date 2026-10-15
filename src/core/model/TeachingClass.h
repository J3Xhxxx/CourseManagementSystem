#pragma once
#include "core/model/Meeting.h"
#include <string>
#include <vector>

namespace planner {
using namespace std;

// 教学班是唯一课程实体，完整持有课程信息和全部上课安排。
class TeachingClass {
public:
    TeachingClass(string code, string name, double credits, string teacher,
                  vector<Meeting> meetings, int semesterWeeks = 20, bool isFixed = false);
    // 课程代码、名称、教师共同确定 ID，改动任一项都会改变 ID。
    const string& id() const noexcept { return id_; }
    const string& code() const noexcept { return code_; }
    const string& name() const noexcept { return name_; }
    double credits() const noexcept { return credits_; }
    bool isFixed() const noexcept { return isFixed_; }
    // 由代码和名称派生的分组键，用于同课互斥和必选设置。
    const string& courseKey() const noexcept { return courseKey_; }
    const string& teacher() const noexcept { return teacher_; }
    const vector<Meeting>& meetings() const noexcept { return meetings_; }
    const TimeBitSet& timeBitmap() const noexcept { return timeBitmap_; }
private:
    string code_, name_;
    double credits_;
    bool isFixed_;
    string courseKey_, teacher_, id_;
    vector<Meeting> meetings_;
    TimeBitSet timeBitmap_;
};
}
