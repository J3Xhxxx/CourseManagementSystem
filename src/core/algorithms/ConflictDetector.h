#pragma once

#include "core/model/TeachingClass.h"
#include <vector>

namespace planner {
using namespace std;

enum class ConflictReason { TimeOverlap, SameCourseExclusive };

struct ConflictResult {
    vector<ConflictReason> reasons;
    vector<TimeSlot> overlapSlots;
    bool hasConflict() const { return !reasons.empty(); }
};

class ConflictDetector {
public:
    // 同时返回时间重叠和同课互斥，两种原因可能同时存在。
    // 位图求交为 O(ceil(周数 * 42 / 64))，展开具体重叠时段另需 O(周数 * 42)。
    static ConflictResult analyze(const TeachingClass& first, const TeachingClass& second);
};

} // namespace planner
