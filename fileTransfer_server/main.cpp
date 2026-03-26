#include <QCoreApplication>
#include <QDebug>
#include "fileserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // 创建服务器实例
    FileServer server;

    // 连接日志信号到控制台输出
    QObject::connect(&server, &FileServer::logMessage, [](const QString &msg){
        qDebug() << "[Server Log]" << msg;
    });

    // 启动服务器，端口设为 8888，可根据需要修改
    quint16 port = 9999;
    if (!server.start(port)) {
        qCritical() << "Failed to start server on port" << port;
        return -1;
    }

    qDebug() << "Application running... Press Ctrl+C to exit.";

    return a.exec();
}
