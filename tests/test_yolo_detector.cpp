#include <QtTest>
#include "yolo_detector.h"

class TestYoloDetector : public QObject
{
    Q_OBJECT

private slots:
    // ── parsePythonDictStr ───────────────────────────────────────
    void parseDict_basic()
    {
        auto result = YoloDetector::parsePythonDictStr("{0: 'person', 1: 'car', 2: 'dog'}");
        QCOMPARE(result.size(), size_t(3));
        QCOMPARE(result[0], std::string("person"));
        QCOMPARE(result[1], std::string("car"));
        QCOMPARE(result[2], std::string("dog"));
    }
    void parseDict_emptyDict()
    {
        auto result = YoloDetector::parsePythonDictStr("{}");
        QCOMPARE(result.size(), size_t(0));
    }
    void parseDict_singleEntry()
    {
        auto result = YoloDetector::parsePythonDictStr("{0: 'cat'}");
        QCOMPARE(result.size(), size_t(1));
        QCOMPARE(result[0], std::string("cat"));
    }
    void parseDict_doubleQuotes()
    {
        auto result = YoloDetector::parsePythonDictStr("{0: \"person\", 1: \"car\"}");
        QCOMPARE(result.size(), size_t(2));
        QCOMPARE(result[0], std::string("person"));
        QCOMPARE(result[1], std::string("car"));
    }
    void parseDict_noBraces()
    {
        auto result = YoloDetector::parsePythonDictStr("no braces here");
        QCOMPARE(result.size(), size_t(0));
    }
    void parseDict_withSpaces()
    {
        auto result = YoloDetector::parsePythonDictStr("{  0 :  'person' ,  1 :  'car'  }");
        QCOMPARE(result.size(), size_t(2));
        QCOMPARE(result[0], std::string("person"));
        QCOMPARE(result[1], std::string("car"));
    }
    void parseDict_largeClassCount()
    {
        // COCO has 80 classes — verify high indices work
        auto result = YoloDetector::parsePythonDictStr("{79: 'toothbrush'}");
        QCOMPARE(result.size(), size_t(1));
        QCOMPARE(result[79], std::string("toothbrush"));
    }
    void parseDict_emptyString()
    {
        auto result = YoloDetector::parsePythonDictStr("");
        QCOMPARE(result.size(), size_t(0));
    }

    // ── parseImgsz ───────────────────────────────────────────────
    void parseImgsz_arrayFormat()
    {
        auto result = YoloDetector::parseImgsz("[640, 640]");
        QCOMPARE(result.first, 640);
        QCOMPARE(result.second, 640);
    }
    void parseImgsz_rectangularInput()
    {
        auto result = YoloDetector::parseImgsz("[480, 640]");
        QCOMPARE(result.first, 480);
        QCOMPARE(result.second, 640);
    }
    void parseImgsz_singleNumber()
    {
        auto result = YoloDetector::parseImgsz("640");
        QCOMPARE(result.first, 640);
        QCOMPARE(result.second, 640);
    }
    void parseImgsz_nonNumericFallback()
    {
        auto result = YoloDetector::parseImgsz("abc");
        QCOMPARE(result.first, 640);
        QCOMPARE(result.second, 640);
    }
    void parseImgsz_emptyString()
    {
        auto result = YoloDetector::parseImgsz("");
        QCOMPARE(result.first, 640);
        QCOMPARE(result.second, 640);
    }
    void parseImgsz_withSpaces()
    {
        auto result = YoloDetector::parseImgsz("[ 320 , 320 ]");
        QCOMPARE(result.first, 320);
        QCOMPARE(result.second, 320);
    }

    // ── iou ──────────────────────────────────────────────────────
    void iou_perfectOverlap()
    {
        DetectionResult a{0, 0.9f, 0.1f, 0.1f, 0.4f, 0.4f};
        DetectionResult b{0, 0.8f, 0.1f, 0.1f, 0.4f, 0.4f};
        QCOMPARE(YoloDetector::iou(a, b), 1.0f);
    }
    void iou_noOverlap()
    {
        DetectionResult a{0, 0.9f, 0.0f, 0.0f, 0.2f, 0.2f};
        DetectionResult b{0, 0.8f, 0.5f, 0.5f, 0.2f, 0.2f};
        QCOMPARE(YoloDetector::iou(a, b), 0.0f);
    }
    void iou_partialOverlap()
    {
        // a: [0.0, 0.0] to [0.4, 0.4], area = 0.16
        // b: [0.2, 0.2] to [0.6, 0.6], area = 0.16
        // intersection: [0.2, 0.2] to [0.4, 0.4], area = 0.04
        // union: 0.16 + 0.16 - 0.04 = 0.28
        // IoU = 0.04 / 0.28 = 1/7
        DetectionResult a{0, 0.9f, 0.0f, 0.0f, 0.4f, 0.4f};
        DetectionResult b{0, 0.8f, 0.2f, 0.2f, 0.4f, 0.4f};
        float expected = 0.04f / 0.28f;
        QVERIFY(qAbs(YoloDetector::iou(a, b) - expected) < 1e-5f);
    }
    void iou_zeroArea()
    {
        DetectionResult a{0, 0.9f, 0.5f, 0.5f, 0.0f, 0.0f};
        DetectionResult b{0, 0.8f, 0.5f, 0.5f, 0.3f, 0.3f};
        QCOMPARE(YoloDetector::iou(a, b), 0.0f);
    }
    void iou_adjacentBoxes()
    {
        // Boxes touching at edge — no overlap
        DetectionResult a{0, 0.9f, 0.0f, 0.0f, 0.5f, 0.5f};
        DetectionResult b{0, 0.8f, 0.5f, 0.0f, 0.5f, 0.5f};
        QCOMPARE(YoloDetector::iou(a, b), 0.0f);
    }
    void iou_containedBox()
    {
        // b is fully inside a
        // a: [0.0, 0.0] to [1.0, 1.0], area = 1.0
        // b: [0.2, 0.2] to [0.4, 0.4], area = 0.04
        // intersection = 0.04, union = 1.0
        // IoU = 0.04 / 1.0 = 0.04
        DetectionResult a{0, 0.9f, 0.0f, 0.0f, 1.0f, 1.0f};
        DetectionResult b{0, 0.8f, 0.2f, 0.2f, 0.2f, 0.2f};
        float expected = 0.04f / 1.0f;
        QVERIFY(qAbs(YoloDetector::iou(a, b) - expected) < 1e-5f);
    }

    // ── nms ──────────────────────────────────────────────────────
    void nms_emptyInput()
    {
        auto keep = YoloDetector::nms({}, 0.45f);
        QCOMPARE(keep.size(), size_t(0));
    }
    void nms_noSuppression()
    {
        // Two boxes with no overlap — both kept
        std::vector<DetectionResult> boxes = {
            {0, 0.9f, 0.0f, 0.0f, 0.2f, 0.2f},
            {0, 0.8f, 0.5f, 0.5f, 0.2f, 0.2f}
        };
        auto keep = YoloDetector::nms(boxes, 0.45f);
        QCOMPARE(keep.size(), size_t(2));
    }
    void nms_fullSuppression()
    {
        // Two near-identical boxes, same class — lower confidence suppressed
        std::vector<DetectionResult> boxes = {
            {0, 0.9f, 0.1f, 0.1f, 0.4f, 0.4f},
            {0, 0.7f, 0.1f, 0.1f, 0.4f, 0.4f}
        };
        auto keep = YoloDetector::nms(boxes, 0.45f);
        QCOMPARE(keep.size(), size_t(1));
        QCOMPARE(keep[0], 0); // highest confidence kept
    }
    void nms_classAware()
    {
        // Two identical boxes but different classes — both kept
        std::vector<DetectionResult> boxes = {
            {0, 0.9f, 0.1f, 0.1f, 0.4f, 0.4f},
            {1, 0.8f, 0.1f, 0.1f, 0.4f, 0.4f}
        };
        auto keep = YoloDetector::nms(boxes, 0.45f);
        QCOMPARE(keep.size(), size_t(2));
    }
    void nms_confidenceOrdering()
    {
        // Lower-confidence box first in input, highest should still be first in output
        std::vector<DetectionResult> boxes = {
            {0, 0.3f, 0.0f, 0.0f, 0.2f, 0.2f},
            {0, 0.9f, 0.5f, 0.5f, 0.2f, 0.2f},
            {0, 0.6f, 0.8f, 0.8f, 0.1f, 0.1f}
        };
        auto keep = YoloDetector::nms(boxes, 0.45f);
        QCOMPARE(keep.size(), size_t(3));
        // Sorted by confidence: index 1 (0.9), index 2 (0.6), index 0 (0.3)
        QCOMPARE(keep[0], 1);
        QCOMPARE(keep[1], 2);
        QCOMPARE(keep[2], 0);
    }
    void nms_singleBox()
    {
        std::vector<DetectionResult> boxes = {
            {0, 0.9f, 0.5f, 0.5f, 0.2f, 0.2f}
        };
        auto keep = YoloDetector::nms(boxes, 0.45f);
        QCOMPARE(keep.size(), size_t(1));
        QCOMPARE(keep[0], 0);
    }
    void nms_partialOverlapBelowThreshold()
    {
        // Two boxes with small overlap (IoU < 0.45) — both kept
        std::vector<DetectionResult> boxes = {
            {0, 0.9f, 0.0f, 0.0f, 0.3f, 0.3f},
            {0, 0.8f, 0.2f, 0.2f, 0.3f, 0.3f}
        };
        // intersection: [0.2,0.2] to [0.3,0.3] = 0.01
        // union: 0.09 + 0.09 - 0.01 = 0.17
        // IoU = 0.01/0.17 ≈ 0.059 < 0.45
        auto keep = YoloDetector::nms(boxes, 0.45f);
        QCOMPARE(keep.size(), size_t(2));
    }
};

QTEST_GUILESS_MAIN(TestYoloDetector)
#include "test_yolo_detector.moc"
