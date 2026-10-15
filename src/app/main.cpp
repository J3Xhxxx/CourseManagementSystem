#include "ui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>

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
    planner::ui::MainWindow window;
    if (parser.isSet("import")) {
        try {
            window.controller().importCsv(parser.value("import"));
        } catch (const exception& e) {
            qCritical("%s", e.what());
            return 1;
        }
    }
    window.show();
    return app.exec();
}
