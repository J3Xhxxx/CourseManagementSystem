#pragma once
#include "ui/ProjectController.h"
#include <QWidget>
#include <QList>
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
namespace planner::ui {
using namespace std;

class ConstraintPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConstraintPanel(ProjectController& project, QWidget* parent = nullptr);
private:
    void refresh();
    void setRequired(const string& courseKey, bool required);
    void setLocked(const string& courseKey, const string& teachingClassId);
    void editForbidden(bool add);
    void removeSelectedForbidden();
    void movePreference(int offset);
    void resetPreference();
    void updatePreferenceButtons();
    void apply(UserConstraints constraints);
    ProjectController& project_;
    QTableWidget* courses_;
    QTableWidget* forbidden_;
    QLineEdit* weeks_;
    QComboBox* pattern_;
    QComboBox* weekday_;
    QSpinBox* start_;
    QSpinBox* end_;
    QLabel* summary_;
    QLabel* error_;
    QList<QWidget*> inputs_;
    QPushButton* remove_;
    QListWidget* preference_;
    QPushButton* preferenceUp_;
    QPushButton* preferenceDown_;
    QPushButton* preferenceReset_;
    vector<vector<TimeSlot>> groups_;
    int semesterWeeks_ = 0;
};
}
