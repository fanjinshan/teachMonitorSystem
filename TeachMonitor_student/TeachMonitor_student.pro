QT += core gui network multimedia multimediawidgets
LIBS += -lpsapi
QT += widgets
SOURCES += \
    client_core.cpp \
    client_file.cpp \
    client_p2p.cpp \
    client_ui.cpp \
    main.cpp

HEADERS += \
    client.h
