#include "client.h"
#include <QDebug>
#include <QSettings>//持久化设置存储类，用于保存和读取应用程序的配置信息
#include <QScreen>
#include <QBuffer>

#ifdef Q_OS_WIN
#include <windows.h>//Window API
#include <psapi.h>//进程信息API
#endif


// ==================== 构造函数 ====================
/**
 * @brief 学生端主窗口构造函数
 * @param parent 父窗口指针
 *
 * 初始化成员变量，创建 UI，加载配置，启动网络初始化。
 */
client::client(QWidget* parent)
    :QMainWindow(parent),
    m_udpSocket(new QUdpSocket(this)),//创建UDP套接字，用于发现教师端
    m_heartbeatTimer(new QTimer(this)),//创建心跳定时器
    m_fileServer(new QTcpServer(this)),//创建TCP服务器，用于接收文件传输请求
    m_myIp(""),//本机IP
    m_myUdpPort(DEFAULT_STUDENT_START_PORT),//本机UDP端口固定为8889
    m_myTcpPort(0),//本机TCP端口初始为0，后面会随机生成
    m_myNickName(""),//昵称
    m_localSavePath("E:/fileReceive"),//默认下载保存路径
    m_avatarPath(""),//头像路径
    m_studentId(""),//学生id
    m_state(State_Offline),//初始状态
    m_currentTargetIp(""),//目标教师IP
    m_currentTargetPort(0),//目标教师端口
    m_currentTargetName(""),//教师昵称
    m_currentPath("/"),//文件浏览当前路径为根目录
    m_manualTeacherIp(""),//手动连接的教师ip
    m_manualTeacherTcpPort(0),//手动连接的教师端口
    m_teacherIp("")//教师端ip初始为空
{
    //------------获取本机IP地址------------
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    bool ipFound = false;

    //IP选择策略：优先选择常见的私有局域网网段，避免选中VPN或虚拟网卡IP
    QString preferredIp;//首选私有网段IP
    QString fallbackIp;//备用其他的IPv4地址

    //遍历所有网络接口地址
    for(const QHostAddress &addr : list)
    {
        //只考虑IPv4且非回环地址
        if(addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
        {
            QString ipStr = addr.toString();
            //逃过127开头的地址
            if(ipStr.startsWith("127.")) continue;

            //判断是否为常见私有网段
            bool isPrivate = ipStr.startsWith("192.168.") ||
                            ipStr.startsWith("10.") ||
                             (ipStr.startsWith("172.") && [&](){
                                 //检查172.16.0.0 - 172.31.255.255
                                 QStringList parts = ipStr.split('.');
                                 if(parts.size() >= 2)
                                 {
                                     int second = parts[1].toInt();
                                     return (second >= 16 && second <= 31);
                                 }
                                 return false;
                             }());//立即调用该匿名函数
            if(isPrivate)
            {
                preferredIp = ipStr;
                break;//找到首选IP立即停止
            }

            //记录一个备用的IP
            if(fallbackIp.isEmpty())
            {
                fallbackIp = ipStr;
            }
        }
    }

    //应用选择结果
    if(!preferredIp.isEmpty())
    {
        m_myIp = preferredIp;
        ipFound = true;
        qDebug() <<"[Init] 已选择首选私有IP："<<m_myIp;
    }
    else if(!fallbackIp.isEmpty())
    {
        m_myIp = fallbackIp;
        ipFound = true;
        qDebug() <<"[Init] 未发现私有IP，使用备份："<<m_myIp;
    }

    //如果仍然没有有效的IP，就回退到本地的回环地址
    if(!ipFound || m_myIp.isEmpty())
    {
        m_myIp = "127.0.0.1";
        qWarning() <<"[Init] 获取有效非回环IP失败，正在回退至本地主机";
    }
    else
    {
        qDebug() << "[Init]  获得本地IP："<<m_myIp;
    }

    //初始化自定义UI(左侧导航栏，好友列表，文件树等)
    initUi();

    //加载固定学生ID和本地配置（昵称、班级、头像路径、保存路径）
    loadUserSettings();

    //加载配置之后立即更新UI和显示昵称和头像
    if(!m_myNickName.isEmpty())
    {
        m_nickNameLabel->setText(m_myNickName);
    }

    //如果头像路径存在且文件存在，加载并缩放显示
    if(!m_avatarPath.isEmpty() && QFile::exists(m_avatarPath))
    {
        QPixmap originalPix(m_avatarPath);
        if(!originalPix.isNull())
        {
            QPixmap scaledPix = originalPix.scaled(60,60,Qt::KeepAspectRatio,Qt::SmoothTransformation);
            m_avatarLabel->setPixmap(scaledPix);
            m_avatarLabel->setText("");
        }
    }

    //如果昵称仍为空(首次运行)，就使用IP生成默认昵称
    if(m_myNickName.isEmpty())
    {
        m_myNickName = "学生_" + m_myIp.split('.').last();
        m_nicknameEdit->setText(m_myNickName);
    }
    else
    {
        m_nicknameEdit->setText(m_myNickName);
    }

    //生成随机TCP端口（范围20000 ~ 29999）
    m_myTcpPort = 20000 + QRandomGenerator::global()->bounded(10000);
    qDebug() << "[Init] 生成本地TCP端口："<<m_myTcpPort;

    //初始化默认下载目录，如果不存在则创建
    QDir defaultSaveDir(m_localSavePath);
    if(!defaultSaveDir.exists())
    {
        if(!defaultSaveDir.mkpath("."))
        {
            qDebug() << "[Init] 警告：创建默认文件保存路径失败："<<m_localSavePath;
        }
    }

    //初始化定时器：固定每5分钟发送一次截图
    m_screenshotTimer = new QTimer(this);
    m_screenshotTimer->setInterval(300000);
    connect(m_screenshotTimer,&QTimer::timeout,this,[this](){
        qDebug() << "[监控] 5分钟定时到达，发送定时截图";
        captureAndSendScreenshot(true);//true表示定期监控
    });

    //初始化应用检查定时器，每秒检查一次前台应用
    m_appCheckTimer = new QTimer(this);
    m_appCheckTimer->setInterval(1000);
    connect(m_appCheckTimer,&QTimer::timeout,this,&client::onCheckBlacklistTimeout);

    //监控相关标志初始化
    m_isMonitoringEnabled = false;
    m_isReportingViolated = false;
    m_lastViolatedTime = 0;

    //启动网络初始化（绑定UDP、启动心跳等）
    startNetworkInitialization();
}

// 析构函数
client::~client()
{
    // 停止所有定时器
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        disconnect(m_heartbeatTimer, nullptr, this, nullptr);
    }
    if (m_appCheckTimer) {
        m_appCheckTimer->stop();
        disconnect(m_appCheckTimer, nullptr, this, nullptr);
    }
    if (m_screenshotTimer) {
        m_screenshotTimer->stop();
        disconnect(m_screenshotTimer, nullptr, this, nullptr);
    }

    // 关闭 UDP 套接字
    if (m_udpSocket) {
        m_udpSocket->close();
        disconnect(m_udpSocket, nullptr, this, nullptr);
    }

    // 关闭 TCP 服务器
    if (m_fileServer) {
        m_fileServer->close();
        disconnect(m_fileServer, nullptr, this, nullptr);
    }

    stopCameraStream();
    qDebug() << "学生端退出，资源已清理！";
}

// ==================== 配置加载与保存 ====================
/**
 * @brief 加载用户设置（固定 ID、昵称、班级、头像、保存路径）
 *
 * 从 QSettings 中读取之前保存的配置，如果没有则生成新的学生 ID。
 */
void client::loadUserSettings()
{
    QSettings settings("XUPT","TeachMonitor_student");

    //1.加载或生成固定学生ID
    m_studentId = settings.value("studentId").toString();
    if(m_studentId.isEmpty())
    {
        //生成UUID作为唯一标识
        m_studentId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("studentId",m_studentId);
        qDebug() << "[学生] 生成固定ID:"<<m_studentId;
    }
    else
    {
        qDebug()<<"[学生] 加载已存在的固定ID:"<<m_studentId;
    }

    //2.加载昵称
    QString savedNick = settings.value("nickname").toString();
    if(!savedNick.isEmpty())
    {
        m_myNickName = savedNick;
        //设置窗口标题，显示当前昵称
        this->setWindowTitle("教室监控管理系统客户端 - " + m_myNickName);
    }

    //3.加载班级
    QString savedClass = settings.value("class").toString();
    if(!savedClass.isEmpty())
    {
        m_currentClassName = savedClass;
        qDebug() << "[学生]加载已存在的班级："<<m_currentClassName;
    }

    //4.加载头像路径
    QString savedAvatar = settings.value("avatar").toString();
    if(!savedAvatar.isEmpty() && QFile::exists(savedAvatar))
    {
        m_avatarPath = savedAvatar;
    }

    //5.加载保存路径
    QString savedSavePath = settings.value("savePath").toString();
    if(!savedSavePath.isEmpty())
    {
        m_localSavePath = savedSavePath;
    }

    //更新UI显示当前班级状态
    QLabel* label = this->findChild<QLabel*>("currentClassLabel");
    if(label)
    {
        if(!m_currentClassName.isEmpty())
        {
            label->setText("当前班级:<b>" + m_currentClassName + "</b>");
            label->setStyleSheet("color:#27ae60;font-size:14px;font-weight:bold;margin-top:5px;");
            qDebug() << "[UI] 班级标签更新为："<<m_currentClassName;
        }
        else
        {
            label->setText("当前班级：未加入");
            label->setStyleSheet("color:#e67e22;font-size:14px;font-weight:bold;margin-top:5px;");
        }
    }
}

/**
 * @brief 保存用户设置（固定 ID、昵称、班级、头像、保存路径）
 */
void client::saveUserSettings()
{
    QSettings settings("XUPT","TeachMonitor_student");
    settings.setValue("studentId",m_studentId);
    settings.setValue("nickname",m_myNickName);
    settings.setValue("class",m_currentClassName);
    if(!m_avatarPath.isEmpty() && QFile::exists(m_avatarPath))
    {
        settings.setValue("avatar",m_avatarPath);
    }
    settings.setValue("savePath",m_localSavePath);
    settings.sync();//强制写入磁盘
    qDebug()<<"[Settings] 已保存用户配置！";
}

//============网络初始化===========
/**
 * @brief 启动网络初始化
 *
 * 绑定 UDP 端口，启动心跳定时器，创建文件服务器，启动监控。
 */
void client::startNetworkInitialization()
{
    m_state = State_Offline;//初始离线状态
    m_statusLabel->setText("● 离线 (正在尝试连接...)");
    m_statusLabel->setStyleSheet("color:#f39c12;font-size:12px;");

    quint16 targetPort = DEFAULT_STUDENT_START_PORT;//学生端监听端口8889

    //绑定UDP套接字到任意地址，端口为targetPort,允许地址共享和重用
    if(!m_udpSocket->bind(QHostAddress::Any,targetPort,QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        QString errorMsg = m_udpSocket->errorString();
        qDebug()<<"[错误] UDP绑定到标准端口"<<targetPort<<"失败 :"<<errorMsg;

        m_statusLabel->setText("● 离线 (端口 8889 被占用)");
        m_statusLabel->setStyleSheet("color:#e74c3c;font-size:12px;");

        QString errorDetail = QString("无法绑定 UDP 端口 %1。\n错误：%2\n\n可能原因：\n1另一个学生端或老师端正在运行.。\n2. 端口被其他程序占用。\n\n解决方案：\n- 请关闭其他占用端口的程序后重试。\n- 或者使用界面上的'手动连接'功能（通过 TCP 直连，不受此限制）。").arg(targetPort).arg(errorMsg);

        showNetworkErrorInUI(errorDetail);
        return;
    }

    m_myUdpPort = targetPort;//记录本机UDP端口
    qDebug() << "[信息] ✅成功监听标准端口："<<m_myUdpPort;

    //连接UDP数据就绪信号
    connect(m_udpSocket,&QUdpSocket::readyRead,this,&client::onUdpReadyRead);
    //连接心跳定时器信号
    connect(m_heartbeatTimer,&QTimer::timeout,this,&client::sendHeartbeat);

    //启动TCP文件服务器，监听随机端口
    if(!m_fileServer->listen(QHostAddress::Any,m_myTcpPort))
    {
        qDebug()<<"[错误] P2P服务器监听失败："<<m_fileServer->errorString();
    }
    else
    {
        qDebug()<<"[信息] P2P文件服务器监听在："<<m_myTcpPort<<"端口";
        //有新连接时触发onNewPeerConnection
        connect(m_fileServer,&QTcpServer::newConnection,this,&client::onNewPeerConnection);
    }

    startUdpDiscovery();//启动UDP发现（心跳定时器）
    m_state = State_Online;

    m_statusLabel->setText("● 在线 (UDP:" + QString::number(m_myUdpPort) + ",TCP:" + QString::number(m_myTcpPort) + ")");
    m_statusLabel->setStyleSheet("color : #2ecc71; font-size:12px;");

    sendHeartbeat();//立即发送一次心跳，让教师端尽快发现

    //启动应用检查定时器
    if(!m_appCheckTimer->isActive())
    {
        m_isMonitoringEnabled = true;
        m_appCheckTimer->start(1000);
        qDebug() << "[监控] 实时监控已开启。检测间隔：1s";
        qDebug() << "[监控] 计时器是否激活："<<m_appCheckTimer->isActive() << " 间隔："<<m_appCheckTimer->interval();
    }
    else
    {
        qDebug() << "[监控] 警告：应用检查已经开启！";
    }
}

/**
 * @brief 在 UI 中显示网络错误信息
 * @param errorDetail 错误详情
 */
void client::showNetworkErrorInUI(const QString &errorDetail)
{
    m_friendList->clear();//清空列表

    //创建一个错误项添加到列表中
    QListWidgetItem *errItem = new QListWidgetItem();
    errItem->setText("❌ 网络初始化失败\n\n错误：" + errorDetail + "\n\n可能原因：\n1. 端口范围被其他程序大量占用。\n2. 权限严重不足。\n\n操作：\n- 请关闭其他占用网络的程序后重试。");
    errItem->setForeground(QBrush(Qt::red));
    errItem->setFont(QFont("Microsoft YaHei",10));
    errItem->setSizeHint(QSize(0,150));
    m_friendList->addItem(errItem);
}

/**
 * @brief 启动 UDP 发现（即启动心跳定时器）
 */
void client::startUdpDiscovery()
{
    m_heartbeatTimer->start(3000);//每3秒发送一次心跳
}

//===============心跳相关==============
/**
 * @brief 发送心跳包（UDP 广播或单播）
 *
 * 格式：HEARTBEAT|昵称|IsTeacher|IP|UdpPort|TcpPort|班级|学生ID
 */
void client::sendHeartbeat()
{
    if(m_state != State_Online) return;//不在线时不发送

    //构建心跳消息
    QString msg = QString("HEARTBEAT|%1|%2|%3|%4|%5|%6|%7")
                    .arg(m_myNickName)
                    .arg("0")   //学生端isTeacher = false
                    .arg(m_myIp)
                    .arg(m_myUdpPort)
                    .arg(m_myTcpPort)
                    .arg(m_currentClassName)
                    .arg(m_studentId);  //发送固定ID

    QByteArray data = msg.toUtf8();
    QHostAddress targetAddr;

    //如果已知教师端IP，优先使用单播发送心跳，防止广播可能被防火墙拦截的问题
    if(!m_teacherIp.isEmpty())
    {
        targetAddr = QHostAddress(m_teacherIp);
        qDebug() << "[UDP] 向教师端发送单播心跳,老师Ip："<<m_teacherIp;
    }
    else
    {
        targetAddr = QHostAddress::Broadcast;
        qDebug() << "[UDP] 教师端IP未知，发送广播心跳。";
    }

    //发送UDP数据报，目标端口为教师端监听端口DEFAULT_TEACHER_UDP_PORT(9999)
    qint64 bytesSent = m_udpSocket->writeDatagram(data,targetAddr,DEFAULT_TEACHER_UDP_PORT);

    if(bytesSent == -1)
    {
        qDebug() << "[UDP 错误] 向" <<targetAddr.toString() <<"发送心跳失败："<<m_udpSocket->errorString();
    }
}

// ==================== UDP 数据接收处理 ====================
/**
 * @brief UDP 数据就绪时的槽函数
 *
 * 处理来自教师端的广播消息：
 * - USER_LIST|classes：班级列表
 * - HEARTBEAT：教师端心跳
 */
void client::onUdpReadyRead()
{
    while(m_udpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderIp;
        quint16 senderPort;

        qint64 size = m_udpSocket->readDatagram(datagram.data(),datagram.size(),&senderIp,&senderPort);
        if(size == -1)
        {
            qDebug() <<"[UDP错误] 读取数据报失败:"<<m_udpSocket->errorString();
            continue;
        }

        QString contentPreview = QString::fromUtf8(datagram);
        //忽略自己发出的心跳包
        if(senderIp.toString() == m_myIp && contentPreview.startsWith("HEARTBEAT|" + m_myNickName))
        {
            continue;
        }

        QString content = contentPreview;
        QStringList parts = content.split('|');

        //------------处理USER_LIST消息（教师端广播的用户列表）-----------
        if(parts.size() >= 2 && parts[0] == "USER_LIST")
        {
            QString jsonStr = parts[1];
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(),&error);

            if(error.error == QJsonParseError::NoError && doc.isObject())
            {
                QJsonObject rootObj = doc.object();

                //解析班级列表并更新UI下拉框
                if(rootObj.contains("classes") && rootObj["classes"].isArray())
                {
                    QJsonArray classArray = rootObj["classes"].toArray();

                    qDebug() << "[班级] 从教师端接收到班级列表，数量："<<classArray.size();

                    if(m_classComboBox)
                    {
                        QString currentSelection = m_classComboBox->currentText();

                        m_classComboBox->clear();
                        m_classComboBox->addItem("请选择班级...","");//添加占位项

                        bool hasRealClasses = false;
                        bool currentClassStillExists = false;//当前选择的班级是否仍存在

                        for(const QJsonValue &val : classArray)
                        {
                            QString className = val.toString();
                            if(!className.trimmed().isEmpty())
                            {
                                m_classComboBox->addItem(className);
                                hasRealClasses = true;
                                if(className == m_currentClassName)
                                {
                                    currentClassStillExists = true;
                                }
                            }
                        }

                        //如果当前班级已经被教师端删除，清除本地班级信息并更新UI
                        if(!m_currentClassName.isEmpty() && !currentClassStillExists)
                        {
                            qDebug() << "[班级] 当前班级 "<<m_currentClassName<<" 已经被老师删除";
                            m_currentClassName = "";

                            QLabel * label = this->findChild<QLabel*>("currentClassLabel");
                            if(label)
                            {
                                label->setText("当前班级：未加入(原班级已被删除)");
                                label->setStyleSheet("color:#e74c3c;font-size:14px;font-weight:bold;margin-top:5px;");
                            }
                        }
                        else if(!currentSelection.isEmpty() && currentSelection != "请选择班级..." && currentClassStillExists)
                        {
                            //恢复之前的选择
                            int index = m_classComboBox->findText(currentSelection);
                            if(index != -1)
                            {
                                m_classComboBox->setCurrentIndex(index);
                            }
                        }

                        if(hasRealClasses)
                        {
                            m_classComboBox->repaint();//强制刷新下拉框
                            qDebug() << "[班级] UI更新成功";
                        }
                    }
                }
                else
                {
                    qDebug() << "[班级] JSON中没有'classes'项或者其不是一个数组";
                }
            }
            return;//处理完USER_LIST后就不再继续
        }

        if(parts.size() >= 3 && parts[0] == "GET_CAMERA_STREAM")
        {
            QString teacherIp = parts[1];
            quint16 teacherPort = parts[2].toUShort();//教师端临时TCP端口

            qDebug() << "[UDP] 收到教师端摄像头请求，目标端口:"<<teacherPort;
            startCameraStream(teacherIp,teacherPort);//启动摄像头流
            continue;
        }

        //----------处理HEARTBEAT消息（教师端心跳）-----------
        if(parts.size() >= 6 && parts[0] == "HEARTBEAT")
        {
            QString nick = parts[1];
            bool isTeacher = (parts[2] == "1");
            QString ip = parts[3];
            quint16 uPort = parts[4].toUShort();
            quint16 tPort = parts[5].toUShort();

            UserInfo user;
            user.ip = ip;
            user.port = uPort;
            user.tcpPort = tPort;
            user.id = QString("%1:%2").arg(ip).arg(uPort);//临时id
            user.nickName = nick;
            user.isTeacher = isTeacher;
            user.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();

            updateUserList(user);//更新好友列表

            //如果是教师端，更新当前目标信息
            if(isTeacher)
            {
                if(m_currentTargetIp.isEmpty() || m_currentTargetPort != tPort || m_currentTargetIp != ip)
                {
                    m_currentTargetIp = ip;
                    m_currentTargetPort = tPort;
                    m_currentTargetName = nick;

                    //收到教师心跳时，记录其IP
                    m_teacherIp = ip;
                    qDebug() << "[心跳] 收到教师端ip:"<<ip<<"和端口:"<<tPort;
                }
            }
        }
    }

    //----------------清理超时用户（超过10秒未收到心跳）----------------
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> offlineUsers;
    for(auto it = m_onlineUsers.begin();it != m_onlineUsers.end() ;++it)
    {
        if(now - it->lastHeartbeat > 10000)
        {
            offlineUsers.append(it.key());
        }
    }
    for(const QString &id : offlineUsers)
    {
        removeUser(id);//从列表和UI中移除
    }
}

//==============用户列表管理==============
/**
 * @brief 更新在线用户列表 UI
 * @param user 用户信息
 */
void client::updateUserList(const UserInfo &user)
{
    bool isNew = !m_onlineUsers.contains(user.id);//是否为新用户
    m_onlineUsers[user.id] = user;//更新映射

    if(isNew)
    {
        //新用户：创建列表项并添加到好友列表
        QListWidgetItem* item = new QListWidgetItem();
        QString iconStr = user.isTeacher ? "👨‍🏫" : "👨‍🎓";
        QString displayText = QString("%1 %2   IP : %3   端口 : %4").arg(iconStr,2).arg(user.nickName).arg(user.ip).arg(user.port);

        item->setText(displayText);
        item->setData(Qt::UserRole,user.id);
        item->setSizeHint(QSize(0,60));

        if(user.isTeacher)
        {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setForeground(QColor("#2980b9"));
        }

        m_friendList->addItem(item);
        qDebug() << "[UI] 添加用户："<<user.nickName;
    }
    else
    {
        //用户已存在，更新对应的列表项文本
        for(int i = 0;i < m_friendList->count();++i)
        {
            QListWidgetItem* item = m_friendList->item(i);
            if(item->data(Qt::UserRole).toString() == user.id)
            {
                QString iconStr = user.isTeacher ? "👨‍🏫" : "👨‍🎓";
                item->setText(QString("%1 %2   IP : %3   端口 : %4").arg(iconStr,2).arg(user.nickName).arg(user.ip).arg(user.port));
                break;
            }
        }
    }
}

/**
 * @brief 移除离线用户
 * @param uniqueKey 用户唯一标识(ID)
 */
void client::removeUser(const QString &uniqueKey)
{
    if(!m_onlineUsers.contains(uniqueKey)) return;

    m_onlineUsers.remove(uniqueKey);//从映射中删除
    //从好友列表中删除对应项
    for(int i = 0;i < m_friendList->count() ;++i)
    {
        QListWidgetItem* item = m_friendList->item(i);
        if(item && item->data(Qt::UserRole).toString() == uniqueKey)
        {
            delete m_friendList->takeItem(i);
            qDebug() << "[UI] 删除离线用户："<<uniqueKey;
            break;
        }
    }
}

//=============其他工具函数============
/**
 * @brief 检查是否有教师端在线
 * @return true 存在在线教师，false 无教师在线
 */
bool client::checkTeacherOnline()
{
    for(auto it = m_onlineUsers.begin();it != m_onlineUsers.end();++it)
    {
        if(it->isTeacher)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief 尝试直接连接教师端（刷新共享文件夹按钮）
 * @param teacherIp 教师 IP
 * @param teacherTcpPort 教师 TCP 端口
 */
void client::tryDirectConnect(const QString &teacherIp,quint16 teacherTcpPort)
{
    QTcpSocket* socket = new QTcpSocket(this);
    socket->connectToHost(teacherIp,teacherTcpPort);

    if(!socket->waitForConnected(3000))
    {
        QString errStr = socket->errorString();
        if(socket)
        {
            QTreeWidgetItem *errItem = new QTreeWidgetItem(m_fileTree);
            errItem->setText(0,"❌ 直接连接失败：\n" + errStr + "\n请检查 IP 是否正确及防火墙设置。");
            errItem->setForeground(0,Qt::red);
            socket->deleteLater();
        }
        return;
    }

    //构造一个假的教师用户信息，添加到在线列表
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

    qDebug() << "[手动连接] 成功连接到IP:"<<teacherIp<<",端口:"<<teacherTcpPort;

    //请求文件列表
    requestFileList(teacherIp,teacherTcpPort,m_currentPath);

    socket->disconnectFromHost();
    socket->deleteLater();
}

//=============设置相关==============
/**
 * @brief 保存昵称按钮点击响应
 */
void client::onSaveNicknameClicked()
{
    QString newNick = m_nicknameEdit->text().trimmed();
    if(newNick.isEmpty())
    {
        QMessageBox::warning(this,"提示","昵称不能为空");
        return;
    }

    m_myNickName = newNick;
    m_nickNameLabel->setText(m_myNickName);
    //设置窗口标题，显示当前昵称
    this->setWindowTitle("教室监控管理系统客户端 - " + m_myNickName);
    sendHeartbeat();//立即发送心跳，同步教师端的学生昵称

    //保存昵称到本地配置
    saveUserSettings();
    QMessageBox::information(this,"成功","昵称已修改为:" + newNick + "\n将同步至教师端。");
}

/**
 * @brief 更换头像按钮点击响应
 */
void client::onChangeAvatarClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,"选择头像图片","",
                                                    "图片文件类型(*.png *.jpg *.jepg *.bmp *.gif)");

    if(filePath.isEmpty()) return;

    m_avatarPath = filePath;
    QPixmap originalPix(filePath);
    if(originalPix.isNull())
    {
        QMessageBox::warning(this,"错误","无法加载该图片文件");
        return;
    }

    QPixmap scaledPix = originalPix.scaled(60,60,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    m_avatarLabel->setPixmap(scaledPix);
    m_avatarLabel->setText("");

    //保存头像路径到本地配置
    saveUserSettings();

    QMessageBox::information(this,"成功","头像已更换");
}

//==============黑名单检查与违规上报==============
/**
 * @brief 定时检查前台应用是否在黑名单中（每秒触发）
 */
void client::onCheckBlacklistTimeout()
{
    if(!m_isMonitoringEnabled) return;//监控未开启

    QString appName = getCurrentForegroundApp();//获取当前前台应用名称

    static int logCounter = 0;
    logCounter++;
    if(appName.isEmpty())
    {
        if(logCounter % 30 == 0)
        {
            qDebug() << "[监控节点] 当前应用:无";
        }
    }
    else
    {
        qDebug() << "[监控节点] 当前应用:"<<appName;
    }

    //白名单应用
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
        "qt"
    };

    //先检查是否在白名单中，如果是则不违规，跳过黑名单检查
    bool isWhitelisted = false;
    if(!appName.isEmpty())
    {
        QString lowerApp = appName.toLower();
        for(const QString &white : WHITELIST_APPS)
        {
            if(lowerApp.contains(white.toLower()))
            {
                isWhitelisted = true;
                break;
            }
        }
    }

    bool isViolated = false;
    QString matchedApp = "";

    if(isWhitelisted)
    {
        //白名单应用，视为正常，不进行违规检测
        isViolated = false;
        matchedApp = "";
    }
    else
    {
        //检查黑名单
        if(!appName.isEmpty())
        {
            for(const QString& keyword : BLACKLIST_APPS)
            {
                QString lowerKeyword = keyword.toLower();
                if(appName.contains(lowerKeyword))
                {
                    isViolated = true;
                    matchedApp = appName;
                    qDebug() << "[违规应用] APP:" <<appName <<" 匹配的关键字"<<keyword;
                    break;
                }
            }
        }
    }

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if(isViolated)
    {
        //违规状态：如果尚未上报，且距离上次恢复超过10秒，则上报
        if(!m_isReportingViolated)
        {
            qint64 timeSinceRecover = (m_lastViolatedTime == 0) ? 99999 : (currentTime - m_lastViolatedTime);

            if(timeSinceRecover >= 10000)
            {
                qDebug() <<"[状态改变] 正常 -> 违规 ！APP:"<< matchedApp << "(冷却期已过)";

                captureAndSendScreenshot(false,matchedApp);//发送违规报告，带截图
                m_isReportingViolated = true;
            }
            else
            {
                qDebug() <<"[冷却] 正常 -> 忽视本次违规(当前处于10s冷却期内).距离上次恢复正常已过"<<timeSinceRecover<<"ms";
            }
        }
    }
    else
    {
        //正常状态，如果之前正在上报，则发送恢复信号
        if(m_isReportingViolated)
        {
            qDebug() << "[状态改变] 违规 -> 正常";
            m_lastViolatedTime = currentTime;//记录恢复正常的时间
            captureAndSendScreenshot(false,"Status_Recovered");//发送恢复信号
            m_isReportingViolated = false;
            qDebug()<<"[恢复] 恢复状态已发送至教师端，状态重置.";
        }
    }
}

//=====================获取当前前台应用名称（Windows平台）======================
QString client::getCurrentForegroundApp()
{
#ifdef Q_OS_WIN
    HWND hWnd = GetForegroundWindow();//获取前台窗口句柄（用户正在交互的顶层窗口，如果没有前台窗口，比如正在桌面，返回nullptr）
    if(hWnd == NULL)
    {
        return "";
    }

    QString resultName = "";

    //1.获取窗口标题
    wchar_t title[512];
    int len = GetWindowTextW(hWnd,title,512);
    QString windowTitle = "";
    if(len > 0)
    {
        windowTitle = QString::fromWCharArray(title).toLower();

        qDebug() << "[监控] 窗口标题："<<windowTitle;

        QString lowerTitle = windowTitle;
        //检查窗口标题是否包含黑名单关键字
        for(const QString &keyword : BLACKLIST_APPS)
        {
            QString lowerKeyWord = keyword.toLower();
            if(lowerTitle.contains(lowerKeyWord))
            {
                qDebug() << "[黑名单匹配]发现违规窗口标题:" <<windowTitle<<" ,关键字:"<<keyword;
                return windowTitle;//直接返回窗口标题作为违规应用名
            }
        }
    }
    else
    {
        qDebug() << "[监控调试] GetWindowTextW函数返回空或者执行失败";
    }

    //2.获取进程名
    DWORD processId = 0;
    GetWindowThreadProcessId(hWnd,&processId);//获取前台窗口所属进程的PID

    if(processId != 0)
    {
        //打开进程句柄,请求查询进程信息和读取进程内存，第二个参数FALSE表示不继承句柄，第三个参数是进程ID
        //返回进程句柄：成功：非0
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ ,FALSE,processId);
        if(hProcess)
        {
            wchar_t exePath[MAX_PATH];//存储进程可执行文件的完整路径
            if(GetModuleFileNameExW(hProcess,NULL,exePath,MAX_PATH))//获取进程可执行文件的完整路径(包括文件名),成功返回非零
            {
                QString fullPath = QString::fromWCharArray(exePath);//将宽字符路径转换为QString对象
                QFileInfo fi(fullPath);//解析文件路径，提取文件信息
                QString procName = fi.fileName().toLower();//进程名（比如qq.exe）（获取文件名）
                qDebug() << "[监控调试] 进程名:"<<procName;

                //浏览器列表(仅当窗口标题未匹配时，跳过进程名检查，避免误报)
                QStringList browsers = {"chrome.exe","msedge.exe","firefox.exe","iexplore.exe","browser.exe","360se.exe","qqbrowser.exe","edge","ie"};
                bool isBrowser = false;
                for(const QString& b : browsers)
                {
                    if(procName == b)
                    {
                        isBrowser = true;//判断是否为浏览器
                        break;
                    }
                }

                //如果是浏览器且窗口标题未匹配，则跳过进程名检查
                if(isBrowser)
                {
                    qDebug() << "[监控调试] 当前为浏览器且标题不匹配，跳过进程安全检查." ;
                    CloseHandle(hProcess);
                    return "";//如果是浏览器就跳过
                }

                //检查进程名是否包含黑名单关键字
                for(const QString &keyword : BLACKLIST_APPS)
                {
                    QString lowerKeyword = keyword.toLower();
                    QString procNameNoExt = fi.baseName().toLower();//不含扩展的进程名

                    if(procName.contains(lowerKeyword) || procNameNoExt.contains(lowerKeyword))
                    {
                        qDebug() << "[黑名单匹配] 发现违规进程:" <<procName<<",关键字:"<<keyword;
                        CloseHandle(hProcess);
                        return procName;//返回进程名
                    }
                }
            }
            else
            {
                DWORD err = GetLastError();
                qDebug() << "[监控调试] GetModuleFileNameExW函数执行失败,进程PID: "<<processId<<",错误:"<<err;
            }
            CloseHandle(hProcess);//关闭进程句柄
        }
        else
        {
            DWORD err = GetLastError();
            qDebug() << "[监控调试] 打开进程失败，PID:"<<processId<<",错误："<<err;
        }
    }

    return "";//未检测到违规
#else
    return "";
#endif
}

//===============截图与上报=================
/**
 * @brief 截取屏幕并发送给教师端
 * @param isPeriodic 是否为定期巡查（true：定期巡查，false：违规上报）
 * @param violatedAppName 违规应用名称（用于标识）
 *
 * 根据参数构建 JSON 元数据，捕获屏幕截图（JPEG 质量 75），通过 TCP 发送给教师端。
 */
void client::captureAndSendScreenshot(bool isPeriodic,const QString &violatedAppName)
{
    qDebug() << "[发送截图]  违规应用名:"<<violatedAppName;

    //如果没有目标教师信息，尝试从在线列表中查找
    if(m_currentTargetIp.isEmpty() || m_currentTargetPort == 0)
    {
        bool found = false;
        qDebug() << "[截图] 当前目标为空，搜索在线用户中...";
        for(auto it = m_onlineUsers.begin();it != m_onlineUsers.end();++it)
        {
            if(it->isTeacher)
            {
                m_currentTargetIp = it->ip;
                m_currentTargetPort = it->tcpPort;//使用教师端的TCP端口
                m_currentTargetName = it->nickName;
                found = true;

                //找到教师时记录IP
                m_teacherIp = it->ip;
                qDebug() <<"[发送截图] 发现教师:"<<it->nickName<<",IP:"<<it->ip<<",端口:"<<it->tcpPort;
                break;
            }
        }
        if(!found)
        {
            qDebug() << "[发送截图] 在线表中没有发现教师！";
            return;
        }
    }

    //构建JSON元数据
    QJsonObject metaObj;
    QString typeTag = "";

    if(violatedAppName == "Status_Recovered")
    {
        typeTag = "STATUS_RECOVERY";
    }
    else if(violatedAppName == "Live_Request_Response")
    {
        typeTag = "LIVE_RESPONSE";
    }
    else
    {
        typeTag = isPeriodic ? "PERIODIC_MONITOR" : "VIOLATION_REPORT";
    }

    metaObj["type"] = typeTag;
    metaObj["appName"] = violatedAppName;
    metaObj["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    //使用固定学生ID代替IP：端口
    metaObj["studentId"] = m_studentId;

    //在TCP数据包中携带昵称、班级和TCP端口，作为UDP心跳丢失时的备用信息源
    metaObj["nickName"] = m_myNickName;
    metaObj["className"] = m_currentClassName;
    metaObj["tcpPort"] = m_myTcpPort;

    QByteArray jsonData = QJsonDocument(metaObj).toJson(QJsonDocument::Compact);
    QByteArray imageData;

    //仅当不是状态恢复时，才捕获截图
    if(typeTag != "STATUS_RECOVERY")
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        if(screen)
        {
            QPixmap screenshot = screen->grabWindow(0);//捕获整个屏幕
            if(!screenshot.isNull())
            {
                QBuffer buffer(&imageData);
                buffer.open(QIODevice::WriteOnly);
                screenshot.save(&buffer,"JPG",75);//JPEG格式，质量75%
                buffer.close();
            }
        }
    }

    //创建TCP套接字并连接到教师端
    QTcpSocket *socket = new QTcpSocket(this);

    //设置连接超时定时器3秒
    QTimer* connectTimer = new QTimer(socket);
    connectTimer->setSingleShot(true);
    connect(connectTimer,&QTimer::timeout,[socket](){
        if(socket->state() == QAbstractSocket::ConnectingState || socket->state() == QAbstractSocket::ConnectedState)
        {
            socket->abort();
            socket->deleteLater();
        }
    });
    connectTimer->start(3000);

    //连接成功时发送数据
    connect(socket,&QTcpSocket::connected,this,[socket,jsonData,imageData,connectTimer](){
        connectTimer->stop();

        //发送协议头:MONITOR_START|jsonSize|imageSize
        QString headerStr = QString("MONITOR_START|%1|%2").arg(jsonData.size()).arg(imageData.size());
        QByteArray headerData = headerStr.toUtf8();

        //发送长度前缀+头数据
        QByteArray block;
        QDataStream out(&block,QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_15);
        out << (quint32)headerData.size();
        out.writeRawData(headerData.constData(),headerData.size());
        socket->write(block);

        //发送JSON数据(带长度前缀)
        QByteArray jsonBlock;
        QDataStream jsonOut(&jsonBlock,QIODevice::WriteOnly);
        jsonOut.setVersion(QDataStream::Qt_5_15);
        jsonOut<<(quint32)jsonData.size();
        jsonOut.writeRawData(jsonData.constData(),jsonData.size());
        socket->write(jsonBlock);

        //发送图片数据(带长度前缀)
        QByteArray imgBlock;
        QDataStream imgOut(&imgBlock,QIODevice::WriteOnly);
        imgOut.setVersion(QDataStream::Qt_5_15);
        imgOut << (quint32)imageData.size();
        if(!imageData.isEmpty())
        {
            imgOut.writeRawData(imageData.constData(),imageData.size());
        }
        socket->write(imgBlock);
        socket->disconnectFromHost();//发送完成后断开
    });

    //清理套接字
    connect(socket,&QTcpSocket::disconnected,socket,&QTcpSocket::deleteLater);
    connect(socket,QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),[socket](QAbstractSocket::SocketError){
        socket->deleteLater();
    });

    socket->connectToHost(m_currentTargetIp,m_currentTargetPort);
}

// ==================== 用户交互 ====================
/**
 * @brief 双击好友列表项响应（选择教师）
 * @param item 被双击的列表项
 */
void client::onFriendListItemDoubleClicked(QListWidgetItem* item)
{
    QString userId = item->data(Qt::UserRole).toString();
    UserInfo user = m_onlineUsers.value(userId);

    if(user.isTeacher)
    {
        //设置当前目标教师信息
        m_currentTargetIp = user.ip;
        m_currentTargetPort = user.tcpPort;
        m_currentTargetName = user.nickName;
        m_manualTeacherIp = "";

        //记录教师端IP，用于后续UDP单播心跳
        m_teacherIp = user.ip;
        qDebug() <<"[发现] 以记录教师端IP，用于单播心跳,ip:"<<m_teacherIp;

        //切换到文件共享页面
        m_navList->setCurrentRow(1);
        onRefreshClicked();//刷新文件列表
    }
    else
    {
        QMessageBox::information(this,"提示","只能选择教师端进行连接。");
    }
}

/**
 * @brief 刷新文件列表按钮响应
 */
void client::onRefreshClicked()
{
    qDebug() << "[操作] 点击了刷新按钮,检查教师端状态...";
    //如果是手动连接模式，直接连接，不发送心跳等待
    if(!m_manualTeacherIp.isEmpty())
    {
        qDebug()<<"[手动模式] 跳过UDP发现，直接连接到："<<m_manualTeacherIp;
        m_fileTree->clear();
        QTreeWidgetItem* loadingItem = new QTreeWidgetItem(m_fileTree);
        loadingItem->setText(0,"正在连接指定教师端(" + m_manualTeacherIp + ")...");

        //手动连接时也记录教师IP
        m_teacherIp = m_manualTeacherIp;
        tryDirectConnect(m_manualTeacherIp,m_manualTeacherTcpPort);
        return;
    }

    //先发送心跳，然后仅在没有目标IP时才短暂等待发现
    sendHeartbeat();

    //如果已经有目标IP（说明之前已经成功发现或连接过），立即刷新，不等待
    if(!m_currentTargetIp.isEmpty())
    {
        m_fileTree->clear();
        QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
        loadingItem->setText(0,"正在刷新文件列表...");
        requestFileList(m_currentTargetIp,m_currentTargetPort,m_currentPath);
        return;
    }

    //只有在完全没有目标IP时，才等待一小段时间尝试通过UDP发现教师
    QTimer::singleShot(300,this,[this](){
        if(!checkTeacherOnline())
        {
            qDebug() << "[警告]通过UDP重试后未检测到教师端";
            m_fileTree->clear();

            QTreeWidgetItem* tipItem = new QTreeWidgetItem(m_fileTree);
            tipItem->setText(0,"未检测到在线教师端。\n\n可能原因：\n1. 教师端未启动或防火墙拦截。\n2. 网络不在同一网段。\n\n当前配置:\n- 学生监听端口：" + QString::number(DEFAULT_STUDENT_START_PORT) + "\n- 教师广播目标端口：" + QString::number(DEFAULT_STUDENT_START_PORT) + "\n- 教师监听端口：" + QString::number(DEFAULT_TEACHER_UDP_PORT));
            tipItem->setForeground(0,Qt::red);
            tipItem->setFont(0,QFont("Microsoft YaHei",11));

            QTreeWidgetItem* debugItem = new QTreeWidgetItem(m_fileTree);
            debugItem->setText(0,QString("调试信息:\n- 本机 UDP 端口：%1 (标准:%2)\n- 广播目标端口：%3\n- 在线用户数：%4")
                                .arg(m_myUdpPort).arg(DEFAULT_STUDENT_START_PORT)
                                .arg(DEFAULT_TEACHER_UDP_PORT)
                                .arg(m_onlineUsers.size()));
            debugItem->setForeground(0,Qt::darkGray);
            debugItem->setFont(0,QFont("Consolas",10));

            return;
        }

        m_fileTree->clear();
        QTreeWidgetItem* loadingItem = new QTreeWidgetItem(m_fileTree);
        loadingItem->setText(0,"正在加载文件列表...");

        if(m_currentTargetIp.isEmpty())
        {
            for(auto it = m_onlineUsers.begin();it != m_onlineUsers.end();++it)
            {
                if(it->isTeacher)
                {
                    m_currentTargetIp = it->ip;
                    m_currentTargetPort = it->tcpPort;
                    m_currentTargetName = it->nickName;

                    //通过列表发现教师时也记录IP
                    m_teacherIp = it->ip;
                    break;
                }
            }
        }

        if(m_currentTargetIp.isEmpty())
        {
            loadingItem->setText(0,"错误：未找到目标教师IP");
            return;
        }
        requestFileList(m_currentTargetIp,m_currentTargetPort,m_currentPath);
    });
}

//====================班级管理===================
/**
 * @brief 加入班级按钮点击响应
 */
void client::onJoinClassClicked()
{
    QString className = m_classComboBox->currentText().trimmed();
    if(className.isEmpty() || className == "请选择班级...")
    {
        QMessageBox::warning(this,"提示","请先从列表中选择一个有效的班级!");
        return;
    }

    //1.更新本地成员变量
    m_currentClassName = className;

    //2.更新UI显示
    QLabel* label = this->findChild<QLabel*>("currentClassLabel");
    if(label)
    {
        label->setText("当前班级:<b>" + className + "</b>");
        label->setStyleSheet("color:#27ae60;font-size:14px;font-weight:bold;margin-top:5px;");
    }
    else
    {
        qDebug() <<"[警告] 未找到currentClassLabel";
    }

    //3.立即发送心跳包，将班级信息广播给教师端
    sendHeartbeat();

    //4.保存班级到本地配置
    saveUserSettings();

    //5.给用户明确的成功反馈
    QMessageBox::information(this,"加入成功","已成功加入班级: **" + className + "**\n\n"
                                                                "教师端刷新列表后即可在'班级'列看到该信息。\n"
                                                                "心跳包已即时发送，班级信息已同步至服务器。");
    qDebug() <<"[班级] 成功加入班级："<<className;
}

//===============摄像头相关==================

//启动摄像头流，初始化摄像头并连接教师端
void client::startCameraStream(const QString &teacherIp, quint16 teacherPort)
{
    // 检查摄像头可用性，检查系统是否有可用的摄像头设备
    const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    if (cameras.isEmpty()) {
        // 如果没有摄像头，尝试与教师端建立TCP连接并发送错误消息
        QTcpSocket errSock;
        errSock.connectToHost(teacherIp, teacherPort);
        if (errSock.waitForConnected(3000)) {
            QByteArray errMsg = "ERROR:未检测到摄像头设备";
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out << (quint32)errMsg.size();
            out.writeRawData(errMsg.constData(), errMsg.size());
            errSock.write(block);
            errSock.disconnectFromHost();
        }
        return;
    }

    // 创建摄像头，默认使用第一个设备
    m_camera = new QCamera(cameras.first(),this);
    m_videoProbe = new QVideoProbe(this);//视频帧探测器，用于捕获每一帧

    // 将探测器安装到摄像头上
    if (!m_videoProbe->setSource(m_camera)) {
        QMessageBox::warning(this, "错误", "无法捕获摄像头帧，请检查驱动或摄像头占用情况。");
        delete m_camera;
        m_camera = nullptr;
        delete m_videoProbe;
        m_videoProbe = nullptr;
        return;
    }

    // 连接探测信号：每当摄像头产生一帧时，就会调用onCameraFrameProbed
    connect(m_videoProbe, &QVideoProbe::videoFrameProbed,
            this, &client::onCameraFrameProbed);

    // 创建与教师端的TCP连接，用于发送视频流
    m_cameraStreamSocket = new QTcpSocket(this);
    //连接成功时启动摄像头
    connect(m_cameraStreamSocket, &QTcpSocket::connected, [this]() {
        m_camera->start();//开始捕获视频
        m_isCameraActive = true;
    });
    //连接断开时自动停止摄像头流
    connect(m_cameraStreamSocket, &QTcpSocket::disconnected, this, &client::stopCameraStream, Qt::QueuedConnection);
    //connect(m_cameraStreamSocket, &QTcpSocket::disconnected,this, &client::stopCameraStream);
    //连接出错时也停止
    connect(m_cameraStreamSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            [this](QAbstractSocket::SocketError) {
                stopCameraStream();
            });
    //发起连接到教师端提供的临时端口
    m_cameraStreamSocket->connectToHost(teacherIp, teacherPort);
}

//帧捕获处理，处理每一帧并发送给教师端
void client::onCameraFrameProbed(const QVideoFrame &frame)
{
    //检查TCP连接是否仍然有效
    if (!m_cameraStreamSocket ||
        m_cameraStreamSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // 将 QVideoFrame 转换为 QImage
    //QVideoFrame需要映射到内存才能访问像素数据
    QVideoFrame cloneFrame(frame);
    if (!cloneFrame.map(QAbstractVideoBuffer::ReadOnly))
        return;

    //QVideoFrame 像素格式转换
    QImage image;
    //根据像素格式转换成QImage
    if (cloneFrame.pixelFormat() == QVideoFrame::Format_Jpeg) {
        // 如果已经是 JPEG 格式，直接使用数据
        image.loadFromData(cloneFrame.bits(), cloneFrame.mappedBytes(), "JPEG");
    } else {
        // 其他格式转为 QImage（常见如 Format_RGB32）
        QVideoFrame::PixelFormat fmt = cloneFrame.pixelFormat();
        if (fmt == QVideoFrame::Format_RGB32 || fmt == QVideoFrame::Format_ARGB32) {
            image = QImage(cloneFrame.bits(),
                           cloneFrame.width(),
                           cloneFrame.height(),
                           QImage::Format_RGB32);
        } else {
            // 通用转换（效率较低，但兼容性好）
            image = cloneFrame.image();
        }
    }
    cloneFrame.unmap();

    if (image.isNull()) return;

    // 缩放以降低带宽
    QImage scaled = image.scaled(640, 480, Qt::KeepAspectRatio, Qt::FastTransformation);

    // JPEG 编码
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "JPEG", 75);   // 质量 75，平衡画质与带宽
    buffer.close();

    // 发送帧：长度头 + JPEG 数据
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << (quint32)jpegData.size();
    out.writeRawData(jpegData.constData(), jpegData.size());
    m_cameraStreamSocket->write(block);
}

//停止摄像头流
void client::stopCameraStream()
{
    // 【重要】防止重复调用导致崩溃
    if (!m_isCameraActive && !m_camera && !m_videoProbe) {
        return;
    }

    // 1. 先删除视频探测器（必须早于 camera，避免还在处理帧）
    if (m_videoProbe) {
        disconnect(m_videoProbe, nullptr, this, nullptr);
        delete m_videoProbe;
        m_videoProbe = nullptr;
    }

    // 2. 停止并卸载摄像头
    if (m_camera) {
        if (m_camera->state() == QCamera::ActiveState) {
            m_camera->stop();
        }
        m_camera->unload();          // 释放设备驱动
        disconnect(m_camera, nullptr, this, nullptr);
        delete m_camera;
        m_camera = nullptr;
        qDebug() << "[摄像头] 摄像头对象已销毁并卸载";
    }

    // 3. 关闭网络连接
    if (m_cameraStreamSocket) {
        // 断开信号，防止 disconnected 信号再次触发 stopCameraStream 造成递归或错误
        disconnect(m_cameraStreamSocket, nullptr, this, nullptr);
        m_cameraStreamSocket->abort(); // 强制断开

        //m_cameraStreamSocket->disconnectFromHost();
        m_cameraStreamSocket->deleteLater();
        m_cameraStreamSocket = nullptr;
    }

    m_isCameraActive = false;
    qDebug() << "[摄像头] 已完全停止并卸载驱动，灯应熄灭";
}
