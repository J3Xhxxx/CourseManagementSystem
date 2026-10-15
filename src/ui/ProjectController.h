#pragma once
#include "io/CsvImporter.h"
#include <QObject>

namespace planner::ui {

// 持有登录者的文件路径和当前查看的数据；只有自己的课表允许修改、保存。
class ProjectController : public QObject {
    Q_OBJECT
public:
    ProjectController(const QString& accountName, const QString& accountsDirectory, QObject* parent = nullptr);
    const TeachingClassService& project() const noexcept { return project_; }
    bool modified() const noexcept { return modified_; }
    const QString& filePath() const noexcept { return filePath_; }
    const QString& accountName() const noexcept { return accountName_; }
    const QString& accountFilePath() const noexcept { return accountFilePath_; }
    bool readOnly() const noexcept { return readOnly_; }
    void newProject(int weeks);
    void openOwnProject();
    void openProject(const QString& path);
    void saveProject();
    ImportSummary importCsv(const QString& path);
    void putTeachingClass(const QString& oldId, const TeachingClass& teachingClass);
    void removeTeachingClass(const QString& id);
    void setConstraints(UserConstraints constraints);

signals:
    void changed();
    void dataChanged(); // 数据成功变化才发出；保存仅更新修改状态。

private:
    void requireWritable() const;
    void markModified();
    TeachingClassService project_;
    QString accountName_;
    QString accountFilePath_;
    QString filePath_;
    bool modified_ = false;
    bool readOnly_ = false;
};
}
