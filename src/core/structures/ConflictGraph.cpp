#include "core/structures/ConflictGraph.h"
#include <algorithm>

using namespace std;

namespace planner {

ConflictGraph::ConflictGraph(const vector<TeachingClass>& teachingClasses) {
    // 先给每个教学班建一个空邻接表，没有冲突的班也能查询到空列表。
    for (const auto& teachingClass : teachingClasses) {
        adjacency_[teachingClass.id()] = vector<ConflictEdge>();
    }
    // 两两比较所有教学班，O(n^2) 次冲突检测。
    // 无向边在两端各存一次，edgeCount_ 对一对冲突班只计一条边。
    for (size_t i = 0; i < teachingClasses.size(); ++i) {
        for (size_t j = i + 1; j < teachingClasses.size(); ++j) {
            addConflict(teachingClasses[i], teachingClasses[j]);
        }
    }
}

const vector<ConflictEdge>& ConflictGraph::neighbors(const string& teachingClassId) const {
    return adjacency_.at(teachingClassId);
}

void ConflictGraph::addConflict(const TeachingClass& first, const TeachingClass& second) {
    ConflictResult result = ConflictDetector::analyze(first, second);
    if (result.hasConflict()) {
        adjacency_.at(first.id()).push_back({second.id(), result});
        adjacency_.at(second.id()).push_back({first.id(), result});
        ++edgeCount_;
    }
}

void ConflictGraph::addTeachingClass(const TeachingClass& teachingClass,
                                   const vector<TeachingClass>& teachingClasses) {
    adjacency_.emplace(teachingClass.id(), vector<ConflictEdge>());
    for (const auto& other : teachingClasses) {
        if (other.id() != teachingClass.id()) { addConflict(teachingClass, other); }
    }
}

void ConflictGraph::removeTeachingClass(const string& teachingClassId) {
    const auto& edges = adjacency_.at(teachingClassId);
    // 无向边在两端都有记录，同时移除邻居中指向该班的反向边。
    for (const auto& edge : edges) {
        auto& reverse = adjacency_.at(edge.neighborId);
        reverse.erase(remove_if(reverse.begin(), reverse.end(), [&](const ConflictEdge& other) {
            return other.neighborId == teachingClassId;
        }), reverse.end());
    }
    edgeCount_ -= edges.size();
    adjacency_.erase(teachingClassId);
}

void ConflictGraph::updateTeachingClass(const string& oldId, const TeachingClass& teachingClass,
                                      const vector<TeachingClass>& teachingClasses) {
    // 改代码、名称或教师时 ID 也会变化，先清理旧 ID 的全部关联。
    removeTeachingClass(oldId);
    addTeachingClass(teachingClass, teachingClasses);
}

}
