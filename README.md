# 教室监控管理系统 (Classroom Monitoring System)

## 📖 项目简介

本系统是一套基于 **Qt (C++)** 开发的局域网教室监控与管理解决方案，采用 **C/S (Client/Server)** 架构。系统旨在帮助教师实时掌握课堂动态、管理教学文件分发，并对学生端的违规应用使用进行智能检测与记录。

### ✨ 核心功能

#### 👨‍🏫 教师端 (Teacher Client)
- **实时监控大屏**：以表格形式展示所有在线学生的状态（在线/离线/违规）、IP、班级及违规应用名称。
- **远程屏幕查看**：支持双击学生行或点击按钮，实时获取学生端屏幕截图。
- **远程摄像头查看**：支持请求学生端摄像头画面，实时预览课堂情况。
- **智能违规检测日志**：自动接收并记录学生端的违规应用上报（含截图证据），支持按学生查询历史违规记录。
- **班级管理**：支持创建、删除班级，并将学生分配至特定班级；支持按班级或在线状态筛选监控列表。
- **文件共享服务**：内置 TCP 文件服务器，响应学生端的文件列表请求与下载请求，自动统计文件下载次数与来源 IP。
- **数据持久化**：使用 SQLite 数据库存储学生信息、班级结构、违规日志及文件统计数据。

#### 👨‍🎓 学生端 (Student Client)
- **自动发现与连接**：通过 UDP 广播/单播机制自动发现局域网内的教师端，建立心跳连接。
- **应用行为监控**：后台实时监测前台运行进程，基于**黑名单机制**自动识别违规应用（如游戏、短视频、社交软件等）。
- **智能上报机制**：
    - **违规上报**：检测到违规应用时，自动截取屏幕并上报教师端，同时包含冷却期逻辑防止频繁刷屏。
    - **定期巡查**：每 5 分钟自动发送一次屏幕截图供教师端存档。
    - **状态恢复**：当用户退出违规应用后，自动发送恢复信号。
- **文件接收与管理**：浏览教师端共享文件夹，支持多级目录导航、文件下载、断点续传（基础版）及本地保存路径设置。
- **个人信息管理**：支持修改昵称、更换头像、加入指定班级。
- **P2P 通信**：通过 TCP 直接响应教师端的即时截图请求和摄像头流请求。

---

### 核心功能截图
#### 学生端共享文件夹界面
<img width="1202" height="682" alt="学生端共享文件夹" src="https://github.com/user-attachments/assets/7f640796-4e7f-4ec4-bec7-40f7a1ce88b0" />
#### 教师端主界面
<img width="1202" height="832" alt="教师端主界面" src="https://github.com/user-attachments/assets/47b34eaf-7f83-47e0-9874-9cf0160aa444" />
#### 违规应用检测
<img width="1202" height="832" alt="违规应用检测" src="https://github.com/user-attachments/assets/073d64bf-a9e8-4686-990d-3cb0a65d222f" />
#### 教师端查看学生端实时屏幕截图
<img width="1304" height="832" alt="截取屏幕" src="https://github.com/user-attachments/assets/db7e884b-24d8-4788-9b04-da224d76c861" />
#### 教师端查看学生端实时摄像头影像
<img width="1920" height="1080" alt="实时摄像头" src="https://github.com/user-attachments/assets/31653b98-6871-48c1-af48-95e85bf36b1d" />


## 🛠️ 技术栈

- **开发语言**: C++17
- **GUI 框架**: Qt 5.15+ (Widgets)
- **网络通信**:
    - `QUdpSocket`:用于设备发现、心跳保活、广播用户列表。
    - `QTcpServer` / `QTcpSocket`: 用于文件传输、监控数据上报、实时屏幕/视频流传输。
- **数据库**: SQLite (`QSqlDatabase`)
- **多媒体**: `QCamera`, `QVideoProbe` (摄像头采集), `QScreen` (屏幕截图)
- **构建工具**: CMake / qmake

---

## 📂 项目结构

```text
TeachMonitor/
├── TeachMonitor_student/       # 学生端项目
│   ├── main.cpp                # 入口，初始化 QApplication
│   ├── client.h/cpp            # 主窗口类，整合 UI、网络、监控逻辑
│   ├── client_ui.cpp           # UI 界面构建（导航栏、文件树、设置页）
│   ├── client_core.cpp         # 核心逻辑（心跳、UDP发现、黑名单检测、截图上报）
│   ├── client_p2p.cpp          # P2P 命令处理（响应教师端的截图/摄像头请求）
│   └── client_file.cpp         # 文件传输逻辑（请求列表、下载文件）
│
├── TeachMonitor_teacher/       # 教师端项目
│   ├── main.cpp                # 入口，先显示登录框，再启动主窗口
│   ├── logindialog.h/cpp       # 教师登录验证界面
│   ├── serverwindow.h/cpp      # 主窗口类，整合监控、文件、设置页面
│   └── databasemanager.h/cpp   # 数据库单例管理器 (SQLite 操作封装)
│
└── README.md                   # 本文档
```

---

## 🚀 快速开始

### 1. 环境要求
- **操作系统**: Windows 10/11 (目前黑名单检测和摄像头功能主要针对 Windows API优化)
- **编译器**: MSVC 2019/2022 或 MinGW 64-bit
- **Qt 版本**: Qt 5.15.2 或更高版本 (需包含 `QtNetwork`, `QtSql`, `QtMultimedia` 模块)

### 2. 编译与运行

#### 教师端 (Server)
1. 打开 `TeachMonitor_teacher/TeachMonitor_teacher.pro` 。
2. 构建项目。
3. 运行程序，输入默认账号密码登录：
   - **账号**: `fan`
   - **密码**: `123`
4. 启动后，系统会自动初始化 SQLite 数据库 (`server_data.db`) 并监听 UDP 端口 **9999** 和随机 TCP 端口。

#### 学生端 (Client)
1. 打开 `TeachMonitor_student/TeachMonitor_student.pro` 。
2. 构建项目。
3. 运行程序。
4. 学生端会自动绑定 UDP 端口 **8889**，并广播心跳包。若在同一局域网内检测到教师端，将自动上线。

### 3. 网络配置注意
- 确保教师端和学生端处于**同一局域网网段**。
- 若防火墙拦截，请放行以下端口：
    - **UDP**: 9999 (教师接收心跳), 8889 (学生接收心跳)
    - **TCP**: 20000-29999 (动态范围，用于文件和监控数据传输)

---

## ⚙️ 功能详解

### 🔒 违规应用监控机制 (学生端)
系统通过 [client_core.cpp](file://e:\QT\qtCode\TeachMonitor_student\client_core.cpp) 中的 [onCheckBlacklistTimeout()](file://e:\QT\qtCode\TeachMonitor_student\client.h#L241-L241) 每秒检查一次前台窗口。
- **黑名单匹配**: 对比进程名和窗口标题与 `BLACKLIST_APPS` 列表（见 [client.h](file://e:\QT\qtCode\TeachMonitor_student\client.h)）。
- **白名单豁免**: 浏览器、IDE、办公软件等在白名单中，即使标题包含敏感词也可能被豁免（具体逻辑见代码）。
- **冷却期**: 违规上报后有 10 秒冷却期，避免状态抖动导致频繁上报。
- **隐私保护**: 仅上报违规时的截图和进程名，非违规时段不上传屏幕内容（除非开启定期巡查）。

### 📹 实时视频流
- 教师端点击“查看摄像头”时，通过 UDP 发送 `GET_CAMERA_STREAM` 指令。
- 学生端收到指令后，启动 `QCamera`，通过 `QVideoProbe` 捕获帧，压缩为 JPEG 并通过 TCP 流式发送给教师端。
- 教师端接收数据流并实时刷新 QLabel 显示。

### 📁 文件共享协议
采用自定义的应用层协议，基于 TCP：
1. **长度头 + 数据体**: 所有命令和数据包前 4 字节为 `quint32` 长度头。
2. **命令格式**:
   - `LIST|path`: 请求文件列表，返回 JSON 数组。
   - `DOWNLOAD|path|filename`: 请求下载，先返回元数据 JSON，再发送二进制文件流。
   - `MONITOR_START|jsonSize|imgSize`: 监控数据包头，随后分别发送 JSON 元数据和图片二进制数据。

### 💾 数据库设计 (SQLite)
- **students**: 存储学生 ID、姓名、IP、在线状态、所属班级。
- **classes**: 存储班级名称、创建时间。
- **warning_logs**: 存储违规记录，包括学生 ID、应用名、时间戳、**截图 BLOB 数据**。
- **file_stats**: 统计文件下载次数、最后下载者 IP。

---

## 📄 许可证

本项目仅供学习与内部交流使用。

---

## 👥 贡献者

- **Developer**: XUPT Student 
- **Tech Stack**: Qt, C++, SQLite
