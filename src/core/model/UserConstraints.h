#pragma once

#include "core/structures/TimeBitSet.h"
#include <set>
#include <string>

namespace planner {
using namespace std;

class TeachingClassService;

// requiredCourseKeys 明确指定参与生成的课程，每门选一个班，固定课程必须在其中。
// 锁定只允许指定的班入选。时间冲突可作为合法设置保存，由生成器报告无解原因。
class UserConstraints {
public:
    explicit UserConstraints(int semesterWeeks = 20);

    set<string> requiredCourseKeys;
    set<string> lockedTeachingClassIds;
    TimeBitSet forbiddenTimes;

    void validate(const TeachingClassService& project) const;
    bool operator==(const UserConstraints& other) const noexcept;
};

}
