#include "serverwindow.h"
#include "ui_serverwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>
#include <QDir>
#include <QFileDialog>
#include <QPixmap>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDateTime>
#include <QRandomGenerator>
#include <QHeaderView>
#include <QScrollBar>
#include <QScreen>
#include <QProgressBar>
#include <QInputDialog>
#include <QComboBox>

struct UserInfo {
    QString id;
    QString ip;
    quint16 port;
    quint16 tcpPort;
    QString nickName;
    bool isTeacher;
    qint64 lastHeartbeat;
    QString className; // 【新增】缓存心跳包中的班级信息
};

// 【修复】定义全局静态变量，供 onUdpReadyRead 和 broadcastUserList 共用，避免局部变量冲突
static QMap<QString, UserInfo> g_onlineUsers;

const quint16 BASE_UDP_PORT = 9999; 
const quint64 STUDENT_LISTEN_PORT = 8889; 

ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ServerWindow)
    , m_server(nullptr)
    , m_udpSocket(new QUdpSocket(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_fileServer(new QTcpServer(this))
{
    ui->setupUi(this);
    this->setWindowTitle("智慧教室监控系统 - 教师端");
    this->resize(1200, 800);

    // 获取本机 IP
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (const QHostAddress &addr : list) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            m_myIp = addr.toString();
            break;
        }
    }
    if (m_myIp.isEmpty()) m_myIp = "127.0.0.1";

    m_myUdpPort = BASE_UDP_PORT; 
    m_myTcpPort = 20000 + QRandomGenerator::global()->bounded(10000);

    // 初始化数据库
    // 初始化数据库
    if (!DatabaseManager::instance().init()) {
        QMessageBox::critical(this, "错误", "数据库初始化失败！");
    }

    // 【新增】启动前从数据库预加载所有学生信息到 g_onlineUsers
    // 目的：确保即使学生当前未发心跳，表格也能显示其历史班级和名称，避免显示“未知”
    QMap<QString, StudentInfo> allStudents = DatabaseManager::instance().getAllStudents();
    for (auto it = allStudents.begin(); it != allStudents.end(); ++it) {
        UserInfo user;
        user.id = it.key();
        user.nickName = it->name;
        user.ip = it->ip;
        user.className = it->className;
        user.isTeacher = false;
        user.port = 0;
        user.tcpPort = 0;
        user.lastHeartbeat = 0; // 标记为离线/未知状态
        g_onlineUsers[it.key()] = user;
    }
    qDebug() << "[Init] Pre-loaded" << allStudents.size() << "students from database.";

    setupMainLayout();

    // 初始化核心服务器 (用于管理连接和状态)
    m_server = new FileServer(this);
    
    connect(m_server, &FileServer::logMessage, this, &ServerWindow::onLogMessage);
    connect(m_server, &FileServer::studentStatusUpdated, this, &ServerWindow::onStudentStatusUpdated);

    quint16 port = 9999;
    if (!m_server->start(port)) {
        QMessageBox::critical(this, "启动失败", QString("服务器启动失败！\n端口：%1\n错误：%2").arg(port).arg(m_server->errorString()));
    } else {
        onLogMessage(QString(">>> 服务器已成功启动，监听端口：%1 <<<").arg(port));
    }
    
    // --- 启动 UDP 发现机制 ---
    if (!m_udpSocket->bind(QHostAddress::Any, m_myUdpPort)) {
        onLogMessage(QString("UDP 绑定失败，端口：%1").arg(m_myUdpPort));
    } else {
        onLogMessage(QString("教师端 UDP 监听端口：%1").arg(m_myUdpPort));
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &ServerWindow::onUdpReadyRead);
    }

    // --- 启动教师端文件服务器 (P2P) ---
    if (!m_fileServer->listen(QHostAddress::Any, m_myTcpPort)) {
        onLogMessage(QString("教师端文件服务器启动失败：%1").arg(m_fileServer->errorString()));
    } else {
        onLogMessage(QString("教师端文件共享已开启，监听端口：%1").arg(m_myTcpPort));
        connect(m_fileServer, &QTcpServer::newConnection, this, &ServerWindow::onNewFileConnection);
    }

    // 启动心跳广播
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ServerWindow::sendServerHeartbeat);
    m_heartbeatTimer->start(5000);
    sendServerHeartbeat(); // 立即发送一次
    
    // 【新增】启动后立即主动广播班级列表，确保学生端能第一时间获取到已有班级
    // 防止因定时器未触发或丢包导致学生端下拉框初始为空
    broadcastUserList();

    onLogMessage("系统就绪，等待学生上线...");
    
    // 【新增】初始化班级列表
    // 【修复】显式初始化筛选状态为"全部班级"，防止默认值为空字符串导致逻辑歧义
    m_currentFilterClass = "全部班级"; 
    
    // 【修复】初始加载所有学生，并传入明确的筛选条件
    refreshMonitorTable("全部班级"); 
}

void ServerWindow::setupMainLayout() {
    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 左侧导航栏
    QWidget *navWidget = new QWidget();
    navWidget->setFixedWidth(200);
    navWidget->setStyleSheet("QWidget { background-color: #2c3e50; }");
    QVBoxLayout *navLayout = new QVBoxLayout(navWidget);
    navLayout->setSpacing(20);
    navLayout->setContentsMargins(10, 20, 10, 20);

    QLabel *titleLogo = new QLabel("智控中心");
    titleLogo->setStyleSheet("color: white; font-size: 20px; font-weight: bold; margin-bottom: 20px;");
    titleLogo->setAlignment(Qt::AlignCenter);
    navLayout->addWidget(titleLogo);

    // 【修复】初始化成员变量，而不是局部变量
    btnMonitor = new QPushButton("🖥️ 实时监控");
    btnFile = new QPushButton("📁 文件传输");
    btnSettings = new QPushButton("⚙️ 系统设置");

    QVector<QPushButton*> navButtons = {btnMonitor, btnFile, btnSettings};
    for (QPushButton *btn : navButtons) {
        btn->setStyleSheet(R"(
            QPushButton {
                background-color: transparent;
                color: #ecf0f1;
                text-align: left;
                padding: 15px;
                font-size: 16px;
                border-radius: 5px;
            }
            QPushButton:hover { background-color: #34495e; }
            QPushButton:checked { background-color: #3498db; }
        )");
        btn->setCheckable(true);
        navLayout->addWidget(btn);
    }
    
    btnMonitor->setChecked(true);

    connect(btnMonitor, &QPushButton::clicked, this, &ServerWindow::onNavMonitorClicked);
    connect(btnFile, &QPushButton::clicked, this, &ServerWindow::onNavFileClicked);
    connect(btnSettings, &QPushButton::clicked, this, &ServerWindow::onNavSettingsClicked);

    navLayout->addStretch();

    // 右侧内容区
    m_stackedWidget = new QStackedWidget();
    
    setupMonitorPage();
    setupFilePage();
    setupSettingsPage();

    mainLayout->addWidget(navWidget);
    mainLayout->addWidget(m_stackedWidget);
}

void ServerWindow::setupMonitorPage() {
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    
    // 顶部统计栏与班级操作区
    QHBoxLayout *topBar = new QHBoxLayout();
    
    // 【新增】班级筛选下拉框
    QLabel *filterLabel = new QLabel("查看班级：");
    filterLabel->setFont(QFont("Arial", 12, QFont::Bold));
    topBar->addWidget(filterLabel);
    
    m_classFilterCombo = new QComboBox();
    m_classFilterCombo->setMinimumWidth(150);
    m_classFilterCombo->addItem("全部班级");
    // 加载已有班级
    QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
    for (const auto &cls : classes) {
        m_classFilterCombo->addItem(cls.className);
    }
    connect(m_classFilterCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged), 
            this, &ServerWindow::onClassComboBoxChanged);
    topBar->addWidget(m_classFilterCombo);

    // 【新增】确保初始状态选中“全部班级”
    m_classFilterCombo->setCurrentIndex(0);

    // 【新增】查看班级按钮（也可通过下拉框直接触发，此处保留按钮作为快捷操作或刷新）
    btnViewClass = new QPushButton("刷新班级视图");
    btnViewClass->setStyleSheet("background-color: #3498db; color: white; padding: 5px 10px; border-radius: 4px;");
    connect(btnViewClass, &QPushButton::clicked, this, &ServerWindow::onViewClassMembersClicked);
    topBar->addWidget(btnViewClass);

    topBar->addStretch();
    
    QLabel *statLabel = new QLabel("在线人数：0 | 违规警告：0");
    statLabel->setFont(QFont("Arial", 12, QFont::Bold));
    topBar->addWidget(statLabel);
    
    layout->addLayout(topBar);

    // 监控表格
    m_monitorTable = new QTableWidget();
    // 【修改】将第 1 列表头由“学生 ID"改为“学生 IP"
    m_monitorTable->setColumnCount(7);
    m_monitorTable->setHorizontalHeaderLabels({"学生 IP", "姓名", "班级", "状态", "违规应用", "违规次数", "查看屏幕"}); 
    m_monitorTable->horizontalHeader()->setStretchLastSection(true);
    m_monitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_monitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_monitorTable->verticalHeader()->setVisible(false);
    m_monitorTable->setAlternatingRowColors(true);
    
    // 【修复】调整列宽以适应 7 列，确保所有列可见且对齐
    m_monitorTable->setColumnWidth(0, 130); // ID
    m_monitorTable->setColumnWidth(1, 100); // 姓名
    m_monitorTable->setColumnWidth(2, 120); // 班级
    m_monitorTable->setColumnWidth(3, 80);  // 状态
    m_monitorTable->setColumnWidth(4, 150); // 违规应用
    m_monitorTable->setColumnWidth(5, 80);  // 违规次数 (新增独立列)
    m_monitorTable->setColumnWidth(6, 100); // 查看屏幕 (操作列)
    
    connect(m_monitorTable, &QTableWidget::cellDoubleClicked, this, &ServerWindow::onTableCellDoubleClicked);
    
    layout->addWidget(m_monitorTable);

    // 底部详情区
    QWidget *detailPanel = new QWidget();
    detailPanel->setFixedHeight(150);
    detailPanel->setStyleSheet("QWidget { background-color: #f9f9f9; border-top: 1px solid #ddd; }");
    QHBoxLayout *detailLayout = new QHBoxLayout(detailPanel);
    
    m_detailLabel = new QLabel("请选择一名学生查看详情...");
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setMinimumWidth(300);
    
    m_screenshotPreview = new QLabel();
    m_screenshotPreview->setFixedSize(120, 90);
    m_screenshotPreview->setStyleSheet("border: 1px solid #ccc; background: white;");
    m_screenshotPreview->setAlignment(Qt::AlignCenter);
    m_screenshotPreview->setText("无截图");

    detailLayout->addWidget(m_screenshotPreview);
    detailLayout->addWidget(m_detailLabel);
    detailLayout->addStretch();
    
    layout->addWidget(detailPanel);

    m_stackedWidget->addWidget(page);
}

void ServerWindow::setupFilePage() {
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    QLabel *label = new QLabel("文件传输记录与管理界面（此处可嵌入原有的文件列表逻辑）");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    
    // 复用之前的日志窗口作为文件传输日志
    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Consolas", 10));
    layout->addWidget(m_logView);
    
    m_stackedWidget->addWidget(page);
}

void ServerWindow::setupSettingsPage() {
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    
    // --- 原有共享路径设置 ---
    QLabel *lbl = new QLabel("共享文件夹路径设置：");
    lbl->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    layout->addWidget(lbl);
    
    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_sharePathEdit = new QLineEdit("E:/fileShared");
    m_sharePathEdit->setPlaceholderText("请选择共享文件夹根目录");
    pathLayout->addWidget(m_sharePathEdit);
    
    QPushButton *btnBrowse = new QPushButton("浏览...");
    btnBrowse->setStyleSheet("background-color: #3498db; color: white; padding: 5px 15px; border-radius: 4px;");
    
    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        QString currentPath = m_sharePathEdit->text();
        if (!QDir(currentPath).exists()) {
            currentPath = QDir::homePath();
        }
        
        QString selectedDir = QFileDialog::getExistingDirectory(this, "选择共享文件夹根目录", currentPath);
        
        if (!selectedDir.isEmpty()) {
            m_sharePathEdit->setText(selectedDir);
            onLogMessage(QString("[设置] 共享文件夹路径已更新为：%1").arg(selectedDir));
            
            QDir dir(selectedDir);
            if (!dir.exists()) {
                if (QMessageBox::question(this, "提示", "该目录不存在，是否立即创建？") == QMessageBox::Yes) {
                    if (dir.mkpath(".")) {
                        onLogMessage("[系统] 已成功创建共享目录");
                    } else {
                        QMessageBox::warning(this, "错误", "创建目录失败，请检查权限！");
                    }
                }
            }
        }
    });
    
    pathLayout->addWidget(btnBrowse);
    layout->addLayout(pathLayout);
    
    QLabel *hintLabel = new QLabel("提示：修改后即时生效，无需重启服务器。请确保该目录有读写权限。");
    hintLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 10px;");
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);
    
    layout->addSpacing(30);
    
    // --- 【新增】班级管理区域 ---
    QLabel *classLbl = new QLabel("班级管理：");
    classLbl->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    layout->addWidget(classLbl);
    
    QHBoxLayout *classLayout = new QHBoxLayout();
    m_newClassNameEdit = new QLineEdit();
    m_newClassNameEdit->setPlaceholderText("输入新班级名称（例如：三年二班）");
    m_newClassNameEdit->setMinimumWidth(200);
    classLayout->addWidget(m_newClassNameEdit);
    
    btnCreateClass = new QPushButton("创建班级");
    btnCreateClass->setStyleSheet("background-color: #27ae60; color: white; padding: 5px 15px; border-radius: 4px;");
    connect(btnCreateClass, &QPushButton::clicked, this, &ServerWindow::onCreateClassClicked);
    classLayout->addWidget(btnCreateClass);
    classLayout->addStretch();
    
    layout->addLayout(classLayout);
    
    QLabel *classHint = new QLabel("提示：创建班级后，学生端可在设置中加入该班级。");
    classHint->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 5px;");
    layout->addWidget(classHint);
    
    layout->addSpacing(20);
    
    // --- 【新增】删除班级区域 ---
    QLabel *deleteLbl = new QLabel("删除班级：");
    deleteLbl->setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
    layout->addWidget(deleteLbl);
    
    QHBoxLayout *deleteLayout = new QHBoxLayout();
    m_deleteClassCombo = new QComboBox();
    m_deleteClassCombo->setPlaceholderText("选择要删除的班级");
    m_deleteClassCombo->setMinimumWidth(200);
    // 初始化加载现有班级
    QList<ClassInfo> initClasses = DatabaseManager::instance().getAllClasses();
    for (const auto &cls : initClasses) {
        m_deleteClassCombo->addItem(cls.className);
    }
    deleteLayout->addWidget(m_deleteClassCombo);
    
    btnDeleteClass = new QPushButton("删除选中班级");
    btnDeleteClass->setStyleSheet("background-color: #e74c3c; color: white; padding: 5px 15px; border-radius: 4px;");
    connect(btnDeleteClass, &QPushButton::clicked, this, &ServerWindow::onDeleteClassClicked);
    deleteLayout->addWidget(btnDeleteClass);
    deleteLayout->addStretch();
    
    layout->addLayout(deleteLayout);
    
    QLabel *deleteHint = new QLabel("提示：删除班级后，原属于该班级的学生将变为“未分班”状态。");
    deleteHint->setStyleSheet("color: #7f8c8d; font-size: 12px; margin-top: 5px;");
    layout->addWidget(deleteHint);

    layout->addStretch();
    m_stackedWidget->addWidget(page);
}

void ServerWindow::onNavMonitorClicked() {
    btnMonitor->setChecked(true);
    btnFile->setChecked(false);
    btnSettings->setChecked(false);
    m_stackedWidget->setCurrentIndex(0);
    refreshMonitorTable(m_classFilterCombo->currentText()); // 切换回来时刷新当前筛选
}

void ServerWindow::onNavFileClicked() {
    btnMonitor->setChecked(false);
    btnFile->setChecked(true);
    btnSettings->setChecked(false);
    m_stackedWidget->setCurrentIndex(1);
}

void ServerWindow::onNavSettingsClicked() {
    btnMonitor->setChecked(false);
    btnFile->setChecked(false);
    btnSettings->setChecked(true);
    m_stackedWidget->setCurrentIndex(2);
}

void ServerWindow::onLogMessage(const QString &msg) {
    if (m_logView) {
        QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        m_logView->appendPlainText(QString("[%1] %2").arg(timeStr).arg(msg));
        m_logView->verticalScrollBar()->setValue(m_logView->verticalScrollBar()->maximum());
    }
}

// 【修改】更新表格行逻辑，增加详细调试日志和严格的筛选检查
void ServerWindow::updateStudentTableRow(const QString &id, const QString &name, StudentStatus status, const QString &app, const QByteArray &screenshot) {
    Q_UNUSED(screenshot);

    // 【调试】打印入口参数
    qDebug() << "[updateStudentTableRow] ENTER -> id:" << id << "name:" << name
             << "status:" << (int)status << "app:" << app
             << "m_currentFilterClass:" << m_currentFilterClass;

    // 1. 获取学生当前最新的班级信息
    QString studentClass = "未分班";
    if (g_onlineUsers.contains(id) && !g_onlineUsers[id].className.isEmpty()) {
        studentClass = g_onlineUsers[id].className;
    } else {
        StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
        if (!info.className.isEmpty()) {
            studentClass = info.className;
        }
    }

    // 2. 获取学生 IP 地址
    QString studentIp;
    if (g_onlineUsers.contains(id) && !g_onlineUsers[id].ip.isEmpty()) {
        studentIp = g_onlineUsers[id].ip;
    } else {
        StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
        if (!info.ip.isEmpty()) {
            studentIp = info.ip;
        }
    }
    if (studentIp.isEmpty()) {
        studentIp = "未知 IP";
    }

    qDebug() << "[updateStudentTableRow] Resolved -> IP:" << studentIp << "Class:" << studentClass;

    // 3. 【核心修复】筛选检查：增加对空字符串的判断，空字符串视为“全部班级”
    // 只有当筛选条件不为空 且 不为"全部班级" 且 学生班级不匹配时，才进行过滤
    if (!m_currentFilterClass.isEmpty() && m_currentFilterClass != "全部班级" && studentClass != m_currentFilterClass) {
        qDebug() << "[Filter] MISMATCH! Student class ('" << studentClass << "') != Filter ('" << m_currentFilterClass << "'). Removing/Hiding row for" << id;
        
        // 如果该行已存在，则删除它（因为切换了筛选条件，该学生不应再显示）
        if (m_studentRowMap.contains(id)) {
            int row = m_studentRowMap.take(id);
            m_monitorTable->removeRow(row);
            // 调整后续行索引
            for (auto it = m_studentRowMap.begin(); it != m_studentRowMap.end(); ++it) {
                if (it.value() > row) it.value()--;
            }
            qDebug() << "[Filter] Removed existing row" << row << "for student" << name;
        } else {
            qDebug() << "[Filter] Ignored adding new row for" << name << "due to filter mismatch.";
        }
        return; 
    }

    qDebug() << "[Filter] PASS. Proceeding to add/update row for" << id;

    // 4. 原有逻辑：添加或更新行
    int row = -1;
    if (m_studentRowMap.contains(id)) {
        row = m_studentRowMap[id];
        qDebug() << "[UI] Updating existing row" << row << "for" << id;
        
        QTableWidgetItem *nameItem = m_monitorTable->item(row, 1);
        if (nameItem && nameItem->text() != name) {
            nameItem->setText(name);
        }
        
        // 更新 IP 列
        QTableWidgetItem *ipItem = m_monitorTable->item(row, 0);
        if (ipItem && ipItem->text() != studentIp) {
            ipItem->setText(studentIp);
        }

        // 更新班级列
        QTableWidgetItem *classItem = m_monitorTable->item(row, 2);
        if (classItem && classItem->text() != studentClass) {
            classItem->setText(studentClass);
        }
    } else {
        row = m_monitorTable->rowCount();
        m_monitorTable->insertRow(row);
        m_studentRowMap[id] = row;
        qDebug() << "[UI] Created NEW row" << row << "for" << id;
        
        // 第 0 列：显示 IP，UserRole 存 ID
        QTableWidgetItem *ipItem = new QTableWidgetItem(studentIp);
        ipItem->setData(Qt::UserRole, id); 
        m_monitorTable->setItem(row, 0, ipItem);

        m_monitorTable->setItem(row, 1, new QTableWidgetItem(name));
        m_monitorTable->setItem(row, 2, new QTableWidgetItem(studentClass));
    }

    QTableWidgetItem *statusItem = new QTableWidgetItem();
    QTableWidgetItem *appItem = new QTableWidgetItem(); 
    
    switch (status) {
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
        case StudentStatus::Online_Warning:
            statusItem->setText("警告中");
            statusItem->setBackground(QBrush(Qt::yellow));
            statusItem->setForeground(QBrush(Qt::black));
            appItem->setText(app);
            break;
        case StudentStatus::Online_Violated:
            statusItem->setText("违规!");
            statusItem->setBackground(QBrush(Qt::red));
            statusItem->setForeground(QBrush(Qt::white));
            appItem->setText(app);
            break;
    }
    
    m_monitorTable->setItem(row, 3, statusItem);
    m_monitorTable->setItem(row, 4, appItem);

    // 第 5 列：违规次数
    int count = DatabaseManager::instance().getViolationCount(id);
    QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(count));
    countItem->setTextAlignment(Qt::AlignCenter);
    if (count > 0) {
        countItem->setForeground(QBrush(Qt::red));
        QFont font = countItem->font();
        font.setBold(true);
        countItem->setFont(font);
    }
    m_monitorTable->setItem(row, 5, countItem);

    // 第 6 列：查看屏幕
    QTableWidgetItem *actionItem = new QTableWidgetItem("🔍 查看屏幕");
    QColor bgColor("#e3f2fd");
    QColor textColor("#0d47a1");
    actionItem->setBackground(QBrush(bgColor));
    actionItem->setForeground(QBrush(textColor));
    QFont actionFont = actionItem->font();
    actionFont.setBold(true);
    actionFont.setUnderline(true);
    actionFont.setPointSize(actionFont.pointSize() + 1);
    actionItem->setFont(actionFont);
    actionItem->setTextAlignment(Qt::AlignCenter);
    actionItem->setToolTip("双击此行或点击此按钮查看实时屏幕");
    m_monitorTable->setItem(row, 6, actionItem);

    qDebug() << "[updateStudentTableRow] SUCCESS. Row" << row << "updated for" << id << "with Class:" << studentClass;
}

void ServerWindow::onStudentStatusUpdated(const QString &id, StudentStatus status, const QString &appName, const QByteArray &screenshot) {
    // 获取学生信息
    StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
    QString displayName = info.name;
    
    if (displayName.isEmpty()) {
        if (g_onlineUsers.contains(id)) {
            displayName = g_onlineUsers[id].nickName;
        } else if (id.contains(':')) {
            displayName = "学生_" + id.split(':').last(); 
        } else {
            displayName = "未知学生";
        }
        if (info.id.isEmpty()) {
            DatabaseManager::instance().registerStudent(id, displayName, id.split(':').first());
        }
    }

    qDebug() << "[Server Update] ID:" << id << "Status:" << (int)status << "App:" << appName << "HasScreenshot:" << !screenshot.isEmpty();

    bool isAutoViolation = (status == StudentStatus::Online_Violated && 
                            appName != "Periodic_Monitor" && 
                            appName != "Live_Request_Response" && 
                            appName != "Status_Recovered");

    if (isAutoViolation) {
        qDebug() << "[UI] Auto violation detected for" << displayName << ". Updating table ONLY. NO POPUP.";
        updateStudentTableRow(id, displayName, status, appName, screenshot); 
        updateOnlineStats();
        return; 
    }

    bool isLiveRequest = (appName == "Live_Request_Response");

    if (isLiveRequest && !screenshot.isEmpty()) {
        // 【核心修复】确保实时请求的截图能正确显示，并强制关闭加载对话框
        
        // 1. 【关键步骤】直接从映射中查找并关闭对应的加载对话框
        if (m_loadingDialogs.contains(id)) {
            QDialog *existingDlg = m_loadingDialogs.take(id); // 从 Map 中移除
            if (existingDlg && existingDlg->isVisible()) {
                existingDlg->accept(); // 关闭对话框
                existingDlg->deleteLater(); // 延迟删除
                qDebug() << "[UI] Loading dialog for" << displayName << "closed successfully (via signal).";
            } else if (existingDlg) {
                 existingDlg->deleteLater();
            }
        } else {
            // 如果映射中没有，尝试通过子对象查找（兜底策略）
            QList<QDialog*> dialogs = this->findChildren<QDialog*>();
            for (QDialog *dlg : dialogs) {
                if (dlg->windowTitle().contains("正在获取屏幕") && dlg->isVisible()) {
                    dlg->accept();
                    dlg->deleteLater();
                    qDebug() << "[UI] Loading dialog for" << displayName << "closed (via findChildren).";
                    break;
                }
            }
        }
        
        // 2. 更新底部预览区
        if (m_screenshotPreview) {
            QPixmap pix;
            if (pix.loadFromData(screenshot)) {
                m_screenshotPreview->setPixmap(pix.scaled(m_screenshotPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                m_screenshotPreview->setText("图片加载失败");
            }
            
            QString detailHtml = QString("<b>🖥️ 实时屏幕查看</b><br>"
                                         "学生：%1<br>"
                                         "时间：%2<br>"
                                         "ID: %3")
                                 .arg(displayName, QTime::currentTime().toString("hh:mm:ss"), id);
            m_detailLabel->setText(detailHtml);
            m_detailLabel->setStyleSheet("color: #2980b9; font-size: 14px;");
        }
        
        // 3. 弹窗显示大图 (稍微延迟以确保 UI 刷新和旧对话框销毁)
        QTimer::singleShot(100, this, [this, displayName, screenshot]() {
            showScreenshotDialog(displayName + " (实时查看)", screenshot);
        });
        
        return; // 实时请求处理完毕，不再执行下方的普通状态更新逻辑
    }

    if (status == StudentStatus::Online_Normal) {
        if (m_detailLabel->text().contains(displayName) && (m_detailLabel->text().contains("违规") || m_detailLabel->text().contains("实时"))) {
             m_detailLabel->setText(QString("学生 %1 当前状态正常。").arg(displayName));
             m_detailLabel->setStyleSheet("color: #27ae60; font-size: 14px;");
             if (m_screenshotPreview) m_screenshotPreview->clear();
        }
    }
    
    updateStudentTableRow(id, displayName, status, appName, screenshot);
    updateOnlineStats();
}

// 【新增】刷新监控表格，支持按班级过滤
void ServerWindow::refreshMonitorTable(const QString &filterClass) {
    // 【修复】处理空字符串情况，视为“全部班级”
    QString effectiveFilter = filterClass;
    if (effectiveFilter.isEmpty()) {
        effectiveFilter = "全部班级";
    }
    m_currentFilterClass = effectiveFilter;
    
    qDebug() << "[Refresh] Starting table refresh. Filter set to:'" << m_currentFilterClass << "'";
    
    m_monitorTable->setRowCount(0);
    m_studentRowMap.clear();
    
    // 第一步：获取所有学生（从数据库）
    QMap<QString, StudentInfo> dbMap = DatabaseManager::instance().getAllStudents();
    QList<StudentInfo> allStudents = dbMap.values();

    // 第二步：合并在线状态和最新班级信息（从 g_onlineUsers 覆盖）
    for (auto &stu : allStudents) {
        if (g_onlineUsers.contains(stu.id)) {
            const UserInfo &onlineUser = g_onlineUsers[stu.id];
            stu.isOnline = true;
            // 优先使用心跳包中的最新班级信息
            if (!onlineUser.className.isEmpty()) {
                stu.className = onlineUser.className;
            }
        } else {
            stu.isOnline = false;
        }
    }

    // 调试日志：打印所有待处理学生
    qDebug() << "[Refresh] Total students in DB:" << allStudents.size();
    for (const auto &stu : allStudents) {
        qDebug() << "  [Student]" << stu.name << "(" << stu.id << ") Online:" << stu.isOnline << "Class:'" << stu.className << "'";
    }

    // 第三步：根据筛选条件过滤
    if (!m_currentFilterClass.isEmpty() && m_currentFilterClass != "全部班级") {
        QList<StudentInfo> filtered;
        QString targetClass = m_currentFilterClass.trimmed();
        
        for (const StudentInfo &stu : allStudents) {
            // 严格匹配：学生班级不为空 且 等于目标班级
            if (!stu.className.trimmed().isEmpty() && stu.className.trimmed() == targetClass) {
                filtered.append(stu);
            } else {
                qDebug() << "[Refresh] Filtering OUT student" << stu.name << "because class'" << stu.className << "'!='"<< targetClass << "'";
            }
        }
        
        qDebug() << "[Refresh] Filtered count:" << filtered.size() << "for class:" << targetClass;
        allStudents = filtered;
    } else {
        qDebug() << "[Refresh] Showing all classes (Filter is 'All Classes' or empty).";
    }
    
    // 第四步：更新表格
    for (const StudentInfo &stu : allStudents) {
        StudentStatus status = stu.isOnline ? StudentStatus::Online_Normal : StudentStatus::Offline;
        // 注意：这里传入空的 app 和 screenshot，只更新基础信息和状态
        updateStudentTableRow(stu.id, stu.name, status, "", QByteArray());
    }
    
    updateOnlineStats();
    qDebug() << "[Refresh] Table refresh completed.";
}

void ServerWindow::updateOnlineStats() {
    QWidget *central = this->centralWidget();
    if (!central) return;

    QList<QLabel*> labels = central->findChildren<QLabel*>();
    for (QLabel *lbl : labels) {
        if (lbl->text().contains("在线人数")) {
            int onlineCount = 0;
            int warningCount = 0;
            for (int r = 0; r < m_monitorTable->rowCount(); ++r) {
                // 【修复】状态列索引现在是 3
                QTableWidgetItem *statusItem = m_monitorTable->item(r, 3); 
                if (statusItem && statusItem->text() != "离线") {
                    onlineCount++;
                    if (statusItem->text().contains("违规") || statusItem->text().contains("警告")) {
                        warningCount++;
                    }
                }
            }
            lbl->setText(QString("在线人数：%1 | 违规警告：%2").arg(onlineCount).arg(warningCount));
            break;
        }
    }
}

void ServerWindow::onTableCellDoubleClicked(int row, int col) {
    // 【核心修改】仅当双击的是第 6 列（“查看屏幕”列）时，才触发截图请求
    if (col != 6) {
        return;
    }

    // 【修改】从第 0 列的 UserRole 中获取真实的学生 ID，而不是读取显示的 IP 文本
    QTableWidgetItem *ipItem = m_monitorTable->item(row, 0);
    if (!ipItem) {
        return;
    }
    QString studentId = ipItem->data(Qt::UserRole).toString();
    if (studentId.isEmpty()) {
        qWarning() << "[Error] Cannot find student ID in UserRole for row" << row;
        return;
    }

    onRequestLiveScreenshotClicked(studentId);
}

void ServerWindow::showScreenshotDialog(const QString &studentName, const QByteArray &imageData)
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(QString("屏幕截图 - %1").arg(studentName));
    
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    int width = screenRect.width() * 2 / 3;
    int height = screenRect.height() * 2 / 3;
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);

    QLabel *infoLabel = new QLabel(QString("<b>学生：</b>%1<br><b>时间：</b>%2").arg(studentName, QTime::currentTime().toString("hh:mm:ss")));
    infoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(infoLabel);

    QLabel *imgLabel = new QLabel();
    imgLabel->setFixedSize(width, height);
    imgLabel->setAlignment(Qt::AlignCenter);
    imgLabel->setStyleSheet("border: 1px solid #ccc; background: #fff;");

    QPixmap pix;
    if (pix.loadFromData(imageData)) {
        imgLabel->setPixmap(pix.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imgLabel->setText("图片数据损坏或无法解析");
        imgLabel->setAlignment(Qt::AlignCenter);
        imgLabel->setStyleSheet("color: red; font-weight: bold;");
    }

    layout->addWidget(imgLabel);

    QPushButton *closeBtn = new QPushButton("关闭");
    closeBtn->setFixedHeight(40);
    closeBtn->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px;");
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg->resize(width + 20, height + 80);
    dlg->exec();
    dlg->deleteLater();
}

void ServerWindow::onUdpReadyRead()
{
    bool userListChanged = false; 

    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderIp;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);

        if (senderIp.toString() == m_myIp && senderPort == m_myUdpPort) continue;

        QString content = QString::fromUtf8(datagram);
        QStringList parts = content.split('|');

        if (parts.size() >= 6 && parts[0] == "HEARTBEAT") {
            QString nick = parts[1];
            bool isTeacher = (parts[2] == "1");
            
            if (isTeacher) continue; 

            // 【修复】获取 IP 并增加容错逻辑
            QString ip = parts[3];
            if (ip.isEmpty() || ip == "127.0.0.1" || ip.startsWith("127.")) {
                qDebug() << "[UDP] ⚠️ Heartbeat IP invalid ('" << ip << "'), using UDP sender IP:" << senderIp.toString();
                ip = senderIp.toString();
            }
            
            quint16 uPort = parts[4].toUShort();
            quint16 tPort = parts[5].toUShort();

            if (tPort == 0) {
                qWarning() << "[UDP] ⚠️ Received heartbeat with TCP Port 0 from" << nick << "(" << ip << "). Screen view may fail.";
            } else {
                qDebug() << "[UDP] ✅ Valid Heartbeat from" << nick << "- IP:" << ip << "TCP Port:" << tPort;
            }

            // 解析班级信息
            QString className = "";
            if (parts.size() >= 7) {
                className = parts[6].trimmed();
                if (className == "请选择班级..." || className.isEmpty()) {
                    className = "";
                }
                qDebug() << "[Heartbeat] Student" << nick << "(" << ip << ") class raw:" << parts[6] << "-> cleaned:" << className;
            } else {
                qDebug() << "[Heartbeat] Student" << nick << "heartbeat missing class field.";
            }

            // 获取固定学生 ID
            QString studentId = "";
            if (parts.size() >= 8) {
                studentId = parts[7].trimmed();
                qDebug() << "[Heartbeat] Received fixed StudentId:" << studentId;
            }
            
            if (studentId.isEmpty()) {
                studentId = QString("%1:%2").arg(ip).arg(uPort);
                qDebug() << "[Heartbeat] No fixed ID found, using fallback ID:" << studentId;
            }

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
            
            // 【调试】打印旧班级信息以便对比
            if (!isNewUser) {
                QString oldClass = g_onlineUsers[studentId].className;
                QString oldNick = g_onlineUsers[studentId].nickName;
                QString oldIp = g_onlineUsers[studentId].ip;
                
                if (oldClass != className || oldNick != nick || oldIp != ip) {
                    qDebug() << "[Sync] Student" << studentId << "info changed -> Nick:" << oldNick << "->" << nick 
                             << ", Class:" << oldClass << "->" << className 
                             << ", IP:" << oldIp << "->" << ip;
                }
            } else {
                userListChanged = true;
                qDebug() << "[Sync] New student detected:" << studentId << "(" << nick << ") Class:" << className;
            }

            g_onlineUsers[studentId] = user;

            QString displayName = nick; 
            
            StudentInfo info = DatabaseManager::instance().getStudentInfo(studentId);
            if (info.id.isEmpty()) {
                DatabaseManager::instance().registerStudent(studentId, displayName, ip);
                onLogMessage(QString("[DB] 新学生注册：%1 (ID:%2)").arg(displayName, studentId));
            }
            
            // 如果心跳包带了班级，且与数据库中记录不同，则立即更新数据库
            if (!className.isEmpty() && info.className != className) {
                bool updateOk = DatabaseManager::instance().updateStudentClass(studentId, className);
                if (updateOk) {
                    onLogMessage(QString("[同步] ✅ 学生 %1 成功更新/加入班级：%2").arg(displayName, className));
                } else {
                    onLogMessage(QString("[同步] ❌ 学生 %1 更新班级失败：%2").arg(displayName, className));
                }
            }

            // 【核心修复】无论是否为新用户，只要行存在，就强制更新显示内容（特别是班级和 IP）
            if (m_studentRowMap.contains(studentId)) {
                int row = m_studentRowMap[studentId];
                
                // 1. 更新姓名列
                QTableWidgetItem *nameItem = m_monitorTable->item(row, 1);
                if (nameItem && nameItem->text() != nick) {
                    nameItem->setText(nick);
                    // 如果昵称变了，也同步更新数据库
                    if (info.name != nick) {
                         DatabaseManager::instance().registerStudent(studentId, nick, ip);
                    }
                    qDebug() << "[UI] Updated name for" << studentId << "to" << nick;
                }

                // 2. 强制更新班级列 (解决不同步问题的关键)
                QTableWidgetItem *classItem = m_monitorTable->item(row, 2);
                if (classItem) {
                    QString displayClass = className.isEmpty() ? "未分班" : className;
                    if (classItem->text() != displayClass) {
                        classItem->setText(displayClass);
                        qDebug() << "[UI] Forced update class for" << studentId << "to" << displayClass << "(Was:" << classItem->text() << ")";
                    }
                }

                // 3. 强制更新 IP 列
                QTableWidgetItem *ipItem = m_monitorTable->item(row, 0);
                if (ipItem && ipItem->text() != ip) {
                    ipItem->setText(ip);
                    qDebug() << "[UI] Forced update IP for" << studentId << "to" << ip;
                }
                
                // 【调试】检查当前筛选条件，确认该行是否应该可见
                qDebug() << "[Debug] Row" << row << "exists for" << studentId 
                         << ". CurrentFilter:" << m_currentFilterClass 
                         << ". StudentClass:" << (className.isEmpty() ? "未分班" : className);
                         
                // 注意：这里不再调用 updateStudentTableRow 来避免潜在的过滤逻辑干扰，
                // 而是直接修改单元格文本。如果筛选条件导致该行本应被删除，
                // 那么在下一次 refreshMonitorTable 或状态变更时会处理。
                // 但如果只是班级变了，而筛选是"全部班级"或匹配的班级，直接更新是最安全的。
                
            } else {
                // 新用户，创建行
                qDebug() << "[UI] Adding new row for student:" << studentId << "Class:" << className;
                updateStudentTableRow(studentId, displayName, StudentStatus::Online_Normal, "", QByteArray());
            }
        }
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> toRemove;
    for (auto it = g_onlineUsers.begin(); it != g_onlineUsers.end(); ++it) {
        if (now - it->lastHeartbeat > 10000) { 
            toRemove.append(it.key());
        }
    }

    for (const QString &id : toRemove) {
        g_onlineUsers.remove(id);
        
        if (m_studentRowMap.contains(id)) {
            int row = m_studentRowMap[id];
            onLogMessage(QString("[监控] 学生 %1 下线 (超时)，已从列表移除").arg(id));
            
            m_monitorTable->removeRow(row);
            m_studentRowMap.remove(id);
            
            for (auto it = m_studentRowMap.begin(); it != m_studentRowMap.end(); ++it) {
                if (it.value() > row) {
                    it.value()--;
                }
            }
            userListChanged = true;
            updateOnlineStats();
        }
    }
    
    if (userListChanged && toRemove.isEmpty()) {
         updateOnlineStats();
    }
}

void ServerWindow::broadcastUserList()
{
    QJsonArray userList;
    
    for (auto it = g_onlineUsers.begin(); it != g_onlineUsers.end(); ++it) {
        QJsonObject obj;
        obj["id"] = it->id;
        obj["nickName"] = it->nickName;
        obj["isTeacher"] = it->isTeacher;
        obj["ip"] = it->ip;
        obj["udpPort"] = it->port;
        obj["tcpPort"] = it->tcpPort;
        obj["lastHeartbeat"] = it->lastHeartbeat;
        userList.append(obj);
    }

    // 【新增】获取数据库中所有存在的班级，一并广播给学生端
    QJsonArray classList;
    QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
    
    // 【修复】增加日志，确认教师端确实查到了班级
    qDebug() << "[Teacher Broadcast] Found" << classes.size() << "classes in DB.";
    for (const auto &cls : classes) {
        classList.append(cls.className);
        qDebug() << "[Teacher Broadcast]   - Class:" << cls.className;
    }

    QJsonDocument doc(userList);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    // 【修改】构建包含班级列表的消息格式：USER_LIST|{"users":[...], "classes":[...]}
    QJsonObject rootObj;
    rootObj["users"] = userList;
    rootObj["classes"] = classList;
    
    QJsonDocument finalDoc(rootObj);
    QByteArray finalData = finalDoc.toJson(QJsonDocument::Compact);

    // 【修复】将以下逻辑移入函数体内，确保能访问 finalData 和 m_udpSocket
    QString msg = QString("USER_LIST|%1").arg(QString(finalData));
    QByteArray data = msg.toUtf8();
    
    // 【修复】增加发送日志
    qDebug() << "[Teacher Broadcast] Sending USER_LIST packet size:" << data.size() << "bytes.";
    qint64 sent = m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, STUDENT_LISTEN_PORT);
    if (sent == -1) {
        qWarning() << "[Teacher Broadcast] Failed to send broadcast:" << m_udpSocket->errorString();
    }
}

void ServerWindow::sendServerHeartbeat()
{
    QString msg = QString("HEARTBEAT|%1|%2|%3|%4|%5")
                  .arg("教师端")
                  .arg("1") 
                  .arg(m_myIp)
                  .arg(m_myUdpPort)
                  .arg(m_myTcpPort);
    
    QByteArray data = msg.toUtf8();
    m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, STUDENT_LISTEN_PORT);

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> toRemove;
    for (auto it = g_onlineUsers.begin(); it != g_onlineUsers.end(); ++it) {
        if (now - it->lastHeartbeat > 10000) { 
            toRemove.append(it.key());
        }
    }
    for (const QString &id : toRemove) {
        if (g_onlineUsers.contains(id)) { 
            g_onlineUsers.remove(id);
            if (m_studentRowMap.contains(id)) {
                int row = m_studentRowMap[id];
                onLogMessage(QString("[定时清理] 学生 %1 下线，已从列表移除").arg(id));
                m_monitorTable->removeRow(row);
                m_studentRowMap.remove(id);
                for (auto it = m_studentRowMap.begin(); it != m_studentRowMap.end(); ++it) {
                    if (it.value() > row) it.value()--;
                }
            }
        }
    }
    
    // 【修复】删除此处原本错误的多余代码块和孤立语句
    updateOnlineStats();
}

void ServerWindow::onNewFileConnection()
{
    QTcpSocket *clientSocket = m_fileServer->nextPendingConnection();
    
    qDebug() << "[FileServer] New connection received from:" << clientSocket->peerAddress().toString();
             
    onLogMessage(QString("[文件服务] 新连接来自：%1").arg(clientSocket->peerAddress().toString()));
    
    struct MonitorReceiveContext {
        enum State { WaitHeader, WaitJsonLen, WaitJsonData, WaitImgLen, WaitImgData } state = WaitHeader;
        quint32 jsonSize = 0;
        quint32 imgSize = 0;
        QByteArray jsonBuffer;
        QByteArray imgBuffer;
        QString pendingStudentId; 
    };
    
    auto ctx = std::make_shared<MonitorReceiveContext>();
    QPointer<QTcpSocket> safeSocket(clientSocket);

    connect(clientSocket, &QTcpSocket::readyRead, this, [this, safeSocket, ctx]() {
        if (!safeSocket) return;
        QTcpSocket *socket = safeSocket.data();

        // 【修复】添加标签，供 MONITOR_START 命令处理后跳转至此，继续处理二进制数据
        process_binary:

        if (ctx->state != MonitorReceiveContext::WaitHeader) {
            while (socket->bytesAvailable() > 0) {
                if (ctx->state == MonitorReceiveContext::WaitJsonLen) {
                    if (socket->bytesAvailable() >= 4) {
                        QByteArray h = socket->read(4);
                        QDataStream ds(&h, QIODevice::ReadOnly);
                        ds.setVersion(QDataStream::Qt_5_15);
                        quint32 len;
                        ds >> len;
                        if (len > 10 * 1024 * 1024) { qWarning() << "Invalid JSON len"; socket->disconnectFromHost(); return; }
                        ctx->jsonSize = len;
                        ctx->state = MonitorReceiveContext::WaitJsonData;
                    } else { break; }
                }
                
                if (ctx->state == MonitorReceiveContext::WaitJsonData) {
                    if (socket->bytesAvailable() >= (int)ctx->jsonSize) {
                        ctx->jsonBuffer = socket->read(ctx->jsonSize);
                        ctx->state = MonitorReceiveContext::WaitImgLen;
                    } else { break; }
                }

                if (ctx->state == MonitorReceiveContext::WaitImgLen) {
                    if (socket->bytesAvailable() >= 4) {
                        QByteArray h = socket->read(4);
                        QDataStream ds(&h, QIODevice::ReadOnly);
                        ds.setVersion(QDataStream::Qt_5_15);
                        quint32 len;
                        ds >> len;
                        if (len > 50 * 1024 * 1024) { qWarning() << "Invalid Img len"; socket->disconnectFromHost(); return; }
                        ctx->imgSize = len;
                        ctx->state = MonitorReceiveContext::WaitImgData;
                    } else { break; }
                }

                if (ctx->state == MonitorReceiveContext::WaitImgData) {
                    if (socket->bytesAvailable() >= (int)ctx->imgSize) {
                        ctx->imgBuffer = socket->read(ctx->imgSize);
                        
                        onLogMessage(QString("[监控] 收到完整数据包，JSON:%1 bytes, Img:%2 bytes").arg(ctx->jsonSize).arg(ctx->imgSize));
                        
                        QJsonParseError error;
                        QJsonDocument doc = QJsonDocument::fromJson(ctx->jsonBuffer, &error);
                        if (error.error == QJsonParseError::NoError && doc.isObject()) {
                            QJsonObject obj = doc.object();
                            QString studentId = obj["studentId"].toString();
                            QString appName = obj["appName"].toString();
                            QString type = obj["type"].toString();
                            QString timeStr = obj["time"].toString();

                            // 【新增】从 TCP 包中提取备用信息 (昵称、班级、TCP 端口)
                            QString reportedNick = obj["nickName"].toString();
                            QString reportedClass = obj["className"].toString();
                            quint16 reportedTcpPort = 0;
                            if (obj.contains("tcpPort")) {
                                reportedTcpPort = static_cast<quint16>(obj["tcpPort"].toInt());
                            }

                            // 【新增】从 Socket 获取对端 IP (处理 IPv6 映射)
                            QString peerIp = socket->peerAddress().toString();
                            if (peerIp.startsWith("::ffff:")) {
                                peerIp = peerIp.mid(7);
                            }

                            QString displayName;
                            qint64 now = QDateTime::currentMSecsSinceEpoch(); // 【新增】获取当前时间戳

                            // 【核心修复】如果 g_onlineUsers 中没有该学生，利用 TCP 信息创建临时记录
                            if (!g_onlineUsers.contains(studentId)) {
                                UserInfo tempUser;
                                tempUser.id = studentId;
                                tempUser.ip = peerIp; // 使用 TCP 源 IP
                                tempUser.tcpPort = reportedTcpPort; // 【修复】使用上报的 TCP 端口
                                tempUser.port = 0;
                                tempUser.nickName = reportedNick.isEmpty() ? ("未知_" + studentId.right(4)) : reportedNick;
                                tempUser.className = reportedClass;
                                tempUser.isTeacher = false;
                                tempUser.lastHeartbeat = now; // 【关键修复】初始化心跳时间为当前时间，防止立即被清理
                                
                                g_onlineUsers[studentId] = tempUser;
                                
                                // 同步到数据库
                                StudentInfo dbInfo = DatabaseManager::instance().getStudentInfo(studentId);
                                if (dbInfo.id.isEmpty()) {
                                    DatabaseManager::instance().registerStudent(studentId, tempUser.nickName, peerIp);
                                    if (!reportedClass.isEmpty()) {
                                        DatabaseManager::instance().updateStudentClass(studentId, reportedClass);
                                    }
                                    onLogMessage(QString("[TCP 补录] 新学生注册：%1 (IP:%2, 班级:%3, TCP 端口:%4)").arg(tempUser.nickName, peerIp, reportedClass).arg(reportedTcpPort));
                                } else {
                                    // 更新现有记录的 IP、班级和端口
                                    DatabaseManager::instance().registerStudent(studentId, dbInfo.name, peerIp);
                                    if (!reportedClass.isEmpty() && dbInfo.className != reportedClass) {
                                        DatabaseManager::instance().updateStudentClass(studentId, reportedClass);
                                    }
                                    onLogMessage(QString("[TCP 补录] 更新学生 %1 信息 (IP:%2, 班级:%3, TCP 端口:%4)").arg(dbInfo.name, peerIp, reportedClass).arg(reportedTcpPort));
                                }
                                
                                displayName = tempUser.nickName;
                            } else {
                                if (g_onlineUsers.contains(studentId)) {
                                    displayName = g_onlineUsers[studentId].nickName;
                                    
                                    // 【关键修复】无论是否有信息变化，只要收到 TCP 包，就更新 lastHeartbeat
                                    // 这能防止因 UDP 心跳丢失导致学生被超时清理机制移除
                                    g_onlineUsers[studentId].lastHeartbeat = now;
                                    
                                    // 【修复】即使学生已存在，也要更新可能缺失或变化的 TCP 端口
                                    if (reportedTcpPort != 0 && g_onlineUsers[studentId].tcpPort != reportedTcpPort) {
                                        g_onlineUsers[studentId].tcpPort = reportedTcpPort;
                                        qDebug() << "[TCP 补录] 更新在线学生" << studentId << "的 TCP 端口为:" << reportedTcpPort;
                                    }
                                    
                                    // 如果 TCP 包里有更新的班级信息，也尝试更新
                                    if (!reportedClass.isEmpty() && g_onlineUsers[studentId].className != reportedClass) {
                                        g_onlineUsers[studentId].className = reportedClass;
                                        DatabaseManager::instance().updateStudentClass(studentId, reportedClass);
                                    }
                                    // 如果之前 IP 为空，现在补上
                                    if (g_onlineUsers[studentId].ip.isEmpty() && !peerIp.isEmpty()) {
                                        g_onlineUsers[studentId].ip = peerIp;
                                    }
                                    // 更新昵称
                                    if (!reportedNick.isEmpty() && g_onlineUsers[studentId].nickName != reportedNick) {
                                        g_onlineUsers[studentId].nickName = reportedNick;
                                    }
                                } else {
                                    displayName = "未知学生";
                                }
                            }

                            if (type == "STATUS_RECOVERY") {
                                m_lastRecoverTime[studentId] = now;
                                updateStudentTableRow(studentId, displayName, StudentStatus::Online_Normal, "", QByteArray());
                                onLogMessage(QString("[监控] 学生 %1 已恢复正常").arg(displayName));
                            } 
                            else if (type == "VIOLATION_REPORT") {
                                qint64 lastRecover = m_lastRecoverTime.value(studentId, 0);
                                
                                if (lastRecover > 0 && (now - lastRecover) < 10000) {
                                    onLogMessage(QString("[忽略] 学生 %1 在冷却期内违规（距恢复 %2ms），不计入次数").arg(displayName).arg(now - lastRecover));
                                } else {
                                    onLogMessage(QString("[警报] 学生 %1 违规使用：%2").arg(displayName, appName));
                                    processViolationReport(studentId, appName, ctx->imgBuffer, timeStr);
                                    updateStudentTableRow(studentId, displayName, StudentStatus::Online_Violated, appName, ctx->imgBuffer);
                                    updateOnlineStats();
                                }
                            }
                            else if (type == "LIVE_RESPONSE") {
                                updateStudentTableRow(studentId, displayName, StudentStatus::Online_Normal, appName, ctx->imgBuffer);
                                
                                if (m_loadingDialogs.contains(studentId)) {
                                    QDialog *loadingDlg = m_loadingDialogs.take(studentId);
                                    if (loadingDlg && loadingDlg->isVisible()) {
                                        loadingDlg->accept();
                                        qDebug() << "[UI] Loading dialog for" << displayName << "closed successfully.";
                                    }
                                    if (loadingDlg) {
                                        loadingDlg->deleteLater();
                                    }
                                } else {
                                    qDebug() << "[UI] Warning: No loading dialog found in map for" << studentId;
                                }

                                if (!ctx->imgBuffer.isEmpty()) {
                                    QTimer::singleShot(50, this, [this, displayName, img = ctx->imgBuffer]() {
                                        showScreenshotDialog(displayName + " (实时查看)", img);
                                    });
                                }
                            }
                            else if (type == "PERIODIC_MONITOR") {
                                onLogMessage(QString("[监控] 收到 %1 的定期巡查截图").arg(displayName));
                                if (!ctx->imgBuffer.isEmpty()) {
                                    DatabaseManager::instance().insertMonitorRecord(studentId, "Periodic_Monitor", "定期巡查", ctx->imgBuffer);
                                }
                                // 【新增】定期巡查也刷新表格，确保学生可见（状态设为正常）
                                updateStudentTableRow(studentId, displayName, StudentStatus::Online_Normal, "", QByteArray());
                            }
                        }
                        
                        ctx->state = MonitorReceiveContext::WaitHeader;
                        ctx->jsonBuffer.clear();
                        ctx->imgBuffer.clear();
                        ctx->pendingStudentId.clear();
                        ctx->jsonSize = 0;
                        ctx->imgSize = 0;
                        
                        return; 
                    } else { break; }
                }
            }
            return; 
        }

        if (socket->bytesAvailable() >= (int)sizeof(quint32)) {
             QByteArray header = socket->read(4);
             QDataStream ds(&header, QIODevice::ReadOnly);
             quint32 len;
             ds >> len;
             
             if (socket->bytesAvailable() >= len) {
                 QByteArray cmdData = socket->read(len);
                 QString cmd = QString::fromUtf8(cmdData);
                 
                 // 【修复】检测到 MONITOR_START 后，设置状态并跳转到二进制处理逻辑，不再 return
                 if (cmd.startsWith("MONITOR_START|")) {
                     QStringList parts = cmd.mid(14).split('|');
                     if (parts.size() == 2) {
                         ctx->jsonSize = parts[0].toUInt();
                         ctx->imgSize = parts[1].toUInt();
                         ctx->state = MonitorReceiveContext::WaitJsonLen;
                     }
                     goto process_binary; // 关键修复：立即处理后续数据
                 }
                 
                 handlePeerCommand(cmdData, socket);
             } else {
                 socket->waitForReadyRead(2000);
                 if (socket->bytesAvailable() >= len) {
                     QByteArray cmdData = socket->read(len);
                     QString cmd = QString::fromUtf8(cmdData);
                     
                     if (cmd.startsWith("MONITOR_START|")) {
                         QStringList parts = cmd.mid(14).split('|');
                         if (parts.size() == 2) {
                             ctx->jsonSize = parts[0].toUInt();
                             ctx->imgSize = parts[1].toUInt();
                             ctx->state = MonitorReceiveContext::WaitJsonLen;
                         }
                         goto process_binary; // 关键修复：立即处理后续数据
                     }
                     
                     handlePeerCommand(cmdData, socket);
                 }
             }
        }
    });
    connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
}

void ServerWindow::handlePeerCommand(const QByteArray &data, QTcpSocket *socket)
{
    QString cmd = QString::fromUtf8(data);

    QString shareRoot = "E:/fileShared";
    if (m_sharePathEdit && !m_sharePathEdit->text().isEmpty()) {
        shareRoot = m_sharePathEdit->text();
    }

    if (cmd == "GET_FILE_LIST" || cmd.startsWith("LIST|")) {
        QString pathArg = "/";
        if (cmd.startsWith("LIST|")) {
            pathArg = cmd.mid(5);
        }

        QString physicalPath = shareRoot;
        if (!pathArg.isEmpty() && pathArg != "/") {
            QString relative = pathArg;
            if (relative.startsWith("/")) relative = relative.mid(1);
            relative = QDir::fromNativeSeparators(relative);
            QDir root(shareRoot);
            physicalPath = root.filePath(relative);
        }
        physicalPath = QDir::cleanPath(physicalPath);

        if (!physicalPath.startsWith(shareRoot)) {
             qWarning() << "[Security] Blocked directory traversal attempt:" << physicalPath;
             QJsonArray emptyArray;
             QJsonDocument doc(emptyArray);
             QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
             QByteArray block;
             QDataStream out(&block, QIODevice::WriteOnly);
             out << (quint32)jsonData.size();
             out.writeRawData(jsonData.constData(), jsonData.size());
             socket->write(block);
             return;
        }

        QDir dir(physicalPath);
        if (!dir.exists()) {
            if (physicalPath == shareRoot) {
                if (!dir.mkpath(shareRoot)) {
                     onLogMessage(QString("[错误] 无法创建共享根目录：%1").arg(shareRoot));
                }
            } else {
                 QJsonArray emptyArray;
                 QJsonDocument doc(emptyArray);
                 QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                 QByteArray block;
                 QDataStream out(&block, QIODevice::WriteOnly);
                 out << (quint32)jsonData.size();
                 out.writeRawData(jsonData.constData(), jsonData.size());
                 socket->write(block);
                 return;
            }
        }
        
        QJsonArray array;
        QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst);
        for (const QFileInfo &fi : list) {
            QJsonObject obj;
            obj["name"] = fi.fileName();
            obj["isDir"] = fi.isDir();
            obj["size"] = (qint64)fi.size();
            
            if (!fi.isDir()) {
                QString key = pathArg;
                if (!key.endsWith("/")) key += "/";
                key += fi.fileName();
                key.replace("\\", "/");
                key = QDir::cleanPath(key);
                if (key.startsWith("//")) key = key.mid(1);
                if (!key.startsWith("/")) key = "/" + key;

                FileStatRecord stats = DatabaseManager::instance().getFileStat(key);
                obj["downloadCount"] = stats.downloadCount;
                obj["sourceIp"] = stats.lastSourceIp;
            } else {
                obj["downloadCount"] = 0;
                obj["sourceIp"] = "-";
            }
            
            array.append(obj);
        }
        
        QJsonDocument doc(array);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out << (quint32)jsonData.size();
        out.writeRawData(jsonData.constData(), jsonData.size());
        socket->write(block);
        return;
    }

    if (cmd.startsWith("DOWNLOAD|")) {
        QString args = cmd.mid(9);
        int splitIndex = args.indexOf('|');
        QString pathArg = "/";
        QString fileName = args;

        if (splitIndex != -1) {
            pathArg = args.left(splitIndex);
            fileName = args.mid(splitIndex + 1);
        }

        QString physicalPath = shareRoot;
        if (!pathArg.isEmpty() && pathArg != "/") {
            QString relative = pathArg;
            if (relative.startsWith("/")) relative = relative.mid(1);
            relative = QDir::fromNativeSeparators(relative);
            QDir root(shareRoot);
            physicalPath = root.filePath(relative);
        }
        physicalPath = QDir::cleanPath(physicalPath);
        
        QString filePath = QDir(physicalPath).filePath(fileName);

        QFile file(filePath);
        
        QJsonObject meta;
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            meta["name"] = fileName;
            meta["size"] = (qint64)file.size();
            meta["status"] = "ok";
            
            QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);
            
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out << (quint32)metaBytes.size();
            socket->write(block);
            socket->write(metaBytes);
            
            QByteArray fileData = file.readAll();
            file.close();
            
            int chunkSize = 8192;
            for (int i = 0; i < fileData.size(); i += chunkSize) {
                socket->write(fileData.mid(i, chunkSize));
            }
            
            QString statsKey = pathArg;
            if (!statsKey.endsWith("/")) statsKey += "/";
            statsKey += fileName;
            statsKey.replace("\\", "/");
            statsKey = QDir::cleanPath(statsKey);
            if (statsKey.startsWith("//")) statsKey = statsKey.mid(1);
            // 【修复】将错误的变量名 'key' 修正为 'statsKey'
            if (!statsKey.startsWith("/")) statsKey = "/" + statsKey;
            
            QString clientIp = socket->peerAddress().toString();
            
            bool updateOk = DatabaseManager::instance().updateFileStat(statsKey, clientIp);
            onLogMessage(QString("[下载] 学生下载了文件：%1 (大小:%2) [DB Key:%3] [更新结果:%4]")
                         .arg(fileName).arg(fileData.size()).arg(statsKey).arg(updateOk ? "成功" : "失败"));
            
        } else {
            meta["status"] = "error";
            meta["msg"] = "File not found: " + filePath;
            QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out << (quint32)metaBytes.size();
            socket->write(block);
            socket->write(metaBytes);
            onLogMessage(QString("[下载] 文件未找到：%1").arg(filePath));
        }
    }
    // 【修复】添加缺失的闭合大括号，结束 handlePeerCommand 函数
}

ServerWindow::~ServerWindow()
{
    if (m_server) m_server->stop();
    if (m_fileServer) m_fileServer->close();
    delete ui;
}

void ServerWindow::closeEvent(QCloseEvent *event)
{
    if (m_server && m_server->isListening()) {
        int ret = QMessageBox::question(this, "确认退出", 
                                        "服务器正在运行，确定要关闭服务器并退出程序吗？",
                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::No) {
            event->ignore();
            return;
        }
        m_server->stop();
    }
    event->accept();
}

void ServerWindow::onRequestLiveScreenshotClicked(const QString &studentId) {
    StudentInfo info = DatabaseManager::instance().getStudentInfo(studentId);
    QString displayName = info.name;
    if (displayName.isEmpty()) {
        if (g_onlineUsers.contains(studentId)) {
            displayName = g_onlineUsers[studentId].nickName;
        } else {
            displayName = "未知学生";
        }
    }

    onLogMessage(QString("[监控] 正在向 %1 请求实时屏幕...").arg(displayName));
    
    // 【修复】先清理可能存在的旧加载框（防止重复点击导致多个加载框）
    if (m_loadingDialogs.contains(studentId)) {
        QDialog *oldDlg = m_loadingDialogs.take(studentId);
        if (oldDlg) {
            oldDlg->close();
            oldDlg->deleteLater();
        }
    }

    // 创建加载对话框
    QDialog *loadingDlg = showLoadingDialog(displayName);
    
    // 将对话框存入 Map，键为学生 ID，以便收到数据后能找到并关闭它
    m_loadingDialogs[studentId] = loadingDlg;

    if (g_onlineUsers.contains(studentId)) {
        UserInfo user = g_onlineUsers[studentId];
        sendGetScreenshotCommand(user.ip, user.tcpPort);
        
        // 可选：添加一个超时机制，如果 5 秒没反应也关闭加载框，避免死等
        // 【修复】超时回调中再次确认并清理 Map
        QTimer::singleShot(8000, this, [this, studentId, loadingDlg]() {
            if (m_loadingDialogs.contains(studentId)) {
                QDialog *dlg = m_loadingDialogs.take(studentId);
                if (dlg && dlg->isVisible()) {
                    dlg->reject();
                    dlg->deleteLater();
                    onLogMessage(QString("[监控] 请求 %1 实时屏幕超时").arg(studentId));
                    // 注意：这里不弹 Warning，避免干扰用户，只在日志记录
                }
            }
        });
    } else {
        onLogMessage(QString("[错误] 无法找到学生 %1 的网络信息，无法发送请求").arg(studentId));
        // 清理 Map
        if (m_loadingDialogs.contains(studentId)) {
            QDialog *dlg = m_loadingDialogs.take(studentId);
            if (dlg) {
                dlg->reject();
                dlg->deleteLater();
            }
        }
        QMessageBox::warning(this, "错误", "无法获取学生网络信息，请确认学生在线。");
    }
}

QDialog* ServerWindow::showLoadingDialog(const QString &studentName) {
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("正在获取屏幕...");
    dlg->setModal(true);
    dlg->setFixedSize(300, 150);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setAlignment(Qt::AlignCenter);
    
    QLabel *infoLabel = new QLabel(QString("正在请求 <b>%1</b> 的实时屏幕...\n请稍候").arg(studentName));
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);
    
    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 0);
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(10);
    progressBar->setStyleSheet(R"(
        QProgressBar {
            border: 1px solid #ccc;
            border-radius: 5px;
            background: white;
        }
        QProgressBar::chunk {
            background-color: #3498db;
            width: 20px;
        }
    )");
    layout->addWidget(progressBar);
    
    dlg->show();
    return dlg;
}

void ServerWindow::sendGetScreenshotCommand(const QString &studentIp, quint16 studentTcpPort) {
    QTcpSocket *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, [socket]() {
        QString cmd = "GET_SCREENSHOT_NOW";
        QByteArray data = cmd.toUtf8();
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_15);
        out << (quint32)data.size();
        out.writeRawData(data.constData(), data.size());
        socket->write(block);
        socket->disconnectFromHost();
    });
    connect(socket, &QTcpSocket::errorOccurred, [socket](QAbstractSocket::SocketError /*err*/) {
        qDebug() << "[Command] Failed to send screenshot request:" << socket->errorString();
        socket->deleteLater();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    
    socket->connectToHost(studentIp, studentTcpPort);
}

void ServerWindow::processViolationReport(const QString &studentId, const QString &appName, 
                                          const QByteArray &imageData, const QString &timeStr) {
    QString details = QString("自动检测违规应用：%1 时间：%2").arg(appName, timeStr);
    
    // 无论是否有图片数据，都插入记录
    // 如果是定期巡查或特定类型，图片可能为空，但日志仍需记录
    if (!imageData.isEmpty()) {
        DatabaseManager::instance().insertMonitorRecord(studentId, appName, details, imageData);
        onLogMessage(QString("[DB] 已保存 %1 的违规记录及截图 (%2)").arg(studentId, appName));
    } else {
        // 即使没有截图（例如恢复事件或纯计数），也记录文本日志
        // 注意：insertMonitorRecord 支持空 imageData
        DatabaseManager::instance().insertMonitorRecord(studentId, appName, details, imageData);
        onLogMessage(QString("[DB] 已保存 %1 的状态变更记录 (%2)").arg(studentId, appName));
    }
}

// 【新增】统一刷新班级下拉框实现
void ServerWindow::refreshClassComboBoxes() {
    // 1. 刷新设置页的“删除班级”下拉框
    if (m_deleteClassCombo) {
        QString currentSelection = m_deleteClassCombo->currentText();
        m_deleteClassCombo->clear();
        m_deleteClassCombo->addItem("请选择要删除的班级", ""); // 占位符
        
        QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
        for (const auto &cls : classes) {
            m_deleteClassCombo->addItem(cls.className);
        }
        
        // 尝试恢复之前的选择（如果还存在）
        if (!currentSelection.isEmpty() && currentSelection != "请选择要删除的班级") {
            int index = m_deleteClassCombo->findText(currentSelection);
            if (index != -1) {
                m_deleteClassCombo->setCurrentIndex(index);
            }
        }
    }

    // 2. 刷新监控页的“查看班级”筛选下拉框
    if (m_classFilterCombo) {
        QString currentFilter = m_classFilterCombo->currentText();
        m_classFilterCombo->clear();
        m_classFilterCombo->addItem("全部班级");
        
        QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
        for (const auto &cls : classes) {
            m_classFilterCombo->addItem(cls.className);
        }
        
        // 尝试恢复之前的筛选条件
        int index = m_classFilterCombo->findText(currentFilter);
        if (index != -1) {
            m_classFilterCombo->setCurrentIndex(index);
        } else {
            // 如果当前正在查看被删除的班级，切换回“全部班级”
            m_currentFilterClass = "全部班级";
            refreshMonitorTable("全部班级");
        }

    }
    
    qDebug() << "[UI] Class combo boxes refreshed.";
}

// 【新增】班级管理槽函数实现
void ServerWindow::onCreateClassClicked() {
    QString className = m_newClassNameEdit->text().trimmed();
    if (className.isEmpty()) {
        QMessageBox::warning(this, "提示", "班级名称不能为空！");
        return;
    }
    
    if (DatabaseManager::instance().createClass(className)) {
        QMessageBox::information(this, "成功", QString("班级 '%1' 创建成功！\n学生端现在可以加入该班级。").arg(className));
        m_newClassNameEdit->clear();
        
        // 刷新本地设置页下拉框
        refreshClassComboBoxes();
        
        // 【新增】创建班级后立即广播，让学生端下拉框立即更新
        broadcastUserList();
        
        onLogMessage(QString("[班级管理] 创建了新班级：%1 并已广播至全网").arg(className));
    } else {
        QMessageBox::warning(this, "失败", "创建失败，可能该班级已存在。");
    }
}

// 【新增】删除班级槽函数实现
void ServerWindow::onDeleteClassClicked() {
    QString className = m_deleteClassCombo->currentText();
    if (className.isEmpty() || className == "请选择要删除的班级") {
        QMessageBox::warning(this, "提示", "请先选择一个要删除的班级！");
        return;
    }

    int ret = QMessageBox::question(this, "确认删除", 
        QString("确定要删除班级 \"%1\" 吗？\n\n注意：\n1. 该班级将从列表中移除。\n2. 原属于该班级的所有学生将变为“未分班”状态。\n3. 学生端需要重新选择班级加入。").arg(className),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (ret != QMessageBox::Yes) {
        return;
    }

    if (DatabaseManager::instance().deleteClass(className)) {
        onLogMessage(QString("[班级管理] 成功删除班级：%1").arg(className));
        QMessageBox::information(this, "成功", QString("班级 '%1' 已删除。\n相关学生已重置为未分班状态。").arg(className));
        
        // 刷新本地设置页下拉框（移除已删除项）
        refreshClassComboBoxes();
        
        // 刷新监控页面的筛选下拉框
        m_classFilterCombo->clear();
        m_classFilterCombo->addItem("全部班级");
        QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
        for (const auto &cls : classes) {
            m_classFilterCombo->addItem(cls.className);
        }
        // 如果当前正在查看被删除的班级，切换回“全部班级”
        if (m_currentFilterClass == className) {
            m_currentFilterClass = "全部班级";
            refreshMonitorTable("全部班级");
        }

        // 【关键】广播最新班级列表，通知学生端该班级已不存在
        broadcastUserList();
        
    } else {
        QMessageBox::warning(this, "失败", "删除失败，请检查数据库连接或日志。");
        onLogMessage(QString("[班级管理] 删除班级失败：%1").arg(className));
    }
}

void ServerWindow::onViewClassMembersClicked() {
    QString selectedClass = m_classFilterCombo->currentText();
    m_currentFilterClass = selectedClass; // 【修复】同步更新筛选状态
    refreshMonitorTable(selectedClass);
    onLogMessage(QString("[班级管理] 查看班级成员：%1").arg(selectedClass == "全部班级" ? "所有" : selectedClass));
}

void ServerWindow::onClassComboBoxChanged(const QString &className) {
    // 【修复】如果下拉框返回空字符串，强制设置为“全部班级”
    if (className.isEmpty()) {
        m_currentFilterClass = "全部班级";
    } else {
        m_currentFilterClass = className;
    }
    refreshMonitorTable(m_currentFilterClass);
}
