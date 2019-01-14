TEMPLATE = app
CONFIG += console c++1z
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        src/addon.cpp \
        uWebSockets/uSockets/src/eventing/epoll.c \
        uWebSockets/uSockets/src/context.c \
        uWebSockets/uSockets/src/socket.c \
        uWebSockets/uSockets/src/eventing/libuv.c \
        uWebSockets/uSockets/src/ssl.c \
        uWebSockets/uSockets/src/loop.c

INCLUDEPATH += targets/node-v11.1.0/include/node
INCLUDEPATH += uWebSockets/src
INCLUDEPATH += uWebSockets/uSockets/src

HEADERS += \
    src/AppWrapper.h \
    src/HttpRequestWrapper.h \
    src/HttpResponseWrapper.h \
    src/Utilities.h \
    src/WebSocketWrapper.h
