#include "core/model/Meeting.h"
#include "core/model/Text.h"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace std;

namespace planner {
namespace {
int parseNumber(string_view value, int maximum) {
    int number = 0;
    const auto parsed = from_chars(value.data(), value.data() + value.size(), number);
    if (parsed.ec != errc{} || parsed.ptr != value.data() + value.size() || number < 1 || number > maximum) {
        throw invalid_argument("周次格式错误或超出当前学期");
    }
    return number;
}
}

vector<int> parseWeeks(const string& expression, int semesterWeeks, WeekPattern pattern) {
    auto text = detail::requiredText(expression);
    if (text.front() == '[') {
        if (text.back() != ']') {
            throw invalid_argument("周次方括号不匹配");
        }
        text = detail::requiredText(text.substr(1, text.size() - 2));
    }
    const string suffix = "周";
    if (text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0) {
        text = detail::requiredText(text.substr(0, text.size() - suffix.size()));
    }

    vector<int> result;
    size_t begin = 0;
    while (begin <= text.size()) {
        const auto comma = text.find(',', begin);
        const auto token = detail::requiredText(text.substr(begin, comma == string::npos ? comma : comma - begin));
        const auto dash = token.find('-');
        const int first = parseNumber(detail::requiredText(token.substr(0, dash)), semesterWeeks);
        const int last = dash == string::npos ? first
            : parseNumber(detail::requiredText(token.substr(dash + 1)), semesterWeeks);
        if (last < first) {
            throw invalid_argument("周次范围的起始周不能大于结束周");
        }
        for (int week = first; week <= last; ++week) {
            if (pattern == WeekPattern::EveryWeek ||
                (pattern == WeekPattern::OddWeek && week % 2 == 1) ||
                (pattern == WeekPattern::EvenWeek && week % 2 == 0)) {
                result.push_back(week);
            }
        }
        if (comma == string::npos) { break; }
        begin = comma + 1;
    }
    sort(result.begin(), result.end());
    result.erase(unique(result.begin(), result.end()), result.end());
    if (result.empty()) {
        throw invalid_argument("单双周筛选后没有上课周次");
    }
    return result;
}

Meeting::Meeting(vector<int> weeks, int weekday, int startPeriod,
                int endPeriod, string classroom)
    : weeks_(move(weeks)), weekday_(weekday), startPeriod_(startPeriod),
      endPeriod_(endPeriod), classroom_(detail::requiredText(move(classroom))) {
    if (weeks_.empty() || weekday < 1 || weekday > TimeBitSet::daysPerWeek ||
        startPeriod < 1 || endPeriod > TimeBitSet::periodsPerDay || startPeriod > endPeriod) {
        throw invalid_argument("请检查周次、星期和起止节次");
    }
    sort(weeks_.begin(), weeks_.end());
    weeks_.erase(unique(weeks_.begin(), weeks_.end()), weeks_.end());
}

TimeBitSet Meeting::timeBitmap(int semesterWeeks) const {
    TimeBitSet result(semesterWeeks);
    // 注意Meeting不绑定学期长度；这里写入位图时才检查周次是否超过当前学期。
    for (int week : weeks_) {
        for (int period = startPeriod_; period <= endPeriod_; ++period) {
            result.set({week, weekday_, period});
        }
    }
    return result;
}

bool Meeting::operator==(const Meeting& other) const noexcept {
    return weeks_ == other.weeks_ && weekday_ == other.weekday_ && startPeriod_ == other.startPeriod_
        && endPeriod_ == other.endPeriod_ && classroom_ == other.classroom_;
}

}
