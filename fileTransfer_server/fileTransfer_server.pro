QT += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
HEADERS += \
    clienthandler.h \
    databasemanager.h \
    fileserver.h \
    serverwindow.h

SOURCES += \
    clienthandler.cpp \
    databasemanager.cpp \
    fileserver.cpp \
    main.cpp \
    serverwindow.cpp

FORMS += \
    serverwindow.ui
