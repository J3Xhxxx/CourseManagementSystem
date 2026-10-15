#pragma once
#include "core/model/Schedule.h"
#include "core/services/TeachingClassService.h"

namespace planner {
using namespace std;

enum class ValidationCode {
    DuplicateCourse, MissingCourse, UnexpectedCourse, MissingLockedTeachingClass,
    MissingFixedTeachingClass, ForbiddenTime, TimeOverlap
};
struct ValidationIssue {
    ValidationCode code;
    string subjectId;
    string otherId;
    vector<TimeSlot> overlapTimes;
};
struct ValidationResult {
    vector<ValidationIssue> issues;
    bool valid() const noexcept { return issues.empty(); }
};
class ScheduleValidator {
public:
    // 检查每门必选课程恰选一个班、固定班和锁定班均在场，且不占用禁止时间。
    // 直接展开 Meeting 坐标，不使用冲突图和教学班位图，供暴力枚举基线逐个检查组合。
    static ValidationResult validate(const TeachingClassService& project, const Schedule& schedule);
};
}
