#pragma once
#include "core/model/Schedule.h"
#include <array>
#include <string>
#include <vector>

namespace planner {
using namespace std;

// 指标标识。用于用户设置的优先级顺序、界面显示和 JSON 保存。
enum class MetricId {
    EarlyMorningCount, ClassDays, TotalGapPeriods,
    MaximumContinuousPeriods, LatestEndPeriod, FreeHalfDays
};

// 指标固定方向：前五项越小越好，半天数越多越好。方向由指标本身决定，用户只调整优先级。
inline bool ascendingMetric(MetricId id) noexcept {
    return id != MetricId::FreeHalfDays;
}

inline const array<MetricId, 6>& metricIds() {
    static const array<MetricId, 6> ids{MetricId::EarlyMorningCount, MetricId::ClassDays,
        MetricId::TotalGapPeriods, MetricId::MaximumContinuousPeriods,
        MetricId::LatestEndPeriod, MetricId::FreeHalfDays};
    return ids;
}

inline int metricValue(const ScheduleMetrics& metrics, MetricId id) noexcept {
    switch (id) {
    case MetricId::EarlyMorningCount: return metrics.earlyMorningCount;
    case MetricId::ClassDays: return metrics.classDays;
    case MetricId::TotalGapPeriods: return metrics.totalGapPeriods;
    case MetricId::MaximumContinuousPeriods: return metrics.maximumContinuousPeriods;
    case MetricId::LatestEndPeriod: return metrics.latestEndPeriod;
    case MetricId::FreeHalfDays: return metrics.freeHalfDays;
    }
    return 0;
}

// 界面标签；同时作为 JSON 中的保存值和读取校验依据。
const char* metricLabel(MetricId id);
// 按标签还原指标标识；标签不认识时返回 false，用于 JSON 读取校验。
bool metricFromLabel(const string& label, MetricId& id);
// 按前后顺序用“优于”连成一行，例如 早课次数 优于 空堂节数 优于 上课天数。
string preferenceText(const vector<MetricId>& order);

// 偏好优先级：六项指标各出现一次，靠前的指标先比较。
struct PreferenceOrder {
    vector<MetricId> values;
    static PreferenceOrder defaultOrder();
    void validate() const;
    bool operator==(const PreferenceOrder& other) const noexcept { return values == other.values; }
};

// 方案比较器：按偏好优先级做字典序比较。指标全部相同的方案再按教学班 ID 序列稳定区分，
// 保证同一份输入每次运行得到相同顺序。
class ScheduleRanking {
public:
    explicit ScheduleRanking(PreferenceOrder preferences = PreferenceOrder::defaultOrder());
    const vector<MetricId>& preferences() const noexcept { return preferences_.values; }
    bool better(const Schedule& left, const Schedule& right) const;
    bool worse(const Schedule& left, const Schedule& right) const { return better(right, left); }
private:
    PreferenceOrder preferences_;
};

// 供 BoundedMaxHeap 使用的比较器：让“更差”的方案排在堆顶，
// 于是装满 K 个后，只要新方案优于堆顶就替换它。
struct ScheduleAbove {
    ScheduleRanking ranking;
    static ScheduleAbove from(PreferenceOrder preferences) {
        return ScheduleAbove{ScheduleRanking(move(preferences))};
    }
    bool operator()(const Schedule& left, const Schedule& right) const { return ranking.worse(left, right); }
};

class TeachingClassService;

class ScheduleEvaluator {
public:
    // 按真实周次统计六项指标；空闲半天统计上午（1-2 节）与下午（3-4 节）。
    static ScheduleMetrics evaluate(const TeachingClassService& project, const Schedule& schedule);
    // 按偏好优先级排序；指标相同时按教学班 ID 序列确定顺序。
    static vector<Schedule> sortByPreference(vector<Schedule> schedules, const PreferenceOrder& preferences);
};
}
