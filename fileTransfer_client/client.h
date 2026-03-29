#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket> 
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QTimer>
#include <QTcpServer>
#include <QDialog>
#include <QJsonObject>
#include <QSplitter>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QFile>
#include <QDataStream>
#include <QByteArray>
#include <QHostAddress>
#include <QPointer>
#include <QComboBox>

// 【新增】黑名单配置：进程名或窗口标题包含的关键字 (不区分大小写)
// 策略：覆盖全称、简称、中文、英文、带.exe 后缀、常见别名等多种情况
static const QStringList BLACKLIST_APPS = {
    // --- 短视频/直播 (重点扩充) ---
    "douyin", "dy", "抖音", "douyin.exe", "dy.exe", "tiktok",
    "bilibili", "b 站", "哔哩哔哩", "bilibili.exe", "blive", "livehime",
    "kuaishou", "快手", "kwai", "gifshow",
    "huya", "虎牙", "yy", "duowan",
    "douyu", "斗鱼", "dylive",
    "inke", "花椒", "huajiao",
    "longzhu", "龙珠", "panda", "熊猫",
    
    // --- 游戏类 (平台 + 热门游戏) ---
    "game", "steam", "steamwebhelper", "wegame", "tgp",
    "epic games", "epicgameslauncher", "origin", "uplay", "ubisoft connect",
    "blizzard", "battle.net", "战网",
    "league of legends", "lol", "英雄联盟",
    "valorant", "无畏契约", "riot client",
    "crossfire", "cf", "穿越火线",
    "overwatch", "守望先锋",
    "apex", "apex legends", "重生引擎",
    "pubg", "绝地求生", "playerunknown",
    "genshin", "原神", "yuanshen", "mhypbase",
    "honkai", "崩坏", "star rail", "星穹铁道",
    "naraka", "永劫无间",
    "dota2", "dota 2",
    "fifa", "nba2k", "gta", "grand theft auto",
    "minecraft", "我的世界", "mc",
    "roblox", "robloxplayer",
    "king of glory", "王者荣耀", "honor of kings",
    "peacekeeper elite", "和平精英", "game for peace",
    "qq speed", "qq 飞车",
    "dnf", "地下城与勇士", "dungeon and fighter",
    
    // --- 社交/娱乐/社区 (重点扩充) ---
    "wechat", "微信", "weixin", "wechat.exe", "weixin.exe", "wework", "企业微信",
    "qq", "txqq", "tencentqq", "qq.exe", "tim", "tim.exe",
    "qzone", "qq 空间",
    "weibo", "微博", "sina weibo", "xiaohongshu", "小红书", "xhs",
    "zhihu", "知乎",
    "douban", "豆瓣",
    "tieba", "百度贴吧", "baidutieba",
    "soul", "陌陌", "momo", "tantan", "探探",
    "discord", "telegram", "tg", "whatsapp", "snapchat",
    "instagram", "ins", "facebook", "fb", "twitter", "x.com",
    
    // --- 购物/消费 ---
    "taobao", "淘宝", "tb", "aliwangwang", "千牛",
    "jd", "京东", "jdmall",
    "pinduoduo", "拼多多", "pdd",
    "xianyu", "闲鱼",
    "vip", "唯品会",
    "dewu", "得物", "poizon",
    "meituan", "美团", "ele.me", "饿了么",
    
    // --- 视频/长视频/影视 ---
    "youku", "优酷",
    "iqiyi", "爱奇艺", "qiyi",
    "tudou", "土豆",
    "tencent video", "腾讯视频", "v.qq",
    "netflix", "hulu", "disney+", "hbo",
    "pptv", "pps", "sohu video", "搜狐视频",
    "mgtv", "芒果 tv",
    "acfun", "a 站",
    
    // --- 音乐/音频/听书 ---
    "netease music", "网易云音乐", "cloudmusic",
    "qq music", "qq 音乐", "qqmusic",
    "kuwo", "酷我", "kugou", "酷狗",
    "ximalaya", "喜马拉雅",
    "qingting", "蜻蜓 fm",
    "lizard", "荔枝 fm",
    
    // --- 小说/漫画/阅读 ---
    "qidian", "起点读书",
    "jinjiang", "晋江文学城",
    "fanqie", "番茄小说",
    "seven cats", "七猫免费小说",
    "tencent animation", "腾讯动漫",
    "bilibili comic", "哔哩哔哩漫画",
    "kindle", "多看阅读",
    
    // --- 浏览器 (防止通过网页访问违规网站，配合窗口标题检测) ---
    "chrome", "google chrome", "chromium",
    "msedge", "microsoft edge", "edge",
    "firefox", "mozilla firefox",
    "iexplore", "ie", "internet explorer",
    "360se", "360 安全浏览器", "360chrome", "360 极速浏览器",
    "qqbrowser", "qq 浏览器",
    "ucbrowser", "uc 浏览器", "uc",
    "sogou explorer", "搜狗浏览器",
    "maxthon", "傲游浏览器",
    "opera", "cent browser", "百分浏览器",
    
    // --- 其他常见摸鱼/工具 ---
    "stock", "同花顺", "eastmoney", "东方财富", "da zhi hui", "大智慧",
    "trade hunter", "通达信",
    "idle fish", "咸鱼",
    "calculator", "计算器",
    "cards", "纸牌", "solitaire", "扫雷", "minesweeper"
};

// UI 类声明
#include "ui_client.h"

// 【确认】定义默认端口常量，区分教师和学生端口以避免冲突
const quint16 DEFAULT_TEACHER_UDP_PORT = 9999; // 教师端监听端口
const quint16 DEFAULT_STUDENT_START_PORT = 8889; // 学生端监听端口

QT_BEGIN_NAMESPACE
namespace Ui { class client; }
QT_END_NAMESPACE

struct UserInfo {
    QString id;
    QString ip;
    quint16 port;
    quint16 tcpPort;
    QString nickName;
    bool isTeacher;
    qint64 lastHeartbeat;
    // 新增：记录是否是手动指定的连接
    bool isManualConnect;
};

// 【修复】确保 StudentStatus 枚举定义在 class client 声明之前，且为全局枚举
enum class StudentStatus {
    Offline,
    Online_Normal,
    Online_Warning, // 本地警告中
    Online_Violated // 已上报老师
};

class client : public QMainWindow
{
    Q_OBJECT

public:
    explicit client(QWidget *parent = nullptr);
    ~client();

    // 工具函数
    bool checkTeacherOnline();
    bool isPortOpen(quint16 port, QAbstractSocket::SocketType socketType);

    // UI 指针
    Ui::client *ui;

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onUdpReadyRead(); // 【修复】补全缺失的 UDP 读取槽函数声明
    void sendHeartbeat();
    void onNewPeerConnection();
    
    // 防火墙检查
    void checkFirewallStatus();

    // 导航与列表交互
    void onFriendListItemDoubleClicked(QListWidgetItem *item);
    void onRefreshClicked();
    void onFileListItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onFileListItemClicked(QTreeWidgetItem *item, int column);
    void onDownloadClicked();
    void onQuitClicked();
    void onManualConnectClicked();

    // 设置相关
    void onSaveNicknameClicked();
    void onChangeAvatarClicked();

    // 网络管理
    void showFriendListPage();
    void showFileSharePage();
    void startNetworkInitialization();
    void showNetworkErrorInUI(const QString &errorDetail);
    void onRetryConnectionClicked();

    // 黑名单检查
    void onCheckBlacklistTimeout();

    // 【新增】班级管理相关槽函数
    void onJoinClassClicked();

    // 【修复】补全缺失的私有函数声明
    void initUi();
    void requestFileList(const QString &targetIp, quint16 targetTcpPort, const QString &path);
    void requestFileDownload(const QString &targetIp, quint16 targetTcpPort, const QString &fileName, const QString &path, const QString &savePath);
    void handlePeerCommand(const QByteArray &data, QTcpSocket *socket);
    void tryDirectConnect(const QString &teacherIp, quint16 teacherTcpPort);
    void startUdpDiscovery();
    void updateUserList(const UserInfo &user);
    void removeUser(const QString &uniqueKey);
    void updateUserListFromMap();
    void showFirewallWarning();
    QString getCurrentForegroundApp();
    void captureAndSendScreenshot(bool isPeriodic, const QString &violatedAppName = "", bool countOnly = false);

private:
    // 网络组件
    QTcpSocket *m_tcpSocket;
    QUdpSocket *m_udpSocket;
    QTimer *m_heartbeatTimer;
    QTcpServer *m_fileServer;

    // 本机信息
    QString m_myIp;
    quint16 m_myUdpPort;
    quint16 m_myTcpPort;
    QString m_myNickName;
    QString m_sharedDirPath;
    QString m_localSavePath;
    QString m_avatarPath;

    // 状态管理
    enum State {
        State_Offline,
        State_Online
    };
    State m_state;

    // 目标教师信息
    QString m_currentTargetIp;
    quint16 m_currentTargetPort;
    QString m_currentTargetName;
    QString m_currentPath;

    // 在线用户列表
    QMap<QString, UserInfo> m_onlineUsers;

    // UI 组件指针
    QWidget *m_leftPanel;
    QListWidget *m_navList;
    QStackedWidget *m_mainStack;
    QWidget *m_friendPage;
    QListWidget *m_friendList;
    QWidget *m_fileSharePage;
    QPushButton *m_refreshBtn;
    QTreeWidget *m_fileTree;
    QPushButton *m_settingBtn;
    QPushButton *m_downloadBtn;
    QPushButton *m_backBtn;
    QLabel *m_avatarLabel;
    QLabel *m_nickNameLabel;
    QLabel *m_statusLabel;
    
    // 设置页面组件
    QLineEdit *m_nicknameEdit;
    QPushButton *m_saveNicknameBtn;
    QPushButton *m_changeAvatarBtn;

    // 手动连接记录
    QString m_manualTeacherIp;
    quint16 m_manualTeacherTcpPort;

    // 【新增】班级信息
    QString m_currentClassName;
    QComboBox *m_classComboBox;      // 设置页班级选择框
    QPushButton *m_joinClassBtn;     // 设置页加入班级按钮

    // 黑名单检查
    QTimer *m_screenshotTimer;     // 定期监控定时器 (5 分钟)
    QTimer *m_appCheckTimer;       // 【新增】高频检查前台应用定时器 (每秒)
    
    bool m_isMonitoringEnabled;    // 是否开启了监控模式
    bool m_isReportingViolated;    // 【新增】防止重复上报同一应用的防抖标志
    qint64 m_lastViolatedTime;     // 【新增】记录上次违规结束的时间戳 (毫秒)，用于计算 10s 间隔

signals:
    // 【修复】确保信号声明使用正确的枚举类型
    void studentStatusChanged(const QString &studentId, StudentStatus status, const QString &appName, const QByteArray &screenshot);
};

#endif
