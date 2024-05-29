#-------------------------------------------------
#
# Project created by QtCreator 2011-09-17T07:41:27
#
#-------------------------------------------------

QT       += core gui network

TARGET = EthernetFlasher
TEMPLATE = app

LIBS += -lqscintilla2

SOURCES += main.cpp\
        mw.cpp \
    buildsettings.cpp \
    workthread.cpp

HEADERS  += mw.h \
    buildsettings.h \
    workthread.h

FORMS    += mw.ui \
    buildsettings.ui
