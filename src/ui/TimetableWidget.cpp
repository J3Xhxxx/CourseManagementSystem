#include "ui/TimetableWidget.h"
#include "ui/DisplayText.h"
#include "ui/ReverseSpinBox.h"
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

using namespace std;

namespace planner::ui {
TimetableWidget::TimetableWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    title_ = new QLabel("选择一个教学班查看上课安排", this);
    title_->setWordWrap(true);
    layout->addWidget(title_);
    auto* bar = new QHBoxLayout;
    // 只改变输入控件的步进方向，真实周次与时间索引仍从 1 开始。
    week_ = new ReverseSpinBox(this);
    week_->setRange(1, 20);
    week_->setPrefix("第 ");
    week_->setSuffix(" 周");
    bar->addWidget(week_);
    summary_ = new QLabel(this);
    bar->addWidget(summary_);
    bar->addStretch();
    layout->addLayout(bar);
    table_ = new QTableWidget(6, 7, this);
    QStringList headers;
    for (int day = 1; day <= 7; ++day) { headers << weekdayText(day); }
    table_->setHorizontalHeaderLabels(headers);
    headers.clear();
    for (int i = 0; i < 6; ++i) {
        headers << QString("第 %1 节\n%2").arg(i + 1).arg(PERIOD_TIMES[i]);
    }
    table_->setVerticalHeaderLabels(headers);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setMinimumHeight(380);
    layout->addWidget(table_);
    hint_ = new QLabel(this);
    hint_->setWordWrap(true);
    layout->addWidget(hint_);
    connect(week_, &QSpinBox::valueChanged, this, &TimetableWidget::refresh);
    refresh();
}
void TimetableWidget::setTeachingClass(const TeachingClassService& project, const TeachingClass* teachingClass) {
    entries_.clear();
    for (const auto& fixed : project.teachingClasses()) {
        if (fixed.isFixed()) { entries_.push_back(fixed); }
    }
    const auto fixedCount = entries_.size();
    if (teachingClass && !teachingClass->isFixed()) { entries_.push_back(*teachingClass); }
    showingSchedule_ = false;
    forbiddenTimes_ = project.constraints().forbiddenTimes;
    title_->setText(teachingClass ? QString::fromStdString(teachingClass->name()+" / "+teachingClass->teacher())
        : QString("固定初始课表 · %1 门课程").arg(fixedCount));
    hint_->setText("学校固定课程始终显示。选择教学班可预览它与固定课表的冲突；点击方案列表查看完整结果。");
    week_->setRange(1, project.semesterWeeks());
    refresh();
}
void TimetableWidget::setSchedule(const TeachingClassService& project, const Schedule& schedule, int number) {
    entries_.clear();
    double credits = 0;
    for (const auto& id : schedule.selectedTeachingClassIds) {
        const auto& teachingClass = *project.findTeachingClass(id);
        entries_.push_back(teachingClass);
        credits += teachingClass.credits();
    }
    showingSchedule_ = true;
    forbiddenTimes_ = project.constraints().forbiddenTimes;
    title_->setText(QString("方案 %1 · %2 门课程 · %3 学分").arg(number).arg(entries_.size()).arg(credits));
    hint_->setText("完整方案 · 按真实周次展示。点击左侧教学班可查看该班的详细安排和冲突。");
    week_->setRange(1, project.semesterWeeks());
    refresh();
}
void TimetableWidget::refresh() {
    table_->clearContents();
    int occupied = 0;
    int forbiddenCount = 0;
    for (int day = 1; day <= 7; ++day) {
        for (int period = 1; period <= 6; ++period) {
            if (!forbiddenTimes_.contains({week_->value(),day,period})) { continue; }
            auto* item = new QTableWidgetItem("禁止上课");
            item->setTextAlignment(Qt::AlignCenter);
            item->setBackground(QColor("#fbe4e4"));
            item->setForeground(QColor("#802424"));
            item->setToolTip(QString("第 %1 周 %2 第 %3 节禁止上课").arg(week_->value()).arg(weekdayText(day)).arg(period));
            table_->setItem(period-1,day-1,item);
            ++forbiddenCount;
        }
    }
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& teachingClass = entries_[i];
        // 按实际周次过滤，同一个星期和节次在不同周可以安排不同课程。
        for (const auto& meeting : teachingClass.meetings()) {
            if (!binary_search(meeting.weeks().begin(), meeting.weeks().end(), week_->value())) { continue; }
            for (int period = meeting.startPeriod(); period <= meeting.endPeriod(); ++period) {
                const auto room = QString::fromStdString(meeting.classroom());
                const bool banned = forbiddenTimes_.contains({week_->value(),meeting.weekday(),period});
                auto label = QString::fromStdString(teachingClass.name()+"\n"+teachingClass.teacher()) + "\n" + room;
                if (banned) { label += "\n与禁止时间冲突"; }
                const auto* previous = table_->item(period-1,meeting.weekday()-1);
                // UserRole 标记课程格；仅有禁止时间背景时，不计为两门课重叠。
                const bool overlap = previous && previous->data(Qt::UserRole).toBool();
                if (overlap) { label = previous->text()+"\n"+label+"\n时间冲突"; }
                auto* item = new QTableWidgetItem(label);
                item->setData(Qt::UserRole,true);
                item->setTextAlignment(Qt::AlignCenter);
                item->setBackground(QColor::fromHsv(static_cast<int>((i * 67 + 205) % 360),40,247));
                item->setForeground(QColor("#163b56"));
                if (banned || overlap) {
                    item->setBackground(QColor("#fbe4e4"));
                    item->setForeground(QColor("#802424"));
                }
                item->setToolTip(QString("第 %1 周 %2 第 %3 节\n%4").arg(week_->value()).arg(weekdayText(meeting.weekday())).arg(period).arg(item->text()));
                table_->setItem(period - 1, meeting.weekday() - 1, item);
                if (!overlap) { ++occupied; }
            }
        }
    }
    summary_->setText(!entries_.empty() || showingSchedule_ ? QString("本周 %1 个上课时段").arg(occupied) : "未选择教学班");
    if (forbiddenCount) { summary_->setText(summary_->text()+QString(" · %1 个禁止时段").arg(forbiddenCount)); }
}
}
