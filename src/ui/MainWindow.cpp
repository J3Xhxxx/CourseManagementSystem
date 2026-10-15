#include "ui/MainWindow.h"
#include "ui/TeachingClassPanel.h"
#include "ui/TimetableWidget.h"
#include "ui/TimetableConverterWidget.h"
#include "ui/ConflictPanel.h"
#include "ui/CandidatePanel.h"
#include "ui/ConstraintPanel.h"
#include "ui/TeachingClassDialog.h"
#include "ui/DisplayText.h"
#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTabWidget>
#include <QSignalBlocker>

using namespace std;

namespace planner::ui {
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), controller_(this), search_(controller_,this) {
    resize(1480, 800);
    setMinimumSize(1100, 620);
    auto* splitter = new QSplitter(this);
    teachingClasses_ = new TeachingClassPanel(splitter);
    timetableTabs_ = new QTabWidget(splitter);
    timetable_ = new TimetableWidget(timetableTabs_);
    timetableTabs_->addTab(timetable_,"课表");
    timetableTabs_->addTab(new TimetableConverterWidget(timetableTabs_),"课表转换");
    tabs_ = new QTabWidget(splitter);
    candidates_ = new CandidatePanel(controller_,search_,tabs_);
    conflicts_ = new ConflictPanel(tabs_);
    tabs_->addTab(candidates_, "候选方案");
    auto* constraints = new ConstraintPanel(controller_,tabs_);
    tabs_->addTab(constraints, "选课约束");
    tabs_->addTab(conflicts_, "教学班冲突");
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({310, 700, 450});
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 1);
    setCentralWidget(splitter);
    auto* bar = addToolBar("常用操作");
    bar->setMovable(false);
    // 操作只在工具栏展示；QAction 同时绑定快捷键和回调。
    auto action = [&](const QString& text, const QKeySequence& shortcut, auto callback) {
        auto* result = bar->addAction(text);
        result->setShortcut(shortcut);
        connect(result, &QAction::triggered, this, callback);
        return result;
    };
    action("新建", QKeySequence::New, [this] { newProject(); });
    action("打开…", QKeySequence::Open, [this] { openProject(); });
    action("保存", QKeySequence::Save, [this] { saveProject(); });
    action("另存为…", QKeySequence::SaveAs, [this] { saveProject(true); });
    action("导入 CSV…", QKeySequence("Ctrl+I"), [this] { importCsv(); });
    bar->addSeparator();
    action("新增教学班", {}, [this] { editTeachingClass(true); });
    edit_ = action("编辑", {}, [this] { editSelection(); });
    remove_ = action("删除", QKeySequence::Delete, [this] { removeSelection(); });
    bar->addSeparator();
    action("选课约束",{},[this,constraints] {
        tabs_->setCurrentWidget(constraints);
    });
    auto* generate = action("生成方案",QKeySequence("Ctrl+G"),[this] {
        tabs_->setCurrentWidget(candidates_);
        candidates_->generate();
    });
    auto updateGenerate = [this,generate] {
        generate->setEnabled(!search_.busy() && !controller_.project().teachingClasses().empty());
    };
    connect(&search_,&SearchController::stateChanged,this,updateGenerate);
    connect(&controller_,&ProjectController::changed,this,updateGenerate);
    updateGenerate();
    totals_ = new QLabel(this);
    statusBar()->addPermanentWidget(totals_);
    connect(&controller_, &ProjectController::changed, this, &MainWindow::refresh);
    connect(teachingClasses_, &TeachingClassPanel::selectionChanged, this, [this] {
        viewingCandidate_ = false;
        if (!teachingClasses_->teachingClassId().isEmpty()) { timetableTabs_->setCurrentWidget(timetable_); }
        showSelection();
    });
    connect(candidates_, &CandidatePanel::candidateSelected, this, &MainWindow::showCandidate);
    connect(teachingClasses_, &TeachingClassPanel::editRequested, this, &MainWindow::editSelection);
    connect(conflicts_, &ConflictPanel::teachingClassActivated, teachingClasses_, &TeachingClassPanel::selectTeachingClass);
    refresh();
    statusBar()->showMessage("直接新增教学班，或导入 CSV 文件。");
}
void MainWindow::refresh() {
    const auto& project = controller_.project();
    {
        QSignalBlocker blocker(teachingClasses_);
        teachingClasses_->setProject(project);
    }
    showSelection();
    const auto name = controller_.filePath().isEmpty() ? QString("未命名项目") : QFileInfo(controller_.filePath()).fileName();
    setWindowTitle(name + "[*] — 个人选课与冲突分析");
    setWindowModified(controller_.modified());
    totals_->setText(QString("%1 周  |  %2 门课程  |  %3 个教学班  |  %4 条冲突边")
        .arg(project.semesterWeeks()).arg(project.courseKeys().size()).arg(project.teachingClasses().size()).arg(project.graph().edgeCount()));
    if (!project.teachingClasses().empty()) {
        statusBar()->showMessage("在“选课约束”设置必选课程、锁定教学班和禁止时间，再生成方案。");
    }
}
void MainWindow::showSelection() {
    const auto& project = controller_.project();
    const auto* teachingClass = project.findTeachingClass(teachingClasses_->teachingClassId().toStdString());
    edit_->setEnabled(teachingClass && !teachingClass->isFixed());
    remove_->setEnabled(teachingClass && !teachingClass->isFixed());
    if (!viewingCandidate_) {
        timetable_->setTeachingClass(project, teachingClass);
    }
    conflicts_->setTeachingClass(project, teachingClasses_->teachingClassId());
}
void MainWindow::showCandidate(int index) {
    viewingCandidate_ = index >= 0;
    if (viewingCandidate_) {
        timetableTabs_->setCurrentWidget(timetable_);
        timetable_->setSchedule(controller_.project(),search_.result()->schedules[static_cast<size_t>(index)],index+1);
    } else {
        showSelection();
    }
}
bool MainWindow::execute(const function<void()>& operation) {
    try { operation(); return true; }
    catch (const exception& e) {
        QMessageBox::warning(this, "操作未完成", errorText(e));
        return false;
    }
}
bool MainWindow::maybeSave() {
    if (!controller_.modified()) { return true; }
    QMessageBox box(QMessageBox::Question, "未保存的修改", "当前项目有未保存的修改，是否先保存？",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.button(QMessageBox::Save)->setText("保存");
    box.button(QMessageBox::Discard)->setText("放弃修改");
    box.button(QMessageBox::Cancel)->setText("取消");
    box.setDefaultButton(QMessageBox::Save);
    const auto result = box.exec();
    if (result == QMessageBox::Cancel) { return false; }
    return result == QMessageBox::Discard || saveProject();
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) { event->accept(); } else { event->ignore(); }
}
void MainWindow::newProject() {
    if (!maybeSave()) { return; }
    bool ok = false;
    const int weeks = QInputDialog::getInt(this, "新建项目", "学期周数", 20, 1, 104, 1, &ok);
    if (ok) { execute([&] { controller_.newProject(weeks); }); }
}
void MainWindow::openProject() {
    const auto path = QFileDialog::getOpenFileName(this, "打开选课项目", {}, "选课项目 (*.json)");
    if (path.isEmpty() || !maybeSave()) { return; }
    execute([&] {
        controller_.openProject(path);
        statusBar()->showMessage("已打开：" + path);
    });
}
bool MainWindow::saveProject(bool saveAs) {
    auto path = controller_.filePath();
    if (saveAs || path.isEmpty()) {
        QFileDialog dialog(this, "保存选课项目");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter("选课项目 (*.json)");
        dialog.setDefaultSuffix("json");
        dialog.selectFile(path.isEmpty() ? "选课项目.json" : path);
        if (dialog.exec() != QDialog::Accepted) { return false; }
        path = dialog.selectedFiles().front();
    }
    return execute([&] {
        controller_.saveProject(path);
        statusBar()->showMessage("已保存：" + path);
    });
}
void MainWindow::importCsv() {
    const auto path = QFileDialog::getOpenFileName(this, "导入课程 CSV", {}, "CSV 文件 (*.csv)");
    if (path.isEmpty()) { return; }
    execute([&] {
        const auto result = controller_.importCsv(path);
        statusBar()->showMessage(QString("导入成功：新增 %1 个教学班、%2 条安排；跳过 %3 条重复安排。")
            .arg(result.addedTeachingClasses).arg(result.addedMeetings).arg(result.skippedDuplicates));
    });
}
void MainWindow::editTeachingClass(bool create) {
    const auto oldId = create ? QString{} : teachingClasses_->teachingClassId();
    const auto* initial = create ? nullptr : controller_.project().findTeachingClass(oldId.toStdString());
    // 双击列表也会触发编辑，固定班需要在这里拦下。
    if (initial && initial->isFixed()) { return; }
    TeachingClassDialog dialog(controller_.project().semesterWeeks(), initial, this);
    while (dialog.exec() == QDialog::Accepted) {
        if (execute([&] { controller_.putTeachingClass(oldId, dialog.value()); })) {
            teachingClasses_->selectTeachingClass(QString::fromStdString(dialog.value().id()));
            break;
        }
    }
}
void MainWindow::editSelection() {
    editTeachingClass(false);
}
void MainWindow::removeSelection() {
    const auto teachingClass = teachingClasses_->teachingClassId();
    QMessageBox box(QMessageBox::Question, "确认删除",
        "删除该教学班及其全部上课安排？",
        QMessageBox::Yes | QMessageBox::Cancel, this);
    box.button(QMessageBox::Yes)->setText("删除");
    box.button(QMessageBox::Cancel)->setText("取消");
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Yes) { return; }
    execute([&] { controller_.removeTeachingClass(teachingClass); });
}
}
