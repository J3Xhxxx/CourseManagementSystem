#include "ui/ProjectController.h"
#include "io/JsonProjectStore.h"
#include <QFileInfo>

using namespace std;

namespace planner::ui {
ProjectController::ProjectController(QObject* parent) : QObject(parent) {}
void ProjectController::markModified() { modified_ = true; emit dataChanged(); emit changed(); }
void ProjectController::newProject(int weeks) {
    project_ = TeachingClassService(weeks);
    filePath_.clear();
    modified_ = false;
    emit dataChanged();
    emit changed();
}
void ProjectController::openProject(const QString& path) {
    // 读取和校验完成后才赋值，打开失败时保留原数据。
    project_ = JsonProjectStore::load(path);
    filePath_ = QFileInfo(path).absoluteFilePath();
    modified_ = false;
    emit dataChanged();
    emit changed();
}
void ProjectController::saveProject(const QString& path) {
    JsonProjectStore::save(path, project_);
    filePath_ = QFileInfo(path).absoluteFilePath();
    modified_ = false;
    emit changed();
}
ImportSummary ProjectController::importCsv(const QString& path) {
    const auto summary = CsvImporter::importFile(path, project_);
    if (summary.addedTeachingClasses || summary.addedMeetings) { markModified(); }
    return summary;
}
void ProjectController::putTeachingClass(const QString& oldId, const TeachingClass& teachingClass) {
    if (oldId.isEmpty()) { project_.addTeachingClass(teachingClass); }
    else { project_.updateTeachingClass(oldId.toStdString(), teachingClass); }
    markModified();
}
void ProjectController::removeTeachingClass(const QString& id) {
    project_.removeTeachingClass(id.toStdString());
    markModified();
}
void ProjectController::setConstraints(UserConstraints constraints) {
    if (constraints == project_.constraints()) { return; }
    project_.setConstraints(move(constraints));
    markModified();
}
}
