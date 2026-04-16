#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>

//=============数据结构定义=================
/**
 * @brief 学生信息结构体
 * 用于存储单个学生的基本信息，从数据库students表中获取
*/
struct StudentInfo
{
    QString id;         //学生唯一标识
    QString name;       //学生姓名/昵称
    QString ip;         //学生端ip
    bool isOnline;      //是否在线
    QString className;  //所属班级
};

/**
 * @brief 班级信息结构体
 * 用于存储系统中创建的班级信息，从classes表中获取
 */
struct ClassInfo
{
    int id;             //班级自增主键id
    QString className;  //班级名称（唯一约束）
    qint64 createTime;  //创建时间（Unix时间戳，秒）
};

/**
 * @brief 文件统计信息结构体（用于数据库映射）
 * 记录某个文件被下载的次数及最近一次下载来源IP，从file_stats表中获取
 */
struct FileStatRecord
{
    int downloadCount = 0;      //下载次数默认为0
    QString lastSourceIp = "-"; //最近一次下载该文件的学生IP，默认为"-"
    QString filePath;           //文件的相对路径（作为主键，例如"/docs/readme.txt"）
};

//==================DatabaseManager类=======================
/**
 * @brief 数据库管理器（单例模式）
 *
 * 负责整个教师端所有数据库操作，包括
 * - 学生信息管理（注册、查询、状态更新、班级分配）
 * - 班级管理（创建、删除、查询班级列表）
 * - 监控日志管理（记录违规警告，插入截图记录）
 * - 文件统计管理（记录文件下载次数，来源IP）
 *
 * 使用SQLite数据库，表结构：
 * - students:学生表
 * - classes:班级表
 * - warning_logs:警告日志表（含截图BLOB）
 * - file_stats:文件统计表
 *
 * 单例访问方式：DatabaseManager::instance()
 */
class DatabaseManager: public QObject
{
    Q_OBJECT    //QT元对象宏
public:
    /**
     * @brief 获取单例实例
     * @return DatabaseManager& 单例引用
     */
    static DatabaseManager& instance();

    /**
     * @brief 初始化数据库连接和表结构
     * @return  true初始化成功，false失败
     *
     * 会自动创建以下表（如果不存在）:
     * - students
     * - classes
     * - warning_logs
     * - file_stats
     * 并插入测试数据（如学生S001，S002）
     */
    bool init();

    //=============学生信息管理==============
    /**
     * @brief 注册或更新学生信息
     * @param id 学生唯一标识（固定 ID）
     * @param name 学生姓名/昵称
     * @param ip 学生端 IP 地址
     * @return true 操作成功，false 失败
     *
     * 如果学生 ID 已存在，则更新其 name 和 ip，并将 isOnline 设为 1；
     * 否则插入新记录。
     */
    bool registerStudent(const QString &id,const QString &name,const QString &ip);

    /**
     * @brief 获取单个学生的完整信息
     * @param id 学生 ID
     * @return StudentInfo 学生信息结构体；若未找到则返回的 id 为空字符串
     */
    StudentInfo getStudentInfo(const QString &id);

    /**
     * @brief 获取所有学生的信息
     * @return QMap<QString, StudentInfo> 以学生 ID 为键，信息结构为值
     */
    QMap<QString,StudentInfo> getAllStudents();

    /**
     * @brief 更新学生在线状态
     * @param id 学生 ID
     * @param online true-在线，false-离线
     */
    void updateStudentStatus(const QString &id,bool online);

    /**
     * @brief 更新学生所属班级
     * @param id 学生 ID
     * @param className 班级名称（例如“三年二班”）
     * @return true 更新成功，false 失败
     *
     * 如果班级不存在，仍会更新（班级名称可以是任意字符串），
     * 但建议通过班级管理接口创建班级后再分配。
     */
    bool updateStudentClass(const QString &id,const QString &className);

    //===========班级管理============
    /**
     * @brief 创建一个新班级
     * @param className 班级名称（必须唯一）
     * @return true 创建成功，false 失败（可能班级已存在或数据库错误）
     *
     * 注意：此操作不会自动分配学生，需要后续调用 updateStudentClass 将学生加入班级。
     */
    bool createClass(const QString &className);

    /**
     * @brief 删除一个班级
     * @param className 要删除的班级名称
     * @return true 删除成功，false 失败
     *
     * 删除班级后，会将原属于该班级的所有学生的 class_name 字段置为空字符串。
     * 学生端需要重新选择班级加入。
     */
    bool deleteClass(const QString &className);

    /**
     * @brief 获取系统中所有班级的信息列表
     * @return QList<ClassInfo> 班级信息列表，按创建时间倒序排列（最新在前）
     */
    QList<ClassInfo> getAllClasses();

    /**
     * @brief 获取指定班级的成员列表
     * @param className 班级名称
     * @param onlineOnly 是否仅返回在线学生（true：仅在线；false：全部）
     * @return QList<StudentInfo> 学生信息列表
     */
    QList<StudentInfo> getClassMembers(const QString &className,bool onlineOnly = false);

    //=============监控日志==============
    /**
     * @brief 记录违规警告日志（不包含截图）
     * @param studentId 学生 ID
     * @param appName 违规应用名称
     * @param details 详细信息（如“自动检测违规应用：抖音”）
     * @return true 记录成功，false 失败
     *
     * 写入 warning_logs 表，screenshot 字段为空。
     */
    bool logWarning(const QString &studentId,const QString &appName,const QString &details);

    /**
     * @brief 保存截图记录（旧接口，内部调用 insertMonitorRecord）
     * @param studentId 学生 ID
     * @param imageData 截图二进制数据（JPEG 格式）
     * @return true 保存成功，false 失败
     */
    bool saveScreenshotRecord(const QString& studentId,const QByteArray &imageData);

    /**
     * @brief 插入完整的监控记录（包含应用名、详情、截图）
     * @param studentId 学生 ID
     * @param appName 应用名称（违规应用或“Periodic_Monitor”等）
     * @param details 详细信息
     * @param imageData 截图二进制数据（可为空）
     * @return true 插入成功，false 失败
     *
     * 统一用于记录各类监控事件：违规报告、定期巡查、状态恢复、实时响应等。
     */
    bool insertMonitorRecord(const QString &studentId,const QString &appName,const QString &details,const QByteArray& imagedata);

    //================统计与历史查询==================
    /**
     * @brief 获取学生的违规次数
     * @param studentId 学生 ID
     * @return int 违规记录总数（不包括 Periodic_Monitor 类型的记录）
     *
     * 用于教师端监控表格中显示“违规次数”列。
     */
    int getViolationCount(const QString &studentId);

    /**
     * @brief 违规记录结构体（用于历史查询）
     */
    struct ViolationRecord
    {
        qint64 id;              //记录自增id
        QString appName;        //应用名称
        QString timestamp;      //时间戳字符串(SQLite格式)
        QByteArray screenshot;  //截图二进制数据
    };

    /**
     * @brief 获取学生所有截图历史记录（包括定期监控和违规截图）
     * @param studentId 学生 ID
     * @return QList<ViolationRecord> 记录列表，按时间倒序排列（最新的在前）
     *
     * 用于教师端查看学生的历史屏幕记录（点击历史记录按钮时调用）。
     */
    QList<ViolationRecord> getScreenshotHistory(const QString &studentId);

    //==============文件统计管理================

    /**
     * @brief 初始化文件统计表（在 init() 中调用）
     * @return true 成功，false 失败
     *
     * 创建 file_stats 表，用于记录每个文件被下载的次数和来源 IP。
     */
    bool initFileStatsTable();

    /**
     * @brief 获取文件的下载统计信息
     * @param filePath 文件的相对路径（例如 "/docs/readme.txt"）
     * @return FileStatRecord 统计信息（包含下载次数、最后来源 IP）
     *
     * 如果文件中从未被下载过，则返回的 downloadCount 为 0，lastSourceIp 为 "-"。
     */
    FileStatRecord getFileStat(const QString &filePath);

    /**
     * @brief 更新文件统计信息（增加一次下载次数）
     * @param filePath 文件的相对路径
     * @param sourceIp 下载学生的 IP 地址
     * @return true 更新成功，false 失败
     *
     * 如果记录不存在，则创建新记录，downloadCount 设为 1；
     * 如果记录存在，则 downloadCount 加 1，更新 lastSourceIp 和 last_access_time。
     */
    bool updateFileStat(const QString &filePath,const QString &sourceIp);

private:
    /**
     * @brief 私有构造函数（单例模式）
     * @param parent 父对象指针
    */
    explicit DatabaseManager(QObject *parent = nullptr);

    QSqlDatabase m_db;  //SQLite 数据库连接对象
};

#endif // DATABASEMANAGER_H
