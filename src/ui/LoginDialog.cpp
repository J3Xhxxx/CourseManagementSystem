#include "ui/LoginDialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace planner::ui {
LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("登录 — 个人选课与冲突分析");
    setMinimumWidth(400);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);
    auto* hint = new QLabel("请输入姓名进入系统。\n再次使用同一姓名，可继续编辑自己的课表。", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* form = new QFormLayout;
    name_ = new QLineEdit(this);
    name_->setPlaceholderText("请输入姓名");
    // 姓名直接作为文件名，输入时排除 Windows 文件名禁止的字符。
    name_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(R"([^<>:"/\\|?*\x00-\x1f]+)"), name_));
    form->addRow("姓名", name_);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* login = buttons->button(QDialogButtonBox::Ok);
    login->setText("登录");
    login->setDefault(true);
    login->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setText("退出");
    layout->addWidget(buttons);
    connect(name_, &QLineEdit::textChanged, this, [this, login] {
        login->setEnabled(name_->hasAcceptableInput() && !accountName().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    name_->setFocus();
}
QString LoginDialog::accountName() const {
    return name_->text().trimmed();
}
}
