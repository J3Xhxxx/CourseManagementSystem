#pragma once
#include <QSpinBox>

namespace planner::ui {
// 主课表周次和禁止时间节次共用：向下递增，向上递减。
class ReverseSpinBox : public QSpinBox {
public:
    using QSpinBox::QSpinBox;
protected:
    void stepBy(int steps) override { QSpinBox::stepBy(-steps); }
    StepEnabled stepEnabled() const override {
        // 步进反向后也要交换边界状态，否则最小值处的向下按钮仍会被禁用。
        const auto enabled = QSpinBox::stepEnabled();
        StepEnabled reversed = StepNone;
        if (enabled.testFlag(StepUpEnabled)) { reversed |= StepDownEnabled; }
        if (enabled.testFlag(StepDownEnabled)) { reversed |= StepUpEnabled; }
        return reversed;
    }
};
}
