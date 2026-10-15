#include "ui/ConflictPanel.h"
#include "ui/DisplayText.h"
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
namespace planner::ui {
ConflictPanel::ConflictPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("冲突详情", this));
    summary_ = new QLabel(this);
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({"教学班", "原因", "具体时间"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setAlternatingRowColors(true);
    connect(table_->horizontalHeader(), &QHeaderView::sectionResized, this, [this] {
        table_->resizeRowsToContents();
    });
    layout->addWidget(table_);
    auto* hint = new QLabel("双击冲突记录可切换到对应教学班。\n同课程的不同教学班即使时间不重叠，也只能选择一个。", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        emit teachingClassActivated(table_->item(row, 0)->data(Qt::UserRole).toString());
    });
}
void ConflictPanel::setTeachingClass(const TeachingClassService& project, const QString& teachingClassId) {
    table_->setRowCount(0);
    const auto* teachingClass = project.findTeachingClass(teachingClassId.toStdString());
    if (!teachingClass) { summary_->setText("选择教学班后查看不能同时选择的其他班。"); return; }
    const auto& neighbors = project.graph().neighbors(teachingClass->id());
    summary_->setText(QString("与 %1 个教学班存在互斥关系").arg(neighbors.size()));
    for (const auto& edge : neighbors) {
        const auto& other = *project.findTeachingClass(edge.neighborId);
        QStringList reasons;
        for (const auto reason : edge.detail.reasons) {
            reasons << (reason == ConflictReason::TimeOverlap ? "时间重叠" : "同课程互斥");
        }
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto* name = new QTableWidgetItem(QString::fromStdString(other.name() + "\n" + other.teacher()));
        name->setData(Qt::UserRole, QString::fromStdString(other.id()));
        table_->setItem(row, 0, name);
        table_->setItem(row, 1, new QTableWidgetItem(reasons.join('\n')));
        table_->setItem(row, 2, new QTableWidgetItem(edge.detail.overlapSlots.empty() ? "无时间重叠" : overlapText(edge.detail.overlapSlots)));
        for (int column = 0; column < 3; ++column) { table_->item(row, column)->setToolTip(table_->item(row, column)->text()); }
    }
    table_->resizeRowsToContents();
}
}
