#ifndef SERVERWINDOW_H
#define SERVERWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QComboBox> // 【新增】用于班级选择
#include "fileserver.h"
#include "databasemanager.h"
#include "clienthandler.h"

namespace Ui {
class ServerWindow;
}

// 【新增】自定义可点击 Label 类，用于解决 protected 成员访问错误
class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {}
    ~ClickableLabel() override {}

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *ev) override {
        QLabel::mousePressEvent(ev);
        emit clicked();
    }
};

class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ServerWindow(QWidget *parent = nullptr);
    ~ServerWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 导航切换
    void onNavMonitorClicked();
    void onNavFileClicked();
    void onNavSettingsClicked();
    void onLogMessage(const QString &msg);
    void onStudentStatusUpdated(const QString &id, StudentStatus status, const QString &appName, const QByteArray &screenshot);
    void onTableCellDoubleClicked(int row, int col);
    
    // 【修改】移除查看历史声明，统一使用请求实时截图逻辑
    // void onViewViolationHistoryClicked(const QString &studentId); // 删除
    
    // 【修改】响应查看屏幕按钮点击，执行“显示缓存 + 请求实时”逻辑
    void onRequestLiveScreenshotClicked(const QString &studentId);
    
    // 新增：UDP 相关槽函数
    void onUdpReadyRead();
    void sendServerHeartbeat();
    void broadcastUserList();
    
    // 新增：处理教师端作为文件服务器的请求
    void onNewFileConnection();
    void handlePeerCommand(const QByteArray &data, QTcpSocket *socket);

    // 【新增】班级管理相关槽函数
    void onCreateClassClicked();
    void onDeleteClassClicked(); // 【新增】删除班级槽函数
    void onViewClassMembersClicked();
    void onClassComboBoxChanged(const QString &className);

private:
    void setupMainLayout();
    void setupMonitorPage();
    void setupFilePage();
    void setupSettingsPage();
    void updateStudentTableRow(const QString &id, const QString &name, StudentStatus status, const QString &app, const QByteArray &screenshot);
    void showScreenshotDialog(const QString &studentName, const QByteArray &imageData);
    QDialog* showLoadingDialog(const QString &studentName);
    void updateOnlineStats();
    void sendGetScreenshotCommand(const QString &studentIp, quint16 studentTcpPort);
    
    // 【新增】处理违规报告的核心逻辑（写库 + 日志）
    void processViolationReport(const QString &studentId, const QString &appName, 
                                const QByteArray &imageData, const QString &timeStr);
    
    // 【新增】刷新监控表格（支持按班级过滤）
    void refreshMonitorTable(const QString &filterClass = "");
    
    // 【新增】统一刷新班级下拉框
    void refreshClassComboBoxes();

    Ui::ServerWindow *ui;
    
    // 核心组件
    QStackedWidget *m_stackedWidget;
    QPlainTextEdit *m_logView;
    FileServer *m_server;
    
    // 监控页面组件
    QTableWidget *m_monitorTable;
    QLabel *m_detailLabel;
    QLabel *m_screenshotPreview;
    QMap<QString, int> m_studentRowMap;
    
    // 【新增】班级筛选组件
    QComboBox *m_classFilterCombo;
    QPushButton *btnViewClass;

    // 设置页面组件
    QLineEdit *m_sharePathEdit;
    // 【新增】设置页班级创建组件
    QLineEdit *m_newClassNameEdit;
    QPushButton *btnCreateClass;
    // 【新增】设置页班级删除组件
    QComboBox *m_deleteClassCombo;
    QPushButton *btnDeleteClass;

    // 新增：网络组件
    QUdpSocket *m_udpSocket;
    QTimer *m_heartbeatTimer;
    QTcpServer *m_fileServer;
    
    // 新增：教师端信息
    QString m_myIp;
    quint16 m_myUdpPort;
    quint16 m_myTcpPort;

    // 【新增】导航按钮成员变量
    QPushButton *btnMonitor;
    QPushButton *btnFile;
    QPushButton *btnSettings;

    // 【新增】记录每个学生最后一次恢复正常的时间戳，用于实现 10s 冷却期
    QMap<QString, qint64> m_lastRecoverTime;
    
    // 【新增】存储正在等待实时截图的加载对话框，以便收到数据后关闭
    QMap<QString, QDialog*> m_loadingDialogs;

    // 【新增】当前筛选的班级名称，用于控制 updateStudentTableRow 的显示逻辑
    QString m_currentFilterClass;
};

#endif // SERVERWINDOW_H