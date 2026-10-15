#include "ui/ConstraintPanel.h"
#include "ui/DisplayText.h"
#include "ui/ReverseSpinBox.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <map>

using namespace std;

namespace planner::ui {
ConstraintPanel::ConstraintPanel(ProjectController& project, QWidget* parent)
    : QWidget(parent), project_(project) {
    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel("学校固定课程始终保留，无法取消或换班。\n其他课程每门选一个班；锁定会自动勾选课程。",this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    courses_ = new QTableWidget(0,2,this);
    courses_->setHorizontalHeaderLabels({"必选课程","锁定教学班"});
    courses_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    courses_->verticalHeader()->hide();
    courses_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    courses_->setMinimumHeight(140);
    layout->addWidget(courses_,1);
    summary_ = new QLabel(this);
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    // 禁止时间单独成组，与上方的必选、锁定设置在视觉上分开。
    auto* forbiddenBox = new QGroupBox("禁止上课的时间",this);
    auto* forbiddenLayout = new QVBoxLayout(forbiddenBox);
    layout->addWidget(forbiddenBox,1);
    auto* form = new QFormLayout;
    weeks_ = new QLineEdit(forbiddenBox);
    weeks_->setPlaceholderText("如 1-5,7-9");
    pattern_ = new QComboBox(forbiddenBox);
    pattern_->addItems({"每周","单周","双周"});
    auto* weekRow = new QHBoxLayout;
    weekRow->addWidget(weeks_,1);
    weekRow->addWidget(pattern_);
    form->addRow("周次",weekRow);
    weekday_ = new QComboBox(forbiddenBox);
    for (int day = 1; day <= 7; ++day) { weekday_->addItem(weekdayText(day)); }
    start_ = new ReverseSpinBox(forbiddenBox);
    end_ = new ReverseSpinBox(forbiddenBox);
    start_->setRange(1,6);
    end_->setRange(1,6);
    auto* periodRow = new QHBoxLayout;
    periodRow->addWidget(weekday_);
    periodRow->addWidget(new QLabel("第",forbiddenBox));
    periodRow->addWidget(start_);
    periodRow->addWidget(new QLabel("至",forbiddenBox));
    periodRow->addWidget(end_);
    periodRow->addWidget(new QLabel("节",forbiddenBox));
    form->addRow("时段",periodRow);
    forbiddenLayout->addLayout(form);
    auto* actions = new QHBoxLayout;
    auto* add = new QPushButton("添加禁止时间",forbiddenBox);
    auto* clearRange = new QPushButton("解除此范围",forbiddenBox);
    actions->addWidget(add);
    actions->addWidget(clearRange);
    forbiddenLayout->addLayout(actions);
    forbidden_ = new QTableWidget(0,3,forbiddenBox);
    forbidden_->setHorizontalHeaderLabels({"周次","星期","节次"});
    forbidden_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    forbidden_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    forbidden_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    forbidden_->verticalHeader()->hide();
    forbidden_->setSelectionBehavior(QAbstractItemView::SelectRows);
    forbidden_->setSelectionMode(QAbstractItemView::SingleSelection);
    forbidden_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    forbidden_->setMinimumHeight(100);
    forbiddenLayout->addWidget(forbidden_,1);
    auto* remove = new QPushButton("删除选中的禁止时间",forbiddenBox);
    remove->setEnabled(false);
    forbiddenLayout->addWidget(remove);
    error_ = new QLabel(forbiddenBox);
    error_->setTextFormat(Qt::PlainText);
    error_->setWordWrap(true);
    error_->setStyleSheet("color: #a12622;");
    forbiddenLayout->addWidget(error_);
    // 软偏好优先级单独成组：只影响合法方案之间的排序，不影响哪些方案合法。
    auto* preferenceBox = new QGroupBox("方案偏好优先级（软偏好，不影响合法性）",this);
    auto* preferenceLayout = new QVBoxLayout(preferenceBox);
    layout->addWidget(preferenceBox);
    preferenceHint_ = new QLabel("从上到下依次比较：上一项相同的方案才继续比下一项。",preferenceBox);
    preferenceHint_->setWordWrap(true);
    preferenceLayout->addWidget(preferenceHint_);
    preference_ = new QListWidget(preferenceBox);
    preference_->setSelectionMode(QAbstractItemView::SingleSelection);
    preference_->setMaximumHeight(150);
    preferenceLayout->addWidget(preference_);
    auto* preferenceButtons = new QHBoxLayout;
    preferenceUp_ = new QPushButton("上移",preferenceBox);
    preferenceDown_ = new QPushButton("下移",preferenceBox);
    preferenceReset_ = new QPushButton("恢复默认顺序",preferenceBox);
    preferenceButtons->addWidget(preferenceUp_);
    preferenceButtons->addWidget(preferenceDown_);
    preferenceButtons->addWidget(preferenceReset_);
    preferenceLayout->addLayout(preferenceButtons);
    connect(courses_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() == 0) { setRequired(item->data(Qt::UserRole).toString().toStdString(),item->checkState() == Qt::Checked); }
    });
    connect(add, &QPushButton::clicked, this, [this] { editForbidden(true); });
    connect(clearRange, &QPushButton::clicked, this, [this] { editForbidden(false); });
    connect(remove, &QPushButton::clicked, this, &ConstraintPanel::removeSelectedForbidden);
    connect(forbidden_, &QTableWidget::itemSelectionChanged, this, [this,remove] {
        remove->setEnabled(forbidden_->currentRow() >= 0);
    });
    connect(forbidden_, &QTableWidget::cellDoubleClicked, this, [this](int row,int) {
        const auto& group = groups_[static_cast<size_t>(row)];
        vector<int> weeks;
        for (const auto& time : group) { weeks.push_back(time.week); }
        weeks_->setText(weeksText(weeks));
        pattern_->setCurrentIndex(0);
        weekday_->setCurrentIndex(group.front().weekday-1);
        start_->setValue(group.front().period);
        end_->setValue(group.front().period);
    });
    connect(preference_, &QListWidget::itemSelectionChanged, this, &ConstraintPanel::updatePreferenceButtons);
    connect(preferenceUp_, &QPushButton::clicked, this, [this] { movePreference(-1); });
    connect(preferenceDown_, &QPushButton::clicked, this, [this] { movePreference(1); });
    connect(preferenceReset_, &QPushButton::clicked, this, &ConstraintPanel::resetPreference);
    // 先停用旧控件，再排队重建；直接刷新会在信号处理中删除正在使用的下拉框。
    connect(&project_, &ProjectController::dataChanged, this, [this] { setEnabled(false); });
    connect(&project_, &ProjectController::dataChanged, this, &ConstraintPanel::refresh,Qt::QueuedConnection);
    refresh();
}
void ConstraintPanel::refresh() {
    const auto& project = project_.project();
    const auto& constraints = project.constraints();
    if (semesterWeeks_ != project.semesterWeeks()) {
        semesterWeeks_ = project.semesterWeeks();
        weeks_->setText(QString("1-%1").arg(semesterWeeks_));
        pattern_->setCurrentIndex(0);
    }
    size_t fixedCount = 0;
    {
        QSignalBlocker blocker(courses_);
        courses_->setRowCount(0);
        for (const auto& key : project.courseKeys()) {
            const auto ids = project.teachingClassesForCourse(key);
            const auto& representative = *project.findTeachingClass(ids.front());
            const auto* fixed = project.fixedTeachingClassForCourse(key);
            if (fixed) { ++fixedCount; }
            const int row = courses_->rowCount();
            courses_->insertRow(row);
            auto* item = new QTableWidgetItem(QString::fromStdString(representative.code()+" · "+representative.name()));
            item->setFlags(fixed ? Qt::ItemIsSelectable : Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            item->setData(Qt::UserRole,QString::fromStdString(key));
            item->setToolTip(fixed ? "学校固定课程，不能取消必选或选择其他教学班" : item->text());
            item->setCheckState(constraints.requiredCourseKeys.count(key) ? Qt::Checked : Qt::Unchecked);
            courses_->setItem(row,0,item);
            auto* locked = new QComboBox(courses_);
            if (fixed) {
                locked->addItem("学校固定："+QString::fromStdString(fixed->teacher()),QString::fromStdString(fixed->id()));
            } else {
                locked->addItem("不锁定",{});
                for (const auto& id : ids) {
                    const auto* teachingClass = project.findTeachingClass(id);
                    locked->addItem(QString::fromStdString(teachingClass->teacher()),QString::fromStdString(id));
                    if (constraints.lockedTeachingClassIds.count(id)) { locked->setCurrentIndex(locked->count()-1); }
                }
            }
            locked->setEnabled(!fixed && locked->count() > 1);
            courses_->setCellWidget(row,1,locked);
            connect(locked,&QComboBox::currentIndexChanged,this,[this,locked,key] {
                setLocked(key,locked->currentData().toString().toStdString());
            });
        }
    }
    summary_->setText(QString("固定 %1 门 · 必选 %2 门 · 个人锁定 %3 个班 · 禁止 %4 个全学期时段")
        .arg(fixedCount).arg(constraints.requiredCourseKeys.size())
        .arg(constraints.lockedTeachingClassIds.size()).arg(constraints.forbiddenTimes.count()));
    // 偏好优先级列表：保持当前选中项，重排后仍停在同一个指标上。
    MetricId selected = MetricId::EarlyMorningCount;
    bool hasSelection = false;
    if (const auto* current = preference_->currentItem()) {
        selected = static_cast<MetricId>(current->data(Qt::UserRole).toInt());
        hasSelection = true;
    }
    {
        QSignalBlocker blocker(preference_);
        preference_->clear();
        for (const MetricId id : constraints.preferenceOrder.values) {
            auto* item = new QListWidgetItem(QString::fromUtf8(metricLabel(id)),preference_);
            item->setData(Qt::UserRole,static_cast<int>(id));
            if (hasSelection && id == selected) { preference_->setCurrentItem(item); }
        }
        if (!preference_->currentItem() && preference_->count()) { preference_->setCurrentRow(0); }
    }
    updatePreferenceButtons();
    map<pair<int,int>,vector<TimeSlot>> grouped;
    for (const auto& time : constraints.forbiddenTimes.occupiedSlots()) { grouped[{time.weekday,time.period}].push_back(time); }
    forbidden_->setRowCount(0);
    groups_.clear();
    for (const auto& entry : grouped) {
        const int row = forbidden_->rowCount();
        forbidden_->insertRow(row);
        groups_.push_back(entry.second);
        vector<int> weeks;
        for (const auto& time : entry.second) { weeks.push_back(time.week); }
        const int period = entry.first.second;
        const QStringList labels{weeksText(weeks),weekdayText(entry.first.first),
            QString("%1 · %2").arg(period).arg(PERIOD_TIMES[period-1])};
        for (int column = 0; column < 3; ++column) {
            auto* item = new QTableWidgetItem(labels[column]);
            item->setToolTip(item->text());
            forbidden_->setItem(row,column,item);
        }
    }
    setEnabled(true);
}
void ConstraintPanel::apply(UserConstraints constraints) {
    try {
        project_.setConstraints(std::move(constraints));
        error_->clear();
    } catch (const exception& e) {
        error_->setText(errorText(e));
        QMetaObject::invokeMethod(this,[this] { refresh(); },Qt::QueuedConnection);
    }
}
void ConstraintPanel::setRequired(const string& courseKey, bool required) {
    auto constraints = project_.project().constraints();
    if (required) { constraints.requiredCourseKeys.insert(courseKey); }
    else {
        constraints.requiredCourseKeys.erase(courseKey);
        for (const auto& teachingClassId : project_.project().teachingClassesForCourse(courseKey)) { constraints.lockedTeachingClassIds.erase(teachingClassId); }
    }
    apply(std::move(constraints));
}
void ConstraintPanel::setLocked(const string& courseKey, const string& teachingClassId) {
    auto constraints = project_.project().constraints();
    for (const auto& id : project_.project().teachingClassesForCourse(courseKey)) { constraints.lockedTeachingClassIds.erase(id); }
    if (!teachingClassId.empty()) {
        constraints.lockedTeachingClassIds.insert(teachingClassId);
        // 锁定的班必须参与生成，因此同时将所属课程设为必选。
        constraints.requiredCourseKeys.insert(courseKey);
    }
    apply(std::move(constraints));
}
void ConstraintPanel::editForbidden(bool add) {
    try {
        const auto weeks = parseWeeks(weeks_->text().trimmed().toStdString(),project_.project().semesterWeeks(),
            static_cast<WeekPattern>(pattern_->currentIndex()));
        const auto range = Meeting(weeks,weekday_->currentIndex()+1,start_->value(),end_->value(),"禁止时间")
            .timeBitmap(project_.project().semesterWeeks());
        auto constraints = project_.project().constraints();
        if (add) { constraints.forbiddenTimes.unite(range); }
        else { constraints.forbiddenTimes.subtract(range); }
        apply(std::move(constraints));
    } catch (const exception& e) { error_->setText(errorText(e)); }
}
void ConstraintPanel::removeSelectedForbidden() {
    const int row = forbidden_->currentRow();
    auto constraints = project_.project().constraints();
    TimeBitSet removed(project_.project().semesterWeeks());
    for (const auto& time : groups_[static_cast<size_t>(row)]) { removed.set(time); }
    constraints.forbiddenTimes.subtract(removed);
    apply(std::move(constraints));
}
// 把选中指标上移或下移一位，再整体写回约束；只改变合法方案之间的排序。
void ConstraintPanel::movePreference(int offset) {
    const int row = preference_->currentRow();
    const int target = row + offset;
    if (row < 0 || target < 0 || target >= preference_->count()) { return; }
    auto constraints = project_.project().constraints();
    swap(constraints.preferenceOrder.values[static_cast<size_t>(row)],
        constraints.preferenceOrder.values[static_cast<size_t>(target)]);
    apply(std::move(constraints));
}
void ConstraintPanel::resetPreference() {
    auto constraints = project_.project().constraints();
    constraints.preferenceOrder = PreferenceOrder::defaultOrder();
    apply(std::move(constraints));
}
void ConstraintPanel::updatePreferenceButtons() {
    const int row = preference_->currentRow();
    preferenceUp_->setEnabled(row > 0);
    preferenceDown_->setEnabled(row >= 0 && row + 1 < preference_->count());
}
}
