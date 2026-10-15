#pragma once
#include "core/algorithms/ScheduleGenerator.h"
#include "ui/ProjectController.h"
#include <atomic>
#include <memory>
#include <optional>
class QThread;

namespace planner::ui {
using namespace std;

// 后台线程搜索项目副本；公开方法和信号均在 GUI 线程使用。
// 项目改动会取消旧任务，并使旧结果失效。
class SearchController : public QObject {
    Q_OBJECT
public:
    explicit SearchController(ProjectController& project, QObject* parent = nullptr);
    ~SearchController() override;
    bool busy() const noexcept { return thread_ != nullptr; }
    const optional<SearchResult>& result() const noexcept { return result_; }
    bool start(size_t maxSolutions = 100, uint64_t maxNodes = 100000);
    void cancel();
signals:
    void stateChanged();
    void resultsChanged();
private:
    void invalidate();
    ProjectController& project_;
    QThread* thread_ = nullptr;
    shared_ptr<atomic_bool> cancelled_;
    optional<SearchResult> result_;
    uint64_t revision_ = 0;
};
}
