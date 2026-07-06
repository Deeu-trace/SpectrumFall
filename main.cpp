#include "game.h"
#include "UIButtonEffects.h"
#include "ThemeManager.h"
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 加载 Qt 内置中文翻译（QColorDialog / QFileDialog / QMessageBox 等）
    QTranslator qtTranslator;
    if (qtTranslator.load(QStringLiteral("qt_zh_CN"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // 初始化主题管理器（自动加载 QSS 并应用默认主题）
    ThemeManager::instance();

    // 初始化按钮音效 + 悬停辉光动效
    UIButtonEffects::install(&app);

    MainWindow window;
    window.show();
    return app.exec();
}
