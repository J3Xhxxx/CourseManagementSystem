> 本文保留对应阶段的历史记录。当前已移除 Course 实体，以 [TEACHING_CLASS_MODEL.md](TEACHING_CLASS_MODEL.md) 的教学班模型及接口为准。

> 2026-10-09 已移除自动化测试源码和构建入口。下文测试统计属于历史记录，当前构建和检查方法见 [README.md](../README.md)。

# 第四轮：基础组合生成

本阶段实现固定必选课程集合的基础搜索、独立验证、完整枚举基线，以及方案生成和浏览。当前项目中的每门课程都必选，每门恰选一个教学班。选择一个教学班就选择它的全部 Meeting。

## 文件与职责

```text
src/core/model/Schedule.h
src/core/algorithms/
    ScheduleGenerator.h/.cpp
    ScheduleValidator.h/.cpp
    BruteForceGenerator.h/.cpp
src/ui/
    SearchController.h/.cpp
    CandidatePanel.h/.cpp
    TimetableWidget.h/.cpp
examples/search_demo.csv
```

| 类 | 职责 |
| --- | --- |
| Schedule | 保存所选 TeachingClass ID 与全学期占用位图；不缓存可能失效的 validity 标志 |
| ScheduleGenerator | 固定顺序 DFS、即时冲突剪枝、显式栈回溯、上限及取消、统计 |
| ScheduleValidator | 独立检查教学班存在性、重复 ID、课程覆盖及互斥、时间冲突、位图一致性 |
| BruteForceGenerator | 以混合进制枚举笛卡尔积，在完整组合处校验；仅用于小规模正确性对照 |
| SearchController | 在工作线程中运行项目快照，接收取消、丢弃旧项目结果，销毁前等待线程结束 |
| CandidatePanel | 参数、生成/取消、结果状态、方案列表与前后切换 |
| TimetableWidget | 单班预览及完整方案按真实周次显示；每格展示课程、教师和教室 |

ProjectController 新增 dataChanged 信号：成功编辑、导入新记录、新建或打开项目会触发，保存和完全重复导入不会触发。既有 changed 信号继续用于文件名和修改标记刷新。

## 搜索与状态恢复

课程和教学班按 CourseService 中的稳定顺序搜索，暂不重新排序。每次尝试一个教学班，计算它的缓存位图与当前占用的交集；有交集就剪枝，否则进入下一层。

显式 DFS 栈保存每层的下一个候选位置、已选 ID 和该层占用位图副本。回溯弹出当前层，父层位图不被修改，因此多个 Meeting 的时间能一起选择和一起恢复。这里先用副本换取清晰的正确性，后续优化必须继续与基线对照。

时间索引仍为：

```text
index = (week - 1) * 7 * 6 + (weekday - 1) * 6 + (period - 1)
```

位图冲突检查/合并的复杂度为 O(B)，B = ceil(W×42/64)。设每门课有 m_i 个班，最坏仍为指数枚举；保存搜索前缀额外需要 O(N×B) 位图机器字，保存 K 个方案需要 O(K×(N+B))。冲突图仍用于界面的详细解释，本轮 DFS 直接使用时间位图。

## 独立验证

验证器不调用搜索器、冲突图、ConflictDetector 或 TeachingClass::timeBitmap。它直接遍历 Meeting 中的周次、星期和闭区间节次，以 (week, weekday, period) 元组建立占用表，检测不同选中班的重叠。错误以 ValidationCode、相关 ID 和冲突时间点返回。

验证器供暴力基线使用：完全枚举笛卡尔积后逐个调用验证器过滤。搜索器自身通过前置过滤和位图冲突检查保证方案合法，不再对每个叶子重复验证；正确性通过与暴力基线的结果对照来检查。

## 有界搜索语义

SearchOptions 默认最多保留 100 个方案、最多尝试 100000 次教学班赋值。

| SearchStatus | 含义 |
| --- | --- |
| Complete | 全部搜索树已遍历，或发现某必选课程根本没有教学班 |
| SolutionLimit | 已保留 K 个方案，并又发现第 K+1 个合法方案，余下搜索停止 |
| NodeLimit | 已尝试规定数量的赋值，仍有分支未处理 |
| Cancelled | 检测到取消请求，保留已经完整验证的方案 |

到达保留上限后会继续探测一个合法叶子，因此恰好只有 K 个方案且遍历结束时可报告 Complete；如果探测途中先达到节点上限，则报告 NodeLimit。没有返回方案且搜索未完成时，不能推断无解。方案按搜索顺序显示，未进行评价、排序或 Top-K 优化。

visitedNodes 统计尝试教学班的次数，包含被剪枝的尝试；prunedBranches 统计时间冲突剪枝；completeLeaves 统计搜索到的完整叶子，可能比保留数多 1。耗时采用 steady_clock，含准备候选域，不含 GUI 快照复制及绘制。

核心接口把空课程集合视为一个合法的空组合，与笛卡尔积约定一致；GUI 在空项目时禁用生成。当前课程分组直接来自已有 TeachingClass，不会出现没有教学班的分组。其他完整搜索的零结果会显示无解，但本轮尚未分析最小无解原因。

BruteForceGenerator 默认最多枚举 100000 个完整组合；乘积超过限制会在枚举之前抛出 length_error。通过除法预判防止 size_t 溢出。基线只用于小规模测试，不出现在 GUI 操作中。

## 线程与持久化

工作线程只读项目值拷贝，不接触 Widgets 或可变项目。取消使用共享 atomic_bool；编辑项目时还递增版本号，旧线程即使完成也无法提交过期方案。允许一项搜索在运行；退出时发出取消并等待结束。取消检查位于候选域准备和 DFS 迭代边界，单次位运算/完整方案验证执行完才会响应。

候选方案是临时派生结果。保存 JSON 不清除当前方案，也不把方案写入文件；打开项目后需重新生成。当前文件格式见 [DATA_FORMAT.md](DATA_FORMAT.md)。搜索不增加“未保存修改”标记。

## 演示与下一步

```powershell
./scripts/build.ps1
./scripts/run.ps1 -ImportCsv examples/search_demo.csv
```

点击“生成方案”应得到 3 个方案：操作系统张老师只能配数据库赵老师；操作系统李老师可配数据库两位教师，体育固定陈老师。第 2 周可以看到周日第 6 节体育；第 5 周没有体育。原 examples/courses.csv 只有 1 个合法方案。

下一阶段建议加入显式必选课程集合、锁定教学班和禁止时间，使 Generator、Validator、BruteForce 同时支持硬约束；保持小规模全集对照后再加入 MRV / Forward Checking。软偏好评价和 Top-K 留到硬约束稳定之后。
