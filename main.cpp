#include "game.h"
#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 加载全局 QSS 样式
    QFile styleFile(QStringLiteral(":/game/resources/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        styleFile.close();
    }

    MainWindow window;
    window.show();
    return app.exec();
}
