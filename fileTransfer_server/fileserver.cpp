#include "fileserver.h"
#include "clienthandler.h"
#include <QDebug>
#include <QThreadPool>

FileServer::FileServer(QObject *parent)
    : QTcpServer(parent)
{
    m_threadPool.setMaxThreadCount(QThreadPool::globalInstance()->maxThreadCount());
}

FileServer::~FileServer()
{
    stop();
}

bool FileServer::start(quint16 port)
{
    if (isListening()) {
        return true;
    }

    if (!listen(QHostAddress::Any, port)) {
        emit logMessage(QString("服务器启动失败: %1").arg(errorString()));
        return false;
    }

    emit logMessage(QString("服务器已启动，监听端口: %1").arg(port));
    return true;
}

void FileServer::stop()
{
    if (isListening()) {
        close();
        // 清理所有客户端连接
        for (const auto &client : m_clients) {
            if (client) {
                client->deleteLater();
            }
        }
        m_clients.clear();
        emit logMessage("服务器已停止");
    }
}

void FileServer::incomingConnection(qintptr socketDescriptor)
{
    // 创建 Handler
    ClientHandler *client = new ClientHandler(socketDescriptor, this);
    
    // 获取对端地址和端口用于日志区分单机实例
    QTcpSocket tempSocket;
    tempSocket.setSocketDescriptor(socketDescriptor);
    QString peerInfo = QString("%1:%2").arg(tempSocket.peerAddress().toString()).arg(tempSocket.peerPort());
    tempSocket.setSocketDescriptor(-1); // 释放描述符，避免析构时关闭

    connect(client, &ClientHandler::disconnected, this, [this, client, peerInfo]() {
        emit clientDisconnected(client->getSocketDescriptor());
        m_clients.removeAll(client);
        client->deleteLater();
        emit logMessage(QString("客户端断开：%1").arg(peerInfo));
    });

    connect(client, &ClientHandler::logMessage, this, &FileServer::logMessage);
    
    // 转发学生状态信号
    connect(client, &ClientHandler::studentStatusChanged, this, &FileServer::onStudentStatusChanged);

    m_clients.append(client);
    emit clientConnected(socketDescriptor);
    
    // 日志中显示端口，方便区分同一 IP 的不同学生进程
    emit logMessage(QString("新客户端连接 (总数:%1): %2").arg(m_clients.size()).arg(peerInfo));
}

void FileServer::onStudentStatusChanged(const QString &id, StudentStatus status, const QString &app, const QByteArray &img) {
    emit studentStatusUpdated(id, status, app, img);
}

void FileServer::onClientDisconnected()
{
    // 槽函数逻辑已在 lambda 中处理，此处保留以备扩展
}