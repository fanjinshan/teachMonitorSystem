#include <QApplication>
#include <QDebug>
#include "serverwindow.h"
#include "logindialog.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 【新增】首先显示登录对话框
    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted) {
        // 如果用户点击取消或关闭登录框，直接退出程序
        qDebug() << "User cancelled login. Exiting...";
        return 0;
    }

    // 登录成功后，创建并显示主窗口
    ServerWindow w;
    w.show();

    qDebug() << "Server GUI Application running after successful login...";

    return a.exec();
}