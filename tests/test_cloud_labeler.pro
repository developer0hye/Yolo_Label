QT += core network testlib
QT -= gui
CONFIG += c++17 console testcase
DEFINES += UNIT_TEST
SOURCES += test_cloud_labeler.cpp ../cloud_labeler.cpp
HEADERS += ../cloud_labeler.h
INCLUDEPATH += ..
