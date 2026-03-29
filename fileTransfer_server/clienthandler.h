#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QDataStream>
#include <QByteArray>
#include <QMap>
#include <QThreadPool>
#include <QRunnable>
#include "databasemanager.h"

// 新增：学生状态枚举
enum class StudentStatus {
    Offline,
    Online_Normal,
    Online_Warning, // 本地警告中
    Online_Violated // 已上报老师
};

// 新增：文件统计信息结构
struct FileStats {
    int downloadCount = 0;
    QString lastSourceIp = "-";
};

class ClientHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClientHandler(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientHandler();

    qintptr getSocketDescriptor() const { return m_socketDescriptor; }

    // 新增：获取当前关联的学生 ID
    QString getStudentId() const { return m_studentId; }
    StudentStatus getStatus() const { return m_status; }

signals:
    void disconnected();
    void logMessage(const QString &message);
    void fileReceived(const QString &fileName, qint64 fileSize);
    // 新增：学生状态变化信号 (ID, 状态, 应用名，截图数据)
    void studentStatusChanged(const QString &studentId, StudentStatus status, const QString &appName, const QByteArray &screenshot);
    void heartbeatReceived(const QString &studentId);

private slots:
    void onReadyRead();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

private:
    void processCommand(const QByteArray &data);
    void handleListRequest(const QString &virtualPath);
    void handleDownloadRequest(const QString &virtualPath, const QString &fileName);
    void sendResponse(const QString &cmd);
    void processProtocol();
    
    // 新增：处理学生上报的警告
    void handleWarningReport(const QString &jsonData);
    // 新增：处理截图数据
    void handleScreenshotData(const QByteArray &rawData);
    // 【修复】添加缺失的声明
    void handleMonitorReport(const QByteArray &jsonData, const QByteArray &imageData);

    qintptr m_socketDescriptor;
    QTcpSocket *m_socket;
    QByteArray m_recvBuffer;

    enum ReceiveState {
        WaitFileName,
        WaitFileSize,
        ReceivingData,
        ReceivingScreenshot,
        ReceivingMonitorData // 【新增】接收监控数据状态
    };
    
    ReceiveState m_state;
    QString m_currentFileName;
    qint64 m_totalBytes;
    qint64 m_bytesReceived;
    QFile *m_file;
    QDataStream m_inStream;

    bool m_loggedIn;
    QString m_currentPath;
    
    // 新增：学生相关属性
    QString m_studentId;
    QString m_studentName;
    StudentStatus m_status;
    
    // 临时缓存截图数据
    QByteArray m_screenshotBuffer;
    qint64 m_screenshotSize;

    // 【新增】监控数据临时缓存
    QByteArray m_monitorJsonBuffer;
    QByteArray m_monitorImageBuffer;
    quint32 m_monitorJsonSize = 0;
    quint32 m_monitorImageSize = 0;
    int m_monitorStep = 0; // 0:WaitJson, 1:WaitImage
    
    // 【新增】通用接收缓冲区长度变量，用于处理粘包
    quint32 m_currentBlockSize = 0;

};

#endif // CLIENTHANDLER_H