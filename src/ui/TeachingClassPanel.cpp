#include "ui/TeachingClassPanel.h"
#include "ui/DisplayText.h"
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

using namespace std;

namespace planner::ui {
namespace {
class TeachingClassDelegate : public QStyledItemDelegate {
public:
    explicit TeachingClassDelegate(QTreeWidget* tree) : QStyledItemDelegate(tree), tree_(tree) {}
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        auto styled = option;
        initStyleOption(&styled, index);
        const int width = max(1, tree_->columnWidth(0) - 12);
        const auto bounds = styled.fontMetrics.boundingRect(QRect(0,0,width,100000),
            Qt::TextWordWrap, styled.text);
        return {width + 12, bounds.height() + 12};
    }
private:
    QTreeWidget* tree_;
};
}
TeachingClassPanel::TeachingClassPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("教学班", this));
    search_ = new QLineEdit(this);
    search_->setPlaceholderText("代码、名称、教师、周次、星期、教室");
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);
    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({"教学班与完整上课安排"});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->setRootIsDecorated(false);
    tree_->setWordWrap(true);
    tree_->setTextElideMode(Qt::ElideNone);
    tree_->setItemDelegate(new TeachingClassDelegate(tree_));
    connect(tree_->header(), &QHeaderView::sectionResized, tree_, &QTreeWidget::doItemsLayout);
    tree_->setAlternatingRowColors(true);
    layout->addWidget(tree_);
    auto* hint = new QLabel("选择教学班查看周课表；非固定班双击可编辑。\n多个搜索条件用空格分隔。", this);
    hint->setWordWrap(true); layout->addWidget(hint);
    connect(search_, &QLineEdit::textChanged, this, &TeachingClassPanel::applyFilter);
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &TeachingClassPanel::selectionChanged);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this] { emit editRequested(); });
}
QString TeachingClassPanel::teachingClassId() const {
    return tree_->currentItem() ? tree_->currentItem()->data(0, Qt::UserRole).toString() : QString{};
}
void TeachingClassPanel::setProject(const TeachingClassService& project) {
    const auto previous = teachingClassId();
    {
        QSignalBlocker blocker(tree_);
        tree_->clear();
        for (const auto& teachingClass : project.teachingClasses()) {
            QStringList lines{QString::fromStdString(teachingClass.code()+" · "+teachingClass.name()),
                QString("%1 · %2 学分").arg(QString::fromStdString(teachingClass.teacher())).arg(teachingClass.credits())};
            if (teachingClass.isFixed()) { lines << "学校固定课程（不可取消或换班）"; }
            for (const auto& meeting : teachingClass.meetings()) {
                const auto period = meeting.startPeriod() == meeting.endPeriod() ? QString::number(meeting.startPeriod())
                    : QString("%1-%2").arg(meeting.startPeriod()).arg(meeting.endPeriod());
                lines << QString("第 %1 周 · %2 第 %3 节 · %4").arg(weeksText(meeting.weeks()),
                    weekdayText(meeting.weekday()), period, QString::fromStdString(meeting.classroom()));
            }
            auto* item = new QTreeWidgetItem(tree_, {lines.join('\n')});
            item->setData(0, Qt::UserRole, QString::fromStdString(teachingClass.id()));
            item->setToolTip(0,item->text(0));
            if (previous == QString::fromStdString(teachingClass.id())) { tree_->setCurrentItem(item); }
        }
    }
    applyFilter();
    emit selectionChanged();
}
bool TeachingClassPanel::selectTeachingClass(const QString& id) {
    for (int row = 0; row < tree_->topLevelItemCount(); ++row) {
        auto* item = tree_->topLevelItem(row);
        if (item->data(0, Qt::UserRole).toString() != id) { continue; }
        if (item->isHidden()) { search_->clear(); }
        tree_->setCurrentItem(item); tree_->scrollToItem(item); return true;
    }
    return false;
}
void TeachingClassPanel::applyFilter() {
    const auto terms = search_->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (int row = 0; row < tree_->topLevelItemCount(); ++row) {
        auto* item = tree_->topLevelItem(row);
        bool match = true;
        for (const auto& term : terms) { match = match && item->text(0).contains(term, Qt::CaseInsensitive); }
        item->setHidden(!match);
    }
    if (tree_->currentItem() && tree_->currentItem()->isHidden()) { tree_->setCurrentItem(nullptr); }
}
}
