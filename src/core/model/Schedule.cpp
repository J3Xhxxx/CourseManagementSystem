#include "core/model/Schedule.h"
#include <string>

using namespace std;

namespace planner {

string scheduleMetricsText(const ScheduleMetrics& metrics) {
    return "早课 " + to_string(metrics.earlyMorningCount)
        + " 次 · 上课 " + to_string(metrics.classDays)
        + " 天 · 空堂 " + to_string(metrics.totalGapPeriods)
        + " 节 · 最长连续 " + to_string(metrics.maximumContinuousPeriods)
        + " 节 · 最晚第 " + to_string(metrics.latestEndPeriod)
        + " 节 · 空闲半天 " + to_string(metrics.freeHalfDays);
}

}
