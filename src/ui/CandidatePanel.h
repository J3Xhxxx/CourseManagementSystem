#pragma once
#include "ui/SearchController.h"
#include <QWidget>
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QPlainTextEdit;
namespace planner::ui {
class CandidatePanel : public QWidget {
    Q_OBJECT
public:
    CandidatePanel(ProjectController& project, SearchController& search, QWidget* parent = nullptr);
    void generate();
signals:
    void candidateSelected(int index); // -1 表示清除方案预览。
private:
    void updateState();
    void updateResults();
    void showCandidate(int index);
    ProjectController& project_;
    SearchController& search_;
    QPushButton* generate_;
    QPushButton* cancel_;
    QPushButton* previous_;
    QPushButton* next_;
    QSpinBox* maxSolutions_;
    QSpinBox* maxNodes_;
    QLabel* status_;
    QLabel* statistics_;
    QPlainTextEdit* diagnostics_;
    QLabel* details_;
    QListWidget* list_;
};
}
