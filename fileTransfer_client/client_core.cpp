#include "client.h"
#include "ui_client.h"
#include <QTcpSocket>
#include <QUdpSocket>
#include <QMessageBox>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QDateTime>
#include <QRandomGenerator>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimer>
#include <QProcess>
#include <QWindow>
#include <QScreen>
#include <QApplication>
#include <QBuffer>
// 【修复】添加缺失的 QThread 头文件以支持 msleep
#include <QThread>
// 【新增】用于字符串匹配
#include <QStringList>

// 【新增】Windows API 头文件，用于获取前台应用信息
#ifdef Q_OS_WIN
#include <windows.h>
// 【修复】添加 psapi.h 以支持 GetModuleFileNameExW
#include <psapi.h>
#endif

// 【新增】用于验证代码版本的标记，如果日志没看到这个，说明没重新编译
#define CODE_VERSION_TAG "v2.0_FixSavePath"

client::client(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::client),
    m_tcpSocket(new QTcpSocket(this)),
    m_udpSocket(new QUdpSocket(this)),
    m_heartbeatTimer(new QTimer(this)),
    m_fileServer(new QTcpServer(this)),
    m_myIp(""),
    m_myUdpPort(DEFAULT_STUDENT_START_PORT), 
    m_myTcpPort(0),
    m_myNickName(""),
    m_sharedDirPath(""),
    m_localSavePath("E:/fileReceive"),
    m_avatarPath(""), 
    m_state(State_Offline),
    m_currentTargetIp(""),
    m_currentTargetPort(0),
    m_currentTargetName(""),
    m_currentPath("/"),
    m_manualTeacherIp(""),
    m_manualTeacherTcpPort(0)
{
    // 初始化本机信息
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (const QHostAddress &addr : list) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            m_myIp = addr.toString();
            break;
        }
    }
    if (m_myIp.isEmpty()) m_myIp = "127.0.0.1";

    m_myNickName = "学生_" + m_myIp.split('.').last();
    m_myTcpPort = 20000 + QRandomGenerator::global()->bounded(10000);
    m_sharedDirPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/MyShare";
    QDir().mkpath(m_sharedDirPath);

    // 初始化默认下载目录
    QDir defaultSaveDir(m_localSavePath);
    if (!defaultSaveDir.exists()) {
        if (!defaultSaveDir.mkpath(".")) {
            qDebug() << "[Init] Warning: Failed to create default save directory:" << m_localSavePath;
        }
    }

    // 初始化自定义 UI (实现在 client_ui.cpp)
    initUi();

    // 【新增】初始化定时器：固定每 5 分钟发送一次截图 (定期巡查)
    m_screenshotTimer = new QTimer(this);
    m_screenshotTimer->setInterval(300000); // 5 分钟
    connect(m_screenshotTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "[Monitor] 5 分钟定时到达，发送定期截图...";
        captureAndSendScreenshot(true); // true 表示是定期监控
    });
    
    // 【新增】初始化应用检查定时器：每秒检查一次前台应用
    m_appCheckTimer = new QTimer(this);
    m_appCheckTimer->setInterval(1000); // 1 秒
    connect(m_appCheckTimer, &QTimer::timeout, this, &client::onCheckBlacklistTimeout);
    
    m_isMonitoringEnabled = false;
    m_isReportingViolated = false; // 初始未上报
    m_lastViolatedTime = 0;        // 【新增】初始化上次违规时间为 0
    
    // 【新增】初始化班级信息
    m_currentClassName = ""; 

    // 网络连接初始化
    connect(m_tcpSocket, &QTcpSocket::connected, this, &client::onConnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &client::onReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &client::onDisconnected);
    
    startNetworkInitialization();
}

client::~client()
{
    delete ui;
}

void client::startNetworkInitialization()
{
    m_state = State_Offline;
    m_statusLabel->setText("● 离线 (正在尝试连接...)");
    m_statusLabel->setStyleSheet("color: #f39c12; font-size: 12px;");

    quint16 targetPort = DEFAULT_STUDENT_START_PORT; 
    
    if (!m_udpSocket->bind(QHostAddress::Any, targetPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        QString errorMsg = m_udpSocket->errorString();
        qDebug() << "[Critical] UDP Bind failed on standard port" << targetPort << ":" << errorMsg;
        
        m_statusLabel->setText("● 离线 (端口 9998 被占用)");
        m_statusLabel->setStyleSheet("color: #e74c3c; font-size: 12px;");
        
        QString errorDetail = QString("无法绑定 UDP 端口 %1。\n错误：%2\n\n可能原因：\n1. 另一个学生端或教师端正在运行。\n2. 端口被其他程序占用。\n\n解决方案：\n- 请关闭其他占用端口的程序后重试。\n- 或者使用界面上的'手动连接'功能（通过 TCP 直连，不受此限制）。").arg(targetPort).arg(errorMsg);
        
        showNetworkErrorInUI(errorDetail);
        return; 
    }

    m_myUdpPort = targetPort;
    qDebug() << "[Info] ✅ UDP Successfully listening on standard port:" << m_myUdpPort;
    
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &client::onUdpReadyRead);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &client::sendHeartbeat);
    
    if (!m_fileServer->listen(QHostAddress::Any, m_myTcpPort)) {
        qDebug() << "[Error] P2P Server listen failed:" << m_fileServer->errorString();
    } else {
        qDebug() << "[Info] P2P File Server listening on port:" << m_myTcpPort;
        connect(m_fileServer, &QTcpServer::newConnection, this, &client::onNewPeerConnection);
    }

    startUdpDiscovery();
    m_state = State_Online;
    
    m_statusLabel->setText("● 在线 (UDP:" + QString::number(m_myUdpPort) + ", TCP:" + QString::number(m_myTcpPort) + ")");
    m_statusLabel->setStyleSheet("color: #2ecc71; font-size: 12px;");
    
    sendHeartbeat(); 

    // 【修复】网络初始化成功后，立即启动监控定时器，无需等待 TCP 连接
    // 即使教师端未上线，定时器也会运行，但截图发送函数会因无目标而直接返回
    if (!m_appCheckTimer->isActive()) {
        m_isMonitoringEnabled = true;
        m_appCheckTimer->start(1000); // 每秒检查一次前台应用
        qDebug() << "[Monitor] ✅ Real-time monitoring STARTED in startNetworkInitialization. Check interval: 1s.";
        qDebug() << "[Monitor] Timer active:" << m_appCheckTimer->isActive() << "Interval:" << m_appCheckTimer->interval();
    } else {
        qDebug() << "[Monitor] Warning: App check timer was already active.";
    }
}

void client::showNetworkErrorInUI(const QString &errorDetail)
{
    m_friendList->clear();
    
    QListWidgetItem *errItem = new QListWidgetItem();
    errItem->setText("❌ 网络初始化失败\n\n错误：" + errorDetail + "\n\n可能原因：\n1. 端口范围被其他程序大量占用。\n2. 权限严重不足。\n\n操作：\n- 请关闭其他占用网络的程序后重试。");
    errItem->setForeground(QBrush(Qt::red));
    errItem->setFont(QFont("Microsoft YaHei", 10));
    errItem->setSizeHint(QSize(0, 150));
    m_friendList->addItem(errItem);
}

void client::startUdpDiscovery()
{
    m_heartbeatTimer->start(3000);
}

void client::sendHeartbeat()
{
    if (m_state != State_Online) return;

    // 【修改】心跳包增加班级信息字段 (第 6 个参数)
    // 格式：HEARTBEAT|NickName|IsTeacher|IP|UdpPort|TcpPort|ClassName
    QString msg = QString("HEARTBEAT|%1|%2|%3|%4|%5|%6")
                  .arg(m_myNickName)
                  .arg("0") 
                  .arg(m_myIp)
                  .arg(m_myUdpPort)
                  .arg(m_myTcpPort)
                  .arg(m_currentClassName); // 发送当前班级
    
    QByteArray data = msg.toUtf8();
    qint64 bytesSent = m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, DEFAULT_TEACHER_UDP_PORT);
    
    if (bytesSent == -1) {
        qDebug() << "[UDP Error] Failed to send heartbeat to Broadcast:" << m_udpSocket->errorString();
    }
}

void client::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderIp;
        quint16 senderPort;

        qint64 size = m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);
        if (size == -1) {
            qDebug() << "[UDP Error] Read datagram failed:" << m_udpSocket->errorString();
            continue;
        }

        QString contentPreview = QString::fromUtf8(datagram);
        if (senderIp.toString() == m_myIp && contentPreview.startsWith("HEARTBEAT|" + m_myNickName)) {
            continue;
        }

        QString content = contentPreview;
        QStringList parts = content.split('|');

        if (parts.size() >= 2 && parts[0] == "USER_LIST") {
            // 【修改】解析新的 JSON 结构：{ "users": [...], "classes": [...] }
            QString jsonStr = parts[1];
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
            
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject rootObj = doc.object();
                
                // 1. 解析用户列表
                if (rootObj.contains("users") && rootObj["users"].isArray()) {
                    QSet<QString> currentVisibleIds; 
                    for (const QJsonValue &val : rootObj["users"].toArray()) {
                        QJsonObject obj = val.toObject();
                        UserInfo user;
                        user.id = obj["id"].toString();
                        user.nickName = obj["nickName"].toString();
                        user.isTeacher = obj["isTeacher"].toBool();
                        user.ip = obj["ip"].toString();
                        user.port = obj["udpPort"].toInt();
                        user.tcpPort = obj["tcpPort"].toInt();
                        user.lastHeartbeat = obj["lastHeartbeat"].toVariant().toLongLong();
                        
                        m_onlineUsers[user.id] = user;
                        currentVisibleIds.insert(user.id);
                        updateUserList(user);
                    }
                }

                // 【新增】2. 解析班级列表并更新 UI 下拉框
                if (rootObj.contains("classes") && rootObj["classes"].isArray()) {
                    QJsonArray classArray = rootObj["classes"].toArray();
                    if (m_classComboBox) {
                        // 保存当前选中的班级（如果有）
                        QString currentSelection = m_classComboBox->currentText();
                        
                        // 清空现有选项（保留占位符逻辑在 UI 层处理，这里全清重加）
                        m_classComboBox->clear();
                        m_classComboBox->addItem("请选择班级...", ""); 
                        
                        bool hasRealClasses = false;
                        for (const QJsonValue &val : classArray) {
                            QString className = val.toString();
                            if (!className.trimmed().isEmpty()) {
                                m_classComboBox->addItem(className);
                                hasRealClasses = true;
                            }
                        }
                        
                        // 尝试恢复之前的选择
                        if (!currentSelection.isEmpty() && currentSelection != "请选择班级...") {
                            int index = m_classComboBox->findText(currentSelection);
                            if (index != -1) {
                                m_classComboBox->setCurrentIndex(index);
                            }
                        }

                        if (hasRealClasses) {
                            qDebug() << "[Class] Updated class list from server, count:" << (classArray.size());
                        }
                    }
                }
            } else {
                // 【兼容旧版】如果解析对象失败，尝试按旧版数组解析 (仅用户列表)
                if (doc.isArray()) {
                     QSet<QString> currentVisibleIds; 
                     for (const QJsonValue &val : doc.array()) {
                        QJsonObject obj = val.toObject();
                        UserInfo user;
                        user.id = obj["id"].toString();
                        user.nickName = obj["nickName"].toString();
                        user.isTeacher = obj["isTeacher"].toBool();
                        user.ip = obj["ip"].toString();
                        user.port = obj["udpPort"].toInt();
                        user.tcpPort = obj["tcpPort"].toInt();
                        user.lastHeartbeat = obj["lastHeartbeat"].toVariant().toLongLong();
                        
                        m_onlineUsers[user.id] = user;
                        currentVisibleIds.insert(user.id);
                        updateUserList(user);
                    }
                }
            }
            return;
        }

        if (parts.size() >= 6 && parts[0] == "HEARTBEAT") {
            QString nick = parts[1];
            bool isTeacher = (parts[2] == "1");
            QString ip = parts[3];
            quint16 uPort = parts[4].toUShort();
            quint16 tPort = parts[5].toUShort();

            UserInfo user;
            user.ip = ip;
            user.port = uPort;
            user.tcpPort = tPort;
            user.id = QString("%1:%2").arg(ip).arg(uPort);
            user.nickName = nick;
            user.isTeacher = isTeacher;
            user.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();

            updateUserList(user);
        }
    }
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> offlineUsers;
    for (auto it = m_onlineUsers.begin(); it != m_onlineUsers.end(); ++it) {
        if (now - it->lastHeartbeat > 10000) {
            offlineUsers.append(it.key());
        }
    }
    for (const QString &id : offlineUsers) {
        removeUser(id);
    }
}

void client::updateUserList(const UserInfo &user)
{
    bool isNew = !m_onlineUsers.contains(user.id);
    m_onlineUsers[user.id] = user;

    if (isNew) {
        QListWidgetItem *item = new QListWidgetItem();
        QString iconStr = user.isTeacher ? "👨‍🏫" : "👨‍🎓";
        QString displayText = QString("%1 %2\n%3:%4").arg(iconStr, 2).arg(user.nickName).arg(user.ip).arg(user.port);
        
        item->setText(displayText);
        item->setData(Qt::UserRole, user.id);
        item->setSizeHint(QSize(0, 60));
        
        if (user.isTeacher) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setForeground(QColor("#2980b9"));
        }
        
        m_friendList->addItem(item);
        qDebug() << "[UI] Added user:" << user.nickName;
    } else {
        for (int i = 0; i < m_friendList->count(); ++i) {
            QListWidgetItem *item = m_friendList->item(i);
            if (item->data(Qt::UserRole).toString() == user.id) {
                QString iconStr = user.isTeacher ? "👨‍🏫" : "👨‍🎓";
                item->setText(QString("%1 %2\n%3:%4").arg(iconStr, 2).arg(user.nickName).arg(user.ip).arg(user.port));
                break;
            }
        }
    }
}

void client::removeUser(const QString &uniqueKey)
{
    if (!m_onlineUsers.contains(uniqueKey)) return;
    
    m_onlineUsers.remove(uniqueKey);
    
    for (int i = 0; i < m_friendList->count(); ++i) {
        QListWidgetItem *item = m_friendList->item(i);
        if (item && item->data(Qt::UserRole).toString() == uniqueKey) {
            delete m_friendList->takeItem(i);
            qDebug() << "[UI] Removed offline user:" << uniqueKey;
            break;
        }
    }
}

void client::updateUserListFromMap()
{
    m_friendList->clear();
    for (auto it = m_onlineUsers.begin(); it != m_onlineUsers.end(); ++it) {
        QListWidgetItem *item = new QListWidgetItem();
        QString iconStr = it->isTeacher ? "👨‍🏫" : "👨‍🎓";
        QString displayText = QString("%1 %2\n%3:%4").arg(iconStr, 2).arg(it->nickName).arg(it->ip).arg(it->port);
        
        item->setText(displayText);
        item->setData(Qt::UserRole, it->id);
        item->setSizeHint(QSize(0, 60));
        
        if (it->isTeacher) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setForeground(QColor("#2980b9"));
        }
        
        m_friendList->addItem(item);
    }
}

bool client::checkTeacherOnline()
{
    for (auto it = m_onlineUsers.begin(); it != m_onlineUsers.end(); ++it) {
        if (it->isTeacher) {
            return true;
        }
    }
    return false;
}

void client::onRetryConnectionClicked()
{
    qDebug() << "[Action] User triggered network retry...";
    if (m_udpSocket->isOpen()) {
        m_udpSocket->close();
    }
    if (m_fileServer->isListening()) {
        m_fileServer->close();
    }
    
    startNetworkInitialization();
    
    if (m_state == State_Online) {
        QMessageBox::information(this, "成功", "网络初始化成功！");
        m_friendList->clear();
        sendHeartbeat();
    } else {
        QMessageBox::warning(this, "失败", "重试失败，请检查是否有其他程序占用端口 9998 或以管理员身份运行。");
    }
}

void client::tryDirectConnect(const QString &teacherIp, quint16 teacherTcpPort)
{
    QTcpSocket *socket = new QTcpSocket(this);
    socket->connectToHost(teacherIp, teacherTcpPort);
    
    if (!socket->waitForConnected(3000)) {
        QString errStr = socket->errorString();
        if (socket) {
            QTreeWidgetItem *errItem = new QTreeWidgetItem(m_fileTree);
            errItem->setText(0, "❌ 直接连接失败:\n" + errStr + "\n请检查 IP 是否正确及防火墙设置。");
            errItem->setForeground(0, Qt::red);
            socket->deleteLater();
        }
        return;
    }
    
    UserInfo fakeTeacher;
    fakeTeacher.ip = teacherIp;
    fakeTeacher.tcpPort = teacherTcpPort;
    fakeTeacher.port = 0;
    fakeTeacher.nickName = "手动连接的教师";
    fakeTeacher.isTeacher = true;
    fakeTeacher.id = teacherIp + ":manual";
    fakeTeacher.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
    fakeTeacher.isManualConnect = true;
    
    m_currentTargetIp = teacherIp;
    m_currentTargetPort = teacherTcpPort;
    m_currentTargetName = "手动连接的教师";
    
    m_onlineUsers[fakeTeacher.id] = fakeTeacher;
    
    qDebug() << "[Manual] Direct connection successful to" << teacherIp << ":" << teacherTcpPort;
    
    requestFileList(teacherIp, teacherTcpPort, m_currentPath);
    
    socket->disconnectFromHost();
    socket->deleteLater();
}

// 占位实现
void client::checkFirewallStatus() { qDebug() << "[Check] Firewall status check triggered (not implemented yet)."; }
bool client::isPortOpen(quint16 port, QAbstractSocket::SocketType socketType) { Q_UNUSED(port); Q_UNUSED(socketType); return true; }
void client::showFirewallWarning() { QMessageBox::warning(this, "防火墙提示", "检测到可能存在的防火墙拦截问题。\n请确保 UDP 9998 端口和 TCP 动态端口已放行。"); }
void client::onConnected()
{ 
    qDebug() << "Connected"; 
    // 【删除】不再在这里启动监控，因为学生端没有持久 TCP 连接到教师端，此槽函数不会被调用
    qDebug() << "[Monitor] onConnected called (but monitoring should already be running from init).";
}

void client::onReadyRead() { qDebug() << "Received:" << m_tcpSocket->readAll(); }
void client::onDisconnected() { qDebug() << "Disconnected"; }
void client::showFriendListPage() {}
void client::showFileSharePage() {}

// 【修改】实现黑名单即时检查逻辑：增加本地冷却期和严格的状态跳变检测
void client::onCheckBlacklistTimeout() {
    if (!m_isMonitoringEnabled) return;

    QString appName = getCurrentForegroundApp();
    
    // 【调试】减少日志频率，仅每 30 秒打印一次当前应用，避免刷屏
    static int logCounter = 0;
    logCounter++;
    if (logCounter % 30 == 0) { 
        qDebug() << "[Monitor Tick] Current App:" << (appName.isEmpty() ? "(None)" : appName);
    }

    bool isViolated = false;
    QString matchedApp = ""; 

    // 黑名单匹配逻辑
    if (!appName.isEmpty()) {
        for (const QString &keyword : BLACKLIST_APPS) {
            QString lowerKeyword = keyword.toLower();
            if (appName.contains(lowerKeyword)) {
                isViolated = true;
                matchedApp = appName; 
                break;
            }
        }
    }

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    // 【核心修复】状态机逻辑 + 本地冷却期
    if (isViolated) {
        // 场景 A：检测到违规应用
        if (!m_isReportingViolated) {
            // 状态跳变：正常 -> 违规
            // 检查是否在冷却期内（距离上次恢复是否不足 10 秒）
            qint64 timeSinceRecover = (m_lastViolatedTime == 0) ? 99999 : (currentTime - m_lastViolatedTime);
            
            if (timeSinceRecover >= 10000) {
                // 冷却期已过，允许上报
                qDebug() << "[STATUS CHANGE] Normal -> Violated! App:" << matchedApp << "(Cooldown passed)";
                
                // 立即上报完整违规报告（带截图）
                captureAndSendScreenshot(false, matchedApp, false); 
                
                m_isReportingViolated = true;
                // 注意：此时不更新 m_lastViolatedTime，因为它记录的是恢复时间
            } else {
                // 冷却期内，静默忽略，不改变状态也不上报
                qDebug() << "[COOLDOWN] Normal -> Violated ignored (within 10s cooldown). Time elapsed:" << timeSinceRecover << "ms";
                // 保持 m_isReportingViolated = false，下次检测时会再次检查冷却期
            }
        } else {
            // 持续违规中，静默忽略，不上报（防止每秒都发）
        }
    } else {
        // 场景 B：未检测到违规应用
        if (m_isReportingViolated) {
            // 状态跳变：违规 -> 正常
            qDebug() << "[STATUS CHANGE] Violated -> Normal. Reporting recovery.";
            
            // 记录恢复时间戳（用于后续冷却期计算）
            m_lastViolatedTime = currentTime;
            
            // 上报恢复事件（不带截图）
            captureAndSendScreenshot(false, "Status_Recovered", false);
            
            m_isReportingViolated = false;
        }
        // 持续正常中，静默忽略
    }
}

// 【修改】辅助函数：获取当前前台应用名称（Windows 平台实现）
// 策略优化：
// 1. 优先获取窗口标题 (Title) ，因为浏览器（Edge/Chrome）的进程名都是 msedge/chrome，无法区分具体网站。
// 2. 如果标题匹配黑名单，直接返回标题。
// 3. 如果标题未匹配，再尝试获取进程名（用于检测独立 exe 应用，如微信、游戏等）。
QString client::getCurrentForegroundApp() {
#ifdef Q_OS_WIN
    HWND hWnd = GetForegroundWindow();
    if (hWnd == NULL) {
        // qDebug() << "[Monitor Debug] GetForegroundWindow() returned NULL. No active window.";
        return "";
    }

    QString resultName = "";
    
    // --- 第一步：获取窗口标题 (Title) ---
    // 浏览器标签页、UWP 应用等通常能在标题中找到特征字（如 "哔哩哔哩 (゜-゜) つロ 干杯~-bilibili"）
    wchar_t title[512]; // 增大缓冲区以防长标题
    int len = GetWindowTextW(hWnd, title, 512);
    QString windowTitle = "";
    if (len > 0) {
        windowTitle = QString::fromWCharArray(title).toLower();
        // qDebug() << "[Monitor Debug] Window Title:" << windowTitle;
        
        // 【关键修改】先用标题匹配黑名单
        for (const QString &keyword : BLACKLIST_APPS) {
            QString lowerKeyword = keyword.toLower();
            if (windowTitle.contains(lowerKeyword)) {
                // qDebug() << "[Monitor Match] Found violation in Window Title:" << windowTitle << "(Keyword:" << keyword << ")";
                return windowTitle; // 直接返回标题，包含完整信息
            }
        }
    }

    // --- 第二步：如果标题没命中，再获取进程名 (Process Name) ---
    // 适用于独立应用程序（如 WeChat.exe, League of Legends.exe）
    DWORD processId = 0;
    GetWindowThreadProcessId(hWnd, &processId);
    
    if (processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH)) {
                QString fullPath = QString::fromWCharArray(exePath);
                QFileInfo fi(fullPath);
                QString procName = fi.fileName().toLower();
                
                // 特殊处理：如果是浏览器进程，且标题没命中，说明是在看其他网页，不算违规（除非标题里有违规词，上面已判断）
                // 这里主要为了捕获非浏览器的独立应用
                if (procName != "msedge.exe" && procName != "chrome.exe" && procName != "firefox.exe" && procName != "iexplore.exe") {
                    for (const QString &keyword : BLACKLIST_APPS) {
                        QString lowerKeyword = keyword.toLower();
                        if (procName.contains(lowerKeyword)) {
                            // qDebug() << "[Monitor Match] Found violation in Process Name:" << procName << "(Keyword:" << keyword << ")";
                            CloseHandle(hProcess);
                            return procName;
                        }
                    }
                } else {
                    // qDebug() << "[Monitor Debug] Detected Browser (" << procName << "), relying on Window Title only.";
                }
            } else {
                // DWORD err = GetLastError();
                // qDebug() << "[Monitor Debug] GetModuleFileNameExW failed for PID:" << processId;
            }
            CloseHandle(hProcess);
        } else {
            // DWORD err = GetLastError();
            // qDebug() << "[Monitor Debug] OpenProcess failed for PID:" << processId;
        }
    }

    // 如果都没命中，返回空
    // qDebug() << "[Monitor Debug] No violation detected. Title:" << windowTitle << "Proc:" << (resultName.isEmpty() ? "N/A" : resultName);
    return ""; 

#else
    // qDebug() << "[Monitor Debug] Not running on Windows, getCurrentForegroundApp returns empty.";
    return "";
#endif
}

// 【修改】辅助函数：截取屏幕并发送
void client::captureAndSendScreenshot(bool isPeriodic, const QString &violatedAppName, bool countOnly)
{
    Q_UNUSED(countOnly); 
    
    qDebug() << "[SCREENSHOT_SEND] Type:" << violatedAppName;
    
    // 1. 检查是否有目标教师
    if (m_currentTargetIp.isEmpty() || m_currentTargetPort == 0) {
        bool found = false;
        for (auto it = m_onlineUsers.begin(); it != m_onlineUsers.end(); ++it) {
            if (it->isTeacher) {
                m_currentTargetIp = it->ip;
                m_currentTargetPort = it->tcpPort; 
                m_currentTargetName = it->nickName;
                found = true;
                break;
            }
        }
        if (!found) {
            // 静默返回，避免频繁报错
            return;
        }
    }

    // 2. 构建元数据 JSON
    QJsonObject metaObj;
    QString typeTag = "";

    if (violatedAppName == "Status_Recovered") {
        typeTag = "STATUS_RECOVERY";
    } else if (violatedAppName == "Live_Request_Response") {
        typeTag = "LIVE_RESPONSE"; 
    } else {
        typeTag = isPeriodic ? "PERIODIC_MONITOR" : "VIOLATION_REPORT";
    }
    
    metaObj["type"] = typeTag;
    metaObj["appName"] = violatedAppName;
    metaObj["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metaObj["studentId"] = QString("%1:%2").arg(m_myIp).arg(m_myUdpPort); 
    
    QByteArray jsonData = QJsonDocument(metaObj).toJson(QJsonDocument::Compact);
    QByteArray imageData;

    // 【优化】如果是恢复事件，不需要截图，直接发送空图片数据
    if (typeTag != "STATUS_RECOVERY") {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QPixmap screenshot = screen->grabWindow(0);
            if (!screenshot.isNull()) {
                QBuffer buffer(&imageData);
                buffer.open(QIODevice::WriteOnly);
                screenshot.save(&buffer, "JPG", 75);
                buffer.close();
            }
        }
    }

    // 3. 发送数据到教师端
    QTcpSocket *socket = new QTcpSocket(this);
    
    // 连接超时处理
    QTimer *connectTimer = new QTimer(socket);
    connectTimer->setSingleShot(true);
    connect(connectTimer, &QTimer::timeout, [socket]() {
        if (socket->state() == QAbstractSocket::ConnectingState || socket->state() == QAbstractSocket::ConnectedState) {
            socket->abort();
            socket->deleteLater();
        }
    });
    connectTimer->start(3000); 

    connect(socket, &QTcpSocket::connected, this, [socket, jsonData, imageData, connectTimer]() {
        connectTimer->stop();
        
        // 发送头：MONITOR_START|jsonSize|imageSize
        QString headerStr = QString("MONITOR_START|%1|%2").arg(jsonData.size()).arg(imageData.size());
        QByteArray headerData = headerStr.toUtf8();
        
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_15);
        out << (quint32)headerData.size();
        out.writeRawData(headerData.constData(), headerData.size());
        socket->write(block);
        
        // 发送 JSON
        QByteArray jsonBlock;
        QDataStream jsonOut(&jsonBlock, QIODevice::WriteOnly);
        jsonOut.setVersion(QDataStream::Qt_5_15);
        jsonOut << (quint32)jsonData.size();
        jsonOut.writeRawData(jsonData.constData(), jsonData.size());
        socket->write(jsonBlock);
        
        // 发送图片 (可能为空)
        QByteArray imgBlock;
        QDataStream imgOut(&imgBlock, QIODevice::WriteOnly);
        imgOut.setVersion(QDataStream::Qt_5_15);
        imgOut << (quint32)imageData.size();
        if (!imageData.isEmpty()) {
            imgOut.writeRawData(imageData.constData(), imageData.size());
        }
        socket->write(imgBlock);
        
        socket->disconnectFromHost();
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), [socket](QAbstractSocket::SocketError){
        socket->deleteLater();
    });

    socket->connectToHost(m_currentTargetIp, m_currentTargetPort);
}

// 【新增】处理来自老师的命令 (在 handlePeerCommand 或专门的 TCP 读取逻辑中)
// 由于学生端作为 P2P 服务器监听 m_myTcpPort，需要在 handlePeerCommand 中增加解析
// 注意：原代码中 handlePeerCommand 在 client_p2p.cpp 中，我们需要修改它
// 这里为了方便，直接在 client_core.cpp 中补充逻辑，或者修改 client_p2p.cpp
// 鉴于文件结构，最好修改 client_p2p.cpp 来接收这个命令

// 【修复】确保加入班级逻辑完整且 UI 反馈明确
void client::onJoinClassClicked() {
    QString className = m_classComboBox->currentText().trimmed();
    // 【修改】增加对 placeholder 的检查，防止用户选择"请选择班级..."这种无效项
    if (className.isEmpty() || className == "请选择班级...") {
        QMessageBox::warning(this, "提示", "请先从列表中选择一个有效的班级！");
        return;
    }

    // 1. 更新本地成员变量
    m_currentClassName = className;
    
    // 2. 更新 UI 显示
    QLabel *label = this->findChild<QLabel*>("currentClassLabel");
    if (label) {
        label->setText("当前班级：<b>" + className + "</b>");
        label->setStyleSheet("color: #27ae60; font-size: 14px; font-weight: bold; margin-top: 5px;");
    } else {
        qDebug() << "[Warning] currentClassLabel not found in UI hierarchy.";
    }

    // 3. 立即发送心跳包，将班级信息广播给教师端
    // 心跳包格式：HEARTBEAT|NickName|IsTeacher|IP|UdpPort|TcpPort|ClassName
    sendHeartbeat();

    // 4. 给用户明确的成功反馈
    QMessageBox::information(this, "加入成功", 
        "已成功加入班级：**" + className + "**\n\n"
        "教师端刷新列表后即可在'班级'列看到该信息。\n"
        "心跳包已即时发送，班级信息已同步至服务器。");
    
    qDebug() << "[Class] Student joined class:" << className << ", heartbeat sent immediately.";
}
