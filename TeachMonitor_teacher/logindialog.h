#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFrame>

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget * parent = nullptr);
    ~LoginDialog();

private slots:
    void onLoginClicked();
private:
    //UI组件
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QPushButton *m_loginBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_titleLabel;//标题标签
    QLabel *m_iconLabel;//图标标签
    QFrame *m_cardFrame;//卡片容器

    //账号密码
    const QString VALID_USER = "fan";
    const QString VALID_PASS = "123";
};

#endif // LOGINDIALOG_H
