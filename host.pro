TEMPLATE = app
CONFIG += console c++1z
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        src/host.cpp \
        src/addon.cpp \
        uWebSockets/uSockets/src/eventing/epoll.c \
        uWebSockets/uSockets/src/context.c \
        uWebSockets/uSockets/src/socket.c \
        uWebSockets/uSockets/src/eventing/libuv.c \
        uWebSockets/uSockets/src/ssl.c \
        uWebSockets/uSockets/src/loop.c


#Separate the Node.js addon from host compilation
QMAKE_CXXFLAGS += -DADDON_IS_HOST

INCLUDEPATH += /home/alexhultman/v8/v8/include
INCLUDEPATH += uWebSockets/src
INCLUDEPATH += uWebSockets/uSockets/src

# We can link statically when I figure out how to compile V8 properly
LIBS += /home/alexhultman/v8/v8/out/x64.release/libv8_libplatform.so
LIBS += /home/alexhultman/v8/v8/out/x64.release/libv8_libbase.so
LIBS += /home/alexhultman/v8/v8/out/x64.release/libv8.so
LIBS += /home/alexhultman/v8/v8/out/x64.release/libicui18n.so
LIBS += /home/alexhultman/v8/v8/out/x64.release/libicuuc.so

LIBS += -lssl -lcrypto
