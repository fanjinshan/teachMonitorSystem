#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QDataStream>
#include <QByteArray>
#include <QMap> // 新增引入

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
    
    // 新增：获取全局统计数据的静态方法（方便在 handleListRequest 中访问）
    static QMap<QString, FileStats>& getFileStatsMap();

signals:
    void disconnected();
    void logMessage(const QString &message);
    void fileReceived(const QString &fileName, qint64 fileSize);

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

    qintptr m_socketDescriptor;
    QTcpSocket *m_socket;
    QByteArray m_recvBuffer;

    enum ReceiveState {
        WaitFileName,
        WaitFileSize,
        ReceivingData
    };
    
    ReceiveState m_state;
    QString m_currentFileName;
    qint64 m_totalBytes;
    qint64 m_bytesReceived;
    QFile *m_file;
    QDataStream m_inStream;

    bool m_loggedIn;
    QString m_currentPath;
    
    // 新增：全局静态统计表，key 为文件相对路径，value 为统计信息
    static QMap<QString, FileStats> s_fileStatsMap;
};

#endif // CLIENTHANDLER_H