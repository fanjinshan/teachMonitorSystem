/****************************************************************************
** Meta object code from reading C++ file 'client.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../client.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'client.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_client_t {
    QByteArrayData data[64];
    char stringdata0[963];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_client_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_client_t qt_meta_stringdata_client = {
    {
QT_MOC_LITERAL(0, 0, 6), // "client"
QT_MOC_LITERAL(1, 7, 20), // "studentStatusChanged"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 9), // "studentId"
QT_MOC_LITERAL(4, 39, 13), // "StudentStatus"
QT_MOC_LITERAL(5, 53, 6), // "status"
QT_MOC_LITERAL(6, 60, 7), // "appName"
QT_MOC_LITERAL(7, 68, 10), // "screenshot"
QT_MOC_LITERAL(8, 79, 11), // "onConnected"
QT_MOC_LITERAL(9, 91, 11), // "onReadyRead"
QT_MOC_LITERAL(10, 103, 14), // "onDisconnected"
QT_MOC_LITERAL(11, 118, 14), // "onUdpReadyRead"
QT_MOC_LITERAL(12, 133, 13), // "sendHeartbeat"
QT_MOC_LITERAL(13, 147, 19), // "onNewPeerConnection"
QT_MOC_LITERAL(14, 167, 19), // "checkFirewallStatus"
QT_MOC_LITERAL(15, 187, 29), // "onFriendListItemDoubleClicked"
QT_MOC_LITERAL(16, 217, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(17, 234, 4), // "item"
QT_MOC_LITERAL(18, 239, 16), // "onRefreshClicked"
QT_MOC_LITERAL(19, 256, 27), // "onFileListItemDoubleClicked"
QT_MOC_LITERAL(20, 284, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(21, 301, 6), // "column"
QT_MOC_LITERAL(22, 308, 21), // "onFileListItemClicked"
QT_MOC_LITERAL(23, 330, 17), // "onDownloadClicked"
QT_MOC_LITERAL(24, 348, 13), // "onQuitClicked"
QT_MOC_LITERAL(25, 362, 22), // "onManualConnectClicked"
QT_MOC_LITERAL(26, 385, 21), // "onSaveNicknameClicked"
QT_MOC_LITERAL(27, 407, 21), // "onChangeAvatarClicked"
QT_MOC_LITERAL(28, 429, 18), // "showFriendListPage"
QT_MOC_LITERAL(29, 448, 17), // "showFileSharePage"
QT_MOC_LITERAL(30, 466, 26), // "startNetworkInitialization"
QT_MOC_LITERAL(31, 493, 20), // "showNetworkErrorInUI"
QT_MOC_LITERAL(32, 514, 11), // "errorDetail"
QT_MOC_LITERAL(33, 526, 24), // "onRetryConnectionClicked"
QT_MOC_LITERAL(34, 551, 23), // "onCheckBlacklistTimeout"
QT_MOC_LITERAL(35, 575, 18), // "onJoinClassClicked"
QT_MOC_LITERAL(36, 594, 6), // "initUi"
QT_MOC_LITERAL(37, 601, 15), // "requestFileList"
QT_MOC_LITERAL(38, 617, 8), // "targetIp"
QT_MOC_LITERAL(39, 626, 13), // "targetTcpPort"
QT_MOC_LITERAL(40, 640, 4), // "path"
QT_MOC_LITERAL(41, 645, 19), // "requestFileDownload"
QT_MOC_LITERAL(42, 665, 8), // "fileName"
QT_MOC_LITERAL(43, 674, 8), // "savePath"
QT_MOC_LITERAL(44, 683, 17), // "handlePeerCommand"
QT_MOC_LITERAL(45, 701, 4), // "data"
QT_MOC_LITERAL(46, 706, 11), // "QTcpSocket*"
QT_MOC_LITERAL(47, 718, 6), // "socket"
QT_MOC_LITERAL(48, 725, 16), // "tryDirectConnect"
QT_MOC_LITERAL(49, 742, 9), // "teacherIp"
QT_MOC_LITERAL(50, 752, 14), // "teacherTcpPort"
QT_MOC_LITERAL(51, 767, 17), // "startUdpDiscovery"
QT_MOC_LITERAL(52, 785, 14), // "updateUserList"
QT_MOC_LITERAL(53, 800, 8), // "UserInfo"
QT_MOC_LITERAL(54, 809, 4), // "user"
QT_MOC_LITERAL(55, 814, 10), // "removeUser"
QT_MOC_LITERAL(56, 825, 9), // "uniqueKey"
QT_MOC_LITERAL(57, 835, 21), // "updateUserListFromMap"
QT_MOC_LITERAL(58, 857, 19), // "showFirewallWarning"
QT_MOC_LITERAL(59, 877, 23), // "getCurrentForegroundApp"
QT_MOC_LITERAL(60, 901, 24), // "captureAndSendScreenshot"
QT_MOC_LITERAL(61, 926, 10), // "isPeriodic"
QT_MOC_LITERAL(62, 937, 15), // "violatedAppName"
QT_MOC_LITERAL(63, 953, 9) // "countOnly"

    },
    "client\0studentStatusChanged\0\0studentId\0"
    "StudentStatus\0status\0appName\0screenshot\0"
    "onConnected\0onReadyRead\0onDisconnected\0"
    "onUdpReadyRead\0sendHeartbeat\0"
    "onNewPeerConnection\0checkFirewallStatus\0"
    "onFriendListItemDoubleClicked\0"
    "QListWidgetItem*\0item\0onRefreshClicked\0"
    "onFileListItemDoubleClicked\0"
    "QTreeWidgetItem*\0column\0onFileListItemClicked\0"
    "onDownloadClicked\0onQuitClicked\0"
    "onManualConnectClicked\0onSaveNicknameClicked\0"
    "onChangeAvatarClicked\0showFriendListPage\0"
    "showFileSharePage\0startNetworkInitialization\0"
    "showNetworkErrorInUI\0errorDetail\0"
    "onRetryConnectionClicked\0"
    "onCheckBlacklistTimeout\0onJoinClassClicked\0"
    "initUi\0requestFileList\0targetIp\0"
    "targetTcpPort\0path\0requestFileDownload\0"
    "fileName\0savePath\0handlePeerCommand\0"
    "data\0QTcpSocket*\0socket\0tryDirectConnect\0"
    "teacherIp\0teacherTcpPort\0startUdpDiscovery\0"
    "updateUserList\0UserInfo\0user\0removeUser\0"
    "uniqueKey\0updateUserListFromMap\0"
    "showFirewallWarning\0getCurrentForegroundApp\0"
    "captureAndSendScreenshot\0isPeriodic\0"
    "violatedAppName\0countOnly"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_client[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      38,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    4,  204,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       8,    0,  213,    2, 0x08 /* Private */,
       9,    0,  214,    2, 0x08 /* Private */,
      10,    0,  215,    2, 0x08 /* Private */,
      11,    0,  216,    2, 0x08 /* Private */,
      12,    0,  217,    2, 0x08 /* Private */,
      13,    0,  218,    2, 0x08 /* Private */,
      14,    0,  219,    2, 0x08 /* Private */,
      15,    1,  220,    2, 0x08 /* Private */,
      18,    0,  223,    2, 0x08 /* Private */,
      19,    2,  224,    2, 0x08 /* Private */,
      22,    2,  229,    2, 0x08 /* Private */,
      23,    0,  234,    2, 0x08 /* Private */,
      24,    0,  235,    2, 0x08 /* Private */,
      25,    0,  236,    2, 0x08 /* Private */,
      26,    0,  237,    2, 0x08 /* Private */,
      27,    0,  238,    2, 0x08 /* Private */,
      28,    0,  239,    2, 0x08 /* Private */,
      29,    0,  240,    2, 0x08 /* Private */,
      30,    0,  241,    2, 0x08 /* Private */,
      31,    1,  242,    2, 0x08 /* Private */,
      33,    0,  245,    2, 0x08 /* Private */,
      34,    0,  246,    2, 0x08 /* Private */,
      35,    0,  247,    2, 0x08 /* Private */,
      36,    0,  248,    2, 0x08 /* Private */,
      37,    3,  249,    2, 0x08 /* Private */,
      41,    5,  256,    2, 0x08 /* Private */,
      44,    2,  267,    2, 0x08 /* Private */,
      48,    2,  272,    2, 0x08 /* Private */,
      51,    0,  277,    2, 0x08 /* Private */,
      52,    1,  278,    2, 0x08 /* Private */,
      55,    1,  281,    2, 0x08 /* Private */,
      57,    0,  284,    2, 0x08 /* Private */,
      58,    0,  285,    2, 0x08 /* Private */,
      59,    0,  286,    2, 0x08 /* Private */,
      60,    3,  287,    2, 0x08 /* Private */,
      60,    2,  294,    2, 0x28 /* Private | MethodCloned */,
      60,    1,  299,    2, 0x28 /* Private | MethodCloned */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString, 0x80000000 | 4, QMetaType::QString, QMetaType::QByteArray,    3,    5,    6,    7,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 16,   17,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 20, QMetaType::Int,   17,   21,
    QMetaType::Void, 0x80000000 | 20, QMetaType::Int,   17,   21,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   32,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::UShort, QMetaType::QString,   38,   39,   40,
    QMetaType::Void, QMetaType::QString, QMetaType::UShort, QMetaType::QString, QMetaType::QString, QMetaType::QString,   38,   39,   42,   40,   43,
    QMetaType::Void, QMetaType::QByteArray, 0x80000000 | 46,   45,   47,
    QMetaType::Void, QMetaType::QString, QMetaType::UShort,   49,   50,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 53,   54,
    QMetaType::Void, QMetaType::QString,   56,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::QString,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString, QMetaType::Bool,   61,   62,   63,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   61,   62,
    QMetaType::Void, QMetaType::Bool,   61,

       0        // eod
};

void client::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<client *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->studentStatusChanged((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< StudentStatus(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QByteArray(*)>(_a[4]))); break;
        case 1: _t->onConnected(); break;
        case 2: _t->onReadyRead(); break;
        case 3: _t->onDisconnected(); break;
        case 4: _t->onUdpReadyRead(); break;
        case 5: _t->sendHeartbeat(); break;
        case 6: _t->onNewPeerConnection(); break;
        case 7: _t->checkFirewallStatus(); break;
        case 8: _t->onFriendListItemDoubleClicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 9: _t->onRefreshClicked(); break;
        case 10: _t->onFileListItemDoubleClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 11: _t->onFileListItemClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 12: _t->onDownloadClicked(); break;
        case 13: _t->onQuitClicked(); break;
        case 14: _t->onManualConnectClicked(); break;
        case 15: _t->onSaveNicknameClicked(); break;
        case 16: _t->onChangeAvatarClicked(); break;
        case 17: _t->showFriendListPage(); break;
        case 18: _t->showFileSharePage(); break;
        case 19: _t->startNetworkInitialization(); break;
        case 20: _t->showNetworkErrorInUI((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 21: _t->onRetryConnectionClicked(); break;
        case 22: _t->onCheckBlacklistTimeout(); break;
        case 23: _t->onJoinClassClicked(); break;
        case 24: _t->initUi(); break;
        case 25: _t->requestFileList((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< quint16(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 26: _t->requestFileDownload((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< quint16(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4])),(*reinterpret_cast< const QString(*)>(_a[5]))); break;
        case 27: _t->handlePeerCommand((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< QTcpSocket*(*)>(_a[2]))); break;
        case 28: _t->tryDirectConnect((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< quint16(*)>(_a[2]))); break;
        case 29: _t->startUdpDiscovery(); break;
        case 30: _t->updateUserList((*reinterpret_cast< const UserInfo(*)>(_a[1]))); break;
        case 31: _t->removeUser((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 32: _t->updateUserListFromMap(); break;
        case 33: _t->showFirewallWarning(); break;
        case 34: { QString _r = _t->getCurrentForegroundApp();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 35: _t->captureAndSendScreenshot((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 36: _t->captureAndSendScreenshot((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 37: _t->captureAndSendScreenshot((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 27:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QTcpSocket* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (client::*)(const QString & , StudentStatus , const QString & , const QByteArray & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&client::studentStatusChanged)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject client::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_client.data,
    qt_meta_data_client,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *client::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *client::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_client.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int client::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 38)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 38;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 38)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 38;
    }
    return _id;
}

// SIGNAL 0
void client::studentStatusChanged(const QString & _t1, StudentStatus _t2, const QString & _t3, const QByteArray & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
