#include <QApplication>
#include <QDebug>
#include "serverwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 创建并显示主窗口（内部已包含服务器逻辑）
    ServerWindow w;
    w.show();

    qDebug() << "Server GUI Application running...";

    return a.exec();
}
