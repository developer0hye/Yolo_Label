#include "yolo_detector.h"
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <string>

static const char* versionToString(YoloVersion v)
{
    switch (v) {
    case YoloVersion::V5:  return "V5";
    case YoloVersion::V8:  return "V8";
    case YoloVersion::V11: return "V11";
    case YoloVersion::V12: return "V12";
    case YoloVersion::V26: return "V26";
    default:               return "Unknown";
    }
}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    if (argc < 4) {
        std::cerr << "Usage: test_inference <model.onnx> <image> <conf_threshold>" << std::endl;
        return 1;
    }

    std::string modelPath = argv[1];
    QString imagePath = QString::fromLocal8Bit(argv[2]);
    float confThreshold = std::stof(argv[3]);

    QImage image(imagePath);
    if (image.isNull()) {
        std::cerr << "Error: cannot load image: " << imagePath.toStdString() << std::endl;
        return 1;
    }

    YoloDetector detector;
    std::string errorMsg;
    if (!detector.loadModel(modelPath, errorMsg)) {
        std::cerr << "Error loading model: " << errorMsg << std::endl;
        return 1;
    }

    auto detections = detector.detect(image, confThreshold);

    // Build JSON output
    QJsonObject root;
    root["model"] = QString::fromStdString(modelPath);
    root["version"] = versionToString(detector.getVersion());
    root["endToEnd"] = detector.isEndToEnd();
    root["numClasses"] = detector.getNumClasses();

    QJsonArray detsArray;
    for (const auto& det : detections) {
        QJsonObject obj;
        obj["classId"] = det.classId;
        obj["confidence"] = static_cast<double>(det.confidence);
        obj["x"] = static_cast<double>(det.x);
        obj["y"] = static_cast<double>(det.y);
        obj["width"] = static_cast<double>(det.width);
        obj["height"] = static_cast<double>(det.height);
        detsArray.append(obj);
    }
    root["detections"] = detsArray;

    QJsonDocument doc(root);
    std::cout << doc.toJson(QJsonDocument::Indented).toStdString();

    return 0;
}
