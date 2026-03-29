/****************************************************************************
** Meta object code from reading C++ file 'fileserver.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../fileserver.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'fileserver.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_FileServer_t {
    QByteArrayData data[19];
    char stringdata0[217];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_FileServer_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_FileServer_t qt_meta_stringdata_FileServer = {
    {
QT_MOC_LITERAL(0, 0, 10), // "FileServer"
QT_MOC_LITERAL(1, 11, 15), // "clientConnected"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 7), // "qintptr"
QT_MOC_LITERAL(4, 36, 16), // "socketDescriptor"
QT_MOC_LITERAL(5, 53, 18), // "clientDisconnected"
QT_MOC_LITERAL(6, 72, 10), // "logMessage"
QT_MOC_LITERAL(7, 83, 7), // "message"
QT_MOC_LITERAL(8, 91, 20), // "studentStatusUpdated"
QT_MOC_LITERAL(9, 112, 9), // "studentId"
QT_MOC_LITERAL(10, 122, 13), // "StudentStatus"
QT_MOC_LITERAL(11, 136, 6), // "status"
QT_MOC_LITERAL(12, 143, 7), // "appName"
QT_MOC_LITERAL(13, 151, 10), // "screenshot"
QT_MOC_LITERAL(14, 162, 20), // "onClientDisconnected"
QT_MOC_LITERAL(15, 183, 22), // "onStudentStatusChanged"
QT_MOC_LITERAL(16, 206, 2), // "id"
QT_MOC_LITERAL(17, 209, 3), // "app"
QT_MOC_LITERAL(18, 213, 3) // "img"

    },
    "FileServer\0clientConnected\0\0qintptr\0"
    "socketDescriptor\0clientDisconnected\0"
    "logMessage\0message\0studentStatusUpdated\0"
    "studentId\0StudentStatus\0status\0appName\0"
    "screenshot\0onClientDisconnected\0"
    "onStudentStatusChanged\0id\0app\0img"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_FileServer[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   44,    2, 0x06 /* Public */,
       5,    1,   47,    2, 0x06 /* Public */,
       6,    1,   50,    2, 0x06 /* Public */,
       8,    4,   53,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      14,    0,   62,    2, 0x08 /* Private */,
      15,    4,   63,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 10, QMetaType::QString, QMetaType::QByteArray,    9,   11,   12,   13,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 10, QMetaType::QString, QMetaType::QByteArray,   16,   11,   17,   18,

       0        // eod
};

void FileServer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<FileServer *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->clientConnected((*reinterpret_cast< qintptr(*)>(_a[1]))); break;
        case 1: _t->clientDisconnected((*reinterpret_cast< qintptr(*)>(_a[1]))); break;
        case 2: _t->logMessage((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->studentStatusUpdated((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< StudentStatus(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QByteArray(*)>(_a[4]))); break;
        case 4: _t->onClientDisconnected(); break;
        case 5: _t->onStudentStatusChanged((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< StudentStatus(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QByteArray(*)>(_a[4]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (FileServer::*)(qintptr );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FileServer::clientConnected)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (FileServer::*)(qintptr );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FileServer::clientDisconnected)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (FileServer::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FileServer::logMessage)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (FileServer::*)(const QString & , StudentStatus , const QString & , const QByteArray & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FileServer::studentStatusUpdated)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject FileServer::staticMetaObject = { {
    QMetaObject::SuperData::link<QTcpServer::staticMetaObject>(),
    qt_meta_stringdata_FileServer.data,
    qt_meta_data_FileServer,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *FileServer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FileServer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_FileServer.stringdata0))
        return static_cast<void*>(this);
    return QTcpServer::qt_metacast(_clname);
}

int FileServer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QTcpServer::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void FileServer::clientConnected(qintptr _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void FileServer::clientDisconnected(qintptr _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void FileServer::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void FileServer::studentStatusUpdated(const QString & _t1, StudentStatus _t2, const QString & _t3, const QByteArray & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
