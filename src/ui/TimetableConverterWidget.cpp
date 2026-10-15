#include "ui/TimetableConverterWidget.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <stdexcept>

using namespace std;

namespace planner::ui {
namespace {
constexpr auto kReviewReminder = "请人工补齐编号、学分并审核，第9行其他课程未读取、未结构化；请对照原表人工补充";
}
TimetableConverterWidget::TimetableConverterWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel("将学校 .xls 总课表转换为 CSV 草稿。同名课程合并为一个固定教学班、一行 CSV。",this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* controls = new QHBoxLayout;
    open_ = new QPushButton("选择学校课表 .xls…",this);
    export_ = new QPushButton("导出 CSV 草稿…",this);
    export_->setEnabled(false);
    controls->addWidget(open_);
    controls->addWidget(export_);
    controls->addStretch();
    layout->addLayout(controls);
    source_ = new QLabel("尚未选择课表。",this);
    source_->setTextFormat(Qt::PlainText);
    source_->setWordWrap(true);
    layout->addWidget(source_);
    auto* previews = new QTabWidget(this);
    auto* calendar = new QWidget(previews);
    auto* calendarLayout = new QVBoxLayout(calendar);
    auto* weekBar = new QHBoxLayout;
    week_ = new QSpinBox(calendar);
    week_->setPrefix("第 ");
    week_->setSuffix(" 周");
    week_->setRange(1,20);
    weekBar->addWidget(week_);
    weekBar->addWidget(new QLabel("基础固定课表预览（显示各周实际教师）",calendar));
    weekBar->addStretch();
    calendarLayout->addLayout(weekBar);
    timetable_ = new QTableWidget(6,7,calendar);
    timetable_->setHorizontalHeaderLabels({"周一","周二","周三","周四","周五","周六","周日"});
    timetable_->setVerticalHeaderLabels({"第 1 大节\n08:00-09:45","第 2 大节\n10:05-11:50",
        "第 3 大节\n14:00-15:45","第 4 大节\n16:05-17:50","第 5 大节\n18:40-20:25","第 6 大节\n20:45-22:30"});
    timetable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    timetable_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    timetable_->verticalHeader()->setMinimumSectionSize(46);
    timetable_->setTextElideMode(Qt::ElideNone);
    timetable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendarLayout->addWidget(timetable_);
    previews->addTab(calendar,"按周预览");
    rows_ = new QTableWidget(0,8,previews);
    rows_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    rows_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rows_->setAlternatingRowColors(true);
    previews->addTab(rows_,"CSV 草稿");
    layout->addWidget(previews,1);
    status_ = new QLabel(kReviewReminder,this);
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    process_ = new QProcess(this);
    connect(open_,&QPushButton::clicked,this,[this] {
        const auto path = QFileDialog::getOpenFileName(this,"选择学校导出的总课表",{},"Excel 97-2003 课表 (*.xls)");
        if (!path.isEmpty()) { previewFile(path); }
    });
    connect(export_,&QPushButton::clicked,this,&TimetableConverterWidget::exportCsv);
    connect(week_,&QSpinBox::valueChanged,this,&TimetableConverterWidget::refreshWeek);
    connect(process_,&QProcess::finished,this,[this](int exitCode,QProcess::ExitStatus) { receiveResult(exitCode); });
    connect(process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            open_->setEnabled(true);
            status_->setText("转换器未能启动："+process_->errorString());
        }
    });
}
TimetableConverterWidget::~TimetableConverterWidget() {
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(1000);
    }
}
void TimetableConverterWidget::previewFile(const QString& path) {
    if (process_->state() != QProcess::NotRunning) { return; }
    draft_ = {};
    export_->setEnabled(false);
    rows_->setRowCount(0);
    timetable_->clearContents();
    const auto root = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
    const auto python = root+"/.tools/timetable-converter/Scripts/python.exe";
    const auto script = root+"/tools/timetable_converter/timetable_to_csv.py";
    source_->setText(QFileInfo(path).absoluteFilePath());
    if (!QFileInfo::exists(python)) {
        status_->setText("转换器需要 Python 环境。请在项目目录运行 ./scripts/setup-converter.ps1 后重试。");
        return;
    }
    open_->setEnabled(false);
    status_->setText("正在解析学校课表…");
    // 子进程只返回预览数据和草稿文本，由用户审核后另行导入主体项目。
    process_->setProgram(python);
    process_->setArguments({script,QFileInfo(path).absoluteFilePath(),"--preview-json"});
    process_->start();
}
void TimetableConverterWidget::receiveResult(int exitCode) {
    open_->setEnabled(true);
    if (exitCode != 0) {
        status_->setText(QString::fromUtf8(process_->readAllStandardError()).trimmed());
        return;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(process_->readAllStandardOutput(),&error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        status_->setText("转换器返回的预览数据无法读取："+error.errorString());
        return;
    }
    draft_ = document.object();
    QStringList headers;
    for (const auto& header : draft_["headers"].toArray()) { headers << header.toString(); }
    rows_->setHorizontalHeaderLabels(headers);
    const auto data = draft_["rows"].toArray();
    const auto sources = draft_["sources"].toArray();
    rows_->setRowCount(static_cast<int>(data.size()));
    for (qsizetype row = 0; row < data.size(); ++row) {
        const auto fields = data[row].toArray();
        for (qsizetype col = 0; col < fields.size(); ++col) {
            auto* item = new QTableWidgetItem(fields[col].toString());
            item->setToolTip("原表："+sources[row].toString()+"\n"+item->text());
            if (col == 0 || col == 6) { item->setBackground(QColor("#fff1c9")); }
            rows_->setItem(static_cast<int>(row),static_cast<int>(col),item);
        }
    }
    rows_->resizeRowsToContents();
    week_->setRange(1,draft_["semesterWeeks"].toInt());
    refreshWeek();
    status_->setText(kReviewReminder);
    export_->setEnabled(!data.empty());
}
void TimetableConverterWidget::refreshWeek() {
    timetable_->clearContents();
    // 使用明细中的实际教师，而非 CSV 中合并后的共同授课教师字段。
    for (const auto& value : draft_["entries"].toArray()) {
        const auto entry = value.toObject();
        bool active = false;
        for (const auto& week : entry["weeks"].toArray()) { if (week.toInt() == week_->value()) { active = true; break; } }
        if (!active) { continue; }
        const int row = entry["period"].toInt()-1;
        const int col = entry["weekday"].toInt()-1;
        auto text = entry["course"].toString()+"\n"+entry["teacher"].toString()+"\n"+entry["room"].toString();
        auto tooltip = entry["source"].toString()+"\n"+text;
        const auto* previous = timetable_->item(row,col);
        if (previous) {
            text = previous->text()+"\n"+text+"\n时间冲突";
            tooltip = previous->toolTip()+"\n"+tooltip;
        }
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setToolTip(tooltip);
        item->setBackground(QColor(previous ? "#fbe4e4" : "#dcecf8"));
        timetable_->setItem(row,col,item);
    }
}
void TimetableConverterWidget::saveDraft(const QString& path) const {
    const auto info = QFileInfo(path);
    const auto write = [](const QString& target,const QString& text) {
        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly)) { throw runtime_error(file.errorString().toStdString()); }
        file.write(QByteArray("\xEF\xBB\xBF")+text.toUtf8());
        if (!file.commit()) { throw runtime_error(file.errorString().toStdString()); }
    };
    write(path,draft_["csv"].toString());
    // 界面只保留一条提醒，完整审核明细仍随 CSV 导出到独立文件。
    write(info.dir().filePath(info.completeBaseName()+".审核说明.txt"),draft_["review"].toString());
}
void TimetableConverterWidget::exportCsv() {
    QFileDialog dialog(this,"导出待人工审核的 CSV 草稿");
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter("CSV 草稿 (*.csv)");
    dialog.setDefaultSuffix("csv");
    dialog.selectFile(QFileInfo(source_->text()).completeBaseName()+"_待审核.csv");
    if (dialog.exec() != QDialog::Accepted) { return; }
    try {
        saveDraft(dialog.selectedFiles().front());
        status_->setText(kReviewReminder);
    } catch (const exception& error) {
        QMessageBox::warning(this,"导出失败",QString::fromUtf8(error.what()));
    }
}
}
