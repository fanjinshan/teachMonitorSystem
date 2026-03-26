#include "fileserver.h"
#include "clienthandler.h"
#include <QDebug>

FileServer::FileServer(QObject *parent)
    : QTcpServer(parent)
{
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
    // 创建新的客户端处理器
    ClientHandler *client = new ClientHandler(socketDescriptor, this);
    
    // 连接信号以跟踪断开连接
    connect(client, &ClientHandler::disconnected, this, [this, client]() {
        emit clientDisconnected(client->getSocketDescriptor());
        m_clients.removeAll(client);
        client->deleteLater();
    });

    connect(client, &ClientHandler::logMessage, this, &FileServer::logMessage);

    m_clients.append(client);
    emit clientConnected(socketDescriptor);
    
    emit logMessage(QString("新客户端连接: %1").arg(socketDescriptor));
}

void FileServer::onClientDisconnected()
{
    // 槽函数逻辑已在 lambda 中处理，此处保留以备扩展
}