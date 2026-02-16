#ifndef YOLO_DETECTOR_H
#define YOLO_DETECTOR_H

#include <onnxruntime_cxx_api.h>
#include <QImage>
#include <string>
#include <vector>
#include <map>
#include <memory>

struct DetectionResult {
    int classId;
    float confidence;
    float x;      // normalized [0,1] top-left x
    float y;      // normalized [0,1] top-left y
    float width;  // normalized [0,1]
    float height; // normalized [0,1]
};

struct YoloModelMetadata {
    std::map<int, std::string> classNames;
    std::string task;
    int stride = 32;
    int imgszH = 640;
    int imgszW = 640;
    bool endToEnd = false;
    std::string author;
    std::string description;
};

enum class YoloVersion {
    Unknown,
    V5,   // output shape [B, N, C+5] — YOLOv5
    V8,   // output shape [B, C+4, N] — YOLOv8
    V11,  // output shape [B, C+4, N] — YOLO11
    V12,  // output shape [B, C+4, N] — YOLO12
    V26   // output shape [B, C+4, N] — YOLOv26
    // Note: end-to-end (NMS baked in) is tracked via YoloModelMetadata::endToEnd,
    // not as a separate version. End-to-end output shape: [1, maxDet, 6].
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
    bool isEndToEnd() const;
    int getNumClasses() const;
    int getInputWidth() const;
    int getInputHeight() const;
    YoloVersion getVersion() const;
    const YoloModelMetadata& getMetadata() const;
    const std::map<int, std::string>& getClassNames() const;

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
    YoloVersion detectVersionFromMetadata(YoloVersion fallback);

    void readMetadata();
    static std::map<int, std::string> parsePythonDictStr(const std::string& str);
    static std::pair<int, int> parseImgsz(const std::string& str);

    std::vector<DetectionResult> postprocessEndToEnd(
        const float* outputData,
        int numDetections,
        float confThreshold,
        float scaleX, float scaleY,
        float padX, float padY,
        int imgWidth, int imgHeight
    );

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
    YoloModelMetadata m_metadata;
};

#endif // YOLO_DETECTOR_H
