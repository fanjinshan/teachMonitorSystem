#include "client.h"
#include <QFileInfo>
#include <QStyle>
#include <QComboBox>

//=========UI初始化==========
/**
 * @brief 初始化用户界面
 *
 * 创建主窗口的布局和所有 UI 组件：
 * - 左侧导航栏（用户卡片、导航列表、退出按钮）
 * - 右侧内容区（堆叠窗口，包含三个页面：成员列表、共享文件、个人设置）
 */
void client::initUi()
{
    //设置窗口标题，显示当前昵称
    this->setWindowTitle("教室监控管理系统客户端 - " + m_myNickName);
    //窗口默认宽度
    this->resize(1200,650);
    //设置全局样式表
    this->setStyleSheet(R"(
        QMainWindow { background-color : #f5f6fa;}
        QLabel { color : #333;}
        QPushButton { border : none; border-radius:4px;padding : 8px;}
        QListWidget { border : none; background : transparent;outline : none;}
        QListWidget::Item { border-radius : 4px;padding : 10px;margin:2px 5px;}
        QListWidget::Item:hover { background-color : #eef2f7;}
        QListWidget::Item:selected { background-color : #dcebf9;color : #0078d4;}
        QTreeWidge { border : 1px solid #ddd;border-radius:4px;background-bottom:1px solid #ddd;font-weight:bold;}
        QHeaderView::section {background:#f0f0f0;padding:8px;border:none;border-bottom:1px solid #ddd;font-weight:bold;}
    )");

    //---------中央控件---------
    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    //主布局：水平布局，无边框
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0,0,0,0);

    //---------左侧导航栏---------
    m_leftPanel = new QWidget();
    m_leftPanel->setFixedWidth(240);//固定240
    m_leftPanel->setStyleSheet("background-color:#2c3e50;");
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0,20,0,0);
    leftLayout->setSpacing(15);

    //------用户卡片（头像、昵称、状态）-------
    QWidget* userCard = new QWidget();
    userCard->setStyleSheet("background-color : #34495e;border-radius:8px;margin:0 10px;");
    QVBoxLayout *userLayout = new QVBoxLayout(userCard);
    userLayout->setAlignment(Qt::AlignCenter);

    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(60,60);
    //头像样式：圆形、灰色背景、无边框
    m_avatarLabel->setStyleSheet("background-color : #ecf0f1;border-radius:30px;border:2px solid #bdc3c7;margin-left:5px;margin-right:5px;border:none;");
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setText("👨‍🎓");//默认学生图标
    m_avatarLabel->setFont(QFont("Segoe UI",30));

    m_nickNameLabel = new QLabel(m_myNickName);
    m_nickNameLabel->setStyleSheet("color:white;font-size:16px;font-weight:bold;margin-top:5px;");
    m_nickNameLabel->setAlignment(Qt::AlignCenter);

    m_statusLabel = new QLabel("● 在线");
    m_statusLabel->setStyleSheet("color:#2ecc71;font-size:12px;");
    m_statusLabel->setAlignment(Qt::AlignCenter);

    userLayout->addWidget(m_avatarLabel,0,Qt::AlignHCenter);
    userLayout->addWidget(m_nickNameLabel);
    userLayout->addWidget(m_statusLabel);
    leftLayout->addWidget(userCard);

    //---------导航列表（成员、共享文件、个人设置）---------
    m_navList = new QListWidget();
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);//禁止垂直滚动条
    m_navList->setFixedHeight(180);//固定高度，只显示3个条目

    QStringList navItems = {"💬 成员列表", "📁 共享文件", "⚙️ 个人设置"};
    for(const QString &item : navItems)
    {
        QListWidgetItem *navItem = new QListWidgetItem(item);
        navItem->setSizeHint(QSize(0,50));//设置每个条目的高度
        navItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        navItem->setFont(QFont("Microsoft YaHei",14));
        navItem->setForeground(QBrush(QColor("#bdc3c7")));//浅灰色文字
        m_navList->addItem(navItem);
    }
    //默认选中第一项（成员）
    m_navList->item(0)->setForeground(QBrush(QColor("#ffffff")));
    m_navList->item(0)->setBackground(QColor("#34495e"));

    //导航栏点击切换页面
    connect(m_navList,&QListWidget::currentRowChanged,this,[this](int row){
        //重置所有项样式
        for(int i = 0;i < m_navList->count();++i)
        {
            QListWidgetItem* item = m_navList->item(i);
            item->setForeground(QColor("#bdc3c7"));
            item->setBackground(QColor("transparent"));
        }

        //高亮当前选中项
        QListWidgetItem* curr = m_navList->item(row);
        if(curr)
        {
            curr->setForeground(QColor("#ffffff"));
            curr->setBackground(QColor("#34495e"));
            m_mainStack->setCurrentIndex(row);//切换堆叠窗口的当前页
        }

        //如果切换到共享文件夹（索引1），就检查教师端是否在线
        if(row == 1)
        {
            if(!checkTeacherOnline())
            {
                QMessageBox::information(this,"提示","暂未检测到在线的教师端。\n请确认教师端已启动，或稍后点击'刷新'按钮。");
            }
        }
    });
    leftLayout->addWidget(m_navList);
    leftLayout->addStretch();//将下面的按钮推到底部

    //退出登录按钮
    QPushButton *quitBtn = new QPushButton("退出登录");
    quitBtn->setStyleSheet("background-color:#c0392b;color:white;font-size:14px;margin:10px;");
    quitBtn->setFixedHeight(50);
    connect(quitBtn,&QPushButton::clicked,this,&client::onQuitClicked);
    leftLayout->addWidget(quitBtn);

    //--------右侧内容区（堆叠窗口）---------
    m_mainStack = new QStackedWidget();
    m_mainStack->setStyleSheet("background-color:#f5f6fa");

    //----------页面1：成员列表-----------
    m_friendPage = new QWidget();
    QVBoxLayout *friendLayout = new QVBoxLayout(m_friendPage);
    friendLayout->setContentsMargins(20,20,20,20);

    QLabel *friendTitle = new QLabel("在线成员");
    friendTitle->setFont(QFont("Microsoft YaHei",18,QFont::Bold));
    friendTitle->setStyleSheet("color:#2c3e50;margin-bottom:10px;");
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
    //双击列表项触发连接教师端
    connect(m_friendList,&QListWidget::itemDoubleClicked,this,&client::onFriendListItemDoubleClicked);
    friendLayout->addWidget(m_friendList);

    m_mainStack->addWidget(m_friendPage);

    //---------页面2：共享文件---------
    m_fileSharePage = new QWidget();
    QVBoxLayout *fileLayout = new QVBoxLayout(m_fileSharePage);
    fileLayout->setContentsMargins(20,20,20,20);

    //头部：标题 + 刷新按钮
    QWidget* fileHeader = new QWidget();
    QHBoxLayout* headerLayout = new QHBoxLayout(fileHeader);
    headerLayout->setContentsMargins(0,0,0,0);

    QLabel *fileTitle = new QLabel("教师共享文件");
    fileTitle->setFont(QFont("Microsoft YaHei",18,QFont::Bold));
    fileTitle->setStyleSheet("color:#2c3e50;");
    headerLayout->addWidget(fileTitle);

    headerLayout->addStretch();

    m_refreshBtn = new QPushButton("🔄 刷新");
    m_refreshBtn->setStyleSheet("background-color:#3498db;color:white;font-weight:bold;padding:8px 15px;");
    connect(m_refreshBtn,&QPushButton::clicked,this,&client::onRefreshClicked);
    headerLayout->addWidget(m_refreshBtn);

    fileLayout->addWidget(fileHeader);

    //文件树控件，显示教师端共享的文件列表
    m_fileTree = new QTreeWidget();
    //设置列标题：文件名、类型、大小、修改时间、下载次数、来自于
    m_fileTree->setHeaderLabels({"文件名","类型","大小","修改时间","下载次数","来自于"});

    //列宽设置
    m_fileTree->setColumnWidth(0,300);//文件名
    m_fileTree->setColumnWidth(1,80);//类型
    m_fileTree->setColumnWidth(2,100);//大小
    m_fileTree->setColumnWidth(3,170);//修改时间
    m_fileTree->setColumnWidth(4,90);//下载次数
    m_fileTree->setColumnWidth(5,220);//来自于

    m_fileTree->header()->setStretchLastSection(true);//最后一列自动拉伸
    m_fileTree->header()->setMinimumSectionSize(60);//最小列宽

    m_fileTree->setRootIsDecorated(false);//不显示根节点装饰线
    m_fileTree->setAlternatingRowColors(true);//交替行颜色
    //双击目录时进入目录
    connect(m_fileTree,&QTreeWidget::itemDoubleClicked,this,&client::onFileListItemDoubleClicked);
    //单击文件时更新下载按钮状态
    connect(m_fileTree,&QTreeWidget::itemClicked,this,&client::onFileListItemClicked);
    fileLayout->addWidget(m_fileTree);

    //底部按钮栏
    QWidget *fileFooter = new QWidget();
    QHBoxLayout* footerLayout = new QHBoxLayout(fileFooter);
    footerLayout->setContentsMargins(0,10,0,0);

    m_settingBtn = new QPushButton("💾 保存路径设置");
    m_settingBtn->setStyleSheet("background-color:#95a5a6;color:white;padding:8px 15px;");
    connect(m_settingBtn,&QPushButton::clicked,this,[this](){
        QString dir = QFileDialog::getExistingDirectory(this,"选择保存路径",m_localSavePath);
        if(!dir.isEmpty())
        {
            m_localSavePath = dir;
            //保存到配置文件
            saveUserSettings();
            QMessageBox::information(this,"成功","保存路径已更新:\n" + dir);
        }
    });

    m_downloadBtn = new QPushButton("⬇️ 下载选中");
    m_downloadBtn->setStyleSheet("background-color:#27ae60;color:white;font-weight:bold;padding:8px 15px;");
    m_downloadBtn->setEnabled(false);//初始不可用，选中文件后才可用
    connect(m_downloadBtn,&QPushButton::clicked,this,&client::onDownloadClicked);

    m_backBtn = new QPushButton("返回");
    m_backBtn->setStyleSheet("background-color:#e74c3c;color:white;padding:8px 15px;");
    connect(m_backBtn,&QPushButton::clicked,this,[this](){
        m_navList->setCurrentRow(0);//返回成员列表页面
    });

    footerLayout->addWidget(m_settingBtn);
    footerLayout->addStretch();
    footerLayout->addWidget(m_downloadBtn);
    footerLayout->addWidget(m_backBtn);

    fileLayout->addWidget(fileFooter);

    m_mainStack->addWidget(m_fileSharePage);

    //------------页面3：个人设置（支持滚动）--------------
    //先创建一个可滚动的内容区域
    QWidget* settingsContent = new QWidget();
    QVBoxLayout* setLayout = new QVBoxLayout(settingsContent);
    setLayout->setContentsMargins(50,60,50,60);
    setLayout->setSpacing(30);

    QLabel *settingsTitle = new QLabel("个人设置");
    settingsTitle->setFont(QFont("Microsoft YaHei",20,QFont::Bold));
    settingsTitle->setStyleSheet("color:#2c3e50;margin-bottom:10px;");
    settingsTitle->setAlignment(Qt::AlignCenter);
    setLayout->addWidget(settingsTitle);

    //---设置昵称卡片---
    QWidget* nickCard = new QWidget();
    nickCard->setStyleSheet("background-color:white;border-radius:10px;padding:25px;");
    nickCard->setFixedWidth(500);
    QVBoxLayout *nickLayout = new QVBoxLayout(nickCard);
    nickLayout->setSpacing(15);

    QLabel* nickLabel = new QLabel("修改昵称:");
    nickLabel->setFont(QFont("Microsoft YaHei",14,QFont::Bold));
    nickLabel->setAlignment(Qt::AlignLeft);
    nickLayout->addWidget(nickLabel);

    m_nicknameEdit = new QLineEdit(m_myNickName);
    m_nicknameEdit->setPlaceholderText("输入新昵称");
    m_nicknameEdit->setFixedHeight(45);
    m_nicknameEdit->setStyleSheet("border:1px solid #ddd;border-radius:6px;padding:0 15px;font-size:15px;background-color:#fafafa;");
    nickLayout->addWidget(m_nicknameEdit);

    m_saveNicknameBtn = new QPushButton("💾 保存昵称");
    m_saveNicknameBtn->setFixedHeight(60);
    m_saveNicknameBtn->setCursor(Qt::PointingHandCursor);
    m_saveNicknameBtn->setStyleSheet(
        "background-color:#3498db;color:white;font-weight:bold;font-size:20px;"
        "border-radius:6px;padding:0px;margin:0px;"
        );
    connect(m_saveNicknameBtn,&QPushButton::clicked,this,&client::onSaveNicknameClicked);
    nickLayout->addWidget(m_saveNicknameBtn);

    //将昵称卡片放入容器并居中
    QWidget *nickContainer = new QWidget();
    QVBoxLayout *nickContainerLayout = new QVBoxLayout(nickContainer);
    nickContainerLayout->setContentsMargins(0,0,0,0);
    nickContainerLayout->addWidget(nickCard);
    nickContainerLayout->addStretch();
    nickContainerLayout->setAlignment(Qt::AlignHCenter);
    setLayout->addWidget(nickContainer);

    //-------班级管理卡片------
    QWidget* classCard = new QWidget();
    classCard->setStyleSheet("background-color:white;border-radius:10px;padding:25px;");
    classCard->setFixedWidth(500);
    QVBoxLayout *classLayout = new QVBoxLayout(classCard);
    classLayout->setSpacing(15);

    QLabel *classLabelTitle = new QLabel("加入班级：");
    classLabelTitle->setFont(QFont("Microsoft YaHei",14,QFont::Bold));
    classLabelTitle->setAlignment(Qt::AlignLeft);
    classLayout->addWidget(classLabelTitle);

    QLabel* classHint = new QLabel("从列表中选择当前存在的班级，点击加入后，教师端可按班级查看学生");
    classHint->setStyleSheet("color:#7f8c8d;font-size:13px;");
    classHint->setWordWrap(true);//自动换行
    classLayout->addWidget(classHint);

    QHBoxLayout *classInputLayout = new QHBoxLayout();
    m_classComboBox = new QComboBox();
    m_classComboBox->setEditable(false);
    m_classComboBox->setPlaceholderText("正在同步班级列表...");

    m_classComboBox->clear();
    m_classComboBox->addItem("请选择班级...","");//占位项

    m_classComboBox->setFixedHeight(45);
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
    m_joinClassBtn->setStyleSheet("background-color:#27ae60;color:white;font-weight:bold;font-size:15px;border-radius:6px;padding:0 15px;");
    connect(m_joinClassBtn,&QPushButton::clicked,this,&client::onJoinClassClicked);
    classInputLayout->addWidget(m_joinClassBtn);

    classLayout->addLayout(classInputLayout);

    //显示当前加入的班级
    QLabel* currentClassLabel = new QLabel("当前班级：未加入");
    currentClassLabel->setObjectName("currentClassLabel");
    currentClassLabel->setStyleSheet("color:#e67e22;font-size:14px;font-weight:bold;margin-top:5px;");
    classLayout->addWidget(currentClassLabel);

    //班级卡片容器，居中显示
    QWidget* classcontainer = new QWidget();
    QVBoxLayout* classContainerLayout = new QVBoxLayout(classcontainer);
    classContainerLayout->setContentsMargins(0,0,0,0);
    classContainerLayout->addWidget(classCard);
    classContainerLayout->addStretch();
    classContainerLayout->setAlignment(Qt::AlignHCenter);
    setLayout->addWidget(classcontainer);

    //-------设置头像卡片-------
    QWidget* avatarCard = new QWidget();
    avatarCard->setStyleSheet("background-color:white;border-radois:10px;padding:25px;");
    avatarCard->setFixedWidth(500);
    QVBoxLayout *avatarLayout = new QVBoxLayout(avatarCard);
    avatarLayout->setSpacing(15);

    QLabel* avatarLabelTitle = new QLabel("更换头像：");
    avatarLabelTitle->setFont(QFont("Microsoft YaHei",14,QFont::Bold));
    avatarLabelTitle->setAlignment(Qt::AlignLeft);
    avatarLayout->addWidget(avatarLabelTitle);

    QLabel* avatarHint = new QLabel("当前头像显示在左侧导航栏，点击按钮选择本地图片。");
    avatarHint->setStyleSheet("color:#7f8c8d;font-size:13px;");
    avatarHint->setWordWrap(true);
    avatarLayout->addWidget(avatarHint);

    m_changeAvatarBtn = new QPushButton("🖼️ 选择图片");
    m_changeAvatarBtn->setFixedHeight(60);
    m_changeAvatarBtn->setCursor(Qt::PointingHandCursor);
    m_changeAvatarBtn->setStyleSheet("background-color:#9b59b6;color-white;font-weight:bold;font-size:20px;border-radius:6px;padding:0px;");
    connect(m_changeAvatarBtn,&QPushButton::clicked,this,&client::onChangeAvatarClicked);
    avatarLayout->addWidget(m_changeAvatarBtn);

    //头像卡片容器，居中
    QWidget* avatarContainer = new QWidget();
    QVBoxLayout* avatarContainerLayout = new QVBoxLayout(avatarContainer);
    avatarContainerLayout->setContentsMargins(0,0,0,0);
    avatarContainerLayout->addWidget(avatarCard);
    avatarContainerLayout->addStretch();
    avatarContainerLayout->setAlignment(Qt::AlignHCenter);
    setLayout->addWidget(avatarContainer);

    setLayout->addStretch();

    //创建滚动区域并包裹设置内容，使得设置页面可滚动
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidget(settingsContent);
    scrollArea->setWidgetResizable(true);//允许内容随窗口大小调整
    scrollArea->setFrameShape(QFrame::NoFrame);//去掉边框
    scrollArea->setStyleSheet("background-color:#f5f6fa;");
    //将滚动区域添加到堆叠窗口，而不是直接添加到settingsContent
    m_mainStack->addWidget(scrollArea);

    //将左侧导航栏和右侧堆叠窗口添加到主布局
    mainLayout->addWidget(m_leftPanel);
    mainLayout->addWidget(m_mainStack);
}

//=============文件列表交互==============
/**
 * @brief 文件树项目双击响应
 * @param item 被双击的树项
 * @param column 列索引（未使用）
 *
 * 行为：
 * - 如果是文件（非目录且不是".."），忽略（防止误触下载）
 * - 如果是".."：返回上级目录
 * - 如果是目录：进入该目录
 */
void client::onFileListItemDoubleClicked(QTreeWidgetItem* item)
{
    if(!item || !m_fileTree) return;//非空检查

    bool isDir = item->data(0,Qt::UserRole).toBool();//从UserRole获取是否为目录
    QString name = item->text(0);

    if(!isDir && name != "返回上一级目录/..")
    {
        return;
    }

    //只有进入目录或返回上级时，才显示加载状态并刷新
    m_fileTree->clear();
    QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
    loadingItem->setText(0,"正在加载...");
    loadingItem->setForeground(0,QColor("#7f8c8d"));

    if(name == "返回上一级目录/..")
    {
        //返回上级目录
        if(m_currentPath == "/" || m_currentPath.isEmpty())
        {
            qDebug()<<"[Nav] 已经是根目录了，忽略返回目录.";
            delete loadingItem;
            //在根目录点击..，重新加载根目录
            requestFileList(m_currentTargetIp,m_currentTargetPort,"/");
            return;
        }

        QString newPath = m_currentPath;
        //安全地去除末尾斜杠
        while(newPath.endsWith("/") && newPath.length() > 1)
        {
            newPath.chop(1);
        }

        int lastSlashIndex = newPath.lastIndexOf("/");

        if(lastSlashIndex <= 0)
        {
            newPath = "/";
        }
        else
        {
            newPath = newPath.left(lastSlashIndex);
            if(newPath.isEmpty())
            {
                newPath = "/";
            }
        }

        m_currentPath = newPath;
        qDebug()<<"[Nav] Going up to:"<<m_currentPath;

        //导航操作直接请求列表，不等待，消除卡顿
        requestFileList(m_currentTargetIp,m_currentTargetPort,m_currentPath);
        return;
    }

    if(isDir)
    {
        //进入目录
        QString nextPath;
        if(m_currentPath == "/" || m_currentPath.isEmpty())
        {
            nextPath = "/" + name;
        }
        else
        {
            nextPath = m_currentPath + "/" + name;
        }

        nextPath.replace("\\","/");
        while(nextPath.contains("//"))
        {
            nextPath.replace("//","/");
        }

        if(!nextPath.startsWith("/"))
        {
            nextPath = "/" + nextPath;
        }

        m_currentPath = nextPath;

        //导航操作直接请求列表
        requestFileList(m_currentTargetIp,m_currentTargetPort,m_currentPath);
    }
}

/**
 * @brief 文件树项目单击响应
 * @param item 被单击的树项
 * @param column 列索引（未使用）
 *
 * 根据选中项是否为文件（且不是".."）来启用或禁用下载按钮。
 */
void client::onFileListItemClicked(QTreeWidgetItem* item,int column)
{
    Q_UNUSED(column);
    bool isDir = item->data(0,Qt::UserRole).toBool();
    //只有非目录且不是".."的项才启用下载按钮
    m_downloadBtn->setEnabled(!isDir && item->text(0) != "返回上一级目录/..");
}

// ==================== 退出程序 ====================
/**
 * @brief 退出程序按钮点击响应
 */
void client::onQuitClicked()
{
    if(QMessageBox::question(this,"确认退出","确定要退出客户端吗？") == QMessageBox::Yes)
    {
        qApp->quit();//退出应用程序
    }
}

// ==================== 手动连接教师端 ====================
/**
 * @brief 手动连接教师端按钮点击响应
 *
 * 弹出输入对话框，让用户输入教师 IP 和 TCP 端口，然后尝试连接。
 */
void client::onManualConnectClicked()
{
    bool okIp = false;
    bool okPort = false;

    QString teacherIp = QInputDialog::getText(this,"手动连接教师端","请输入教师端IP地址:",QLineEdit::Normal,"",&okIp);
    if(!okIp || teacherIp.isEmpty()) return;

    quint16 teacherTcpPort = QInputDialog::getInt(this,"手动连接教师端","请输入教师端TCP端口:",20000,1024,65535,1,&okPort);
    if(!okPort) return;

    //保存手动连接信息
    m_manualTeacherIp = teacherIp;
    m_manualTeacherTcpPort = teacherTcpPort;

    //切换到共享文件界面
    m_navList->setCurrentRow(1);
    //触发更新(调用tryDirectConnect连接)
    onRefreshClicked();
}
