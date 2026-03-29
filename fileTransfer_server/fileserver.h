#ifndef FILESERVER_H
#define FILESERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QPointer>
#include <QThreadPool>
// 新增：必须包含 ClientHandler 以识别 StudentStatus 枚举和 ClientHandler 类
#include "clienthandler.h"

class FileServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit FileServer(QObject *parent = nullptr);
    ~FileServer();

    bool start(quint16 port);
    void stop();
    
    // 获取线程池实例（可用于提交其他耗时任务）
    QThreadPool* getThreadPool() { return &m_threadPool; }

signals:
    void clientConnected(qintptr socketDescriptor);
    void clientDisconnected(qintptr socketDescriptor);
    void logMessage(const QString &message);
    // 修复：StudentStatus 现在已定义
    void studentStatusUpdated(const QString &studentId, StudentStatus status, const QString &appName, const QByteArray &screenshot);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected();
    void onStudentStatusChanged(const QString &id, StudentStatus status, const QString &app, const QByteArray &img);

private:
    // 修复：ClientHandler 现在已定义
    QList<QPointer<ClientHandler>> m_clients;
    QThreadPool m_threadPool; // 线程池
};

#endif // FILESERVER_H