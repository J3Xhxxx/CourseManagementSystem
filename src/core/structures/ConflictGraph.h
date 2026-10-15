#pragma once

#include "core/algorithms/ConflictDetector.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace planner {
using namespace std;

struct ConflictEdge {
    string neighborId;
    ConflictResult detail;
};

// 无向图，邻接表存储；用教学班 ID 作为顶点键，避免删除元素后向量下标变化导致边指错对象。
class ConflictGraph {
public:
    explicit ConflictGraph(const vector<TeachingClass>& teachingClasses = {});
    const vector<ConflictEdge>& neighbors(const string& teachingClassId) const;
    size_t edgeCount() const { return edgeCount_; }

private:
    unordered_map<string, vector<ConflictEdge>> adjacency_;
    size_t edgeCount_ = 0;
};

} // namespace planner
