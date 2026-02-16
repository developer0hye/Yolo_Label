#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <onnxruntime_cxx_api.h>
#include <QImage>
#include <string>
#include <vector>
#include <memory>

struct DetectionResult {
    int classId;
    float confidence;
    float x;      // normalized [0,1] top-left x
    float y;      // normalized [0,1] top-left y
    float width;  // normalized [0,1]
    float height; // normalized [0,1]
};

enum class YoloVersion {
    Unknown,
    V5,   // output shape [B, N, C+5] — YOLOv5
    V8    // output shape [B, C+4, N] — YOLOv8/11/12/26
};

class YoloDetector {
public:
    YoloDetector();
    ~YoloDetector();

    bool loadModel(const std::string& modelPath, std::string& errorMsg);

    std::vector<DetectionResult> detect(
        const QImage& image,
        float confThreshold,
        float nmsIouThreshold = 0.45f
    );

    bool isLoaded() const;
    int getNumClasses() const;
    int getInputWidth() const;
    int getInputHeight() const;
    YoloVersion getVersion() const;

private:
    std::vector<float> preprocess(
        const QImage& image,
        float& scaleX, float& scaleY,
        float& padX, float& padY
    );

    std::vector<DetectionResult> postprocessV5(
        const float* outputData,
        int numDetections,
        int numClasses,
        float confThreshold,
        float scaleX, float scaleY,
        float padX, float padY,
        int imgWidth, int imgHeight
    );

    std::vector<DetectionResult> postprocessV8(
        const float* outputData,
        int numClasses,
        int numDetections,
        float confThreshold,
        float scaleX, float scaleY,
        float padX, float padY,
        int imgWidth, int imgHeight
    );

    static std::vector<int> nms(
        const std::vector<DetectionResult>& boxes,
        float iouThreshold
    );

    static float iou(const DetectionResult& a, const DetectionResult& b);

    YoloVersion detectVersion();

    Ort::Env m_env;
    std::unique_ptr<Ort::Session> m_session;
    Ort::AllocatorWithDefaultOptions m_allocator;

    std::vector<std::string> m_inputNameStrings;
    std::vector<std::string> m_outputNameStrings;
    std::vector<int64_t> m_inputShape;
    std::vector<int64_t> m_outputShape;

    int m_inputWidth;
    int m_inputHeight;
    int m_numClasses;
    YoloVersion m_version;
    bool m_loaded;
};

#endif // YOLO_DETECTOR_H
