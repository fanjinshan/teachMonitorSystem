#include "client.h"
#include "ui_client.h"
#include <QTcpSocket>
#include <QMessageBox>
#include <QDebug>
#include <QByteArray>
#include <QHostAddress>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QTreeWidget>
#include <QHeaderView>
#include <QDateTime>

client::client(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::client)
{
    ui->setupUi(this);
    
    ui->passwordEdit->setEchoMode(QLineEdit::Password);
    m_socket = new QTcpSocket(this);
    m_state = State_Disconnected;
    m_localFile = nullptr;
    m_fileSize = 0;
    m_bytesReceived = 0;
    m_serverIp = "";
    
    m_savePath = "E:/fileReceive/";
    QDir receiveDir(m_savePath);
    if (!receiveDir.exists()) {
        receiveDir.mkpath(".");
    }

    connect(m_socket, &QTcpSocket::connected, this, &client::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &client::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &client::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &client::onErrorOccurred);
            
    connect(ui->fileList, &QTreeWidget::itemDoubleClicked, this, &client::on_fileList_itemDoubleClicked);
    
    QTreeWidget *treeWidget = qobject_cast<QTreeWidget*>(ui->fileList);
    if (treeWidget) {
        treeWidget->setColumnCount(Col_Count);
        
        QStringList headers;
        headers << "文件名" << "类型" << "大小" << "修改时间" << "下载次数" << "来源 IP";
        treeWidget->setHeaderLabels(headers);
        
        QHeaderView *header = treeWidget->header();
        header->setStretchLastSection(false);
        header->setSectionResizeMode(Col_Name, QHeaderView::Stretch);
        header->setSectionResizeMode(Col_Type, QHeaderView::Interactive);
        header->setSectionResizeMode(Col_Size, QHeaderView::Interactive);
        header->setSectionResizeMode(Col_Time, QHeaderView::Interactive);
        header->setSectionResizeMode(Col_Downloads, QHeaderView::Interactive);
        header->setSectionResizeMode(Col_Source, QHeaderView::Interactive);
        
        header->setMinimumSectionSize(60);

        header->resizeSection(Col_Name, 150);
        header->resizeSection(Col_Type, 80);
        header->resizeSection(Col_Size, 100);
        header->resizeSection(Col_Time, 160);
        header->resizeSection(Col_Downloads, 80);
        header->resizeSection(Col_Source, 120);
        
        header->setSectionsMovable(true);
        header->setSectionsClickable(true);
        
        treeWidget->setIndentation(0); 
        treeWidget->setRootIsDecorated(false);
    } else {
        qCritical() << "Error: ui->fileList is not a QTreeWidget. Please update the UI file.";
    }

    // 新增：连接刷新按钮和退出按钮
    connect(ui->refreshBt, &QPushButton::clicked, this, &client::on_refreshBt_clicked);
    connect(ui->quitBt, &QPushButton::clicked, this, &client::on_quitBt_clicked);

    connectToServer();
}

client::~client()
{
    delete ui;
}

void client::on_loginBt_clicked()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        if (ui->statusbar) ui->statusbar->showMessage("尚未连接到服务器");
        QMessageBox::warning(this, "错误", "请先连接到服务器！");
        return;
    }

    QString username = ui->account->text();
    QString password = ui->passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "警告", "账号或密码不能为空");
        return;
    }

    QString message = QString("LOGIN|%1|%2").arg(username).arg(password);
    QByteArray data = message.toUtf8();
    
    qDebug() << "[Client] Sending:" << message;

    QByteArray sendBuffer; 
    QDataStream out(&sendBuffer, QIODevice::WriteOnly);
    out << (quint32)data.size(); 
    out.writeRawData(data.constData(), data.size());
    
    m_socket->write(sendBuffer);
    
    if (ui->statusbar) ui->statusbar->showMessage("正在登录...");
}

void client::onConnected()
{
    if (ui->statusbar) {
        ui->statusbar->showMessage("已连接到服务器，请输入账号密码登录");
    }
    qDebug() << "Connected to server";
}

void client::onReadyRead()
{
    static QByteArray s_recvBuffer; 
    
    s_recvBuffer.append(m_socket->readAll());
    
    while (true) {
        if (static_cast<quint32>(s_recvBuffer.size()) <= sizeof(quint32)) {
            break; 
        }
        
        quint32 blockSize = 0;
        QDataStream tempStream(&s_recvBuffer, QIODevice::ReadOnly);
        tempStream >> blockSize;
        
        if (static_cast<quint32>(s_recvBuffer.size()) < sizeof(quint32) + blockSize) {
            break; 
        }
        
        QByteArray data = s_recvBuffer.mid(sizeof(quint32), blockSize);
        s_recvBuffer.remove(0, sizeof(quint32) + blockSize);
        
        if (m_state == State_ReceivingFile && m_localFile && m_localFile->isOpen()) {
            m_localFile->write(data);
            m_bytesReceived += data.size();
            
            QString progress = QString("正在下载：%1 (%2/%3)")
                               .arg(m_downloadingFileName)
                               .arg(m_bytesReceived)
                               .arg(m_fileSize);
            if (ui->statusbar) ui->statusbar->showMessage(progress);
            
            if (m_bytesReceived >= m_fileSize) {
                m_localFile->close();
                
                // 【新增】下载完成后校验文件
                QFileInfo checkFile(m_localFile->fileName());
                if (!checkFile.exists()) {
                    QMessageBox::critical(this, "严重错误", "文件写入失败！文件在磁盘中不存在。\n路径：" + checkFile.absoluteFilePath());
                } else if (checkFile.size() != m_fileSize) {
                    QMessageBox::warning(this, "警告", "文件大小不匹配！\n预期：" + QString::number(m_fileSize) + "\n实际：" + QString::number(checkFile.size()) + "\n路径：" + checkFile.absoluteFilePath());
                } else {
                    // 文件校验成功
                    qInfo() << "[Client] 文件下载并校验成功：" << checkFile.absoluteFilePath();
                    // 可选：自动打开文件夹让用户亲眼看到
                    // QDesktopServices::openUrl(QUrl::fromLocalFile(checkFile.absolutePath())); 
                }

                delete m_localFile;
                m_localFile = nullptr;
                m_state = State_LoggedIn;
                
                QMessageBox::information(this, "完成", "文件下载成功！\n保存位置：" + checkFile.absoluteFilePath());
                if (ui->statusbar) ui->statusbar->showMessage("下载完成，正在刷新列表...");
                
                // 下载完成后刷新列表，此时服务器会返回更新后的下载次数和 IP
                requestFileList(m_currentPath);
            }
            continue; 
        }

        QString response = QString::fromUtf8(data);
        
        qDebug() << "[Client] Received Cmd:" << response;

        if (response == "LOGIN_OK") {
            m_state = State_LoggedIn;
            QString userName = ui->account ? ui->account->text() : "用户";
            QString msg = "登录成功！欢迎 " + userName;
            if (ui->statusbar) ui->statusbar->showMessage(msg);
            QMessageBox::information(this, "成功", msg);
            
            m_currentPath = "/";
            requestFileList("/"); 
        } else if (response == "LOGIN_FAIL") {
            m_state = State_Disconnected;
            if (ui->statusbar) ui->statusbar->showMessage("登录失败：账号或密码错误");
            QMessageBox::critical(this, "失败", "账号或密码错误！");
        } 
        else if (response.startsWith("FILE_LIST|")) {
            QString jsonStr = response.mid(10);
            parseFileListData(jsonStr.toUtf8());
        }
        else if (response.startsWith("FILE_START|")) {
            QStringList parts = response.split('|');
            if (parts.size() >= 3) {
                QString fileName = parts[1];
                qint64 size = parts[2].toLongLong();
                startDownload(fileName, size);
            } else {
                qDebug() << "[Client] Error: Invalid FILE_START format";
            }
        }
        else if (response.startsWith("ERROR|")) {
            QString errorMsg = response.mid(6);
            qDebug() << "[Client] Server Error:" << errorMsg;
            if (ui->statusbar) ui->statusbar->showMessage("服务器错误：" + errorMsg);
        }
        else {
            qDebug() << "Received unknown command:" << response;
        }
    }
}

void client::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    QString errorStr;
    switch (socketError) {
    case QAbstractSocket::ConnectionRefusedError:
        errorStr = "连接被拒绝 (请检查服务器是否启动)";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorStr = "远程主机关闭连接";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorStr = "主机未找到 (IP 地址错误)";
        break;
    case QAbstractSocket::SocketAccessError:
        errorStr = "套接字访问错误";
        break;
    default:
        errorStr = m_socket->errorString();
        break;
    }
    
    if (ui->statusbar) {
        ui->statusbar->showMessage("连接错误：" + errorStr);
    }
    qDebug() << "Socket Error:" << errorStr;
}

void client::on_fileList_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    QTreeWidget *treeWidget = qobject_cast<QTreeWidget*>(ui->fileList);
    if (!treeWidget || !item) return;
    
    if (!treeWidget->isEnabled()) {
        return;
    }

    int type = item->data(Col_Name, Qt::UserRole).toInt();
    
    if (type == 2) {
        if (m_currentPath == "/" || m_currentPath.isEmpty()) {
            return;
        }
        
        QString newPath = m_currentPath;
        if (newPath.endsWith("/")) newPath.chop(1);
        int lastSlash = newPath.lastIndexOf("/");
        if (lastSlash == -1) {
            newPath = "/";
        } else if (lastSlash == 0) {
            newPath = "/";
        } else {
            newPath = newPath.left(lastSlash);
        }
        
        m_currentPath = newPath;
        
        treeWidget->setEnabled(false);
        if (ui->statusbar) ui->statusbar->showMessage("加载中...");
        
        requestFileList(m_currentPath);
        return;
    }
    
    if (type == 1) {
        QString dirName = item->text(Col_Name);
        if (dirName.isEmpty()) return;

        QString newPath = m_currentPath;
        if (newPath != "/") newPath += "/";
        newPath += dirName;
        
        m_currentPath = newPath;
        
        treeWidget->setEnabled(false);
        if (ui->statusbar) ui->statusbar->showMessage("正在加载目录：" + dirName);
        
        requestFileList(m_currentPath);
    } else {
        treeWidget->setCurrentItem(item);
        QString fileName = item->text(Col_Name);
        if (ui->statusbar) ui->statusbar->showMessage("已选择文件：" + fileName + "，请点击下载按钮开始");
    }
}

void client::on_downloadBt_clicked()
{
    qDebug() << "[DEBUG] on_downloadBt_clicked triggered.";

    if (m_state != State_LoggedIn) {
        qDebug() << "[DEBUG] Download failed: Not logged in. Current state:" << m_state;
        QMessageBox::warning(this, "错误", "请先登录服务器！");
        return;
    }

    QTreeWidget *treeWidget = qobject_cast<QTreeWidget*>(ui->fileList);
    if (!treeWidget) return;

    QTreeWidgetItem *selectedItem = treeWidget->currentItem();
    if (!selectedItem) {
        qDebug() << "[DEBUG] Download failed: No item selected.";
        QMessageBox::warning(this, "警告", "请在列表中选择一个文件进行下载");
        return;
    }

    int type = selectedItem->data(Col_Name, Qt::UserRole).toInt();
    qDebug() << "[DEBUG] Selected item type:" << type << "Text:" << selectedItem->text(Col_Name);

    if (type == 1) {
        qDebug() << "[DEBUG] Download failed: Selected item is a directory.";
        QMessageBox::warning(this, "警告", "不能下载目录，请双击进入目录。");
        return;
    }
    if (type == 2) {
        return;
    }

    QString fileName = selectedItem->text(Col_Name);
    if (fileName.isEmpty()) {
         qDebug() << "[DEBUG] Download failed: Filename parsed is empty.";
         QMessageBox::warning(this, "错误", "文件名解析错误");
         return;
    }

    qDebug() << "[DEBUG] Calling startDownloadRequest for:" << fileName;
    startDownloadRequest(fileName);
}

void client::on_downLoadBt_clicked()
{
    qDebug() << "[DEBUG] on_downLoadBt_clicked triggered (Alias for on_downloadBt_clicked).";
    on_downloadBt_clicked();
}

void client::startDownloadRequest(const QString &fileName)
{
    if (fileName.isEmpty()) {
        qDebug() << "[Client] Error: Attempted to download with empty filename!";
        QMessageBox::warning(this, "错误", "下载失败：文件名为空");
        return;
    }

    QString message = QString("DOWNLOAD|%1|%2").arg(m_currentPath).arg(fileName);
    QByteArray data = message.toUtf8();

    qDebug() << "[Client] Sending Download Request:" << message;

    QByteArray sendBuffer;
    QDataStream out(&sendBuffer, QIODevice::WriteOnly);
    out << (quint32)data.size();
    out.writeRawData(data.constData(), data.size());

    qint64 bytesWritten = m_socket->write(sendBuffer);
    qDebug() << "[Client] Bytes written to socket:" << bytesWritten;

    if (ui->statusbar) {
        ui->statusbar->showMessage("正在请求下载：" + fileName);
    }
}

void client::startDownload(const QString &fileName, qint64 size)
{
    m_downloadingFileName = fileName;
    m_fileSize = size;
    m_bytesReceived = 0;
    
    QString savePath = m_savePath + fileName;
    
    QDir dir(m_savePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
             QMessageBox::critical(this, "错误", "保存目录不存在且无法创建：" + m_savePath);
             m_state = State_LoggedIn;
             return;
        }
    }

    m_localFile = new QFile(savePath);
    if (!m_localFile->open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "错误", "无法创建本地文件：" + savePath + "\n错误信息：" + m_localFile->errorString());
        delete m_localFile;
        m_localFile = nullptr;
        m_state = State_LoggedIn;
        return;
    }
    
    m_state = State_ReceivingFile;
    
    // 【新增】打印绝对路径，方便用户确认文件保存位置
    QString absoluteFilePath = m_localFile->fileName(); // 或者 QFileInfo(savePath).absoluteFilePath()
    qInfo() << "[Client] 文件将保存至绝对路径:" << absoluteFilePath;
    if (ui->statusbar) {
        ui->statusbar->showMessage("开始下载，保存位置：" + absoluteFilePath);
    }
    
    qDebug() << "Start receiving file:" << fileName << "Size:" << size << "Save to:" << absoluteFilePath;
}

void client::onDisconnected()
{
    if (ui->statusbar) {
        ui->statusbar->showMessage("与服务器断开连接");
    }
    qDebug() << "Disconnected from server";
}

void client::onBytesWritten(qint64 bytes)
{
    qDebug() << "Bytes written:" << bytes;
}

void client::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectingState) {
        return;
    }
    
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }

    QString ip = "192.168.14.114";
    m_serverIp = ip;
    quint16 port = 9999;

    qDebug() << "正在连接服务器：" << ip << ":" << port;
    if (ui->statusbar) ui->statusbar->showMessage(QString("正在连接 %1:%2...").arg(ip).arg(port));
    
    m_state = State_Connecting;
    
    m_socket->connectToHost(QHostAddress(ip), port);
}

void client::requestFileList(const QString &path)
{
    if (m_state != State_LoggedIn) return;

    QString message = QString("LIST|%1").arg(path);
    QByteArray data = message.toUtf8();

    QByteArray sendBuffer;
    QDataStream out(&sendBuffer, QIODevice::WriteOnly);
    out << (quint32)data.size();
    out.writeRawData(data.constData(), data.size());

    m_socket->write(sendBuffer);
    qDebug() << "Requesting file list for:" << path;
}

QString formatFileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

QString formatTime(const QDateTime &dt) {
    if (!dt.isValid()) return "-";
    return dt.toString("yyyy-MM-dd hh:mm");
}

void client::parseFileListData(const QByteArray &data)
{
    QTreeWidget *treeWidget = qobject_cast<QTreeWidget*>(ui->fileList);
    if (!treeWidget) return;

    treeWidget->clear();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "JSON Parse Error:" << error.errorString();
        if (ui->statusbar) ui->statusbar->showMessage("解析文件列表失败");
        treeWidget->setEnabled(true);
        return;
    }
    
    QJsonArray array = doc.array();
    QDateTime now = QDateTime::currentDateTime();
    
    if (m_currentPath != "/" && !m_currentPath.isEmpty()) {
        QTreeWidgetItem *upItem = new QTreeWidgetItem(treeWidget);
        upItem->setText(Col_Name, "../ (返回上一级)");
        upItem->setText(Col_Type, "DIR");
        upItem->setData(Col_Name, Qt::UserRole, 2);
        upItem->setForeground(Col_Name, QBrush(Qt::gray));
        for (int i = 1; i < Col_Count; ++i) {
            upItem->setText(i, "-");
        }
    }
    
    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();
        QString name = obj["name"].toString();
        bool isDir = obj["isDir"].toBool();
        qint64 size = static_cast<qint64>(obj["size"].toDouble());
        
        // 新增：读取服务器返回的统计信息
        int downloadCount = 0;
        QString sourceIp = "-";
        
        if (!isDir) {
            if (obj.contains("downloadCount")) {
                downloadCount = obj["downloadCount"].toInt();
            }
            if (obj.contains("sourceIp")) {
                sourceIp = obj["sourceIp"].toString();
            }
        }
        
        QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget);
        
        item->setText(Col_Name, name);
        item->setData(Col_Name, Qt::UserRole, isDir ? 1 : 0);
        
        item->setText(Col_Type, isDir ? "文件夹" : "文件");
        
        item->setText(Col_Size, isDir ? "-" : formatFileSize(size));
        item->setData(Col_Size, Qt::UserRole, size);
        
        item->setText(Col_Time, formatTime(now));
        
        // 设置从服务器获取的真实统计值
        item->setText(Col_Downloads, QString::number(downloadCount));
        item->setData(Col_Downloads, Qt::UserRole, downloadCount);
        
        item->setText(Col_Source, sourceIp);
        
        if (isDir) {
            // item->setFont(Col_Name, boldFont);
        }
    }
    
    if (ui->statusbar) {
        ui->statusbar->showMessage("当前路径：" + (m_currentPath.isEmpty() ? "/" : m_currentPath) + " (共" + QString::number(array.size()) + "项)");
    }

    treeWidget->setEnabled(true);
}

void client::on_settingBt_clicked()
{
    QString selectedDir = QFileDialog::getExistingDirectory(this, tr("选择文件保存路径"), m_savePath);
    
    if (selectedDir.isEmpty()) {
        return;
    }

    QDir dir(selectedDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::critical(this, "错误", "无法创建目录：" + selectedDir);
            return;
        }
    }

    m_savePath = selectedDir;
    
    m_savePath = QDir::toNativeSeparators(m_savePath).replace("\\", "/");
    if (!m_savePath.endsWith("/")) {
        m_savePath += "/";
    }

    if (ui->statusbar) {
        ui->statusbar->showMessage("保存路径已设置为：" + m_savePath);
    }
    qDebug() << "[Client] Save path updated to:" << m_savePath;
}

// 新增：刷新按钮槽函数
void client::on_refreshBt_clicked()
{
    if (m_state != State_LoggedIn) {
        QMessageBox::information(this, "提示", "请先登录服务器后再刷新列表");
        return;
    }

    if (ui->statusbar) {
        ui->statusbar->showMessage("正在刷新文件列表...");
    }
    qDebug() << "[Client] Refreshing file list for path:" << m_currentPath;
    
    // 重新请求当前路径的文件列表
    requestFileList(m_currentPath);
}

// 新增：退出按钮槽函数
void client::on_quitBt_clicked()
{
    qDebug() << "[Client] Quit button clicked. Closing main window.";
    
    // 询问用户是否确认退出
    int ret = QMessageBox::question(this, "确认退出", "确定要退出当前页面吗？", 
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // 关闭当前窗口，而不是直接退出整个应用程序进程
        // 如果这是最后一个窗口且没有设置特殊属性，程序可能会自然结束，但行为更符合“关闭页面”
        this->close();
        
        // 如果需要在关闭窗口前断开连接，可以在这里添加
        if (m_socket && m_socket->isOpen()) {
            m_socket->disconnectFromHost();
        }
    }
}