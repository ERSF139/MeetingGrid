#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MeetingGrid"));
    app.setOrganizationName(QStringLiteral("Marco"));

    // 统一应用字体（中文环境下回退到系统默认中文字体）
    QFont font = app.font();
    font.setPointSize(10);
    app.setFont(font);

    // 可选参数：--members=N 启动时演示 N 人宫格（如 1/4/9/16/25）
    int initialMembers = -1;
    const QStringList args = app.arguments();
    for (const QString &arg : args) {
        if (arg.startsWith(QLatin1String("--members="))) {
            const QString value = arg.mid(10);
            bool ok = false;
            const int n = value.toInt(&ok);
            if (ok && n > 0)
                initialMembers = n;
        }
    }

    MainWindow w(initialMembers);
    w.show();
    return app.exec();
}
