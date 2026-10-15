#include "core/algorithms/ScheduleEvaluator.h"
#include "core/services/TeachingClassService.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

using namespace std;

namespace planner {
namespace {
// 每天六个大节分成上午（1-3 节）和下午/晚上（4-6 节）两个半天。
constexpr int halfDayCount = 2;
constexpr int periodsPerHalfDay = TimeBitSet::periodsPerDay / halfDayCount;

int halfDayFirstPeriod(int halfDay) { return halfDay * periodsPerHalfDay + 1; }
int halfDayLastPeriod(int halfDay) { return (halfDay + 1) * periodsPerHalfDay; }
}

const char* metricLabel(MetricId id) {
    switch (id) {
    case MetricId::EarlyMorningCount: return "早课次数";
    case MetricId::ClassDays: return "上课天数";
    case MetricId::TotalGapPeriods: return "空堂节数";
    case MetricId::MaximumContinuousPeriods: return "最大连续节数";
    case MetricId::LatestEndPeriod: return "最晚结束节次";
    case MetricId::FreeHalfDays: return "空闲半天数";
    }
    return "";
}

bool metricFromLabel(const string& label, MetricId& id) {
    for (const MetricId candidate : metricIds()) {
        if (label == metricLabel(candidate)) { id = candidate; return true; }
    }
    return false;
}

string preferenceText(const vector<MetricId>& order) {
    string text;
    for (const MetricId id : order) {
        if (!text.empty()) { text += " 优于 "; }
        text += metricLabel(id);
    }
    return text;
}

PreferenceOrder PreferenceOrder::defaultOrder() {
    PreferenceOrder order;
    order.values.assign(metricIds().begin(), metricIds().end());
    return order;
}

void PreferenceOrder::complete() {
    for (const MetricId id : metricIds()) {
        if (find(values.begin(), values.end(), id) == values.end()) { values.push_back(id); }
    }
}

void PreferenceOrder::validate() const {
    if (values.size() != metricIds().size()) {
        throw invalid_argument("偏好优先级必须恰好包含六项评价指标");
    }
    set<MetricId> seen;
    for (const MetricId id : values) {
        if (!seen.insert(id).second) { throw invalid_argument("偏好优先级中存在重复指标"); }
    }
}

ScheduleRanking::ScheduleRanking(PreferenceOrder preferences) : preferences_(move(preferences)) {
    preferences_.complete();
    preferences_.validate();
}

// 字典序比较：按优先级依次比较各指标，全部相同再比较教学班 ID 序列。
bool ScheduleRanking::better(const Schedule& left, const Schedule& right) const {
    for (const MetricId id : preferences_.values) {
        const int leftValue = metricValue(left.evaluation, id);
        const int rightValue = metricValue(right.evaluation, id);
        if (leftValue == rightValue) { continue; }
        return ascendingMetric(id) ? leftValue < rightValue : leftValue > rightValue;
    }
    return left.selectedTeachingClassIds < right.selectedTeachingClassIds;
}
// 逐周展开方案的时间位集合，按真实周次统计六项指标。
// 不能把所有周叠加成一张课表：那样会把“第 3 周周一有课”和“第 5 周周一有课”混为一天。
ScheduleMetrics ScheduleEvaluator::evaluate(const TeachingClassService& project, const Schedule& schedule) {
    (void)project;
    ScheduleMetrics metrics;
    const int weekdayCount = TimeBitSet::daysPerWeek;
    const int periodCount = TimeBitSet::periodsPerDay;
    for (int week = 1; week <= schedule.occupiedTimeBitmap.semesterWeeks(); ++week) {
        for (int day = 0; day < weekdayCount; ++day) {
            bool occupied[TimeBitSet::periodsPerDay] = {};
            int first = 0, last = 0, used = 0, run = 0;
            for (int period = 0; period < periodCount; ++period) {
                const TimeSlot slot{week, day + 1, period + 1};
                occupied[period] = schedule.occupiedTimeBitmap.contains(slot);
                if (!occupied[period]) { run = 0; continue; }
                if (first == 0) { first = period + 1; }
                last = period + 1;
                ++used;
                ++run;
                metrics.maximumContinuousPeriods = max(metrics.maximumContinuousPeriods, run);
            }
            if (first == 0) {
                // 这一周这一天完全没课，上午和下午都算空闲半天。
                metrics.freeHalfDays += halfDayCount;
                continue;
            }
            ++metrics.classDays;
            // 第一大节有课就记一次早课，一个班一周上两次第一大节也只记一次。
            if (occupied[0]) { ++metrics.earlyMorningCount; }
            // 单日空堂 = 第一节课到最后一节课之间的时长减去实际有课的节数。
            metrics.totalGapPeriods += last - first + 1 - used;
            metrics.latestEndPeriod = max(metrics.latestEndPeriod, last);
            for (int halfDay = 0; halfDay < halfDayCount; ++halfDay) {
                bool free = true;
                for (int period = halfDayFirstPeriod(halfDay); period <= halfDayLastPeriod(halfDay) && free; ++period) {
                    free = !occupied[period - 1];
                }
                if (free) { ++metrics.freeHalfDays; }
            }
        }
    }
    // 早课次数已在上面的逐周扫描中统计完成。
    return metrics;
}

vector<Schedule> ScheduleEvaluator::sortByPreference(vector<Schedule> schedules, const PreferenceOrder& preferences) {
    const ScheduleRanking ranking(preferences);
    // 与 Top-K 堆使用同一个全序（含教学班 ID 兜底比较），保证两条路径得到完全相同的顺序。
    sort(schedules.begin(), schedules.end(), [&ranking](const Schedule& left, const Schedule& right) {
        return ranking.better(left, right);
    });
    return schedules;
}
}
