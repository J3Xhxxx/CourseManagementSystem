#pragma once
#include <QJsonObject>
#include <QWidget>
class QLabel;
class QPushButton;
class QProcess;
class QSpinBox;
class QTableWidget;

namespace planner::ui {
// 独立调用 Python 预览和导出草稿，不持有选课项目，也不自动导入。
class TimetableConverterWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimetableConverterWidget(QWidget* parent = nullptr);
    ~TimetableConverterWidget() override;
    void previewFile(const QString& path);
    void saveDraft(const QString& path) const;
private:
    void receiveResult(int exitCode);
    void refreshWeek();
    void exportCsv();
    QPushButton* open_;
    QPushButton* export_;
    QLabel* source_;
    QLabel* status_;
    QSpinBox* week_;
    QTableWidget* timetable_;
    QTableWidget* rows_;
    QProcess* process_;
    QJsonObject draft_;
};
}
