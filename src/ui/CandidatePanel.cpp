#include "ui/CandidatePanel.h"
#include "ui/DisplayText.h"
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace std;

namespace planner::ui {
CandidatePanel::CandidatePanel(ProjectController& project, SearchController& search, QWidget* parent)
    : QWidget(parent), project_(project), search_(search) {
    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel("保留学校固定课表，按必选、个人锁定和禁止时间生成。\n每门必选课程恰选一个班，方案尚未按偏好排序。", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* limits = new QFormLayout;
    maxSolutions_ = new QSpinBox(this);
    maxSolutions_->setRange(1,1000);
    maxSolutions_->setValue(100);
    maxNodes_ = new QSpinBox(this);
    maxNodes_->setRange(1,10000000);
    maxNodes_->setValue(100000);
    maxNodes_->setGroupSeparatorShown(true);
    limits->addRow("最多保留方案", maxSolutions_);
    limits->addRow("最多尝试教学班", maxNodes_);
    layout->addLayout(limits);
    auto* controls = new QHBoxLayout;
    generate_ = new QPushButton("生成方案", this);
    cancel_ = new QPushButton("取消搜索", this);
    controls->addWidget(generate_);
    controls->addWidget(cancel_);
    layout->addLayout(controls);
    status_ = new QLabel(this);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    diagnostics_ = new QPlainTextEdit(this);
    diagnostics_->setReadOnly(true);
    diagnostics_->setMaximumHeight(160);
    diagnostics_->hide();
    layout->addWidget(diagnostics_);
    statistics_ = new QLabel(this);
    statistics_->setWordWrap(true);
    layout->addWidget(statistics_);
    list_ = new QListWidget(this);
    layout->addWidget(list_,1);
    auto* navigation = new QHBoxLayout;
    previous_ = new QPushButton("上一个", this);
    next_ = new QPushButton("下一个", this);
    navigation->addWidget(previous_);
    navigation->addWidget(next_);
    layout->addLayout(navigation);
    details_ = new QLabel(this);
    details_->setTextFormat(Qt::PlainText);
    details_->setWordWrap(true);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    details_->setMaximumHeight(180);
    layout->addWidget(details_);
    connect(generate_, &QPushButton::clicked, this, &CandidatePanel::generate);
    connect(cancel_, &QPushButton::clicked, this, [this] {
        search_.cancel();
        cancel_->setEnabled(false);
        status_->setText("正在取消搜索…");
    });
    connect(previous_, &QPushButton::clicked, this, [this] { list_->setCurrentRow(list_->currentRow()-1); });
    connect(next_, &QPushButton::clicked, this, [this] { list_->setCurrentRow(list_->currentRow()+1); });
    connect(list_, &QListWidget::currentRowChanged, this, &CandidatePanel::showCandidate);
    // 预览单个教学班后，再点当前方案也应恢复完整课表。
    connect(list_, &QListWidget::itemClicked, this, [this] { showCandidate(list_->currentRow()); });
    connect(&search_, &SearchController::stateChanged, this, &CandidatePanel::updateState);
    connect(&search_, &SearchController::resultsChanged, this, &CandidatePanel::updateResults);
    connect(&project_, &ProjectController::changed, this, &CandidatePanel::updateState);
    updateResults();
    updateState();
}
void CandidatePanel::generate() {
    search_.start(static_cast<size_t>(maxSolutions_->value()), static_cast<uint64_t>(maxNodes_->value()));
}
void CandidatePanel::updateState() {
    generate_->setEnabled(!search_.busy() && !project_.project().teachingClasses().empty());
    cancel_->setEnabled(search_.busy());
    maxSolutions_->setEnabled(!search_.busy());
    maxNodes_->setEnabled(!search_.busy());
    if (search_.busy()) { status_->setText("正在后台搜索；修改课程或约束将取消本次搜索。"); return; }
    if (!search_.result()) {
        status_->setText(project_.project().teachingClasses().empty() ? "请先添加教学班。"
            : project_.project().constraints().requiredCourseKeys.empty() ? "未勾选必选课程；生成时将得到一个空方案。"
            : "点击生成方案。修改课程或约束后需要重新生成。");
        return;
    }
    const auto& result = *search_.result();
    const auto count = result.schedules.size();
    switch (result.status) {
    case SearchStatus::Complete:
        status_->setText(count ? QString("搜索完成，共 %1 个合法方案。").arg(count)
                              : "搜索完成：当前课程集合没有合法方案。");
        break;
    case SearchStatus::SolutionLimit:
        status_->setText(QString("达到保留上限，显示前 %1 个合法方案；仍有其他方案。").arg(count));
        break;
    case SearchStatus::NodeLimit:
        status_->setText(QString("达到尝试上限，找到 %1 个合法方案；搜索未完成。").arg(count));
        break;
    case SearchStatus::Cancelled:
        status_->setText(QString("搜索已取消，保留 %1 个合法方案；搜索未完成。").arg(count));
        break;
    }
    QStringList explanations;
    const auto& project = project_.project();
    auto className = [&](const TeachingClass& teachingClass) {
        return QString::fromStdString(teachingClass.code()+" · "+teachingClass.name());
    };
    auto teachingClassName = [&](const string& id) {
        const auto& teachingClass = *project.findTeachingClass(id);
        return className(teachingClass)+" / "+QString::fromStdString(teachingClass.teacher());
    };
    for (const auto& key : result.coursesWithoutCandidates) {
        const auto& course = *project.findTeachingClass(project.teachingClassesForCourse(key).front());
        explanations << className(course)+"：锁定条件、禁止时间或固定初始课表排除了所有候选。";
    }
    for (const auto& conflict : result.constraintConflicts) {
        QString label;
        switch (conflict.reason) {
        case ConstraintConflictReason::LockedTeachingClassesOverlap: label = "锁定教学班之间有冲突："; break;
        case ConstraintConflictReason::LockedTeachingClassForbidden: label = "锁定教学班与禁止时间冲突："; break;
        case ConstraintConflictReason::FixedTeachingClassesOverlap: label = "学校固定课程之间有冲突："; break;
        case ConstraintConflictReason::FixedTeachingClassForbidden: label = "学校固定课程与禁止时间冲突："; break;
        case ConstraintConflictReason::FixedAndLockedOverlap: label = "学校固定课程与锁定教学班冲突："; break;
        }
        QString description = label+teachingClassName(conflict.teachingClassId);
        if (!conflict.otherTeachingClassId.empty()) { description += " ↔ "+teachingClassName(conflict.otherTeachingClassId); }
        explanations << description+"\n"+overlapText(conflict.overlapTimes);
    }
    if (result.complete() && count == 0 && explanations.empty()) {
        explanations << "必选课程的剩余教学班无法互相兼容。可在“教学班冲突”页查看具体冲突；尚未计算最小无解原因。";
    }
    diagnostics_->setPlainText(explanations.join("\n\n"));
    diagnostics_->setVisible(!explanations.empty());
}
void CandidatePanel::updateResults() {
    {
        QSignalBlocker blocker(list_);
        list_->clear();
        statistics_->clear();
        diagnostics_->clear();
        diagnostics_->hide();
        if (search_.result()) {
            const auto& result = *search_.result();
            for (size_t i = 0; i < result.schedules.size(); ++i) {
                const auto& candidate = result.schedules[i];
                list_->addItem(QString("方案 %1 · %2 门课程 · %3 个全学期时段")
                    .arg(i+1).arg(candidate.selectedTeachingClassIds.size()).arg(candidate.occupiedTimeBitmap.count()));
            }
            const auto& stats = result.statistics;
            statistics_->setText(QString("尝试 %1 次 · 剪枝 %2 次\n提前排除 %3 个候选 · %4 次无后续选择\n得到 %5 个完整组合 · 用时 %6 ms")
                .arg(stats.visitedNodes).arg(stats.prunedBranches).arg(stats.candidateRemovals).arg(stats.domainWipeouts)
                .arg(stats.completeLeaves).arg(stats.elapsedMilliseconds,0,'f',1));
        }
        list_->setCurrentRow(list_->count() ? 0 : -1);
    }
    showCandidate(list_->currentRow());
}
void CandidatePanel::showCandidate(int index) {
    previous_->setEnabled(index > 0);
    next_->setEnabled(index >= 0 && index+1 < list_->count());
    details_->clear();
    // 列表与当前结果同步重建，有选中行时结果一定存在。
    if (index >= 0) {
        QStringList names;
        for (const auto& id : search_.result()->schedules[static_cast<size_t>(index)].selectedTeachingClassIds) {
            const auto* teachingClass = project_.project().findTeachingClass(id);
            names << QString::fromStdString(teachingClass->name() + " · " + teachingClass->teacher());
        }
        details_->setText(names.join("\n"));
        details_->setToolTip(names.join("\n"));
    }
    emit candidateSelected(index);
}
}
