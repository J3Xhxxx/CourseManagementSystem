#pragma once
#include "core/model/Schedule.h"
#include "core/services/TeachingClassService.h"

namespace planner {
using namespace std;

class BruteForceGenerator {
public:
    // 小规模正确性基线：完整枚举笛卡尔积，不使用剪枝，供结果对照。
    // 组合数超过上限时直接报错，不用于大规模交互搜索。
    static vector<Schedule> generate(const TeachingClassService& project,
        size_t maxCombinations = 100000);
};
}
