#pragma once
#include "core/services/TeachingClassService.h"
#include <QWidget>
class QLabel;
class QTableWidget;
namespace planner::ui {
class ConflictPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConflictPanel(QWidget* parent = nullptr);
    void setTeachingClass(const TeachingClassService& project, const QString& teachingClassId);
signals:
    void teachingClassActivated(const QString& id);
private:
    QLabel* summary_;
    QTableWidget* table_;
};
}
