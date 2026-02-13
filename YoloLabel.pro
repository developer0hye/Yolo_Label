#-------------------------------------------------
#
# Project created by QtCreator 2018-10-23T15:07:41
#
#-------------------------------------------------

QT       += core gui widgets

TARGET = YoloLabel
TEMPLATE = app

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

CONFIG += c++17

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    label_img.cpp

HEADERS += \
        mainwindow.h \
    label_img.h

FORMS += \
        mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
