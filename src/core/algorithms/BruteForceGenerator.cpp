#include "core/algorithms/BruteForceGenerator.h"
#include "core/algorithms/ScheduleEvaluator.h"
#include "core/algorithms/ScheduleValidator.h"
#include <stdexcept>

using namespace std;

namespace planner {
vector<Schedule> BruteForceGenerator::generate(const TeachingClassService& project, size_t maxCombinations) {
    vector<vector<string>> domains;
    for (const auto& course : project.courseKeys()) {
        if (!project.constraints().requiredCourseKeys.count(course)) { continue; }
        // 不提前过滤锁定或禁止时间，由校验器检查每个完整组合。
        domains.push_back(project.teachingClassesForCourse(course));
    }
    size_t product = 1;
    for (const auto& domain : domains) {
        product *= domain.size();
        if (product > maxCombinations) { throw length_error("组合数量超出暴力枚举上限"); }
    }
    vector<Schedule> result;
    for (size_t ordinal = 0; ordinal < product; ++ordinal) {
        // 把组合序号当作混合进制数，各课程的候选数分别作为进制。
        auto remaining = ordinal;
        Schedule candidate(project.semesterWeeks());
        for (const auto& domain : domains) {
            const auto& id = domain[remaining % domain.size()];
            remaining /= domain.size();
            candidate.selectedTeachingClassIds.push_back(id);
            candidate.occupiedTimeBitmap.unite(project.findTeachingClass(id)->timeBitmap());
        }
        // 先评价再校验：校验器会复核方案保存的六项指标，指标为空的方案视为过期结果。
        candidate.evaluation = ScheduleEvaluator::evaluate(project, candidate);
        if (ScheduleValidator::validate(project, candidate).valid()) {
            result.push_back(move(candidate));
        }
    }
    return result;
}
}
