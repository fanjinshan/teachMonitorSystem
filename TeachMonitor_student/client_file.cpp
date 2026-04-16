#include "client.h"

// ==================== 辅助函数 ====================
/**
 * @brief 将字节数格式化为人类可读的字符串（B, KB, MB, GB, TB）
 * @param bytes 文件大小（字节）
 * @return 格式化后的字符串，如 "1.23 MB"
 */
static QString formatFileSize(qint64 bytes)
{
    //负数返回"-"
    if(bytes < 0) return "-";
    //0 字节返回 "0 B"
    if(bytes == 0) return "0 B";

    //单位列表，从B到TB
    const QStringList units = {"B","KB","MB","GB","TB"};
    int unitIndex = 0;//当前单位索引
    double size = static_cast<double>(bytes);//转换为浮点数

    //循环除以1024，直到小于1024或到达最大单位
    while(size >= 1024.0 && unitIndex < units.size() - 1 )
    {
        size /= 1024.0;
        unitIndex++;
    }

    //根据大小决定小数位数：大于10保留1位，否则保留2位，整数则不保留小数
    if(size > 10.0)
    {
        return QString("%1 %2").arg(size,0,'f',1).arg(units[unitIndex]);
    }
    else
    {
        return QString("%1 %2").arg(size,0,'f',2).arg(units[unitIndex]);
    }
}

// ==================== 文件列表请求 ====================
/**
 * @brief 向教师端请求文件列表
 * @param targetIp 教师端 IP
 * @param targetTcpPort 教师端 TCP 端口
 * @param path 请求的路径（如 "/" 或 "/subdir"）
 *
 * 建立 TCP 连接，发送 LIST|path 命令，接收 JSON 数组格式的文件列表，
 * 解析后填充到文件树控件 m_fileTree 中。
 */
void client::requestFileList(const QString &targetIp,quint16 targetTcpPort,const QString& path)
{
    //空指针检查
    if(!m_fileTree) return;

    //清空当前文件树内容
    m_fileTree->clear();
    //设置文件树的列标题（共6列）
    m_fileTree->setHeaderLabels({"文件名","类型","大小","修改时间","下载次数","来自于"});
    //设置各列宽度
    m_fileTree->setColumnWidth(0,280);//文件名
    m_fileTree->setColumnWidth(1,70);//类型
    m_fileTree->setColumnWidth(2,90);//大小
    m_fileTree->setColumnWidth(3,160);//修改时间
    m_fileTree->setColumnWidth(4,70);//下载次数
    m_fileTree->setColumnWidth(5,180);//来自于
    //最后一列(来自于)自动拉伸填满剩余空间
    m_fileTree->header()->setStretchLastSection(true);
    //设置表头的最小节大小为60像素
    m_fileTree->header()->setMinimumSectionSize(60);

    //创建一个临时的"正在加载..."项,添加到文件树，用户可看到等待状态
    QTreeWidgetItem *loadingItem = new QTreeWidgetItem(m_fileTree);
    loadingItem->setText(0,"正在加载文件列表...");
    loadingItem->setForeground(0,QColor("#7f8c8d"));

    //创建TCP套接字，父对象为this,由Qt管理生命周期
    QTcpSocket *socket = new QTcpSocket(this);

    //-------------------连接建立成功时的处理-------------
    connect(socket,&QTcpSocket::connected,this,[socket,path](){
        //如果套接字已被销毁或不在连接状态，则返回
        if(!socket || socket->state() != QAbstractSocket::ConnectedState) return;
        qDebug() << "[异步] 已连接到教师端，正在发送LIST请求";
        //构建命令字符串：LIST|路径
        QString cmdStr = "LIST|" + path;
        QByteArray cmd = cmdStr.toUtf8();

        //封装带长度前缀的协议包：4字节长度 + 命令数据
        QByteArray block;
        QDataStream out(&block,QIODevice::WriteOnly);
        out<<(quint32)cmd.size();//先写入长度
        out.writeRawData(cmd.constData(),cmd.size());//再写入命令数据
        socket->write(block);//发送到教师端
    });

    //------------收到数据时的处理（可能会分多次到达）-------------
    connect(socket,&QTcpSocket::readyRead,this,[this,loadingItem,socket,path,targetIp]()mutable{
        //多次安全检查，防止m_fileTree 或 Socket 已被销毁
        if(!m_fileTree || !socket) return;

        //如果接收到的数据不足4字节(长度头),等待更多数据
        if(socket->bytesAvailable() < (int)sizeof(quint32))
        {
            return;
        }

        //看一下前4字节(长度头)，但是不消耗数据
        QByteArray header = socket->peek(sizeof(quint32));
        QDataStream ds(&header,QIODevice::ReadOnly);
        quint32 blockSize = 0;
        ds >> blockSize;//读取数据块大小

        //如果收到的数据不足以包含长度头 + 数据块，则等待更多数据
        if(socket->bytesAvailable() < (int)(sizeof(quint32) + blockSize))
        {
            return;
        }

        //消耗掉长度头(4字节)
        socket->read(sizeof(quint32));
        //读取实际的数据块
        QByteArray data = socket->read(blockSize);

        //解析JSON数据
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data,&error);

        //安全移除loadingItem,先检查它是否还在树中，然后删除
        if(loadingItem && m_fileTree)
        {
            int index = m_fileTree->indexOfTopLevelItem(loadingItem);
            if(index != -1)
            {
                m_fileTree->takeTopLevelItem(index);//从树中移除
            }
            delete loadingItem;//释放内存
            loadingItem = nullptr;//标记已删除，防止重复操作
        }

        //如果JSON解析成功且为数组
        if(error.error == QJsonParseError::NoError && doc.isArray())
        {
            //再次检查m_fileTree 有效性
            if(!m_fileTree) return;

            //清空文件树
            m_fileTree->clear();

            //如果当前路径不是根目录，添加一个".."项，用于返回上一级目录
            if(path != "/")
            {
                QTreeWidgetItem *parentItem = new QTreeWidgetItem(m_fileTree);
                parentItem->setText(0,"返回上一级目录/..");
                parentItem->setData(0,Qt::UserRole,true);//标记为目录
                parentItem->setForeground(0,QColor("#7f8c8d"));
            }

            //遍历JSON数组中的每一项(文件或目录)
            for(const QJsonValue &val : doc.array())
            {
                QJsonObject obj = val.toObject();
                QString name = obj["name"].toString();
                qint64 sizeBytes = obj["size"].toVariant().toLongLong();//大小(字节)
                bool isDir = obj["isDir"].toBool();//是否为目录

                //类型：目录显示“文件夹”，文件则尝试从JSOn中获取类型，否则为“文件”
                QString fileType = isDir ? "文件夹" : (obj["type"].toString().isEmpty() ? "文件" : obj["type"].toString());
                //修改时间：如果没有则使用当前时间
                QString modTime = obj["modTime"].toString();
                if(modTime.isEmpty()) modTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");

                //下载次数（仅文件显示）
                int downloadCount = 0;
                if(obj.contains("downloadCount"))
                {
                    QVariant countVal = obj["downloadCount"].toVariant();
                    downloadCount = countVal.toInt();
                }

                //来源IP(上次下载该文件的客户端IP)
                QString sourceIp = "-";
                if(obj.contains("sourceIp"))
                {
                    sourceIp = obj["sourceIp"].toString();
                }
                //如果来源IP为空或为"-"，且是文件，则使用教师端IP作为默认值
                if((sourceIp == "-" || sourceIp.isEmpty()) && !isDir)
                {
                    sourceIp = targetIp;
                }

                //创建新的树项
                QTreeWidgetItem *item = new QTreeWidgetItem(m_fileTree);
                item->setText(0,name);//文件名
                item->setText(1,fileType);//类型
                //使用格式化后的大小显示，文件夹显示为空
                if(isDir)
                {
                    item->setText(2,"");//目录不显示大小
                }
                else
                {
                    item->setText(2,formatFileSize(sizeBytes));//格式化后的大小
                }
                item->setText(3,modTime);//修改时间
                item->setText(4,isDir ? "-" :QString::number(downloadCount));//下载次数
                item->setText(5,sourceIp);//来源IP

                item->setData(0,Qt::UserRole,isDir);//保存是否为目录

                //设置图标（文件夹或文件）
                if(isDir)
                {
                    item->setIcon(0,style()->standardIcon(QStyle::SP_DirIcon));
                }
                else
                {
                    item->setIcon(0,style()->standardIcon(QStyle::SP_FileIcon));
                }

                //将项添加到文件树
                m_fileTree->addTopLevelItem(item);
            }

            //如果Json数组为空且路径为根目录，显示提示信息
            if(doc.array().isEmpty() && path == "/")
            {
                if(m_fileTree)
                {
                    QTreeWidgetItem* tip = new QTreeWidgetItem(m_fileTree);
                    tip->setText(0,"共享文件夹为空");
                    tip->setForeground(0,Qt::gray);
                }
            }
        }
        else
        {
            //JSON解析失败，显示错误信息
            if(m_fileTree)
            {
                QTreeWidgetItem* errItem = new QTreeWidgetItem(m_fileTree);
                errItem->setText(0,"解析失败：" + error.errorString());
                errItem->setForeground(0,Qt::red);
            }
        }

        //数据接收完毕，删除套接字（需要释放资源）
        socket->deleteLater();
    });

    //------------连接错误处理----------
    connect(socket,&QTcpSocket::errorOccurred,this,[this,socket,loadingItem](QAbstractSocket::SocketError){
        if(!socket) return;

        //安全移除loadingItem
        if (loadingItem && m_fileTree) {
            int index = m_fileTree->indexOfTopLevelItem(loadingItem);
            if (index != -1) {
                m_fileTree->takeTopLevelItem(index);
            }
            delete loadingItem;
        }

        //显示错误信息
        if(m_fileTree)
        {
            QTreeWidgetItem *errItem = new QTreeWidgetItem(m_fileTree);
            errItem->setText(0,"连接错误：" + socket->errorString());
            errItem->setForeground(0,Qt::red);
        }
        socket->deleteLater();//删除套接字
    });

    //连接断开时自动删除套接字
    connect(socket,&QTcpSocket::disconnected,socket,&QTcpSocket::deleteLater);

    //----------设置超时定时器----------
    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);//只触发一次

    //使用QPointer安全持有socket和loadingItem，避免悬空指针
    QPointer<QTcpSocket> safeSocket(socket);
    QTreeWidgetItem *safeLoadingItem = loadingItem;

    //超时处理：如果5秒内未收到响应或连接未完成，则断开并提示超时
    connect(timeoutTimer,&QTimer::timeout,this,[safeSocket,safeLoadingItem,this](){
        if(!safeSocket) return;

        //如果套接字仍在连接中或已连接但未收到数据（状态为ConnectingState 或 ConnectedState）
        if(safeSocket->state() == QAbstractSocket::ConnectingState || safeSocket->state() == QAbstractSocket::ConnectedState)
        {
            //检查loadingItem是否还在树中，防止访问已删除的内存
            if(safeLoadingItem && m_fileTree)
            {
                int index = m_fileTree->indexOfTopLevelItem(safeLoadingItem);
                if(index != -1)
                {
                    m_fileTree->takeTopLevelItem(index);
                    delete safeLoadingItem;
                }
            }
            if(m_fileTree)
            {
                QTreeWidgetItem *errItem = new QTreeWidgetItem(m_fileTree);
                errItem->setText(0,"请求超时：服务器无响应");
                errItem->setForeground(0,Qt::red);
            }
            safeSocket->abort();//强制断开连接
            safeSocket->deleteLater();//删除套接字
        }
    });

    timeoutTimer->start(5000);//5秒超时

    //发起连接
    socket->connectToHost(targetIp,targetTcpPort);
}

// ==================== 下载按钮响应 ====================
/**
 * @brief 下载按钮点击响应
 *
 * 获取当前选中的文件项，检查是否为文件（非目录），然后调用 requestFileDownload 开始下载。
 */
void client::onDownloadClicked()
{
    //获取当前选中的项
    QTreeWidgetItem* item = m_fileTree->currentItem();
    //如果没有选中的项或选中的是".."，则忽略
    if(!item || item->text(0) == "返回上一级目录/..") return;

    //检查是否为目录(通过UserRole中保存的isDir标志)
    bool isDir = item->data(0,Qt::UserRole).toBool();
    if(isDir)
    {
        QMessageBox::warning(this,"提示","无法下载文件夹");
        return;
    }

    //获取文件名
    QString fileName = item->text(0);

    //检查保存目录是否存在
    QDir saveDir(m_localSavePath);
    QString rootPath = saveDir.rootPath();//获取根路径，比如C:/

    //如果根路径不存在，提示错误
    if(!QDir(rootPath).exists())
    {
        QMessageBox::critical(this,"下载失败",
                              QString("目标磁盘 %1 不存在！\n\n当前设置的保存路径为:%2\n"
                                      "请插入磁盘或点击'保存路径设置'修改下载目录。")
                                    .arg(rootPath).arg(m_localSavePath));
        return;
    }

    if(!saveDir.exists())
    {
        if(!saveDir.mkpath("."))
        {
            QMessageBox::critical(this, "下载失败",
                                  QString("无法在 %1 创建文件夹！\n\n可能原因：\n"
                                          "1. 磁盘空间不足。\n2. 权限不足。\n3. 磁盘被写保护。\n\n"
                                          "请点击'保存路径设置'修改下载目录。")
                                      .arg(m_localSavePath));
            return;
        }
    }

    //构建完整的保存路径
    QString savePath = saveDir.filePath(fileName);

    //如果文件已存在，尝试删除（覆盖）
    if(QFile::exists(savePath))
    {
        if(!QFile::remove(savePath))
        {
            QMessageBox::critical(this,"文件占用",
                                  QString("文件 \"%1\" 已存在且无法覆盖。\n可能原因：文件正在被其他程序打开。\n请关闭相关程序后重试。").arg(fileName));
            return;
        }
    }

    //调用下载请求函数
    requestFileDownload(m_currentTargetIp,m_currentTargetPort,fileName,m_currentPath,savePath);
}

// ==================== 文件下载请求 ====================
/**
 * @brief 请求下载文件
 * @param targetIp 教师端 IP
 * @param targetTcpPort 教师端 TCP 端口
 * @param fileName 文件名
 * @param path 文件所在路径（相对于共享根目录）
 * @param savePath 本地保存路径
 *
 * 建立 TCP 连接，发送 DOWNLOAD|path|fileName 命令，接收文件元数据后接收文件数据并保存。
 */
void client::requestFileDownload(const QString& targetIp,quint16 targetTcpPort,const QString& fileName,const QString& path,const QString &savePath)
{
    /**
     * @brief 下载上下文结构体
     *
     * 用于保存下载过程中的状态，包括套接字、文件对象、接收状态等。
     * 在 lambda 中捕获此对象的指针，确保下载过程中数据不会丢失。
     */
    struct DownloadContext{
        QTcpSocket *socket = nullptr;//TCP套接字
        QFile* file = nullptr;//本地文件对象
        enum State{ WaitingHeader,WaitingMeta,ReceivingFile} state = WaitingHeader;//接收状态
        quint32 metaLen = 0;//元数据长度
        qint64 totalFileSize = 0;//文件总大小
        qint64 receivedBytes = 0;//已接收字节数
        QString savePath;//保存路径
        bool finished = false;//是否已完成(防止重复处理)

        //析构函数：关闭文件、删除文件对象、删除套接字
        ~DownloadContext()
        {
            if(file)
            {
                if(file->isOpen()) file->close();
                delete file;
            }
            if(socket)
            {
                socket->deleteLater();
            }
        }
    };

    //创建下载上下文对象（在堆上分配,由lambda管理生命周期）
    DownloadContext *ctx = new DownloadContext();
    ctx->savePath = savePath;
    ctx->socket = new QTcpSocket();//创建新的TCP套接字
    ctx->file = new QFile(savePath);//打开本地文件

    //确保保存路径的目录存在
    QFileInfo fileInfo(savePath);
    if(!fileInfo.dir().exists())
    {
        fileInfo.dir().mkpath(".");
    }

    //尝试以写入模式打开本地文件
    if(!ctx->file->open(QIODevice::WriteOnly))
    {
        QString errorMsg = ctx->file->errorString();
        if(errorMsg.isEmpty()) errorMsg = "未知错误(可能是权限不足或路径无效)";

        QMessageBox::critical(nullptr,"保存文件失败",
                              "无法在本地创建/写入文件：" + savePath + "\n错误详情：" + errorMsg +
                                  "\n\n建议：\n1. 检查保存路径设置是否正确。\n2. 检查是否有杀毒软件拦截。\n3. 尝试以管理员身份运行程序。");
        delete ctx;//释放上下文
        return;
    }

    //-----------成功建立连接，发送下载命令------------
    connect(ctx->socket,&QTcpSocket::connected,[ctx,path,fileName](){
        //构建命令字符串：DOWNLOAD|路径|文件名
        QString cmdStr = "DOWNLOAD|" + path + "|" + fileName;
        QByteArray cmd = cmdStr.toUtf8();
        QByteArray block;
        QDataStream out(&block,QIODevice::WriteOnly);
        out<<(quint32)cmd.size();//长度头
        out.writeRawData(cmd.constData(),cmd.size());//命令数据
        ctx->socket->write(block);
    });

    //---------------接收数据（元数据和文件内容）------------
    connect(ctx->socket,&QTcpSocket::readyRead,[this,ctx](){
        if(ctx->finished) return;//已完成，不再处理

        //循环处理，直到没有数据或完成下载
        while(ctx->socket->bytesAvailable() > 0 && !ctx->finished)
        {
            //状态：等待元数据长度头
            if(ctx->state == DownloadContext::WaitingHeader)
            {
                if(ctx->socket->bytesAvailable() >= 4)
                {
                    QByteArray h = ctx->socket->read(4);
                    QDataStream ds(&h,QIODevice::ReadOnly);
                    ds >> ctx->metaLen;//读取元数据长度
                    ctx->state = DownloadContext::WaitingMeta;
                }
                else
                {
                    break;//数据不足，等待下一次readyRead
                }
            }

            //状态：等待元数据（JSON）
            if(ctx->state == DownloadContext::WaitingMeta)
            {
                if(ctx->socket->bytesAvailable() >= ctx->metaLen)
                {
                    QByteArray metaBytes = ctx->socket->read(ctx->metaLen);
                    QJsonDocument metaDoc = QJsonDocument::fromJson(metaBytes);

                    //检查元数据中的状态是否为"ok"
                    if(metaDoc.object()["status"].toString() != "ok")
                    {
                        ctx->finished = true;
                        QString serverMsg = metaDoc.object()["msg"].toString();
                        if(serverMsg.isEmpty()) serverMsg = "服务器端拒绝请求或文件不存在";

                        //关闭并删除本地文件（此时下载失败了）
                        if(ctx->file)
                        {
                            ctx->file->close();
                            QFile::remove(ctx->file->fileName());
                        }

                        QMessageBox::warning(nullptr,"下载失败",serverMsg);
                        delete ctx;//释放上下文
                        return;
                    }

                    //获取文件总大小
                    ctx->totalFileSize = metaDoc.object()["size"].toVariant().toLongLong();
                    ctx->state = DownloadContext::ReceivingFile;
                    ctx->receivedBytes = 0;
                }
                else
                {
                    break;
                }
            }

            //状态：接收文件数据
            if(ctx->state == DownloadContext::ReceivingFile)
            {
                QByteArray chunk = ctx->socket->readAll();//读取当前所有可用数据
                if(chunk.isEmpty()) break;

                ctx->file->write(chunk);    //写入本地文件
                ctx->receivedBytes += chunk.size();//更新已接收字节数

                //如果已接收完所有数据，则完成下载
                if(ctx->receivedBytes >= ctx->totalFileSize)
                {
                    ctx->file->flush();//确保所有数据写入磁盘
                    ctx->file->close();
                    ctx->finished = true;

                    //延迟弹出成功对话框，避免阻塞事件循环
                    QTimer::singleShot(100,this,[this,savePath = ctx->savePath]{
                        QMessageBox::information(nullptr,"下载成功","文件已成功保存至:\n" + savePath);
                    });

                    delete ctx;//释放上下文
                    return;
                }
            }
        }
    });

    //---------错误处理---------
    connect(ctx->socket,&QTcpSocket::errorOccurred,[ctx](QAbstractSocket::SocketError){
        if(ctx->finished) return;
        qDebug() << "[下载] 错误："<<ctx->socket->errorString();
        //根据当前状态决定提示内容
        if(ctx->state == DownloadContext::ReceivingFile && ctx->receivedBytes > 0 && ctx->receivedBytes < ctx->totalFileSize)
        {
            QMessageBox::warning(nullptr,"下载中断","网络连接错误:\n" + ctx->socket->errorString());
        }
        else if(ctx->state == DownloadContext::WaitingHeader || ctx->state == DownloadContext::WaitingMeta)
        {
            QMessageBox::warning(nullptr,"连接失败","无法连接到文件服务器:\n" + ctx->socket->errorString());
        }
        delete ctx;
    });

    //-----------连接断开处理-------------
    connect(ctx->socket,&QTcpSocket::disconnected,[ctx](){
        if(ctx->finished)
        {
            return; //已经正常完成，忽略断开
        }

        //如果已经接收完所有数据
        if(ctx->state == DownloadContext::ReceivingFile && ctx->receivedBytes >= ctx->totalFileSize)
        {
            ctx->file->flush();
            ctx->file->close();
            ctx->finished = true;

            QTimer::singleShot(100,nullptr,[savePath = ctx->savePath](){
                QMessageBox::information(nullptr,"下载成功","文件已成功保存至:\n" + savePath);
            });
            delete ctx;
            return;
        }

        //未完成就断开，提示错误
        if(ctx->state == DownloadContext::WaitingHeader || ctx->state == DownloadContext::WaitingMeta)
        {
            if(!ctx->finished)
            {
                QMessageBox::warning(nullptr,"下载异常","服务器提前断开连接，文件可能不存在或格式错误。");
            }
        }
        else
        {
            QMessageBox::warning(nullptr,"下载中断","连接意外断开,文件可能不完整。");
        }
        delete ctx;
    });

    //-----------设置下载超时定时器-----------
    QTimer* dlTimeout = new QTimer();
    dlTimeout->setSingleShot(true);

    //超时处理：10秒内未完成，则中断连接并提示
    connect(dlTimeout,&QTimer::timeout,[ctx,dlTimeout](){
        if(ctx->finished) return;
        //如果套接字还在连接中或已连接，则中断
        if(ctx->socket->state() == QAbstractSocket::ConnectingState ||
            ctx->socket->state() == QAbstractSocket::ConnectedState)
        {
            ctx->socket->abort();
            QMessageBox::warning(nullptr,"超时","下载请求超时，请检查网络或服务器状态。");
            delete ctx;
        }
        dlTimeout->deleteLater();//不再需要定时器
    });
    dlTimeout->start(10000);//10秒超时

    //发起连接
    ctx->socket->connectToHost(targetIp,targetTcpPort);
}
