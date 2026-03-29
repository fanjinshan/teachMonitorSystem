#include "client.h"
#include "ui_client.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDataStream>
#include <QFile>
#include <QDebug>


void client::onNewPeerConnection()
{
    QTcpSocket *clientSocket = m_fileServer->nextPendingConnection();
    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        if (clientSocket->bytesAvailable() >= (int)sizeof(quint32)) {
            QByteArray header = clientSocket->read(4);
            QDataStream ds(&header, QIODevice::ReadOnly);
            quint32 len;
            ds >> len;
            
            if (clientSocket->bytesAvailable() >= len) {
                QByteArray cmdData = clientSocket->read(len);
                handlePeerCommand(cmdData, clientSocket);
            }
        }
    });
    connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
}

void client::handlePeerCommand(const QByteArray &data, QTcpSocket *socket)
{
    QString cmd = QString::fromUtf8(data);
    
    // 【新增】处理老师端发出的“立即截图”请求
    if (cmd == "GET_SCREENSHOT_NOW") {
        qDebug() << "[P2P Command] Received GET_SCREENSHOT_NOW from teacher. Capturing immediately...";
        
        // 【修复】传入 "Live_Request_Response" 作为应用名，标记为实时响应
        // 这样教师端收到后会识别为 Live_Request_Response，从而关闭加载框并弹窗
        captureAndSendScreenshot(false, "Live_Request_Response");
        
        // 命令处理完毕，截图会通过新连接发送（或复用当前连接，取决于具体实现，此处按原逻辑断开）
        socket->disconnectFromHost();
        return;
    }

    if (cmd == "GET_FILE_LIST") {
        QDir dir(m_sharedDirPath);
        QJsonArray array;
        QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &fi : list) {
            QJsonObject obj;
            obj["name"] = fi.fileName();
            obj["isDir"] = false;
            obj["size"] = (qint64)fi.size();
            array.append(obj);
        }
        QJsonDocument doc(array);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out << (quint32)jsonData.size();
        out.writeRawData(jsonData.constData(), jsonData.size());
        socket->write(block);
    }
    else if (cmd.startsWith("DOWNLOAD|")) {
        QString fileName = cmd.mid(9);
        QString filePath = m_sharedDirPath + "/" + fileName;
        QFile file(filePath);
        QJsonObject meta;
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            meta["name"] = fileName;
            meta["size"] = (qint64)file.size();
            meta["status"] = "ok";
            QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out << (quint32)metaBytes.size();
            socket->write(block);
            socket->write(metaBytes);
            socket->write(file.readAll());
            file.close();
        } else {
            meta["status"] = "error";
            meta["msg"] = "File not found";
            QByteArray metaBytes = QJsonDocument(meta).toJson(QJsonDocument::Compact);
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out << (quint32)metaBytes.size();
            socket->write(block);
            socket->write(metaBytes);
        }
    }
}