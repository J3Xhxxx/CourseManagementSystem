// 性能对照：同一组数据分别用暴力枚举和三种搜索策略求解，
// 比较方案数是否一致，以及尝试次数、剪枝次数和用时。输出为 CSV 格式。
#include "core/algorithms/BruteForceGenerator.h"
#include "core/algorithms/ScheduleGenerator.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

using namespace std;
using namespace planner;

namespace {

// 测试数据都只有 1 周，每个教学班每周上一次课。
TeachingClass makeClass(const string& code, const string& name, const string& teacher, int weekday, int period) {
    return TeachingClass(code, name, 1, teacher, {Meeting({1}, weekday, period, period, "H")}, 1);
}

// 场景 1：6 门课各 3 个班，每门课在不同的星期上课，互不冲突，共 3^6 = 729 个方案。
TeachingClassService independent() {
    vector<TeachingClass> classes;
    for (int c = 0; c < 6; ++c) {
        for (int t = 0; t < 3; ++t) {
            classes.push_back(makeClass(to_string(c), "Course", to_string(t), c + 1, 1));
        }
    }
    TeachingClassService project(1);
    project.replaceTeachingClasses(classes);
    return project;
}

// 场景 2、3：在场景 1 后面再加一门只有一个班的课，放在周一第 1 节。
// 不可满足时，它与第 0 门课的 3 个班全部冲突；可满足时，第 0 门课的第 3 个班改到第 2 节。
// 这门课排在最后，普通回溯要很晚才发现冲突，MRV 会优先选它。
TeachingClassService lateBottleneck(bool satisfiable) {
    vector<TeachingClass> classes;
    for (int c = 0; c < 6; ++c) {
        for (int t = 0; t < 3; ++t) {
            int period = 1;
            if (satisfiable && c == 0 && t == 2) { period = 2; }
            classes.push_back(makeClass(to_string(c), "Course", to_string(t), c + 1, period));
        }
    }
    classes.push_back(makeClass("Gate", "Last input course", "Single choice", 1, 1));
    TeachingClassService project(1);
    project.replaceTeachingClasses(classes);
    return project;
}

// 场景 4：鸽巢问题。5 门课都只能排在周一的 4 个时段里，一定无解。
TeachingClassService pigeonhole() {
    vector<TeachingClass> classes;
    for (int c = 0; c < 5; ++c) {
        for (int period = 1; period <= 4; ++period) {
            classes.push_back(makeClass(to_string(c), "Pigeonhole", to_string(period), 1, period));
        }
    }
    TeachingClassService project(1);
    project.replaceTeachingClasses(classes);
    return project;
}

void compare(const string& name, const TeachingClassService& project) {
    // 暴力枚举要检查的组合总数 = 各门课班数的乘积。
    size_t combinations = 1;
    for (const auto& course : project.courseKeys()) {
        combinations *= project.teachingClassesForCourse(course).size();
    }
    auto start = chrono::steady_clock::now();
    vector<Schedule> brute = BruteForceGenerator::generate(project, 100000);
    auto end = chrono::steady_clock::now();
    double elapsed = chrono::duration<double, milli>(end - start).count();
    cout << name << ",BruteForce," << brute.size() << ',' << brute.size() << ",," << combinations << ",0," << elapsed << ",\n";

    const SearchStrategy strategies[3] = {SearchStrategy::Backtracking, SearchStrategy::Mrv, SearchStrategy::MrvForwardChecking};
    const char* strategyNames[3] = {"Backtracking", "MRV", "MRV+FC"};
    for (int i = 0; i < 3; ++i) {
        SearchOptions options;
        options.strategy = strategies[i];
        options.maxSolutions = 100000;
        options.maxNodes = 1000000;
        SearchResult result = ScheduleGenerator::generate(project, options);
        const SearchStatistics& stats = result.statistics;
        // 上限设得足够大，正常情况下都能搜完；合法方案总数应与暴力枚举相同。
        bool same = result.complete() && result.totalSolutions == brute.size();
        cout << name << ',' << strategyNames[i] << ',' << result.totalSolutions << ',' << result.schedules.size() << ','
             << stats.visitedNodes << ',' << stats.completeLeaves << ',' << stats.prunedBranches << ','
             << stats.elapsedMilliseconds << ',' << (same ? "yes" : "no") << '\n';
    }
}

} // namespace

int main() {
    cout << fixed << setprecision(3);
    cout << "case,strategy,solutions,kept,visited_nodes,complete_combinations,pruned_branches,elapsed_ms,same_as_brute\n";
    compare("independent", independent());
    compare("late_bottleneck_unsatisfiable", lateBottleneck(false));
    compare("late_bottleneck_satisfiable", lateBottleneck(true));
    compare("dense_unsatisfiable", pigeonhole());
    return 0;
}
