/********************************************************************************
** Form generated from reading UI file 'client.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CLIENT_H
#define UI_CLIENT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_client
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLineEdit *account;
    QLineEdit *passwordEdit;
    QPushButton *loginBt;
    QTreeWidget *fileList;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *settingBt;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *downLoadBt;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *refreshBt;
    QSpacerItem *horizontalSpacer;
    QPushButton *quitBt;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *client)
    {
        if (client->objectName().isEmpty())
            client->setObjectName(QString::fromUtf8("client"));
        client->resize(796, 585);
        centralwidget = new QWidget(client);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        account = new QLineEdit(centralwidget);
        account->setObjectName(QString::fromUtf8("account"));

        horizontalLayout->addWidget(account);

        passwordEdit = new QLineEdit(centralwidget);
        passwordEdit->setObjectName(QString::fromUtf8("passwordEdit"));

        horizontalLayout->addWidget(passwordEdit);

        loginBt = new QPushButton(centralwidget);
        loginBt->setObjectName(QString::fromUtf8("loginBt"));

        horizontalLayout->addWidget(loginBt);


        verticalLayout->addLayout(horizontalLayout);

        fileList = new QTreeWidget(centralwidget);
        QTreeWidgetItem *__qtreewidgetitem = new QTreeWidgetItem();
        __qtreewidgetitem->setText(0, QString::fromUtf8("1"));
        fileList->setHeaderItem(__qtreewidgetitem);
        fileList->setObjectName(QString::fromUtf8("fileList"));

        verticalLayout->addWidget(fileList);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        settingBt = new QPushButton(centralwidget);
        settingBt->setObjectName(QString::fromUtf8("settingBt"));

        horizontalLayout_2->addWidget(settingBt);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_2);

        downLoadBt = new QPushButton(centralwidget);
        downLoadBt->setObjectName(QString::fromUtf8("downLoadBt"));

        horizontalLayout_2->addWidget(downLoadBt);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_3);

        refreshBt = new QPushButton(centralwidget);
        refreshBt->setObjectName(QString::fromUtf8("refreshBt"));

        horizontalLayout_2->addWidget(refreshBt);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        quitBt = new QPushButton(centralwidget);
        quitBt->setObjectName(QString::fromUtf8("quitBt"));

        horizontalLayout_2->addWidget(quitBt);


        verticalLayout->addLayout(horizontalLayout_2);

        client->setCentralWidget(centralwidget);
        menubar = new QMenuBar(client);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 796, 20));
        client->setMenuBar(menubar);
        statusbar = new QStatusBar(client);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        client->setStatusBar(statusbar);

        retranslateUi(client);

        QMetaObject::connectSlotsByName(client);
    } // setupUi

    void retranslateUi(QMainWindow *client)
    {
        client->setWindowTitle(QCoreApplication::translate("client", "client", nullptr));
        account->setPlaceholderText(QCoreApplication::translate("client", "\350\264\246\345\217\267", nullptr));
        passwordEdit->setPlaceholderText(QCoreApplication::translate("client", "\345\257\206\347\240\201", nullptr));
        loginBt->setText(QCoreApplication::translate("client", "\347\231\273\345\275\225", nullptr));
        settingBt->setText(QCoreApplication::translate("client", "\350\256\276\347\275\256", nullptr));
        downLoadBt->setText(QCoreApplication::translate("client", "\344\270\213\350\275\275", nullptr));
        refreshBt->setText(QCoreApplication::translate("client", "\345\210\267\346\226\260", nullptr));
        quitBt->setText(QCoreApplication::translate("client", "\351\200\200\345\207\272", nullptr));
    } // retranslateUi

};

namespace Ui {
    class client: public Ui_client {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CLIENT_H
