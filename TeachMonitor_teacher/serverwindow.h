#ifndef SERVERWINDOW_H
#define SERVERWINDOW_H

#include <QMainWindow>      // Qt 主窗口类，作为教师端主窗口的基类
#include <QPlainTextEdit>   // 纯文本编辑控件，用于显示文件传输日志
#include <QVBoxLayout>      // 垂直布局管理器，用于界面布局
#include <QStackedWidget>   // 堆叠窗口控件，用于切换三个主页面（监控、文件、设置）
#include <QTableWidget>     // 表格控件，用于显示学生监控信息（状态、违规次数等）
#include <QLabel>           // 标签控件，用于显示标题、统计信息、预览图等
#include <QPushButton>      // 按钮控件，用于导航、刷新、下载等操作
#include <QLineEdit>        // 单行输入框，用于设置共享路径、班级名称等
#include <QUdpSocket>       // UDP 套接字，用于学生发现（接收学生心跳）和广播用户列表
#include <QTcpServer>       // TCP 服务器，用于处理学生的文件列表/下载请求以及监控数据上报
#include <QTcpSocket>
#include <QTimer>           // 定时器，用于定时发送教师端心跳和清理超时学生
#include <QComboBox>        // 下拉框，用于班级筛选和删除班级选择
#include <QSettings>        //本地配置
#include "databasemanager.h"// 自定义数据库管理器（单例），负责所有数据库操作（学生、班级、日志、文件统计）

namespace Ui {
class ServerWindow;
}
//===================枚举定义==================
/**
 * @brief 学生状态枚举
 * 用于表示学生客户端当前所处的状态，供教师端监控界面展示
 */
enum class StudentStatus
{
    Offline,        //离线状态：学生未连接或心跳超时
    Online_Normal,  //在线正常：学生在线且未检测到违规行为
    Online_Warning, //本地警告中：学生端本地检测到疑似违规
    Online_Violated //已上报老师：学生以确认违规并向教师端发送警告
};

// ==================== ServerWindow 类 ====================

/**
 * @brief 教师端主窗口类
 *
 * 智慧教室监控系统的教师端主界面，负责：
 * - 显示所有学生的在线状态、违规情况、班级信息
 * - 提供实时屏幕查看功能
 * - 管理共享文件（浏览、下载）
 * - 班级管理（创建/删除班级、筛选班级成员）
 * - 接收 UDP 广播以发现学生端
 * - 处理 TCP 连接（文件传输、监控数据接收）
 *
 * 主要界面分为三个页面（通过 QStackedWidget 切换）：
 * 1. 实时监控页：学生列表表格，展示状态、违规应用、违规次数，支持双击查看屏幕
 * 2. 文件传输页：展示共享文件列表，供学生端浏览下载（实际此页在教师端仅作日志展示）
 * 3. 系统设置页：设置共享文件夹路径、创建/删除班级
 *
 * 网络通信：
 * - UDP：用于学生发现和心跳广播（监听 9999 端口，广播到 8889 端口）
 * - TCP：用于接收学生的监控数据（违规报告、截图）、处理文件下载请求
 */
class ServerWindow : public QMainWindow
{
    Q_OBJECT   // Qt 元对象宏，支持信号槽

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     *
     * 初始化 UI、数据库、网络组件（UDP、TCP 服务器），
     * 加载学生列表，启动心跳广播。
     */
    explicit ServerWindow(QWidget *parent = nullptr);

    /// 析构函数，停止服务器并释放资源
    ~ServerWindow();

protected:
    /**
     * @brief 重写窗口关闭事件
     * @param event 关闭事件对象
     *
     * 在关闭窗口前询问用户是否确认退出，若确认则停止服务器并接受关闭。
     */
    void closeEvent(QCloseEvent *event) override;

private slots:
    // ========== 导航切换槽函数 ==========

    /// 切换到“实时监控”页面
    void onNavMonitorClicked();

    /// 切换到“文件传输”页面
    void onNavFileClicked();

    /// 切换到“系统设置”页面
    void onNavSettingsClicked();

    /**
     * @brief 接收日志消息并在界面上显示
     * @param msg 日志文本
     */
    void onLogMessage(const QString &msg);

    /**
     * @brief 监控表格双击事件处理
     * @param row 行索引
     * @param col 列索引
     *
     * 仅当双击“查看屏幕”列（col == 6）时触发请求实时截图。
     */
    void onTableCellDoubleClicked(int row, int col);

    /**
     * @brief 请求实时截图（教师主动查看）
     * @param studentId 学生 ID
     *
     * 显示加载对话框，向学生端发送 GET_SCREENSHOT_NOW 命令，
     * 等待学生通过 TCP 返回 LIVE_RESPONSE 类型数据。
     */
    void onRequestLiveScreenshotClicked(const QString &studentId);

    //请求实时摄像头影像
    void onRequestCameraStream(const QString &studentId);

    // ========== UDP 发现与心跳相关 ==========

    /**
     * @brief UDP 套接字数据到达时的槽函数
     *
     * 解析 UDP 包：
     * - 如果是 HEARTBEAT 包：更新学生在线信息、班级信息、IP、TCP 端口
     * - 如果是 USER_LIST 包：更新用户列表及班级下拉框
     * 同时进行超时清理（超过 10 秒未收到心跳的学生标记为离线）。
     */
    void onUdpReadyRead();

    /**
     * @brief 定时发送教师端心跳广播
     *
     * 每 5 秒向学生端监听端口（8889）发送广播，告知教师端 IP、UDP 端口、TCP 端口，
     * 以便学生端发现教师端。
     */
    void sendServerHeartbeat();

    /**
     * @brief 广播当前在线用户列表和班级列表
     *
     * 将 g_onlineUsers 中的用户信息和数据库中的班级列表打包成 JSON，
     * 通过 UDP 广播发送给学生端，用于学生端更新好友列表和班级下拉框。
     */
    void broadcastUserList();

    // ========== 教师端文件服务器相关 ==========

    /**
     * @brief 处理新的文件服务连接（教师端作为文件服务器时）
     *
     * 当学生端请求文件列表或下载文件时，QTcpServer 会触发此槽函数。
     * 内部建立连接并处理 LIST|、DOWNLOAD| 等命令。
     */
    void onNewFileConnection();

    /**
     * @brief 处理来自学生端的文件相关命令
     * @param data 命令数据
     * @param socket 对应的 TCP 套接字
     *
     * 支持命令：GET_FILE_LIST / LIST|（获取文件列表）、DOWNLOAD|（下载文件）。
     * 处理完成后通过 socket 返回数据（JSON 数组或文件二进制流）。
     */
    void handlePeerCommand(const QByteArray &data, QTcpSocket *socket);

    // ========== 班级管理相关槽函数 ==========

    /// 创建班级按钮点击响应：从输入框读取班级名，调用数据库创建班级，刷新下拉框并广播
    void onCreateClassClicked();

    /// 删除班级按钮点击响应：从下拉框选择班级，确认后调用数据库删除，刷新下拉框并广播
    void onDeleteClassClicked();

    /// 查看班级成员按钮点击响应：根据筛选下拉框的班级刷新监控表格
    void onViewClassMembersClicked();

    /**
     * @brief 班级筛选下拉框选项改变时的响应
     * @param className 新选择的班级名称（或“全部班级”）
     *
     * 更新当前筛选状态并刷新监控表格。
     */
    void onClassComboBoxChanged(const QString &className);

    /**
     * @brief 状态过滤下拉框选项改变时的响应
     * @param status 新选择的状态
     *
     * 更新当前筛选状态并刷新监控表格。
     */
    void onStatusComboBoxChanged(const QString &status);

    void onNewCameraConnection();//处理学生端摄像头连接

    void onCameraStreamDisconnected();//学生端断开连接

private:
    // ========== 私有方法（UI 初始化） ==========

    /// 初始化主布局（左侧导航栏 + 右侧堆叠窗口）
    void setupMainLayout();

    /// 初始化“实时监控”页面（表格、筛选栏、详情区）
    void setupMonitorPage();

    /// 初始化“文件传输”页面（日志显示区域，原有文件列表逻辑复用在此）
    void setupFilePage();

    /// 初始化“系统设置”页面（共享路径设置、班级管理）
    void setupSettingsPage();

    // ========== 私有方法（业务逻辑） ==========

    /**
     * @brief 更新或添加监控表格中的一行
     * @param id 学生 ID
     * @param name 学生姓名/昵称
     * @param status 学生状态
     * @param app 违规应用名称（若无违规则为空）
     * @param screenshot 截图数据（本函数内未使用，仅保留接口）
     *
     * 根据当前筛选条件（m_currentFilterClass）决定是否显示该学生，
     * 若通过筛选则更新表格的 7 列：IP、姓名、班级、状态、违规应用、违规次数、查看屏幕。
     */
    void updateStudentTableRow(const QString &id, const QString &name, StudentStatus status, const QString &app);

    /**
     * @brief 显示截图弹窗
     * @param studentName 学生姓名（用于窗口标题）
     * @param imageData 截图二进制数据（JPEG 格式）
     *
     * 创建一个模态对话框，展示缩放后的截图，供教师详细查看。
     */
    void showScreenshotDialog(const QString &studentName, const QByteArray &imageData);

    /**
     * @brief 显示正在请求实时屏幕的加载对话框
     * @param studentName 学生姓名
     * @return QDialog* 对话框指针，用于后续关闭
     *
     * 对话框包含不确定进度条和提示文本，直到收到学生响应或超时。
     */
    QDialog* showLoadingDialog(const QString &studentName);

    /// 更新界面顶部的在线人数和违规警告数统计
    void updateOnlineStats();

    /**
     * @brief 向学生端发送获取实时截图的命令
     * @param studentIp 学生 IP
     * @param studentTcpPort 学生 TCP 端口（用于建立命令连接）
     *
     * 建立 TCP 连接，发送 "GET_SCREENSHOT_NOW" 命令，然后断开。
     * 学生端收到后会立即捕获屏幕并通过 TCP 返回（协议为 MONITOR_START + 二进制数据）。
     */
    void sendGetScreenshotCommand(const QString &studentIp, quint16 studentTcpPort);

    /**
     * @brief 处理违规报告的核心逻辑
     * @param studentId 学生 ID
     * @param appName 违规应用名称
     * @param imageData 截图数据（可为空）
     * @param timeStr 时间字符串
     *
     * 调用 DatabaseManager 插入监控记录，并输出日志。
     */
    void processViolationReport(const QString &studentId, const QString &appName,
                                const QByteArray &imageData, const QString &timeStr);

    /**
     * @brief 刷新监控表格（支持按班级过滤）
     * @param filterClass 班级名称或空字符串（空字符串视为“全部班级”）
     *
     * 从数据库获取所有学生，结合 g_onlineUsers 的在线状态和班级信息，
     * 根据筛选条件过滤后调用 updateStudentTableRow 重建表格。
     */
    void refreshMonitorTable(const QString &filterClass = "");

    /**
     * @brief 统一刷新班级下拉框
     *
     * 同时刷新设置页的“删除班级”下拉框和监控页的“班级筛选”下拉框，
     * 从数据库重新加载班级列表并保持当前选中项（如果仍存在）。
     */
    void refreshClassComboBoxes();

    // ========== 成员变量 ==========
    // ---- 核心组件 ----
    QStackedWidget *m_stackedWidget;   // 右侧内容堆叠窗口（管理三个页面）
    QPlainTextEdit *m_logView;         // 日志显示控件（文件传输页面中）

    // ---- 监控页面组件 ----
    QTableWidget *m_monitorTable;          // 学生监控表格
    QMap<QString, int> m_studentRowMap;    // 学生 ID -> 表格行索引的映射，用于快速定位

    // ---- 班级筛选组件 ----
    QComboBox *m_classFilterCombo;         // 班级筛选下拉框（监控页面）
    QPushButton *btnViewClass;             // “刷新班级视图”按钮
    QComboBox* m_statusFilterCombo;        //在线/离线状态过滤下拉框

    // ---- 设置页面组件 ----
    QLineEdit *m_sharePathEdit;            // 共享文件夹路径输入框
    // 设置页班级创建组件
    QLineEdit *m_newClassNameEdit;         // 新班级名称输入框
    QPushButton *btnCreateClass;           // 创建班级按钮
    // 设置页班级删除组件
    QComboBox *m_deleteClassCombo;         // 要删除的班级下拉框
    QPushButton *btnDeleteClass;           // 删除班级按钮

    // ---- 网络组件（用于 UDP 发现和教师端文件服务器）----
    QUdpSocket *m_udpSocket;               // UDP 套接字（监听 9999 端口，用于学生发现）
    QTimer *m_heartbeatTimer;              // 心跳定时器（每 5 秒发送教师端广播）
    QTcpServer *m_fileServer;              // 教师端文件服务器（用于处理学生的文件请求）

    // ---- 教师端自身信息 ----
    QString m_myIp;         // 教师端本机 IP 地址
    quint16 m_myUdpPort;    // 教师端 UDP 端口（固定 9999）
    quint16 m_myTcpPort;    // 教师端 TCP 文件服务端口（随机生成，用于学生连接）

    // ---- 导航按钮成员变量 ----
    QPushButton *btnMonitor;    // 导航栏“实时监控”按钮
    QPushButton *btnFile;       // 导航栏“文件传输”按钮
    QPushButton *btnSettings;   // 导航栏“系统设置”按钮

    // ---- 违规冷却机制 ----
    /**
     * @brief 记录每个学生最后一次恢复正常的时间戳（毫秒）
     *
     * 用于实现 10 秒冷却期：学生在恢复正常后的 10 秒内再次违规不计入次数，
     * 避免短时间内频繁上报（例如关闭违规应用后马上打开另一个）。
     */
    QMap<QString, qint64> m_lastRecoverTime;

    // ---- 加载对话框管理 ----
    /**
     * @brief 存储正在等待实时截图的加载对话框，键为学生 ID
     *
     * 当教师请求实时截图时创建加载对话框，收到学生响应后关闭对应对话框。
     */
    QMap<QString, QDialog*> m_loadingDialogs;

    // ---- 当前筛选的班级名称 ----
    /**
     * @brief 当前筛选的班级名称（用于控制 updateStudentTableRow 的显示逻辑）
     *
     * 可选值：“全部班级” 或具体的班级名称。
     * 当 m_currentFilterClass 不为“全部班级”时，只显示属于该班级的学生。
     */
    QString m_currentFilterClass;

    //当前筛选的学生状态
    QString m_currentFilterStatus;

    //摄像头部分
    QTcpServer *m_cameraServer; //临时TCP服务器用于接收视频流
    QDialog *m_cameraDialog;    //视频显示对话框
    QLabel *m_cameraLabel;      //显示视频帧的标签
    QTcpSocket *m_cameraSocket; //当前视频流连接
    void startCameraServer();   //启动临时TCP服务器
    struct CameraStreamContext {
        QByteArray buffer;          // 未完成数据的临时缓存
        quint32 expectedFrameSize = 0;  // 正在接收的帧大小（0 表示等待帧头）
    };
};

#endif // SERVERWINDOW_H
