#pragma once
#include "core/structures/ConflictGraph.h"
#include "core/model/UserConstraints.h"
#include "core/model/Schedule.h"
#include <unordered_map>
namespace planner {
using namespace std;

// 统一管理教学班、ID 索引、冲突图和约束，成功提交后才替换项目数据。
// 增删改会使旧引用失效，界面和搜索任务应保存 ID 或数据副本。
class TeachingClassService {
public:
    explicit TeachingClassService(int semesterWeeks = 20);
    int semesterWeeks() const noexcept { return semesterWeeks_; }
    const vector<TeachingClass>& teachingClasses() const noexcept { return teachingClasses_; }
    const ConflictGraph& graph() const noexcept { return graph_; }
    const UserConstraints& constraints() const noexcept { return constraints_; }
    const TeachingClass* findTeachingClass(const string& id) const noexcept;
    vector<string> courseKeys() const;
    bool hasCourseKey(const string& key) const;
    vector<string> teachingClassesForCourse(const string& key) const;
    const TeachingClass* fixedTeachingClassForCourse(const string& key) const;
    Schedule fixedSchedule() const;
    void addTeachingClass(TeachingClass teachingClass);
    void updateTeachingClass(const string& oldId, TeachingClass replacement);
    void removeTeachingClass(const string& id);
    void setConstraints(UserConstraints constraints);
    void replaceTeachingClasses(vector<TeachingClass> teachingClasses);
private:
    void commitData(vector<TeachingClass> teachingClasses, UserConstraints constraints);
    void reconcile(const vector<TeachingClass>& teachingClasses, UserConstraints& constraints) const;
    int semesterWeeks_;
    vector<TeachingClass> teachingClasses_;
    unordered_map<string, size_t> teachingClassIndex_;
    ConflictGraph graph_;
    UserConstraints constraints_;
};
}
