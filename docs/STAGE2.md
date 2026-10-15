> 本文保留对应阶段的历史记录。当前已移除 Course 实体，以 [TEACHING_CLASS_MODEL.md](TEACHING_CLASS_MODEL.md) 的教学班模型及接口为准。

> 2026-10-09 已移除自动化测试源码和构建入口。下文测试统计属于历史记录，当前构建和检查方法见 [README.md](../README.md)。

# 第二轮：数据管理、文件读写与冲突图

## 分层

- course_core：原有四个模型、位集合、冲突检测，加上 CourseService 与 ConflictGraph；只依赖 C++17 STL。
- course_io：CsvImporter 和 JsonProjectStore，依赖 course_core 与 Qt Core，不依赖 Widgets。
- course_data：文件导入、合并、项目读取和冲突展示的控制台入口。
- course_app：保留第一轮 Qt 空窗口，尚未接入数据业务。

## CourseService

服务保存 vector<Course> 和 vector<TeachingClass>，使用两张 unordered_map 将稳定 ID 映射到 vector 下标。findCourse/findTeachingClass 平均查找复杂度 O(1)，找不到时返回 nullptr。searchCourses 对代码和名称做区分大小写的子串搜索，返回课程 ID。

```cpp
CourseService project(20);
Course os("CS101", "操作系统", 3.5);
project.addCourse(os);
TeachingClass teachingClass(os, "张老师", {
    Meeting(parseWeeks("[1-5,7-9周]"), 1, 1, 1, "H415")
});
project.addTeachingClass(teachingClass);

// 编辑整份教学班数据，同时重新计算位图和图。
project.updateTeachingClass(teachingClass.id(), TeachingClass(os, "李老师", {
    Meeting(parseWeeks("1-16周"), 2, 2, 2, "H205")
}));

// 修改课程身份时，所有关联教学班一起换到新身份。
project.updateCourse(os.id(), Course("CS101", "操作系统原理", 4.0));
```

removeTeachingClass 删除单个班；removeCourse 级联删除所属教学班。未知 ID 的修改/删除抛出 out_of_range，重复 ID、无所属课程和学期不一致抛出 invalid_argument。服务返回的引用和指针在成功修改后失效；界面应持有 ID 而非对象地址或行号。

所有写操作都先构造候选数据和索引，完成校验和冲突图构建后交换到正式状态。修改失败，原始数据、索引、图都不改变。CSV 使用一次 replaceData 提交整个批次，避免导入到一半留下残缺项目。

## ConflictGraph

unordered_map<TeachingClassId, vector<ConflictEdge>> 保存邻接表。一个无向边在两个邻接表中各存一份；edgeCount 返回去重后的无向边数。每个节点包括无冲突的孤立教学班。

ConflictEdge 包含邻居 ID、冲突原因列表及具体 TimeSlot。时间重叠与同课程互斥可以同时存在。邻居顺序遵循输入 TeachingClass 顺序；对不存在的节点查询会抛出 out_of_range。

此阶段每次成功修改都重建图，优先保证删改后没有悬挂 ID，不实现增量更新。设 M 为班数，T=W×7×6，B=ceil(T/64)，E_t 为有时间冲突的班对数：

- 完整重建的时间复杂度为 O(M²×B + E_t×T)，后一项来自解码具体冲突点。
- 空间包括模型/位图、O(M+E) 的邻接结构和具体冲突点列表。
- 事务式更新还需要一份候选数据，当前面向课程设计的小中规模数据。

不应将当前全量重建实现描述成增量算法。后续若测得数据修改性能不足，可替换成仅重算受影响节点的边，外部 ID 接口保持不变。

## 本轮边界与后续

新增 4 组 service_tests、6 组 io_tests 和一个 CSV 命令行测试。继承第一轮 9 组 core_tests，不改动既有时间模型规则。

下一轮建议先接入课程管理/文件操作/冲突查看的最小 Qt 界面，完成可视化数据验收；再开发固定课程集合的基础回溯及独立验证器。此轮未实现硬约束、偏好评价、Top-K 或组合搜索。
