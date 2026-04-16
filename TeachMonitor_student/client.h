#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QListWidget>//列表控件
#include <QLineEdit>
#include <QPushButton>
#include <QLabel> //标签
#include <QTreeWidgetItem>
#include <QTcpServer>
#include <QStackedWidget>//堆叠窗口
#include <QComboBox>//下拉框
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QNetworkInterface>//本地网络接口
#include <QFile>
#include <QRandomGenerator>
#include <QDir>
#include <QMessageBox>
#include <QHeaderView>//树形视图的标题栏
#include <QFileDialog>
#include <QScrollArea>
#include <QVariant>
#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QCamera>//摄像头相关
#include <QCameraInfo>
#include <QVideoProbe>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QPluginLoader>
#include <QThread>
//================黑名单配置=================
/**
 * @brief 黑名单应用关键字列表（不区分大小写）
 *
 * 用于检测学生端的前台应用是否属于违规应用（如短视频、游戏、社交等）。
 * 策略：覆盖进程名、窗口标题的常见关键词（全称、简称、中英文、带 .exe 后缀等）。
 * 该列表在 Windows 平台下用于 getCurrentForegroundApp() 函数中匹配。
 */
static const QStringList BLACKLIST_APPS = {
    // --- 短视频/直播 ---
    "douyin", "dy", "抖音", "douyin.exe", "dy.exe", "tiktok",
    "bilibili", "b 站", "哔哩哔哩", "bilibili.exe", "blive", "livehime",
    "b 站", "bilibili live",
    "kuaishou", "快手", "kwai", "gifshow",
    "huya", "虎牙", "yy", "duowan",
    "douyu", "斗鱼", "dylive",
    "inke", "花椒", "huajiao",
    "longzhu", "龙珠", "panda", "熊猫",

    // --- 游戏类 ---
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

    // --- 社交/娱乐/社区  ---
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
    // 补充
    "b 站", "bilibili", "哔哩哔哩",
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

//==================端口常量定义=================
///教师端UDP监听端口（教师端心跳广播的目标端口）(学生端发送心跳的端口)
const quint16 DEFAULT_TEACHER_UDP_PORT = 9999;
///学生端UDP监听端口（学生端接收广播的端口）（学生端接收老师端的心跳的端口）
const quint16 DEFAULT_STUDENT_START_PORT = 8889;

QT_BEGIN_NAMESPACE
namespace Ui { class client; }//前向声明UI
QT_END_NAMESPACE

// ====================用户信息结构体==================
/**
 * @brief 在线用户信息结构体
 *
 * 用于存储从 UDP 心跳或教师端广播中获取的用户（教师或学生）信息。
 */
struct UserInfo
{
    QString id; //用户唯一标识（学生固定ID或IP：端口）
    QString ip; //IP地址
    quint16 port;//UDP端口
    quint16 tcpPort;//TCP端口（用于文件传输和监控数据）
    QString nickName;//昵称
    bool isTeacher;//是否为教师端
    qint64 lastHeartbeat;//最后一次心跳时间戳（毫秒）
    bool isManualConnect;//是否为手动连接（用于记录特殊标记）
};

//=================学生端主窗口类==================
/**
 * @brief 学生端主窗口类
 *
 * 学生客户端，主要功能：
 * - 通过 UDP 广播发现教师端，并维持心跳连接
 * - 显示在线教师，连接教师端获取共享文件列表
 * - 下载教师端的共享文件
 * - 实时监控前台应用（通过黑名单检测违规应用）
 * - 定期或违规时截取屏幕并上报教师端
 * - 个人设置：修改昵称、头像、选择班级
 * - 支持手动连接教师端（指定 IP 和端口）
 *
 * 网络通信：
 * - UDP：监听端口 8889，接收教师端广播的心跳和用户列表
 * - TCP：用于文件传输、发送监控数据（违规报告、截图）等
 */
class client : public QMainWindow
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     *
     * 初始化 UI、网络组件、本地配置、启动 UDP 发现、启动监控定时器等。
     */
    explicit client(QWidget *parent = nullptr);
    ~client();

    // ========== 工具函数 ==========
    /**
     * @brief 检查是否有教师端在线
     * @return true 存在在线教师，false 无教师在线
     */
    bool checkTeacherOnline();

private slots:
    //============网络连接相关槽函数===========
    void onUdpReadyRead();//UDP数据可读时的槽函数（接收教师端广播）
    void sendHeartbeat();//定时发送心跳包（UDP广播或单播）
    void onNewPeerConnection();//新的TCP连接（作为文件服务器）到达时的槽函数

    //==========导航与列表交互==========
    void onFriendListItemDoubleClicked(QListWidgetItem* item);//双击教师列表项，进入目录
    void onRefreshClicked();//刷新文件列表
    void onFileListItemDoubleClicked(QTreeWidgetItem* item);//双击文件列表（进入目录）
    void onFileListItemClicked(QTreeWidgetItem* item,int column);//单击文件列表项（启用/禁用下载）
    void onDownloadClicked();//下载选中文件
    void onQuitClicked();//退出程序
    void onManualConnectClicked();//手动连接教师端（输入IP和端口）

    //==========设置相关===========
    void onSaveNicknameClicked();//保存昵称
    void onChangeAvatarClicked();//更换头像

    //============网络管理===========
    void startNetworkInitialization();//启动网络初始化（绑定UDP、启动定时器）
    void showNetworkErrorInUI(const QString &errorDetail);//在UI中显示网络错误

    //==========黑名单检查===========
    void onCheckBlacklistTimeout();//定时检查前台应用是否在黑名单中

    //===========班级管理=========
    void onJoinClassClicked();//加入班级按钮点击响应

    //==========摄像头相关=========
    void onCameraFrameProbed(const QVideoFrame &frame);

private:
    void initUi();
    void requestFileList(const QString &targetIp,quint16 targetTcpPort,const QString& path);//请求文件列表
    void requestFileDownload(const QString& targetIp,quint16 targetTcpPort,
                             const QString& fileName,const QString &path,const QString&  savePath);//请求下载文件
    void handlePeerCommand(const QByteArray& data,QTcpSocket *socket);//处理来自教师端的命令
    void tryDirectConnect(const QString& teacherIp,quint16 teacherTcpPort);//处理来自教师端的命令
    void startUdpDiscovery();//启动UDP发现（发送心跳）
    void updateUserList(const UserInfo &user);//更新在线用户列表（UI）
    void removeUser(const  QString& uniqueKey);//移除离线用户
    QString getCurrentForegroundApp();//获取当前前台应用名称（Windows）
    void captureAndSendScreenshot(bool isPeriodic,const QString& violatedAppName = "");//截屏并发送给教师端
    //摄像头相关
    void startCameraStream(const QString &teacherIp,quint16 teacherPort);
    void stopCameraStream();
    void processCameraFrame(const QVideoFrame& frame);//帧处理槽函数
    //============网络组件===========
    QUdpSocket *m_udpSocket;//UDP套接字（用于发现教师端和心跳）
    QTimer *m_heartbeatTimer;//心跳定时器
    QTcpServer *m_fileServer;//文件服务器（用于接收教师端的文件传输请求）

    //==========本机信息=========
    QString m_myIp;//本机Ip地址
    quint16 m_myUdpPort;//本机UDP监听端口（固定8889）
    quint16 m_myTcpPort;//本机TCP端口（随机生成，用于文件传输和监控数据）
    QString m_myNickName;//本机昵称
    QString m_localSavePath;//本地下载保存路径
    QString m_avatarPath;//头像图片路径
    QString m_studentId;//固定学生 ID（由UUID生成，保存在配置文件中）

    //===========状态管理==========
    enum State{
        State_Offline,//离线状态
        State_Online//在线状态
    };
    State m_state;//当前连接状态

    // ===========目标教师信息===========
    QString m_currentTargetIp;//当前连接的教师IP
    quint16 m_currentTargetPort;//当前连接的教师TCP端口
    QString m_currentTargetName;//当前连接的教师昵称
    QString m_currentPath;//当前浏览的文件路径（相对于共享根目录）

    //===========在线用户列表==========
    QMap<QString,UserInfo> m_onlineUsers;//在线用户（教师），键为用户ID

    //============UI组件指针===========
    QWidget* m_leftPanel;//左侧面板
    QListWidget* m_navList;//导航列表
    QStackedWidget *m_mainStack;//主内容堆叠窗口
    QWidget* m_friendPage;//好友列表页面
    QListWidget* m_friendList;//好友列表控件
    QWidget* m_fileSharePage;//文件共享页面
    QPushButton *m_refreshBtn;//刷新按钮
    QTreeWidget* m_fileTree;//文件树（显示教师端的共享文件夹）
    QPushButton* m_settingBtn;//设置按钮
    QPushButton* m_downloadBtn;//下载按钮
    QPushButton* m_backBtn;//返回按钮
    QLabel* m_avatarLabel;//头像标签
    QLabel* m_nickNameLabel;//昵称标签
    QLabel* m_statusLabel;//状态标签

    //==========设置页面组件=========
    QLineEdit *m_nicknameEdit;//昵称输入框
    QPushButton* m_saveNicknameBtn;//保存昵称按钮
    QPushButton* m_changeAvatarBtn;//更换头像按钮

    //==========手动连接记录=========
    QString m_manualTeacherIp;//手动指定的教师IP
    quint16 m_manualTeacherTcpPort;//手动指定的教师TCP端口
    QString m_teacherIp;//教师端IP（通过UDP发现或手动连接获得）

    //=======班级信息======
    QString m_currentClassName;//当前选择的班级名称
    QComboBox *m_classComboBox;//设置界面的班级选择下拉框
    QPushButton* m_joinClassBtn;//加入班级按钮

    //===========黑名单检查相关=========
    QTimer* m_screenshotTimer;//定期监控定时器，每5分钟发送一次截图
    QTimer* m_appCheckTimer;//检查前台应用定时器，每秒一次

    bool m_isMonitoringEnabled;//监控模式是否开启
    bool m_isReportingViolated;//是否正在上报违规
    qint64 m_lastViolatedTime;//上次违规结束的时间戳

    //配置加载与保存函数
    void loadUserSettings();//从QSettings加载用户配置（学生ID，昵称，班级，头像，保存路径）
    void saveUserSettings();//保存用户配置到QSettings

    //摄像头相关
    QCamera *m_camera;//摄像头对象
    QVideoProbe *m_videoProbe;//探测视频帧（用来抓取帧）
    QTcpSocket *m_cameraStreamSocket;//与教师端的视频流连接
    bool m_isCameraActive;//摄像头是否已启动
};

#endif // CLIENT_H
