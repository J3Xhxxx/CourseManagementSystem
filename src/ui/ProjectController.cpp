#include "ui/ProjectController.h"
#include "io/JsonProjectStore.h"
#include <QDir>
#include <QFileInfo>
#include <stdexcept>

using namespace std;

namespace planner::ui {
ProjectController::ProjectController(const QString& accountName, const QString& accountsDirectory, QObject* parent)
    : QObject(parent), accountName_(accountName),
      accountFilePath_(QDir(accountsDirectory).absoluteFilePath(accountName + ".json")) {
    openOwnProject();
}
void ProjectController::requireWritable() const {
    if (readOnly_) { throw runtime_error("他人的课表只读，请返回“我的课表”后修改和保存。"); }
}
void ProjectController::markModified() { modified_ = true; emit dataChanged(); emit changed(); }
void ProjectController::newProject(int weeks) {
    requireWritable();
    project_ = TeachingClassService(weeks);
    // 新建仍属于当前账号；保存后用空项目替换自己原来的课表。
    markModified();
}
void ProjectController::openOwnProject() {
    project_ = QFileInfo::exists(accountFilePath_) ? JsonProjectStore::load(accountFilePath_) : TeachingClassService{};
    filePath_ = accountFilePath_;
    readOnly_ = false;
    modified_ = false;
    emit dataChanged();
    emit changed();
}
void ProjectController::openProject(const QString& path) {
    // 读取和校验完成后才赋值，打开失败时保留原数据。
    project_ = JsonProjectStore::load(path);
    filePath_ = QFileInfo(path).absoluteFilePath();
    readOnly_ = QFileInfo(filePath_) != QFileInfo(accountFilePath_);
    modified_ = false;
    emit dataChanged();
    emit changed();
}
void ProjectController::saveProject() {
    requireWritable();
    if (!QDir().mkpath(QFileInfo(accountFilePath_).absolutePath())) {
        throw runtime_error("无法创建账号课表目录。");
    }
    // 不接受另存路径，防止查看他人课表时覆盖文件。
    JsonProjectStore::save(accountFilePath_, project_);
    modified_ = false;
    emit changed();
}
ImportSummary ProjectController::importCsv(const QString& path) {
    requireWritable();
    const auto summary = CsvImporter::importFile(path, project_);
    if (summary.addedTeachingClasses || summary.addedMeetings) { markModified(); }
    return summary;
}
void ProjectController::putTeachingClass(const QString& oldId, const TeachingClass& teachingClass) {
    requireWritable();
    if (oldId.isEmpty()) { project_.addTeachingClass(teachingClass); }
    else { project_.updateTeachingClass(oldId.toStdString(), teachingClass); }
    markModified();
}
void ProjectController::removeTeachingClass(const QString& id) {
    requireWritable();
    project_.removeTeachingClass(id.toStdString());
    markModified();
}
void ProjectController::setConstraints(UserConstraints constraints) {
    requireWritable();
    if (constraints == project_.constraints()) { return; }
    project_.setConstraints(move(constraints));
    markModified();
}
}
