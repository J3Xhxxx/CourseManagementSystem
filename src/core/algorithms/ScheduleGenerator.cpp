#include "core/algorithms/ScheduleGenerator.h"
#include "core/algorithms/ScheduleEvaluator.h"
#include "core/structures/BoundedMaxHeap.h"
#include <chrono>
#include <cstdint>
#include <unordered_map>

using namespace std;

namespace planner {
namespace {

// 一次搜索用到的全部状态。用类把数据和各个步骤放在一起，避免一个函数过长。
class Searcher {
public:
    Searcher(const TeachingClassService& project, const SearchOptions& options)
        : project_(project), options_(options), initial_(project.fixedSchedule()),
          occupied_(initial_.occupiedTimeBitmap),
          heap_(options.maxSolutions, ScheduleAbove::from(project.constraints().preferenceOrder)) {}
    SearchResult run();

private:
    bool buildDomains();
    size_t chooseCourse(vector<size_t>& values);
    bool forwardCheck(const TeachingClass* chosen);
    void restore(size_t mark);
    void saveSchedule();
    bool dfs(size_t depth);

    const TeachingClassService& project_;
    const SearchOptions& options_;
    Schedule initial_; // 学校固定课程构成的初始课表
    TimeBitSet occupied_; // 固定课程加上已选教学班占用的时间
    SearchResult result_;

    vector<vector<const TeachingClass*>> domains_; // domains_[i]：第 i 门课的候选教学班
    vector<const TeachingClass*> selected_; // selected_[i]：第 i 门课选中的班，未选时为 nullptr

    // 以下状态只在前向检查中使用。
    // 删除候选时只把 active_ 置为 false，不真正从数组中删除，回溯时容易恢复。
    vector<vector<bool>> active_;
    vector<size_t> remaining_; // 每门课还剩多少个有效候选
    vector<pair<size_t, size_t>> removed_; // 删除记录 (课程, 候选)，回溯时按栈的顺序恢复
    unordered_map<string, pair<size_t, size_t>> candidatePositions_; // 教学班 ID -> (课程, 候选)
    BoundedMaxHeap<Schedule, ScheduleAbove> heap_; // 堆顶为保留方案中最差的一个。
};

// 第一步：为每门要搜索的课程准备候选教学班，并检查不能替换的班之间有没有直接冲突。
// 返回 false 表示已经确定无解，不需要再搜索。
bool Searcher::buildDomains() {
    const UserConstraints& constraints = project_.constraints();
    // 固定班和个人锁定班都不能换，单独收集起来，后面检查它们之间的冲突。
    vector<const TeachingClass*> pinned;
    for (const auto& id : initial_.selectedTeachingClassIds) {
        pinned.push_back(project_.findTeachingClass(id));
    }
    for (const auto& course : project_.courseKeys()) {
        // 不是必选课，或者是固定课程（已经在初始课表里），都不需要搜索。
        if (!constraints.requiredCourseKeys.count(course) || project_.fixedTeachingClassForCourse(course)) {
            continue;
        }
        const vector<string> ids = project_.teachingClassesForCourse(course);
        const TeachingClass* locked = nullptr;
        for (const auto& id : ids) {
            if (constraints.lockedTeachingClassIds.count(id)) {
                locked = project_.findTeachingClass(id);
                pinned.push_back(locked);
                break;
            }
        }
        vector<const TeachingClass*> domain;
        for (const auto& id : ids) {
            const TeachingClass* teachingClass = project_.findTeachingClass(id);
            // 锁定了某个班时只保留这一个；占用禁止时间或与固定课程冲突的班直接排除。
            if (locked && teachingClass != locked) { continue; }
            if (teachingClass->timeBitmap().intersects(constraints.forbiddenTimes)) { continue; }
            if (teachingClass->timeBitmap().intersects(initial_.occupiedTimeBitmap)) { continue; }
            if (options_.strategy == SearchStrategy::MrvForwardChecking) {
                candidatePositions_[teachingClass->id()] = {domains_.size(), domain.size()};
            }
            domain.push_back(teachingClass);
        }
        if (domain.empty()) { result_.coursesWithoutCandidates.push_back(course); }
        domains_.push_back(domain);
    }

    for (size_t i = 0; i < pinned.size(); ++i) {
        TimeBitSet forbidden = pinned[i]->timeBitmap().intersection(constraints.forbiddenTimes);
        if (!forbidden.empty()) {
            ConstraintConflictReason reason = pinned[i]->isFixed() ? ConstraintConflictReason::FixedTeachingClassForbidden
                                                                   : ConstraintConflictReason::LockedTeachingClassForbidden;
            result_.constraintConflicts.push_back({reason, pinned[i]->id(), "", forbidden.occupiedSlots()});
        }
        for (size_t j = i + 1; j < pinned.size(); ++j) {
            TimeBitSet overlap = pinned[i]->timeBitmap().intersection(pinned[j]->timeBitmap());
            if (overlap.empty()) { continue; }
            ConstraintConflictReason reason = ConstraintConflictReason::LockedTeachingClassesOverlap;
            if (pinned[i]->isFixed() && pinned[j]->isFixed()) {
                reason = ConstraintConflictReason::FixedTeachingClassesOverlap;
            } else if (pinned[i]->isFixed() || pinned[j]->isFixed()) {
                reason = ConstraintConflictReason::FixedAndLockedOverlap;
            }
            result_.constraintConflicts.push_back({reason, pinned[i]->id(), pinned[j]->id(), overlap.occupiedSlots()});
        }
    }
    return result_.coursesWithoutCandidates.empty() && result_.constraintConflicts.empty();
}

// 选出下一门要决定的课程，并把这门课可以尝试的候选下标放进 values。
size_t Searcher::chooseCourse(vector<size_t>& values) {
    size_t best = 0;
    if (options_.strategy == SearchStrategy::Backtracking) {
        // 普通回溯：按顺序取第一门还没选的课，所有候选都尝试。
        while (selected_[best]) { ++best; }
        for (size_t v = 0; v < domains_[best].size(); ++v) { values.push_back(v); }
    } else if (options_.strategy == SearchStrategy::Mrv) {
        // MRV：每门未选的课都重新数一遍与已选时间不冲突的候选，取最少的那门。
        size_t bestCount = SIZE_MAX;
        for (size_t i = 0; i < domains_.size() && bestCount > 0; ++i) {
            if (selected_[i]) { continue; }
            vector<size_t> candidates;
            for (size_t v = 0; v < domains_[i].size(); ++v) {
                if (!occupied_.intersects(domains_[i][v]->timeBitmap())) { candidates.push_back(v); }
            }
            if (candidates.size() < bestCount) {
                bestCount = candidates.size();
                best = i;
                values = candidates;
            }
        }
    } else {
        // MRV + 前向检查：冲突候选已被前向检查删除，remaining_ 就是准确的剩余数。
        size_t bestCount = SIZE_MAX;
        for (size_t i = 0; i < domains_.size() && bestCount > 0; ++i) {
            if (!selected_[i] && remaining_[i] < bestCount) {
                bestCount = remaining_[i];
                best = i;
            }
        }
        for (size_t v = 0; v < domains_[best].size(); ++v) {
            if (active_[best][v]) { values.push_back(v); }
        }
    }
    return best;
}

// 前向检查：只遍历 chosen 的冲突邻居，通过 ID 索引找到需要停用的候选。
// 如果某门课的候选被删空，说明这样选下去一定无解，返回 false。
bool Searcher::forwardCheck(const TeachingClass* chosen) {
    for (const auto& edge : project_.graph().neighbors(chosen->id())) {
        const auto position = candidatePositions_.find(edge.neighborId);
        // 冲突图包含所有教学班，已过滤或未参与搜索的班没有候选位置。
        if (position == candidatePositions_.end()) { continue; }
        const auto [i, v] = position->second;
        // 同课程的其他班、之前已选的课程以及上层已排除的候选都不再处理。
        if (selected_[i] || !active_[i][v]) { continue; }
        active_[i][v] = false;
        --remaining_[i];
        removed_.push_back({i, v});
        ++result_.statistics.candidateRemovals;
        if (remaining_[i] == 0) {
            ++result_.statistics.domainWipeouts;
            return false;
        }
    }
    return true;
}

// 撤销删除记录，直到记录数回到 mark，即恢复本次选班之前被删的候选。
void Searcher::restore(size_t mark) {
    while (removed_.size() > mark) {
        active_[removed_.back().first][removed_.back().second] = true;
        ++remaining_[removed_.back().first];
        removed_.pop_back();
    }
}

// 每个合法方案先评价，再由大根堆保留较优的 K 个；堆满后仍继续搜索。
void Searcher::saveSchedule() {
    ++result_.statistics.completeLeaves;
    ++result_.totalSolutions;
    Schedule schedule = initial_;
    // 按课程分组顺序排列，不跟随 MRV 的实际选课顺序。
    for (const TeachingClass* teachingClass : selected_) {
        schedule.selectedTeachingClassIds.push_back(teachingClass->id());
    }
    schedule.occupiedTimeBitmap = occupied_;
    schedule.evaluation = ScheduleEvaluator::evaluate(project_, schedule);
    heap_.offer(move(schedule));
}

// 深度优先搜索，depth 是已经选好的课程数。
// 返回 false 表示需要停止整个搜索（用户取消或达到尝试上限）。
bool Searcher::dfs(size_t depth) {
    if (depth == domains_.size()) { saveSchedule(); return true; }
    vector<size_t> values;
    size_t course = chooseCourse(values);
    if (values.empty()) {
        // 这门课已经没有可选的班，当前分支无解。
        ++result_.statistics.prunedBranches;
        return true;
    }
    for (size_t v : values) {
        if (options_.cancelled && options_.cancelled()) {
            result_.status = SearchStatus::Cancelled;
            return false;
        }
        if (result_.statistics.visitedNodes == options_.maxNodes) {
            result_.status = SearchStatus::NodeLimit;
            return false;
        }
        const TeachingClass* teachingClass = domains_[course][v];
        ++result_.statistics.visitedNodes;
        // 剪枝 1：与已选课程时间冲突。
        if (occupied_.intersects(teachingClass->timeBitmap())) {
            ++result_.statistics.prunedBranches;
            continue;
        }
        size_t mark = removed_.size();
        selected_[course] = teachingClass;
        // 剪枝 2：前向检查发现某门课已经没有候选。
        if (options_.strategy == SearchStrategy::MrvForwardChecking && !forwardCheck(teachingClass)) {
            ++result_.statistics.prunedBranches;
            restore(mark);
            selected_[course] = nullptr;
            continue;
        }
        occupied_.unite(teachingClass->timeBitmap());
        bool keepGoing = dfs(depth + 1);
        // 回溯。选中的班与原占用时间不相交，所以直接减去它的时间就能还原。
        occupied_.subtract(teachingClass->timeBitmap());
        restore(mark);
        selected_[course] = nullptr;
        if (!keepGoing) { return false; }
    }
    return true;
}

SearchResult Searcher::run() {
    auto start = chrono::steady_clock::now();
    if (buildDomains()) {
        selected_.assign(domains_.size(), nullptr);
        for (const auto& domain : domains_) {
            active_.push_back(vector<bool>(domain.size(), true));
            remaining_.push_back(domain.size());
        }
        dfs(0);
    }
    result_.schedules = heap_.drainSorted();
    // 只有完整搜索后，Top-K 才是整个候选空间中的最优结果。
    if (result_.status == SearchStatus::Complete && result_.totalSolutions > options_.maxSolutions) {
        result_.status = SearchStatus::SolutionLimit;
    }
    auto end = chrono::steady_clock::now();
    result_.statistics.elapsedMilliseconds = chrono::duration<double, milli>(end - start).count();
    return result_;
}

} // namespace

SearchResult ScheduleGenerator::generate(const TeachingClassService& project, const SearchOptions& options) {
    Searcher searcher(project, options);
    return searcher.run();
}

} // namespace planner
