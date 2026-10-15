#include "core/algorithms/ConflictDetector.h"

namespace planner {

ConflictResult ConflictDetector::analyze(const TeachingClass& first, const TeachingClass& second) {
    ConflictResult result;
    // 两个班的时间位图求交，非空说明有时间重叠。
    TimeBitSet overlap = first.timeBitmap().intersection(second.timeBitmap());
    if (!overlap.empty()) {
        result.reasons.push_back(ConflictReason::TimeOverlap);
        result.overlapSlots = overlap.occupiedSlots();
    }
    // 同一门课的不同教学班只能选一个，即使时间不重叠也互斥。
    if (first.courseKey() == second.courseKey()) {
        result.reasons.push_back(ConflictReason::SameCourseExclusive);
    }
    return result;
}

} // namespace planner
