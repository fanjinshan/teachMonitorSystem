#include <QApplication>
#include <QDebug>
#include "logindialog.h"
#include "serverwindow.h"
int main(int argc,char* argv[])
{
    QApplication a(argc,argv);

    //首先显示登录对话框
    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted) {
        // 如果用户点击取消或关闭登录框，直接退出程序
        qDebug() << "用户取消登录. 退出中...";
        return 0;
    }

    ServerWindow w;
    w.show();


    return a.exec();
}
