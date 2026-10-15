#pragma once
#include "io/CsvImporter.h"
#include <QObject>

namespace planner::ui {

// 持有当前项目和修改状态；界面通过 ID 访问教学班，增删改后重新查询。
class ProjectController : public QObject {
    Q_OBJECT
public:
    explicit ProjectController(QObject* parent = nullptr);
    const TeachingClassService& project() const noexcept { return project_; }
    bool modified() const noexcept { return modified_; }
    const QString& filePath() const noexcept { return filePath_; }
    void newProject(int weeks);
    void openProject(const QString& path);
    void saveProject(const QString& path);
    ImportSummary importCsv(const QString& path);
    void putTeachingClass(const QString& oldId, const TeachingClass& teachingClass);
    void removeTeachingClass(const QString& id);
    void setConstraints(UserConstraints constraints);

signals:
    void changed();
    void dataChanged(); // 数据成功变化才发出；保存仅更新文件名和修改状态。

private:
    void markModified();
    TeachingClassService project_;
    QString filePath_;
    bool modified_ = false;
};
}
