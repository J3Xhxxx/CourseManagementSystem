# 改动说明

> 基准版本：仓库当前 HEAD（v0.8，JSON version 4，尚未实现方案评价与 Top-K）。

## PR 标题

```
feat: 方案评价指标、偏好优先级排序与 Top-K 大根堆（v0.9）
```

## PR 描述

### 背景与目标

原实现的搜索已经能枚举出全部合法选课组合，但结果按“发现顺序”排列、全部留在内存里，任务书第四阶段要求的**方案评价、偏好排序、Top-K 维护**尚未实现。本次改动补齐这三项，并保证现有功能与既有格式约定不被破坏。

### 主要改动

**1. 新增方案评价体系（六项指标，按真实周次统计）**

- 新增 `ScheduleEvaluator`，逐周扫描方案的时间位集合，统计早课次数、上课天数、空堂节数、最大连续上课节数、最晚结束节次、空闲半天数；
- 明确“不把整个学期叠加成一张课表”的统计口径，并把每个指标的方向固定（前五项越小越好，空闲半天数越多越好）；
- 指标随方案保存（`Schedule::evaluation`），并额外提供一行文本供界面显示。

**2. 新增偏好优先级与字典序排序**

- `PreferenceOrder` 保存六项指标的一个排列（必须恰好六项、不重复），用户只调整先后顺序；
- `ScheduleRanking` 按优先级做字典序比较，全部指标相同的方案再按教学班 ID 序列兜底，保证全序、同一份输入结果可重复；
- 偏好随项目保存（JSON constraints 的 `preferences`）。

**3. 新增固定容量大根堆，实现 Top-K 保留**

- 新增 `BoundedMaxHeap`：比较器规定“较差者在上”，堆顶是当前保留元素中最差的一个，堆满后只需与新方案比较一次即可决定替换；
- 搜索不再“达到方案数上限就停止”，改为**搜索跑完后保留全局最优的前 K 个**；新增 `totalSolutions` 记录合法方案总数，并区分“搜索完整跑完”与“达到尝试上限/被取消”。

**4. 独立校验器增加指标复核**

- `ScheduleValidator` 在校验合法性之外，用评价器独立重算六项指标并与方案保存值比对，不一致报 `MetricsMismatch`；同时约定“任何构造方案的地方先评价、再校验”（暴力枚举基线同步调整）。

**5. 界面补上评价与偏好**

- “选课约束”页签新增“方案偏好优先级”列表（上移/下移/恢复默认）；
- 候选方案列表与详情显示六项指标、当前排序优先级与实际合法方案总数；
- 状态措辞区分“已全部显示 / 只保留了较优的前 K 个 / 搜索未完成不能视为全局最优”。

**6. 数据格式与工具链适配**

- JSON 版本升为 5，`constraints` 增加必需的 `preferences` 数组；
- `course_benchmark` 性能输出增加 `solutions`（合法总数）与 `kept`（保留数）两列；
- **修复** `CsvImporter` 拒绝“课程代码/学分为空”的课表转换草稿，使“转换 → 人工补齐 → 导入”链路可用（空学分按 0、空代码暂用课程名称占位）；
- **修复** 学校课表转换器 `timetable_to_csv.py` 无法解析新导出格式（详见“修复的缺陷”）。

### 修复的缺陷（都是实测发现的）

| 问题 | 原因 | 处理 |
| --- | --- | --- |
| 大根堆丢掉更优方案 | 下沉比较写成“子结点之间比较后直接与父结点交换”，替换分支缺少提前返回 | 重写为“每层选出更优的孩子，只有它优于当前元素才交换”；用“每次插入后与暴力求前 K 个对照”的不变量测试验证 |
| 空闲半天数恒为很小的值 | 按“跨周去重的星期×半天”统计，实际应按“每一周×每天”统计 | 改为逐周统计，上限 W×7×2；用两个手工算例核对（278 / 279） |
| Top-K 与暴力枚举顺序不一致 | `sortByPreference` 用 `stable_sort`，与堆使用的全序不同 | 统一使用同一个含 ID 兜底的全序 |
| 评价指标复核使暴力枚举返回 0 个方案 | 基线候选在“校验之后”才计算指标，被判为指标过期 | 调整为“先评价、再校验” |
| 班级课表转换得 0 门课程 | 新导出用 `</br>` 与 `◇` 分隔同一课格的多条记录，而代码先把 `<...>` 整段删除，导致教室与下一门课被拼在一起 | 把 `<br>/<br/>/</br>/◇/◆` 统一当作记录分隔符 |
| 含逗号的周次永远匹配失败 | 教师/周次正则把逗号排除在名字之外，且要求方括号后紧跟“周” | 改为方括号内可选结尾“周”，并让周次解析容忍结尾“周”字 |
| 全角周次 `［1-5，7-9周］` 不识别 | 标点归一发生在正则匹配之后 | 标点归一提前到单元格清洗阶段 |
| 转换草稿无法导入 | 导入器把空学分判为“格式无效”、空代码判为“必填为空” | 空学分按 0 处理，空代码用课程名称占位（界面补齐代码时会同步更新 ID 与约束引用） |

## 文件级改动清单

### 新增

| 文件 | 说明 |
| --- | --- |
| `src/core/algorithms/ScheduleEvaluator.h/.cpp` | 六项指标定义与统计、指标标签、偏好顺序校验、字典序比较器、按偏好排序 |
| `src/core/structures/BoundedMaxHeap.h` | 固定容量二叉堆，只保留较优的前 K 个元素 |
| `src/core/model/Schedule.cpp` | `scheduleMetricsText`：一行指标文本 |
| `docs/STAGE7.md` | 第七阶段说明：指标口径与手工算例、排序与堆设计、验证结果、后续可做项 |

### 修改（核心）

| 文件 | 说明 |
| --- | --- |
| `src/core/model/Schedule.h` | 新增 `ScheduleMetrics` 与 `Schedule::evaluation` |
| `src/core/model/UserConstraints.h/.cpp` | 新增 `preferenceOrder`，参与相等比较与校验 |
| `src/core/algorithms/ScheduleGenerator.h/.cpp` | 每方案评价后入堆；新增 `totalSolutions`；`complete()/globallyRanked()`；`SolutionLimit` 语义调整 |
| `src/core/algorithms/ScheduleValidator.h/.cpp` | 新增 `MetricsMismatch` 指标复核 |
| `src/core/algorithms/BruteForceGenerator.cpp` | 先评价再校验 |
| `src/demo/benchmark_main.cpp` | 输出增加 `solutions`、`kept` 列 |
| `src/demo/main.cpp`、`src/app/main.cpp` | 版本号 0.9 |
| `CMakeLists.txt` | 加入 `Schedule.cpp`、`ScheduleEvaluator.cpp`，工程版本 0.9.0 |

### 修改（界面 / IO / 工具）

| 文件 | 说明 |
| --- | --- |
| `src/ui/ConstraintPanel.h/.cpp` | 偏好优先级列表（上移/下移/恢复默认） |
| `src/ui/CandidatePanel.cpp` | 方案显示六项指标、排序优先级、合法方案总数与状态措辞 |
| `src/io/JsonProjectStore.h/.cpp` | JSON version 5，读写 `constraints.preferences` |
| `src/io/CsvImporter.cpp` | 允许空学分、空课程代码的转换草稿导入 |
| `tools/timetable_converter/timetable_to_csv.py` | 记录分隔符、标点归一、教师/周次正则、周次结尾“周” |
| `examples/选课项目.json` | 升为 version 5 并补 `preferences` |

### 文档

`README.md`、`PROJECT_BRIEF.md`、`docs/DATA_FORMAT.md`（version 4 → 5）、`docs/GUI_GUIDE.md`（新增偏好优先级章节）、`docs/CODE_EXPLANATION.md`（新增第 7、8 节：评价器与大根堆）、`docs/TEACHING_CLASS_MODEL.md`、`docs/STAGE5.md`、`docs/STAGE6.md`。

## 测试与验证

**编译**：Qt 6 全量构建通过（MSYS2 Qt 6.11.2 + g++ 16.2，开启 `-Wall -Wextra -Wpedantic -Wconversion`），5 个可执行文件全部生成：`course_app`、`course_data`、`course_demo`、`course_benchmark`。核心层另可用 `-CoreOnly` 独立编译。

**算法验证**（临时检查程序，验证后已按仓库约定删除，不在仓库保留自动化测试）：手工指标算例、空堂算例、堆不变量（保留最大/最小 1、3 个及容量大于元素数）、偏好改变最优解、Top-K 与暴力枚举全局前 5 逐个一致、节点上限声明未完成、方案数少于 K 报告完整、指标被篡改能被独立检出 —— 全部通过。

**性能对照**（`course_benchmark`，4 组固定场景）：四种方式合法方案数完全一致（`same_as_brute` 均为 yes）；瓶颈无解场景普通回溯 1821 个节点、MRV 与 MRV+前向检查各 1 个节点；瓶颈有解场景 1821 → 365（约 −80%）；鸽巢场景 260 → 64。

**真实数据验证**：学校《2404101 班级课表》导出件经转换器得到 8 门课程（另有 1 处体育条目提示人工核对），补齐代码学分后导入共 9 门课程、20 条上课安排；独立检查程序复核：合法方案 1 个、独立校验器 0 问题、固定课表跨课程时段重叠 0 处；第 1 周 9 个上课时段、第 11 周 14 个，佐证必须按真实周次统计。

**回归**：原有样例 `examples/courses.csv`（5 门课 9 条安排）、`examples/fixed_courses.csv`、`examples/官网课表_待审核.csv`、`examples/search_demo.csv` 导入与搜索行为不变；旧样例课表 `examples/导出课表示例.xls` 转换结果一致。

## 建议的提交拆分

```
feat(eval): 新增 ScheduleEvaluator，按真实周次统计六项方案指标
feat(eval): 新增偏好优先级 PreferenceOrder 与字典序比较器
feat(search): 新增 BoundedMaxHeap，改为搜索跑完后保留全局最优前 K 个
feat(validate): 校验器独立复核方案评价指标
feat(ui): 选课约束增加偏好优先级设置，候选方案展示指标与排序
feat(io): JSON 升为 version 5 并保存 preferences
fix(io): 允许课程代码/学分为空的课表转换草稿导入
fix(converter): 适配学校课表新导出格式（记录分隔符、全角标点、含逗号周次）
docs: 新增 STAGE7 并同步 README/GUI/DATA_FORMAT/CODE_EXPLANATION
chore: 版本号 0.9.0，性能工具输出 solutions/kept 两列
```

## 需要注意的事项

- **不兼容变更**：JSON 只接受 version 5，version 4 项目会被拒绝（这是仓库原有的“只支持当前版本、不做迁移”约定的延续），需要按新 CSV 重新导入或手工补 `preferences` 字段。
- **`SolutionLimit` 语义变化**：以前表示“达到方案上限、搜索未完成”，现在表示“搜索已完整跑完、合法方案多于 K，保留了较优的前 K 个”；界面与文档已同步。
- **`ScheduleMetrics` 的位置**：定义放在 `core/model/Schedule.h`（方案自带指标），评价与排序逻辑放在 `core/algorithms/ScheduleEvaluator.*`，避免 model 依赖 algorithms。
- **本次未包含**（避免超出任务书范围）：单日最多/最少课程量指标、无解诊断增强（逐项放宽约束重搜）、冲突图增量更新、可选拓展“尽量少改动原课表”。
