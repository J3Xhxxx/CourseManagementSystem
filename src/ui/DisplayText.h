#pragma once
#include "core/structures/TimeBitSet.h"
#include <QStringList>
#include <exception>
#include <map>

namespace planner::ui {
using namespace std;

inline QString weeksText(const vector<int>& weeks) {
    QStringList parts;
    for (size_t i = 0; i < weeks.size(); ++i) {
        const int first = weeks[i];
        while (i + 1 < weeks.size() && weeks[i + 1] == weeks[i] + 1) { ++i; }
        parts << (first == weeks[i] ? QString::number(first) : QString("%1-%2").arg(first).arg(weeks[i]));
    }
    return parts.join(',');
}
inline QString weekdayText(int day) {
    static const QStringList days{"周一","周二","周三","周四","周五","周六","周日"};
    return days.at(day - 1);
}
inline QString overlapText(const vector<TimeSlot>& times) {
    map<pair<int,int>, vector<int>> groups;
    for (const auto& slot : times) { groups[{slot.weekday, slot.period}].push_back(slot.week); }
    QStringList lines;
    for (const auto& group : groups) {
        lines << QString("第 %1 周 · %2第 %3 节").arg(weeksText(group.second))
            .arg(weekdayText(group.first.first)).arg(group.first.second);
    }
    return lines.join('\n');
}
inline QString errorText(const exception& error) {
    return QString::fromUtf8(error.what());
}
}
