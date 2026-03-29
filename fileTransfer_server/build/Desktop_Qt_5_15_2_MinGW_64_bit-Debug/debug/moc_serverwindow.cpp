/****************************************************************************
** Meta object code from reading C++ file 'serverwindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../serverwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'serverwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ClickableLabel_t {
    QByteArrayData data[3];
    char stringdata0[24];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ClickableLabel_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ClickableLabel_t qt_meta_stringdata_ClickableLabel = {
    {
QT_MOC_LITERAL(0, 0, 14), // "ClickableLabel"
QT_MOC_LITERAL(1, 15, 7), // "clicked"
QT_MOC_LITERAL(2, 23, 0) // ""

    },
    "ClickableLabel\0clicked\0"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ClickableLabel[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   19,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void,

       0        // eod
};

void ClickableLabel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ClickableLabel *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->clicked(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ClickableLabel::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ClickableLabel::clicked)) {
                *result = 0;
                return;
            }
        }
    }
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject ClickableLabel::staticMetaObject = { {
    QMetaObject::SuperData::link<QLabel::staticMetaObject>(),
    qt_meta_stringdata_ClickableLabel.data,
    qt_meta_data_ClickableLabel,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ClickableLabel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ClickableLabel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ClickableLabel.stringdata0))
        return static_cast<void*>(this);
    return QLabel::qt_metacast(_clname);
}

int ClickableLabel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QLabel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void ClickableLabel::clicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
struct qt_meta_stringdata_ServerWindow_t {
    QByteArrayData data[30];
    char stringdata0[424];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ServerWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ServerWindow_t qt_meta_stringdata_ServerWindow = {
    {
QT_MOC_LITERAL(0, 0, 12), // "ServerWindow"
QT_MOC_LITERAL(1, 13, 19), // "onNavMonitorClicked"
QT_MOC_LITERAL(2, 33, 0), // ""
QT_MOC_LITERAL(3, 34, 16), // "onNavFileClicked"
QT_MOC_LITERAL(4, 51, 20), // "onNavSettingsClicked"
QT_MOC_LITERAL(5, 72, 12), // "onLogMessage"
QT_MOC_LITERAL(6, 85, 3), // "msg"
QT_MOC_LITERAL(7, 89, 22), // "onStudentStatusUpdated"
QT_MOC_LITERAL(8, 112, 2), // "id"
QT_MOC_LITERAL(9, 115, 13), // "StudentStatus"
QT_MOC_LITERAL(10, 129, 6), // "status"
QT_MOC_LITERAL(11, 136, 7), // "appName"
QT_MOC_LITERAL(12, 144, 10), // "screenshot"
QT_MOC_LITERAL(13, 155, 24), // "onTableCellDoubleClicked"
QT_MOC_LITERAL(14, 180, 3), // "row"
QT_MOC_LITERAL(15, 184, 3), // "col"
QT_MOC_LITERAL(16, 188, 30), // "onRequestLiveScreenshotClicked"
QT_MOC_LITERAL(17, 219, 9), // "studentId"
QT_MOC_LITERAL(18, 229, 14), // "onUdpReadyRead"
QT_MOC_LITERAL(19, 244, 19), // "sendServerHeartbeat"
QT_MOC_LITERAL(20, 264, 17), // "broadcastUserList"
QT_MOC_LITERAL(21, 282, 19), // "onNewFileConnection"
QT_MOC_LITERAL(22, 302, 17), // "handlePeerCommand"
QT_MOC_LITERAL(23, 320, 4), // "data"
QT_MOC_LITERAL(24, 325, 11), // "QTcpSocket*"
QT_MOC_LITERAL(25, 337, 6), // "socket"
QT_MOC_LITERAL(26, 344, 20), // "onCreateClassClicked"
QT_MOC_LITERAL(27, 365, 25), // "onViewClassMembersClicked"
QT_MOC_LITERAL(28, 391, 22), // "onClassComboBoxChanged"
QT_MOC_LITERAL(29, 414, 9) // "className"

    },
    "ServerWindow\0onNavMonitorClicked\0\0"
    "onNavFileClicked\0onNavSettingsClicked\0"
    "onLogMessage\0msg\0onStudentStatusUpdated\0"
    "id\0StudentStatus\0status\0appName\0"
    "screenshot\0onTableCellDoubleClicked\0"
    "row\0col\0onRequestLiveScreenshotClicked\0"
    "studentId\0onUdpReadyRead\0sendServerHeartbeat\0"
    "broadcastUserList\0onNewFileConnection\0"
    "handlePeerCommand\0data\0QTcpSocket*\0"
    "socket\0onCreateClassClicked\0"
    "onViewClassMembersClicked\0"
    "onClassComboBoxChanged\0className"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ServerWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   89,    2, 0x08 /* Private */,
       3,    0,   90,    2, 0x08 /* Private */,
       4,    0,   91,    2, 0x08 /* Private */,
       5,    1,   92,    2, 0x08 /* Private */,
       7,    4,   95,    2, 0x08 /* Private */,
      13,    2,  104,    2, 0x08 /* Private */,
      16,    1,  109,    2, 0x08 /* Private */,
      18,    0,  112,    2, 0x08 /* Private */,
      19,    0,  113,    2, 0x08 /* Private */,
      20,    0,  114,    2, 0x08 /* Private */,
      21,    0,  115,    2, 0x08 /* Private */,
      22,    2,  116,    2, 0x08 /* Private */,
      26,    0,  121,    2, 0x08 /* Private */,
      27,    0,  122,    2, 0x08 /* Private */,
      28,    1,  123,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    6,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 9, QMetaType::QString, QMetaType::QByteArray,    8,   10,   11,   12,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   14,   15,
    QMetaType::Void, QMetaType::QString,   17,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QByteArray, 0x80000000 | 24,   23,   25,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   29,

       0        // eod
};

void ServerWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ServerWindow *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->onNavMonitorClicked(); break;
        case 1: _t->onNavFileClicked(); break;
        case 2: _t->onNavSettingsClicked(); break;
        case 3: _t->onLogMessage((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 4: _t->onStudentStatusUpdated((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< StudentStatus(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QByteArray(*)>(_a[4]))); break;
        case 5: _t->onTableCellDoubleClicked((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 6: _t->onRequestLiveScreenshotClicked((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 7: _t->onUdpReadyRead(); break;
        case 8: _t->sendServerHeartbeat(); break;
        case 9: _t->broadcastUserList(); break;
        case 10: _t->onNewFileConnection(); break;
        case 11: _t->handlePeerCommand((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< QTcpSocket*(*)>(_a[2]))); break;
        case 12: _t->onCreateClassClicked(); break;
        case 13: _t->onViewClassMembersClicked(); break;
        case 14: _t->onClassComboBoxChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 11:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 1:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QTcpSocket* >(); break;
            }
            break;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ServerWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_ServerWindow.data,
    qt_meta_data_ServerWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ServerWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ServerWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ServerWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int ServerWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
