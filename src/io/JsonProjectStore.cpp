#include "io/JsonProjectStore.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <set>
#include <stdexcept>

using namespace std;

namespace planner {
namespace {
[[noreturn]] void invalid(const QString& path, const QString& message) {
    throw invalid_argument((path + QStringLiteral(": ") + message).toStdString());
}

QJsonObject object(const QJsonValue& value, const QStringList& keys, const QString& path) {
    if (!value.isObject()) { invalid(path, QStringLiteral("应为对象")); }
    const auto result = value.toObject();
    for (const auto& key : keys) {
        if (!result.contains(key)) { invalid(path, QStringLiteral("缺少字段 ") + key); }
    }
    return result;
}

QString jsonString(const QJsonValue& value, const QString& path) {
    if (!value.isString()) { invalid(path, QStringLiteral("应为字符串")); }
    return value.toString();
}

QJsonArray array(const QJsonValue& value, const QString& path) {
    if (!value.isArray()) { invalid(path, QStringLiteral("应为数组")); }
    return value.toArray();
}

int integer(const QJsonValue& value, const QString& path) {
    // 不是整数时 toInt 返回默认值 0，与非正数一并报错。
    const int number = value.toInt(0);
    if (number < 1) { invalid(path, QStringLiteral("应为正整数")); }
    return number;
}
}

QByteArray JsonProjectStore::serialize(const TeachingClassService& project) {
    // 只保存原始安排和用户约束；位图、冲突图和搜索结果都由这些数据重建。
    QJsonArray teachingClasses;
    for (const auto& teachingClass : project.teachingClasses()) {
        QJsonArray meetings;
        for (const auto& meeting : teachingClass.meetings()) {
            QJsonArray weeks;
            for (int week : meeting.weeks()) { weeks.append(week); }
            meetings.append(QJsonObject{{"weeks", weeks}, {"weekday", meeting.weekday()},
                {"startPeriod", meeting.startPeriod()}, {"endPeriod", meeting.endPeriod()},
                {"classroom", QString::fromStdString(meeting.classroom())}});
        }
        teachingClasses.append(QJsonObject{{"code", QString::fromStdString(teachingClass.code())},
            {"name", QString::fromStdString(teachingClass.name())}, {"credits", teachingClass.credits()},
            {"teacher", QString::fromStdString(teachingClass.teacher())}, {"isFixed", teachingClass.isFixed()}, {"meetings", meetings}});
    }
    QJsonArray requiredCourseKeys;
    for (const auto& id : project.constraints().requiredCourseKeys) {
        requiredCourseKeys.append(QString::fromStdString(id));
    }
    QJsonArray lockedTeachingClassIds;
    for (const auto& id : project.constraints().lockedTeachingClassIds) {
        lockedTeachingClassIds.append(QString::fromStdString(id));
    }
    QJsonArray forbiddenTimes;
    for (const auto& time : project.constraints().forbiddenTimes.occupiedSlots()) {
        forbiddenTimes.append(QJsonObject{{"week", time.week}, {"weekday", time.weekday}, {"period", time.period}});
    }
    const QJsonObject constraints{{"requiredCourseKeys", requiredCourseKeys}, {"lockedTeachingClassIds", lockedTeachingClassIds},
        {"forbiddenTimes", forbiddenTimes}};
    return QJsonDocument(QJsonObject{{"version", formatVersion}, {"semesterWeeks", project.semesterWeeks()},
        {"teachingClasses", teachingClasses}, {"constraints", constraints}}).toJson(QJsonDocument::Indented);
}

TeachingClassService JsonProjectStore::deserialize(const QByteArray& json) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError) {
        invalid(QStringLiteral("JSON 第 %1 字节").arg(error.offset), error.errorString());
    }
    if (!document.isObject()) { invalid("项目", "根节点应为对象"); }
    const int version = integer(document.object()["version"], "version");
    if (version != formatVersion) { invalid("version", "不支持的项目文件版本"); }
    const auto root = object(document.object(), {"version", "semesterWeeks", "teachingClasses", "constraints"}, "project");
    TeachingClassService project(integer(root["semesterWeeks"], "semesterWeeks"));
    vector<TeachingClass> teachingClasses;
    const auto entries = array(root["teachingClasses"], "teachingClasses");
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const auto path = QStringLiteral("teachingClasses[%1]").arg(i);
        const auto entry = object(entries[i], {"code", "name", "credits", "teacher", "isFixed", "meetings"}, path);
        if (!entry["credits"].isDouble()) { invalid(path + ".credits", "应为数字"); }
        if (!entry["isFixed"].isBool()) { invalid(path + ".isFixed", "应为是或否"); }
        try {
            const auto meetingArray = array(entry["meetings"], path + ".meetings");
            vector<Meeting> meetings;
            for (qsizetype m = 0; m < meetingArray.size(); ++m) {
                const auto mp = path + QStringLiteral(".meetings[%1]").arg(m);
                const auto meeting = object(meetingArray[m], {"weeks", "weekday", "startPeriod", "endPeriod", "classroom"}, mp);
                vector<int> weeks;
                for (const auto& week : array(meeting["weeks"], mp + ".weeks")) { weeks.push_back(integer(week, mp + ".weeks")); }
                meetings.emplace_back(move(weeks), integer(meeting["weekday"], mp + ".weekday"),
                    integer(meeting["startPeriod"], mp + ".startPeriod"), integer(meeting["endPeriod"], mp + ".endPeriod"),
                    jsonString(meeting["classroom"], mp + ".classroom").toStdString());
            }
            teachingClasses.emplace_back(jsonString(entry["code"], path + ".code").toStdString(),
                jsonString(entry["name"], path + ".name").toStdString(), entry["credits"].toDouble(),
                jsonString(entry["teacher"], path + ".teacher").toStdString(), move(meetings), project.semesterWeeks(), entry["isFixed"].toBool());
        } catch (const exception& e) { invalid(path, QString::fromUtf8(e.what())); }
    }
    project.replaceTeachingClasses(move(teachingClasses));
    const auto entry = object(root["constraints"], {"requiredCourseKeys", "lockedTeachingClassIds", "forbiddenTimes"}, "constraints");
    UserConstraints constraints(project.semesterWeeks());
    const auto readIds = [&](const char* field, set<string>& ids) {
        const auto path = QStringLiteral("constraints.") + QString::fromLatin1(field);
        const auto values = array(entry[field], path);
        for (qsizetype i = 0; i < values.size(); ++i) {
            ids.insert(jsonString(values[i], path + QStringLiteral("[%1]").arg(i)).toStdString());
        }
    };
    readIds("requiredCourseKeys", constraints.requiredCourseKeys);
    readIds("lockedTeachingClassIds", constraints.lockedTeachingClassIds);
    const auto times = array(entry["forbiddenTimes"], "constraints.forbiddenTimes");
    for (qsizetype i = 0; i < times.size(); ++i) {
        const auto path = QStringLiteral("constraints.forbiddenTimes[%1]").arg(i);
        const auto time = object(times[i], {"week", "weekday", "period"}, path);
        constraints.forbiddenTimes.set({integer(time["week"], path + ".week"), integer(time["weekday"], path + ".weekday"),
            integer(time["period"], path + ".period")});
    }
    project.setConstraints(move(constraints));
    return project;
}

void JsonProjectStore::save(const QString& path, const TeachingClassService& project) {
    const auto bytes = serialize(project);
    QSaveFile file(path);
    // commit 成功前保留原文件，避免写入中断造成已有项目损坏。
    // 写入失败时 commit 也会失败，只需检查打开和提交两步。
    if (!file.open(QIODevice::WriteOnly)) { throw runtime_error(file.errorString().toStdString()); }
    file.write(bytes);
    if (!file.commit()) { throw runtime_error(file.errorString().toStdString()); }
}

TeachingClassService JsonProjectStore::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { throw runtime_error(file.errorString().toStdString()); }
    return deserialize(file.readAll());
}

} // namespace planner
