QT += core gui
TEMPLATE = app
TARGET = test_inference
CONFIG += c++17 console
CONFIG -= app_bundle
DEFINES += ONNXRUNTIME_AVAILABLE

SOURCES += test_inference.cpp ../yolo_detector.cpp
HEADERS += ../yolo_detector.h

isEmpty(ONNXRUNTIME_DIR): ONNXRUNTIME_DIR = $$PWD/../onnxruntime
INCLUDEPATH += .. $$ONNXRUNTIME_DIR/include
LIBS += -L$$ONNXRUNTIME_DIR/lib -lonnxruntime
unix: QMAKE_RPATHDIR += $$ONNXRUNTIME_DIR/lib
