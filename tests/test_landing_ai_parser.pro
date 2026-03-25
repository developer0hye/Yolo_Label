QT += core network testlib
QT -= gui
CONFIG += c++17 console testcase
CONFIG -= app_bundle
DEFINES += UNIT_TEST
SOURCES += test_landing_ai_parser.cpp ../cloud_labeler.cpp
HEADERS += ../cloud_labeler.h
INCLUDEPATH += ..
