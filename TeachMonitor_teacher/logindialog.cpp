#include "logindialog.h"
#include <QApplication>
//包含图形阴影效果类，给卡片容器添加阴影
#include <QGraphicsDropShadowEffect>
#include <QTimer>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle("教师监控管理系统 - 教师端登录");
    this->setMinimumSize(400,500);
    //设置初始大小
    this->resize(420,340);
    this->setModal(true);//设置为模态对话框（阻塞其他窗口交互）
    //设置整个对话框的整体背景色为浅灰色
    this->setStyleSheet("QDialog {background-color: #f0f2f5;}");

    //主布局(垂直布局)
    QVBoxLayout *mainLayout = new QVBoxLayout(this);//设置垂直布局管理器
    mainLayout->setContentsMargins(20,20,20,20);//设置布局的四个方向的边距均为20像素
    mainLayout->setSpacing(15);

    //卡片容器(白色背景，圆角)
    m_cardFrame = new QFrame();
    //设置卡片容器的样式：白色背景，圆角15像素，边框1像素实现浅灰色
    m_cardFrame->setStyleSheet(R"(
        QFrame {
            background-color: white;
            border-radius:15px;
            border:1px solid #e1e4e8;
        }
    )");
    //添加阴影效果，使卡片浮起来
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);//设置阴影模糊半径为20像素
    shadow->setColor(QColor(0,0,0,30));//设置阴影颜色为黑色，透明度为30(0-255)
    shadow->setOffset(0,4);//设置阴影偏移量：水平0，垂直4像素
    m_cardFrame->setGraphicsEffect(shadow);//将阴影效果应用到卡片容器

    //卡片容器内部的垂直布局
    QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
    cardLayout->setContentsMargins(30,25,30,25);//内部边距，左30，上25，右30，下25
    cardLayout->setSpacing(15);//内部空间间距15像素

    //---顶部图标和标题---
    m_iconLabel = new QLabel("🔒"); //创建图标标签
    m_iconLabel->setAlignment(Qt::AlignCenter);//设置标签内容居中对齐
    m_iconLabel->setStyleSheet("font-size:40px;color:#3498db;margin-bottom:5px;border:none;");

    m_titleLabel = new QLabel("教师端身份验证");
    m_titleLabel->setAlignment(Qt::AlignCenter);//居中对齐
    m_titleLabel->setStyleSheet("font-size:20px;font-weight:bold;color:#2c3e50;margin-bottom:5px;border:none;");

    QLabel *subTitle = new QLabel("请输入管理员账号和密码继续");//创建副标题标签
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setStyleSheet("font-size:12px;color:#7f8c8d;margin-bottom:10px;border:none;");
    //允许副标题根据宽度自动换行
    subTitle->setWordWrap(true);

    //将顶部图标、标题、副标题依次添加到卡片布局中
    cardLayout->addWidget(m_iconLabel);
    cardLayout->addWidget(m_titleLabel);
    cardLayout->addWidget(subTitle);
    cardLayout->addSpacing(5);//添加5像素额外空白

    //---输入框区域（账号和密码）---
    //定义输入框的通用样式（QSS）
    QString inputStyle = R"(
        QLineEdit{
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            padding:8px 12px;
            font-size:13px;
            color:#333;
            background-color:#fafafa;
            min-height:36px;
        }
        QLineEdit:focus{
            border-color:#3498db;
            background-color:white;
            outline:none;
        }
        QLineEdit:hover{
            border-color:#c0c4cc;
        }
    )";//focus:当输入框获得焦点时，hover：当鼠标悬停在输入框上时

    //账号行（包含图标和输入框）
    QWidget *userRow = new QWidget();// 创建一个容器Widget
    QHBoxLayout *userLayout = new QHBoxLayout(userRow);//水平布局
    userLayout->setContentsMargins(0,0,0,0);
    userLayout->setSpacing(10);//图标和输入框间距10像素

    QLabel *userIcon = new QLabel("👤");//用户图标
    userIcon->setStyleSheet("font-size: 36px;color:#909399;min-width:20px;border:none");
    userIcon->setAlignment(Qt::AlignCenter);

    m_usernameEdit = new QLineEdit();//用户名输入框
    m_usernameEdit->setPlaceholderText("请输入账号");//设置占位提示符
    m_usernameEdit->setStyleSheet(inputStyle);//应用通用输入框样式

    userLayout->addWidget(userIcon);
    userLayout->addWidget(m_usernameEdit);

    //密码行（包含图标和输入框）
    QWidget* passRow = new QWidget();
    QHBoxLayout *passLayout = new QHBoxLayout(passRow);//此时 passRow 的父对象会被自动设置为 mainLayout 的父窗口（即 this）,被当作一个容器使用
    passLayout->setContentsMargins(0,0,0,0);
    passLayout->setSpacing(10);

    QLabel *passIcon = new QLabel("🔑");//密码图标
    passIcon->setStyleSheet("font-size:36px;color:#909399;min-width:20px;border:none");
    passIcon->setAlignment(Qt::AlignCenter);

    m_passwordEdit = new QLineEdit();//密码输入框
    m_passwordEdit->setPlaceholderText("请输入密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);//设置回显模式为密码模式
    m_passwordEdit->setStyleSheet(inputStyle);

    passLayout->addWidget(passIcon);
    passLayout->addWidget(m_passwordEdit);

    //将账号行和密码行添加到卡片布局
    cardLayout->addWidget(userRow);
    cardLayout->addWidget(passRow);
    cardLayout->addSpacing(10);//图标和输入栏的距离

    //---按钮区域（登录和取消）---
    //定义按钮的通用样式
    QString btnCommonStyle = R"(
        QPushButton {
            border-radius:6px;
            font-size:14px;
            font-weight:bold;
            padding:8px;
            min-height:36px;
        }
        QPushButton:hover
        {
            opacity:0.9;
        }
        QPushButton:pressed
        {
            opacity:0.8;
        }
    )";//opacity:不透明度

    m_loginBtn = new QPushButton("立即登录");
    m_loginBtn->setCursor(Qt::PointingHandCursor);//鼠标悬停时变为手型
    m_loginBtn->setMinimumWidth(120);//最小宽度120像素
    m_loginBtn->setFixedHeight(40);//设置固定高度40像素
    //样式：通用样式+蓝色背景，深灰色文字
    m_loginBtn->setStyleSheet(btnCommonStyle + "background-color:#3498db;color:#666;");

    m_cancelBtn = new QPushButton("取消");//取消按钮
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumWidth(120);//最小宽度120像素
    m_cancelBtn->setFixedHeight(40);//设置固定高度40像素
    m_cancelBtn->setStyleSheet(btnCommonStyle + "background-color: #e0e0e0;color : #666");

    QHBoxLayout * btnLayout = new QHBoxLayout();//水平布局容纳两个按钮
    btnLayout->setSpacing(20);//设置两按钮间距
    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_cancelBtn);

    //设置伸缩因子（让两按钮均匀伸缩）
    btnLayout->setStretchFactor(m_loginBtn,1);
    btnLayout->setStretchFactor(m_cancelBtn,1);

    cardLayout->addLayout(btnLayout);//将按钮布局添加到卡片布局

    //将卡片容器添加到主布局
    mainLayout->addWidget(m_cardFrame);

    //============信号与槽连接===========
    //登录按钮点击->调用onLoginClicked槽函数
    connect(m_loginBtn,&QPushButton::clicked,this,&LoginDialog::onLoginClicked);
    //取消按钮点击->调用reject关闭对话框并返回QDialog::Rejected
    connect(m_cancelBtn,&QPushButton::clicked,this,[this](){
        this->rejected();
    });

    //支持回车键登录：当密码输入框按下回车时，触发登录
    connect(m_passwordEdit,&QLineEdit::returnPressed,this,&LoginDialog::onLoginClicked);
    //默认聚焦到账号输入框，方便用户直接输入
    m_usernameEdit->setFocus();
}

//Qt父子对象机制会自动释放子控件，暂时不需要析构函数
LoginDialog::~LoginDialog()
{
}

void LoginDialog::onLoginClicked()
{
    //获取用户名（取出首尾空格）和密码（保留原始输入）
    QString user = m_usernameEdit->text().trimmed();//移除字符串开头和结尾的空白字符
    QString pass = m_passwordEdit->text();

    //校验：账号密码不能为空
    if(user == VALID_USER && pass == VALID_PASS)
    {
        //登录成功：更改登录按钮文字和样式，提示用户
        m_loginBtn->setText("登录成功");
        m_loginBtn->setStyleSheet("background-color:#2ecc71;color:white;border-radius:6px;font-size:14px;font-weight:bold;padding:8px;min-height:36px;");
        //延迟500毫秒再结束对话框（返回QDialog::Accepted）,给用户视觉反馈
        QTimer::singleShot(500,this,[this](){
            this->accept();
        });
    }
    else//登录失败，弹出警告消息框
    {
        //临时将卡片边框设置为红色，提示用户输入有误
        m_cardFrame->setStyleSheet("QFrame {background:white;border-radius:15px;border:1px solid #e74c3c;border-width:2px;}");
        QMessageBox::warning(this,"登录失败","账号或密码错误！\n请检查后重新输入");
        //清空密码输入框，并把焦点设置到密码输入框
        m_passwordEdit->clear();
        m_passwordEdit->setFocus();

        //延迟两秒后恢复原始颜色
        QTimer::singleShot(500,this,[this](){
            m_cardFrame->setStyleSheet("QFrame {background:white;border-radius:15px;border:1px solid #e1e4e8;}") ;
        });
    }
}
