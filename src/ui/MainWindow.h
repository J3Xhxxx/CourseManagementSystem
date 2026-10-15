#pragma once
#include "ui/ProjectController.h"
#include "ui/SearchController.h"
#include <QMainWindow>
#include <functional>
class QAction;
class QLabel;
class QTabWidget;
namespace planner::ui {
using namespace std;

class TeachingClassPanel;
class TimetableWidget;
class ConflictPanel;
class CandidatePanel;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ProjectController& controller() noexcept { return controller_; }
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void refresh();
    void showSelection();
    void showCandidate(int index);
    void newProject();
    void openProject();
    bool saveProject(bool saveAs = false);
    bool maybeSave();
    void importCsv();
    void editTeachingClass(bool create);
    void editSelection();
    void removeSelection();
    bool execute(const function<void()>& operation);
    ProjectController controller_;
    SearchController search_;
    TeachingClassPanel* teachingClasses_;
    TimetableWidget* timetable_;
    QTabWidget* timetableTabs_;
    ConflictPanel* conflicts_;
    CandidatePanel* candidates_;
    QTabWidget* tabs_;
    bool viewingCandidate_ = false;
    QLabel* totals_;
    QAction* edit_;
    QAction* remove_;
};
}
