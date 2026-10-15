#include "core/model/UserConstraints.h"
#include "core/services/TeachingClassService.h"
#include <stdexcept>

using namespace std;

namespace planner {

UserConstraints::UserConstraints(int semesterWeeks) : forbiddenTimes(semesterWeeks) {}

void UserConstraints::validate(const TeachingClassService& project) const {
    for (const auto& courseKey : requiredCourseKeys) {
        if (!project.hasCourseKey(courseKey)) {
            throw invalid_argument("必选课程不存在");
        }
    }
    for (const auto& teachingClass : project.teachingClasses()) {
        if (teachingClass.isFixed() && !requiredCourseKeys.count(teachingClass.courseKey())) {
            throw invalid_argument("学校固定课程不能取消必选");
        }
    }
    set<string> lockedCourseKeys;
    for (const auto& teachingClassId : lockedTeachingClassIds) {
        const auto* teachingClass = project.findTeachingClass(teachingClassId);
        if (!teachingClass) { throw invalid_argument("锁定的教学班不存在"); }
        const auto* fixed = project.fixedTeachingClassForCourse(teachingClass->courseKey());
        if (fixed && fixed->id() != teachingClassId) {
            throw invalid_argument("学校固定课程不能选择其他教学班");
        }
        if (!requiredCourseKeys.count(teachingClass->courseKey())) {
            throw invalid_argument("锁定教学班所属课程必须设为必选");
        }
        if (!lockedCourseKeys.insert(teachingClass->courseKey()).second) {
            throw invalid_argument("同一课程只能锁定一个教学班");
        }
    }
}

bool UserConstraints::operator==(const UserConstraints& other) const noexcept {
    return requiredCourseKeys == other.requiredCourseKeys
        && lockedTeachingClassIds == other.lockedTeachingClassIds
        && forbiddenTimes == other.forbiddenTimes;
}

}
