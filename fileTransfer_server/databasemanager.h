#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>

struct StudentInfo {
    QString id;
    QString name;
    QString ip;
    bool isOnline;
    QString className; // 【新增】所属班级
};

// 【新增】班级信息结构
struct ClassInfo {
    int id;
    QString className;
    qint64 createTime;
};

// 【新增】文件统计信息结构（用于数据库映射）
struct FileStatRecord {
    int downloadCount = 0;
    QString lastSourceIp = "-";
    QString filePath; // 相对路径作为唯一键
};

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance();
    bool init();
    
    // 学生信息管理
    bool registerStudent(const QString &id, const QString &name, const QString &ip);
    StudentInfo getStudentInfo(const QString &id);
    QMap<QString, StudentInfo> getAllStudents();
    void updateStudentStatus(const QString &id, bool online);
    // 【新增】更新学生班级
    bool updateStudentClass(const QString &id, const QString &className);

    // 【新增】班级管理
    bool createClass(const QString &className);
    // 【新增】删除班级
    bool deleteClass(const QString &className);
    QList<ClassInfo> getAllClasses();
    // 获取班级成员 (onlineOnly=false 获取所有)
    QList<StudentInfo> getClassMembers(const QString &className, bool onlineOnly = false);

    // 监控日志
    bool logWarning(const QString &studentId, const QString &appName, const QString &details);
    bool saveScreenshotRecord(const QString &studentId, const QByteArray &imageData);
    
    // 【新增】插入完整的监控记录（包含应用名、详情、截图）
    bool insertMonitorRecord(const QString &studentId, const QString &appName, const QString &details, const QByteArray &imageData);
    
    // 【新增】统计与历史查询
    int getViolationCount(const QString &studentId);
    struct ViolationRecord {
        qint64 id;
        QString appName;
        QString timestamp;
        QByteArray screenshot;
    };
    // 【新增】获取学生所有截图历史记录（包括定期监控和违规截图）
    QList<ViolationRecord> getScreenshotHistory(const QString &studentId);

    // 【新增】文件统计管理
    bool initFileStatsTable();
    FileStatRecord getFileStat(const QString &filePath);
    bool updateFileStat(const QString &filePath, const QString &sourceIp);

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H