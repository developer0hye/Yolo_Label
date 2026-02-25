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

# --- ONNX Runtime integration (optional) ---
isEmpty(ONNXRUNTIME_DIR): ONNXRUNTIME_DIR = $$PWD/onnxruntime
exists($$ONNXRUNTIME_DIR/include) {
    DEFINES += ONNXRUNTIME_AVAILABLE
    # Support both flat layout (include/onnxruntime_cxx_api.h)
    # and Homebrew layout (include/onnxruntime/onnxruntime_cxx_api.h)
    INCLUDEPATH += $$ONNXRUNTIME_DIR/include
    exists($$ONNXRUNTIME_DIR/include/onnxruntime) {
        INCLUDEPATH += $$ONNXRUNTIME_DIR/include/onnxruntime
    }
    LIBS += -L$$ONNXRUNTIME_DIR/lib -lonnxruntime
    unix: QMAKE_RPATHDIR += $$ONNXRUNTIME_DIR/lib
    SOURCES += yolo_detector.cpp
    HEADERS += yolo_detector.h
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
