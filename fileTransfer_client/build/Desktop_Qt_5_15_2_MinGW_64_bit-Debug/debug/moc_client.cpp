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
    QByteArrayData data[19];
    char stringdata0[295];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_client_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_client_t qt_meta_stringdata_client = {
    {
QT_MOC_LITERAL(0, 0, 6), // "client"
QT_MOC_LITERAL(1, 7, 18), // "on_loginBt_clicked"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 11), // "onConnected"
QT_MOC_LITERAL(4, 39, 11), // "onReadyRead"
QT_MOC_LITERAL(5, 51, 15), // "onErrorOccurred"
QT_MOC_LITERAL(6, 67, 28), // "QAbstractSocket::SocketError"
QT_MOC_LITERAL(7, 96, 29), // "on_fileList_itemDoubleClicked"
QT_MOC_LITERAL(8, 126, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(9, 143, 4), // "item"
QT_MOC_LITERAL(10, 148, 6), // "column"
QT_MOC_LITERAL(11, 155, 21), // "on_downloadBt_clicked"
QT_MOC_LITERAL(12, 177, 14), // "onDisconnected"
QT_MOC_LITERAL(13, 192, 14), // "onBytesWritten"
QT_MOC_LITERAL(14, 207, 5), // "bytes"
QT_MOC_LITERAL(15, 213, 21), // "on_downLoadBt_clicked"
QT_MOC_LITERAL(16, 235, 20), // "on_settingBt_clicked"
QT_MOC_LITERAL(17, 256, 20), // "on_refreshBt_clicked"
QT_MOC_LITERAL(18, 277, 17) // "on_quitBt_clicked"

    },
    "client\0on_loginBt_clicked\0\0onConnected\0"
    "onReadyRead\0onErrorOccurred\0"
    "QAbstractSocket::SocketError\0"
    "on_fileList_itemDoubleClicked\0"
    "QTreeWidgetItem*\0item\0column\0"
    "on_downloadBt_clicked\0onDisconnected\0"
    "onBytesWritten\0bytes\0on_downLoadBt_clicked\0"
    "on_settingBt_clicked\0on_refreshBt_clicked\0"
    "on_quitBt_clicked"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_client[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   74,    2, 0x08 /* Private */,
       3,    0,   75,    2, 0x08 /* Private */,
       4,    0,   76,    2, 0x08 /* Private */,
       5,    1,   77,    2, 0x08 /* Private */,
       7,    2,   80,    2, 0x08 /* Private */,
      11,    0,   85,    2, 0x08 /* Private */,
      12,    0,   86,    2, 0x08 /* Private */,
      13,    1,   87,    2, 0x08 /* Private */,
      15,    0,   90,    2, 0x08 /* Private */,
      16,    0,   91,    2, 0x08 /* Private */,
      17,    0,   92,    2, 0x08 /* Private */,
      18,    0,   93,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 6,    2,
    QMetaType::Void, 0x80000000 | 8, QMetaType::Int,    9,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::LongLong,   14,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void client::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<client *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->on_loginBt_clicked(); break;
        case 1: _t->onConnected(); break;
        case 2: _t->onReadyRead(); break;
        case 3: _t->onErrorOccurred((*reinterpret_cast< QAbstractSocket::SocketError(*)>(_a[1]))); break;
        case 4: _t->on_fileList_itemDoubleClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 5: _t->on_downloadBt_clicked(); break;
        case 6: _t->onDisconnected(); break;
        case 7: _t->onBytesWritten((*reinterpret_cast< qint64(*)>(_a[1]))); break;
        case 8: _t->on_downLoadBt_clicked(); break;
        case 9: _t->on_settingBt_clicked(); break;
        case 10: _t->on_refreshBt_clicked(); break;
        case 11: _t->on_quitBt_clicked(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAbstractSocket::SocketError >(); break;
            }
            break;
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
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
