#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QDataStream>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFile>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QWidget>
#include <QTreeWidget>
#include <QHeaderView>

QT_BEGIN_NAMESPACE
namespace Ui {
class client;
}
QT_END_NAMESPACE

// 定义简单的文件信息结构
struct FileInfo {
    QString name;
    bool isDir;
    qint64 size;
    QDateTime lastModified; // 新增：修改时间
    int downloadCount;      // 新增：下载次数 (本地统计或服务器返回)
    QString sourceIp;       // 新增：来源 IP
};

class client : public QMainWindow
{
    Q_OBJECT

public:
    explicit client(QWidget *parent = nullptr);
    ~client();

private slots:
    void on_loginBt_clicked();
    void onConnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError);
    void on_fileList_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_downloadBt_clicked();
    void onDisconnected();
    void onBytesWritten(qint64 bytes);
    
    void on_downLoadBt_clicked();
    void on_settingBt_clicked();
    
    // 新增：补全刷新和退出按钮的槽函数声明
    void on_refreshBt_clicked();
    void on_quitBt_clicked();

private:
    // 辅助函数
    void connectToServer();
    void requestFileList(const QString &path = "");
    void parseFileListData(const QByteArray &data);
    
    void startDownload(const QString &fileName, qint64 size);
    void startDownloadRequest(const QString &fileName);
    
    // 状态管理
    enum ClientState {
        State_Disconnected,
        State_Connecting,
        State_LoggedIn,
        State_ReceivingFile
    };
    
    // 列索引枚举
    enum ColumnType {
        Col_Name = 0,
        Col_Type,
        Col_Size,
        Col_Time,
        Col_Downloads,
        Col_Source,
        Col_Count
    };

    Ui::client *ui;
    QTcpSocket *m_socket;
    QString m_currentPath;
    QString m_loggedInUser;
    QString m_serverIp; // 新增：记录服务器 IP
    
    QFile *m_localFile;
    qint64 m_fileSize;
    qint64 m_bytesReceived;
    QString m_downloadingFileName;
    ClientState m_state;
    
    // 新增：用户配置的保存路径
    QString m_savePath;
};
#endif // CLIENT_H
