#include "core/model/TeachingClass.h"
#include "core/model/Text.h"

#include <stdexcept>
#include <cmath>
#include <utility>

using namespace std;

namespace planner {

TeachingClass::TeachingClass(string code, 
                            string name, 
                            double credits, 
                            string teacher,
                            vector<Meeting> meetings, 
                            int semesterWeeks, 
                            bool isFixed)
    : code_(detail::requiredText(move(code))), name_(detail::requiredText(move(name))),
      credits_(credits), isFixed_(isFixed), courseKey_(detail::identityPart(code_) + detail::identityPart(name_)), teacher_(detail::requiredText(move(teacher))),
      id_(courseKey_ + detail::identityPart(teacher_)),
      meetings_(move(meetings)), timeBitmap_(semesterWeeks) {
        
    if (!isfinite(credits_) || credits_ < 0) {
        throw invalid_argument("学分必须是有限的非负数");
    }

    if (meetings_.empty()) {
        throw invalid_argument("教学班至少需要一条上课安排");
    }

    for (const auto& meeting : meetings_) {
        const auto occupied = meeting.timeBitmap(semesterWeeks);// 合并所有真实安排，同一教学班内部仅上1次课。
        if (timeBitmap_.intersects(occupied)) {
            throw invalid_argument("同一教学班内存在重叠的上课安排");
        }
        timeBitmap_.unite(occupied);
    }
}

}
