# 核心算法代码解释

本文解释排课搜索相关的 7 个文件：

| 文件 | 作用 |
| --- | --- |
| `src/core/structures/TimeBitSet.h/.cpp` | 用位图表示“一学期中的若干个上课时间点”，提供求交、求并等集合运算 |
| `src/core/algorithms/ConflictDetector.cpp` | 判断两个教学班能否同时选，并给出原因 |
| `src/core/structures/ConflictGraph.cpp` | 把所有教学班两两之间的冲突建成一张无向图，供界面查看 |
| `src/core/algorithms/ScheduleGenerator.h/.cpp` | 选课方案搜索：回溯、MRV、MRV + 前向检查三种策略 |
| `src/demo/benchmark_main.cpp` | 用 4 组固定数据比较暴力枚举和三种策略的正确性与效率 |

它们的依赖关系是：

```
TimeBitSet  ←  TeachingClass（每个教学班缓存一份自己的时间位图）
    ↑
ConflictDetector  ←  ConflictGraph  ←  TeachingClassService（项目数据）
    ↑                                         ↑
    └──────────── ScheduleGenerator ──────────┘
                         ↑
                  benchmark_main（与 BruteForceGenerator 对照）
```

---

## 1. TimeBitSet：时间位图

### 1.1 为什么用位图

一个教学班可能“1-16 周的周一第 1 大节 + 单周周四第 3 大节”，判断两个班是否冲突，本质是判断两个**时间点集合**有没有交集。

如果用 `vector<TimeSlot>` 存，判断交集要两重循环或者排序后归并；用位图后，判断交集只要把两个位图逐个 64 位整数做 `&`，只要有一个结果不为 0 就说明冲突。20 周的学期共有 20 × 7 × 6 = 840 个时间点，只需要 ⌈840 / 64⌉ = 14 个 `uint64_t`，比较一次最多 14 次与运算。

### 1.2 时间点的编号

`TimeSlot` 是一个时间点：

```cpp
struct TimeSlot {
    int week;    // 第几周，从 1 开始
    int weekday; // 星期几，1~7
    int period;  // 第几大节，1~6
};
```

`index()` 把三元组按“周 → 星期 → 节次”的顺序展开成一个从 0 开始的下标，相当于三位的混合进制数：

```
index = (week-1) * 42 + (weekday-1) * 6 + (period-1)
```

其中 42 = 7 天 × 6 节，是一周的时间点数。例如第 2 周周三第 4 节：(2-1)×42 + (3-1)×6 + (4-1) = 42 + 12 + 3 = 57。

`decode()` 是反过来的运算，用除法和取余拆回三个分量：

```
week    = index / 42 + 1
weekday = index % 42 / 6 + 1
period  = index % 6 + 1
```

`index()` 会检查周次、星期、节次是否越界，越界时抛出 `out_of_range`。这是真正需要的输入检查：例如 JSON 文件里写了第 30 周，而学期只有 20 周，就会在这里报错。

### 1.3 位的存取

下标为 `position` 的位存在第 `position / 64` 个整数的第 `position % 64` 位：

```cpp
void TimeBitSet::set(TimeSlot slot) {
    const size_t position = index(slot);
    words_[position / 64] |= 1ULL << (position % 64);       // 把这一位置 1
}

bool TimeBitSet::contains(TimeSlot slot) const {
    const size_t position = index(slot);
    return (words_[position / 64] >> (position % 64) & 1ULL) != 0;  // 取出这一位
}
```

`1ULL` 表示 64 位的 1。如果写成 `1 << 40`，`1` 是 32 位 int，左移 40 位是未定义行为，这是位运算里常见的坑。

构造函数里 `(bitCount_ + 63) / 64` 是向上取整的写法。最后一个整数里多出来的高位永远是 0，不代表任何时间。

### 1.4 集合运算

| 函数 | 含义 | 位运算 |
| --- | --- | --- |
| `intersects(other)` | 是否有交集 | 任意 `a[i] & b[i] != 0` 就返回 true，可以提前结束 |
| `intersection(other)` | 交集 | `a[i] & b[i]` |
| `unite(other)` | 并集（原地修改） | `a[i] \|= b[i]` |
| `subtract(other)` | 差集（原地修改） | `a[i] &= ~b[i]` |
| `empty()` | 是否为空 | 所有整数都为 0 |

这些运算都要求两个位图的学期周数相同（整数个数一样）。项目里所有位图都按项目的学期周数创建，所以这个条件总是成立，代码里不再重复检查。

### 1.5 count()：统计 1 的个数

```cpp
while (word != 0) {
    word &= word - 1;
    ++result;
}
```

`word - 1` 会把最低位的 1 变成 0，并把它右边的 0 全变成 1；再和原数做 `&`，效果就是“清掉最低位的 1”。例如 `word = 1011000`，`word - 1 = 1010111`，两者相与得 `1010000`。循环次数等于 1 的个数，比逐位检查 64 次快。

### 1.6 occupiedSlots()

从 0 到 `bitCount_ - 1` 逐位检查，把为 1 的位用 `decode()` 转回 `TimeSlot`。因为下标按“周 → 星期 → 节次”排列，所以结果天然按时间先后排序。界面显示具体冲突时间时用它。

### 1.7 PERIOD_TIMES

```cpp
const char* const PERIOD_TIMES[6] = {"08:00-09:45", ...};
```

六个大节的具体时间，下标 0 对应第 1 大节。写在头文件里的 `const` 全局常量默认只在本文件内可见，所以被多个 `.cpp` 包含也不会出现重复定义的链接错误。

---

## 2. ConflictDetector：两个教学班能否同时选

```cpp
ConflictResult ConflictDetector::analyze(const TeachingClass& first, const TeachingClass& second)
```

两种互斥原因，可能同时存在：

1. **时间重叠**：两个班的时间位图求交，交集非空就记 `TimeOverlap`，并用 `occupiedSlots()` 把交集展开成具体时间点，方便界面显示“第 3 周周一第 1 节冲突”。
2. **同课互斥**：同一门课（课程代码 + 名称相同，即 `courseKey` 相同）的不同教学班只能选一个，即使时间完全不重叠也不能同时选，记 `SameCourseExclusive`。

`ConflictResult::hasConflict()` 只要原因列表不为空就返回 true。

复杂度：位图求交 O(⌈W×42/64⌉)，W 为学期周数；展开具体时间点 O(W×42)。

---

## 3. ConflictGraph：冲突图

### 3.1 存储结构

```cpp
unordered_map<string, vector<ConflictEdge>> adjacency_;   // 邻接表
size_t edgeCount_;
```

- 顶点是教学班，用教学班 ID（字符串）作为键。不用 `vector` 下标作顶点，是因为删除教学班后后面的下标会整体前移，已经建好的边就会指错对象。
- 每条边 `ConflictEdge` 记录邻居 ID 和冲突详情（原因 + 冲突时间）。

### 3.2 建图过程

```cpp
for (const auto& teachingClass : teachingClasses) {
    adjacency_[teachingClass.id()] = vector<ConflictEdge>();
}
for (size_t i = 0; i < n; ++i)
    for (size_t j = i + 1; j < n; ++j) {
        ConflictResult result = ConflictDetector::analyze(classes[i], classes[j]);
        if (result.hasConflict()) {
            adjacency_[i 的 ID].push_back({j 的 ID, result});
            adjacency_[j 的 ID].push_back({i 的 ID, result});
            ++edgeCount_;
        }
    }
```

1. 先给每个顶点放一个空的邻接表，这样没有任何冲突的教学班调用 `neighbors()` 时也能得到空列表，而不是找不到键。
2. 再对所有 `i < j` 的教学班对调用一次 `analyze`，共 n(n-1)/2 次。`j` 从 `i + 1` 开始，保证每一对只比较一次。
3. 无向图的一条边在两个端点的邻接表里各存一份，但 `edgeCount_` 只加 1。

整图构建需要两两比较全部教学班。现在单班新增、修改只比较该班与其他 n−1 个班，保留其他班之间的边；删除时只清理关联的双向记录。批量导入和 JSON 读取仍整图构建。

### 3.3 和搜索的关系

冲突图同时用于界面展示和搜索中的前向检查。前向检查沿所选班的邻接表排除冲突候选；DFS 判断候选与“已选所有班的时间并集”是否冲突时，仍使用位图求交。

---

## 4. ScheduleGenerator.h：搜索的输入与输出

### 4.1 问题建模

这是一个**约束满足问题（CSP）**：

- **变量**：每门需要搜索的必选课程。
- **值域（domain）**：这门课可以选的教学班。
- **约束**：任意两门课选中的班时间不能重叠；不能占用禁止时间；不能与学校固定课程冲突；锁定了班的课程只能选那个班。

一个合法方案就是给每个变量赋一个值，且满足所有约束。

### 4.2 SearchOptions：搜索参数

| 字段 | 含义 |
| --- | --- |
| `maxSolutions` | 最多保留多少个方案，默认 100 |
| `maxNodes` | 最多尝试选多少次教学班，默认 100000，防止数据量大时搜太久 |
| `cancelled` | 一个返回 bool 的函数，界面点“取消搜索”后返回 true |
| `strategy` | 搜索策略，默认 MRV + 前向检查 |

`cancelled` 用 `function<bool()>` 而不是直接传一个 `bool`，是因为搜索在后台线程运行，界面线程会在搜索过程中修改取消标志；传函数进来，搜索每次调用时都能读到最新的值。界面里实际传入的是读取 `atomic_bool` 的 lambda。

### 4.3 SearchStatus：结束原因

| 值 | 含义 |
| --- | --- |
| `Complete` | 整棵搜索树都搜完了，结果是全部合法方案 |
| `SolutionLimit` | 方案数达到上限，后面还有方案没保存 |
| `NodeLimit` | 尝试次数达到上限，搜索没完成 |
| `Cancelled` | 用户取消 |

只有 `Complete` 时“0 个方案”才能说明真的无解；其他情况下 0 个方案只表示“还没找到”。

### 4.4 SearchStatistics：统计数据

| 字段 | 含义 |
| --- | --- |
| `visitedNodes` | 尝试选择一个教学班就加 1（包括马上被剪掉的） |
| `prunedBranches` | 被剪掉的分支数 |
| `completeLeaves` | 走到底（每门课都选了班）的次数 |
| `candidateRemovals` | 前向检查删除候选的累计次数 |
| `domainWipeouts` | 前向检查发现某门课已没有候选的次数 |
| `elapsedMilliseconds` | 用时 |

### 4.5 ConstraintConflict：搜索前就能发现的无解原因

学校固定课程和个人锁定的班都不能换。如果它们之间时间重叠，或者占用了禁止时间，那就一定无解，没必要搜索。这种情况会直接返回冲突原因（5 种，见 `ConstraintConflictReason`）和具体冲突时间，界面据此提示用户“哪两个班冲突、在哪个时间”。

---

## 5. ScheduleGenerator.cpp：搜索算法

### 5.1 整体结构

所有搜索状态放在匿名命名空间里的 `Searcher` 类中，`ScheduleGenerator::generate` 只是创建一个 `Searcher` 并调用 `run()`：

```cpp
SearchResult ScheduleGenerator::generate(const TeachingClassService& project, const SearchOptions& options) {
    Searcher searcher(project, options);
    return searcher.run();
}
```

用类的好处是各个步骤（建候选域、选课程、前向检查、回溯恢复、递归搜索）可以拆成独立的成员函数，共享的状态做成成员变量，不用在函数之间传一大串参数。匿名命名空间让 `Searcher` 只在这个 `.cpp` 中可见。

主要成员变量：

| 成员 | 含义 |
| --- | --- |
| `initial_` | 学校固定课程组成的初始课表 |
| `occupied_` | 当前已占用的时间 = 固定课程 + 已选教学班 |
| `domains_[i]` | 第 i 门课的候选教学班列表 |
| `selected_[i]` | 第 i 门课当前选中的班，没选为 `nullptr` |
| `active_[i][v]` | 第 i 门课的第 v 个候选是否还有效（前向检查用） |
| `remaining_[i]` | 第 i 门课还剩几个有效候选（前向检查用） |
| `removed_` | 前向检查的删除记录栈，回溯时用来恢复 |
| `candidatePositions_` | 教学班 ID 到 `(课程下标, 候选下标)` 的散列表，用于从冲突邻居定位候选 |

`run()` 的流程：

```
记录开始时间
buildDomains()  —— 如果发现无解原因，直接结束
初始化 selected_、active_、remaining_
dfs(0)
记录用时，返回结果
```

### 5.2 第一步：buildDomains() 预处理

对每门课程：

1. 不是必选课，跳过；是学校固定课程，也跳过（它已经在 `initial_` 里了，不需要再选）。
2. 看这门课有没有被用户锁定某个班。
3. 过滤候选：
   - 锁定了班，就只保留那一个；
   - 占用了禁止时间的班，删掉；
   - 与学校固定课程时间冲突的班，删掉。
4. 过滤后为空，说明这门课怎么选都不行，记入 `coursesWithoutCandidates`。

这一步叫**节点相容（node consistency）**：只涉及单个变量的约束（禁止时间、固定课表、锁定）在搜索前一次性处理掉，搜索时就不用再管它们。

然后检查所有“不能换的班”（固定班 + 锁定班）两两之间是否冲突、是否占用禁止时间，有问题就记入 `constraintConflicts`。

只要上面两类问题有一个，就返回 false，不进行搜索。

### 5.3 第二步：dfs() 递归搜索

```cpp
bool Searcher::dfs(size_t depth) {
    if (depth == domains_.size()) { return saveSchedule(); }  // 每门课都选好了
    vector<size_t> values;
    size_t course = chooseCourse(values);                     // 决定这层选哪门课
    if (values.empty()) { ++prunedBranches; return true; }    // 这门课无班可选
    for (size_t v : values) {
        检查取消 / 尝试次数上限 → 需要停止就返回 false
        teachingClass = domains_[course][v];
        ++visitedNodes;
        if (与 occupied_ 冲突) { ++prunedBranches; continue; }      // 剪枝 1
        mark = removed_.size();
        selected_[course] = teachingClass;
        if (前向检查失败) { ++prunedBranches; 恢复; continue; }     // 剪枝 2
        occupied_.unite(teachingClass 的时间);
        keepGoing = dfs(depth + 1);
        occupied_.subtract(teachingClass 的时间);                    // 回溯
        restore(mark);
        selected_[course] = nullptr;
        if (!keepGoing) return false;
    }
    return true;
}
```

要点：

- **depth** 表示已经选好的课程数。等于课程总数时说明得到一个完整方案。
- **返回值** 表示要不要继续搜索。用户取消、尝试次数到上限、方案数到上限时返回 false，这个 false 会一层层往上传，整个搜索就停下来。
- **剪枝 1**：候选班与已占用时间冲突，这个分支不可能合法，直接跳过。
- **回溯**：递归返回后，要把本层做的修改全部撤销，才能尝试下一个候选。

**为什么回溯时用 `subtract` 就能还原 `occupied_`？** 能走到 `unite` 这一步，说明这个班与 `occupied_` 没有任何交集（剪枝 1 已经检查过）。两个不相交的集合，先并上再减去，正好得到原来的集合。所以不需要像“每层保存一份位图副本”那样多占内存。

**递归深度** 等于需要搜索的课程数，一个学生一学期的课最多二三十门，不会栈溢出。

### 5.4 chooseCourse()：三种策略的区别

三种策略唯一的区别就是“下一步决定哪门课”以及“这门课有哪些候选可以试”。

**普通回溯（Backtracking）**：按课程顺序，取第一门还没选的课，它的所有候选都要试一遍。

**MRV（Minimum Remaining Values，最少剩余值优先）**：对每门还没选的课，数一下它有多少个候选与当前已占用时间不冲突，选候选最少的那门课。

直观理解：如果某门课只剩 1 个班能选，那就先选它；如果某门课已经 0 个班能选，就立刻发现这条路走不通，不必再去选其他课。这就是“先处理最难的”，让失败尽早发生（fail-first）。循环条件里的 `bestCount > 0` 表示找到 0 个候选的课后就不用再比较了。

MRV 每次都要重新扫描所有未选课程的所有候选，单次选择的代价比普通回溯大。

**MRV + 前向检查（Forward Checking）**：同样选剩余候选最少的课，但“剩余候选数”不是现算的，而是由前向检查一直维护在 `remaining_` 里，直接比较即可。这门课要试的候选就是 `active_` 仍为 true 的那些。

三种策略在候选数相同时都取顺序靠前的课，组内候选按录入顺序尝试，所以同一份数据每次运行的结果完全相同。

### 5.5 forwardCheck()：前向检查

建立候选域时，把本次有效候选的 ID 和位置存入 `candidatePositions_`。每选定一个班 `chosen`，只遍历它在冲突图中的邻居，定位并停用其中尚未选课的有效候选：

```cpp
for (const auto& edge : project_.graph().neighbors(chosen->id())) {
    auto position = candidatePositions_.find(edge.neighborId);
    if (position == candidatePositions_.end()) { continue; }
    const auto [i, v] = position->second;
    if (selected_[i] || !active_[i][v]) { continue; }
    active_[i][v] = false;
    --remaining_[i];
    removed_.push_back({i, v});      // 记下来，回溯时恢复
}
```

如果某门课的候选被删光（`remaining_[i] == 0`），说明“选了 chosen 之后这门课无班可选”，不用等搜到那门课才发现，现在就可以剪枝（剪枝 2），并记一次 `domainWipeouts`。

和普通回溯相比，前向检查把“将来一定会发生的冲突”提前到现在发现，搜索树会小很多。

邻接表避免了逐个扫描无关候选和重复计算位图交集。图中可能还有未勾选课程、被固定课表或禁止时间过滤掉的班，因此查不到候选位置时直接跳过。同课程互斥邻居也会跳过，因为 `chosen` 所在课程已被选定。

### 5.6 removed_ 与 restore()：可撤销的删除

前向检查删除候选时**不真正从数组里删**，只把 `active_` 置为 false，这样候选的下标不会变化，恢复也很简单。

每次删除都压入 `removed_` 栈。选班前先记下栈的大小 `mark`，回溯时调用 `restore(mark)`，把 `mark` 之后的删除记录逐条弹出并恢复：

```cpp
void Searcher::restore(size_t mark) {
    while (removed_.size() > mark) {
        active_[课程][候选] = true;
        ++remaining_[课程];
        removed_.pop_back();
    }
}
```

这样只会恢复**本层**删除的候选，上层删除的候选在 `mark` 之前，不会被误恢复。这和“撤销操作”的栈是同一个思路。

### 5.7 saveSchedule()：保存方案与上限处理

```cpp
++completeLeaves;
if (schedules.size() == maxSolutions) {
    status = SolutionLimit;
    return false;
}
保存方案
return true;
```

注意判断上限是在**保存之前**：已经存满 K 个，又找到第 K+1 个时，才标记 `SolutionLimit` 并停止。如果恰好只有 K 个方案，搜索会正常搜完并报告 `Complete`，用户就能知道“方案就这么多”，而不是误以为还有更多。也正因为如此，`completeLeaves` 可能比保存的方案数多 1。

方案里的教学班顺序：先是固定课程（来自 `initial_`），再按课程顺序排列 `selected_`，与 MRV 实际的选择顺序无关，方便界面展示。

### 5.8 一个小例子

3 门课 A、B、C，都在周一，可选时段：

| 课程 | 候选班的时段 |
| --- | --- |
| A | 第 1 节、第 2 节 |
| B | 第 1 节、第 2 节 |
| C | 第 1 节 |

**普通回溯** 按 A → B → C 的顺序：

```
A=1 → B=1 冲突剪枝；B=2 → C=1 冲突剪枝 → 回溯
A=2 → B=1 → C=1 冲突剪枝；B=2 冲突剪枝 → 回溯
无解，共尝试 8 次（A1、B1、B2、C1、A2、B1、C1、B2）
```

**MRV**：开始时 C 只有 1 个候选，最少，先选 C=1；此时 A、B 都只剩第 2 节（各 1 个），选 A=2；B 剩 0 个候选，剪枝。无解，只尝试 2 次。

**MRV + 前向检查**：选 C=1 后，前向检查把 A、B 的第 1 节候选都删掉，各剩 1 个；再选 A=2，前向检查把 B 的第 2 节也删掉，B 变空，立即剪枝。同样 2 次就发现无解，而且不需要像 MRV 那样每层重新扫描所有候选。

### 5.9 复杂度

设需要搜索的课程有 C 门，第 i 门有 mᵢ 个候选，位图有 B 个整数：

- 最坏情况要搜完整个笛卡尔积，节点数 O(∏mᵢ)，这是 CSP 的固有难度，任何剪枝都不能改变最坏复杂度。
- 每个节点的代价：普通回溯 O(B)；MRV 要重新扫描所有未选课程的候选，O(M × B)，M 为候选总数；前向检查也是 O(M × B)。
- 额外空间：`active_`、`removed_` 为 O(M)，递归每层的 `values` 最多 O(M)，总共 O(C × M)。

剪枝不改变最坏情况，但在冲突多的实际数据上能把搜索树缩小几个数量级，见第 6 节的对照结果。

---

## 6. benchmark_main.cpp：性能对照

### 6.1 目的

1. **验证正确性**：暴力枚举（`BruteForceGenerator`）不做任何剪枝，枚举所有组合再逐个检查，结果一定对。三种搜索策略的方案数都应该和它相同，输出的最后一列 `same_as_brute` 就是这个检查。
2. **比较效率**：同样的数据，比较三种策略的尝试次数和剪枝次数。

### 6.2 四个测试场景

所有数据都只有 1 周，每个班每周上一次课，由 `makeClass()` 统一创建。

| 场景 | 构造方式 | 设计意图 |
| --- | --- | --- |
| `independent()` | 6 门课各 3 个班，每门课在不同的星期，互不冲突 | 没有任何冲突，共 3⁶ = 729 个方案。剪枝无用武之地，用来观察 MRV 和前向检查的额外开销 |
| `lateBottleneck(false)` | 在上面基础上加一门只有 1 个班的课，放在周一第 1 节，与第 0 门课的 3 个班全冲突 | “瓶颈”课排在最后。普通回溯要先把前 6 门课的 729 种组合都排好，才发现最后一门课选不了；MRV 一开始就选这门只有 1 个候选的课 |
| `lateBottleneck(true)` | 同上，但第 0 门课的第 3 个班改到第 2 节 | 有解（3⁵ = 243 个），比较可满足时的搜索量 |
| `pigeonhole()` | 5 门课都只能排在周一的 4 个时段 | 鸽巢原理：5 只鸽子放不进 4 个笼子，一定无解，但每两门课单独看都能排开，属于比较难发现的无解 |

### 6.3 compare() 函数

1. 计算暴力枚举的组合总数（各课程班数的乘积），运行暴力枚举并计时。
2. 依次用三种策略运行 `ScheduleGenerator::generate`。上限设得很大（10 万个方案、100 万次尝试），保证能搜完。
3. 每种策略输出一行 CSV：方案数、尝试次数、完整组合数、剪枝次数、用时、是否与暴力枚举一致。

### 6.4 实际结果（Release，本机）

```
case,strategy,solutions,visited_nodes,complete_combinations,pruned_branches,elapsed_ms,same_as_brute
independent,BruteForce,729,,729,0,1.369,
independent,Backtracking,729,1092,729,0,0.321,yes
independent,MRV,729,1092,729,0,0.645,yes
independent,MRV+FC,729,1092,729,0,0.543,yes
late_bottleneck_unsatisfiable,BruteForce,0,,729,0,1.584,
late_bottleneck_unsatisfiable,Backtracking,0,1821,0,729,0.066,yes
late_bottleneck_unsatisfiable,MRV,0,1,0,1,0.006,yes
late_bottleneck_unsatisfiable,MRV+FC,0,1,0,1,0.005,yes
late_bottleneck_satisfiable,BruteForce,243,,729,0,2.969,
late_bottleneck_satisfiable,Backtracking,243,1821,243,486,0.192,yes
late_bottleneck_satisfiable,MRV,243,365,243,0,0.117,yes
late_bottleneck_satisfiable,MRV+FC,243,365,243,0,0.117,yes
dense_unsatisfiable,BruteForce,0,,1024,0,3.807,
dense_unsatisfiable,Backtracking,0,260,0,196,0.020,yes
dense_unsatisfiable,MRV,0,64,0,24,0.017,yes
dense_unsatisfiable,MRV+FC,0,64,0,24,0.014,yes
```

分析：

- **所有策略的方案数都与暴力枚举一致**，说明剪枝没有误删合法方案。
- **无冲突场景**：三种策略的尝试次数都是 1092 = 3 + 9 + 27 + 81 + 243 + 729（每层的节点数之和），剪枝没有效果；MRV 类策略因为要额外扫描候选，用时反而更长。
- **瓶颈无解场景**：普通回溯尝试 1821 次，MRV 只尝试 1 次。MRV 第一步就选了只有 1 个候选的瓶颈课，选它之后第 0 门课的 3 个候选全部冲突，立即发现无解。
- **瓶颈有解场景**：普通回溯 1821 次，MRV 365 次，减少约 80%。
- **鸽巢场景**：普通回溯 260 次，MRV 64 次。
- 本例中 MRV 与 MRV + 前向检查的尝试次数相同，因为数据规模小，两者做出的选择一样；区别在于前向检查不用每层重新扫描候选，冲突多、课程多时差距会更明显。
- 毫秒级的计时受系统调度影响较大，只能作参考，尝试次数和剪枝次数才是稳定的比较指标。

运行方法：

```powershell
./scripts/build.ps1 -Configuration Release
<构建目录>/course_benchmark.exe
```
