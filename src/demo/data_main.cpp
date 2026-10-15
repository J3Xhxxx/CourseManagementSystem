#include "io/CsvImporter.h"
#include "io/JsonProjectStore.h"
#include <QCoreApplication>
#include <QTextStream>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

using namespace planner;

namespace {
void describe(const TeachingClassService& project, QTextStream& out) {
    out << "学期 " << project.semesterWeeks() << " 周；课程 " << project.courseKeys().size()
        << " 门；教学班 " << project.teachingClasses().size() << " 个；冲突边 " << project.graph().edgeCount() << " 条\n";
    for (const auto& teachingClass : project.teachingClasses()) {
        out << QString::fromStdString(teachingClass.code()) << " / " << QString::fromStdString(teachingClass.name())
            << " / " << QString::fromStdString(teachingClass.teacher()) << "：" << teachingClass.meetings().size()
            << " 条上课安排，" << teachingClass.credits() << " 学分"
            << (teachingClass.isFixed() ? "（学校固定）" : "") << '\n';
        for (const auto& edge : project.graph().neighbors(teachingClass.id())) {
            if (teachingClass.id() >= edge.neighborId) { continue; }
            const auto& other = *project.findTeachingClass(edge.neighborId);
            out << "  与 " << QString::fromStdString(other.name())
                << " / " << QString::fromStdString(other.teacher()) << " 不能同时选择：";
            for (auto reason : edge.detail.reasons) {
                out << (reason == ConflictReason::TimeOverlap ? "时间重叠 " : "同课程互斥 ");
            }
            out << '\n';
            for (const auto& slot : edge.detail.overlapSlots) {
                out << "    第 " << slot.week << " 周，星期 " << slot.weekday << "，第 " << slot.period << " 节\n";
            }
        }
    }
}
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    QCoreApplication app(argc, argv);
    QTextStream out(stdout), error(stderr);
    out.setEncoding(QStringConverter::Utf8);
    error.setEncoding(QStringConverter::Utf8);
    const auto args = app.arguments();
    try {
        if (args.size() == 3 && args[1] == "inspect") {
            describe(JsonProjectStore::load(args[2]), out);
        } else if ((args.size() == 3 && args[1] == "check") || (args.size() == 4 && args[1] == "import") ||
                   (args.size() == 5 && args[1] == "merge")) {
            auto project = args[1] == "merge" ? JsonProjectStore::load(args[3]) : TeachingClassService{};
            const auto summary = CsvImporter::importFile(args[2], project);
            out << "读取 " << summary.rows << " 行，新增 " << summary.addedTeachingClasses << " 个教学班、" << summary.addedMeetings << " 条安排；跳过重复 "
                << summary.skippedDuplicates << " 条安排\n";
            if (args[1] != "check") {
                JsonProjectStore::save(args.back(), project);
                out << "已保存：" << args.back() << '\n';
            }
            describe(project, out);
        } else {
            error << "用法：course_data check CSV\n"
                  << "      course_data import CSV OUTPUT.json\n"
                  << "      course_data merge CSV INPUT.json OUTPUT.json\n"
                  << "      course_data inspect INPUT.json\n";
            return 2;
        }
    } catch (const exception& exception) {
        error << QString::fromUtf8(exception.what()) << '\n';
        return 1;
    }
    return 0;
}
