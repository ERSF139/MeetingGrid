#include "SignalingServer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

namespace {
void printUsage()
{
    qInfo().noquote() << QStringLiteral(
        "MeetingGridServer - WebSocket 信令服务端\n"
        "用法: MeetingGridServer [--host=IP] [--port=PORT]\n"
        "  --host=IP    监听地址，默认 0.0.0.0（所有网卡）\n"
        "  --port=PORT  监听端口，默认 8090");
}
} // namespace

int main(int argc, char *argv[])
{
    // 后台运行 / 输出重定向时也强制日志写入 stderr（否则 Qt 在非控制台场景仅写调试器）
    qputenv("QT_FORCE_STDERR_LOGGING", "1");

    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MeetingGridServer"));
    app.setOrganizationName(QStringLiteral("Marco"));

    QString host = QStringLiteral("0.0.0.0");
    quint16 port = 8090;

    const QStringList args = app.arguments();
    for (const QString &arg : args) {
        if (arg.startsWith(QLatin1String("--port="))) {
            bool ok = false;
            const quint16 p = arg.mid(7).toUShort(&ok);
            if (ok && p > 0)
                port = p;
        } else if (arg.startsWith(QLatin1String("--host="))) {
            host = arg.mid(7);
        } else if (arg == QLatin1String("--help") || arg == QLatin1String("-h")) {
            printUsage();
            return 0;
        }
    }

    SignalingServer server(port);
    if (!server.start(host))
        return 1;

    qInfo().noquote() << QStringLiteral("按 Ctrl+C 停止服务");
    return app.exec();
}
