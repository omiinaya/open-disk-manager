#include "main_window.hpp"
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QTextStream>
#include <iostream>

using namespace opm::gui;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // Application info
    QApplication::setApplicationName("Open Partition Manager");
    QApplication::setApplicationDisplayName("OPM");
    QApplication::setOrganizationName("OPM Project");
    QApplication::setApplicationVersion("0.2.0");
    
    // Set application icon
    // app.setWindowIcon(QIcon(":/icons/opm.png"));
    
    // Set style
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    
    // Set stylesheet for dark theme
    QFile styleFile(":/styles/dark.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QTextStream stream(&styleFile);
        app.setStyleSheet(stream.readAll());
    }
    
    // Create and show main window
    MainWindow window;
    window.initialize();
    window.show();
    
    return app.exec();
}
