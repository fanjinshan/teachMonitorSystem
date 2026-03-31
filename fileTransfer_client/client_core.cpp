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
#include <QThread>
#include <QStringList>
#include <QUuid>
#include <QSettings>
#include <QFileDialog> // 【修复】添加缺失的头文件

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#define CODE_VERSION_TAG "v2.1_FixUniqueId"

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
    m_studentId(""),
    m_state(State_Offline),
    m_currentTargetIp(""),
    m_currentTargetPort(0),
    m_currentTargetName(""),
    m_currentPath("/"),
    m_manualTeacherIp(""),
    m_manualTeacherTcpPort(0),
    m_teacherIp("") // 【新增】初始化教师端 IP 为空
{
    // 初始化本机信息
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    bool ipFound = false;
    
    // 【修复】优化 IP 选择策略：优先选择常见私有局域网网段，避免选中 VPN 或虚拟网卡 IP
    QString preferredIp;  // 首选私有网段 IP (192.168, 10, 172.16-31)
    QString fallbackIp;   // 备用其他 IPv4 地址

    for (const QHostAddress &addr : list) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            QString ipStr = addr.toString();
            // 跳过 127 开头
            if (ipStr.startsWith("127.")) continue;

            // 判断是否为常见私有网段
            bool isPrivate = ipStr.startsWith("192.168.") || 
                             ipStr.startsWith("10.") || 
                             (ipStr.startsWith("172.") && [&]() {
                                 // 检查 172.16.0.0 - 172.31.255.255
                                 QStringList parts = ipStr.split('.');
                                 if (parts.size() >= 2) {
                                     int second = parts[1].toInt();
                                     return (second >= 16 && second <= 31);
                                 }
                                 return false;
                             }());

            if (isPrivate) {
                preferredIp = ipStr;
                break; // 找到首选 IP 立即停止
            }
            
            // 记录一个备用 IP
            if (fallbackIp.isEmpty()) {
                fallbackIp = ipStr;
            }
        }
    }

    // 应用选择结果
    if (!preferredIp.isEmpty()) {
        m_myIp = preferredIp;
        ipFound = true;
        qDebug() << "[Init] ✅ Selected preferred private IP:" << m_myIp;
    } else if (!fallbackIp.isEmpty()) {
        m_myIp = fallbackIp;
        ipFound = true;
        qDebug() << "[Init] ⚠️ No private IP found, using fallback:" << m_myIp;
    }

    if (!ipFound || m_myIp.isEmpty()) {
        m_myIp = "127.0.0.1";
        qWarning() << "[Init] ⚠️ Failed to obtain valid non-loopback IP, fallback to localhost.";
    } else {
        qDebug() << "[Init] ✅ Local IP obtained:" << m_myIp;
    }

    // 初始化自定义 UI
    initUi();

    // 【新增】加载固定学生 ID 和本地配置
    loadUserSettings();

    // 【修复】问题 1：加载配置后立即更新 UI 显示昵称和头像
    if (!m_myNickName.isEmpty()) {
        m_nickNameLabel->setText(m_myNickName);
    }
    
    if (!m_avatarPath.isEmpty() && QFile::exists(m_avatarPath)) {
        QPixmap originalPix(m_avatarPath);
        if (!originalPix.isNull()) {
            QPixmap scaledPix = originalPix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_avatarLabel->setPixmap(scaledPix);
            m_avatarLabel->setText("");
        }
    }

    // 如果昵称仍为空（首次运行），使用 IP 生成默认昵称
    if (m_myNickName.isEmpty()) {
        m_myNickName = "学生_" + m_myIp.split('.').last();
        m_nicknameEdit->setText(m_myNickName);
    } else {
        m_nicknameEdit->setText(m_myNickName);
    }

    m_myTcpPort = 20000 + QRandomGenerator::global()->bounded(10000);
    qDebug() << "[Init] Local TCP Port generated:" << m_myTcpPort;
    
    m_sharedDirPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/MyShare";
    QDir().mkpath(m_sharedDirPath);

    // 初始化默认下载目录
    QDir defaultSaveDir(m_localSavePath);
    if (!defaultSaveDir.exists()) {
        if (!defaultSaveDir.mkpath(".")) {
            qDebug() << "[Init] Warning: Failed to create default save directory:" << m_localSavePath;
        }
    }

    // 【新增】如果头像路径已保存且文件存在，则加载显示
    if (!m_avatarPath.isEmpty() && QFile::exists(m_avatarPath)) {
        QPixmap originalPix(m_avatarPath);
        if (!originalPix.isNull()) {
            QPixmap scaledPix = originalPix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_avatarLabel->setPixmap(scaledPix);
            m_avatarLabel->setText("");
        }
    }

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
    m_isReportingViolated = false;
    m_lastViolatedTime = 0;
    
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

// 【新增】加载用户设置（固定 ID、昵称、班级、头像、保存路径）
void client::loadUserSettings()
{
    QSettings settings("YourCompany", "SmartClassroomClient");
    
    // 1. 加载或生成固定学生 ID
    m_studentId = settings.value("studentId").toString();
    if (m_studentId.isEmpty()) {
        m_studentId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("studentId", m_studentId);
        qDebug() << "[Student] Generated new fixed ID:" << m_studentId;
    } else {
        qDebug() << "[Student] Loaded existing fixed ID:" << m_studentId;
    }

    // 2. 加载昵称
    QString savedNick = settings.value("nickname").toString();
    if (!savedNick.isEmpty()) {
        m_myNickName = savedNick;
    }

    // 3. 加载班级
    QString savedClass = settings.value("class").toString();
    if (!savedClass.isEmpty()) {
        m_currentClassName = savedClass;
        qDebug() << "[Student] Loaded existing class:" << m_currentClassName;
    }

    // 4. 加载头像路径
    QString savedAvatar = settings.value("avatar").toString();
    if (!savedAvatar.isEmpty() && QFile::exists(savedAvatar)) {
        m_avatarPath = savedAvatar;
    }

    // 5. 加载保存路径
    QString savedSavePath = settings.value("savePath").toString();
    if (!savedSavePath.isEmpty()) {
        m_localSavePath = savedSavePath;
    }

    // 【新增】更新 UI 显示当前班级状态
    QLabel *label = this->findChild<QLabel*>("currentClassLabel");
    if (label) {
        if (!m_currentClassName.isEmpty()) {
            label->setText("当前班级：<b>" + m_currentClassName + "</b>");
            label->setStyleSheet("color: #27ae60; font-size: 14px; font-weight: bold; margin-top: 5px;");
            qDebug() << "[UI] Class label updated to:" << m_currentClassName;
        } else {
            label->setText("当前班级：未加入");
            label->setStyleSheet("color: #e67e22; font-size: 14px; font-weight: bold; margin-top: 5px;");
        }
    }
}

// 【新增】保存用户设置
void client::saveUserSettings()
{
    QSettings settings("YourCompany", "SmartClassroomClient");
    settings.setValue("studentId", m_studentId);
    settings.setValue("nickname", m_myNickName);
    settings.setValue("class", m_currentClassName);
    if (!m_avatarPath.isEmpty() && QFile::exists(m_avatarPath)) {
        settings.setValue("avatar", m_avatarPath);
    }
    settings.setValue("savePath", m_localSavePath);
    settings.sync(); // 强制写入磁盘
    qDebug() << "[Settings] User settings saved.";
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

    if (!m_appCheckTimer->isActive()) {
        m_isMonitoringEnabled = true;
        m_appCheckTimer->start(1000);
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

    // 【修改】心跳包增加固定学生 ID 字段 (第 8 个参数)
    // 格式：HEARTBEAT|NickName|IsTeacher|IP|UdpPort|TcpPort|ClassName|StudentId
    QString msg = QString("HEARTBEAT|%1|%2|%3|%4|%5|%6|%7")
                  .arg(m_myNickName)
                  .arg("0") 
                  .arg(m_myIp)
                  .arg(m_myUdpPort)
                  .arg(m_myTcpPort)
                  .arg(m_currentClassName)
                  .arg(m_studentId); // 发送固定 ID
    
    QByteArray data = msg.toUtf8();
    QHostAddress targetAddr;
    
    // 【核心修改】如果已知教师端 IP，优先使用单播发送心跳，绕过广播可能被防火墙拦截的问题
    if (!m_teacherIp.isEmpty()) {
        targetAddr = QHostAddress(m_teacherIp);
        qDebug() << "[UDP] Sending UNICAST heartbeat to teacher:" << m_teacherIp;
    } else {
        targetAddr = QHostAddress::Broadcast;
        qDebug() << "[UDP] Sending BROADCAST heartbeat (teacher IP unknown)";
    }
    
    qint64 bytesSent = m_udpSocket->writeDatagram(data, targetAddr, DEFAULT_TEACHER_UDP_PORT);
    
    if (bytesSent == -1) {
        qDebug() << "[UDP Error] Failed to send heartbeat to" << targetAddr.toString() << ":" << m_udpSocket->errorString();
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
                        
                        // 【新增】如果是教师端，记录其 IP 用于单播心跳
                        if (user.isTeacher && m_teacherIp.isEmpty()) {
                            m_teacherIp = user.ip;
                            qDebug() << "[UDP Discovery] Teacher found via USER_LIST, recording IP:" << m_teacherIp;
                        }
                    }
                }

                // 2. 解析班级列表并更新 UI 下拉框
                if (rootObj.contains("classes") && rootObj["classes"].isArray()) {
                    QJsonArray classArray = rootObj["classes"].toArray();
                    
                    qDebug() << "[Class] Received class list from server, count:" << classArray.size();
                    
                    if (m_classComboBox) {
                        QString currentSelection = m_classComboBox->currentText();
                        
                        m_classComboBox->clear();
                        m_classComboBox->addItem("请选择班级...", ""); 
                        
                        bool hasRealClasses = false;
                        bool currentClassStillExists = false; 
                        
                        for (const QJsonValue &val : classArray) {
                            QString className = val.toString();
                            if (!className.trimmed().isEmpty()) {
                                m_classComboBox->addItem(className);
                                hasRealClasses = true;
                                if (className == m_currentClassName) {
                                    currentClassStillExists = true;
                                }
                            }
                        }
                        
                        if (!m_currentClassName.isEmpty() && !currentClassStillExists) {
                            qDebug() << "[Class] Current class '" << m_currentClassName << "' was deleted by teacher. Resetting...";
                            m_currentClassName = ""; 
                            
                            QLabel *label = this->findChild<QLabel*>("currentClassLabel");
                            if (label) {
                                label->setText("当前班级：未加入 (原班级已被删除)");
                                label->setStyleSheet("color: #e74c3c; font-size: 14px; font-weight: bold; margin-top: 5px;");
                            }
                            
                            // 可选：弹窗提示用户
                            // QMessageBox::information(this, "班级变更", "您所在的班级已被教师删除，您现已恢复为未分班状态。");
                        } else if (!currentSelection.isEmpty() && currentSelection != "请选择班级..." && currentClassStillExists) {
                            int index = m_classComboBox->findText(currentSelection);
                            if (index != -1) {
                                m_classComboBox->setCurrentIndex(index);
                            }
                        }

                        if (hasRealClasses) {
                            m_classComboBox->repaint(); 
                            qDebug() << "[Class] UI Updated successfully.";
                        }
                    }
                } else {
                    qDebug() << "[Class] JSON missing 'classes' field or not an array.";
                }
            } else {
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
                        
                        // 【新增】兼容旧协议格式的用户列表解析
                        if (user.isTeacher && m_teacherIp.isEmpty()) {
                            m_teacherIp = user.ip;
                            qDebug() << "[UDP Discovery] Teacher found via legacy USER_LIST, recording IP:" << m_teacherIp;
                        }
                    }
                } else {
                    qDebug() << "[UDP Error] Failed to parse USER_LIST JSON:" << error.errorString();
                    qDebug() << "[UDP Error] Raw data preview:" << jsonStr.left(100);
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

            if (isTeacher) {
                if (m_currentTargetIp.isEmpty() || m_currentTargetPort != tPort || m_currentTargetIp != ip) {
                    m_currentTargetIp = ip;
                    m_currentTargetPort = tPort;
                    m_currentTargetName = nick;
                    
                    // 【新增】收到教师心跳时，记录其 IP
                    m_teacherIp = ip;
                    qDebug() << "[Heartbeat] Updated teacher target and recorded IP for unicast:" << ip << ":" << tPort;
                }
            }
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
    qDebug() << "[Monitor] onConnected called (but monitoring should already be running from init).";
}

void client::onReadyRead() { qDebug() << "Received:" << m_tcpSocket->readAll(); }
void client::onDisconnected() { qDebug() << "Disconnected"; }
void client::showFriendListPage() {}
void client::showFileSharePage() {}

void client::onSaveNicknameClicked()
{
    QString newNick = m_nicknameEdit->text().trimmed();
    if (newNick.isEmpty()) {
        QMessageBox::warning(this, "提示", "昵称不能为空");
        return;
    }

    m_myNickName = newNick;
    m_nickNameLabel->setText(m_myNickName);
    this->setWindowTitle("智慧教室客户端 - " + m_myNickName);
    updateUserListFromMap();
    sendHeartbeat();
    
    // 【新增】保存昵称到本地配置
    saveUserSettings();
    
    QMessageBox::information(this, "成功", "昵称已修改为：" + newNick + "\n将同步至教师端。");
}

void client::onChangeAvatarClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择头像图片", "", 
        "Image Files (*.png *.jpg *.jpeg *.bmp *.gif)");
    
    if (filePath.isEmpty()) return;

    m_avatarPath = filePath;
    QPixmap originalPix(filePath);
    if (originalPix.isNull()) {
        QMessageBox::warning(this, "错误", "无法加载该图片文件");
        return;
    }

    QPixmap scaledPix = originalPix.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_avatarLabel->setPixmap(scaledPix);
    m_avatarLabel->setText("");
    
    // 【新增】保存头像路径到本地配置
    saveUserSettings();
    
    QMessageBox::information(this, "成功", "头像已更换");
}

void client::onCheckBlacklistTimeout() {
    if (!m_isMonitoringEnabled) return;

    QString appName = getCurrentForegroundApp();
    
    static int logCounter = 0;
    logCounter++;
    if (appName.isEmpty()) {
        if (logCounter % 30 == 0) { 
            qDebug() << "[Monitor Tick] Current App: (None/Empty)";
        }
    } else {
        qDebug() << "[Monitor Tick] Current App:" << appName;
    }

    static const QStringList WHITELIST_APPS = {
        "filetransfer_client", 
        "serverwindow",
        "explorer",            
        "taskmgr",             
        "applicationframehost",
        "searchapp",           
        "startmenuexperience", 
        "vnc", "realvnc", "tightvnc", "ultravnc",
        "teamviewer",
        "anydesk",
        "rustdesk",
        "todesk",
        "sunlogin", "向日葵",
        "zoom",
        "texmeeting", "腾讯会议",
        "dingtalk", "钉钉",
        "wemeetapp", "welink",
        "deepseek", "深度求索",
        "豆包", "doubao",
        "通义千问", "tongyi", "qwen",
        "kimi", "月之暗面",
        "智谱清言", "zhipu", "glm",
        "文心一言", "ernie",
        "copilot", "github copilot",
        "cursor",                
        "poe",
        "character.ai",
        "devenv",              
        "qtcreator",           
        "vscode", "visual studio code",
        "sublime_text", "sublime merge",
        "notepad++", "notepadplusplus",
        "jetbrains",           
        "intellij", "idea",
        "pycharm",
        "clion",
        "webstorm",
        "phpstorm",
        "goland",
        "rider",
        "datagrip",
        "android studio", "studio64",
        "eclipse",
        "visualgdb",
        "postman",
        "insomnia",
        "navicat",
        "dbeaver",
        "sqlworkbench",
        "xshell", "xftp",
        "putty", "winscp",
        "wireshark",
        "fiddler", "charles",
        "git", "gitbash", "sourcetree", "tortoisegit", "tortoisehg",
        "docker", "kubernetes", "kubectl",
        "terminal", "wt", "windowsterminal",
        "powershell", "cmd",
        "notion",
        "obsidian",
        "typora",
        "onenote",
        "evernote",
        "marktext",
        "logseq",
        "cherry studio",
        "youdao", "有道词典",
        "bing dict", "必应词典",
        "金山词霸", "iciba",
        "deepL", "deepl",
        "bandizip",
        "7-zip", "7z",
        "winrar",
        "everything",
        "geek uninstaller",
        "ruanmei", "软媒",
        "dism++",
    };

    if (!appName.isEmpty()) {
        QString lowerApp = appName.toLower();
        for (const QString &white : WHITELIST_APPS) {
            if (lowerApp.contains(white)) {
                return; 
            }
        }
    }

    bool isViolated = false;
    QString matchedApp = ""; 

    if (!appName.isEmpty()) {
        for (const QString &keyword : BLACKLIST_APPS) {
            QString lowerKeyword = keyword.toLower();
            if (appName.contains(lowerKeyword)) {
                isViolated = true;
                matchedApp = appName; 
                qDebug() << "[VIOLATION DETECTED] App:" << appName << "matched keyword:" << keyword;
                break;
            }
        }
    }

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if (isViolated) {
        if (!m_isReportingViolated) {
            qint64 timeSinceRecover = (m_lastViolatedTime == 0) ? 99999 : (currentTime - m_lastViolatedTime);
            
            if (timeSinceRecover >= 10000) {
                qDebug() << "[STATUS CHANGE] Normal -> Violated! App:" << matchedApp << "(Cooldown passed)";
                
                captureAndSendScreenshot(false, matchedApp, false); 
                
                m_isReportingViolated = true;
            } else {
                qDebug() << "[COOLDOWN] Normal -> Violated ignored (within 10s cooldown). Time elapsed:" << timeSinceRecover << "ms";
            }
        } else {
        }
    } else {
        if (m_isReportingViolated) {
            qDebug() << "[STATUS CHANGE] Violated -> Normal. Reporting recovery immediately.";
            
            m_lastViolatedTime = currentTime;
            
            captureAndSendScreenshot(false, "Status_Recovered", false);
            
            m_isReportingViolated = false;
            
            qDebug() << "[RECOVERY SENT] Recovery signal sent to teacher. Flag reset.";
        }
    }
}

QString client::getCurrentForegroundApp() {
#ifdef Q_OS_WIN
    HWND hWnd = GetForegroundWindow();
    if (hWnd == NULL) {
        return "";
    }

    QString resultName = "";
    
    wchar_t title[512]; 
    int len = GetWindowTextW(hWnd, title, 512);
    QString windowTitle = "";
    if (len > 0) {
        windowTitle = QString::fromWCharArray(title).toLower();
        
        qDebug() << "[Monitor Debug] Window Title:" << windowTitle;
        
        for (const QString &keyword : BLACKLIST_APPS) {
            QString lowerKeyword = keyword.toLower();
            if (windowTitle.contains(lowerKeyword)) {
                qDebug() << "[Monitor Match] Found violation in Window Title:" << windowTitle << "(Keyword:" << keyword << ")";
                return windowTitle; 
            }
        }
        qDebug() << "[Monitor Debug] Window Title did not match any blacklist keyword.";
    } else {
        qDebug() << "[Monitor Debug] GetWindowTextW returned empty or failed.";
    }

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
                
                qDebug() << "[Monitor Debug] Process Name:" << procName;

                QStringList browsers = {"chrome.exe", "msedge.exe", "firefox.exe", "iexplore.exe", "browser.exe", "360se.exe", "qqbrowser.exe"};
                bool isBrowser = false;
                for (const QString &b : browsers) {
                    if (procName == b) {
                        isBrowser = true;
                        break;
                    }
                }

                if (isBrowser) {
                    qDebug() << "[Monitor Debug] Is Browser and Title not matched. Skipping process name check (Safe).";
                    CloseHandle(hProcess);
                    return ""; 
                }

                for (const QString &keyword : BLACKLIST_APPS) {
                    QString lowerKeyword = keyword.toLower();
                    QString procNameNoExt = fi.baseName().toLower();
                    
                    if (procName.contains(lowerKeyword) || procNameNoExt.contains(lowerKeyword)) {
                        qDebug() << "[Monitor Match] Found violation in Process Name:" << procName << "(Keyword:" << keyword << ")";
                        CloseHandle(hProcess);
                        return procName;
                    }
                }
                qDebug() << "[Monitor Debug] Process Name did not match any blacklist keyword.";
            } else {
                DWORD err = GetLastError();
                qDebug() << "[Monitor Debug] GetModuleFileNameExW failed for PID:" << processId << "Error:" << err;
            }
            CloseHandle(hProcess);
        } else {
            DWORD err = GetLastError();
            qDebug() << "[Monitor Debug] OpenProcess failed for PID:" << processId << "Error:" << err;
        }
    }

    return ""; 

#else
    return "";
#endif
}

void client::captureAndSendScreenshot(bool isPeriodic, const QString &violatedAppName, bool countOnly)
{
    Q_UNUSED(countOnly); 
    
    qDebug() << "[SCREENSHOT_SEND] Type:" << violatedAppName;
    
    if (m_currentTargetIp.isEmpty() || m_currentTargetPort == 0) {
        bool found = false;
        qDebug() << "[SCREENSHOT_SEND] Current target empty, searching online users...";
        for (auto it = m_onlineUsers.begin(); it != m_onlineUsers.end(); ++it) {
            if (it->isTeacher) {
                m_currentTargetIp = it->ip;
                m_currentTargetPort = it->tcpPort; 
                m_currentTargetName = it->nickName;
                found = true;
                
                // 【新增】找到教师时记录 IP
                m_teacherIp = it->ip;
                qDebug() << "[SCREENSHOT_SEND] Found teacher:" << it->nickName << "at" << it->ip << ":" << it->tcpPort << ", recorded for unicast.";
                break;
            }
        }
        if (!found) {
            qDebug() << "[SCREENSHOT_SEND] No teacher found in online users. Aborting.";
            return;
        }
    }

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
    
    // 【关键修改】使用固定学生 ID 代替 IP:端口
    metaObj["studentId"] = m_studentId; 
    
    // 【新增】在 TCP 数据包中携带昵称、班级和 TCP 端口，作为 UDP 心跳丢失时的备用信息源
    metaObj["nickName"] = m_myNickName;
    metaObj["className"] = m_currentClassName;
    metaObj["tcpPort"] = m_myTcpPort;   // 【修复】新增 tcpPort 字段，供教师端补录
    
    QByteArray jsonData = QJsonDocument(metaObj).toJson(QJsonDocument::Compact);
    QByteArray imageData;

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
        
        QString headerStr = QString("MONITOR_START|%1|%2").arg(jsonData.size()).arg(imageData.size());
        QByteArray headerData = headerStr.toUtf8();
        
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_15);
        out << (quint32)headerData.size();
        out.writeRawData(headerData.constData(), headerData.size());
        socket->write(block);
        
        QByteArray jsonBlock;
        QDataStream jsonOut(&jsonBlock, QIODevice::WriteOnly);
        jsonOut.setVersion(QDataStream::Qt_5_15);
        jsonOut << (quint32)jsonData.size();
        jsonOut.writeRawData(jsonData.constData(), jsonData.size());
        socket->write(jsonBlock);
        
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

void client::onFriendListItemDoubleClicked(QListWidgetItem *item)
{
    QString userId = item->data(Qt::UserRole).toString();
    UserInfo user = m_onlineUsers.value(userId);
    
    if (user.isTeacher) {
        m_currentTargetIp = user.ip;
        m_currentTargetPort = user.tcpPort;
        m_currentTargetName = user.nickName;
        m_manualTeacherIp = ""; 
        
        // 【新增】记录教师端 IP，用于后续 UDP 单播心跳
        m_teacherIp = user.ip;
        qDebug() << "[Discovery] Teacher IP recorded for unicast heartbeat:" << m_teacherIp;

        m_navList->setCurrentRow(1); 
        onRefreshClicked(); 
    } else {
        QMessageBox::information(this, "提示", "只能选择教师端进行连接。");
    }
}

void client::onRefreshClicked()
{
    qDebug() << "[Action] Refresh clicked. Checking teacher status...";
    
    // 【优化】如果是手动连接模式，直接连接，不发送心跳等待
    if (!m_manualTeacherIp.isEmpty()) {
        qDebug() << "[Manual Mode] Skipping UDP discovery, connecting directly to:" << m_manualTeacherIp;
        m_fileTree->clear();
        QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
        loadingItem->setText(0, "正在连接指定教师端 (" + m_manualTeacherIp + ")...");
        
        // 【新增】手动连接时也记录教师 IP
        m_teacherIp = m_manualTeacherIp;
        
        tryDirectConnect(m_manualTeacherIp, m_manualTeacherTcpPort);
        return;
    }

    // 【优化】先发送心跳，然后仅在没有目标 IP 时才短暂等待发现
    sendHeartbeat();

    // 如果已经有目标 IP（说明之前已经成功发现或连接过），立即刷新，不等待
    if (!m_currentTargetIp.isEmpty()) {
        m_fileTree->clear();
        QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
        loadingItem->setText(0, "正在刷新文件列表...");
        requestFileList(m_currentTargetIp, m_currentTargetPort, m_currentPath);
        return;
    }

    // 只有在完全没有目标 IP 时，才等待一小段时间尝试通过 UDP 发现教师
    // 将等待时间从 800ms 缩短为 300ms，加快响应速度
    QTimer::singleShot(300, this, [this]() {
        if (!checkTeacherOnline()) {
            qDebug() << "[Warning] No teacher detected after retry via UDP.";
            m_fileTree->clear();
            
            QTreeWidgetItem *tipItem = new QTreeWidgetItem(m_fileTree);
            tipItem->setText(0, "未检测到在线教师端。\n\n可能原因：\n1. 教师端未启动或防火墙拦截。\n2. 网络不在同一网段。\n\n当前配置:\n- 学生监听端口：" + QString::number(DEFAULT_STUDENT_START_PORT) + "\n- 教师广播目标端口：" + QString::number(DEFAULT_STUDENT_START_PORT) + "\n- 教师监听端口：" + QString::number(DEFAULT_TEACHER_UDP_PORT));
            tipItem->setForeground(0, Qt::red);
            tipItem->setFont(0, QFont("Microsoft YaHei", 11));
            
            QTreeWidgetItem *debugItem = new QTreeWidgetItem(m_fileTree);
            debugItem->setText(0, QString("调试信息:\n- 本机 UDP 端口：%1 (标准:%2)\n- 广播目标端口：%3\n- 在线用户数：%4")
                               .arg(m_myUdpPort).arg(DEFAULT_STUDENT_START_PORT)
                               .arg(DEFAULT_TEACHER_UDP_PORT)
                               .arg(m_onlineUsers.size()));
            debugItem->setForeground(0, Qt::darkGray);
            debugItem->setFont(0, QFont("Consolas", 10));
            
            return;
        }

        m_fileTree->clear();
        QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
        loadingItem->setText(0, "正在加载文件列表...");
        
        if (m_currentTargetIp.isEmpty()) {
            for (auto it = m_onlineUsers.begin(); it != m_onlineUsers.end(); ++it) {
                if (it->isTeacher) {
                    m_currentTargetIp = it->ip;
                    m_currentTargetPort = it->tcpPort;
                    m_currentTargetName = it->nickName;
                    
                    // 【新增】通过列表发现教师时也记录 IP
                    m_teacherIp = it->ip;
                    break;
                }
            }
        }

        if (m_currentTargetIp.isEmpty()) {
             loadingItem->setText(0, "错误：未找到目标教师 IP");
             return;
        }

        requestFileList(m_currentTargetIp, m_currentTargetPort, m_currentPath);
    });
}

void client::onJoinClassClicked() {
    QString className = m_classComboBox->currentText().trimmed();
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
    sendHeartbeat();

    // 4. 保存班级到本地配置
    saveUserSettings();

    // 5. 给用户明确的成功反馈
    QMessageBox::information(this, "加入成功", 
        "已成功加入班级：**" + className + "**\n\n"
        "教师端刷新列表后即可在'班级'列看到该信息。\n"
        "心跳包已即时发送，班级信息已同步至服务器。");
    
    qDebug() << "[Class] Student joined class:" << className << ", heartbeat sent immediately.";
}
