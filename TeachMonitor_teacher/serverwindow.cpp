// 包含 ServerWindow 类的头文件
#include "ServerWindow.h"
// 包含自动生成的 UI 头文件（由 Qt Designer 生成）
#include "ui_ServerWindow.h"
// Qt 调试输出
#include <QDebug>
// 消息对话框
#include <QMessageBox>
// 网络地址处理
#include <QHostAddress>
// 获取本机网络接口信息
#include <QNetworkInterface>
// JSON 文档解析与生成
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
// 数据流操作
#include <QDataStream>
// 目录操作
#include <QDir>
// 文件对话框
#include <QFileDialog>
// 图片处理
#include <QPixmap>
// 对话框
#include <QDialog>
// 标签控件
#include <QLabel>
// 垂直布局
#include <QVBoxLayout>
// 滚动区域
#include <QScrollArea>
// 日期时间处理
#include <QDateTime>
// 随机数生成
#include <QRandomGenerator>
// 表格头视图
#include <QHeaderView>
// 滚动条
#include <QScrollBar>
// 屏幕信息
#include <QScreen>
// 进度条
#include <QProgressBar>
// 输入对话框
#include <QInputDialog>
// 下拉框
#include <QComboBox>
#include <QPointer>

// ==================== 辅助结构体 ====================

/**
 * @brief 在线用户信息结构体（内部使用）
 * 用于缓存从 UDP 心跳包中获取的学生信息，包括网络地址、昵称、班级等。
 */
struct UserInfo
{
    QString id;           // 学生唯一标识（固定 ID）
    QString ip;           // 学生 IP 地址
    quint16 port;         // 学生 UDP 端口（用于发现）
    quint16 tcpPort;      // 学生 TCP 端口（用于文件传输和监控数据）
    QString nickName;     // 学生昵称
    bool isTeacher;       // 是否为教师（本结构体只存储学生，此字段始终为 false）
    qint64 lastHeartbeat; // 最后一次心跳时间戳
    QString className;    // 缓存心跳包中的班级信息
};

// 定义全局静态变量，供 onUdpReadyRead 和 broadcastUserList 共用，避免局部变量冲突
// 该 map 用于存储所有当前在线学生（包括通过 TCP 补录的临时记录），键为学生 ID
static QMap<QString, UserInfo> g_onlineUsers;

// ==================== 常量定义 ====================

/// 教师端 UDP 监听端口（用于接收学生心跳和广播用户列表）
const quint16 BASE_UDP_PORT = 9999;
/// 学生端 UDP 监听端口（教师端广播的目标端口）
const quint64 STUDENT_LISTEN_PORT = 8889;

// ==================== ServerWindow 类实现 ====================

/**
 * @brief ServerWindow 构造函数
 * @param parent 父窗口指针
 *
 * 初始化教师端主窗口：
 * - 设置窗口标题和初始大小
 * - 获取本机 IP 地址
 * - 初始化数据库
 * - 从数据库预加载所有学生信息到 g_onlineUsers
 * - 初始化 UI 布局（左侧导航栏、右侧堆叠窗口）
 * - 创建 FileServer 对象并启动 TCP 服务器
 * - 启动 UDP 发现机制（绑定端口、连接信号）
 * - 启动教师端文件服务器（处理学生文件请求）
 * - 启动心跳广播定时器
 * - 立即广播一次用户列表（包含班级列表）
 * - 初始化班级筛选下拉框并刷新监控表格
 */
ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent)
      ,
      m_udpSocket(new QUdpSocket(this)) // 创建 UDP 套接字，用于学生发现
      ,
      m_heartbeatTimer(new QTimer(this)) // 创建心跳定时器
      ,
      m_fileServer(new QTcpServer(this)) // 创建教师端文件服务器
{
    // 设置窗口标题
    this->setWindowTitle("教室监控管理系统 - 教师端");
    // 设置窗口初始大小为 1200x800
    this->resize(1200, 800);

    // ---------- 获取本机 IP 地址 ----------
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (const QHostAddress &addr : list)
    {
        // 选择第一个非回环的 IPv4 地址（protocol()：返回该地址的协议类型）
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
        {
            m_myIp = addr.toString();
            qDebug() << "教师端IP："<<m_myIp;
            break;
        }
    }
    // 如果未找到有效 IP，使用本地回环地址
    if (m_myIp.isEmpty())
        m_myIp = "127.0.0.1";

    // 设置教师端 UDP 端口为固定值 9999，TCP 端口随机生成（范围 20000~29999）
    m_myUdpPort = BASE_UDP_PORT;
    m_myTcpPort = 20000 + QRandomGenerator::global()->bounded(10000);

    // ---------- 初始化数据库 ----------
    if (!DatabaseManager::instance().init())
    {
        // 数据库初始化失败时弹出严重错误对话框，但程序继续运行（可能部分功能不可用）
        QMessageBox::critical(this, "错误", "数据库初始化失败！");
    }

    // 启动前从数据库预加载所有学生信息到 g_onlineUsers
    // 目的：确保即使学生当前未发心跳，表格也能显示其历史班级和名称，避免显示“未知”
    QMap<QString, StudentInfo> allStudents = DatabaseManager::instance().getAllStudents();
    for (auto it = allStudents.begin(); it != allStudents.end(); ++it)
    {
        UserInfo user;
        user.id = it.key();             // 学生 ID
        user.nickName = it->name;       // 学生姓名
        user.ip = it->ip;               // IP（可能为空）
        user.className = it->className; // 班级（可能为空）
        user.isTeacher = false;         // 不是教师
        user.port = 0;                  // UDP 端口未知（尚未收到心跳）
        user.tcpPort = 0;               // TCP 端口未知
        user.lastHeartbeat = 0;         // 标记为离线/未知状态
        g_onlineUsers[it.key()] = user; // 存入全局 map
    }
    qDebug() << "[Init] 预计从数据库中加载" << allStudents.size() << "名学生！";

    // ---------- 初始化 UI 布局 ----------
    setupMainLayout();   // 创建左侧导航栏和右侧堆叠窗口
    setupMonitorPage();  // 初始化“实时监控”页面
    setupFilePage();     // 初始化“文件传输”页面
    setupSettingsPage(); // 初始化“系统设置”页面

    // ---------- 启动 UDP 发现机制 ----------
    // 绑定 UDP 套接字到任意地址，端口为 m_myUdpPort（9999）
    if (!m_udpSocket->bind(QHostAddress::Any, m_myUdpPort))
    {
        onLogMessage(QString("UDP 绑定失败，端口：%1").arg(m_myUdpPort));
    }
    else
    {
        onLogMessage(QString("教师端 UDP 监听端口：%1").arg(m_myUdpPort));
        // 连接 UDP 数据就绪信号到槽函数
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &ServerWindow::onUdpReadyRead);
    }

    // ---------- 启动教师端文件服务器（P2P）----------
    // 用于响应学生的文件列表和下载请求
    if (!m_fileServer->listen(QHostAddress::Any, m_myTcpPort))
    {
        onLogMessage(QString("教师端文件服务器启动失败：%1").arg(m_fileServer->errorString()));
    }
    else
    {
        onLogMessage(QString("教师端文件共享已开启，监听端口：%1").arg(m_myTcpPort));
        // 有新连接时触发 onNewFileConnection 槽函数
        connect(m_fileServer, &QTcpServer::newConnection, this, &ServerWindow::onNewFileConnection);
    }

    // ---------- 启动心跳广播定时器 ----------
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ServerWindow::sendServerHeartbeat);
    m_heartbeatTimer->start(5000); // 每 5 秒广播一次
    sendServerHeartbeat();         // 立即发送一次，加快学生发现速度

    // 启动后立即主动广播班级列表，确保学生端能第一时间获取到已有班级
    // 防止因定时器未触发或丢包导致学生端下拉框初始为空
    broadcastUserList();

    onLogMessage("系统就绪，等待学生上线...");

    // 初始化班级筛选状态为“全部班级”，防止默认值为空字符串导致逻辑歧义
    m_currentFilterClass = "全部班级";
    m_currentFilterStatus = "全部";//过滤在线状态初始化为"全部"

    //初始化所有学生在线状态为"离线"
    QSqlQuery resetQuery("UPDATE students SET isOnline = 0");
    if(!resetQuery.exec())
    {
        qDebug() << "[Init] 重置在线状态失败：" <<resetQuery.lastError().text();
    }
    else
    {
        qDebug() << "[Init] 已将所有学生在线状态重置为离线";
    }

    // 初始加载所有学生，并传入明确的筛选条件“全部班级”
    refreshMonitorTable("全部班级");

    //加载保存的共享文件夹路径
    QSettings settings("XUPT","TeachMonitor_teacher");
    QString savedSharePath = settings.value("sharePath","E:/fileShared").toString();
    if(m_sharePathEdit)
    {
        m_sharePathEdit->setText(savedSharePath);
    }
    onLogMessage(QString("[配置] 共享文件夹路径已加载：%1").arg(savedSharePath));

    //启动摄像头服务器
    m_cameraServer = new QTcpServer(this);
    connect(m_cameraServer,&QTcpServer::newConnection,this,&ServerWindow::onNewCameraConnection);
}

/**
 * @brief 初始化主布局（左侧导航栏 + 右侧堆叠窗口）
 *
 * 创建中央控件，水平布局：
 * - 左侧导航栏：深色背景，包含 logo 和三个可切换按钮（实时监控、文件传输、系统设置）
 * - 右侧内容区：QStackedWidget，包含三个页面（由 setupMonitorPage、setupFilePage、setupSettingsPage 创建）
 */
void ServerWindow::setupMainLayout()
{
    // 创建中央控件并设置为主窗口的中央控件
    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    // 主布局：水平布局，无边距
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    //-------------左侧导航栏-------------
    QWidget *navWidget = new QWidget();
    navWidget->setFixedWidth(200);                                   // 固定宽度200像素
    navWidget->setStyleSheet("QWidget {background-color:#2c3e50;}"); // 深蓝色背景
    QVBoxLayout *navLayout = new QVBoxLayout(navWidget);
    navLayout->setSpacing(20);
    navLayout->setContentsMargins(10, 20, 10, 20);

    // 顶部logo
    QLabel *titleLogo = new QLabel("控制中心");
    titleLogo->setStyleSheet("color: white;font-size:20px;font-weight:bold;margin-bottom:20px;");
    titleLogo->setAlignment(Qt::AlignCenter);
    navLayout->addWidget(titleLogo);

    // 导航按钮（实时监控，文件传输，系统设置）
    btnMonitor = new QPushButton("🖥️ 实时监控");
    btnFile = new QPushButton("📁 文件传输");
    btnSettings = new QPushButton("⚙️ 系统设置");

    QVector<QPushButton *> navButtons = {btnMonitor, btnFile, btnSettings};
    for (QPushButton *btn : navButtons)
    {
        // 统一设置按钮样式：透明背景，白色文字，左对齐，圆角
        btn->setStyleSheet(R"(
            QPushButton {
                background-color : transparent;
                color : #ecf0f1;
                text-align : left;
                padding : 15px;
                font-size : 16px;
                border-radius : 5px;
            }
            QPushButton:hover { background-color : #34495e;}
            QPushButton:checked {background-color : #3498db;}
        )");
        btn->setCheckable(true); // 可选中状态
        navLayout->addWidget(btn);
    }
    // 默认选中实时监控按钮
    btnMonitor->setChecked(true);

    // 连接按钮点击信号到导航切换槽函数
    connect(btnMonitor, &QPushButton::clicked, this, &ServerWindow::onNavMonitorClicked);
    connect(btnFile, &QPushButton::clicked, this, &ServerWindow::onNavFileClicked);
    connect(btnSettings, &QPushButton::clicked, this, &ServerWindow::onNavSettingsClicked);

    // 添加弹簧，将按钮推至顶部
    navLayout->addStretch();

    //------------右侧内容区-------------
    m_stackedWidget = new QStackedWidget(); // 堆叠窗口，用于切换不同页面

    // 将左侧导航栏和右侧堆叠窗口加入主布局
    mainLayout->addWidget(navWidget);
    mainLayout->addWidget(m_stackedWidget);
}

/**
 * @brief 初始化“实时监控”页面
 *
 * 包含：
 * - 顶部统计栏：班级筛选下拉框、刷新班级视图按钮、在线/违规统计标签
 * - 监控表格（7列）：学生 IP、姓名、班级、状态、违规应用、违规次数、查看屏幕
 * - 底部详情区：截图预览和详情文本
 */
void ServerWindow::setupMonitorPage()
{
    // 创建页面容器
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page); // 垂直布局

    // ---------- 顶部统计栏（水平布局） ----------
    QHBoxLayout *topBar = new QHBoxLayout();

    // 班级筛选下拉框
    QLabel *filterLabel = new QLabel("查看班级:");
    filterLabel->setFont(QFont("Arial", 12, QFont::Bold));
    topBar->addWidget(filterLabel);

    m_classFilterCombo = new QComboBox();
    m_classFilterCombo->setMinimumWidth(150);
    m_classFilterCombo->addItem("全部班级"); // 默认选项

    // 从数据库加载已有班级，添加到下拉框
    QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
    for (const auto &cls : classes)
    {
        m_classFilterCombo->addItem(cls.className);
    }
    // 当下拉框选择变化时，触发 onClassComboBoxChanged 槽函数,显式指定信号参数类型，从而从重载集合中选出正确的信号
    connect(m_classFilterCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &ServerWindow::onClassComboBoxChanged);
    topBar->addWidget(m_classFilterCombo);

    // 确保初始状态选中“全部班级”
    m_classFilterCombo->setCurrentIndex(0);

    // 查看班级按钮（也可通过下拉框直接触发，此处保留按钮作为快捷操作或刷新）
    btnViewClass = new QPushButton("刷新班级视图");
    btnViewClass->setStyleSheet("background-color: #3498db; color: white; padding: 5px 10px; border-radius: 4px;");
    connect(btnViewClass, &QPushButton::clicked, this, &ServerWindow::onViewClassMembersClicked);
    topBar->addWidget(btnViewClass);

    // 弹簧
    topBar->addStretch();

    //过滤状态
    QLabel *statusLabel = new QLabel("过滤状态:");
    statusLabel->setFont(QFont("Arial",12,QFont::Bold));
    topBar->addWidget(statusLabel);

    m_statusFilterCombo = new QComboBox();
    m_statusFilterCombo->setMinimumWidth(150);
    m_statusFilterCombo->addItem("全部");//默认
    m_statusFilterCombo->addItem("在线");
    m_statusFilterCombo->addItem("离线");
    topBar->addWidget(m_statusFilterCombo);
    //当状态过滤下拉框选择变化时，触发onStatusComboBoxChanged 槽函数
    connect(m_statusFilterCombo,QOverload<const QString& >::of(&QComboBox::currentTextChanged),
            this,&ServerWindow::onStatusComboBoxChanged);

    // 弹簧，将后续控件推到右侧
    topBar->addStretch();

    //当前人数/违规警告统计标签（初始值，实际数值会动态更新）
    QLabel *statLabel = new QLabel("当前人数：0 | 违规警告：0");
    statLabel->setFont(QFont("Arial", 12, QFont::Bold));
    topBar->addWidget(statLabel);

    layout->addLayout(topBar);

    // ---------- 监控表格 ----------
    m_monitorTable = new QTableWidget();
    // 设置列数为 7 列，列标题依次为：学生 IP、姓名、班级、状态、违规应用、违规次数、查看屏幕
    m_monitorTable->setColumnCount(8);
    m_monitorTable->setHorizontalHeaderLabels({"学生 IP", "姓名", "班级", "状态", "违规应用", "违规次数", "查看屏幕","查看摄像头"});
    // 最后一列自动拉伸填充剩余空间
    m_monitorTable->horizontalHeader()->setStretchLastSection(true);
    // 整行选中
    m_monitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    // 禁止编辑
    m_monitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 隐藏行号列（垂直表头）
    m_monitorTable->verticalHeader()->setVisible(false);
    // 交替行背景色
    m_monitorTable->setAlternatingRowColors(true);

    // 调整各列宽度，确保所有列内容可见
    m_monitorTable->setColumnWidth(0, 130); // IP
    m_monitorTable->setColumnWidth(1, 100); // 姓名
    m_monitorTable->setColumnWidth(2, 120); // 班级
    m_monitorTable->setColumnWidth(3, 80);  // 状态
    m_monitorTable->setColumnWidth(4, 150); // 违规应用
    m_monitorTable->setColumnWidth(5, 80);  // 违规次数
    m_monitorTable->setColumnWidth(6, 100); // 查看屏幕
    m_monitorTable->setColumnWidth(7, 100); // 查看摄像头

    // 连接表格双击信号到 onTableCellDoubleClicked 槽函数
    connect(m_monitorTable, &QTableWidget::cellDoubleClicked, this, &ServerWindow::onTableCellDoubleClicked);

    layout->addWidget(m_monitorTable);

    // 将当前页面添加到堆叠窗口（索引 0）
    m_stackedWidget->addWidget(page);
}

/**
 * @brief 初始化“文件传输”页面
 *
 * 当前页面主要用于显示日志信息（文件传输记录），并复用原有的文件列表逻辑。
 * 实际文件浏览和下载功能已集成在教师端的 onNewFileConnection 和 handlePeerCommand 中。
 */
void ServerWindow::setupFilePage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    // 提示标签
    QLabel *label = new QLabel("文件传输记录与管理界面");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    // 日志窗口（只读，用于显示文件传输日志）
    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Consolas", 10));
    layout->addWidget(m_logView);

    // 将页面添加到堆叠窗口
    m_stackedWidget->addWidget(page);
}

/**
 * @brief 初始化“系统设置”页面
 *
 * 包含：
 * - 共享文件夹路径设置（浏览按钮、提示）
 * - 班级管理区域：创建班级（输入框+按钮）、删除班级（下拉框+按钮）
 * - 提示信息
 */
void ServerWindow::setupSettingsPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    //---------------共享文件夹路径设置--------------
    QLabel *lbl = new QLabel("共享文件夹路径设置");
    lbl->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    layout->addWidget(lbl);

    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_sharePathEdit = new QLineEdit("E:/fileShared"); // 默认路径
    m_sharePathEdit->setPlaceholderText("请选择共享文件夹根目录");
    m_sharePathEdit->setReadOnly(true);
    pathLayout->addWidget(m_sharePathEdit);

    QPushButton *btnBrowse = new QPushButton("浏览...");
    btnBrowse->setStyleSheet("background-color:#3498db;color : white;padding : 5px 15px;border-radius:4px;");

    // 浏览按钮点击响应：弹出目录选择对话框，更新路径并记录日志
    connect(btnBrowse, &QPushButton::clicked, this, [this]()
            {
        QString currentPath = m_sharePathEdit->text();
        if(!QDir(currentPath).exists())
        {
            currentPath = QDir::homePath();//如果当前路径不存在，回退到用户主目录
        }
        QString selectedDir = QFileDialog::getExistingDirectory(this,"选择共享文件夹根目录",currentPath);

        if(!selectedDir.isEmpty())
        {
            m_sharePathEdit->setText(selectedDir);
            //保存到配置文件
            QSettings settings("XUPT","TeachMonitor_teacher");
            settings.setValue("sharePath",selectedDir);
            onLogMessage(QString("[设置] 共享文件夹路径已更新为:%1").arg(selectedDir));

            QDir dir(selectedDir);
            if(!dir.exists())
            {
                //如果目录不存在，询问是否创建
                if(QMessageBox::question(this,"提示","该目录不存在，是否立即创建？") == QMessageBox::Yes)
                {
                    if(dir.mkpath("."))
                    {
                        onLogMessage("[系统] 已成功创建共享目录");
                    }
                    else
                    {
                        QMessageBox::warning(this,"错误","创建目录失败,请检查权限！");
                    }
                }
            }
        } });
    pathLayout->addWidget(btnBrowse);
    layout->addLayout(pathLayout);

    QLabel *hintLabel = new QLabel("提示：修改后即时生效，无需重启服务器。请确保该目录有读写权限。");
    hintLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 10px;");
    hintLabel->setWordWrap(true); // 自动换行
    layout->addWidget(hintLabel);

    layout->addSpacing(30);

    //-------------班级管理区域--------------
    QLabel *classLbl = new QLabel("班级管理");
    classLbl->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    layout->addWidget(classLbl);

    // 创建班级区域（水平布局）
    QHBoxLayout *classLayout = new QHBoxLayout();
    m_newClassNameEdit = new QLineEdit();
    m_newClassNameEdit->setPlaceholderText("输入新班级名称（例如：三年二班）");
    m_newClassNameEdit->setMinimumWidth(200);
    classLayout->addWidget(m_newClassNameEdit);

    btnCreateClass = new QPushButton("创建班级");
    btnCreateClass->setStyleSheet("background-color: #27ae60; color: white; padding: 5px 15px; border-radius: 4px;");
    connect(btnCreateClass, &QPushButton::clicked, this, &ServerWindow::onCreateClassClicked);
    classLayout->addWidget(btnCreateClass);
    classLayout->addStretch(); // 弹簧，将按钮推到左侧

    layout->addLayout(classLayout);

    QLabel *classHint = new QLabel("提示：创建班级后，学生端可在设置中加入该班级。");
    classHint->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 5px;");
    layout->addWidget(classHint);

    layout->addSpacing(20);

    // ---------- 删除班级区域 ----------
    QLabel *deleteLbl = new QLabel("删除班级：");
    deleteLbl->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    layout->addWidget(deleteLbl);

    QHBoxLayout *deleteLayout = new QHBoxLayout();
    m_deleteClassCombo = new QComboBox();
    m_deleteClassCombo->setPlaceholderText("选择要删除的班级");
    m_deleteClassCombo->setMinimumWidth(200);
    // 初始化加载现有班级
    QList<ClassInfo> initClasses = DatabaseManager::instance().getAllClasses();
    for (const auto &cls : initClasses)
    {
        m_deleteClassCombo->addItem(cls.className);
    }
    deleteLayout->addWidget(m_deleteClassCombo);

    btnDeleteClass = new QPushButton("删除选中班级");
    btnDeleteClass->setStyleSheet("background-color:#e74c3c;color:white;padding:5px 15px;border-radius:4px;");
    connect(btnDeleteClass, &QPushButton::clicked, this, &ServerWindow::onDeleteClassClicked);
    deleteLayout->addWidget(btnDeleteClass);
    deleteLayout->addStretch();

    layout->addLayout(deleteLayout);

    QLabel *deleteHint = new QLabel("提示：删除班级后，原属于该班级的学生将变为“未分班”状态。");
    deleteHint->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 5px;");
    layout->addWidget(deleteHint);

    layout->addStretch();             // 底部弹簧，将内容推到顶部
    m_stackedWidget->addWidget(page); // 页面索引2
}

/**
 * @brief 切换到“实时监控”页面
 *
 * 更新导航栏按钮选中状态，设置堆叠窗口当前索引为 0，并刷新监控表格（保持当前班级筛选）。
 */
void ServerWindow::onNavMonitorClicked()
{
    btnMonitor->setChecked(true);
    btnFile->setChecked(false);
    btnSettings->setChecked(false);
    m_stackedWidget->setCurrentIndex(0);                    // 切换到监控页面
    refreshMonitorTable(m_classFilterCombo->currentText()); // 刷新表格（保持当前筛选）
}

/**
 * @brief 切换到“文件传输”页面
 */
void ServerWindow::onNavFileClicked()
{
    btnMonitor->setChecked(false);
    btnFile->setChecked(true);
    btnSettings->setChecked(false);
    m_stackedWidget->setCurrentIndex(1); // 切换到日志页面
}

/**
 * @brief 切换到“系统设置”页面
 */
void ServerWindow::onNavSettingsClicked()
{
    btnMonitor->setChecked(false);
    btnFile->setChecked(false);
    btnSettings->setChecked(true);
    m_stackedWidget->setCurrentIndex(2); // 切换到设置页面
}

/**
 * @brief 接收日志消息并在界面上显示
 * @param msg 日志文本
 *
 * 将消息附加到文件传输页面的 m_logView 中，并自动滚动到底部。
 */
void ServerWindow::onLogMessage(const QString &msg)
{
    if (m_logView)
    {
        QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        m_logView->appendPlainText(QString("[%1] %2").arg(timeStr).arg(msg));
        // 自动滚动到底部
        m_logView->verticalScrollBar()->setValue(m_logView->verticalScrollBar()->maximum());
    }
}

/**
 * @brief 更新或添加监控表格中的一行
 * @param id 学生 ID
 * @param name 学生姓名/昵称
 * @param status 学生状态
 * @param app 违规应用名称（若无违规则为空）
 *
 * 根据当前筛选条件（m_currentFilterClass）决定是否显示该学生，
 * 若通过筛选则更新表格的 7 列：IP、姓名、班级、状态、违规应用、违规次数、查看屏幕。
 */
void ServerWindow::updateStudentTableRow(const QString &id, const QString &name, StudentStatus status, const QString &app)
{
    // 打印入口参数
    qDebug() << "[更新学生表行] ENTER -> id:" << id << "姓名:" << name << "状态:" << (int)status << "违规应用:" << app
             << "当前筛选班级:" << m_currentFilterClass;

    //状态过滤检查
    //判断当前学生状态是否符合筛选条件
    bool statusMatch = true;
    if(!m_currentFilterStatus.isEmpty() && m_currentFilterStatus != "全部")
    {
        bool needOnline = (m_currentFilterStatus == "在线");
        bool studentOnline = (status != StudentStatus::Offline);
        if(studentOnline != needOnline)
        {
            statusMatch = false;
        }
    }

    //如果状态不匹配，且该学生已在表格中，则需要移除该行
    if(!statusMatch && m_studentRowMap.contains(id))
    {
        int row = m_studentRowMap.take(id);
        m_monitorTable->removeRow(row);
        //调整后续行索引
        for(auto it = m_studentRowMap.begin();it != m_studentRowMap.end();++it)
        {
            if(it.value() > row)
            {
                it.value()--;
            }
        }
        qDebug() << "[状态过滤] 移除不符合状态筛选的学生行：" << id << "行号："<<row;
        return;//直接返回，不执行后续的添加/更新逻辑
    }

    //如果状态不匹配且该学生不在表格中，则直接返回（不添加）
    if(!statusMatch)
    {
        qDebug() <<"[状态过滤] 忽略不符合状态筛选的学生:"<<id;
        return;
    }

    // 1.获取学生当前最新的班级信息
    QString studentClass = "未分班";
    if (g_onlineUsers.contains(id) && !g_onlineUsers[id].className.isEmpty())
    {
        studentClass = g_onlineUsers[id].className;
    }
    else
    {
        StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
        if (!info.className.isEmpty())
        {
            studentClass = info.className;
        }
    }

    // 2.获取学生IP地址
    QString studentIp;
    if (g_onlineUsers.contains(id) && !g_onlineUsers[id].ip.isEmpty())
    {
        studentIp = g_onlineUsers[id].ip;
    }
    else
    {
        StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
        if (!info.ip.isEmpty())
        {
            studentIp = info.ip;
        }
    }

    if (studentIp.isEmpty())
    {
        studentIp = "未知 IP";
    }

    qDebug() << "[更新学生表行] 已解决 -> IP:" << studentIp << "Class:" << studentClass;

    // 3.筛选检查：只有当前筛选条件为“全部班级”或学生班级与筛选条件匹配时，才显示该行
    if (!m_currentFilterClass.isEmpty() && m_currentFilterClass != "全部班级" && studentClass != m_currentFilterClass)
    {
        qDebug() << "不匹配！ 学生班级为：('" << studentClass << "') != Filter ('" << m_currentFilterClass << "').移除或者隐藏id=" << id << "的行";

        // 如果改行已存在，则删除它
        if (m_studentRowMap.contains(id))
        {
            int row = m_studentRowMap.take(id);
            m_monitorTable->removeRow(row);
            // 调整后续行索引(所有大于当前行的索引减一)
            for (auto it = m_studentRowMap.begin(); it != m_studentRowMap.end(); ++it)
            {
                if (it.value() > row)
                    it.value()--;
            }
            qDebug() << "[过滤] 移除学生行" << row << "，姓名：" << name;
        }
        else
        {
            qDebug() << "[过滤] 忽略改行：" << name;
        }
        return;
    }
    qDebug() << "[过滤] 通过！正在继续添加更新行" << id;

    // 4.添加或更新行
    int row = -1;
    if (m_studentRowMap.contains(id))
    {
        // 更新已有行
        row = m_studentRowMap[id];
        qDebug() << "[UI] 更新存在行" << row << ",id:" << id;

        // 更新姓名列(第一列)
        QTableWidgetItem *nameItem = m_monitorTable->item(row, 1);
        if (nameItem && nameItem->text() != name)
        {
            nameItem->setText(name);
        }

        // 更新IP列(第0列)
        QTableWidgetItem *ipItem = m_monitorTable->item(row, 0);
        if (ipItem && ipItem->text() != studentIp)
        {
            ipItem->setText(studentIp);
        }

        // 更新班级列(第二列)
        QTableWidgetItem *classItem = m_monitorTable->item(row, 2);
        if (classItem && classItem->text() != studentClass)
        {
            classItem->setText(studentClass);
        }
    }
    else // 如果不存在则创建新行
    {
        row = m_monitorTable->rowCount();
        m_monitorTable->insertRow(row);
        m_studentRowMap[id] = row;
        qDebug() << "[UI] 创建新行" << row << "，id:" << id;

        // 第0列：显示IP，UserRole存ID（后面双击获取学生id）
        QTableWidgetItem *ipItem = new QTableWidgetItem(studentIp);
        ipItem->setData(Qt::UserRole, id);
        m_monitorTable->setItem(row, 0, ipItem);

        // 第1列：姓名
        m_monitorTable->setItem(row, 1, new QTableWidgetItem(name));
        // 第2列：班级
        m_monitorTable->setItem(row, 2, new QTableWidgetItem(studentClass));
    }

    // 第3列：状态(使用枚举设置文本，背景色，前景色)
    QTableWidgetItem *statusItem = new QTableWidgetItem();
    QTableWidgetItem *appItem = new QTableWidgetItem();

    switch (status)
    {
    case StudentStatus::Offline:
        statusItem->setText("离线");
        statusItem->setBackground(QBrush(Qt::gray));
        statusItem->setForeground(QBrush(Qt::white));
        appItem->setText("-");
        break;
    case StudentStatus::Online_Normal:
        statusItem->setText("正常");
        statusItem->setBackground(QBrush(Qt::green));
        statusItem->setForeground(QBrush(Qt::white));
        appItem->setText("-");
        break;
    case StudentStatus::Online_Violated:
        statusItem->setText("违规");
        statusItem->setBackground(QBrush(Qt::red));
        statusItem->setForeground(QBrush(Qt::white));
        appItem->setText(app);
        break;
    default:
        statusItem->setText("未知");
        appItem->setText("-");
        break;
    }

    m_monitorTable->setItem(row, 3, statusItem);
    m_monitorTable->setItem(row, 4, appItem);

    // 第5列：违规次数（从数据库查询）
    int count = DatabaseManager::instance().getViolationCount(id);
    QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(count));
    countItem->setTextAlignment(Qt::AlignCenter);
    if (count > 0)
    {
        countItem->setForeground(QBrush(Qt::red)); // 次数大于0时显示红色
        QFont font = countItem->font();
        font.setBold(true);
        countItem->setFont(font);
    }
    m_monitorTable->setItem(row, 5, countItem);

    // 第6列：查看屏幕(可点击)
    QTableWidgetItem *actionItem = new QTableWidgetItem("🔍 查看屏幕");
    QColor bgColor("#e3f2fd");   // 浅蓝色背景
    QColor textColor("#0d4731"); // 深蓝色文字
    actionItem->setBackground(QBrush(bgColor));
    actionItem->setForeground(QBrush(textColor));
    QFont actionFont = actionItem->font();
    actionFont.setBold(true);
    actionFont.setUnderline(true);
    actionFont.setPointSize(actionFont.pointSize() + 1);
    actionItem->setFont(actionFont);
    actionItem->setTextAlignment(Qt::AlignCenter);
    actionItem->setToolTip("点击此按钮查看实时屏幕");
    m_monitorTable->setItem(row, 6, actionItem);

    // 第7列：查看屏幕(可点击)
    QTableWidgetItem *cameraItem = new QTableWidgetItem("📷 摄像头");
    cameraItem->setBackground(QBrush(QColor("#e3f2fd")));
    cameraItem->setForeground(QBrush(QColor("#0d4731")));
    QFont cameraFont = cameraItem->font();
    cameraFont.setBold(true);
    cameraFont.setUnderline(true);
    cameraFont.setPointSize(cameraFont.pointSize() + 1);
    cameraItem->setFont(cameraFont);
    cameraItem->setTextAlignment(Qt::AlignCenter);
    cameraItem->setToolTip("双击查看实时摄像头画面");
    m_monitorTable->setItem(row, 7, cameraItem);

    qDebug() << "[更新学生表行] 成功.行号：" << row << ",id:" << id << ",班级：" << studentClass;
}

/**
 * @brief 刷新监控表格，支持按班级过滤
 * @param filterClass 班级名称或空字符串（空字符串视为“全部班级”）
 *
 * 从数据库获取所有学生，结合 g_onlineUsers 的在线状态和班级信息，
 * 根据筛选条件过滤后调用 updateStudentTableRow 重建表格。
 */
void ServerWindow::refreshMonitorTable(const QString &filterClass)
{
    // 处理空字符串情况，视为"全部班级"
    QString effectiveFilter = filterClass;
    if (effectiveFilter.isEmpty())
    {
        effectiveFilter = "全部班级";
    }
    m_currentFilterClass = effectiveFilter;

    qDebug() << "[刷新] 刷新学生表，当前班级：" << m_currentFilterClass;

    // 清空表格和行映射
    m_monitorTable->setRowCount(0);
    m_studentRowMap.clear();

    // 第一步：从数据库获取所有学生
    QMap<QString, StudentInfo> dbMap = DatabaseManager::instance().getAllStudents();
    QList<StudentInfo> allStudents = dbMap.values();

    // 第二步：合并在线状态和最新班级信息（从g_onlineUsers覆盖）
    for (auto &stu : allStudents)
    {
        if (g_onlineUsers.contains(stu.id))
        {
            const UserInfo &onlineUser = g_onlineUsers[stu.id];
            stu.isOnline = true;
            // 优先使用心跳包里的最新班级信息
            if (!onlineUser.className.isEmpty())
            {
                stu.className = onlineUser.className;
            }
        }
        else
        {
            stu.isOnline = false;
        }
    }

    qDebug() << "[刷新] 数据库中总共有" << allStudents.size() << "名学生";

    // 第三步：根据筛选条件过滤
    if (!m_currentFilterClass.isEmpty() && m_currentFilterClass != "全部班级")
    {
        QList<StudentInfo> filtered;
        QString targetClass = m_currentFilterClass.trimmed();

        for (const StudentInfo &stu : allStudents)
        {
            // 学生班级不为空 且 等于目标班级
            if (!stu.className.isEmpty() && stu.className.trimmed() == targetClass)
            {
                filtered.append(stu);
            }
            else
            {
                qDebug() << "[刷新] 过滤掉学生：" << stu.name << ",因为该学生班级为：" << stu.className << ",不是目标班级：" << targetClass;
            }
        }

        qDebug() << "[刷新] 目标班级" << targetClass << "共有" << filtered.size() << "名学生";
        allStudents = filtered;
    }
    else
    {
        qDebug() << "[刷新] 筛选的班级名称为”全部班级“ 或者 为空！";
    }

    if(!m_currentFilterStatus.isEmpty() && m_currentFilterStatus != "全部")
    {
        QList<StudentInfo> statusFiltered;
        bool needOnline = (m_currentFilterStatus == "在线");
        for(const StudentInfo& stu : allStudents)
        {
            if(stu.isOnline == needOnline)
            {
                statusFiltered.append(stu);
            }
        }
        allStudents = statusFiltered;
        qDebug() << "[刷新] 状态过滤后剩余" <<allStudents.size() << "名学生";
    }

    // 第四步：更新表格
    for (const StudentInfo &stu : allStudents)
    {
        StudentStatus status = stu.isOnline ? StudentStatus::Online_Normal : StudentStatus::Offline;
        // 注：这里传入空的app，只更新基础信息和状态
        updateStudentTableRow(stu.id, stu.name, status, "");
    }

    updateOnlineStats();
    qDebug() << "[刷新] 学生表刷新完成！";
}

/**
 * @brief 更新界面顶部的当前人数和违规警告数统计
 *
 * 遍历监控表格的所有行，统计非离线状态的学生数量和状态为“违规”或“警告”的学生数量，
 * 并更新顶部统计标签。
 */
void ServerWindow::updateOnlineStats()
{
    QWidget *central = this->centralWidget();
    if (!central)
        return;

    // 查找所有标签控件，找到包含“在线人数”文本的那一个
    QList<QLabel *> labels = central->findChildren<QLabel *>();
    for (QLabel *lbl : labels)
    {
        if (lbl->text().contains("当前人数"))
        {
            int currentRowCount = m_monitorTable->rowCount();
            int warningCount = 0;
            for (int r = 0; r < m_monitorTable->rowCount(); ++r)
            {
                QTableWidgetItem *statusItem = m_monitorTable->item(r, 3); // 状态列
                if (statusItem && statusItem->text() != "离线")
                {
                    if (statusItem->text().contains("违规"))
                    {
                        warningCount++;
                    }
                }
            }
            lbl->setText(QString("当前人数：%1 | 违规警告：%2").arg(currentRowCount).arg(warningCount));
            break;
        }
    }
}

/**
 * @brief 监控表格双击事件处理
 * @param row 行索引
 * @param col 列索引
 *
 * 仅当双击的是第 6 列（“查看屏幕”列）时，才触发请求实时截图。
 * 从第 0 列的 UserRole 中获取学生 ID，然后调用 onRequestLiveScreenshotClicked。
 */
void ServerWindow::onTableCellDoubleClicked(int row, int col)
{
    QTableWidgetItem *ipItem = m_monitorTable->item(row, 0);
    if (!ipItem) return;
    QString studentId = ipItem->data(Qt::UserRole).toString();
    if (studentId.isEmpty()) return;

    if (col == 6) {
        onRequestLiveScreenshotClicked(studentId);
    } else if (col == 7) {
        onRequestCameraStream(studentId);//查看摄像头
    }
}

/**
 * @brief 显示截图弹窗
 * @param studentName 学生姓名（用于窗口标题）
 * @param imageData 截图二进制数据（JPEG 格式）
 *
 * 创建一个模态对话框，展示缩放后的截图，供教师详细查看。
 * 窗口大小根据主屏幕尺寸动态计算（宽高各占 2/3）。
 */
void ServerWindow::showScreenshotDialog(const QString &studentName, const QByteArray &imageData)
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(QString("屏幕截图 - %1").arg(studentName));

    // 获取主屏幕几何信息，计算对话框大小（宽高各占当前屏幕2/3）
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    int width = screenRect.width() * 2 / 3;
    int height = screenRect.height() * 2 / 3;

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    // 信息标签：学生姓名、时间
    QLabel *infoLabel = new QLabel(QString("<b>学生：</b>%1<br><b>时间：</b>%2").arg(studentName, QTime::currentTime().toString("hh:mm:ss")));
    infoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(infoLabel);

    // 图片标签
    QLabel *imgLabel = new QLabel();
    imgLabel->setFixedSize(width, height);
    imgLabel->setAlignment(Qt::AlignCenter);
    imgLabel->setStyleSheet("border:1px solid #ccc;background: #fff;");

    QPixmap pix;
    if (pix.loadFromData(imageData))
    {
        // 缩放图片以适配标签大小
        imgLabel->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else
    {
        imgLabel->setText("图片数据损坏或无法解析");
        imgLabel->setAlignment(Qt::AlignCenter);
        imgLabel->setStyleSheet("color:red;font-weight:bold;");
    }

    layout->addWidget(imgLabel);

    dlg->resize(width + 20, height + 80);
    dlg->exec();        // 模态显示
    dlg->deleteLater(); // 关闭后自动删除
}

/**
 * @brief UDP 套接字数据到达时的槽函数（接收学生心跳包）
 *
 * 解析 UDP 包：
 * - 如果是 HEARTBEAT 包：更新学生在线信息、班级信息、IP、TCP 端口
 * 同时进行超时清理（超过 10 秒未收到心跳的学生标记为离线）。
 */
void ServerWindow::onUdpReadyRead()
{
    bool userListChanged = false; // 标记是否有新用户加入或用户下线，用于更新统计

    while (m_udpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderIp;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);

        // 忽略自己发出的广播(防止回环)
        if (senderIp.toString() == m_myIp && senderPort == m_myUdpPort)
            continue;

        QString content = QString::fromUtf8(datagram);
        QStringList parts = content.split('|');

        //------------------处理HEARTBEAT包-----------------
        //心跳包格式：HEARTBEAT|昵称|是否教师|IP|UDP端口|TCP端口|班级|学生ID
        if (parts.size() >= 6 && parts[0] == "HEARTBEAT")
        {
            QString nick = parts[1];
            bool isTeacher = (parts[2] == "1");

            if (isTeacher)
                continue;

            // 获取IP并增加容错逻辑：如果保重IP无效，使用UDP发送方IP
            QString ip = parts[3];
            if (ip.isEmpty() || ip == "127.0.0.1" || ip.startsWith("127."))
            {
                qDebug() << "[UDP]  心跳IP无效,ip:" << ip << "使用UDP发送者IP：" << senderIp.toString();
                ip = senderIp.toString();
            }

            quint16 uPort = parts[4].toUShort(); // 学生端UDP端口
            quint16 tPort = parts[5].toUShort(); // 学生端TCP端口

            if (tPort == 0)
            {
                qWarning() << "[UDP]  从" << nick << "接收到TCP端口为0,ip:" << ip;
            }
            else
            {
                qDebug() << "[UDP]  从" << nick << "获得有效的心跳包，ip:" << ip << "TCP端口：" << tPort;
            }

            // 解析班级信息（心跳包第7个字段）
            QString className = "";
            if (parts.size() >= 7)
            {
                className = parts[6].trimmed();
                if (className == "请选择班级..." || className.isEmpty())
                {
                    className = "";
                }
                qDebug() << "[心跳] 学生姓名：" << nick << ",ip:" << ip << ",班级:" << parts[6];
            }
            else
            {
                qDebug() << "[心跳] 学生姓名：" << nick << ",心跳丢失！";
            }

            // 获取固定学生ID（心跳包第8个字段）
            QString studentId = "";
            if (parts.size() >= 8)
            {
                studentId = parts[7].trimmed();
                qDebug() << "[心跳] 接收到固定学生ID：" << studentId;
            }

            if (studentId.isEmpty())
            {
                studentId = QString("%1:%2").arg(ip).arg(uPort);
                qDebug() << "[心跳] 没有固定ID，使用备用ID：" << studentId;
            }

            // 构建UserInfo对象
            UserInfo user;
            user.ip = ip;
            user.port = uPort;
            user.tcpPort = tPort;
            user.id = studentId;
            user.nickName = nick;
            user.isTeacher = isTeacher;
            user.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();
            user.className = className;

            bool isNewUser = !g_onlineUsers.contains(studentId);

            // 打印旧班级信息对比
            if (!isNewUser)
            {
                QString oldClass = g_onlineUsers[studentId].className;
                QString oldNick = g_onlineUsers[studentId].nickName;
                QString oldIp = g_onlineUsers[studentId].ip;

                if (oldClass != className || oldNick != nick || oldIp != ip)
                {
                    qDebug() << "[同步] 学生id:" << studentId << ",信息变化：姓名：" << oldNick << "->" << nick
                             << ", 班级:" << oldClass << "->" << className
                             << ", IP:" << oldIp << "->" << ip;
                }
            }
            else
            {
                userListChanged = true;
                qDebug() << "[同步] 发现新学生：" << studentId << ",姓名：" << nick << ",班级：" << className;
            }

            // 更新全局map
            g_onlineUsers[studentId] = user;

            QString displayName = nick;

            // 同步到数据库
            StudentInfo info = DatabaseManager::instance().getStudentInfo(studentId);
            DatabaseManager::instance().registerStudent(studentId, displayName, ip);

            // 如果心跳包带了班级，且与数据库中记录不同，则立即更新数据库
            if (!className.isEmpty() && info.className != className)
            {
                bool updateOk = DatabaseManager::instance().updateStudentClass(studentId, className);
            }

            // 无论是否是新用户，只要行存在，就强制更新显示内容（主要是班级和IP）
            if (m_studentRowMap.contains(studentId))
            {
                int row = m_studentRowMap[studentId];

                //获取当前行的状态文本，如果是"违规"则保持违规状态
                QTableWidgetItem* statusItem = m_monitorTable->item(row,3);//状态列
                QString currentStatus = statusItem ? statusItem->text() : "";
                QString currentApp = "";
                StudentStatus newStatus;
                if(currentStatus == "违规")
                {
                    newStatus = StudentStatus::Online_Violated;
                    QTableWidgetItem* appItem = m_monitorTable->item(row,4);
                    currentApp = appItem ? appItem->text() : "";
                }
                else
                {
                    newStatus = StudentStatus::Online_Normal;
                }

                //更新数据库学生在线状态
                DatabaseManager::instance().updateStudentStatus(studentId,true);
                //更新整行，传入当前违规应用名（如果有的话）
                updateStudentTableRow(studentId,displayName,newStatus,currentApp);
                qDebug() <<"[UI] 刷新学生行状态：" <<studentId<<"->"<<currentStatus;

            }
            else
            {
                // 新用户，创建行
                qDebug() << "[UI] 新增学生行：" << studentId << "Class:" << className;
                updateStudentTableRow(studentId, displayName, StudentStatus::Online_Normal, "");
            }
        }
    }

    //-------------超时清理：移除超过10秒未收到心跳的学生---------------
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> toRemove;
    for (auto it = g_onlineUsers.begin(); it != g_onlineUsers.end(); ++it)
    {
        if (now - it->lastHeartbeat > 10000)
        {
            toRemove.append(it.key());
        }
    }

    for (const QString &id : toRemove)
    {
        g_onlineUsers.remove(id);
        //更新数据库中的在线状态为离线
        DatabaseManager::instance().updateStudentStatus(id, false);
        if (m_studentRowMap.contains(id))
        {
            onLogMessage(QString("[监控] 学生 %1 下线（超时），状态更新为离线").arg(id));

            //获取学生姓名
            StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
            QString displayName = info.name.isEmpty() ? id : info.name;
            //更新表格行状态为离线
            updateStudentTableRow(id,displayName,StudentStatus::Offline,"");

            userListChanged = true;
        }
    }

    // 如果只有新用户加入但没有超时下线，仍需要更新统计
    if (userListChanged)
    {
        updateOnlineStats();
    }
}

/**
 * @brief 广播班级列表
 *
 * 将 g_onlineUsers 中的用户信息和数据库中的班级列表打包成 JSON，
 * 通过 UDP 广播发送给学生端（端口 STUDENT_LISTEN_PORT），用于学生端更新好友列表和班级下拉框。
 */
void ServerWindow::broadcastUserList()
{
    // 获取数据库中所有存在的班级，一并广播给学生端
    QJsonArray classList;
    QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();

    for (const auto &cls : classes)
    {
        classList.append(cls.className);
    }

    // 构建包含班级列表的消息格式：USER_LIST|{"classes":[...]}
    QJsonObject rootObj;
    rootObj["classes"] = classList;

    QJsonDocument finalDoc(rootObj);
    QByteArray finalData = finalDoc.toJson(QJsonDocument::Compact);

    // 封装为文本协议：USER_LIST|JSON字符串
    QString msg = QString("USER_LIST|%1").arg(QString(finalData));
    QByteArray data = msg.toUtf8();

    // 发送日志
    qDebug() << "[教师广播] 广播班级列表";
    qint64 sent = m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, STUDENT_LISTEN_PORT);
    if (sent == -1)
    {
        qWarning() << "[教师广播] 广播失败" << m_udpSocket->errorString();
    }
}

/**
 * @brief 定时发送教师端心跳广播
 *
 * 每 5 秒向学生端监听端口（8889）发送广播，告知教师端 IP、UDP 端口、TCP 端口，
 * 以便学生端发现教师端。
 * 同时，在发送心跳时也会清理超时的学生（超过 10 秒未收到心跳）。
 */
void ServerWindow::sendServerHeartbeat()
{
    // 构建心跳包：HEARTBEAT|教师端|1|IP|UDP端口|TCP端口
    QString msg = QString("HEARTBEAT|%1|%2|%3|%4|%5")
                      .arg("教师端")
                      .arg("1") // isTeacher = True
                      .arg(m_myIp)
                      .arg(m_myUdpPort)
                      .arg(m_myTcpPort);

    QByteArray data = msg.toUtf8();
    m_udpSocket->writeDatagram(data, QHostAddress("192.168.14.255"), STUDENT_LISTEN_PORT);

    //广播班级列表
    broadcastUserList();

    // 清理超时学生
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> toRemove;
    for (auto it = g_onlineUsers.begin(); it != g_onlineUsers.end(); ++it)
    {
        if (now - it->lastHeartbeat > 10000)
        {
            toRemove.append(it.key());
        }
    }

    for (const QString &id : toRemove)
    {
        if (g_onlineUsers.contains(id))
        {
            g_onlineUsers.remove(id);
            //更新数据库中的在线状态为离线
            DatabaseManager::instance().updateStudentStatus(id, false);
            if (m_studentRowMap.contains(id))
            {
                onLogMessage(QString("[监控] 学生 %1 下线（超时），状态更新为离线").arg(id));

                StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
                QString displayName = info.name.isEmpty() ? id : info.name;

                updateStudentTableRow(id, displayName, StudentStatus::Offline, "");
            }
        }
    }
    // 更新统计
    updateOnlineStats();
}

/**
 * @brief 处理新的文件服务连接（教师端作为文件服务器时）
 *
 * 当学生端请求文件列表或下载文件时，QTcpServer 会触发此槽函数。
 * 内部建立连接并处理 LIST|、DOWNLOAD| 等命令，以及接收监控数据（MONITOR_START）。
 */
void ServerWindow::onNewFileConnection()
{
    // 获取新连接的套接字
    QTcpSocket *clientSocket = m_fileServer->nextPendingConnection();

    qDebug() << "[fileserver] 来自于" << clientSocket->peerAddress().toString() << "的新连接";

    onLogMessage(QString("[文件服务] 新连接来自：%1").arg(clientSocket->peerAddress().toString()));

    //--------------定义用于接收监控数据的上下文结构-------------
    struct MonitorReceiverContext
    {
        enum State
        {
            WaitHeader,
            WaitJsonLen,
            WaitJsonData,
            WaitImgLen,
            WaitImgData
        } state = WaitHeader;
        quint32 jsonSize = 0;
        quint32 imgSize = 0;
        QByteArray jsonBuffer;
        QByteArray imgBuffer;
        QString pendingStudentId;
    };

    // 创建上下文对象(使用shared_ptr管理生命周期)
    auto ctx = std::make_shared<MonitorReceiverContext>();
    QPointer<QTcpSocket> safeSocket(clientSocket); // 安全指针

    // 连接readyRead信号
    connect(clientSocket, &QTcpSocket::readyRead, this, [this, safeSocket, ctx]()
            {
        if(!safeSocket) return;
        QTcpSocket* socket = safeSocket.data();

    //标签，用于从MONITOR_START 命令处理后跳转到二进制数据接收部分
    process_binary:

        //如果当前状态不是WAitHeader(即正在接收二进制数据)，则进入二进制接收循环
        if(ctx->state != MonitorReceiverContext::WaitHeader)
        {
            while(socket->bytesAvailable() > 0)
            {
                //步骤：接收JSON长度头
                if(ctx->state == MonitorReceiverContext::WaitJsonLen)
                {
                    if(socket->bytesAvailable() >= 4)
                    {
                        QByteArray h = socket->read(4);
                        QDataStream ds(&h,QIODevice::ReadOnly);
                        ds.setVersion(QDataStream::Qt_5_15);
                        quint32 len;
                        ds >> len;
                        if(len > 10 * 1024 * 1024)//限制JSON最大为10MB
                        {
                            qWarning()<<"无效的JSON长度";
                            socket->disconnectFromHost();
                            return;
                        }
                        ctx->jsonSize = len;
                        ctx->state = MonitorReceiverContext::WaitJsonData;
                    }
                    else
                    {
                        break;
                    }
                }

                //步骤：接收JSON数据
                if(ctx->state == MonitorReceiverContext::WaitJsonData)
                {
                    if(socket->bytesAvailable() >= (int)ctx->jsonSize)
                    {
                        ctx->jsonBuffer = socket->read(ctx->jsonSize);
                        ctx->state = MonitorReceiverContext::WaitImgLen;
                    }
                    else
                    {
                        break;
                    }
                }

                //步骤：接收图片长度头
                if(ctx->state == MonitorReceiverContext::WaitImgLen)
                {
                    if(socket->bytesAvailable() >= 4)
                    {
                        QByteArray h = socket->read(4);
                        QDataStream ds(&h,QIODevice::ReadOnly);
                        ds.setVersion(QDataStream::Qt_5_15);
                        quint32 len;
                        ds >> len;
                        if(len > 50 * 1024 * 1024)  //图片最大为50MB
                        {
                            qWarning() << "无效的图片长度";
                            socket->disconnectFromHost();
                            return;
                        }
                        ctx->imgSize = len;
                        ctx->state = MonitorReceiverContext::WaitImgData;
                    }
                    else
                    {
                        break;
                    }
                }

                //步骤：接收图片数据
                if(ctx->state == MonitorReceiverContext::WaitImgData)
                {
                    if(socket->bytesAvailable() >= (int)ctx->imgSize)
                    {
                        ctx->imgBuffer = socket->read(ctx->imgSize);

                        onLogMessage(QString("[监控] 收到完整数据包，JSON:%1 bytes,Img:%2 bytes").arg(ctx->jsonSize).arg(ctx->imgSize));

                        //解析JSON
                        QJsonParseError error;
                        QJsonDocument doc = QJsonDocument::fromJson(ctx->jsonBuffer,&error);
                        if(error.error == QJsonParseError::NoError && doc.isObject())
                        {
                            QJsonObject obj = doc.object();
                            QString studentId = obj["studentId"].toString();
                            QString appName = obj["appName"].toString();
                            QString type = obj["type"].toString();
                            QString timeStr = obj["time"].toString();

                            //从TCP包中提取备用信息(昵称，班级，TCP，端口)
                            QString reportedNick = obj["nickName"].toString();
                            QString reportedClass = obj["className"].toString();
                            quint16 reportedTcpport = 0;
                            if(obj.contains("tcpPort"))
                            {
                                reportedTcpport = static_cast<quint16>(obj["tcpPort"].toInt());
                            }

                            //从socket中获取对端IP(处理IPV6映射)
                            QString peerIp = socket->peerAddress().toString();
                            if(peerIp.startsWith("::ffff:"))
                            {
                                peerIp = peerIp.mid(7);
                            }

                            QString displayName;
                            qint64 now = QDateTime::currentMSecsSinceEpoch();

                            //如果g_onlineUsers中没有该学生，利用TCP信息创建临时记录
                            if(!g_onlineUsers.contains(studentId))
                            {
                                UserInfo tempUser;
                                tempUser.id = studentId;
                                tempUser.ip = peerIp; //利用TCP的ip
                                tempUser.tcpPort = reportedTcpport;
                                tempUser.port = 0;
                                tempUser.nickName = reportedNick.isEmpty() ? ("未知_" + studentId.right(4)) : reportedNick;
                                tempUser.className = reportedClass;
                                tempUser.isTeacher = false;
                                tempUser.lastHeartbeat = now;//初始化心跳时间为当前时间，防止立即被清理

                                g_onlineUsers[studentId] = tempUser;

                                //同步到数据库
                                StudentInfo dbInfo = DatabaseManager::instance().getStudentInfo(studentId);
                                if(dbInfo.id.isEmpty())
                                {
                                    DatabaseManager::instance().registerStudent(studentId,tempUser.nickName,peerIp);
                                    if(!reportedClass.isEmpty())
                                    {
                                        DatabaseManager::instance().updateStudentClass(studentId,reportedClass);
                                    }
                                    onLogMessage(QString("[TCP 补录] 新学生注册：%1 (IP:%2, 班级:%3, TCP端口:%4)")
                                                     .arg(tempUser.nickName,peerIp,reportedClass).arg(reportedTcpport));
                                }
                                else
                                {
                                    //更新现有记录的IP、班级和端口
                                    DatabaseManager::instance().registerStudent(studentId,dbInfo.name,peerIp);
                                    if(!reportedClass.isEmpty() && dbInfo.className != reportedClass)
                                    {
                                        DatabaseManager::instance().updateStudentClass(studentId,reportedClass);
                                    }
                                    onLogMessage(QString("[TCP 补录] 更新学生 %1 信息 (IP:%2, 班级:%3, TCP 端口:%4)")
                                                     .arg(dbInfo.name,peerIp,reportedClass).arg(reportedTcpport));
                                }

                                displayName = tempUser.nickName;
                            }
                            else
                            {
                                if(g_onlineUsers.contains(studentId))
                                {
                                    displayName = g_onlineUsers[studentId].nickName;

                                    //无论是否有信息变化，只要收到TCP包，就更新lastHeartbeat
                                    //这能防止因UDP心跳丢失导致学生被超时清理机制移除
                                    g_onlineUsers[studentId].lastHeartbeat = now;

                                    //即使学生已存在，也要更新可能缺失或变化的TCP端口
                                    if(reportedTcpport != 0 && g_onlineUsers[studentId].tcpPort != reportedTcpport)
                                    {
                                        g_onlineUsers[studentId].tcpPort = reportedTcpport;
                                        qDebug()<<"[TCP 补录] 更新在线学生" <<studentId <<"的TCP端口为："<<reportedTcpport;
                                    }

                                    //如果TCP包里有更新的班级信息，也尝试更新
                                    if(!reportedClass.isEmpty() && g_onlineUsers[studentId].className != reportedClass)
                                    {
                                        g_onlineUsers[studentId].className = reportedClass;
                                        DatabaseManager::instance().updateStudentClass(studentId,reportedClass);
                                    }

                                    //如果之前IP为空，现在补上
                                    if(g_onlineUsers[studentId].ip.isEmpty() && !peerIp.isEmpty())
                                    {
                                        g_onlineUsers[studentId].ip = peerIp;
                                    }

                                    //更新昵称
                                    if(!reportedNick.isEmpty() && g_onlineUsers[studentId].nickName != reportedNick)
                                    {
                                        g_onlineUsers[studentId].nickName = reportedNick;
                                    }
                                }
                                    else
                                    {
                                        displayName = "未知学生";
                                    }
                            }

                            //根据type类型处理不同事件
                            if(type == "STATUS_RECOVERY")
                            {
                                m_lastRecoverTime[studentId] = now;
                                updateStudentTableRow(studentId,displayName,StudentStatus::Online_Normal,"");
                                onLogMessage(QString("[监控] 学生 %1 已恢复正常").arg(displayName));
                            }
                            else if(type == "VIOLATION_REPORT")
                            {
                                qint64 lastRecover = m_lastRecoverTime.value(studentId,0);

                                if(lastRecover > 0 && (now - lastRecover) < 10000)
                                {
                                    onLogMessage(QString("[忽略] 学生 %1 在冷却期内违规（距恢复 %2ms）,不计入次数")
                                                     .arg(displayName).arg(now - lastRecover));
                                }
                                else
                                {
                                    onLogMessage(QString("[警报] 学生 %1 违规使用： %2").arg(displayName,appName));
                                    processViolationReport(studentId,appName,ctx->imgBuffer,timeStr);
                                    updateStudentTableRow(studentId,displayName,StudentStatus::Online_Violated,appName);
                                    updateOnlineStats();
                                }
                            }
                            else if(type == "LIVE_RESPONSE")
                            {
                                //关闭对应的加载对话框
                                if(m_loadingDialogs.contains(studentId))
                                {
                                    QDialog* loadingDlg = m_loadingDialogs.take(studentId);
                                    if(loadingDlg && loadingDlg->isVisible())
                                    {
                                        loadingDlg->accept();
                                        qDebug()<<"[UI] 加载对话框"<<displayName<<"成功关闭";
                                    }
                                    if(loadingDlg)
                                    {
                                        loadingDlg->deleteLater();
                                    }
                                }
                                else
                                {
                                    qDebug() << "[UI] 警告：没有加载对话框,id:"<<studentId;
                                }

                                if(!ctx->imgBuffer.isEmpty())
                                {
                                    QTimer::singleShot(50,this,[this,displayName,img = ctx->imgBuffer](){
                                        showScreenshotDialog(displayName + "(实时查看)",img);
                                    });
                                }
                            }
                            else if(type == "PERIODIC_MONITOR")
                            {
                                onLogMessage(QString("[监控] 收到 %1 的定期巡查截图").arg(displayName));
                                if(!ctx->imgBuffer.isEmpty())
                                {
                                    DatabaseManager::instance().insertMonitorRecord(studentId,"Periodic_Monitor","定期巡查",ctx->imgBuffer);
                                }
                                //定期巡查也刷新表格，确保学生可见（状态设为正常）
                                updateStudentTableRow(studentId,displayName,StudentStatus::Online_Normal,"");
                            }
                        }

                        //重置上下文，准备接收下一个数据包
                        ctx->state = MonitorReceiverContext::WaitHeader;
                        ctx->jsonBuffer.clear();
                        ctx->imgBuffer.clear();
                        ctx->pendingStudentId.clear();
                        ctx->jsonSize = 0;
                        ctx->imgSize = 0;

                        return;//处理完一个完整包后退出，避免重复处理
                    }
                    else
                    {
                        break;
                    }
                }
            }
            return; //二进制接收模式下，不再执行下方的文本协议解析
        }

        //---------------处理文本命令（带长度前缀）-----------------
        if(socket->bytesAvailable() >= (int)sizeof(quint32))
        {
            QByteArray header = socket->read(4);
            QDataStream ds(&header,QIODevice::ReadOnly);
            quint32 len;
            ds >> len;

            if(socket->bytesAvailable() >= len)
            {
                QByteArray cmdData = socket->read(len);
                QString cmd = QString::fromUtf8(cmdData);

                //检测到MONITOR_START后，设置状态并跳转到二进制处理逻辑
                if(cmd.startsWith("MONITOR_START|"))
                {
                    QStringList parts = cmd.mid(14).split('|');
                    if(parts.size() == 2)
                    {
                        ctx->jsonSize = parts[0].toUInt();
                        ctx->imgSize = parts[1].toUInt();
                        ctx->state = MonitorReceiverContext::WaitJsonLen;
                    }
                    goto process_binary;//立即处理后续二进制数据
                }

                //其他命令交给handlerPeerCommand处理
                handlePeerCommand(cmdData,socket);
            }
            else
            {
                //数据不足，等待更多数据（一般不会触发，长度头以告知大小）
                socket->waitForReadyRead(2000);
                if(socket->bytesAvailable() >= len)
                {
                    QByteArray cmdData = socket->read(len);
                    QString cmd = QString::fromUtf8(cmdData);

                    if(cmd.startsWith("MONITOR_START|"))
                    {
                        QStringList parts = cmd.mid(14).split('|');
                        if(parts.size() == 2)
                        {
                            ctx->jsonSize = parts[0].toUInt();
                            ctx->imgSize = parts[1].toUInt();
                            ctx->state = MonitorReceiverContext::WaitJsonLen;
                        }
                        goto process_binary;
                    }

                    handlePeerCommand(cmdData,socket);
                }
            }
        } });

    // 连接断开信号，自动删除套接字
    connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
}

/**
 * @brief 处理来自学生端的文件相关命令（LIST、DOWNLOAD）
 * @param data 命令数据
 * @param socket 对应的 TCP 套接字
 *
 * 支持命令：GET_FILE_LIST / LIST|（获取文件列表）、DOWNLOAD|（下载文件）。
 * 处理完成后通过 socket 返回数据（JSON 数组或文件二进制流）。
 * 使用老师端生成的动态端口，学生端直接连接
 */
void ServerWindow::handlePeerCommand(const QByteArray& data,QTcpSocket* socket)
{
    QString cmd = QString::fromUtf8(data);//将 UTF-8 格式的字节数组转换成 UTF-16 格式

    //获取共享根目录
    QString shareRoot = "E:/fileShared";
    if(m_sharePathEdit && !m_sharePathEdit->text().isEmpty())
    {
        shareRoot = m_sharePathEdit->text();
    }

    //-------------处理文件列表请求-------------
    if(cmd == "GET_FILE_LIST" || cmd.startsWith("LIST|"))
    {
        QString pathArg = "/";
        if(cmd.startsWith("LIST|"))
        {
            pathArg = cmd.mid(5);
        }
        //构建物理路径
        QString physicalPath = shareRoot;
        if(!pathArg.isEmpty() && pathArg != "/")
        {
            QString relative = pathArg;
            if(relative.startsWith("/"))
                relative = relative.mid(1);
            relative = QDir::fromNativeSeparators(relative);//将平台相关的路径分隔符统一转换为 正斜杠 /
            QDir root(shareRoot);
            physicalPath = root.filePath(relative);
        }
        physicalPath = QDir::cleanPath(physicalPath);//规范化路径字符串，移除冗余的部分

        // 安全检查：防止目录遍历攻击(物理路径必须在shareRoot下)
        if(!physicalPath.startsWith(shareRoot))
        {
            qWarning()<<"[安全检查] 已拦截目录遍历攻击请求："<<physicalPath;
            QJsonArray emptyArray;
            QJsonDocument doc(emptyArray);
            QByteArray jsonData = doc.toJson(QJsonDocument::Compact);// 将 JSON 文档序列化为紧凑格式的字节数组
            QByteArray block;
            QDataStream out(&block,QIODevice::WriteOnly);
            out<<(quint32)jsonData.size();
            out.writeRawData(jsonData.constData(),jsonData.size());//将 JSON 数据本身（字节数组）写入数据流，紧接着长度头之后。
            socket->write(block);//将组装好的完整数据包（长度头 + JSON 数据）写入 TCP 套接字，发送给学生端。
            return;
        }

        QDir dir(physicalPath);
        if(!dir.exists())
        {
            //如果请求的根目录不存在，尝试创建
            if(physicalPath == shareRoot)
            {
                if(!dir.mkpath(shareRoot))
                {
                    onLogMessage(QString("[错误] 无法创建共享根目录：%1").arg(shareRoot));
                }
            }
            else
            {
                //子目录不存在，返回空数组
                QJsonArray emptyArray;
                QJsonDocument doc(emptyArray);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                QByteArray block;
                QDataStream out(&block,QIODevice::WriteOnly);
                out << (quint32)jsonData.size();
                out.writeRawData(jsonData.constData(),jsonData.size());
                socket->write(block);
                return;
            }
        }

        //列出目录内容
        QJsonArray array;
        QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst);
        //第一个参数：包含文件和目录，排除.和..特殊目录
        //第二个参数：按名称排序，且目录排在文件前面
        for(const QFileInfo &fi : list)
        {
            QJsonObject obj;
            obj["name"] = fi.fileName();
            obj["isDir"] = fi.isDir();
            obj["size"] = (qint64)fi.size();

            if(!fi.isDir())
            {
                //对于文件，获取下载统计信息
                QString key = pathArg;
                if(!key.endsWith("/")) key += "/";
                key += fi.fileName();
                key.replace("\\","/");
                key = QDir::cleanPath(key);
                if(key.startsWith("//")) key = key.mid(1);
                if(!key.startsWith("/")) key = "/" + key;

                FileStatRecord stats = DatabaseManager::instance().getFileStat(key);
                obj["downloadCount"] = stats.downloadCount;
                obj["sourceIp"] = stats.lastSourceIp;
            }
            else
            {
                obj["downloadCount"] = 0;
                obj["sourceIp"] = "-";
            }

            array.append(obj);
        }

        //发送JSON数组
        QJsonDocument doc(array);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

        QByteArray block;
        QDataStream out(&block,QIODevice::WriteOnly);
        out<<(quint32)jsonData.size();
        out.writeRawData(jsonData.constData(),jsonData.size());
        socket->write(block);
        return;
    }

    //--------处理文件下载请求--------
    if(cmd.startsWith("DOWNLOAD|"))
    {
        QString args = cmd.mid(9);
        int splitIndex = args.indexOf('|');//查找分隔符
        QString pathArg = "/";
        QString fileName = args;

        if(splitIndex != -1)
        {
            pathArg = args.left(splitIndex);//提取路径部分
            fileName = args.mid(splitIndex + 1);//提取文件名部分
        }

        //构建物理路径
        QString physicalPath = shareRoot;
        if(!pathArg.isEmpty() && pathArg != "/")
        {
            QString relative = pathArg;
            if(relative.startsWith("/")) relative = relative.mid(1);
            relative = QDir::fromNativeSeparators(relative);
            QDir root(shareRoot);
            physicalPath = root.filePath(relative);
        }
        physicalPath = QDir::cleanPath(physicalPath);

        QString filePath = QDir(physicalPath).filePath(fileName);
        QFile file(filePath);

        QJsonObject meta;
        if(file.exists() && file.open(QIODevice::ReadOnly))
        {
            meta["name"] = fileName;
            meta["size"] = (qint64)file.size();
            meta["status"] = "ok";

            //发送元数据（文件名，大小，状态）
            QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);

            QByteArray block;
            QDataStream out(&block,QIODevice::WriteOnly);
            out<<(quint32)metaBytes.size();
            socket->write(block);
            socket->write(metaBytes);

            //发送文件数据（分块发送）
            QByteArray fileData = file.readAll();
            file.close();

            int chunkSize = 8192;
            for(int i = 0;i < fileData.size();i += chunkSize)
            {
                socket->write(fileData.mid(i,chunkSize));
            }

            //更新文件下载统计
            QString statsKey = pathArg;
            if(!statsKey.endsWith("/")) statsKey += "/";
            statsKey += fileName;
            statsKey.replace("\\","/");
            statsKey = QDir::cleanPath(statsKey);
            if(statsKey.startsWith("//")) statsKey = statsKey.mid(1);
            if(!statsKey.startsWith("/")) statsKey = "/" + statsKey;

            QString clientIp = socket->peerAddress().toString();

            bool updateOk = DatabaseManager::instance().updateFileStat(statsKey,clientIp);
            onLogMessage(QString("[下载] 学生下载了文件：%1（大小：%2） [DB KEY:%3] [更新结果：%4]")
                             .arg(fileName).arg(fileData.size()).arg(statsKey).arg(updateOk ? "成功" : "失败"));
        }
        else
        {
            //文件不存在，返回错误信息
            meta["status"] = "error";
            meta["msg"] = "文件未发现：" + filePath;
            QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);
            QByteArray block;
            QDataStream out(&block,QIODevice::WriteOnly);
            out<<(quint32)metaBytes.size();
            socket->write(block);
            socket->write(metaBytes);
            onLogMessage(QString("[下载] 文件未找到：%1").arg(filePath));
        }
    }
}

/**
 * @brief ServerWindow 析构函数
 *
 * 停止服务器、关闭文件服务器、删除 UI 对象。
 */
ServerWindow::~ServerWindow()
{
    if(m_fileServer) m_fileServer->close();     //关闭文件服务器
}

/**
 * @brief 重写窗口关闭事件
 * @param event 关闭事件对象
 *
 * 在关闭窗口前询问用户是否确认退出，若确认则停止服务器并接受关闭。
 */
void ServerWindow::closeEvent(QCloseEvent *event)
{
    if(m_fileServer && m_fileServer->isListening())
    {
        int ret = QMessageBox::question(this,"确认退出",
                                        "服务器正在运行，确定要关闭服务器并退出程序吗？",
                                        QMessageBox::Yes | QMessageBox::No,QMessageBox::No);
        if(ret == QMessageBox::No)
        {
            event->ignore();//忽略关闭事件，不退出
            return;
        }
        m_fileServer->close();//停止服务器
    }
    event->accept();//接受关闭事件，退出程序
}

/**
 * @brief 请求实时截图（教师主动查看）
 * @param studentId 学生 ID
 *
 * 显示加载对话框，向学生端发送 GET_SCREENSHOT_NOW 命令，
 * 等待学生通过 TCP 返回 LIVE_RESPONSE 类型数据。
 */
void ServerWindow::onRequestLiveScreenshotClicked(const QString& studentId)
{
    //获取学生姓名用于显示
    StudentInfo info = DatabaseManager::instance().getStudentInfo(studentId);
    QString displayName = info.name;
    if(displayName.isEmpty())
    {
        if(g_onlineUsers.contains(studentId))
        {
            displayName = g_onlineUsers[studentId].nickName;
        }
        else
        {
            displayName = "未知学生";
        }
    }

    onLogMessage(QString("[监控] 正在向 %1 请求实时屏幕...").arg(displayName));

    //先清理可能存在的旧加载框（防止重复点击导致多个加载框）
    if(m_loadingDialogs.contains(studentId))
    {
        QDialog *oldDlg = m_loadingDialogs.take(studentId);//从映射中移除键为 studentId 的项，并返回该项的值，即 QDialog* 指针
        if(oldDlg)
        {
            oldDlg->close();
            oldDlg->deleteLater();
        }
    }

    //创建加载对话框
    QDialog *loadingDlg = showLoadingDialog(displayName);

    //将对话框存入Map，键为学生ID，以便收到数据后能找到并关闭它
    m_loadingDialogs[studentId] = loadingDlg;

    if(g_onlineUsers.contains(studentId))
    {
        UserInfo user = g_onlineUsers[studentId];
        sendGetScreenshotCommand(user.ip,user.tcpPort);//发送截图命令

        //添加超时机制：8秒内未收到响应，关闭加载框并记录日志
        QTimer::singleShot(8000,this,[this,studentId,loadingDlg](){
            if(m_loadingDialogs.contains(studentId))
            {
                QDialog* dlg = m_loadingDialogs.take(studentId);
                if(dlg && dlg->isVisible())
                {
                    dlg->reject();
                    dlg->deleteLater();
                    onLogMessage(QString("[监控] 请求 %1 实时屏幕超时").arg(studentId));
                }
            }
        });
    }
    else
    {
        onLogMessage(QString("[错误] 无法找到学生 %1 的网络信息，无法发送请求").arg(studentId));
        //清理Map
        if(m_loadingDialogs.contains(studentId))
        {
            QDialog* dlg = m_loadingDialogs.take(studentId);
            if(dlg)
            {
                dlg->reject();
                dlg->deleteLater();
            }
        }
        QMessageBox::warning(this,"错误","无法获取学生网络信息，请确认学生在线。");
    }
}

/**
 * @brief 显示正在请求实时屏幕的加载对话框
 * @param studentName 学生姓名
 * @return QDialog* 对话框指针，用于后续关闭
 *
 * 对话框包含不确定进度条和提示文本，直到收到学生响应或超时。
 */
QDialog* ServerWindow::showLoadingDialog(const QString& studentName)
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("正在获取屏幕...");
    dlg->setModal(true);    //模态对话框
    dlg->setFixedSize(300,150);

    QVBoxLayout* layout = new QVBoxLayout(dlg);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* infoLabel = new QLabel(QString("正在请求 <b>%1</b> 的实时屏幕...\n请稍候").arg(studentName));
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    //不确定进度条（一直在滚动）
    QProgressBar* progressBar = new QProgressBar();
    progressBar->setRange(0,0); //范围0-0表示不确定模式
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(10);
    progressBar->setStyleSheet(R"(
        QProgressBar{
            border : 1px solid #ccc;
            border-radius: 5px;
            background:white;
        }
        QProgressBar::chunk{
            background-color : #3498db;
            width:20px;
        }
    )");
    layout->addWidget(progressBar);

    dlg->show();//显示对话框(非阻塞,等待信号关闭)
    return dlg;
}

/**
 * @brief 向学生端发送获取实时截图的命令
 * @param studentIp 学生 IP
 * @param studentTcpPort 学生 TCP 端口
 *
 * 此时老师端作为客户端，主动连接学生端TCP端口（之前获得的随机端口）
 * 建立 TCP 连接，发送 "GET_SCREENSHOT_NOW" 命令，然后断开。
 * 学生端收到后会立即捕获屏幕并通过 TCP 返回（协议为 MONITOR_START + 二进制数据）。
 */
void ServerWindow::sendGetScreenshotCommand(const QString &studentIp,quint16 studentTcpPort)
{
    QTcpSocket* socket = new QTcpSocket(this);
    //连接成功后发送命令
    connect(socket,&QTcpSocket::connected,[socket](){
        QString cmd = "GET_SCREENSHOT_NOW";
        QByteArray data = cmd.toUtf8();
        QByteArray block;
        QDataStream out(&block,QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_15);
        out<<(quint32)data.size();
        out.writeRawData(data.constData(),data.size());
        socket->write(block);
        socket->disconnectFromHost();//发送后立即断开
    });
    //错误处理：打印日志并删除套接字
    connect(socket,&QTcpSocket::errorOccurred,[socket](QAbstractSocket::SocketError){
        qDebug()<<"[Command] Failed to send screenshot request:"<<socket->errorString();
        socket->deleteLater();
    });
    //断开后删除套接字
    connect(socket,&QTcpSocket::disconnected,socket,&QTcpSocket::deleteLater);

    socket->connectToHost(studentIp,studentTcpPort);//连接学生端
}

/**
 * @brief 处理违规报告的核心逻辑（写库 + 日志）
 * @param studentId 学生 ID
 * @param appName 违规应用名称
 * @param imageData 截图数据（可为空）
 * @param timeStr 时间字符串
 *
 * 调用 DatabaseManager 插入监控记录，并输出日志。
 * 无论是否有图片数据，都会插入记录（图片可为空）。
 */
void ServerWindow::processViolationReport(const QString &studentId,const QString& appName,
                                        const QByteArray &imageData,const QString &timeStr)
{
    QString details = QString("自动检测违规应用：%1 时间 %2").arg(appName,timeStr);

    //无论是否有图片数据，都插入记录
    //如果是定期巡查或特定类型，图片可能为空，但日志仍需记录
    if(!imageData.isEmpty())
    {
        DatabaseManager::instance().insertMonitorRecord(studentId,appName,details,imageData);
        onLogMessage(QString("[DB] 已保存 %1 的违规记录及截图 （%2）").arg(studentId,appName));
    }
    else
    {
        //即使没有截图，也记录文本日志
        //注：insertMonitorRecord 支持空 imageData
        DatabaseManager::instance().insertMonitorRecord(studentId,appName,details,imageData);
        onLogMessage(QString("[DB] 已保存 %1 的状态变更记录 （%2）").arg(studentId,appName));
    }
}

/**
 * @brief 统一刷新班级下拉框
 *
 * 同时刷新设置页的“删除班级”下拉框和监控页的“班级筛选”下拉框，
 * 从数据库重新加载班级列表并保持当前选中项（如果仍存在）。
 */
void ServerWindow::refreshClassComboBoxes()
{
    //1.刷新设置页的”删除班级“下拉框
    if(m_deleteClassCombo)
    {
        QString currentSelection = m_deleteClassCombo->currentText();
        m_deleteClassCombo->clear();
        m_deleteClassCombo->addItem("请选择要删除的班级","");//占位符

        QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
        for(const auto&cls : classes)
        {
            m_deleteClassCombo->addItem(cls.className);
        }

        //尝试恢复之前的选择（如果还存在）
        if(!currentSelection.isEmpty() && currentSelection != "请选择要删除的班级")
        {
            int index = m_deleteClassCombo->findText(currentSelection);
            if(index != -1)
            {
                m_deleteClassCombo->setCurrentIndex(index);
            }
        }
    }

    //2.刷新监控页的”查看班级“筛选下拉框
    if(m_classFilterCombo)
    {
        QString currentFilter = m_classFilterCombo->currentText();
        m_classFilterCombo->clear();
        m_classFilterCombo->addItem("全部班级");

        QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
        for(const auto  &cls : classes)
        {
            m_classFilterCombo->addItem(cls.className);
        }

        //尝试恢复之前的筛选条件
        int index = m_classFilterCombo->findText(currentFilter);
        if(index != -1)
        {
            m_classFilterCombo->setCurrentIndex(index);
        }
        else
        {
            //如果当前正在查看被删除的班级，则切换回全部班级
            m_currentFilterClass = "全部班级";
            refreshMonitorTable("全部班级");
        }
    }
    qDebug()<<"[UI] 班级下拉框更新";
}

/**
 * @brief 创建班级按钮点击响应
 *
 * 从输入框读取班级名，调用数据库创建班级，刷新下拉框并广播最新班级列表。
 */
void ServerWindow::onCreateClassClicked()
{
    QString className = m_newClassNameEdit->text().trimmed();
    if(className.isEmpty())
    {
        QMessageBox::warning(this,"提示","班级名称不能为空！");
        return;
    }

    if(DatabaseManager::instance().createClass(className))
    {
        QMessageBox::information(this,"成功",QString("班级 '%1' 创建成功！ \n学生端现在可以加入该班级。").arg(className));
        m_newClassNameEdit->clear();

        //刷新本地设置页下拉框
        refreshClassComboBoxes();

        //创建班级后立即广播，让学生下拉框立即更新
        broadcastUserList();

        onLogMessage(QString("[班级管理] 创建了新班级：%1 并已广播").arg(className));
    }
    else
    {
        QMessageBox::warning(this,"失败","创建失败，该班级可能已存在。");
    }
}

/**
 * @brief  删除班级按钮点击响应
 *
 * 从下拉框选择班级，确认后调用数据库删除，刷新下拉框并广播最新班级列表。
 */
void ServerWindow::onDeleteClassClicked()
{
    QString className = m_deleteClassCombo->currentText();
    if(className.isEmpty() || className == "请选择要删除的班级")
    {
        QMessageBox::warning(this,"提示","请先选择一个要删除的班级");
        return;
    }

    int ret = QMessageBox::question(this,"确认删除",
                                    QString("确定要删除班级 \"%1\" 吗？\n\n注意：\n1.该班级将从列表中移除。\n2.原属于该班级的所有学生将变为“未分班”状态。\n3.学生端需要重新选择班级加入。").arg(className));
    if(ret != QMessageBox::Yes)
    {
        return;
    }

    if(DatabaseManager::instance().deleteClass(className))
    {
        onLogMessage(QString("[班级管理] 成功删除班级：%1").arg(className));
        QMessageBox::information(this,"成功",QString("班级 '%1' 已删除。\n相关学生已重置为未分班状态").arg(className));

        //刷新本地设置页下拉框（移除已删除项）
        refreshClassComboBoxes();

        //刷新监控页面的筛选下拉框
        m_classFilterCombo->clear();
        m_classFilterCombo->addItem("全部班级");
        QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
        for(const auto &cls : classes)
        {
            m_classFilterCombo->addItem(cls.className);
        }

        //如果当前正在查看被删除的班级，切换回”全部班级“
        if(m_currentFilterClass == className)
        {
            m_currentFilterClass = "全部班级";
            refreshMonitorTable("全部班级");
        }

        //广播最新班级列表，通知学生端该班级已不存在
        broadcastUserList();
    }
    else
    {
        QMessageBox::warning(this,"失败","删除失败，请检查数据库连接或日志");
        onLogMessage(QString("[班级管理] 删除班级失败：%1").arg(className));
    }
}

/**
 * @brief 查看班级成员按钮点击响应
 *
 * 根据当前筛选下拉框的班级，刷新监控表格。
 */
void ServerWindow::onViewClassMembersClicked()
{
    QString selectedClass = m_classFilterCombo->currentText();
    m_currentFilterClass = selectedClass;//同步更新筛选状态
    refreshMonitorTable(selectedClass);
    onLogMessage(QString("[班级管理] 查看班级成员：%1").arg(selectedClass == "全部班级" ? "所有" : selectedClass));
}

/**
 * @brief 班级筛选下拉框选项改变时的响应
 * @param className 新选择的班级名称（或“全部班级”）
 *
 * 更新当前筛选状态并刷新监控表格。
 */
void ServerWindow::onClassComboBoxChanged(const QString &className)
{
    //如果下拉框返回空字符串，强制设置为”全部班级“
    if(className.isEmpty())
    {
        m_currentFilterClass = "全部班级";
    }
    else
    {
        m_currentFilterClass = className;
    }
    refreshMonitorTable(m_currentFilterClass);
}

/**
 * @brief 状态过滤下拉框选项改变时的响应
 * @param status 新选择的状态
 *
 * 更新当前筛选状态并刷新监控表格。
 */
void ServerWindow::onStatusComboBoxChanged(const QString &status)
{
    if(status.isEmpty())
    {
        m_currentFilterStatus = "全部";
    }
    else
    {
        m_currentFilterStatus = status;
    }
    qDebug() << "[过滤] 当前在线状态选择为:"<<status;
    refreshMonitorTable(m_currentFilterClass);
}

//请求摄像头流
void ServerWindow::onRequestCameraStream(const QString &studentId)
{
    StudentInfo info = DatabaseManager::instance().getStudentInfo(studentId);
    QString displayName = info.name.isEmpty() ? studentId : info.name;

    if (!g_onlineUsers.contains(studentId)) {
        QMessageBox::warning(this, "错误", "学生不在线，无法请求摄像头。");
        return;
    }
    UserInfo user = g_onlineUsers[studentId];

    // 如果之前有未断开的连接，先强制清理，防止资源冲突
    if (m_cameraSocket) {
        qDebug() << "[摄像头] 检测到旧连接，正在清理...";
        m_cameraSocket->disconnectFromHost();
        m_cameraSocket->deleteLater();
        m_cameraSocket = nullptr;
    }

    // 如果之前的对话框还在，先关闭
    if (m_cameraDialog) {
        m_cameraDialog->close();
        m_cameraDialog->deleteLater();
        m_cameraDialog = nullptr;
    }

    // 启动临时TCP服务器（若未监听）
    if (!m_cameraServer->isListening()) {
        if (!m_cameraServer->listen(QHostAddress::Any, 0)) {            //监听任意地址，端口0表示由系统自动分配一个可用端口
            QMessageBox::critical(this, "错误", "无法启动视频接收服务");
            return;
        }
    }
    quint16 cameraPort = m_cameraServer->serverPort();//获取系统实际分配的端口号

    // 获取主屏幕几何信息
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    int width = screenRect.width() * 2 / 3;
    int height = screenRect.height() * 2 / 3;

    // 创建视频显示对话框
    m_cameraDialog = new QDialog(this);
    m_cameraDialog->setWindowTitle(QString("实时摄像头 - %1").arg(displayName));
    m_cameraDialog->resize(width, height);
    QVBoxLayout *layout = new QVBoxLayout(m_cameraDialog);
    m_cameraLabel = new QLabel("正在连接摄像头...");
    m_cameraLabel->setAlignment(Qt::AlignCenter);
    m_cameraLabel->setStyleSheet("background: black; color: white;");
    layout->addWidget(m_cameraLabel);
    m_cameraDialog->show();

    // 发送UDP请求
    //构造UDP命令字符串："GET_CAMERA_STREAM|教师IP|教师端口"
    QString cmd = QString("GET_CAMERA_STREAM|%1|%2").arg(m_myIp).arg(cameraPort);
    //通过UDP单播发送给学生端，学生端监听端口为8889
    m_udpSocket->writeDatagram(cmd.toUtf8(), QHostAddress(user.ip), user.port);

    // 超时处理（10秒）
    QTimer::singleShot(10000, this, [this, studentId](){
        if (m_cameraDialog && m_cameraDialog->isVisible() && !m_cameraSocket) {
            m_cameraDialog->close();
            QMessageBox::information(this, "超时", "连接摄像头超时，请检查学生端网络或摄像头设备。");
        }
    });
}

//当有新的摄像头套接字连接时
void ServerWindow::onNewCameraConnection()
{
    //如果已经有视频连接，先断开旧的（只允许同时查看一个摄像头）
    if (m_cameraSocket) {
        m_cameraSocket->disconnectFromHost();
        m_cameraSocket->deleteLater();
        m_cameraSocket = nullptr;
    }
    //获取新建立的TCP套接字
    m_cameraSocket = m_cameraServer->nextPendingConnection();

    //创建上下文结构体，用于缓存不完整的帧数据（处理粘包/半包）
    auto ctx = new CameraStreamContext;  // 绑定到 socket 的生命周期
    connect(m_cameraSocket, &QTcpSocket::readyRead, this, [this, ctx]() {
        // 将新数据追加到缓冲区
        ctx->buffer.append(m_cameraSocket->readAll());

        while (true) {
            //状态1：等待帧头（4字节长度）
            if (ctx->expectedFrameSize == 0) {
                // 需要读取帧头（4字节）
                if (ctx->buffer.size() < 4) break;//数据不足，等待更多
                QDataStream ds(ctx->buffer.left(4));//读取前四个字节
                ds >> ctx->expectedFrameSize;//获取帧长度
                ctx->buffer.remove(0, 4);//移除已读的帧头
                if (ctx->expectedFrameSize == 0) continue; // 心跳或无效帧
            }

            //状态2： 检查是否收到完整的一帧
            if (ctx->buffer.size() < (int)ctx->expectedFrameSize) break;

            // 取出完整帧数据
            QByteArray frameData = ctx->buffer.left(ctx->expectedFrameSize);
            ctx->buffer.remove(0, ctx->expectedFrameSize);
            ctx->expectedFrameSize = 0;//重置，准备接收下一帧

            // 处理帧（需要在 UI 线程中更新，避免阻塞网络接收）
            if (frameData.startsWith("ERROR:")) {
                //错误消息
                QString errMsg = QString::fromUtf8(frameData.mid(6));
                //跨线程安全弹出警告框
                QMetaObject::invokeMethod(this, [this, errMsg]() {
                    QMessageBox::warning(this, "摄像头错误", errMsg);
                    if (m_cameraSocket) m_cameraSocket->disconnectFromHost();
                }, Qt::QueuedConnection);
            } else {
                //正常图像数据
                // 将解码和显示抛到 UI 线程，但注意 setPixmap 必须在 UI 线程，此处已经处于 UI 线程（因为 readyRead 在主线程）
                // 但是缩放操作可能耗时，建议使用 QImage 预先处理
                QPixmap pix;
                if (pix.loadFromData(frameData, "JPEG")) {
                    if (m_cameraLabel) {
                        QSize labelSize = m_cameraLabel->size();
                        //缩放图像以适应标签大小
                        if (labelSize.isValid() && labelSize.width() > 0 && labelSize.height() > 0) {
                            pix = pix.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        }
                        m_cameraLabel->setPixmap(pix);//显示
                    }
                }
            }
        }
    });

    //连接断开信号，清理上下文并关闭对话框
    connect(m_cameraSocket, &QTcpSocket::disconnected, this, [this, ctx]() {
        delete ctx;
        onCameraStreamDisconnected();//统一清理界面资源
    });
}

//清理显示对话框和服务器
void ServerWindow::onCameraStreamDisconnected()
{
    if (m_cameraSocket) {
        m_cameraSocket->deleteLater();
        m_cameraSocket = nullptr;
    }
    if (m_cameraDialog) {
        m_cameraDialog->close();
        m_cameraDialog->deleteLater();
        m_cameraDialog = nullptr;
    }
    m_cameraServer->close(); // 关闭服务器，下次请求再启动，重新listen
}
