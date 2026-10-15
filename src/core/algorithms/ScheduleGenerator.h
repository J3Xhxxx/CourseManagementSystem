#pragma once
#include "core/model/Schedule.h"
#include "core/services/TeachingClassService.h"
#include <functional>

namespace planner {
using namespace std;

// 搜索结束的原因：全部搜完、方案数到上限、尝试次数到上限、用户取消。
enum class SearchStatus { Complete, SolutionLimit, NodeLimit, Cancelled };

// 三种搜索策略：普通回溯、MRV、MRV + 前向检查。前两种作为对照基线。
enum class SearchStrategy { Backtracking, Mrv, MrvForwardChecking };

struct SearchOptions {
    size_t maxSolutions = 100;  // 最多保留多少个方案（Top-K 的 K）
    uint64_t maxNodes = 100000; // 最多尝试选择多少次教学班
    // 返回 true 表示用户要求取消；在搜索线程中调用。
    function<bool()> cancelled;
    SearchStrategy strategy = SearchStrategy::MrvForwardChecking;
};

struct SearchStatistics {
    uint64_t visitedNodes = 0;      // 每尝试选择一个教学班计一次
    uint64_t prunedBranches = 0;    // 被剪掉的分支数
    uint64_t completeLeaves = 0;    // 搜索到的完整组合数，截断时比保存数多 1
    uint64_t candidateRemovals = 0; // 前向检查删除候选的累计次数
    uint64_t domainWipeouts = 0;    // 前向检查导致某门课没有候选的次数
    double elapsedMilliseconds = 0;
};

// 固定课程、锁定教学班、禁止时间之间的直接冲突，搜索前就能判定无解。
enum class ConstraintConflictReason {
    LockedTeachingClassesOverlap, LockedTeachingClassForbidden,
    FixedTeachingClassesOverlap, FixedTeachingClassForbidden, FixedAndLockedOverlap
};
struct ConstraintConflict {
    ConstraintConflictReason reason;
    string teachingClassId;
    string otherTeachingClassId;
    vector<TimeSlot> overlapTimes;
};

struct SearchResult {
    // 按偏好优先级从优到差排列的 Top-K 方案；K 为 SearchOptions::maxSolutions。
    vector<Schedule> schedules;
    SearchStatus status = SearchStatus::Complete;
    SearchStatistics statistics;
    // 搜索过程中遇到的全部完整合法方案数，可能大于保留数。
    uint64_t totalSolutions = 0;
    // 经锁定、禁止时间、固定课表过滤后，已无可选班的必选课程。
    vector<string> coursesWithoutCandidates;
    vector<ConstraintConflict> constraintConflicts;
    bool complete() const { return status == SearchStatus::Complete || status == SearchStatus::SolutionLimit; }
    // 搜索跑完全部组合时，保留的方案就是全局最优的前 K 个（Complete 表示方案不足 K 个）。
    bool globallyRanked() const { return complete(); }
};

class ScheduleGenerator {
public:
    // 返回的方案先列固定教学班，再按课程分组顺序列其他教学班。
    static SearchResult generate(const TeachingClassService& project, const SearchOptions& options = {});
};
}
