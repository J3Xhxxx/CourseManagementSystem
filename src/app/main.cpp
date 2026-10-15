#include "ui/MainWindow.h"
#include "ui/LoginDialog.h"
#include "ui/DisplayText.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>

using namespace std;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("CoursePlanner");
    QCoreApplication::setApplicationVersion("0.8.0");
    QApplication::setStyle("Fusion");
    auto font = app.font();
    font.setPointSize(10);
    app.setFont(font);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"import", "Import a CSV at startup.", "csv"});
    parser.process(app);
    // Debug 和 Release 共用项目根目录下的 accounts，不受启动工作目录影响。
    const auto accountsDirectory = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../accounts");
    auto importCsv = parser.value("import");
    while (true) {
        planner::ui::LoginDialog login;
        if (login.exec() != QDialog::Accepted) { return 0; }
        try {
            planner::ui::MainWindow window(login.accountName(), accountsDirectory);
            if (!importCsv.isEmpty()) {
                window.controller().importCsv(importCsv);
                importCsv.clear(); // 启动导入仅执行一次，不带入切换后的账号。
            }
            window.show();
            const int result = app.exec();
            if (!window.switchingAccount()) { return result; }
            // 销毁本次账号的窗口及后台搜索，再回到登录；进程保持运行。
        } catch (const exception& e) {
            QMessageBox::warning(nullptr, "无法打开课表", planner::ui::errorText(e));
            return 1;
        }
    }
}
