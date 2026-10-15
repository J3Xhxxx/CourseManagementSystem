#include "ui/TeachingClassDialog.h"
#include "ui/DisplayText.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <stdexcept>

using namespace std;

namespace planner::ui {
TeachingClassDialog::TeachingClassDialog(int semesterWeeks, const TeachingClass* initial, QWidget* parent)
    : QDialog(parent), weeks_(semesterWeeks) {
    setWindowTitle(initial ? "编辑教学班与上课安排" : "新增教学班");
    resize(880, 600);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    code_ = new QLineEdit(this);
    name_ = new QLineEdit(this);
    credits_ = new QLineEdit(this);
    credits_->setPlaceholderText("例如 3.5");
    code_->setPlaceholderText("例如 CS101");
    name_->setPlaceholderText("例如 操作系统");
    if (initial) {
        code_->setText(QString::fromStdString(initial->code()));
        name_->setText(QString::fromStdString(initial->name()));
        credits_->setText(QString::number(initial->credits(), 'g', 17));
    }
    form->addRow("课程代码（代号）", code_);
    form->addRow("课程名称", name_);
    form->addRow("学分", credits_);
    teacher_ = new QLineEdit(this);
    if (initial) { teacher_->setText(QString::fromStdString(initial->teacher())); }
    form->addRow("授课教师", teacher_);
    fixed_ = new QCheckBox("学校固定的必修课（加入后不可编辑或删除）", this);
    fixed_->setChecked(initial && initial->isFixed());
    form->addRow("是否为固定课程", fixed_);
    layout->addLayout(form);
    auto* identityHint = new QLabel("代码、名称、教师共同确定教学班；每次编辑仅修改当前教学班。", this);
    identityHint->setWordWrap(true);
    layout->addWidget(identityHint);
    auto* hint = new QLabel(QString("当前学期 %1 周。每行是一条周期性上课安排；周次如 1-5,7-9。每天共六个完整时段。").arg(weeks_), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels({"周次", "单双周", "星期", "起始节", "结束节", "教室"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(table_);
    connect(table_, &QTableWidget::itemChanged, this, [](QTableWidgetItem* item) {
        if (item->toolTip() != item->text()) { item->setToolTip(item->text()); }
    });
    auto* actions = new QHBoxLayout;
    auto* add = new QPushButton("添加上课安排", this);
    auto* remove = new QPushButton("删除选中安排", this);
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    layout->addLayout(actions);
    error_ = new QLabel(this);
    error_->setWordWrap(true);
    error_->setStyleSheet("color: #a12622;");
    layout->addWidget(error_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);
    connect(add, &QPushButton::clicked, this, [this] { addMeeting(); });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (table_->currentRow() >= 0) { table_->removeRow(table_->currentRow()); }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &TeachingClassDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TeachingClassDialog::reject);
    if (initial) { for (const auto& meeting : initial->meetings()) { addMeeting(&meeting); } }
    else { addMeeting(); }
}
void TeachingClassDialog::addMeeting(const Meeting* meeting) {
    const int row = table_->rowCount();
    table_->insertRow(row);
    QString expression = meeting ? weeksText(meeting->weeks()) : QString("1-%1").arg(weeks_);
    int patternIndex = 0;
    if (meeting && meeting->weeks().size() > 1) {
        const auto& weeks = meeting->weeks();
        bool alternating = true;
        for (size_t i = 1; i < weeks.size(); ++i) {
            if (weeks[i] - weeks[i - 1] != 2) { alternating = false; break; }
        }
        // 将已保存的实际周次还原为等价的输入，便于编辑。
        // 只有连续相隔两周时才还原单双周范围，避免补出原来没有的上课周。
        if (alternating) {
            expression = QString("%1-%2").arg(weeks.front()).arg(weeks.back());
            patternIndex = weeks.front() % 2 == 1 ? 1 : 2;
        }
    }
    table_->setItem(row, 0, new QTableWidgetItem(expression));
    auto* pattern = new QComboBox(table_);
    pattern->addItems({"每周", "单周", "双周"});
    pattern->setCurrentIndex(patternIndex);
    table_->setCellWidget(row, 1, pattern);
    auto* day = new QComboBox(table_);
    for (int d = 1; d <= 7; ++d) { day->addItem(weekdayText(d)); }
    day->setCurrentIndex(meeting ? meeting->weekday() - 1 : 0);
    table_->setCellWidget(row, 2, day);
    for (int column = 3; column <= 4; ++column) {
        auto* spin = new QSpinBox(table_);
        spin->setRange(1, 6);
        spin->setValue(meeting ? (column == 3 ? meeting->startPeriod() : meeting->endPeriod()) : 1);
        table_->setCellWidget(row, column, spin);
    }
    table_->setItem(row, 5, new QTableWidgetItem(meeting ? QString::fromStdString(meeting->classroom()) : QString{}));
    table_->setCurrentCell(row, 0);
}
void TeachingClassDialog::accept() {
    vector<Meeting> meetings;
    for (int row = 0; row < table_->rowCount(); ++row) {
        try {
            const auto pattern = static_cast<WeekPattern>(qobject_cast<QComboBox*>(table_->cellWidget(row, 1))->currentIndex());
            meetings.emplace_back(parseWeeks(table_->item(row, 0)->text().trimmed().toStdString(), weeks_, pattern),
                qobject_cast<QComboBox*>(table_->cellWidget(row, 2))->currentIndex() + 1,
                qobject_cast<QSpinBox*>(table_->cellWidget(row, 3))->value(),
                qobject_cast<QSpinBox*>(table_->cellWidget(row, 4))->value(),
                table_->item(row, 5)->text().trimmed().toStdString());
        } catch (const exception& e) {
            error_->setText(QString("第 %1 条安排：%2").arg(row + 1).arg(errorText(e)));
            return;
        }
    }
    try {
        bool validCredits = false;
        const double credits = credits_->text().trimmed().toDouble(&validCredits);
        if (!validCredits) { throw invalid_argument("学分必须是有限的非负数"); }
        value_.emplace(code_->text().trimmed().toStdString(), name_->text().trimmed().toStdString(), credits, teacher_->text().trimmed().toStdString(), std::move(meetings), weeks_, fixed_->isChecked());
        QDialog::accept();
    } catch (const exception& e) { error_->setText(errorText(e)); }
}
}
