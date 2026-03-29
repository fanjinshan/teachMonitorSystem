#include "client.h"
#include "ui_client.h"
#include <QTcpSocket>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include <QStyle>
#include <QDebug>


// 【新增】辅助函数：将字节数转换为人类可读格式 (B, KB, MB, GB)
static QString formatFileSize(qint64 bytes)
{
    if (bytes < 0) return "-";
    if (bytes == 0) return "0 B";

    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }

    // 根据大小决定小数位数：大于 10 保留 1 位，否则保留 2 位，整数则不保留小数
    if (size > 10.0) {
        return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
    } else {
        return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
    }
}

void client::requestFileList(const QString &targetIp, quint16 targetTcpPort, const QString &path)
{
    // 【修复】增加空指针检查
    if (!m_fileTree) return;

    m_fileTree->clear();
    m_fileTree->setHeaderLabels({"文件名", "类型", "大小", "修改时间", "下载次数", "来自于"});
    m_fileTree->setColumnWidth(0, 280); 
    m_fileTree->setColumnWidth(1, 70);  
    m_fileTree->setColumnWidth(2, 90); 
    m_fileTree->setColumnWidth(3, 160); 
    m_fileTree->setColumnWidth(4, 70);  
    m_fileTree->setColumnWidth(5, 180); 
    
    m_fileTree->header()->setStretchLastSection(true);
    m_fileTree->header()->setMinimumSectionSize(60);

    QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
    loadingItem->setText(0, "正在加载文件列表...");
    loadingItem->setForeground(0, QColor("#7f8c8d"));

    QTcpSocket *socket = new QTcpSocket(this);
    
    connect(socket, &QTcpSocket::connected, this, [socket, path]() {
        if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
        qDebug() << "[Async] Connected to teacher, sending LIST request...";
        QString cmdStr = "LIST|" + path;
        QByteArray cmd = cmdStr.toUtf8();
        
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out << (quint32)cmd.size();
        out.writeRawData(cmd.constData(), cmd.size());
        socket->write(block);
    });

    connect(socket, &QTcpSocket::readyRead, this, [this, loadingItem, socket, path, targetIp]() mutable {
        // 【修复】多重安全检查：防止 m_fileTree 或 socket 已被销毁
        if (!m_fileTree || !socket) return; 

        if (socket->bytesAvailable() < (int)sizeof(quint32)) {
            return;
        }

        QByteArray header = socket->peek(sizeof(quint32));
        QDataStream ds(&header, QIODevice::ReadOnly);
        quint32 blockSize = 0;
        ds >> blockSize;

        if (socket->bytesAvailable() < (int)(sizeof(quint32) + blockSize)) {
            return;
        }

        socket->read(sizeof(quint32)); 
        QByteArray data = socket->read(blockSize);
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        
        // 【修复】安全移除 loadingItem：先检查它是否还在树中
        if (loadingItem && m_fileTree) {
            int index = m_fileTree->indexOfTopLevelItem(loadingItem);
            if (index != -1) {
                m_fileTree->takeTopLevelItem(index);
            }
            delete loadingItem; 
            loadingItem = nullptr; // 标记已删除，防止重复操作
        }

        if (error.error == QJsonParseError::NoError && doc.isArray()) {
            // 再次检查 m_fileTree 有效性
            if (!m_fileTree) return;
            
            m_fileTree->clear(); 
            
            if (path != "/") {
                QTreeWidgetItem *parentItem = new QTreeWidgetItem(m_fileTree);
                parentItem->setText(0, "..");
                parentItem->setText(1, "目录");
                parentItem->setText(5, "-");
                parentItem->setData(0, Qt::UserRole, true);
                parentItem->setForeground(0, QColor("#7f8c8d"));
            }

            for (const QJsonValue &val : doc.array()) {
                QJsonObject obj = val.toObject();
                QString name = obj["name"].toString();
                qint64 sizeBytes = obj["size"].toVariant().toLongLong();
                bool isDir = obj["isDir"].toBool();
                
                QString fileType = isDir ? "文件夹" : (obj["type"].toString().isEmpty() ? "文件" : obj["type"].toString());
                QString modTime = obj["modTime"].toString(); 
                if (modTime.isEmpty()) modTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
                
                int downloadCount = 0;
                if (obj.contains("downloadCount")) {
                    QVariant countVal = obj["downloadCount"].toVariant();
                    downloadCount = countVal.toInt();
                }
                
                QString sourceIp = "-";
                if (obj.contains("sourceIp")) {
                    sourceIp = obj["sourceIp"].toString();
                }
                if ((sourceIp == "-" || sourceIp.isEmpty()) && !isDir) {
                    sourceIp = targetIp;
                }

                QTreeWidgetItem *item = new QTreeWidgetItem(m_fileTree);
                item->setText(0, name);
                item->setText(1, fileType);
                
                // 【修改】使用格式化后的大小显示，文件夹显示为空或特定标识
                if (isDir) {
                    item->setText(2, "");
                } else {
                    item->setText(2, formatFileSize(sizeBytes));
                    // 可选：将原始字节数存储在 UserRole+1 以便后续排序或精确计算使用
                    item->setData(2, Qt::UserRole + 1, sizeBytes); 
                }
                
                item->setText(3, modTime);
                item->setText(4, isDir ? "-" : QString::number(downloadCount));
                item->setText(5, sourceIp);
                
                item->setData(0, Qt::UserRole, isDir);
                
                if (isDir) {
                    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                } else {
                    item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
                }
                
                m_fileTree->addTopLevelItem(item);
            }
            
            if (doc.array().isEmpty() && path == "/") {
                if (m_fileTree) {
                    QTreeWidgetItem *tip = new QTreeWidgetItem(m_fileTree);
                    tip->setText(0, "共享文件夹为空");
                    tip->setForeground(0, Qt::gray);
                }
            }
        } else {
            if (m_fileTree) {
                QTreeWidgetItem *errItem = new QTreeWidgetItem(m_fileTree);
                errItem->setText(0, "解析失败：" + error.errorString());
                errItem->setForeground(0, Qt::red);
            }
        }
        
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket, loadingItem](QAbstractSocket::SocketError /*err*/) {
        if (!socket) return;
        
        // 【修复】安全移除 loadingItem
        if (loadingItem && m_fileTree) {
            int index = m_fileTree->indexOfTopLevelItem(loadingItem);
            if (index != -1) {
                m_fileTree->takeTopLevelItem(index);
            }
            delete loadingItem;
        }
        
        if (m_fileTree) {
            QTreeWidgetItem *errItem = new QTreeWidgetItem(m_fileTree);
            errItem->setText(0, "连接错误：" + socket->errorString());
            errItem->setForeground(0, Qt::red);
        }
        socket->deleteLater();
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);

    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    
    // 【修复】使用 QPointer 安全持有 socket 和 loadingItem，避免悬空指针
    QPointer<QTcpSocket> safeSocket(socket);
    QTreeWidgetItem *safeLoadingItem = loadingItem;

    connect(timeoutTimer, &QTimer::timeout, this, [safeSocket, safeLoadingItem, this]() {
        if (!safeSocket) return; 
        
        // 超时处理：如果还在连接中，强制断开并提示
        if (safeSocket->state() == QAbstractSocket::ConnectingState || safeSocket->state() == QAbstractSocket::ConnectedState) {
             // 检查 loadingItem 是否还在树中，防止访问已删除的内存
             if (safeLoadingItem && m_fileTree) {
                int index = m_fileTree->indexOfTopLevelItem(safeLoadingItem);
                if (index != -1) {
                    m_fileTree->takeTopLevelItem(index);
                    delete safeLoadingItem;
                }
             }
             if (m_fileTree) {
                 QTreeWidgetItem *errItem = new QTreeWidgetItem(m_fileTree);
                 errItem->setText(0, "请求超时：服务器无响应");
                 errItem->setForeground(0, Qt::red);
             }
             safeSocket->abort();
             safeSocket->deleteLater();
        }
    });
    timeoutTimer->start(5000); 

    socket->connectToHost(targetIp, targetTcpPort);
}

void client::onDownloadClicked()
{
    QTreeWidgetItem *item = m_fileTree->currentItem();
    if (!item || item->text(0) == "..") return;
    
    bool isDir = item->data(0, Qt::UserRole).toBool();
    if (isDir) {
        QMessageBox::warning(this, "提示", "无法下载文件夹");
        return;
    }

    QString fileName = item->text(0);
    
    QDir saveDir(m_localSavePath);
    QString rootPath = saveDir.rootPath();
    
    if (!QDir(rootPath).exists()) {
        QMessageBox::critical(this, "下载失败", 
            QString("目标磁盘 %1 不存在！\n\n当前设置的保存路径为：%2\n"
                    "请插入磁盘或点击'保存路径设置'修改下载目录。")
            .arg(rootPath).arg(m_localSavePath));
        return;
    }

    if (!saveDir.exists()) {
        if (!saveDir.mkpath(".")) {
            QMessageBox::critical(this, "下载失败", 
                QString("无法在 %1 创建文件夹！\n\n可能原因：\n"
                        "1. 磁盘空间不足。\n2. 权限不足。\n3. 磁盘被写保护。\n\n"
                        "请点击'保存路径设置'修改下载目录。")
                .arg(m_localSavePath));
            return;
        }
    }

    QString savePath = saveDir.filePath(fileName);
    
    if (QFile::exists(savePath)) {
        if (!QFile::remove(savePath)) {
            QMessageBox::critical(this, "文件占用", 
                QString("文件 \"%1\" 已存在且无法覆盖。\n可能原因：文件正在被其他程序打开。\n请关闭相关程序后重试。").arg(fileName));
            return;
        }
    }
    
    requestFileDownload(m_currentTargetIp, m_currentTargetPort, fileName, m_currentPath, savePath);
}

void client::requestFileDownload(const QString &targetIp, quint16 targetTcpPort, const QString &fileName, const QString &path, const QString &savePath)
{
    struct DownloadContext {
        QTcpSocket *socket = nullptr;
        QFile *file = nullptr;
        enum State { WaitingHeader, WaitingMeta, ReceivingFile } state = WaitingHeader;
        quint32 metaLen = 0;
        qint64 totalFileSize = 0;
        qint64 receivedBytes = 0;
        QString savePath;
        bool finished = false;
        
        ~DownloadContext() {
            if (file) {
                if (file->isOpen()) file->close();
                delete file;
            }
            if (socket) {
                socket->deleteLater();
            }
        }
    };
    
    DownloadContext *ctx = new DownloadContext();
    ctx->savePath = savePath;
    ctx->socket = new QTcpSocket();
    ctx->file = new QFile(savePath);

    QFileInfo fileInfo(savePath);
    if (!fileInfo.dir().exists()) {
        fileInfo.dir().mkpath(".");
    }

    if (!ctx->file->open(QIODevice::WriteOnly)) {
        QString errorMsg = ctx->file->errorString();
        if (errorMsg.isEmpty()) errorMsg = "未知错误 (可能是权限不足或路径无效)";
        
        QMessageBox::critical(nullptr, "保存文件失败", 
            "无法在本地创建/写入文件：" + savePath + "\n错误详情：" + errorMsg + 
            "\n\n建议：\n1. 检查保存路径设置是否正确。\n2. 检查是否有杀毒软件拦截。\n3. 尝试以管理员身份运行程序。");
        delete ctx;
        return;
    }

    connect(ctx->socket, &QTcpSocket::connected, [ctx, path, fileName]() {
        QString cmdStr = "DOWNLOAD|" + path + "|" + fileName;
        QByteArray cmd = cmdStr.toUtf8();
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out << (quint32)cmd.size();
        out.writeRawData(cmd.constData(), cmd.size());
        ctx->socket->write(block);
    });

    connect(ctx->socket, &QTcpSocket::readyRead, [this, ctx]() {
        if (ctx->finished) return;

        while (ctx->socket->bytesAvailable() > 0 && !ctx->finished) {
            if (ctx->state == DownloadContext::WaitingHeader) {
                if (ctx->socket->bytesAvailable() >= 4) {
                    QByteArray h = ctx->socket->read(4);
                    QDataStream ds(&h, QIODevice::ReadOnly);
                    ds >> ctx->metaLen;
                    ctx->state = DownloadContext::WaitingMeta;
                } else {
                    break; 
                }
            }
            
            if (ctx->state == DownloadContext::WaitingMeta) {
                if (ctx->socket->bytesAvailable() >= ctx->metaLen) {
                    QByteArray metaBytes = ctx->socket->read(ctx->metaLen);
                    QJsonDocument metaDoc = QJsonDocument::fromJson(metaBytes);
                    
                    if (metaDoc.object()["status"].toString() != "ok") {
                        ctx->finished = true;
                        QString serverMsg = metaDoc.object()["msg"].toString();
                        if (serverMsg.isEmpty()) serverMsg = "服务端拒绝请求或文件不存在";
                        
                        if (ctx->file) {
                            ctx->file->close();
                            QFile::remove(ctx->file->fileName()); 
                        }
                        
                        QMessageBox::warning(nullptr, "下载失败", serverMsg);
                        delete ctx;
                        return;
                    }
                    ctx->totalFileSize = metaDoc.object()["size"].toVariant().toLongLong();
                    ctx->state = DownloadContext::ReceivingFile;
                    ctx->receivedBytes = 0;
                } else {
                    break;
                }
            }

            if (ctx->state == DownloadContext::ReceivingFile) {
                QByteArray chunk = ctx->socket->readAll();
                if (chunk.isEmpty()) break;
                
                ctx->file->write(chunk);
                ctx->receivedBytes += chunk.size();
                
                if (ctx->receivedBytes >= ctx->totalFileSize) {
                    ctx->file->flush();
                    ctx->file->close();
                    ctx->finished = true;
                    
                    QTimer::singleShot(100, this, [this, savePath = ctx->savePath]() {
                        QMessageBox::information(nullptr, "下载成功", "文件已成功保存至:\n" + savePath);
                    });
                    
                    delete ctx;
                    return;
                }
            }
        }
    });

    connect(ctx->socket, &QTcpSocket::errorOccurred, [ctx](QAbstractSocket::SocketError) {
        if (ctx->finished) return;
        qDebug() << "[Download] Error:" << ctx->socket->errorString();
        if (ctx->state == DownloadContext::ReceivingFile && ctx->receivedBytes > 0 && ctx->receivedBytes < ctx->totalFileSize) {
             QMessageBox::warning(nullptr, "下载中断", "网络连接错误:\n" + ctx->socket->errorString());
        } else if (ctx->state == DownloadContext::WaitingHeader || ctx->state == DownloadContext::WaitingMeta) {
             QMessageBox::warning(nullptr, "连接失败", "无法连接到文件服务器:\n" + ctx->socket->errorString());
        }
        delete ctx;
    });
    
    connect(ctx->socket, &QTcpSocket::disconnected, [ctx]() {
        if (ctx->finished) {
            return; 
        }

        if (ctx->state == DownloadContext::ReceivingFile && ctx->receivedBytes >= ctx->totalFileSize) {
            ctx->file->flush();
            ctx->file->close();
            ctx->finished = true;
            
            QTimer::singleShot(100, nullptr, [savePath = ctx->savePath]() {
                QMessageBox::information(nullptr, "下载成功", "文件已成功保存至:\n" + savePath);
            });
            delete ctx;
            return;
        }

        if (ctx->state == DownloadContext::WaitingHeader || ctx->state == DownloadContext::WaitingMeta) {
             if (!ctx->finished) {
                 QMessageBox::warning(nullptr, "下载异常", "服务器提前断开连接，文件可能不存在或格式错误。");
             }
        } else {
             QMessageBox::warning(nullptr, "下载中断", "连接意外断开，文件可能不完整。");
        }
        delete ctx;
    });
    
    QTimer *dlTimeout = new QTimer();
    dlTimeout->setSingleShot(true);
    
    connect(dlTimeout, &QTimer::timeout, [ctx, dlTimeout]() {
        if (ctx->finished) return;
        if (ctx->socket->state() == QAbstractSocket::ConnectingState || 
            ctx->socket->state() == QAbstractSocket::ConnectedState) {
            ctx->socket->abort();
            QMessageBox::warning(nullptr, "超时", "下载请求超时，请检查网络或服务器状态。");
            delete ctx; 
        }
        dlTimeout->deleteLater();
    });
    dlTimeout->start(10000); 

    ctx->socket->connectToHost(targetIp, targetTcpPort);
}