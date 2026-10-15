#include "core/algorithms/ScheduleValidator.h"
#include <map>
#include <set>
#include <tuple>

using namespace std;

namespace planner {
ValidationResult ScheduleValidator::validate(const TeachingClassService& project, const Schedule& schedule) {
    ValidationResult result;
    set<string> teachingClasses, courses;
    // 记录每个真实时段的占用者，按教学班对汇总所有重叠周次。
    map<tuple<int,int,int>, string> owner;
    map<pair<string,string>, vector<TimeSlot>> overlaps;
    const auto& constraints = project.constraints();
    for (const auto& id : schedule.selectedTeachingClassIds) {
        const auto* teachingClass = project.findTeachingClass(id);
        teachingClasses.insert(id);
        if (!courses.insert(teachingClass->courseKey()).second) {
            result.issues.push_back({ValidationCode::DuplicateCourse, teachingClass->courseKey(), {}, {}});
        }
        if (!constraints.requiredCourseKeys.count(teachingClass->courseKey())) {
            result.issues.push_back({ValidationCode::UnexpectedCourse, teachingClass->courseKey(), id, {}});
        }
        vector<TimeSlot> forbidden;
        for (const auto& meeting : teachingClass->meetings()) {
            for (const int week : meeting.weeks()) {
                for (int period = meeting.startPeriod(); period <= meeting.endPeriod(); ++period) {
                    const TimeSlot time{week, meeting.weekday(), period};
                    const auto entry = owner.emplace(make_tuple(week, meeting.weekday(), period), id);
                    if (!entry.second) { overlaps[{entry.first->second, id}].push_back(time); }
                    if (constraints.forbiddenTimes.contains(time)) { forbidden.push_back(time); }
                }
            }
        }
        if (!forbidden.empty()) {
            result.issues.push_back({ValidationCode::ForbiddenTime, id, {}, move(forbidden)});
        }
    }
    for (const auto& id : constraints.requiredCourseKeys) {
        if (!courses.count(id)) {
            result.issues.push_back({ValidationCode::MissingCourse, id, {}, {}});
        }
    }
    for (const auto& id : constraints.lockedTeachingClassIds) {
        if (!teachingClasses.count(id)) {
            result.issues.push_back({ValidationCode::MissingLockedTeachingClass, id, {}, {}});
        }
    }
    for (const auto& teachingClass : project.teachingClasses()) {
        if (teachingClass.isFixed() && !teachingClasses.count(teachingClass.id())) {
            result.issues.push_back({ValidationCode::MissingFixedTeachingClass, teachingClass.id(), {}, {}});
        }
    }
    for (auto& entry : overlaps) {
        result.issues.push_back({ValidationCode::TimeOverlap, entry.first.first,
            entry.first.second, move(entry.second)});
    }
    return result;
}
}
