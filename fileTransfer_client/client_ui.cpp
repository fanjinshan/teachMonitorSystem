#include "client.h"
#include "ui_client.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QPixmap>
#include <QScrollArea>
#include <QHeaderView>
#include <QPainter>
#include <QBitmap>
#include <QPointer>
#include <QTimer>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDataStream>
#include <QTcpSocket>
#include <QFileInfo>
#include <QStyle>
#include <QComboBox> // 【新增】引入 QComboBox


void client::initUi()
{
    this->setWindowTitle("智慧教室客户端 - " + m_myNickName);
    // 【修改】增加窗口默认宽度，从 900 调整为 1200，以容纳所有列
    this->resize(1200, 650);
    this->setStyleSheet(R"(
        QMainWindow { background-color: #f5f6fa; }
        QLabel { color: #333; }
        QPushButton { border: none; border-radius: 4px; padding: 8px; }
        QListWidget { border: none; background: transparent; outline: none; }
        QListWidget::Item { border-radius: 4px; padding: 10px; margin: 2px 5px; }
        QListWidget::Item:hover { background-color: #eef2f7; }
        QListWidget::Item:selected { background-color: #dcebf9; color: #0078d4; }
        QTreeWidget { border: 1px solid #ddd; border-radius: 4px; background: white; }
        QHeaderView::section { background: #f0f0f0; padding: 8px; border: none; border-bottom: 1px solid #ddd; font-weight: bold; }
    )");

    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // --- 左侧导航栏 ---
    m_leftPanel = new QWidget();
    m_leftPanel->setFixedWidth(240);
    m_leftPanel->setStyleSheet("background-color: #2c3e50;");
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 20, 0, 0);
    leftLayout->setSpacing(15);

    QWidget *userCard = new QWidget();
    userCard->setStyleSheet("background-color: #34495e; border-radius: 8px; margin: 0 10px;");
    QVBoxLayout *userLayout = new QVBoxLayout(userCard);
    userLayout->setAlignment(Qt::AlignCenter); // 确保布局内控件整体居中
    
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(60, 60);
    // 【修改】增强样式，添加左右外边距微调视觉中心，并确保内容居中
    m_avatarLabel->setStyleSheet("background-color: #ecf0f1; border-radius: 30px; border: 2px solid #bdc3c7; margin-left: 5px; margin-right: 5px;border:none");
    m_avatarLabel->setAlignment(Qt::AlignCenter); // 确保图片/文字内容在 Label 内部居中
    m_avatarLabel->setText("👨‍🎓"); 
    m_avatarLabel->setFont(QFont("Segoe UI", 30));
    
    m_nickNameLabel = new QLabel(m_myNickName);
    m_nickNameLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold; margin-top: 5px;");
    m_nickNameLabel->setAlignment(Qt::AlignCenter);

    m_statusLabel = new QLabel("● 在线");
    m_statusLabel->setStyleSheet("color: #2ecc71; font-size: 12px;");
    m_statusLabel->setAlignment(Qt::AlignCenter);

    userLayout->addWidget(m_avatarLabel, 0, Qt::AlignHCenter);
    userLayout->addWidget(m_nickNameLabel);
    userLayout->addWidget(m_statusLabel);
    leftLayout->addWidget(userCard);

    m_navList = new QListWidget();
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setFixedHeight(180);
    
    QStringList navItems = {"💬 成员列表", "📁 共享文件", "⚙️ 个人设置"};
    for (const QString &item : navItems) {
        QListWidgetItem *navItem = new QListWidgetItem(item);
        navItem->setSizeHint(QSize(0, 50));
        navItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        navItem->setFont(QFont("Microsoft YaHei", 14));
        navItem->setForeground(QBrush(QColor("#bdc3c7")));
        m_navList->addItem(navItem);
    }
    m_navList->item(0)->setForeground(QBrush(QColor("#ffffff"))); 
    m_navList->item(0)->setBackground(QColor("#34495e"));
    
    connect(m_navList, &QListWidget::currentRowChanged, this, [this](int row) {
        for (int i = 0; i < m_navList->count(); ++i) {
            QListWidgetItem *item = m_navList->item(i);
            item->setForeground(QColor("#bdc3c7"));
            item->setBackground(QColor("transparent"));
        }
        QListWidgetItem *curr = m_navList->item(row);
        if (curr) {
            curr->setForeground(QColor("#ffffff"));
            curr->setBackground(QColor("#34495e"));
            m_mainStack->setCurrentIndex(row);
        }
        
        if (row == 1) {
            if (!checkTeacherOnline()) {
                QMessageBox::information(this, "提示", "暂未检测到在线的教师端。\n请确认教师端已启动，或稍后点击'刷新'按钮。");
            }
        }
    });
    leftLayout->addWidget(m_navList);
    leftLayout->addStretch();

    QPushButton *quitBtn = new QPushButton("退出登录");
    quitBtn->setStyleSheet("background-color: #c0392b; color: white; font-size: 14px; margin: 10px;");
    quitBtn->setFixedHeight(50);
    connect(quitBtn, &QPushButton::clicked, this, &client::onQuitClicked);
    leftLayout->addWidget(quitBtn);

    // --- 右侧内容区 ---
    m_mainStack = new QStackedWidget();
    m_mainStack->setStyleSheet("background-color: #f5f6fa;");

    // 页面 1: 好友/消息列表
    m_friendPage = new QWidget();
    QVBoxLayout *friendLayout = new QVBoxLayout(m_friendPage);
    friendLayout->setContentsMargins(20, 20, 20, 20);
    
    QLabel *friendTitle = new QLabel("在线成员");
    friendTitle->setFont(QFont("Microsoft YaHei", 18, QFont::Bold));
    friendTitle->setStyleSheet("color: #2c3e50; margin-bottom: 10px;");
    friendLayout->addWidget(friendTitle);

    m_friendList = new QListWidget();
    m_friendList->setStyleSheet(R"(
        QListWidget { 
            border: none; 
            background: white; 
            border-radius: 8px; 
            padding: 10px;
            font-size: 14px;
        }
        QListWidget::Item { 
            border-bottom: 1px solid #eee; 
            padding: 15px; 
        }
        QListWidget::Item:hover { background-color: #f0f8ff; }
    )");
    connect(m_friendList, &QListWidget::itemDoubleClicked, this, &client::onFriendListItemDoubleClicked);
    friendLayout->addWidget(m_friendList);

    m_mainStack->addWidget(m_friendPage);

    // 页面 2: 共享文件
    m_fileSharePage = new QWidget();
    QVBoxLayout *fileLayout = new QVBoxLayout(m_fileSharePage);
    fileLayout->setContentsMargins(20, 20, 20, 20);

    QWidget *fileHeader = new QWidget();
    QHBoxLayout *headerLayout = new QHBoxLayout(fileHeader);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *fileTitle = new QLabel("教师共享文件");
    fileTitle->setFont(QFont("Microsoft YaHei", 18, QFont::Bold));
    fileTitle->setStyleSheet("color: #2c3e50;");
    headerLayout->addWidget(fileTitle);
    
    headerLayout->addStretch();
    
    m_refreshBtn = new QPushButton("🔄 刷新");
    m_refreshBtn->setStyleSheet("background-color: #3498db; color: white; font-weight: bold; padding: 8px 15px;");
    connect(m_refreshBtn, &QPushButton::clicked, this, &client::onRefreshClicked);
    headerLayout->addWidget(m_refreshBtn);

    fileLayout->addWidget(fileHeader);

    m_fileTree = new QTreeWidget();
    m_fileTree->setHeaderLabels({"文件名", "类型", "大小", "修改时间", "下载次数", "来自于"});
    
    // 【修改】优化列宽设置：显著增加关键列宽度，特别是“来自于”列，防止 IP 显示不全
    m_fileTree->setColumnWidth(0, 300); // 文件名
    m_fileTree->setColumnWidth(1, 80);  // 类型
    m_fileTree->setColumnWidth(2, 100); // 大小
    m_fileTree->setColumnWidth(3, 170); // 修改时间
    m_fileTree->setColumnWidth(4, 90);  // 下载次数
    m_fileTree->setColumnWidth(5, 220); // 来自于 (大幅增加宽度以显示完整 IP)
    
    m_fileTree->header()->setStretchLastSection(true);
    m_fileTree->header()->setMinimumSectionSize(60);
    
    m_fileTree->setRootIsDecorated(false);
    m_fileTree->setAlternatingRowColors(true);
    connect(m_fileTree, &QTreeWidget::itemDoubleClicked, this, &client::onFileListItemDoubleClicked);
    connect(m_fileTree, &QTreeWidget::itemClicked, this, &client::onFileListItemClicked);
    fileLayout->addWidget(m_fileTree);

    QWidget *fileFooter = new QWidget();
    QHBoxLayout *footerLayout = new QHBoxLayout(fileFooter);
    footerLayout->setContentsMargins(0, 10, 0, 0);
    
    m_settingBtn = new QPushButton("💾 保存路径设置");
    m_settingBtn->setStyleSheet("background-color: #95a5a6; color: white; padding: 8px 15px;");
    connect(m_settingBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "选择保存路径", m_localSavePath);
        if (!dir.isEmpty()) {
            m_localSavePath = dir;
            QMessageBox::information(this, "成功", "保存路径已更新:\n" + dir);
        }
    });
    
    m_downloadBtn = new QPushButton("⬇️ 下载选中");
    m_downloadBtn->setStyleSheet("background-color: #27ae60; color: white; font-weight: bold; padding: 8px 15px;");
    m_downloadBtn->setEnabled(false);
    connect(m_downloadBtn, &QPushButton::clicked, this, &client::onDownloadClicked);

    m_backBtn = new QPushButton("返回");
    m_backBtn->setStyleSheet("background-color: #e74c3c; color: white; padding: 8px 15px;");
    connect(m_backBtn, &QPushButton::clicked, this, [this]() {
        m_navList->setCurrentRow(0); 
    });

    footerLayout->addWidget(m_settingBtn);
    footerLayout->addStretch();
    footerLayout->addWidget(m_downloadBtn);
    footerLayout->addWidget(m_backBtn);
    
    fileLayout->addWidget(fileFooter);

    m_mainStack->addWidget(m_fileSharePage);

    // 页面 3: 设置 (修改为支持滚动)
    QWidget *settingsContent = new QWidget();
    QVBoxLayout *setLayout = new QVBoxLayout(settingsContent);
    setLayout->setContentsMargins(50, 60, 50, 60);
    setLayout->setSpacing(30);

    QLabel *settingsTitle = new QLabel("个人设置");
    settingsTitle->setFont(QFont("Microsoft YaHei", 20, QFont::Bold));
    settingsTitle->setStyleSheet("color: #2c3e50; margin-bottom: 10px;");
    settingsTitle->setAlignment(Qt::AlignCenter);
    setLayout->addWidget(settingsTitle);

    // --- 昵称设置卡片 (原有) ---
    QWidget *nickCard = new QWidget();
    nickCard->setStyleSheet("background-color: white; border-radius: 10px; padding: 25px;");
    nickCard->setFixedWidth(500);
    QVBoxLayout *nickLayout = new QVBoxLayout(nickCard);
    nickLayout->setSpacing(15); 
    
    QLabel *nickLabel = new QLabel("修改昵称：");
    nickLabel->setFont(QFont("Microsoft YaHei", 14, QFont::Bold));
    nickLabel->setAlignment(Qt::AlignLeft);
    nickLayout->addWidget(nickLabel);

    m_nicknameEdit = new QLineEdit(m_myNickName);
    m_nicknameEdit->setPlaceholderText("输入新昵称");
    m_nicknameEdit->setFixedHeight(45);
    m_nicknameEdit->setStyleSheet("border: 1px solid #ddd; border-radius: 6px; padding: 0 15px; font-size: 15px; background-color: #fafafa;");
    nickLayout->addWidget(m_nicknameEdit);

    m_saveNicknameBtn = new QPushButton("💾 保存昵称");
    m_saveNicknameBtn->setFixedHeight(60);
    m_saveNicknameBtn->setCursor(Qt::PointingHandCursor);
    m_saveNicknameBtn->setStyleSheet(
        "background-color: #3498db; color: white; font-weight: bold; font-size: 20px; "
        "border-radius: 6px; padding: 0px; margin: 0px;"
        );
    connect(m_saveNicknameBtn, &QPushButton::clicked, this, &client::onSaveNicknameClicked);
    nickLayout->addWidget(m_saveNicknameBtn);

    QWidget *nickContainer = new QWidget();
    QVBoxLayout *nickContainerLayout = new QVBoxLayout(nickContainer);
    nickContainerLayout->setContentsMargins(0, 0, 0, 0);
    nickContainerLayout->addWidget(nickCard);
    nickContainerLayout->addStretch();
    nickContainerLayout->setAlignment(Qt::AlignHCenter);

    setLayout->addWidget(nickContainer);

    // --- 【新增】班级管理卡片 ---
    QWidget *classCard = new QWidget();
    classCard->setStyleSheet("background-color: white; border-radius: 10px; padding: 25px;");
    classCard->setFixedWidth(500);
    QVBoxLayout *classLayout = new QVBoxLayout(classCard);
    classLayout->setSpacing(15);

    QLabel *classLabelTitle = new QLabel("加入班级：");
    classLabelTitle->setFont(QFont("Microsoft YaHei", 14, QFont::Bold));
    classLabelTitle->setAlignment(Qt::AlignLeft);
    classLayout->addWidget(classLabelTitle);

    QLabel *classHint = new QLabel("从列表中选择当前存在的班级，点击加入后，教师端可按班级查看学生。");
    classHint->setStyleSheet("color: #7f8c8d; font-size: 13px;");
    classHint->setWordWrap(true);
    classLayout->addWidget(classHint);

    QHBoxLayout *classInputLayout = new QHBoxLayout();
    m_classComboBox = new QComboBox();
    m_classComboBox->setEditable(false); 
    m_classComboBox->setPlaceholderText("正在同步班级列表...");
    
    m_classComboBox->clear(); 
    m_classComboBox->addItem("请选择班级...", ""); 
    
    m_classComboBox->setFixedHeight(45);
    // 【修复】完善 QComboBox 样式，明确指定 hover 和 selected 状态的背景与文字颜色，防止鼠标悬浮时文字变白消失
    m_classComboBox->setStyleSheet(R"(
        QComboBox {
            border: 1px solid #ddd; 
            border-radius: 6px; 
            padding: 0 10px; 
            font-size: 15px; 
            background-color: #fafafa;
            color: #333;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 8px solid #7f8c8d;
            margin-right: 10px;
        }
        QComboBox QAbstractItemView {
            border: 1px solid #ddd;
            border-radius: 6px;
            background-color: white;
            selection-background-color: #3498db;
            selection-color: white;
            outline: none;
            padding: 5px;
        }
        QComboBox QAbstractItemView::item {
            min-height: 40px;
            padding: 5px 10px;
            color: #333;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: #eef2f7;
            color: #333;
        }
        QComboBox QAbstractItemView::item:selected {
            background-color: #3498db;
            color: white;
        }
    )");
    m_classComboBox->setFixedWidth(200);
    classInputLayout->addWidget(m_classComboBox);

    m_joinClassBtn = new QPushButton("加入班级");
    m_joinClassBtn->setFixedHeight(45);
    m_joinClassBtn->setCursor(Qt::PointingHandCursor);
    m_joinClassBtn->setStyleSheet("background-color: #27ae60; color: white; font-weight: bold; font-size: 15px; border-radius: 6px; padding: 0 15px;");
    
    connect(m_joinClassBtn, &QPushButton::clicked, this, &client::onJoinClassClicked);
    classInputLayout->addWidget(m_joinClassBtn);

    classLayout->addLayout(classInputLayout);

    // 显示当前加入的班级
    QLabel *currentClassLabel = new QLabel("当前班级：未加入");
    currentClassLabel->setObjectName("currentClassLabel");
    currentClassLabel->setStyleSheet("color: #e67e22; font-size: 14px; font-weight: bold; margin-top: 5px;");
    classLayout->addWidget(currentClassLabel);

    QWidget *classContainer = new QWidget();
    QVBoxLayout *classContainerLayout = new QVBoxLayout(classContainer);
    classContainerLayout->setContentsMargins(0, 0, 0, 0);
    classContainerLayout->addWidget(classCard);
    classContainerLayout->addStretch();
    classContainerLayout->setAlignment(Qt::AlignHCenter);

    setLayout->addWidget(classContainer);

    // --- 头像设置卡片 (原有) ---
    QWidget *avatarCard = new QWidget();
    avatarCard->setStyleSheet("background-color: white; border-radius: 10px; padding: 25px;");
    avatarCard->setFixedWidth(500);
    QVBoxLayout *avatarLayout = new QVBoxLayout(avatarCard);
    avatarLayout->setSpacing(15);

    QLabel *avatarLabelTitle = new QLabel("更换头像：");
    avatarLabelTitle->setFont(QFont("Microsoft YaHei", 14, QFont::Bold));
    avatarLabelTitle->setAlignment(Qt::AlignLeft);
    avatarLayout->addWidget(avatarLabelTitle);

    QLabel *avatarHint = new QLabel("当前头像显示在左侧导航栏，点击按钮选择本地图片。");
    avatarHint->setStyleSheet("color: #7f8c8d; font-size: 13px;");
    avatarHint->setWordWrap(true);
    avatarLayout->addWidget(avatarHint);

    m_changeAvatarBtn = new QPushButton("🖼️ 选择图片");
    m_changeAvatarBtn->setFixedHeight(60);
    m_changeAvatarBtn->setCursor(Qt::PointingHandCursor);
    m_changeAvatarBtn->setStyleSheet("background-color: #9b59b6; color: white; font-weight: bold; font-size: 20px; border-radius: 6px;padding: 0px");
    connect(m_changeAvatarBtn, &QPushButton::clicked, this, &client::onChangeAvatarClicked);
    avatarLayout->addWidget(m_changeAvatarBtn);

    QWidget *avatarContainer = new QWidget();
    QVBoxLayout *avatarContainerLayout = new QVBoxLayout(avatarContainer);
    avatarContainerLayout->setContentsMargins(0, 0, 0, 0);
    avatarContainerLayout->addWidget(avatarCard);
    avatarContainerLayout->addStretch();
    avatarContainerLayout->setAlignment(Qt::AlignHCenter);

    setLayout->addWidget(avatarContainer);
    setLayout->addStretch();

    // 【修改】创建滚动区域并包裹设置内容
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidget(settingsContent);
    scrollArea->setWidgetResizable(true); // 关键：允许内容随窗口大小调整
    scrollArea->setFrameShape(QFrame::NoFrame); // 去掉边框，更美观
    scrollArea->setStyleSheet("background-color: #f5f6fa;"); // 保持背景色一致
    
    // 将滚动区域添加到堆叠控件，而不是直接添加 settingsContent
    m_mainStack->addWidget(scrollArea);

    mainLayout->addWidget(m_leftPanel);
    mainLayout->addWidget(m_mainStack);
}

void client::onFileListItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item || !m_fileTree) return;

    bool isDir = item->data(0, Qt::UserRole).toBool();
    QString name = item->text(0);

    // 【修改】移除双击文件直接下载的逻辑，防止误触。
    // 现在双击文件将无任何反应，用户需选中文件后点击底部“下载选中”按钮。
    if (!isDir && name != "..") {
        return;
    }

    // 【优化】只有进入目录或返回上级时，才显示加载状态并刷新
    m_fileTree->clear();
    QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
    loadingItem->setText(0, "正在加载...");
    loadingItem->setForeground(0, QColor("#7f8c8d"));

    if (name == "..") {
        if (m_currentPath == "/" || m_currentPath.isEmpty()) {
            qDebug() << "[Nav] Already at root, ignoring '..' click.";
            delete loadingItem;
            // 在根目录点击..，重新加载根目录
            requestFileList(m_currentTargetIp, m_currentTargetPort, "/");
            return;
        }

        QString newPath = m_currentPath;
        // 安全地去除末尾斜杠
        while (newPath.endsWith("/") && newPath.length() > 1) {
            newPath.chop(1);
        }

        int lastSlashIndex = newPath.lastIndexOf("/");

        if (lastSlashIndex <= 0) {
            newPath = "/";
        } else {
            newPath = newPath.left(lastSlashIndex);
            if (newPath.isEmpty()) {
                newPath = "/";
            }
        }

        m_currentPath = newPath;
        
        qDebug() << "[Nav] Going up to:" << m_currentPath;
        
        // 【关键优化】导航操作直接请求列表，不再等待，消除卡顿感
        requestFileList(m_currentTargetIp, m_currentTargetPort, m_currentPath);
        return;
    }

    if (isDir) {
        QString nextPath;
        if (m_currentPath == "/" || m_currentPath.isEmpty()) {
            nextPath = "/" + name;
        } else {
            nextPath = m_currentPath + "/" + name;
        }
        
        nextPath.replace("\\", "/");
        while (nextPath.contains("//")) {
            nextPath.replace("//", "/");
        }

        if (!nextPath.startsWith("/")) {
            nextPath = "/" + nextPath;
        }

        m_currentPath = nextPath;
        
        qDebug() << "[Nav] Entering directory:" << name << "-> New Path:" << m_currentPath;
        
        // 【关键优化】导航操作直接请求列表
        requestFileList(m_currentTargetIp, m_currentTargetPort, m_currentPath);
    }
}

void client::onFileListItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    bool isDir = item->data(0, Qt::UserRole).toBool();
    m_downloadBtn->setEnabled(!isDir && item->text(0) != "..");
}

void client::onQuitClicked()
{
    if (QMessageBox::question(this, "确认退出", "确定要退出客户端吗？") == QMessageBox::Yes) {
        qApp->quit();
    }
}

void client::onManualConnectClicked()
{
    bool okIp = false;
    bool okPort = false;
    
    QString teacherIp = QInputDialog::getText(this, "手动连接教师端", "请输入教师端 IP 地址:", QLineEdit::Normal, "", &okIp);
    if (!okIp || teacherIp.isEmpty()) return;
    
    quint16 teacherTcpPort = QInputDialog::getInt(this, "手动连接教师端", "请输入教师端 TCP 端口:", 20000, 1024, 65535, 1, &okPort);
    if (!okPort) return;
    
    m_manualTeacherIp = teacherIp;
    m_manualTeacherTcpPort = teacherTcpPort;
    
    m_navList->setCurrentRow(1);
    onRefreshClicked();
}
