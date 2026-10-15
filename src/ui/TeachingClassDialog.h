#pragma once
#include "core/model/TeachingClass.h"
#include <QDialog>
#include <optional>
class QLineEdit;
class QLabel;
class QTableWidget;
class QCheckBox;
namespace planner::ui {
using namespace std;

class TeachingClassDialog : public QDialog {
    Q_OBJECT
public:
    TeachingClassDialog(int semesterWeeks, const TeachingClass* initial = nullptr, QWidget* parent = nullptr);
    const TeachingClass& value() const { return value_.value(); }
    void accept() override;
private:
    void addMeeting(const Meeting* meeting = nullptr);
    QLineEdit *code_, *name_, *credits_;
    int weeks_;
    QLineEdit* teacher_;
    QCheckBox* fixed_;
    QLabel* error_;
    QTableWidget* table_;
    optional<TeachingClass> value_;
};
}
