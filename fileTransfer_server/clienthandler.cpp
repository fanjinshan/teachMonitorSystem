#include "clienthandler.h"
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QHostAddress>
#include <QBuffer>
#include <QImage>

const QString VALID_USER = "zhangsan";
const QString VALID_PWD = "123456";
const QString SHARE_ROOT_PATH = "E:/fileShared";

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_socketDescriptor(socketDescriptor)
    , m_state(WaitFileName)
    , m_totalBytes(0)
    , m_bytesReceived(0)
    , m_file(nullptr)
    , m_loggedIn(false)
    , m_currentPath("/")
    , m_status(StudentStatus::Offline)
    , m_screenshotSize(0)
    , m_monitorJsonSize(0)
    , m_monitorImageSize(0)
    , m_monitorStep(0)
{
    m_socket = new QTcpSocket(this);
    
    if (!m_socket->setSocketDescriptor(m_socketDescriptor)) {
        qDebug() << "[Server Debug] 客户端" << socketDescriptor << "套接字设置失败";
        deleteLater();
        return;
    }

    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), 
            this, &ClientHandler::onErrorOccurred);

    m_inStream.setDevice(m_socket);
    m_inStream.setVersion(QDataStream::Qt_5_15);
    
    // 初始化为离线
    m_status = StudentStatus::Offline;
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

void ClientHandler::processCommand(const QByteArray &data)
{
    QString request = QString::fromUtf8(data);

    if (request.startsWith("LOGIN|")) {
        // 格式：LOGIN|UserID|Password (假设密码固定或简单验证)
        QStringList parts = request.split('|');
        if (parts.size() >= 3) {
            QString userId = parts[1];
            // 简单验证：这里假设 ID 即用户名，密码固定或忽略，实际应查库
            m_studentId = userId;
            StudentInfo info = DatabaseManager::instance().getStudentInfo(userId);
            if (!info.id.isEmpty()) {
                m_studentName = info.name;
                m_loggedIn = true;
                m_status = StudentStatus::Online_Normal;
                DatabaseManager::instance().updateStudentStatus(userId, true);
                sendResponse("LOGIN_OK|" + m_studentName);
                emit logMessage(QString("[监控] 学生上线：%1 (%2)").arg(m_studentName).arg(userId));
                emit studentStatusChanged(m_studentId, m_status, "", QByteArray());
            } else {
                // 自动注册未知学生 (可选策略)
                m_studentName = "未知学生_" + userId;
                DatabaseManager::instance().registerStudent(userId, m_studentName, m_socket->peerAddress().toString());
                m_loggedIn = true;
                m_status = StudentStatus::Online_Normal;
                sendResponse("LOGIN_OK|" + m_studentName);
                emit logMessage(QString("[监控] 新学生注册并上线：%1").arg(userId));
                emit studentStatusChanged(m_studentId, m_status, "", QByteArray());
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

    if (request.startsWith("HEARTBEAT|")) {
        // 心跳包，更新在线状态
        emit heartbeatReceived(m_studentId);
        return;
    }

    if (request.startsWith("WARNING_REPORT|")) {
        // 学生上报警告：WARNING_REPORT|{"appId":"douyin", "appName":"抖音", "time":...}
        QString jsonStr = request.mid(15);
        handleWarningReport(jsonStr);
        return;
    }

    if (request.startsWith("SCREENSHOT_START|")) {
        // 准备接收截图：SCREENSHOT_START|size
        QStringList parts = request.split('|');
        if (parts.size() == 2) {
            m_screenshotSize = parts[1].toLongLong();
            m_screenshotBuffer.clear();
            m_screenshotBuffer.reserve(m_screenshotSize);
            m_state = ReceivingScreenshot;
            emit logMessage(QString("[监控] 正在接收 %1 的截图...").arg(m_studentName));
        }
        return;
    }

    if (request.startsWith("MONITOR_START|")) {
        // 格式：MONITOR_START|jsonSize|imageSize
        // 注意：这个命令头本身是在文本协议流里解析出来的
        // 接下来的 json 和 image 数据是二进制流，会触发 onReadyRead 的 ReceivingMonitorData 分支
        QStringList parts = request.mid(14).split('|');
        if (parts.size() == 2) {
            m_monitorJsonSize = parts[0].toUInt();
            m_monitorImageSize = parts[1].toUInt();
            m_monitorJsonBuffer.clear();
            m_monitorImageBuffer.clear();
            m_monitorStep = 0;
            m_state = ReceivingMonitorData;
            emit logMessage(QString("[监控] 准备接收学生 %1 的违规报告 (JSON:%2, Img:%3)...").arg(m_studentName).arg(m_monitorJsonSize).arg(m_monitorImageSize));
        }
        return;
    }

    if (request.startsWith("LIST|")) {
        QString pathArg = request.mid(5);
        emit logMessage(QString("[请求] 用户请求文件列表，路径：%1").arg(pathArg.isEmpty() ? "/" : pathArg));
        handleListRequest(pathArg);
        return;
    }

    if (request.startsWith("DOWNLOAD|")) {
        QString args = request.mid(9);
        int splitIndex = args.indexOf('|');
        QString path = "/";
        QString fileName = args;
        if (splitIndex != -1) {
            path = args.left(splitIndex);
            fileName = args.mid(splitIndex + 1);
        }
        emit logMessage(QString("[请求] 用户请求下载文件：%1 (路径:%2)").arg(fileName).arg(path));
        handleDownloadRequest(path, fileName);
        return;
    }
    
    // 处理二进制数据 (截图内容)
    if (m_state == ReceivingScreenshot) {
        // 注意：这里的逻辑需要结合 onReadyRead 中的二进制读取，通常协议设计会将二进制单独处理
        // 由于简化，我们在 onReadyRead 中专门处理二进制流，这里仅做占位
        return;
    }
}

void ClientHandler::handleWarningReport(const QString &jsonData) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Parse warning report failed:" << error.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString appName = obj["appName"].toString();
    QString details = obj["details"].toString();

    // 更新状态
    m_status = StudentStatus::Online_Violated;
    
    // 记录数据库
    DatabaseManager::instance().logWarning(m_studentId, appName, details);
    
    emit logMessage(QString("[警报] 学生 %1 违规使用：%2").arg(m_studentName).arg(appName));
    
    // 通知界面更新，此时截图可能还在发送中，先标记为警告状态
    emit studentStatusChanged(m_studentId, m_status, appName, QByteArray());
    
    // 主动请求截图 (协议扩展：服务器回复 GET_SCREENSHOT)
    sendResponse("GET_SCREENSHOT");
}

void ClientHandler::handleScreenshotData(const QByteArray &rawData) {
    if (rawData.size() != m_screenshotSize) {
        qWarning() << "Screenshot size mismatch";
        m_state = WaitFileName; // 重置状态
        return;
    }

    // 保存截图到数据库
    DatabaseManager::instance().saveScreenshotRecord(m_studentId, rawData);
    
    // 通知界面更新，带上截图数据
    emit studentStatusChanged(m_studentId, m_status, "", rawData);
    
    emit logMessage(QString("[监控] 收到 %1 的违规截图，大小：%2 bytes").arg(m_studentName).arg(rawData.size()));
    
    m_state = WaitFileName; // 恢复状态
    m_screenshotBuffer.clear();
}

void ClientHandler::onReadyRead()
{
    // 【核心修复】重构接收逻辑，严格区分“命令解析阶段”和“二进制数据接收阶段”
    
    // 1. 如果正在接收监控数据的二进制部分 (JSON 或 Image)
    if (m_state == ReceivingMonitorData) {
        // 我们需要接收两部分数据：JSON 和 Image，每部分都是 4 字节长度 + 数据
        // m_monitorStep: 0=等待 JSON 长度，1=等待 JSON 数据，2=等待图片长度，3=等待图片数据
        
        while (m_socket->bytesAvailable() > 0) {
            if (m_monitorStep == 0) {
                // 等待 JSON 长度头 (4 字节)
                if (m_socket->bytesAvailable() >= 4) {
                    QByteArray h = m_socket->read(4);
                    QDataStream ds(&h, QIODevice::ReadOnly);
                    ds.setVersion(QDataStream::Qt_5_15);
                    quint32 len;
                    ds >> len;
                    if (len > 10 * 1024 * 1024) { // 安全限制：JSON 不可能超过 10MB
                        qWarning() << "Invalid JSON length detected:" << len;
                        m_state = WaitFileName;
                        return;
                    }
                    m_monitorJsonSize = len;
                    m_monitorStep = 1;
                    // 继续循环，尝试立即读取 JSON 数据
                } else {
                    break; // 数据不足，等待下一次 readyRead
                }
            }
            
            if (m_monitorStep == 1) {
                // 等待 JSON 数据
                if (m_socket->bytesAvailable() >= (int)m_monitorJsonSize) {
                    m_monitorJsonBuffer = m_socket->read(m_monitorJsonSize);
                    m_monitorStep = 2;
                    // 继续循环，尝试立即读取图片长度
                } else {
                    break; // 数据不足
                }
            }
            
            if (m_monitorStep == 2) {
                // 等待图片长度头 (4 字节)
                if (m_socket->bytesAvailable() >= 4) {
                    QByteArray h = m_socket->read(4);
                    QDataStream ds(&h, QIODevice::ReadOnly);
                    ds.setVersion(QDataStream::Qt_5_15);
                    quint32 len;
                    ds >> len;
                    if (len > 50 * 1024 * 1024) { // 安全限制：图片不可能超过 50MB
                        qWarning() << "Invalid Image length detected:" << len;
                        m_state = WaitFileName;
                        return;
                    }
                    m_monitorImageSize = len;
                    m_monitorStep = 3;
                    // 继续循环
                } else {
                    break;
                }
            }
            
            if (m_monitorStep == 3) {
                // 等待图片数据
                if (m_socket->bytesAvailable() >= (int)m_monitorImageSize) {
                    m_monitorImageBuffer = m_socket->read(m_monitorImageSize);
                    
                    // 数据全部接收完毕，处理业务逻辑
                    handleMonitorReport(m_monitorJsonBuffer, m_monitorImageBuffer);
                    
                    // 重置状态
                    m_state = WaitFileName;
                    m_monitorStep = 0;
                    m_monitorJsonBuffer.clear();
                    m_monitorImageBuffer.clear();
                    m_monitorJsonSize = 0;
                    m_monitorImageSize = 0;
                    return; // 处理完成，退出循环
                } else {
                    break; // 数据不足，等待下一次
                }
            }
        }
        return; // 二进制接收模式下，不执行下方的文本协议解析
    }

    // 2. 如果正在接收旧协议的截图数据 (兼容保留)
    if (m_state == ReceivingScreenshot) {
        QByteArray data = m_socket->readAll();
        m_screenshotBuffer.append(data);
        
        if (m_screenshotBuffer.size() >= m_screenshotSize) {
            handleScreenshotData(m_screenshotBuffer);
        }
        return;
    }

    // 3. 正常文本命令接收逻辑 (带长度前缀)
    m_recvBuffer.append(m_socket->readAll());

    while (true) {
        if (m_recvBuffer.size() < static_cast<int>(sizeof(quint32))) {
            return; 
        }

        // 如果是第一次解析块大小
        if (m_currentBlockSize == 0) {
            QDataStream tempStream(&m_recvBuffer, QIODevice::ReadOnly);
            tempStream.setVersion(QDataStream::Qt_5_15);
            tempStream >> m_currentBlockSize;
            
            // 安全检查：防止恶意大包
            if (m_currentBlockSize > 10 * 1024 * 1024) {
                qWarning() << "Received oversized packet header:" << m_currentBlockSize;
                m_socket->disconnectFromHost();
                return;
            }
        }

        if (m_recvBuffer.size() < static_cast<int>(sizeof(quint32) + m_currentBlockSize)) {
            return; // 数据不完整，等待后续
        }

        // 提取完整数据包
        QByteArray data = m_recvBuffer.mid(sizeof(quint32), m_currentBlockSize);
        m_recvBuffer.remove(0, sizeof(quint32) + m_currentBlockSize);
        m_currentBlockSize = 0; // 重置，准备解析下一个包

        processCommand(data);
    }
}

void ClientHandler::onDisconnected()
{
    m_status = StudentStatus::Offline;
    if (!m_studentId.isEmpty()) {
        DatabaseManager::instance().updateStudentStatus(m_studentId, false);
        emit studentStatusChanged(m_studentId, m_status, "", QByteArray());
        emit logMessage(QString("[监控] 学生 %1 下线").arg(m_studentName));
    }
    emit disconnected();
}

void ClientHandler::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
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
        emit logMessage("[响应] 发送文件列表失败：路径不存在");
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
        
        if (!fi.isDir()) {
            // 【修复 2】统一键构建逻辑
            QString key = virtualPath;
            if (!key.endsWith("/")) key += "/";
            key += fi.fileName();
            key.replace("\\", "/");
            key = QDir::cleanPath(key);
            if (key.startsWith("//")) key = key.mid(1);
            if (!key.startsWith("/")) key = "/" + key;

            FileStatRecord stats = DatabaseManager::instance().getFileStat(key);
            
            qDebug() << "[DB Query] File:" << key << "Count:" << stats.downloadCount << "IP:" << stats.lastSourceIp;
            
            obj["downloadCount"] = stats.downloadCount;
            obj["sourceIp"] = stats.lastSourceIp;
        } else {
            obj["downloadCount"] = 0;
            obj["sourceIp"] = "-";
        }
        
        array.append(obj);
    }

    QJsonDocument doc(array);
    QString response = "FILE_LIST|" + QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    sendResponse(response);
    emit logMessage(QString("[发送] 已发送文件列表：%1 (共%2项)").arg(physicalPath).arg(array.size()));
}

void ClientHandler::handleDownloadRequest(const QString &virtualPath, const QString &fileName)
{
    if (fileName.isEmpty()) {
        emit logMessage("[发送] 下载请求失败：文件名为空");
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
        emit logMessage(QString("[发送] 下载请求失败：文件不存在 (%1)").arg(fileName));
        sendResponse("ERROR|File Not Found");
        return;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(QString("[发送] 下载请求失败：无法打开文件 (%1)").arg(fileName));
        sendResponse("ERROR|Cannot Open File");
        return;
    }

    qint64 fileSize = file.size();
    QByteArray fileData = file.readAll();
    file.close();

    // 【修复 2】统一键构建逻辑并记录日志
    QString statsKey = virtualPath;
    if (!statsKey.endsWith("/")) statsKey += "/";
    statsKey += fileName;
    statsKey.replace("\\", "/");
    statsKey = QDir::cleanPath(statsKey);
    if (statsKey.startsWith("//")) statsKey = statsKey.mid(1);
    if (!statsKey.startsWith("/")) statsKey = "/" + statsKey;
    
    QString clientIp = m_socket->peerAddress().toString();
    
    bool updateOk = DatabaseManager::instance().updateFileStat(statsKey, clientIp);
    emit logMessage(QString("[下载] 学生下载了文件：%1 [DB Key:%2] [更新结果:%3]").arg(fileName).arg(statsKey).arg(updateOk ? "成功" : "失败"));

    QString header = QString("FILE_START|%1|%2").arg(fileName).arg(fileSize);
    sendResponse(header);

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << (quint32)fileData.size();
    out.writeRawData(fileData.constData(), fileData.size());
    
    m_socket->write(block);
    emit logMessage(QString("[发送] 文件发送完成：%1 (大小:%2 bytes)").arg(fileName).arg(fileSize));
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

void ClientHandler::handleMonitorReport(const QByteArray &jsonData, const QByteArray &imageData)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[Monitor] Parse monitor report failed:" << error.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    QString appName = obj["appName"].toString();
    QString timeStr = obj["time"].toString();

    StudentStatus newStatus = m_status; 
    bool shouldLogViolation = false;
    bool isCountOnly = false;

    if (type == "VIOLATION_COUNT_ONLY") {
        isCountOnly = true;
        shouldLogViolation = true; 
        qDebug() << "[COUNT ONLY] Received count increment request for app:" << appName;
    }
    else if (type == "STATUS_RECOVERY") {
        newStatus = StudentStatus::Online_Normal;
        qDebug() << "[RECOVERY] Student " << m_studentName << " closed violated app.";
    } 
    else if (type == "VIOLATION_REPORT" && appName != "Periodic_Monitor" && appName != "Live_Request_Response") {
        newStatus = StudentStatus::Online_Violated;
        shouldLogViolation = true;
        qDebug() << "[VIOLATION FULL] Detected app: " << appName << ". Full report with screenshot.";
    } 
    else {
        newStatus = StudentStatus::Online_Normal;
    }
    
    if (shouldLogViolation) {
        QString details = "自动检测违规应用：" + appName + " 时间：" + timeStr;
        if (isCountOnly) {
            details += " (仅计数模式)";
            DatabaseManager::instance().logWarning(m_studentId, appName, details);
        } else {
            DatabaseManager::instance().logWarning(m_studentId, appName, details);
            // 【修改】即使是完整上报，为了配合前端“不自动弹窗”的需求，我们也可以选择是否存图到 DB
            // 这里保留存图逻辑，以便老师手动点击查看历史或实时图时有数据
            DatabaseManager::instance().insertMonitorRecord(m_studentId, appName, details, imageData);
        }
    }
    
    // 【核心修改】发送信号给 UI
    // 需求：只切换状态栏为“违规”，不弹窗。
    // 策略：如果是违规状态，我们发送状态信号，但故意将截图数据置空（或者前端逻辑忽略它）
    // 为了防止前端旧逻辑误判弹窗，这里统一处理：
    // 1. 如果是 CountOnly -> 状态设为 Online_Violated (让表格变红)，截图传空。
    // 2. 如果是 ViolationReport -> 状态设为 Online_Violated (让表格变红)，截图传空 (因为不让自动弹)。
    // 只有当老师主动请求 (Live_Request_Response) 时，才传递截图数据。
    
    QByteArray screenshotToSend;
    if (type == "Live_Request_Response") {
        screenshotToSend = imageData; // 只有老师主动看的才传图
    } else {
        screenshotToSend = QByteArray(); // 违规自动上报的，不传图，防止前端自动弹窗
    }

    // 强制更新状态为违规，确保表格变红
    if (type == "VIOLATION_COUNT_ONLY" || (type == "VIOLATION_REPORT" && appName != "Periodic_Monitor")) {
        newStatus = StudentStatus::Online_Violated;
    }

    emit studentStatusChanged(m_studentId, newStatus, appName, screenshotToSend);
    
    QString logType = isCountOnly ? "计数增加" : ((newStatus == StudentStatus::Online_Violated) ? "警报" : "状态更新");
    emit logMessage(QString("[%1] 学生 %2 事件 (应用:%3, 类型:%4)")
                    .arg(logType, m_studentName, appName, type));
}
