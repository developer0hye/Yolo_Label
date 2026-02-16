#include "yolo_detector.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>
#include <thread>

YoloDetector::YoloDetector()
    : m_env(ORT_LOGGING_LEVEL_WARNING, "YoloLabel")
    , m_inputWidth(0)
    , m_inputHeight(0)
    , m_numClasses(0)
    , m_version(YoloVersion::Unknown)
    , m_loaded(false)
{
}

YoloDetector::~YoloDetector() = default;

bool YoloDetector::loadModel(const std::string& modelPath, std::string& errorMsg)
{
    try {
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(
            static_cast<int>(std::thread::hardware_concurrency()));
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        std::wstring wpath(modelPath.begin(), modelPath.end());
        m_session = std::make_unique<Ort::Session>(m_env, wpath.c_str(), sessionOptions);
#else
        m_session = std::make_unique<Ort::Session>(m_env, modelPath.c_str(), sessionOptions);
#endif

        // Read input info
        auto inputInfo = m_session->GetInputTypeInfo(0);
        auto inputTensorInfo = inputInfo.GetTensorTypeAndShapeInfo();
        m_inputShape = inputTensorInfo.GetShape();

        if (m_inputShape.size() != 4) {
            errorMsg = "Expected 4D input tensor [B, C, H, W], got " +
                       std::to_string(m_inputShape.size()) + "D";
            m_session.reset();
            return false;
        }

        m_inputHeight = static_cast<int>(m_inputShape[2]);
        m_inputWidth = static_cast<int>(m_inputShape[3]);

        // Handle dynamic shapes (-1)
        if (m_inputHeight <= 0) m_inputHeight = 640;
        if (m_inputWidth <= 0) m_inputWidth = 640;

        // Read output info
        auto outputInfo = m_session->GetOutputTypeInfo(0);
        auto outputTensorInfo = outputInfo.GetTensorTypeAndShapeInfo();
        m_outputShape = outputTensorInfo.GetShape();

        // Read input/output names
        m_inputNameStrings.clear();
        m_outputNameStrings.clear();

        for (size_t i = 0; i < m_session->GetInputCount(); ++i) {
            auto name = m_session->GetInputNameAllocated(i, m_allocator);
            m_inputNameStrings.push_back(name.get());
        }

        for (size_t i = 0; i < m_session->GetOutputCount(); ++i) {
            auto name = m_session->GetOutputNameAllocated(i, m_allocator);
            m_outputNameStrings.push_back(name.get());
        }

        // Detect YOLO version
        m_version = detectVersion();
        if (m_version == YoloVersion::Unknown) {
            errorMsg = "Cannot detect YOLO version from output shape [";
            for (size_t i = 0; i < m_outputShape.size(); ++i) {
                if (i > 0) errorMsg += ", ";
                errorMsg += std::to_string(m_outputShape[i]);
            }
            errorMsg += "]. Expected Ultralytics YOLOv5 or YOLOv8/11/12/26.";
            m_session.reset();
            return false;
        }

        m_loaded = true;
        return true;

    } catch (const Ort::Exception& e) {
        errorMsg = std::string("ONNX Runtime error: ") + e.what();
        m_session.reset();
        m_loaded = false;
        return false;
    }
}

YoloVersion YoloDetector::detectVersion()
{
    if (m_outputShape.size() != 3) return YoloVersion::Unknown;

    int64_t dim1 = m_outputShape[1];
    int64_t dim2 = m_outputShape[2];

    // Handle dynamic shapes: assume v8 by default (most common)
    if (dim1 <= 0 || dim2 <= 0) {
        m_numClasses = 80; // default COCO
        return YoloVersion::V8;
    }

    // V5: [B, N, C+5] where N (e.g., 25200) >> C+5 (e.g., 85)
    // V8: [B, C+4, N] where N (e.g., 8400) >> C+4 (e.g., 84)
    if (dim1 > dim2) {
        // V5 format: dim1=N (large), dim2=C+5 (small)
        m_numClasses = static_cast<int>(dim2 - 5);
        return (m_numClasses > 0) ? YoloVersion::V5 : YoloVersion::Unknown;
    } else {
        // V8 format: dim1=C+4 (small), dim2=N (large)
        m_numClasses = static_cast<int>(dim1 - 4);
        return (m_numClasses > 0) ? YoloVersion::V8 : YoloVersion::Unknown;
    }
}

std::vector<float> YoloDetector::preprocess(
    const QImage& image,
    float& scaleX, float& scaleY,
    float& padX, float& padY)
{
    int imgW = image.width();
    int imgH = image.height();

    // Letterbox: scale to fit while maintaining aspect ratio
    float scale = std::min(
        static_cast<float>(m_inputWidth) / imgW,
        static_cast<float>(m_inputHeight) / imgH);

    int newW = static_cast<int>(std::round(imgW * scale));
    int newH = static_cast<int>(std::round(imgH * scale));

    padX = (m_inputWidth - newW) / 2.0f;
    padY = (m_inputHeight - newH) / 2.0f;
    scaleX = scale;
    scaleY = scale;

    // Resize image using Qt
    QImage resized = image.scaled(newW, newH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    resized = resized.convertToFormat(QImage::Format_RGB888);

    // Allocate CHW buffer filled with padding value (114/255)
    int totalSize = 3 * m_inputHeight * m_inputWidth;
    std::vector<float> blob(totalSize, 114.0f / 255.0f);

    int padLeft = static_cast<int>(padX);
    int padTop = static_cast<int>(padY);

    // Copy resized image into padded buffer (HWC to CHW)
    for (int y = 0; y < newH; ++y) {
        const uchar* row = resized.constScanLine(y);
        for (int x = 0; x < newW; ++x) {
            int srcIdx = y * resized.bytesPerLine() + x * 3;
            int dstX = padLeft + x;
            int dstY = padTop + y;

            blob[0 * m_inputHeight * m_inputWidth + dstY * m_inputWidth + dstX] = row[srcIdx + 0] / 255.0f; // R
            blob[1 * m_inputHeight * m_inputWidth + dstY * m_inputWidth + dstX] = row[srcIdx + 1] / 255.0f; // G
            blob[2 * m_inputHeight * m_inputWidth + dstY * m_inputWidth + dstX] = row[srcIdx + 2] / 255.0f; // B
        }
    }

    return blob;
}

std::vector<DetectionResult> YoloDetector::detect(
    const QImage& image,
    float confThreshold,
    float nmsIouThreshold)
{
    if (!m_loaded || image.isNull()) return {};

    int imgW = image.width();
    int imgH = image.height();

    float scaleX, scaleY, padX, padY;
    std::vector<float> inputTensor = preprocess(image, scaleX, scaleY, padX, padY);

    // Create input tensor
    std::vector<int64_t> inputShape = {1, 3, m_inputHeight, m_inputWidth};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputOrtTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, inputTensor.data(), inputTensor.size(),
        inputShape.data(), inputShape.size());

    // Build name arrays
    std::vector<const char*> inputNames, outputNames;
    for (const auto& s : m_inputNameStrings) inputNames.push_back(s.c_str());
    for (const auto& s : m_outputNameStrings) outputNames.push_back(s.c_str());

    // Run inference
    std::vector<Ort::Value> outputs;
    try {
        outputs = m_session->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(), &inputOrtTensor, 1,
            outputNames.data(), outputNames.size());
    } catch (const Ort::Exception& e) {
        return {};
    }

    const float* outputData = outputs[0].GetTensorData<float>();
    auto outputShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();

    int64_t dim1 = outputShape[1];
    int64_t dim2 = outputShape[2];

    std::vector<DetectionResult> results;

    if (m_version == YoloVersion::V5) {
        results = postprocessV5(outputData,
            static_cast<int>(dim1), static_cast<int>(dim2 - 5),
            confThreshold, scaleX, scaleY, padX, padY, imgW, imgH);
    } else {
        results = postprocessV8(outputData,
            static_cast<int>(dim1 - 4), static_cast<int>(dim2),
            confThreshold, scaleX, scaleY, padX, padY, imgW, imgH);
    }

    // Apply NMS
    auto keepIndices = nms(results, nmsIouThreshold);
    std::vector<DetectionResult> finalResults;
    finalResults.reserve(keepIndices.size());
    for (int idx : keepIndices) {
        finalResults.push_back(results[idx]);
    }

    return finalResults;
}

std::vector<DetectionResult> YoloDetector::postprocessV5(
    const float* outputData,
    int numDetections,
    int numClasses,
    float confThreshold,
    float scaleX, float scaleY,
    float padX, float padY,
    int imgWidth, int imgHeight)
{
    // V5 output: [B, N, C+5] where each row = [cx, cy, w, h, obj_conf, cls0, cls1, ...]
    std::vector<DetectionResult> results;
    int stride = numClasses + 5;

    for (int i = 0; i < numDetections; ++i) {
        const float* row = outputData + i * stride;
        float objConf = row[4];

        if (objConf < confThreshold) continue;

        // Find best class
        int bestClass = 0;
        float bestScore = row[5];
        for (int c = 1; c < numClasses; ++c) {
            if (row[5 + c] > bestScore) {
                bestScore = row[5 + c];
                bestClass = c;
            }
        }

        float confidence = objConf * bestScore;
        if (confidence < confThreshold) continue;

        // Convert from letterbox coords to original image normalized coords
        float cx = row[0];
        float cy = row[1];
        float w = row[2];
        float h = row[3];

        // Remove padding and scale
        float x1 = (cx - w / 2.0f - padX) / scaleX;
        float y1 = (cy - h / 2.0f - padY) / scaleY;
        float bw = w / scaleX;
        float bh = h / scaleY;

        // Normalize to [0, 1]
        DetectionResult det;
        det.classId = bestClass;
        det.confidence = confidence;
        det.x = std::max(0.0f, x1 / imgWidth);
        det.y = std::max(0.0f, y1 / imgHeight);
        det.width = std::min(bw / imgWidth, 1.0f - det.x);
        det.height = std::min(bh / imgHeight, 1.0f - det.y);

        if (det.width > 0 && det.height > 0) {
            results.push_back(det);
        }
    }

    return results;
}

std::vector<DetectionResult> YoloDetector::postprocessV8(
    const float* outputData,
    int numClasses,
    int numDetections,
    float confThreshold,
    float scaleX, float scaleY,
    float padX, float padY,
    int imgWidth, int imgHeight)
{
    // V8 output: [B, C+4, N] — transposed layout
    // Row 0: cx, Row 1: cy, Row 2: w, Row 3: h, Rows 4..C+3: class scores
    std::vector<DetectionResult> results;

    for (int i = 0; i < numDetections; ++i) {
        // Find best class score
        int bestClass = 0;
        float bestScore = outputData[4 * numDetections + i];
        for (int c = 1; c < numClasses; ++c) {
            float score = outputData[(4 + c) * numDetections + i];
            if (score > bestScore) {
                bestScore = score;
                bestClass = c;
            }
        }

        if (bestScore < confThreshold) continue;

        float cx = outputData[0 * numDetections + i];
        float cy = outputData[1 * numDetections + i];
        float w  = outputData[2 * numDetections + i];
        float h  = outputData[3 * numDetections + i];

        // Remove padding and scale
        float x1 = (cx - w / 2.0f - padX) / scaleX;
        float y1 = (cy - h / 2.0f - padY) / scaleY;
        float bw = w / scaleX;
        float bh = h / scaleY;

        // Normalize to [0, 1]
        DetectionResult det;
        det.classId = bestClass;
        det.confidence = bestScore;
        det.x = std::max(0.0f, x1 / imgWidth);
        det.y = std::max(0.0f, y1 / imgHeight);
        det.width = std::min(bw / imgWidth, 1.0f - det.x);
        det.height = std::min(bh / imgHeight, 1.0f - det.y);

        if (det.width > 0 && det.height > 0) {
            results.push_back(det);
        }
    }

    return results;
}

std::vector<int> YoloDetector::nms(
    const std::vector<DetectionResult>& boxes,
    float iouThreshold)
{
    if (boxes.empty()) return {};

    // Sort by confidence descending
    std::vector<int> indices(boxes.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&boxes](int a, int b) {
        return boxes[a].confidence > boxes[b].confidence;
    });

    std::vector<bool> suppressed(boxes.size(), false);
    std::vector<int> keep;

    for (int i : indices) {
        if (suppressed[i]) continue;
        keep.push_back(i);

        for (int j : indices) {
            if (suppressed[j] || j == i) continue;
            if (boxes[i].classId != boxes[j].classId) continue;

            if (iou(boxes[i], boxes[j]) > iouThreshold) {
                suppressed[j] = true;
            }
        }
    }

    return keep;
}

float YoloDetector::iou(const DetectionResult& a, const DetectionResult& b)
{
    float ax1 = a.x, ay1 = a.y;
    float ax2 = a.x + a.width, ay2 = a.y + a.height;
    float bx1 = b.x, by1 = b.y;
    float bx2 = b.x + b.width, by2 = b.y + b.height;

    float ix1 = std::max(ax1, bx1);
    float iy1 = std::max(ay1, by1);
    float ix2 = std::min(ax2, bx2);
    float iy2 = std::min(ay2, by2);

    float interArea = std::max(0.0f, ix2 - ix1) * std::max(0.0f, iy2 - iy1);
    float aArea = a.width * a.height;
    float bArea = b.width * b.height;
    float unionArea = aArea + bArea - interArea;

    return (unionArea > 0) ? interArea / unionArea : 0.0f;
}

bool YoloDetector::isLoaded() const { return m_loaded; }
int YoloDetector::getNumClasses() const { return m_numClasses; }
int YoloDetector::getInputWidth() const { return m_inputWidth; }
int YoloDetector::getInputHeight() const { return m_inputHeight; }
YoloVersion YoloDetector::getVersion() const { return m_version; }
