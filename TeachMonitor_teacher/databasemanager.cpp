#include "databasemanager.h"
#include <QDir> //目录操作类，用于获取当前程序路径
#include <QDebug>

//====================DatabaseManager 类实现========================

/**
 * @brief 构造函数
 * @param parent 父对象指针
 *
 * 将父对象指针传递给 QObject 基类，用于 Qt 内存管理。
 * 实际数据库连接在 init() 中完成，构造函数不进行任何数据库操作。
 */
DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent)
{

}

/**
 * @brief 获取单例实例（线程安全）
 * @return DatabaseManager& 单例引用
 *
 * 使用局部静态变量实现 Meyers 单例模式，C++11 保证线程安全。
 */
DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager inst;    //静态局部变量，第一次调用时构造，程序结束时析构
    return inst;
}

/**
 * @brief 初始化数据库连接和所有表结构
 * @return true 初始化成功，false 失败（如无法打开数据库文件）
 *
 * 主要步骤：
 * 1. 添加 SQLite 驱动，设置数据库文件路径（当前目录/server_data.db）
 * 2. 打开数据库连接
 * 3. 依次创建 students、classes、warning_logs、file_stats 表（若不存在）
 * 4. 插入两条测试学生记录（S001 张三，S002 李四）
 */

bool DatabaseManager::init()
{
     //添加SQLite数据库驱动
    m_db = QSqlDatabase::addDatabase("QSQLITE");
     //设置数据库文件路径为当前程序运行目录下的server_data.db
    QString dbPath = QDir::currentPath() + "/server_data.db";
    m_db.setDatabaseName(dbPath);

    //尝试打开数据库连接
    if(!m_db.open())
    {
        qDebug()<<"数据库打开失败："<<m_db.lastError().text();
        return false;
    }

    QSqlQuery query;    //用于执行sql语句

    // ---------- 创建学生表 students ----------
    // 字段说明：
    //   id         : 学生唯一标识，主键（由客户端生成的固定 UUID）
    //   name       : 学生姓名/昵称
    //   ip         : 学生端 IP 地址
    //   isOnline   : 在线状态，0-离线，1-在线，默认 0
    //   class_name : 所属班级名称，默认为空字符串
    QString createTableStudents = R"(
        CREATE TABLE IF NOT EXISTS students(
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            ip TEXT,
            isOnline INTEGER DEFAULT 0,
            class_name TEXT DEFAULT ''
        )
    )";
    if(!query.exec(createTableStudents))
    {
        qDebug() << "创建学生表失败：" <<query.lastError().text();
        return false;
    }

    // ---------- 创建班级表 classes ----------
    // 字段说明：
    //   id         : 自增主键
    //   class_name : 班级名称，唯一约束
    //   create_time: 创建时间，默认当前时间戳（SQLite DATETIME 类型）
    QString createTableClasses = R"(
        CREATE TABLE IF NOT EXISTS classes(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_name TEXT UNIQUE NOT NULL,
            create_time DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if(!query.exec(createTableClasses))
    {
        qDebug() << "创建班级表失败："<<query.lastError().text();
        // 不阻断初始化流程，继续执行
    }

    // ---------- 创建警告日志表 warning_logs ----------
    // 字段说明：
    //   id         : 自增主键
    //   student_id : 关联的学生 ID
    //   app_name   : 违规应用名称（如“抖音”、“Periodic_Monitor”等）
    //   details    : 详细描述信息
    //   timestamp  : 记录时间戳，默认当前时间
    //   screenshot : 截图二进制数据（BLOB 类型）
    QString createTableLogs = R"(
        CREATE TABLE IF NOT EXISTS warning_logs(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            student_id TEXT,
            app_name TEXT,
            details TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            screenshot BLOB
        )
    )";

    if(!query.exec(createTableLogs))
    {
        qDebug()<<"创建警告日志表失败："<<query.lastError().text();
        return false;
    }

    // -------------初始化文件统计表--------------
    //调用单独的函数创建
    if(!initFileStatsTable())
    {
        qDebug() << "初始化文件统计表失败";
        //不反悔false,允许程序继续与运行（只是统计功能失效）
    }

    return true;
}

/**
 * @brief 注册或更新学生信息
 * @param id 学生唯一标识
 * @param name 学生姓名/昵称
 * @param ip 学生端 IP 地址
 * @return true 操作成功，false 失败
 *
 * 使用 INSERT OR REPLACE 语法：
 * - 如果 id 已存在，则更新 name、ip，并将 isOnline 设为 1
 * - 如果 id 不存在，则插入新记录，isOnline 设为 1
 */
bool DatabaseManager::registerStudent(const QString &id,const QString &name,const QString &ip)
{
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO students(id,name,ip,isOnline) VALUES (?,?,?,1)");
    query.addBindValue(id);
    query.addBindValue(name);
    query.addBindValue(ip);
    return query.exec();//执行并返回是否成功
}

/**
 * @brief 获取单个学生的完整信息
 * @param id 学生 ID
 * @return StudentInfo 结构体；若未找到则 id 为空字符串，isOnline 为 false
 *
 * 查询 students 表中指定 id 的记录，填充到 StudentInfo 中返回。
 */
StudentInfo DatabaseManager::getStudentInfo(const QString& id)
{
    StudentInfo info;
    info.isOnline = false;
    QSqlQuery query;
    //查询时包含class_name字段
    query.prepare("SELECT id,name,ip,isOnline,class_name FROM students WHERE id = ?");
    query.addBindValue(id);
    if(query.exec() && query.next())
    {
        info.id = query.value(0).toString();
        info.name = query.value(1).toString();
        info.ip = query.value(2).toString();
        info.isOnline = query.value(3).toBool();
        info.className = query.value(4).toString();
    }
    return info;
}

/**
 * @brief 获取所有学生的信息
 * @return QMap<QString, StudentInfo> 以学生 ID 为键，信息结构为值
 *
 * 遍历 students 表的所有记录，构建映射表返回。
 */
QMap<QString,StudentInfo> DatabaseManager::getAllStudents()
{
    QMap<QString,StudentInfo> map;
    //查询所有学生，包含class_name 字段
    QSqlQuery query("SELECT id,name,ip,isOnline,class_name FROM students");
    while(query.next())
    {
        StudentInfo info;
        info.id = query.value(0).toString();
        info.name = query.value(1).toString();
        info.ip = query.value(2).toString();
        info.isOnline = query.value(3).toBool();
        info.className = query.value(4).toString();
        map[info.id] = info;
    }
    return map;
}

/**
 * @brief 更新学生在线状态
 * @param id 学生 ID
 * @param online true-在线，false-离线
 *
 * 将 isOnline 字段设为 1 或 0。
 */
void DatabaseManager::updateStudentStatus(const QString &id,bool online)
{
    QSqlQuery query;
    query.prepare("UPDATE students SET isOnline = ? WHERE id = ?");
    query.addBindValue(online ? 1 : 0);
    query.addBindValue(id);
    query.exec();           //忽略执行结果，即使失败也不影响主流程
}

/**
 * @brief 更新学生所属班级
 * @param id 学生 ID
 * @param className 班级名称（可为空字符串，表示未分班）
 * @return true 更新成功，false 失败
 *
 * 直接将 class_name 字段设置为给定值，不检查班级是否在 classes 表中存在。
 * 建议先通过 createClass 创建班级，再调用此函数分配。
 */
bool DatabaseManager::updateStudentClass(const QString &id,const QString &className)
{
    QSqlQuery query;
    query.prepare("UPDATE students SET class_name = ? WHERE id = ?");
    query.addBindValue(className);
    query.addBindValue(id);
    return query.exec();
}

/**
 * @brief 创建新班级
 * @param className 班级名称（必须唯一）
 * @return true 创建成功，false 失败（可能班级已存在或数据库错误）
 *
 * 使用 INSERT OR IGNORE 语法：如果班级名称已存在，则忽略插入操作。
 * 插入成功则自动生成自增 ID 和当前时间戳。
 */
bool DatabaseManager::createClass(const QString &className)
{
    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO classes (class_name) VALUES (?)");
    query.addBindValue(className);
    return query.exec();
}

/**
 * @brief 删除班级
 * @param className 要删除的班级名称
 * @return true 删除成功，false 失败
 *
 * 执行两步操作：
 * 1. 从 classes 表中删除该班级记录
 * 2. 将 students 表中 class_name 为该班级的所有学生的 class_name 字段置为空字符串
 * 第二步即使失败，也返回 true（班级本身已删除），只记录日志。
 */
bool DatabaseManager::deleteClass(const QString &className)
{
    QSqlQuery query;

    //1.从班级表中删除记录
    query.prepare("DELETE FROM classes WHERE class_name = ?");
    query.addBindValue(className);
    if(!query.exec())
    {
        qDebug()<< "[DB] 删除班级失败:"<<query.lastError().text();
        return false;
    }

    //2.将原属于该班级的学生的班级字段置空
    query.prepare("UPDATE students SET class_name = '' WHERE class_name = ?");
    query.addBindValue(className);
    if(!query.exec())
    {
        qDebug()<<"[DB] 清空学生班级失败:"<<query.lastError().text();
        //这一步即使失败也不影响什么
    }
    return true;
}

/**
 * @brief  获取所有班级信息列表
 * @return QList<ClassInfo> 班级信息列表，按创建时间倒序排列（最新在前）
 *
 * 查询 classes 表，使用 strftime('%s', create_time) 将 DATETIME 转换为 Unix 时间戳（秒），
 * 便于在 UI 中排序或显示。
 */
QList<ClassInfo> DatabaseManager::getAllClasses()
{
    QList<ClassInfo> list;
    //按创建时间倒序排列，最新的班级排在前面
    QSqlQuery query("SELECT id,class_name,strftime('%s',create_time) from classes ORDER BY create_time DESC");
    while(query.next())
    {
        ClassInfo info;
        info.id = query.value(0).toInt();
        info.className = query.value(1).toString();
        info.createTime = query.value(2).toLongLong(); //Unix时间戳(秒)
        list.append(info);
    }
    return list;
}

/**
 * @brief 获取指定班级的成员列表
 * @param className 班级名称
 * @param onlineOnly 是否仅返回在线学生（true：仅在线；false：全部）
 * @return QList<StudentInfo> 学生信息列表
 *
 * 根据 className 筛选 students 表，如果 onlineOnly 为 true 则额外添加 isOnline = 1 条件。
 */
QList<StudentInfo> DatabaseManager::getClassMembers(const QString &className,bool onlineOnly)
{
    QList<StudentInfo> list;
    QSqlQuery query;
    QString sql = "SELECT id ,name,ip,isOnline,class_name FROM students WHERE class_name = ?";
    if(onlineOnly)
    {
        sql += " AND isOnline = 1";
    }
    query.prepare(sql);
    query.addBindValue(className);

    if(query.exec())
    {
        while(query.next())
        {
            StudentInfo info;
            info.id = query.value(0).toString();
            info.name = query.value(1).toString();
            info.ip = query.value(2).toString();
            info.isOnline = query.value(3).toBool();
            info.className = query.value(4).toString();
            list.append(info);
        }
    }
    return list;
}

/**
 * @brief 记录违规警告日志（不带截图）
 * @param studentId 学生 ID
 * @param appName 违规应用名称
 * @param details 详细信息
 * @return true 记录成功，false 失败
 *
 * 插入 warning_logs 表，screenshot 字段为空。
 * 适用于“仅计数”或“恢复”等不需要保存图片的事件。
 */

bool DatabaseManager::logWarning(const QString& studentId,const QString &appName,const QString &details)
{
    QSqlQuery query;
    query.prepare("INSERT INTO warning_logs (student_id,app_name,details) VALUES (?,?,?)");
    query.addBindValue(studentId);
    query.addBindValue(appName);
    query.addBindValue(details);
    return query.exec();
}

/**
 * @brief 保存截图记录（旧接口）
 * @param studentId 学生 ID
 * @param imageData 截图二进制数据（JPEG 格式）
 * @return true 保存成功，false 失败
 *
 * 内部调用 insertMonitorRecord，应用名固定为 "VIOLATION"，详情为 "自动截屏记录"。
 * 保留此函数以兼容旧代码，实际功能已由 insertMonitorRecord 覆盖。
 */
bool DatabaseManager::saveScreenshotRecord(const QString &studentId,const QByteArray& imageData)
{
    QSqlQuery query;
    query.prepare("INSERT INTO warning_logs (student_id,app_name,details,screenshot) VALUES (?,?,?,?)");
    query.addBindValue(studentId);
    query.addBindValue("VIOLATION");
    query.addBindValue("自动截屏记录");
    query.addBindValue(imageData);

    if(!query.exec())
    {
        qDebug()<<"保存截图失败："<<query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief 插入完整的监控记录（包含应用名、详情、截图）
 * @param studentId 学生 ID
 * @param appName 应用名称（违规应用或 "Periodic_Monitor" 等）
 * @param details 详细信息
 * @param imageData 截图二进制数据（可为空）
 * @return true 插入成功，false 失败
 *
 * 统一的记录插入函数，适用于所有监控事件：
 * - 违规报告（VIOLATION_REPORT）
 * - 定期巡查（Periodic_Monitor）
 * - 实时响应（Live_Request_Response）
 * - 状态恢复（STATUS_RECOVERY）
 *
 * 当 imageData 为空时，screenshot 字段存储空 BLOB。
 */
bool DatabaseManager::insertMonitorRecord(const QString &studentId,const QString &appName,const QString &details,const QByteArray& imageData)
{
    QSqlQuery query;
    query.prepare("INSERT INTO warning_logs (student_id,app_name,details,screenshot) VALUES (?,?,?,?)");
    query.addBindValue(studentId);
    query.addBindValue(appName);
    query.addBindValue(details);
    query.addBindValue(imageData);

    if(!query.exec())
    {
        qDebug()<<"插入监控记录失败："<<query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief 获取学生的违规次数
 * @param studentId 学生 ID
 * @return int 违规记录总数（不包括 Periodic_Monitor 类型的记录）
 *
 * 统计 warning_logs 表中 student_id 匹配且 app_name 不等于 'Periodic_Monitor' 的记录数。
 * 用于教师端监控表格中的“违规次数”列显示。
 */
int DatabaseManager::getViolationCount(const QString &studentId)
{
    QSqlQuery query;
    //排除定期巡查记录，只统计真正违规的时间
    query.prepare("SELECT COUNT(*) FROM warning_logs WHERE student_Id = ? AND app_name != 'Periodic_Monitor'");
    query.addBindValue(studentId);
    if(query.exec() && query.next())//query.next()将游标移动到当前行
    {
        return query.value(0).toInt();
    }
    return 0;
}

/**
 * @brief 获取学生所有截图历史记录
 * @param studentId 学生 ID
 * @return QList<ViolationRecord> 记录列表，按时间倒序排列（最新的在前）
 *
 * 查询 warning_logs 表中该学生的所有记录，包括违规报告、定期巡查等，
 * 按 timestamp 降序排列，用于教师端查看历史屏幕记录。
 */
QList<DatabaseManager::ViolationRecord> DatabaseManager::getScreenshotHistory(const QString &studentId)
{
    QList<ViolationRecord> records;
    QSqlQuery query;
    //按时间倒序,最新的记录排在最前面
    query.prepare("SELECT id,app_name,timestamp,screenshot FROM warning_logs WHERE student_Id = ? ORDER BY timestamp DESC");
    query.addBindValue(studentId);

    if(query.exec())
    {
        while(query.next())
        {
            ViolationRecord rec;
            rec.id = query.value(0).toLongLong();
            rec.appName = query.value(1).toString();
            rec.timestamp = query.value(2).toString();
            rec.screenshot = query.value(3).toByteArray();
            records.append(rec);
        }
    }
    else
    {
        qDebug()<<"获取截图历史失败："<<query.lastError().text();
    }
    return records;
}

/**
 * @brief 初始化文件统计表 file_stats
 * @return true 成功，false 失败
 *
 * 创建表结构：
 *   file_path        : 文件的相对路径（主键），如 "/docs/readme.txt"
 *   download_count   : 下载次数，默认为 0
 *   last_source_ip   : 最后一次下载该文件的学生 IP，默认为 "-"
 *   last_access_time : 最后访问时间，默认为当前时间戳
 */
bool DatabaseManager::initFileStatsTable()
{
    QSqlQuery query;
    QString createTableFileStats = R"(
        CREATE TABLE IF NOT EXISTS file_stats(
            file_path TEXT PRIMARY KEY,
            download_count INTEGER DEFAULT 0,
            last_source_ip TEXT DEFAULT '-',
            last_access_time DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if(!query.exec(createTableFileStats))
    {
        qDebug()<<"创建文件统计表失败："<<query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief 获取文件的下载统计信息
 * @param filePath 文件的相对路径
 * @return FileStatRecord 统计信息（包含下载次数、最后来源 IP）
 *
 * 如果文件从未被下载过，返回的 downloadCount 为 0，lastSourceIp 为 "-"。
 * 如果查询失败（如数据库错误），也返回默认值。
 */
FileStatRecord DatabaseManager::getFileStat(const QString &filepath)
{
    FileStatRecord record;
    record.filePath = filepath;

    QSqlQuery query;
    query.prepare("SELECT download_count,last_source_ip FROM file_stats WHERE file_path = ?");
    query.addBindValue(filepath);

    if(query.exec() && query.next())
    {
        record.downloadCount = query.value(0).toInt();
        record.lastSourceIp = query.value(1).toString();
    }
    else
    {
        //记录不存在时返回默认值
        record.downloadCount = 0;
        record.lastSourceIp = "-";
    }
    return record;
}

/**
 * @brief 更新文件统计信息（增加一次下载次数）
 * @param filePath 文件的相对路径
 * @param sourceIp 下载学生的 IP 地址
 * @return true 更新成功，false 失败
 *
 * 执行逻辑：
 * 1. 先尝试 UPDATE 现有记录，将 download_count 加 1，更新 last_source_ip 和 last_access_time
 * 2. 如果 UPDATE 影响行数为 0（记录不存在），则执行 INSERT 新记录，download_count 设为 1
 *
 * 这种方式避免了先查询再插入的两次数据库操作，提高效率。
 */
bool DatabaseManager::updateFileStat(const QString &filePath,const QString &sourceIp)
{
    QSqlQuery query;

    //先尝试更新现有记录
    query.prepare(R"(
        UPDATE file_stats
        SET download_count = download_count + 1,
        last_source_ip = ?,
        last_access_time = CURRENT_TIMESTAMP
        WHERE filepath = ?
    )");
    query.addBindValue(sourceIp);
    query.addBindValue(filePath);

    if(query.exec() && query.numRowsAffected() > 0)
    {
        return true;//更新成功
    }

    //如果更新影响行数为0，说明记录不存在，则插入新记录
    query.prepare(R"(
        INSERT OR REPLACE INTO file_stats (file_path,download_count,last_source_ip) VALUES (?,1,?)
    )");//每次调用prepare，绑定的值会被清除
    query.addBindValue(filePath);
    query.addBindValue(sourceIp);

    return query.exec();
}
