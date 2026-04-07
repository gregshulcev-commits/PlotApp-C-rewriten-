#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("plotapp");
    app.setApplicationDisplayName("PlotApp");
    QApplication::setDesktopFileName("plotapp");

    const auto icon = QIcon::fromTheme("plotapp");
    if (!icon.isNull()) app.setWindowIcon(icon);

    plotapp::ui::MainWindow window;
    window.resize(1400, 900);
    if (!icon.isNull()) window.setWindowIcon(icon);
    window.show();
    return app.exec();
}
