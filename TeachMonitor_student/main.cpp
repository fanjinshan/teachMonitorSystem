#include "client.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc,char* argv[])
{
    QApplication a(argc,argv);

    //设置全局样式
    a.setStyle(QStyleFactory::create("Fusion"));

    client w;
    w.show();
    return a.exec();
}
