#pragma once
#include <QDialog>

class QLineEdit;

namespace planner::ui {
// 姓名只用于区分本机课表文件，不做密码认证。
class LoginDialog : public QDialog {
public:
    explicit LoginDialog(QWidget* parent = nullptr);
    QString accountName() const;
private:
    QLineEdit* name_;
};
}
