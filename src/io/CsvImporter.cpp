#include "io/CsvImporter.h"
#include <QFile>
#include <QHash>
#include <QStringConverter>
#include <QStringList>
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

using namespace std;

namespace planner {
namespace {
[[noreturn]] void fail(qsizetype line, const QString& message) {
    throw invalid_argument(QStringLiteral("CSV 第 %1 行：%2").arg(line).arg(message).toStdString());
}

struct Record { qsizetype line; QStringList fields; };

// 引号内允许逗号、换行和双写引号；错误位置使用原文件的物理行号。
vector<Record> parseCsv(QString text) {
    if (text.startsWith(QChar(0xfeff))) { text.remove(0, 1); }
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));
    vector<Record> rows;
    QStringList fields;
    QString field;
    bool quoted = false, closed = false, touched = false;
    qsizetype line = 1, startLine = 1;
    const auto endField = [&] { fields.push_back(field); field.clear(); closed = false; };
    const auto endRow = [&] {
        endField();
        if (touched) { rows.push_back({startLine, fields}); }
        fields.clear(); touched = false;
    };
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto c = text[i];
        if (quoted) {
            if (c == QChar('"')) {
                if (i + 1 < text.size() && text[i + 1] == QChar('"')) { field += c; ++i; }
                else { quoted = false; closed = true; }
            } else {
                field += c;
                if (c == QChar('\n')) { ++line; }
            }
            continue;
        }
        if (c == QChar(',')) { touched = true; endField(); }
        else if (c == QChar('\n')) { endRow(); ++line; startLine = line; }
        else if (closed) { fail(line, QStringLiteral("结束引号后只能接逗号或换行")); }
        else if (c == QChar('"')) {
            if (!field.isEmpty()) { fail(line, QStringLiteral("引号必须位于字段开头")); }
            quoted = true; touched = true;
        } else { field += c; touched = true; }
    }
    if (quoted) { fail(startLine, QStringLiteral("带引号的字段未结束")); }
    if (touched || !fields.empty()) { endRow(); }
    return rows;
}

int integer(const QString& input) {
    bool ok = false;
    const int value = input.trimmed().toInt(&ok);
    if (!ok || value < 1) { throw invalid_argument("必须填写正整数"); }
    return value;
}

int weekday(const QString& input) {
    const auto text = input.trimmed();
    const QStringList names{QStringLiteral("一"),QStringLiteral("二"),QStringLiteral("三"),QStringLiteral("四"),QStringLiteral("五"),QStringLiteral("六"),QStringLiteral("日")};
    for (int i = 0; i < 7; ++i) {
        if (text == QStringLiteral("周") + names[i] || text == QStringLiteral("星期") + names[i]) { return i + 1; }
    }
    if (text == QStringLiteral("星期天") || text == QStringLiteral("周天")) { return 7; }
    return integer(text);
}

vector<Meeting> parseMeetings(const QString& input, const vector<int>& weeks, const string& classroom) {
    // 每个 (星期,节次) 对生成一条 Meeting，共用本段的周次和教室。
    const auto text = input.trimmed();
    if (!text.startsWith('[') || !text.endsWith(']')) {
        throw invalid_argument("上课时间须为数组，如 [(周一,1),(周四,3)]");
    }
    auto remaining = text.mid(1, text.size()-2).trimmed();
    const QRegularExpression pair(QStringLiteral(R"(^\(\s*([^,()]+),\s*([^,()]+)\))"));
    vector<Meeting> meetings;
    while (!remaining.isEmpty()) {
        const auto match = pair.match(remaining);
        if (!match.hasMatch()) { throw invalid_argument("每项上课时间须为 (星期,节次)"); }
        const auto periods = match.captured(2).trimmed().split('-');
        if (periods.size() > 2) { throw invalid_argument("节次须写成单个数字或起止区间，如 1 或 1-2"); }
        meetings.emplace_back(weeks, weekday(match.captured(1)), integer(periods.front()), integer(periods.back()), classroom);
        remaining = remaining.mid(match.capturedLength()).trimmed();
        if (remaining.isEmpty()) { break; }
        if (!remaining.startsWith(',')) { throw invalid_argument("上课时间数组项之间须用逗号分隔"); }
        remaining = remaining.mid(1).trimmed();
        if (remaining.isEmpty()) { throw invalid_argument("上课时间数组不能以逗号结尾"); }
    }
    if (meetings.empty()) { throw invalid_argument("上课时间数组不能为空"); }
    return meetings;
}

size_t appendUniqueMeetings(vector<Meeting>& target, const vector<Meeting>& incoming,
                                size_t& skippedDuplicates) {
    const auto originalSize = target.size();
    for (const auto& meeting : incoming) {
        if (find(target.begin(), target.end(), meeting) != target.end()) { ++skippedDuplicates; }
        else { target.push_back(meeting); }
    }
    return target.size() - originalSize;
}
}

ImportSummary CsvImporter::importFile(const QString& path, TeachingClassService& target) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { throw runtime_error(file.errorString().toStdString()); }
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString csv = decoder(file.readAll());
    if (decoder.hasError()) { throw invalid_argument("CSV 必须使用有效的 UTF-8 编码"); }
    return importText(csv, target);
}

ImportSummary CsvImporter::importText(const QString& csv, TeachingClassService& target) {
    const auto rows = parseCsv(csv);
    if (rows.empty()) { fail(1, QStringLiteral("缺少表头")); }
    const QStringList required{QStringLiteral("课程代码"),QStringLiteral("课程名称"),QStringLiteral("授课教师"),QStringLiteral("周次"),QStringLiteral("上课时间"),QStringLiteral("教室"),QStringLiteral("学分"),QStringLiteral("是否为固定课程")};
    const auto& header = rows.front().fields;
    QHash<QString, qsizetype> columns;
    for (qsizetype i = 0; i < header.size(); ++i) {
        const auto key = header[i].trimmed();
        if (columns.contains(key) || (!required.contains(key) && key != QStringLiteral("单双周"))) {
            fail(rows.front().line, QStringLiteral("重复或未知表头：") + key);
        }
        columns.insert(key, i);
    }
    for (const auto& key : required) {
        if (!columns.contains(key)) { fail(rows.front().line, QStringLiteral("缺少表头：") + key); }
    }

    // 先在副本中完成所有行的校验和归并，任意一行失败都不改动当前项目。
    auto teachingClasses = target.teachingClasses();
    unordered_map<string, size_t> teachingClassIndex;
    for (size_t i = 0; i < teachingClasses.size(); ++i) { teachingClassIndex.emplace(teachingClasses[i].id(), i); }
    ImportSummary summary;
    for (size_t i = 1; i < rows.size(); ++i) {
        const auto& row = rows[i];
        if (row.fields.size() != header.size()) { fail(row.line, QStringLiteral("字段数量与表头不一致；上课时间数组、含逗号的周次需加双引号")); }
        const auto get = [&](const char* key) { return row.fields[columns.value(QString::fromUtf8(key))].trimmed(); };
        try {
            bool ok = false;
            const double credits = get("学分").toDouble(&ok);
            if (!ok) { throw invalid_argument("学分格式无效"); }
            const auto fixedText = get("是否为固定课程");
            if (fixedText != QStringLiteral("是") && fixedText != QStringLiteral("否")) {
                throw invalid_argument("是否为固定课程只能填写 是 或 否");
            }
            const bool isFixed = fixedText == QStringLiteral("是");
            WeekPattern pattern = WeekPattern::EveryWeek;
            if (columns.contains(QStringLiteral("单双周"))) {
                const auto value = get("单双周");
                if (value == QStringLiteral("单周")) { pattern = WeekPattern::OddWeek; }
                else if (value == QStringLiteral("双周")) { pattern = WeekPattern::EvenWeek; }
                else if (!value.isEmpty() && value != QStringLiteral("每周")) { throw invalid_argument("单双周只能填写每周、单周或双周"); }
            }
            const auto weekParts = get("周次").split(';');
            const auto timeParts = get("上课时间").split(';');
            const auto roomParts = get("教室").split(';');
            if (weekParts.size() != timeParts.size() || weekParts.size() != roomParts.size()) {
                throw invalid_argument("周次、上课时间、教室的分号分段数量必须一致");
            }
            vector<Meeting> rowMeetings;
            // 同一行可描述多组安排；每段只使用对应的周次和教室，避免扩大时间范围。
            for (qsizetype part = 0; part < weekParts.size(); ++part) {
                const auto incoming = parseMeetings(timeParts[part],
                    parseWeeks(weekParts[part].trimmed().toStdString(), target.semesterWeeks(), pattern),
                    roomParts[part].trimmed().toStdString());
                appendUniqueMeetings(rowMeetings, incoming, summary.skippedDuplicates);
            }
            TeachingClass teachingClass(get("课程代码").toStdString(), get("课程名称").toStdString(), credits, get("授课教师").toStdString(), move(rowMeetings), target.semesterWeeks(), isFixed);
            const auto existingTeachingClass = teachingClassIndex.find(teachingClass.id());
            if (existingTeachingClass == teachingClassIndex.end()) {
                summary.addedMeetings += teachingClass.meetings().size();
                teachingClassIndex.emplace(teachingClass.id(), teachingClasses.size()); teachingClasses.push_back(move(teachingClass));
                ++summary.addedTeachingClasses;
            } else {
                if (teachingClasses[existingTeachingClass->second].credits() != credits) {
                    throw invalid_argument("同一教学班的学分不一致");
                }
                if (teachingClasses[existingTeachingClass->second].isFixed() != isFixed) {
                    throw invalid_argument("同一教学班的固定课程标记不一致");
                }
                auto meetings = teachingClasses[existingTeachingClass->second].meetings();
                // 只跳过完全相同的安排；部分时间重叠仍由 TeachingClass 拒绝。
                const auto added = appendUniqueMeetings(meetings, teachingClass.meetings(), summary.skippedDuplicates);
                if (added != 0) {
                    summary.addedMeetings += added;
                    teachingClasses[existingTeachingClass->second] = TeachingClass(teachingClass.code(), teachingClass.name(), credits, teachingClass.teacher(), move(meetings), target.semesterWeeks(), isFixed);
                }
            }
            ++summary.rows;
        } catch (const exception& error) { fail(row.line, QString::fromUtf8(error.what())); }
    }
    target.replaceTeachingClasses(move(teachingClasses));
    return summary;
}

} // namespace planner
