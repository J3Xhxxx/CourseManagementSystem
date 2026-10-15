# 第六阶段：动态 MRV 与 Forward Checking（2026-10-01，v0.7）

本文记录当时的实现。2026-10-07 已增加学校固定初始课表，CSV 改为上课时间数组，JSON 改为 version 4；当前模型与格式以 [TEACHING_CLASS_MODEL.md](TEACHING_CLASS_MODEL.md) 和 [DATA_FORMAT.md](DATA_FORMAT.md) 为准。2026-10-15 已加入方案评价、偏好排序与 Top-K 大根堆（JSON 升为 version 5），当前方案数上限的语义与本文不同，以 [STAGE7.md](STAGE7.md) 为准。

> 2026-10-09 已移除自动化测试源码和构建入口，性能工具仍可独立运行。下面的节点对照来自本阶段原有样例。

界面中的“生成方案”默认使用 MRV＋Forward Checking。模型继续以 TeachingClass 为单位，课程代码＋名称只生成互相替代教学班的分组键。必选集合、锁定班、禁止时间，以及 JSON version 3 的字段规则保持有效。

## 文件与职责

| 文件 | 本轮职责 |
| --- | --- |
| src/core/algorithms/ScheduleGenerator.h/.cpp | 策略选项、动态 MRV、候选域恢复、搜索上限及统计 |
| src/ui/CandidatePanel.cpp | 展示候选排除、无后续选择及既有搜索统计 |
| src/demo/benchmark_main.cpp | 四类固定场景、四种算法方案数与搜索统计、CSV 性能输出 |
| CMakeLists.txt、src/app/main.cpp、src/demo/main.cpp | 基准目标与程序版本 |
| README.md、docs/GUI_GUIDE.md | 使用方法和阶段状态 |

## 搜索策略

`SearchOptions::strategy` 提供三种策略，用于正确性与性能比较：

| 策略 | 下一组如何选择 | 如何处理未来候选 |
| --- | --- | --- |
| Backtracking | 按项目顺序取下一组 | 尝试时与当前占用位图检查冲突 |
| Mrv | 每层重新计算合法候选数，优先选最少的一组 | 用当前前缀重新扫描各组候选 |
| MrvForwardChecking（默认） | 按维护的剩余候选数动态选择最少的一组 | 选择一个班后，提前禁用其他未选组中与它冲突的候选；任一组变空立即回溯 |

MRV 在每个搜索状态执行，并非开始时只排序一次。候选数相同时按原项目分组顺序决定；组内班按录入顺序尝试，因此同一输入重复运行有稳定结果。每个输出 Schedule 的 ID 列表仍按原分组顺序排列。搜索发现方案的顺序可能与旧算法不同；本阶段结果未按软偏好排序，2026-10-15 起改为按偏好优先级输出（见 [STAGE7.md](STAGE7.md)）。

三种策略共用硬约束前置过滤、直接锁定冲突诊断和上限处理。`BruteForceGenerator` 保持独立：枚举原始笛卡尔积，再调用独立验证器逐个检查。

```cpp
SearchOptions options; // 默认 MrvForwardChecking
options.maxSolutions = 100;
options.maxNodes = 100000;
auto result = ScheduleGenerator::generate(project, options);

options.strategy = SearchStrategy::Backtracking; // 性能基线
auto baseline = ScheduleGenerator::generate(project, options);
```

## 状态恢复

候选列表保持固定位置，通过 active 标记及 remaining 计数维护候选域。每次真正禁用一个候选，记录 `(domain, value)` 到恢复栈；每个赋值保存栈标记。回溯只恢复标记之后的删除，已由父层排除的候选不会被子层恢复。候选可在恢复后的兄弟分支再次被排除。

搜索写成 `Searcher` 类中的递归函数 `dfs(depth)`，选中班按分组索引保存在 `selected_` 中。占用位图只保留一份：选中的班与原占用不相交，回溯时用 `subtract` 减去它的时间即可还原。整个搜索状态属于单次调用，取消及达到上限均不会修改项目。

每次尝试一个候选前检查一次取消请求。节点上限统计实际赋值尝试，被提前排除的候选不计为节点。达到精确节点上限后仍可执行回退及耗尽判断；只有需要继续尝试才返回 NodeLimit。方案上限仍额外探测一个合法叶子：恰好 K 个结果时可返回 Complete，发现第 K＋1 个时返回 SolutionLimit。

## 统计口径

| 字段 | 含义 |
| --- | --- |
| visitedNodes | 实际尝试给一组分配一个教学班的次数 |
| prunedBranches | 直接时间冲突、MRV 发现空域或 Forward Checking 发现空域造成的失败分支 |
| candidateRemovals | 累计提前禁用次数；恢复后再次禁用会再次计数 |
| domainWipeouts | Forward Checking 使未选分组的候选域变空的次数 |
| completeLeaves | 搜索到的完整组合数，可能比保存数多 1 |
| elapsedMilliseconds | 包含候选准备、约束诊断和搜索；不包含项目构建、GUI 快照复制和绘制 |

界面用“提前排除候选”“无后续选择”显示新增统计。删除次数不等于剪枝次数：排除几个候选后仍有其他可选班时，该分支继续搜索。

除输出结果外，搜索状态约占 `O(M + C × M)`：M 为过滤后的教学班数，C 为必选分组数；包括候选标记、恢复栈和递归各层的候选下标列表。本轮没有测量完整进程内存，也未纳入冲突图构建耗时；最坏情况的组合搜索仍是指数级。

## 可重复性能对照

`course_benchmark` 按需输出 CSV，直接运行可执行文件即可。每个样例运行暴力枚举、普通回溯、MRV、MRV＋Forward Checking；输出方案数、节点数、完整组合检查数、剪枝次数、耗时，以及方案数是否与暴力枚举一致（same_as_brute）。工具独立运行，不参与构建脚本。

```powershell
./scripts/build.ps1 -Configuration Release
./build-qt-Release/course_benchmark.exe | Out-File build-qt-Release/stage6-benchmark.csv -Encoding utf8
```

2026-10-01 本机 Release 的实际赋值节点数：

| 固定样例 | 合法方案 | 原始完整组合数 | 普通回溯 | MRV | MRV＋FC |
| --- | ---: | ---: | ---: | ---: | ---: |
| 6 组各 3 个候选，组间无冲突 | 729 | 729 | 1092 | 1092 | 1092 |
| 末尾单候选组与第一组全部冲突 | 0 | 729 | 1821 | 1 | 1 |
| 末尾单候选组仅允许第一组一个候选 | 243 | 729 | 1821 | 365 | 365 |
| 5 组争用 4 个时间位置，无法全部选入 | 0 | 1024 | 260 | 64 | 64 |

MRV 与 MRV＋FC 在这些样例中赋值节点数相同，前者重算合法候选，后者维护域；最后一个样例中前向检查提前发现 24 次空域。无冲突输入没有节点缩减，域维护有额外开销；单次毫秒计时容易受到系统调度影响，不能据此宣称所有输入都会更快。这些一周的小规模合成样例不是大规模真实性能结论。

## 测试与后续

当前不保留自动化测试源码或测试目标。算法改动时按需临时核对时间模型、硬约束、回溯恢复和小规模枚举结果，完成后清理检查代码及产物。

`course_benchmark` 现输出 `case,strategy,solutions,kept,visited_nodes,complete_combinations,pruned_branches,elapsed_ms,same_as_brute`：`solutions` 是合法方案总数，`kept` 是 Top-K 保留数；比较算法正确性应使用 `solutions`。

下一阶段建议（已由 [STAGE7.md](STAGE7.md) 完成）：按真实周次计算早课、上课天数和空堂等指标，再加入可配置字典序排序与 Top-K 保存；完整搜索和受上限限制的结果使用不同的结论措辞。
