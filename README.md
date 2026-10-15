# 个人选课组合与冲突分析系统

使用 C++17、Qt 6 Widgets 和 CMake，当前版本 v0.8。

系统按教学班录入课程信息，支持一周多次上课、学校固定初始课表、时间冲突分析和合法选课组合生成。搜索采用 DFS、动态 MRV 和 Forward Checking；可以设置必选课程、个人锁定教学班及禁止时间。方案评价、偏好排序和 Top-K 尚未实现。

## 构建和运行

直接双击根目录的 `启动选课系统.bat` 即可打开界面。入口优先启动 Release，没有 Release 时启动 Debug；启动出错时窗口会保留错误信息。入口支持传递启动脚本参数，例如 `启动选课系统.bat -ImportCsv examples/courses.csv`。首次使用前需按下方命令完成构建。

在项目根目录执行：

```powershell
# 默认构建 Release 并部署运行库
./scripts/build.ps1

# 启动界面，或导入样例后启动
./scripts/run.ps1
./scripts/run.ps1 -ImportCsv examples/courses.csv
# 固定课程与一周多次上课示例
./scripts/run.ps1 -ImportCsv examples/fixed_courses.csv

# 需要调试时显式构建、启动 Debug
./scripts/build.ps1 -Configuration Debug
./scripts/run.ps1 -Configuration Debug

# 只构建不依赖 Qt 的核心部分
./scripts/build.ps1 -CoreOnly
```

其他机器需要 CMake 3.24+、Ninja、Qt 6.2+（Core、Widgets）以及匹配的 C++17 编译器。本机脚本可通过 `-QtPrefix` 指定 Qt 安装目录。启动时会自动补齐缺失的运行库，也可重新运行 `./scripts/build.ps1` 完成构建和部署。

## 使用方法

“新增教学班”一次填写课程代码、名称、教师、学分、固定课程标记和多条上课安排。左侧支持筛选与编辑，中间按实际周次显示课表，右侧提供候选方案、选课约束和冲突详情。

学校固定课程加入后不能编辑、删除、取消必选或换班；初始课表及每个生成方案都保留这些课。其他课程的所有上课安排都会与固定课表比较。固定课程之间、固定课程与个人锁定或禁止时间有冲突时，会明确报告无解。

在“选课约束”中勾选参与搜索的课程，每门恰选一个教学班。可以锁定教师对应的班，并禁止具体周次、星期和节次。点击“生成方案”后，可切换查看多个结果；达到搜索上限或取消时，会明确显示搜索未完成。数据或约束修改后，需要重新生成方案。

CSV 的“上课时间”使用数组，如 `[(周一,1),(周四,3)]`；“是否为固定课程”必须填写“是”或“否”。多组安排可在同一行的“周次、上课时间、教室”三列用英文分号逐段对应，也可分行录入同一教学班并归并。

CSV 样例和转换结果使用 UTF-8 BOM。用表格软件修改后，请按“CSV UTF-8”格式保存，以保持中文显示和导入编码一致。

JSON 用于保存整个项目，当前只支持 version 4，每个教学班保存 `isFixed`。开发期旧格式不兼容；旧数据请按新 CSV 格式重新导入。文件格式见 [DATA_FORMAT.md](docs/DATA_FORMAT.md)，界面说明见 [GUI_GUIDE.md](docs/GUI_GUIDE.md)。

各按钮、菜单、下拉选项及禁用条件的逐项说明见 [界面按钮与菜单说明.docx](docs/界面按钮与菜单说明.docx)。

```powershell
./build-qt-Release/course_data.exe check examples/courses.csv
./build-qt-Release/course_data.exe import examples/courses.csv build-qt-Release/example-project.json
./build-qt-Release/course_data.exe inspect build-qt-Release/example-project.json
```

## 学校总课表转换

中央“课表转换”页签可选择学校导出的 `.xls`，按课程名称归并为一课一班、一行 CSV，保留各周实际教师的预览及审核说明。课程代码、学分留空，固定标记预填“是”，第 9 行其它课程不解析；请手工补齐、审核后再导入。转换器独立运行，不改变当前项目或搜索条件。

```powershell
# 首次使用时安装 Python 依赖；本机已安装
./scripts/setup-converter.ps1
./.tools/timetable-converter/Scripts/python.exe tools/timetable_converter/timetable_to_csv.py "导出课表示例.xls" --output "examples/官网课表_待审核.csv"
```

详细规则及操作见 [TIMETABLE_CONVERTER.md](docs/TIMETABLE_CONVERTER.md)。

## 模型和目录

TeachingClass 包含课程代码、名称、学分、教师、固定课程标记及多个 Meeting。代码＋名称＋教师确定教学班身份；代码＋名称用于分组，不单独建立 Course 对象。每次 Meeting 记录周次、星期、起止节次和教室。

时间索引为 `(week-1)*7*6 + (weekday-1)*6 + (period-1)`。每天六个时段，20 周共 840 位；时间冲突通过位集合交集判断，并反向解析冲突位置。

```text
src/core/model/       数据模型和周次解析
src/core/structures/  时间位集合、冲突图
src/core/algorithms/  冲突检测、搜索、独立验证、暴力枚举
src/core/services/    教学班管理、索引和约束维护
src/io/              CSV、JSON
src/ui/              Qt 窗口、编辑和展示组件
src/app/             程序入口
src/demo/            控制台演示、数据工具和性能对照
scripts/             构建、启动、共用工具链和转换器环境初始化
tools/timetable_converter/  独立学校课表转换脚本
```

模型接口见 [TEACHING_CLASS_MODEL.md](docs/TEACHING_CLASS_MODEL.md)，搜索设计见 [STAGE6.md](docs/STAGE6.md)。

`scripts/` 只保留以下四个文件。数据检查、性能对照和命令行课表转换直接调用各自程序，不再另设包装脚本；控制台演示可按需运行 `./build-qt-Release/course_demo.exe`。

| 文件 | 职责 |
| --- | --- |
| build.ps1 | 配置、编译、部署运行库；默认 Release，支持 Debug 和 CoreOnly |
| run.ps1 | 启动界面，可预先导入 CSV；缺少运行库时自动补齐 |
| toolchain.ps1 | 共用的 Qt/MinGW 查找和运行库部署函数，由构建、启动脚本加载 |
| setup-converter.ps1 | 为独立课表转换器创建 Python 环境、安装 xlrd |

## 性能对照与手动检查

构建脚本只负责配置、编译和部署。后续验证按需使用现有工具或临时检查代码，临时源码及产物验证后删除。

任务书要求比较不同算法，因此另保留一个性能工具，输出方案数、节点数、完整组合检查数、剪枝次数和耗时：

```powershell
# 先构建 Release；性能工具单独运行
./build-qt-Release/course_benchmark.exe | Out-File build-qt-Release/benchmark.csv -Encoding utf8
```

功能修改后手动检查：教学班增删改、按周显示、固定课程、锁定与禁止时间、方案生成和切换、项目保存与重新打开。CSV 可使用上面的 `course_data check` 命令检查；性能输出用于比较各策略的方案数和搜索工作量。
