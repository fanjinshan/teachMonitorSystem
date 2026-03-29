#include "databasemanager.h"
#include <QDir>
#include <QDebug>

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {}

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::init() {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = QDir::currentPath() + "/server_data.db";
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "Database open failed:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query;
    // 创建学生表
    QString createTableStudents = R"(
        CREATE TABLE IF NOT EXISTS students (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            ip TEXT,
            isOnline INTEGER DEFAULT 0,
            class_name TEXT DEFAULT ''
        )
    )";
    if (!query.exec(createTableStudents)) {
        qDebug() << "Create table students failed:" << query.lastError().text();
        return false;
    }

    // 【新增】创建班级表
    QString createTableClasses = R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            class_name TEXT UNIQUE NOT NULL,
            create_time DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (!query.exec(createTableClasses)) {
        qDebug() << "Create table classes failed:" << query.lastError().text();
        // 不阻断，可能是旧版本数据库
    }

    // 创建警告日志表
    QString createTableLogs = R"(
        CREATE TABLE IF NOT EXISTS warning_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            student_id TEXT,
            app_name TEXT,
            details TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            screenshot BLOB
        )
    )";
    if (!query.exec(createTableLogs)) {
        qDebug() << "Create table warning_logs failed:" << query.lastError().text();
        return false;
    }

    // 初始化文件统计表
    if (!initFileStatsTable()) {
        qDebug() << "Init file stats table failed";
    }

    // 初始化一些测试数据
    query.prepare("INSERT OR IGNORE INTO students (id, name, ip) VALUES (?, ?, ?)");
    query.addBindValue("S001"); query.addBindValue("张三"); query.addBindValue("192.168.1.101");
    query.exec();
    query.addBindValue("S002"); query.addBindValue("李四"); query.addBindValue("192.168.1.102");
    query.exec();

    return true;
}

bool DatabaseManager::registerStudent(const QString &id, const QString &name, const QString &ip) {
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO students (id, name, ip, isOnline) VALUES (?, ?, ?, 1)");
    query.addBindValue(id);
    query.addBindValue(name);
    query.addBindValue(ip);
    return query.exec();
}

StudentInfo DatabaseManager::getStudentInfo(const QString &id) {
    StudentInfo info;
    info.isOnline = false;
    QSqlQuery query;
    // 【修改】查询时包含 class_name
    query.prepare("SELECT id, name, ip, isOnline, class_name FROM students WHERE id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        info.id = query.value(0).toString();
        info.name = query.value(1).toString();
        info.ip = query.value(2).toString();
        info.isOnline = query.value(3).toBool();
        info.className = query.value(4).toString();
    }
    return info;
}

QMap<QString, StudentInfo> DatabaseManager::getAllStudents() {
    QMap<QString, StudentInfo> map;
    // 【修改】查询时包含 class_name
    QSqlQuery query("SELECT id, name, ip, isOnline, class_name FROM students");
    while (query.next()) {
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

void DatabaseManager::updateStudentStatus(const QString &id, bool online) {
    QSqlQuery query;
    query.prepare("UPDATE students SET isOnline = ? WHERE id = ?");
    query.addBindValue(online ? 1 : 0);
    query.addBindValue(id);
    query.exec();
}

// 【新增】更新学生班级
bool DatabaseManager::updateStudentClass(const QString &id, const QString &className) {
    QSqlQuery query;
    query.prepare("UPDATE students SET class_name = ? WHERE id = ?");
    query.addBindValue(className);
    query.addBindValue(id);
    return query.exec();
}

// 【新增】创建班级
bool DatabaseManager::createClass(const QString &className) {
    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO classes (class_name) VALUES (?)");
    query.addBindValue(className);
    return query.exec();
}

// 【新增】获取所有班级
QList<ClassInfo> DatabaseManager::getAllClasses() {
    QList<ClassInfo> list;
    QSqlQuery query("SELECT id, class_name, strftime('%s', create_time) FROM classes ORDER BY create_time DESC");
    while (query.next()) {
        ClassInfo info;
        info.id = query.value(0).toInt();
        info.className = query.value(1).toString();
        info.createTime = query.value(2).toLongLong();
        list.append(info);
    }
    return list;
}

// 【新增】获取班级成员
QList<StudentInfo> DatabaseManager::getClassMembers(const QString &className, bool onlineOnly) {
    QList<StudentInfo> list;
    QSqlQuery query;
    QString sql = "SELECT id, name, ip, isOnline, class_name FROM students WHERE class_name = ?";
    if (onlineOnly) {
        sql += " AND isOnline = 1";
    }
    query.prepare(sql);
    query.addBindValue(className);
    
    if (query.exec()) {
        while (query.next()) {
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

bool DatabaseManager::logWarning(const QString &studentId, const QString &appName, const QString &details) {
    QSqlQuery query;
    query.prepare("INSERT INTO warning_logs (student_id, app_name, details) VALUES (?, ?, ?)");
    query.addBindValue(studentId);
    query.addBindValue(appName);
    query.addBindValue(details);
    return query.exec();
}

bool DatabaseManager::saveScreenshotRecord(const QString &studentId, const QByteArray &imageData) {
    QSqlQuery query;
    query.prepare("INSERT INTO warning_logs (student_id, app_name, details, screenshot) VALUES (?, ?, ?, ?)");
    query.addBindValue(studentId);
    query.addBindValue("VIOLATION");
    query.addBindValue("自动截屏记录");
    query.addBindValue(imageData);
    
    if (!query.exec()) {
        qDebug() << "Save screenshot failed:" << query.lastError().text();
        return false;
    }
    return true;
}

// 【新增】在 DatabaseManager::insertMonitorRecord 中确保正确插入数据
bool DatabaseManager::insertMonitorRecord(const QString &studentId, const QString &appName, const QString &details, const QByteArray &imageData) {
    QSqlQuery query;
    query.prepare("INSERT INTO warning_logs (student_id, app_name, details, screenshot) VALUES (?, ?, ?, ?)");
    query.addBindValue(studentId);
    query.addBindValue(appName);
    query.addBindValue(details);
    query.addBindValue(imageData);
    
    if (!query.exec()) {
        qDebug() << "Insert monitor record failed:" << query.lastError().text();
        return false;
    }
    return true;
}

int DatabaseManager::getViolationCount(const QString &studentId) {
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM warning_logs WHERE student_id = ? AND app_name != 'Periodic_Monitor'");
    query.addBindValue(studentId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

// 【新增】获取学生所有截图历史记录
QList<DatabaseManager::ViolationRecord> DatabaseManager::getScreenshotHistory(const QString &studentId) {
    QList<ViolationRecord> records;
    QSqlQuery query;
    // 按时间倒序排列，最新的在前，不区分应用类型
    query.prepare("SELECT id, app_name, timestamp, screenshot FROM warning_logs WHERE student_id = ? ORDER BY timestamp DESC");
    query.addBindValue(studentId);
    
    if (query.exec()) {
        while (query.next()) {
            ViolationRecord rec;
            rec.id = query.value(0).toLongLong();
            rec.appName = query.value(1).toString();
            rec.timestamp = query.value(2).toString();
            rec.screenshot = query.value(3).toByteArray();
            records.append(rec);
        }
    } else {
        qDebug() << "Get history failed:" << query.lastError().text();
    }
    return records;
}

// 初始化文件统计表
bool DatabaseManager::initFileStatsTable() {
    QSqlQuery query;
    QString createTableFileStats = R"(
        CREATE TABLE IF NOT EXISTS file_stats (
            file_path TEXT PRIMARY KEY,
            download_count INTEGER DEFAULT 0,
            last_source_ip TEXT DEFAULT '-',
            last_access_time DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (!query.exec(createTableFileStats)) {
        qDebug() << "Create table file_stats failed:" << query.lastError().text();
        return false;
    }
    return true;
}

// 获取文件统计信息
FileStatRecord DatabaseManager::getFileStat(const QString &filePath) {
    FileStatRecord record;
    record.filePath = filePath;
    
    QSqlQuery query;
    query.prepare("SELECT download_count, last_source_ip FROM file_stats WHERE file_path = ?");
    query.addBindValue(filePath);
    
    if (query.exec() && query.next()) {
        record.downloadCount = query.value(0).toInt();
        record.lastSourceIp = query.value(1).toString();
    } else {
        // 如果不存在，返回默认值（0 次）
        record.downloadCount = 0;
        record.lastSourceIp = "-";
    }
    return record;
}

// 更新文件统计信息（增加下载次数）
bool DatabaseManager::updateFileStat(const QString &filePath, const QString &sourceIp) {
    QSqlQuery query;
    
    // 先尝试更新
    query.prepare(R"(
        UPDATE file_stats 
        SET download_count = download_count + 1, 
            last_source_ip = ?, 
            last_access_time = CURRENT_TIMESTAMP 
        WHERE file_path = ?
    )");
    query.addBindValue(sourceIp);
    query.addBindValue(filePath);
    
    if (query.exec() && query.numRowsAffected() > 0) {
        return true;
    }
    
    // 如果未更新（记录不存在），则插入新记录
    query.prepare(R"(
        INSERT OR REPLACE INTO file_stats (file_path, download_count, last_source_ip) 
        VALUES (?, 1, ?)
    )");
    query.addBindValue(filePath);
    query.addBindValue(sourceIp);
    
    return query.exec();
}