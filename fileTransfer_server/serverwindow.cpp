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
    if (!DatabaseManager::instance().init()) {
        QMessageBox::critical(this, "错误", "数据库初始化失败！");
    }

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
    
    onLogMessage("系统就绪，等待学生上线...");
    
    // 【新增】初始化班级列表
    refreshMonitorTable(); // 初始加载所有学生
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
    // 【修复】将列数设置为 7，以容纳：ID, 姓名，班级，状态，违规应用，违规次数，查看屏幕
    m_monitorTable->setColumnCount(7);
    m_monitorTable->setHorizontalHeaderLabels({"学生 ID", "姓名", "班级", "状态", "违规应用", "违规次数", "查看屏幕"}); 
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

// 【修改】更新表格行逻辑，适配 7 列结构
void ServerWindow::updateStudentTableRow(const QString &id, const QString &name, StudentStatus status, const QString &app, const QByteArray &screenshot) {
    int row = -1;
    if (m_studentRowMap.contains(id)) {
        row = m_studentRowMap[id];
        QTableWidgetItem *nameItem = m_monitorTable->item(row, 1);
        if (nameItem && nameItem->text() != name) {
            nameItem->setText(name);
        }
    } else {
        row = m_monitorTable->rowCount();
        m_monitorTable->insertRow(row);
        m_studentRowMap[id] = row;
        
        m_monitorTable->setItem(row, 0, new QTableWidgetItem(id));
        m_monitorTable->setItem(row, 1, new QTableWidgetItem(name));
        
        // 获取并设置班级 (第 2 列)
        StudentInfo info = DatabaseManager::instance().getStudentInfo(id);
        m_monitorTable->setItem(row, 2, new QTableWidgetItem(info.className.isEmpty() ? "未分班" : info.className));
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
    
    // 第 3 列：状态
    m_monitorTable->setItem(row, 3, statusItem);
    // 第 4 列：违规应用
    m_monitorTable->setItem(row, 4, appItem);

    // 第 5 列：违规次数 (独立显示)
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

    // 第 6 列：查看屏幕 (高亮处理)
    QTableWidgetItem *actionItem = new QTableWidgetItem("🔍 查看屏幕");
    actionItem->setTextAlignment(Qt::AlignCenter);
    
    // 【修改】设置为高亮蓝色加粗，模拟可点击链接
    QColor linkColor("#0078d4"); 
    actionItem->setForeground(QBrush(linkColor));
    QFont actionFont = actionItem->font();
    actionFont.setBold(true);
    actionFont.setUnderline(true);
    actionItem->setFont(actionFont);
    
    // 添加提示
    actionItem->setToolTip("双击此行或点击此按钮查看实时屏幕");
    
    m_monitorTable->setItem(row, 6, actionItem);
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
    m_monitorTable->setRowCount(0);
    m_studentRowMap.clear();
    
    QList<StudentInfo> allStudents;
    
    if (filterClass.isEmpty() || filterClass == "全部班级") {
        // 获取所有学生
        QMap<QString, StudentInfo> map = DatabaseManager::instance().getAllStudents();
        allStudents = map.values();
    } else {
        // 获取指定班级学生（包含离线）
        allStudents = DatabaseManager::instance().getClassMembers(filterClass, false);
    }
    
    // 将在线状态合并进来（因为 g_onlineUsers 是实时的）
    for (auto &stu : allStudents) {
        if (g_onlineUsers.contains(stu.id)) {
            stu.isOnline = true;
        } else {
            stu.isOnline = false;
        }
        
        StudentStatus status = stu.isOnline ? StudentStatus::Online_Normal : StudentStatus::Offline;
        // 注意：这里只是简单刷新列表，具体的违规状态需要更复杂的逻辑维护，
        // 简单起见，这里只显示在线/离线，详细的违规状态由 onStudentStatusUpdated 动态更新
        // 如果需要持久化违规状态，需从数据库读取最新一条日志判断
        
        updateStudentTableRow(stu.id, stu.name, status, "", QByteArray());
    }
    
    updateOnlineStats();
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
    // 【修复】无论点击哪一列，只要双击行，就获取 ID 并请求截图
    // 特别地，如果点击的是“查看屏幕”列 (col==6) 或其他列，行为一致
    QString id = m_monitorTable->item(row, 0)->text();
    onRequestLiveScreenshotClicked(id);
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

            QString ip = parts[3];
            quint16 uPort = parts[4].toUShort();
            quint16 tPort = parts[5].toUShort();

            // 【修复】解析班级信息 (第 7 个部分，索引为 6)
            // 确保数组长度足够，避免越界
            QString className = "";
            if (parts.size() >= 7) {
                className = parts[6].trimmed();
                // 过滤掉无效的占位符
                if (className == "请选择班级...") {
                    className = "";
                }
            }

            QString uniqueId = QString("%1:%2").arg(ip).arg(uPort);

            UserInfo user;
            user.ip = ip;
            user.port = uPort;
            user.tcpPort = tPort;
            user.id = uniqueId;
            user.nickName = nick; 
            user.isTeacher = isTeacher;
            user.lastHeartbeat = QDateTime::currentMSecsSinceEpoch();

            bool isNewUser = !g_onlineUsers.contains(uniqueId);
            
            if (!isNewUser) {
                if (g_onlineUsers[uniqueId].nickName != nick) {
                    onLogMessage(QString("[同步] 学生 %1 昵称更新：%2 -> %3").arg(uniqueId, g_onlineUsers[uniqueId].nickName, nick));
                }
            } else {
                userListChanged = true;
            }

            g_onlineUsers[uniqueId] = user;

            QString displayName = nick; 
            
            StudentInfo info = DatabaseManager::instance().getStudentInfo(uniqueId);
            if (info.id.isEmpty()) {
                DatabaseManager::instance().registerStudent(uniqueId, displayName, ip);
            }
            
            // 【核心修复】如果心跳包带了班级，且与数据库中记录不同，则立即更新数据库
            if (!className.isEmpty() && info.className != className) {
                bool updateOk = DatabaseManager::instance().updateStudentClass(uniqueId, className);
                if (updateOk) {
                    onLogMessage(QString("[同步] ✅ 学生 %1 成功更新/加入班级：%2").arg(displayName, className));
                } else {
                    onLogMessage(QString("[同步] ❌ 学生 %1 更新班级失败：%2").arg(displayName, className));
                }
            } else if (!className.isEmpty() && info.className == className) {
                // 班级未变，无需操作，但可打印调试日志
                // qDebug() << "[Sync] Student" << displayName << "class unchanged:" << className;
            }

            // 更新 UI 表格，此时数据库已更新，refreshMonitorTable 或 updateStudentTableRow 会读取最新数据
            // 为了即时反映，我们直接在这里更新表格行的班级列，或者依赖下一次刷新
            // 这里调用 updateStudentTableRow，它内部会重新查库或直接使用新值
            // 由于 updateStudentTableRow 目前主要依赖传入参数，我们需要确保它显示最新的
            // 简单做法：直接更新该行文本
            if (m_studentRowMap.contains(uniqueId)) {
                int row = m_studentRowMap[uniqueId];
                QTableWidgetItem *classItem = m_monitorTable->item(row, 2);
                if (classItem) {
                    classItem->setText(className);
                }
            } else {
                // 如果是新用户，updateStudentTableRow 会创建新行并查库
                updateStudentTableRow(uniqueId, displayName, StudentStatus::Online_Normal, "", QByteArray());
            }
            
            onLogMessage(QString("[UI] 已更新学生 %1 的显示昵称为：%2").arg(uniqueId, displayName));
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
    for (const auto &cls : classes) {
        classList.append(cls.className);
    }

    QJsonDocument doc(userList);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    // 【修改】构建包含班级列表的消息格式：USER_LIST|{"users":[...]}|{"classes":[...]}
    // 为了兼容旧版，我们尝试将 classes 放在第二个部分，或者合并到一个大 JSON 中
    // 这里采用合并到大 JSON 的方式，结构变为：USER_LIST|{ "users": [...], "classes": [...] }
    
    QJsonObject rootObj;
    rootObj["users"] = userList;
    rootObj["classes"] = classList;
    
    QJsonDocument finalDoc(rootObj);
    QByteArray finalData = finalDoc.toJson(QJsonDocument::Compact);
    
    QString msg = QString("USER_LIST|%1").arg(QString(finalData));
    QByteArray data = msg.toUtf8();
    
    m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, STUDENT_LISTEN_PORT);
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
                updateOnlineStats();
            }
        }
    }
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

                            QString displayName;
                            if (g_onlineUsers.contains(studentId)) {
                                displayName = g_onlineUsers[studentId].nickName;
                            } else {
                                StudentInfo info = DatabaseManager::instance().getStudentInfo(studentId);
                                displayName = info.name.isEmpty() ? ("学生_" + studentId.split(':').last()) : info.name;
                            }

                            qint64 now = QDateTime::currentMSecsSinceEpoch();

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
                                
                                // 【关键修复】直接从映射中查找并关闭加载对话框，不再依赖表格按钮属性
                                if (m_loadingDialogs.contains(studentId)) {
                                    QDialog *loadingDlg = m_loadingDialogs.take(studentId);
                                    if (loadingDlg && loadingDlg->isVisible()) {
                                        loadingDlg->accept();      // 关闭模态对话框
                                        qDebug() << "[UI] Loading dialog for" << displayName << "closed successfully.";
                                    }
                                    if (loadingDlg) {
                                        loadingDlg->deleteLater(); // 延迟删除
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
                 if (cmd.startsWith("MONITOR_START|")) {
                     QStringList parts = cmd.mid(14).split('|');
                     if (parts.size() == 2) {
                         ctx->jsonSize = parts[0].toUInt();
                         ctx->imgSize = parts[1].toUInt();
                         ctx->state = MonitorReceiveContext::WaitJsonLen;
                     }
                     return; 
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
                         return;
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
        
        // 刷新下拉框
        m_classFilterCombo->clear();
        m_classFilterCombo->addItem("全部班级");
        QList<ClassInfo> classes = DatabaseManager::instance().getAllClasses();
        for (const auto &cls : classes) {
            m_classFilterCombo->addItem(cls.className);
        }
        
        onLogMessage(QString("[班级管理] 创建了新班级：%1").arg(className));
    } else {
        QMessageBox::warning(this, "失败", "创建失败，可能该班级已存在。");
    }
}

void ServerWindow::onViewClassMembersClicked() {
    QString selectedClass = m_classFilterCombo->currentText();
    refreshMonitorTable(selectedClass);
    onLogMessage(QString("[班级管理] 查看班级成员：%1").arg(selectedClass == "全部班级" ? "所有" : selectedClass));
}

void ServerWindow::onClassComboBoxChanged(const QString &className) {
    refreshMonitorTable(className);
}