#include "ui/SearchController.h"
#include <QThread>

using namespace std;

namespace planner::ui {
SearchController::SearchController(ProjectController& project, QObject* parent)
    : QObject(parent), project_(project) {
    connect(&project_, &ProjectController::dataChanged, this, &SearchController::invalidate);
}
SearchController::~SearchController() {
    cancel();
    if (thread_) { thread_->wait(); }
}
void SearchController::cancel() {
    if (cancelled_) { cancelled_->store(true, memory_order_relaxed); }
}
void SearchController::invalidate() {
    ++revision_;
    cancel();
    result_.reset();
    emit resultsChanged();
    emit stateChanged();
}
bool SearchController::start(size_t maxSolutions, uint64_t maxNodes) {
    if (busy() || project_.project().teachingClasses().empty()) { return false; }
    auto output = make_shared<SearchResult>();
    auto stop = make_shared<atomic_bool>(false);
    const auto revision = revision_;
    // 启动前复制项目，后台线程不读取界面正在编辑的数据。
    auto* thread = QThread::create([snapshot = project_.project(), stop, output, maxSolutions, maxNodes] {
        SearchOptions options;
        options.maxSolutions = maxSolutions;
        options.maxNodes = maxNodes;
        options.cancelled = [stop] { return stop->load(memory_order_relaxed); };
        *output = ScheduleGenerator::generate(snapshot, options);
    });
    thread->setParent(this);
    thread_ = thread;
    cancelled_ = stop;
    result_.reset();
    connect(thread, &QThread::finished, this, [this, thread, output, revision] {
        thread_ = nullptr;
        cancelled_.reset();
        thread->deleteLater();
        // 搜索期间若项目已变化，丢弃快照的结果，不能展示为当前项目的方案。
        if (revision == revision_) {
            result_ = move(*output);
            emit resultsChanged();
        }
        emit stateChanged();
    });
    emit resultsChanged();
    emit stateChanged();
    thread->start();
    return true;
}
}
