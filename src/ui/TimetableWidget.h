#pragma once
#include "core/model/Schedule.h"
#include "core/services/TeachingClassService.h"
#include <QWidget>
class QLabel;
class QSpinBox;
class QTableWidget;
namespace planner::ui {
using namespace std;

class TimetableWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimetableWidget(QWidget* parent = nullptr);
    void setTeachingClass(const TeachingClassService& project, const TeachingClass* teachingClass);
    void setSchedule(const TeachingClassService& project, const Schedule& schedule, int number);
private:
    void refresh();
    // 保存展示数据的副本，避免项目增删后留下悬空引用。
    vector<TeachingClass> entries_;
    bool showingSchedule_ = false;
    TimeBitSet forbiddenTimes_;
    QLabel* title_;
    QLabel* summary_;
    QLabel* hint_;
    QSpinBox* week_;
    QTableWidget* table_;
};
}
