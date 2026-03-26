#include "clienthandler.h"
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QHostAddress>

const QString VALID_USER = "zhangsan";
const QString VALID_PWD = "123456";
const QString SHARE_ROOT_PATH = "E:/fileShared";

// 初始化静态成员变量
QMap<QString, FileStats> ClientHandler::s_fileStatsMap;

QMap<QString, FileStats>& ClientHandler::getFileStatsMap() {
    return s_fileStatsMap;
}

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_socketDescriptor(socketDescriptor)
    , m_state(WaitFileName)
    , m_totalBytes(0)
    , m_bytesReceived(0)
    , m_file(nullptr)
    , m_loggedIn(false)
    , m_currentPath("/")
{
    m_socket = new QTcpSocket(this);
    
    if (!m_socket->setSocketDescriptor(m_socketDescriptor)) {
        emit logMessage(QString("客户端 %1 套接字设置失败").arg(m_socketDescriptor));
        deleteLater();
        return;
    }

    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), 
            this, &ClientHandler::onErrorOccurred);

    m_inStream.setDevice(m_socket);
    m_inStream.setVersion(QDataStream::Qt_5_15);
    
    emit logMessage(QString("客户端 %1 初始化完成").arg(m_socketDescriptor));
}

ClientHandler::~ClientHandler()
{
    if (m_file && m_file->isOpen()) {
        m_file->close();
    }
    if (m_file) {
        delete m_file;
        m_file = nullptr;
    }
    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
}

void ClientHandler::onReadyRead()
{
    m_recvBuffer.append(m_socket->readAll());

    while (true) {
        if (m_recvBuffer.size() < static_cast<int>(sizeof(quint32))) {
            return; 
        }

        quint32 blockSize = 0;
        QDataStream tempStream(&m_recvBuffer, QIODevice::ReadOnly);
        tempStream >> blockSize;

        if (m_recvBuffer.size() < static_cast<int>(sizeof(quint32) + blockSize)) {
            return;
        }

        QByteArray data = m_recvBuffer.mid(sizeof(quint32), blockSize);
        m_recvBuffer.remove(0, sizeof(quint32) + blockSize);

        processCommand(data);
    }
}

void ClientHandler::processCommand(const QByteArray &data)
{
    QString request = QString::fromUtf8(data);
    emit logMessage(QString("收到指令 [%1]: %2").arg(m_socket->peerPort()).arg(request));

    if (request.startsWith("LOGIN|")) {
        QStringList parts = request.split('|');
        if (parts.size() == 3) {
            QString user = parts[1];
            QString pwd = parts[2];
            if (user == VALID_USER && pwd == VALID_PWD) {
                m_loggedIn = true;
                sendResponse("LOGIN_OK");
                emit logMessage(QString("用户 %1 登录成功").arg(user));
            } else {
                sendResponse("LOGIN_FAIL");
                emit logMessage(QString("用户 %1 登录失败").arg(user));
            }
        } else {
            sendResponse("ERROR|Invalid Login Format");
        }
        return;
    }

    if (!m_loggedIn) {
        sendResponse("ERROR|Not Logged In");
        return;
    }

    if (request.startsWith("LIST|")) {
        QString pathArg = request.mid(5);
        handleListRequest(pathArg);
        return;
    }

    if (request.startsWith("DOWNLOAD|")) {
        QString args = request.mid(9);
        int splitIndex = args.indexOf('|');
        if (splitIndex != -1) {
            QString path = args.left(splitIndex);
            QString fileName = args.mid(splitIndex + 1);
            handleDownloadRequest(path, fileName);
        } else {
            emit logMessage("警告：收到格式不完整的下载请求，尝试按根目录处理");
            handleDownloadRequest("/", args);
        }
        return;
    }

    emit logMessage("收到未知指令：" + request);
    sendResponse("ERROR|Unknown Command");
}

void ClientHandler::handleListRequest(const QString &virtualPath)
{
    QString physicalPath = SHARE_ROOT_PATH;
    
    if (!virtualPath.isEmpty() && virtualPath != "/") {
        QString relative = virtualPath;
        if (relative.startsWith("/")) {
            relative = relative.mid(1);
        }
        relative = QDir::fromNativeSeparators(relative); 
        QDir root(SHARE_ROOT_PATH);
        physicalPath = root.filePath(relative);
    }

    physicalPath = QDir::cleanPath(physicalPath);

    QDir dir(physicalPath);
    if (!dir.exists()) {
        emit logMessage("路径不存在：" + physicalPath);
        sendResponse("ERROR|Path Not Found");
        return;
    }

    QJsonArray array;
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst);
    
    for (const QFileInfo &fi : list) {
        QJsonObject obj;
        obj["name"] = fi.fileName();
        obj["isDir"] = fi.isDir();
        obj["size"] = (qint64)fi.size();
        
        // 新增：如果不是目录，附加统计信息
        if (!fi.isDir()) {
            // 构建用于查找统计信息的 Key (相对路径/文件名)
            QString key = virtualPath;
            if (!key.endsWith("/")) key += "/";
            key += fi.fileName();
            
            // 从全局静态 Map 中获取统计
            if (s_fileStatsMap.contains(key)) {
                FileStats stats = s_fileStatsMap[key];
                obj["downloadCount"] = stats.downloadCount;
                obj["sourceIp"] = stats.lastSourceIp;
                // 【新增】调试日志：确认统计命中
                emit logMessage(QString("[Stats Hit] 文件 [%1] 统计：次数=%2, IP=%3").arg(key).arg(stats.downloadCount).arg(stats.lastSourceIp));
            } else {
                // 默认值
                obj["downloadCount"] = 0;
                obj["sourceIp"] = "-";
                // 【新增】调试日志：确认统计未命中
                emit logMessage(QString("[Stats Miss] 文件 [%1] 未找到统计信息，返回默认值").arg(key));
            }
        } else {
            // 目录不需要统计
            obj["downloadCount"] = 0;
            obj["sourceIp"] = "-";
        }
        
        array.append(obj);
    }

    QJsonDocument doc(array);
    QString response = "FILE_LIST|" + QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    sendResponse(response);
    emit logMessage(QString("已发送文件列表：%1 (共%2项)").arg(physicalPath).arg(array.size()));
}

void ClientHandler::handleDownloadRequest(const QString &virtualPath, const QString &fileName)
{
    if (fileName.isEmpty()) {
        sendResponse("ERROR|Empty Filename");
        return;
    }

    QString physicalPath = SHARE_ROOT_PATH;
    
    if (!virtualPath.isEmpty() && virtualPath != "/") {
        QString relative = virtualPath;
        if (relative.startsWith("/")) {
            relative = relative.mid(1);
        }
        relative = QDir::fromNativeSeparators(relative);
        QDir root(SHARE_ROOT_PATH);
        physicalPath = root.filePath(relative);
    }

    physicalPath = QDir::cleanPath(physicalPath);
    
    QDir dir(physicalPath);
    QString fullPath = dir.filePath(fileName);

    QFileInfo fileInfo(fullPath);
    if (!fileInfo.exists() || fileInfo.isDir()) {
        sendResponse("ERROR|File Not Found");
        emit logMessage("文件不存在：" + fullPath + " (基于虚拟路径：" + virtualPath + ")");
        return;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        sendResponse("ERROR|Cannot Open File");
        emit logMessage("无法打开文件：" + fullPath);
        return;
    }

    qint64 fileSize = file.size();
    QByteArray fileData = file.readAll();
    file.close();

    // --- 新增：在发送文件前，更新统计信息 ---
    QString statsKey = virtualPath;
    if (!statsKey.endsWith("/")) statsKey += "/";
    statsKey += fileName;
    
    // 获取客户端 IP
    QString clientIp = m_socket->peerAddress().toString();
    
    // 更新全局统计
    s_fileStatsMap[statsKey].downloadCount++;
    s_fileStatsMap[statsKey].lastSourceIp = clientIp;
    
    // 【新增】调试日志：确认统计已更新
    emit logMessage(QString("[Stats Update] 文件 [%1] 被下载，Key=[%2], 累计次数：%3, 来源 IP: %4")
                    .arg(fileName)
                    .arg(statsKey)
                    .arg(s_fileStatsMap[statsKey].downloadCount)
                    .arg(clientIp));
    // --------------------------------------

    QString header = QString("FILE_START|%1|%2").arg(fileName).arg(fileSize);
    sendResponse(header);

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << (quint32)fileData.size();
    out.writeRawData(fileData.constData(), fileData.size());
    
    m_socket->write(block);
    emit logMessage(QString("文件发送完成：%1 (%2 bytes)").arg(fileName).arg(fileSize));
}

void ClientHandler::sendResponse(const QString &cmd)
{
    QByteArray data = cmd.toUtf8();
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << (quint32)data.size();
    out.writeRawData(data.constData(), data.size());
    m_socket->write(block);
}

void ClientHandler::onDisconnected()
{
    emit logMessage(QString("客户端 %1 断开连接").arg(m_socketDescriptor));
    if (m_file && m_file->isOpen()) {
        m_file->close();
    }
    emit disconnected();
}

void ClientHandler::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit logMessage(QString("客户端 %1 发生错误：%2").arg(m_socketDescriptor).arg(m_socket->errorString()));
}