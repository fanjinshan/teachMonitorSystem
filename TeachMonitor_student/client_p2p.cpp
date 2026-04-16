#include "client.h"

// ==================== P2P 连接处理 ====================
/**
 * @brief 当有新的 TCP 连接到达时的槽函数
 *
 * 此函数由 m_fileServer 的 newConnection 信号触发。
 * 每当教师端或其他客户端连接到本机的 TCP 端口时，会创建新的 QTcpSocket，
 * 并连接其 readyRead 信号，用于处理来自教师端的命令。
 *
 * 当前场景下，此 TCP 服务器主要用于接收教师端发送的：
 * - GET_SCREENSHOT_NOW：立即截图请求
 */
void client::onNewPeerConnection()
{
    //获取下一个待处理的TCP连接
    QTcpSocket *clientSocket = m_fileServer->nextPendingConnection();

    //连接套接字的readyread信号，处理接收到的数据
    connect(clientSocket,&QTcpSocket::readyRead,this,[this,clientSocket]{
        //检查接收到的数据是否足够容纳一个4字节的长度头
        if(clientSocket->bytesAvailable() >= (int)sizeof(quint32))
        {
            //读取长度头（4字节）
            QByteArray header = clientSocket->read(4);
            QDataStream ds(&header,QIODevice::ReadOnly);
            quint32 len;
            ds >> len;//获取命令数据的长度

            //检查是否已接收到完整的命令数据
            if(clientSocket->bytesAvailable() >= len)
            {
                //读取命令数据
                QByteArray cmdData = clientSocket->read(len);
                //调用命令处理函数
                handlePeerCommand(cmdData,clientSocket);
            }
        }
    });
    //连接断开信号，当连接关闭时，自动删除套接字对象，释放资源
    connect(clientSocket,&QTcpSocket::disconnected,clientSocket,&QTcpSocket::deleteLater);
}

// ==================== 处理来自教师端的命令 ====================
/**
 * @brief 处理来自教师端的 P2P 命令
 * @param data 命令数据（不含长度前缀）
 * @param socket 对应的 TCP 套接字（用于发送响应）
 *
 * 支持的命令：
 * - GET_SCREENSHOT_NOW : 教师端请求立即截图（实时查看）
 */
void client::handlePeerCommand(const QByteArray &data,QTcpSocket *socket)
{
    //将命令数据转换为UTF-8字符串
    QString cmd = QString::fromUtf8(data);

    //处理老师端发出的"立即截图"请求
    if(cmd == "GET_SCREENSHOT_NOW")
    {
        qDebug() << "[P2P命令]收到教师端获取屏幕请求。";

        //传入"Live_Request_Response"作为应用名，标记为实时响应
        //教师端收到后会识别为Live_Request_Response,从而关闭加载框并弹窗
        captureAndSendScreenshot(false,"Live_Request_Response");
        //命令处理完毕，截图会通过新连接发送
        socket->disconnectFromHost();
        return;
    }

    if(cmd.startsWith("GET_CAMERA_STREAM|"))
    {
        QStringList parts = cmd.split('|');
        if(parts.size() >= 3)
        {
            QString teacherIp = parts[1];
            quint16 teacherPort = parts[2].toUShort();
            qDebug() << "[P2P命令] 收到教师摄像头请求，目标端口：" <<teacherPort;
            startCameraStream(teacherIp,teacherPort);
        }
        socket->disconnectFromHost();
        return;
    }
}
