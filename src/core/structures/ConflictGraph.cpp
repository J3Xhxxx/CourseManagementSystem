#include "core/structures/ConflictGraph.h"

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
            ConflictResult result = ConflictDetector::analyze(teachingClasses[i], teachingClasses[j]);
            if (result.hasConflict()) {
                adjacency_[teachingClasses[i].id()].push_back({teachingClasses[j].id(), result});
                adjacency_[teachingClasses[j].id()].push_back({teachingClasses[i].id(), result});
                ++edgeCount_;
            }
        }
    }
}

const vector<ConflictEdge>& ConflictGraph::neighbors(const string& teachingClassId) const {
    return adjacency_.at(teachingClassId);
}

}
