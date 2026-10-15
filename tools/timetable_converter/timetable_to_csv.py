import argparse
import csv
import io
import json
from pathlib import Path
import re
import sys

import xlrd


HEADERS = ["课程代码", "课程名称", "授课教师", "周次", "上课时间", "教室", "学分", "是否为固定课程"]
DAYS = ["周一", "周二", "周三", "周四", "周五", "周六", "周日"]
PERIODS = {f"{2 * period - 1}-{2 * period}": period for period in range(1, 7)}
PUNCTUATION = str.maketrans({"，": ",", "［": "[", "］": "]", "－": "-", "–": "-", "—": "-"})
# 教师与周次：周次写在方括号内，末尾的“周”可有可无（[1-5,7-9周]、[1-5,7-9] 都要识别）。
TEACHER_WEEKS = re.compile(r"([^\n\[\]]+?)\s*\[([^\]\n]+?)\]\s*周?")
ROOM = re.compile(r"([A-Za-z\u4e00-\u9fff]+)-?(\d+[A-Za-z]?)")
# 学校不同版本导出用不同方式分隔同一课格里的多条记录：
# 换行、◇/◆ 附注符号，或 <br>/</br>/&lt;br&gt; 之类的标签。
# 全部换成换行，使每条记录各占一行；注意要先把 <br> 变成换行，
# 再删除其余标签，否则“N楼-218计算机网络”会被拼成一条记录。
RECORD_SEPARATORS = re.compile(r"[\r\n]+|◇|◆|<br\s*/?>|</br\s*>|&lt;br\s*/?&gt;|&lt;/br\s*&gt;", re.IGNORECASE)

def clean(text):
    # 先把全角标点归一成半角，再做分隔符替换：学校导出常把周次写成［1-5，7-9周］，
    # 若等到 parse_cell 才转换，TEACHER_WEEKS 就匹配不到全角方括号。
    text = RECORD_SEPARATORS.sub("\n", str(text)).translate(PUNCTUATION)
    return re.sub(r"<[^>]*>", "", text).strip()

def parse_weeks(expression):
    # 周次可能带结尾的“周”，例如 [1-5,7-9周]；这里统一去掉再解析。
    expression = expression.strip()
    if expression.endswith("周"):
        expression = expression[:-1]
    weeks = set()
    for part in expression.translate(PUNCTUATION).split(","):
        match = re.fullmatch(r"\s*(\d+)\s*(?:-\s*(\d+))?\s*", part)
        if not match:
            raise ValueError(f"无法识别周次：{expression}")
        first = int(match[1])
        last = int(match[2] or match[1])
        if first < 1 or last < first or last > 104:
            raise ValueError(f"周次超出 1～104 或范围倒序：{expression}")
        weeks.update(range(first, last + 1))
    return sorted(weeks)

def weeks_text(weeks):
    ranges = []
    first = last = weeks[0]
    for week in weeks[1:]:
        if week == last + 1:
            last = week
        else:
            ranges.append(str(first) if first == last else f"{first}-{last}")
            first = last = week
    ranges.append(str(first) if first == last else f"{first}-{last}")
    return ",".join(ranges) + "周"

def normalize_room(text):
    text = text.translate(PUNCTUATION).strip(" ,")
    match = ROOM.fullmatch(text)
    if not match:
        raise ValueError(f"无法识别教室：{text or '缺少教室'}")
    building = re.sub(r"(?:教学楼|楼)$", "", match[1])
    return building + match[2]

def parse_cell(text):
    """课程标题关联后续教师/周次；同一标题下的分周教师可以共用末尾教室。"""
    lines = [line.strip().translate(PUNCTUATION) for line in clean(text).splitlines() if line.strip()]
    records = []
    index = 0
    while index < len(lines):
        course = lines[index]
        if "[" in course or ROOM.fullmatch(course):
            raise ValueError(f"缺少课程名称：{course}")
        index += 1
        if index == len(lines):
            raise ValueError(f"{course} 缺少教师和周次")
        clauses = list(TEACHER_WEEKS.finditer(lines[index]))
        if not clauses or clauses[0].start() != 0:
            raise ValueError(f"{course} 的教师/周次无法识别：{lines[index]}")
        details = lines[index]
        index += 1
        rooms = []
        for number, clause in enumerate(clauses):
            end = clauses[number + 1].start() if number + 1 < len(clauses) else len(details)
            suffix = details[clause.end():end].strip(" ,")
            rooms.append(normalize_room(suffix) if suffix else "")
        if any(not room for room in rooms):
            if index == len(lines):
                raise ValueError(f"{course} 缺少教室")
            shared_room = normalize_room(lines[index])
            index += 1
            rooms = [room or shared_room for room in rooms]
        for clause, room in zip(clauses, rooms):
            teacher = clause[1].strip(" ,")
            if not teacher:
                raise ValueError(f"{course} 缺少教师")
            records.append({"course": course, "teacher": teacher, "weeks": parse_weeks(clause[2]), "room": room})
    return records

def merge_courses(entries):
    """单个学生的固定课表按名称归并，分周教师仍属于同一个教学班。

    主体 CSV 导入仍按代码、名称、教师识别教学班，此处是学校课表的专用规则。
    """
    classes = {}
    for entry in entries:
        teaching_class = classes.setdefault(entry["course"], {"teachers": [], "slots": {}, "sources": []})
        if entry["teacher"] not in teaching_class["teachers"]:
            teaching_class["teachers"].append(entry["teacher"])
        if entry["source"] not in teaching_class["sources"]:
            teaching_class["sources"].append(entry["source"])
        # 相同星期、大节和教室可合并周次；实际教师仍保留在原始 entries 中。
        slot = (entry["weekday"], entry["period"], entry["room"])
        teaching_class["slots"].setdefault(slot, set()).update(entry["weeks"])

    rows, sources = [], []
    for course, teaching_class in classes.items():
        arrangements = {}
        for (day, period, room), weeks in teaching_class["slots"].items():
            arrangements.setdefault((tuple(sorted(weeks)), room), []).append((day, period))
        week_parts, time_parts, room_parts = [], [], []
        for (weeks, room), times in arrangements.items():
            week_parts.append(weeks_text(weeks))
            time_parts.append("[" + ",".join(f"({DAYS[day-1]},{period})" for day, period in sorted(times)) + "]")
            room_parts.append(room)
        # 同名课只输出一行，三个分号分段逐项对应，不能把周次和时段分别求并集。
        rows.append(["", course, "、".join(teaching_class["teachers"]), ";".join(week_parts),
                     ";".join(time_parts), ";".join(room_parts), "", "是"])
        sources.append(", ".join(teaching_class["sources"]))
    return rows, sources

def convert(path, sheet_name=None):
    book = xlrd.open_workbook(file_contents=Path(path).read_bytes(), formatting_info=True)
    try:
        sheet = book.sheet_by_name(sheet_name) if sheet_name else book.sheet_by_index(0)
        columns = {}
        for header_row in range(sheet.nrows):
            columns = {}
            for column in range(sheet.ncols):
                label = clean(sheet.cell_value(header_row, column))
                for day, short in enumerate(DAYS, 1):
                    if label in (short, "星期" + short[1]):
                        columns[column] = day
            if len(columns) == 7:
                break
        else:
            raise ValueError("未找到完整的周一至周日表头")

        entries, issues, ignored_rows = [], [], []
        first_day_column = min(columns)
        found_periods = set()
        for row in range(header_row + 1, sheet.nrows):
            # 按约定跳过第 9 行，不能把“其它课程”文字猜成正式上课安排。
            if row == 8:
                ignored_rows.append(row + 1)
                continue
            labels = [clean(sheet.cell_value(row, col)) for col in range(first_day_column)]
            if any(label.startswith(("其它课程", "其他课程")) for label in labels):
                ignored_rows.append(row + 1)
                continue
            period = None
            for label in labels:
                normalized = re.sub(r"[第节\s]", "", label.translate(PUNCTUATION))
                if normalized in PERIODS:
                    period = PERIODS[normalized]
                    break
            if period is None:
                if any(clean(sheet.cell_value(row, col)) for col in columns):
                    issues.append(f"第 {row + 1} 行没有可识别的小节标签，未转换，请人工核对。")
                continue
            found_periods.add(period)
            for col, day in columns.items():
                text = clean(sheet.cell_value(row, col))
                if not text:
                    continue
                address = xlrd.formula.colname(col) + str(row + 1)
                if any(r0 <= row < r1 and c0 <= col < c1 and (r1-r0 > 1 or c1-c0 > 1)
                       for r0, r1, c0, c1 in sheet.merged_cells):
                    issues.append(f"{address} 是跨课格的合并单元格，未自动展开，请人工填写：\n{text}")
                    continue
                try:
                    records = parse_cell(text)
                except ValueError as error:
                    issues.append(f"{address}：{error}\n原文：{text}")
                    continue
                for record in records:
                    entries.append({**record, "weekday": day, "period": period, "source": address})
        if not found_periods:
            raise ValueError("未找到 1-2 至 11-12 节的课表行")

        rows, sources = merge_courses(entries)
        if not entries:
            issues.append("没有解析出上课安排；请核对课格格式。")
        return {"sheet": sheet.name, "headers": HEADERS, "rows": rows, "sources": sources,
                "entries": entries, "issues": issues, "ignoredRows": ignored_rows,
                "semesterWeeks": max(20, max((max(entry["weeks"]) for entry in entries), default=20)),
                "courseCount": len(rows)}
    finally:
        book.release_resources()

def csv_text(result):
    output = io.StringIO(newline="")
    writer = csv.writer(output, lineterminator="\r\n")
    writer.writerow(result["headers"])
    writer.writerows(result["rows"])
    return output.getvalue()

def review_text(result):
    lines = [f"工作表：{result['sheet']}", "课程代码、学分留空，必须手工补齐；固定课程标记预填“是”。",
             "第 9 行其它课程未读取、未结构化；请对照原表人工补充。",
             "按课程名称归并，同名课程只输出一行，视为同一个学校固定教学班。",
             "周次、上课时间、教室用英文分号分段，三个字段的第 N 段共同表示一组上课安排。",
             "相同星期、大节和教室合并周次；相同周次和教室合并时段，不扩大实际上课范围。"]
    lines.append("同一基础固定课程的教师合并为共同授课字段；以下保留各周实际教师。")
    if result["issues"]:
        lines.extend(["", "未识别内容（需要人工处理）：", *result["issues"]])
    lines.extend(["", "原表上课安排："])
    for entry in result["entries"]:
        lines.append(f"{entry['source']}：{entry['course']} / {entry['teacher']} / {weeks_text(entry['weeks'])} / "
                     f"{DAYS[entry['weekday']-1]}第{entry['period']}大节 / {entry['room']}")
    return "\n".join(lines) + "\n"

def main():
    parser = argparse.ArgumentParser(description="将学校 .xls 总课表转换为需人工补齐的 CSV 草稿，不导入选课项目。")
    parser.add_argument("input", type=Path)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("-o", "--output", type=Path)
    target.add_argument("--preview-json", action="store_true", help="向标准输出返回 GUI 预览数据，不生成文件")
    parser.add_argument("--sheet", help="工作表名称，默认读取第一张表")
    args = parser.parse_args()
    try:
        result = convert(args.input, args.sheet)
        result["csv"] = csv_text(result)
        result["review"] = review_text(result)
        if args.preview_json:
            print(json.dumps(result, ensure_ascii=False))
        else:
            args.output.write_text(result["csv"], encoding="utf-8-sig", newline="")
            report = args.output.with_suffix(".审核说明.txt")
            report.write_text(result["review"], encoding="utf-8-sig")
            print(f"已生成 {len(result['rows'])} 条 CSV 草稿，{result['courseCount']} 门课程。")
            print(f"CSV：{args.output}\n审核说明：{report}")
            print("课程代码和学分待填写，第 9 行课程待人工补充；未导入项目。")
            if result["issues"]:
                print(f"另有 {len(result['issues'])} 处未识别内容，见审核说明。")
    except (OSError, ValueError, xlrd.XLRDError) as error:
        print(f"转换失败：{error}", file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    raise SystemExit(main())
