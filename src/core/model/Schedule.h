#pragma once
#include "core/structures/TimeBitSet.h"
#include <string>

namespace planner {
using namespace std;

// 六项方案评价指标，由 ScheduleEvaluator 按真实周次计算后写入方案。
// 定义放在 model 层，使方案自身能携带评价结果，不再单独维护一份指标表。
struct ScheduleMetrics {
    int earlyMorningCount = 0;         // 第一大节（早八）的上课次数
    int classDays = 0;                 // 按周去重的上课天数
    int totalGapPeriods = 0;           // 同一天内第一节课到最后一节课之间的空堂节数
    int maximumContinuousPeriods = 0;  // 单日最大连续上课节数
    int latestEndPeriod = 0;           // 全学期最晚结束节次，没有课时为 0
    int freeHalfDays = 0;              // 全学期完全没课的半天数（每天分上午、下午两个半天）
};

// 方案保存所选班的 ID、时间并集和评价指标；有效性由校验器根据当前项目重新判断。
struct Schedule {
    explicit Schedule(int semesterWeeks = 20) : occupiedTimeBitmap(semesterWeeks) {}
    vector<string> selectedTeachingClassIds;
    TimeBitSet occupiedTimeBitmap;
    ScheduleMetrics evaluation;
};

// 六项指标的一行文本，供界面列表和方案详情复用。
string scheduleMetricsText(const ScheduleMetrics& metrics);
}
