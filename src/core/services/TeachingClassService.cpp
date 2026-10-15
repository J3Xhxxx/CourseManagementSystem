#include "core/services/TeachingClassService.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

using namespace std;

namespace planner {
TeachingClassService::TeachingClassService(int weeks) : semesterWeeks_(weeks), constraints_(weeks) {}
const TeachingClass* TeachingClassService::findTeachingClass(const string& id) const noexcept {
    const auto found = teachingClassIndex_.find(id);
    return found == teachingClassIndex_.end() ? nullptr : &teachingClasses_[found->second];
}
vector<string> TeachingClassService::courseKeys() const {
    set<string> seen;
    vector<string> keys;
    for (const auto& teachingClass : teachingClasses_) {
        if (seen.insert(teachingClass.courseKey()).second) { keys.push_back(teachingClass.courseKey()); }
    }
    return keys;
}
bool TeachingClassService::hasCourseKey(const string& key) const {
    return any_of(teachingClasses_.begin(), teachingClasses_.end(), [&](const auto& teachingClass) { return teachingClass.courseKey() == key; });
}
vector<string> TeachingClassService::teachingClassesForCourse(const string& key) const {
    vector<string> ids;
    for (const auto& teachingClass : teachingClasses_) { if (teachingClass.courseKey() == key) { ids.push_back(teachingClass.id()); } }
    return ids;
}
const TeachingClass* TeachingClassService::fixedTeachingClassForCourse(const string& key) const {
    for (const auto& teachingClass : teachingClasses_) {
        if (teachingClass.isFixed() && teachingClass.courseKey() == key) { return &teachingClass; }
    }
    return nullptr;
}
Schedule TeachingClassService::fixedSchedule() const {
    Schedule schedule(semesterWeeks_);
    for (const auto& teachingClass : teachingClasses_) {
        if (!teachingClass.isFixed()) { continue; }
        schedule.selectedTeachingClassIds.push_back(teachingClass.id());
        schedule.occupiedTimeBitmap.unite(teachingClass.timeBitmap());
    }
    return schedule;
}
void TeachingClassService::reconcile(const vector<TeachingClass>& teachingClasses, UserConstraints& constraints) const {
    set<string> keys, ids;
    // 新课程默认必选，已有课程保留用户选择；删除最后一个班时清理对应设置。
    for (const auto& teachingClass : teachingClasses) {
        keys.insert(teachingClass.courseKey()); ids.insert(teachingClass.id());
        if (teachingClass.isFixed() || !hasCourseKey(teachingClass.courseKey())) { constraints.requiredCourseKeys.insert(teachingClass.courseKey()); }
    }
    for (auto it = constraints.requiredCourseKeys.begin(); it != constraints.requiredCourseKeys.end();) {
        if (!keys.count(*it)) { it = constraints.requiredCourseKeys.erase(it); } else { ++it; }
    }
    for (auto it = constraints.lockedTeachingClassIds.begin(); it != constraints.lockedTeachingClassIds.end();) {
        if (!ids.count(*it)) { it = constraints.lockedTeachingClassIds.erase(it); } else { ++it; }
    }
}
void TeachingClassService::replaceTeachingClasses(vector<TeachingClass> teachingClasses) {
    // 批量导入也必须保留已有固定教学班，不能绕过界面的编辑限制。
    for (const auto& fixed : teachingClasses_) {
        if (!fixed.isFixed()) { continue; }
        const auto found = find_if(teachingClasses.begin(), teachingClasses.end(), [&](const auto& value) { return value.id() == fixed.id(); });
        if (found == teachingClasses.end() || !found->isFixed() || found->credits() != fixed.credits() || found->meetings() != fixed.meetings()) {
            throw invalid_argument("学校固定课程不能修改或删除");
        }
    }
    auto constraints = constraints_;
    reconcile(teachingClasses, constraints);
    ConflictGraph graph(teachingClasses);
    commitData(move(teachingClasses), move(constraints), move(graph));
}
void TeachingClassService::commitData(vector<TeachingClass> teachingClasses, UserConstraints constraints, ConflictGraph graph) {
    // 重建 ID 索引并接收已维护的图，校验通过后再交换到正式项目。
    TeachingClassService candidate(semesterWeeks_);
    set<string> fixedCourses;
    for (size_t i = 0; i < teachingClasses.size(); ++i) {
        if (!candidate.teachingClassIndex_.emplace(teachingClasses[i].id(), i).second) {
            throw invalid_argument("该课程下已有同名教师的教学班");
        }
        if (teachingClasses[i].isFixed() && !fixedCourses.insert(teachingClasses[i].courseKey()).second) {
            throw invalid_argument("同一课程不能有两个学校固定教学班");
        }
    }
    candidate.graph_ = move(graph);
    candidate.teachingClasses_ = move(teachingClasses);
    constraints.validate(candidate);
    candidate.constraints_ = move(constraints);
    teachingClasses_.swap(candidate.teachingClasses_);
    teachingClassIndex_.swap(candidate.teachingClassIndex_);
    swap(graph_, candidate.graph_);
    swap(constraints_, candidate.constraints_);
}
void TeachingClassService::setConstraints(UserConstraints constraints) {
    constraints.validate(*this);
    swap(constraints_, constraints);
}
void TeachingClassService::addTeachingClass(TeachingClass teachingClass) {
    auto teachingClasses = teachingClasses_;
    teachingClasses.push_back(move(teachingClass));
    auto constraints = constraints_;
    reconcile(teachingClasses, constraints);
    auto graph = graph_;
    graph.addTeachingClass(teachingClasses.back(), teachingClasses_);
    commitData(move(teachingClasses), move(constraints), move(graph));
}
void TeachingClassService::updateTeachingClass(const string& oldId, TeachingClass replacement) {
    const auto index = teachingClassIndex_.at(oldId);
    if (teachingClasses_[index].isFixed()) { throw invalid_argument("学校固定课程不能修改或删除"); }
    const auto oldKey = teachingClasses_[index].courseKey();
    const auto newKey = replacement.courseKey();
    const bool required = constraints_.requiredCourseKeys.count(oldKey) != 0;
    const bool newGroup = !hasCourseKey(newKey);
    auto teachingClasses = teachingClasses_;
    auto constraints = constraints_;
    // 改名后锁定应跟随新 ID，不能留下指向旧教学班的设置。
    if (constraints.lockedTeachingClassIds.erase(oldId)) {
        constraints.lockedTeachingClassIds.insert(replacement.id());
        constraints.requiredCourseKeys.insert(newKey);
    }
    teachingClasses[index] = move(replacement);
    reconcile(teachingClasses, constraints);
    // 未勾选的课程改名为新分组时，仍保持未勾选。
    if (newGroup && !required && !teachingClasses[index].isFixed()) { constraints.requiredCourseKeys.erase(newKey); }
    auto graph = graph_;
    graph.updateTeachingClass(oldId, teachingClasses[index], teachingClasses);
    commitData(move(teachingClasses), move(constraints), move(graph));
}
void TeachingClassService::removeTeachingClass(const string& id) {
    const auto index = teachingClassIndex_.at(id);
    if (teachingClasses_[index].isFixed()) { throw invalid_argument("学校固定课程不能修改或删除"); }
    auto teachingClasses = teachingClasses_;
    teachingClasses.erase(teachingClasses.begin() + static_cast<ptrdiff_t>(index));
    auto constraints = constraints_;
    reconcile(teachingClasses, constraints);
    auto graph = graph_;
    graph.removeTeachingClass(id);
    commitData(move(teachingClasses), move(constraints), move(graph));
}
}
