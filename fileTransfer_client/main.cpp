#include "client.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置全局样式，使其更像现代 IM 软件
    a.setStyle(QStyleFactory::create("Fusion"));

    client w;
    w.show();
    return a.exec();
}