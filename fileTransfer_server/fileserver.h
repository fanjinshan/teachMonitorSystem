#ifndef FILESERVER_H
#define FILESERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QPointer>

// 前向声明
class ClientHandler;

class FileServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit FileServer(QObject *parent = nullptr);
    ~FileServer();

    // 启动服务器
    bool start(quint16 port);
    // 停止服务器
    void stop();

signals:
    void clientConnected(qintptr socketDescriptor);
    void clientDisconnected(qintptr socketDescriptor);
    void logMessage(const QString &message);

protected:
    // 重写入站连接处理
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected();

private:
    QList<QPointer<ClientHandler>> m_clients;
};

#endif // FILESERVER_H