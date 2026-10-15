#pragma once
#include "core/structures/TimeBitSet.h"
#include <string>

namespace planner {
using namespace std;

// 方案保存所选班的 ID 和时间并集；有效性由校验器根据当前项目重新判断。
struct Schedule {
    explicit Schedule(int semesterWeeks = 20) : occupiedTimeBitmap(semesterWeeks) {}
    vector<string> selectedTeachingClassIds;
    TimeBitSet occupiedTimeBitmap;
};
}
