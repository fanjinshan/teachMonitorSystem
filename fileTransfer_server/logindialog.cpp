#include "logindialog.h"
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QTimer>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle("智慧教室监控系统 - 教师端登录");
    // 【修改】移除固定尺寸，改为最小尺寸，允许窗口自适应内容或拉伸
    this->setMinimumSize(400, 500);
    // 【新增】设置初始大小，确保默认展示效果良好
    this->resize(420, 340);
    this->setModal(true);
    
    // 设置整体背景色
    this->setStyleSheet("QDialog { background-color: #f0f2f5; }");

    // --- 主布局 ---
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    // 【优化】减小主布局边距，防止窗口边缘留白过多导致内容挤压
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // --- 卡片容器 (白色背景，圆角) ---
    m_cardFrame = new QFrame();
    m_cardFrame->setStyleSheet(R"(
        QFrame {
            background-color: white;
            border-radius: 15px;
            border: 1px solid #e1e4e8;
        }
    )");
    // 添加阴影效果
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 30));
    shadow->setOffset(0, 4);
    m_cardFrame->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(m_cardFrame);
    // 【优化】调整卡片内部边距和间距，使布局更舒展
    cardLayout->setContentsMargins(30, 25, 30, 25);
    cardLayout->setSpacing(15);

    // --- 顶部图标和标题 ---
    m_iconLabel = new QLabel("🔒");
    m_iconLabel->setAlignment(Qt::AlignCenter);
    // 【优化】稍微减小图标字号，防止撑大布局
    m_iconLabel->setStyleSheet("font-size: 40px; color: #3498db; margin-bottom: 5px;border: none;");
    
    m_titleLabel = new QLabel("教师端身份验证");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    // 【优化】确保标题换行正常，字号适中
    m_titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #2c3e50; margin-bottom: 5px;border: none;");
    
    QLabel *subTitle = new QLabel("请输入管理员账号和密码继续");
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setStyleSheet("font-size: 12px; color: #7f8c8d; margin-bottom: 10px;border: none;");
    // 【新增】允许副标题根据宽度自动换行
    subTitle->setWordWrap(true);

    cardLayout->addWidget(m_iconLabel);
    cardLayout->addWidget(m_titleLabel);
    cardLayout->addWidget(subTitle);
    cardLayout->addSpacing(5);

    // --- 输入框区域 ---
    QString inputStyle = R"(
        QLineEdit {
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #333;
            background-color: #fafafa;
            min-height: 36px;
        }
        QLineEdit:focus {
            border-color: #3498db;
            background-color: white;
            outline: none;
        }
        QLineEdit:hover {
            border-color: #c0c4cc;
        }
    )";

    // 账号行
    QWidget *userRow = new QWidget();
    QHBoxLayout *userLayout = new QHBoxLayout(userRow);
    userLayout->setContentsMargins(0, 0, 0, 0);
    userLayout->setSpacing(10);
    
    QLabel *userIcon = new QLabel("👤");
    userIcon->setStyleSheet("font-size: 16px; color: #909399; min-width: 20px;");
    userIcon->setAlignment(Qt::AlignCenter);
    
    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText("请输入账号");
    m_usernameEdit->setStyleSheet(inputStyle);
    
    userLayout->addWidget(userIcon);
    userLayout->addWidget(m_usernameEdit);

    // 密码行
    QWidget *passRow = new QWidget();
    QHBoxLayout *passLayout = new QHBoxLayout(passRow);
    passLayout->setContentsMargins(0, 0, 0, 0);
    passLayout->setSpacing(10);
    
    QLabel *passIcon = new QLabel("🔑");
    passIcon->setStyleSheet("font-size: 16px; color: #909399; min-width: 20px;");
    passIcon->setAlignment(Qt::AlignCenter);
    
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setPlaceholderText("请输入密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setStyleSheet(inputStyle);
    
    passLayout->addWidget(passIcon);
    passLayout->addWidget(m_passwordEdit);

    cardLayout->addWidget(userRow);
    cardLayout->addWidget(passRow);
    cardLayout->addSpacing(5);

    // --- 按钮区域 ---
    QString btnCommonStyle = R"(
        QPushButton {
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
            padding: 8px;
            min-height: 36px;
        }
        QPushButton:hover {
            opacity: 0.9;
        }
        QPushButton:pressed {
            opacity: 0.8;
        }
    )";

    m_loginBtn = new QPushButton("立即登录");
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setMinimumWidth(120);
    m_loginBtn->setFixedHeight(40);
    m_loginBtn->setStyleSheet(btnCommonStyle + "background-color: #3498db; color: #666;");

    m_cancelBtn = new QPushButton("取消");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setMinimumWidth(120);
    m_cancelBtn->setFixedHeight(40);
    m_cancelBtn->setStyleSheet(btnCommonStyle + "background-color: #e0e0e0; color: #666;");

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);
    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_cancelBtn);
    
    btnLayout->setStretchFactor(m_loginBtn, 1);
    btnLayout->setStretchFactor(m_cancelBtn, 1);

    cardLayout->addLayout(btnLayout);

    // 将卡片加入主布局
    mainLayout->addWidget(m_cardFrame);

    // 连接信号
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        this->reject();
    });
    
    // 支持回车键登录
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    
    // 默认聚焦到账号输入框
    m_usernameEdit->setFocus();
}

LoginDialog::~LoginDialog()
{
}

void LoginDialog::onLoginClicked()
{
    QString user = m_usernameEdit->text().trimmed();
    QString pass = m_passwordEdit->text();

    if (user.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, "提示", "账号或密码不能为空！");
        return;
    }

    if (user == VALID_USER && pass == VALID_PASS) {
        m_loginBtn->setText("登录成功...");
        m_loginBtn->setStyleSheet("background-color: #2ecc71; color: white; border-radius: 6px; font-size: 14px; font-weight: bold; padding: 8px; min-height: 36px;");
        QTimer::singleShot(500, this, [this]() {
            this->accept();
        });
    } else {
        QMessageBox::warning(this, "登录失败", "账号或密码错误！\n请检查后重新输入。");
        m_passwordEdit->clear();
        m_passwordEdit->setFocus();
        
        m_cardFrame->setStyleSheet("QFrame { background-color: white; border-radius: 15px; border: 1px solid #e74c3c; }");
        QTimer::singleShot(2000, this, [this]() {
             m_cardFrame->setStyleSheet("QFrame { background-color: white; border-radius: 15px; border: 1px solid #e1e4e8; }");
        });
    }
}
