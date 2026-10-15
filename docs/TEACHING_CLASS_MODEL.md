# TeachingClass：以教学班为唯一录入单位

2026-09-27：按用户要求统一使用 TeachingClass 命名，相关文件、接口及 JSON 字段同步调整。

2026-10-07：增加 `isFixed` 学校固定课程标记；CSV 改为上课时间数组，JSON 当时格式为 version 4。

2026-10-15：`UserConstraints` 增加软偏好 `preferenceOrder`（六项评价指标的优先级顺序），JSON 升为 version 5，评价与 Top-K 见 [STAGE7.md](STAGE7.md)。

按最新需求移除独立的 Course 数据结构。课程代码、课程名称、学分都属于 TeachingClass；没有课程对象、课程外键或单独的课程增删改入口。

## 核心接口

```cpp
TeachingClass teachingClass("CS101", "操作系统", 3.5, "张老师", {
    Meeting(parseWeeks("[1-5,7-9周]"), 1, 1, 1, "H415"),
    Meeting(parseWeeks("[1-5,7-9周]"), 4, 3, 3, "H416")
});
TeachingClassService project(20);
project.addTeachingClass(teachingClass);
```

| 类型 | 职责 |
| --- | --- |
| TeachingClass | 完整教学班：代码、名称、学分、教师、isFixed、多个 Meeting 和缓存的时间位图 |
| Meeting | 一条周期性安排：显式周次、星期、起止节次、教室 |
| TimeBitSet | 周次 × 星期 × 节次的时间集合，支持交并、计数和反向解析 |
| TeachingClassService | 仅持有教学班集合及其索引；原子增删改、重建冲突图、维护约束 |
| UserConstraints | 必选分组键、锁定教学班 ID、禁止时间位图、方案偏好优先级（软偏好） |

TeachingClassService 提供 `teachingClasses / findTeachingClass / addTeachingClass / updateTeachingClass / removeTeachingClass / replaceTeachingClasses`。失败时保留模型、索引、图和约束；调用者可以传入服务自身持有的 ID 引用。

构造函数在 `meetings` 后接受 `semesterWeeks = 20, isFixed = false`。`isFixed()` 读取学校固定标记；服务的 `fixedTeachingClassForCourse(key)` 查找某课程的固定班，`fixedSchedule()` 汇总固定班 ID 及占用位图。

固定课程是学校已经确定、不可调整的初始课表，与用户可解除的个人锁定不同。加入后不能编辑、删除或取消必选；同一课程最多一个固定班，其他班不能替换它。批量替换也检查原固定班的身份、学分、安排和标记，失败时整批不提交。

## 身份与搜索

沿用最初确认的身份规则：代码＋名称＋教师共同确定教学班。时间、教室和学分是该班的属性；多次上课保存为同一班内多个 Meeting。

`courseKey()` 是根据代码＋名称生成的字符串，只用于识别可互相替代的教学班，不是另一个实体或外键。搜索依此保证同一课程只选一个班，必选设置也是对此键勾选。该键与教学班 ID 都采用各字段的 UTF-8 字节长度前缀编码，避免分隔符碰撞。

每个教学班独立保存学分；修改一个班的代码、名称、学分或教师不会批量改动其他班。若同一课程、同一教师确实存在两个独立班，目前仍需扩展独立班号才能区分；本次没有改变三元组身份规则。

时间索引继续使用 `(week-1)*7*6 + (weekday-1)*6 + (period-1)`，20 周共 840 位。

搜索以固定班的 ID 和时间位图作为初始状态，固定课程不进入可变候选域。其他候选的全部 Meeting 必须与初始占用互不相交。固定课表自身冲突或与锁定/禁止时间冲突时报告无解及时间点。

## 约束随编辑变化

- 新增一个从未出现的代码＋名称组合时，默认设为必选；向已有组合增加教师不会重置其勾选状态。
- 修改教学班身份时迁移该班锁定。锁定班移入其他组合会使目标组合必选；若目标已有另一个锁定班，拒绝修改，保留原数据。
- 未勾选的非固定教学班改名形成新组合，仍保持未勾选；若改为学校固定课程，则必须加入必选集合。
- 删除教学班会清除其锁定；最后一个同组合教学班消失时，同时移除该组合的必选键。其他班及禁止时间保持原状。
- 偏好优先级是软偏好，与教学班增删改无关：修改教学班不会重置它，删除课程也不影响它。

## 界面和文件

仅保留“新增教学班”入口，一次输入全部信息；左侧为平铺列表，展示代码、名称、教师、学分以及每次课的周次、星期、节次、教室。搜索条件以空格分隔，同时匹配。

CSV 必须包含“上课时间”和“是否为固定课程”。时间数组 `[(周一,1),(周四,3)]` 逐项生成 Meeting；相同三元组跨行归并。同一教学班的多行记录必须使用一致学分和固定标记，整批校验成功才提交。

JSON 仅支持当前 version 5，根对象直接存储 `teachingClasses` 和 `constraints`，每个教学班必须包含布尔 `isFixed`，约束中必须包含六项指标的 `preferences` 顺序；详见 [DATA_FORMAT.md](DATA_FORMAT.md)。项目处于开发阶段，不保留旧格式读取和迁移逻辑。

历史阶段说明中的 Course 和课程级编辑流程已由本文替代。
