#pragma once
#include "core/services/TeachingClassService.h"
#include <QWidget>
class QLineEdit;
class QTreeWidget;
namespace planner::ui {
class TeachingClassPanel : public QWidget {
    Q_OBJECT
public:
    explicit TeachingClassPanel(QWidget* parent = nullptr);
    void setProject(const TeachingClassService& project);
    QString teachingClassId() const;
    bool selectTeachingClass(const QString& id);
signals:
    void selectionChanged();
    void editRequested();
private:
    void applyFilter();
    QLineEdit* search_;
    QTreeWidget* tree_;
};
}
