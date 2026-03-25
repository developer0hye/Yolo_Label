#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include "cloud_labeler.h"

// Helpers to build a minimal detection JSON object
static QJsonObject makeDetection(const QString &label,
                                  double x1, double y1, double x2, double y2)
{
    QJsonArray bb;
    bb << x1 << y1 << x2 << y2;
    QJsonObject obj;
    obj["label"]        = label;
    obj["bounding_box"] = bb;
    return obj;
}

class TestLandingAIParser : public QObject
{
    Q_OBJECT

private slots:
    // ── basic detection ───────────────────────────────────────────────────────
    void singleDetection_exactMatch()
    {
        QJsonArray dets;
        dets << makeDetection("cat", 100, 50, 300, 250);  // 200x200 px box in 400x400 image
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat", "dog"}, 400.0, 400.0);
        QCOMPARE(lines.size(), 1);
        // cx=(100+300)/2/400=0.5, cy=(50+250)/2/400=0.375, nw=200/400=0.5, nh=200/400=0.5
        QCOMPARE(lines[0], QString("0 0.500000 0.375000 0.500000 0.500000"));
    }

    void singleDetection_caseInsensitiveFallback()
    {
        QJsonArray dets;
        dets << makeDetection("CAT", 0, 0, 100, 100);
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat", "dog"}, 200.0, 200.0);
        QCOMPARE(lines.size(), 1);
        QVERIFY(lines[0].startsWith("0 "));
    }

    // ── unknown label ─────────────────────────────────────────────────────────
    void unknownLabel_skipped()
    {
        QJsonArray dets;
        dets << makeDetection("elephant", 0, 0, 50, 50);
        QStringList skipped;
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat", "dog"}, 100.0, 100.0, &skipped);
        QVERIFY(lines.isEmpty());
        QCOMPARE(skipped, QStringList{"elephant"});
    }

    void unknownLabel_notDuplicatedInSkipped()
    {
        QJsonArray dets;
        dets << makeDetection("elephant", 0, 0, 10, 10);
        dets << makeDetection("elephant", 20, 20, 30, 30);
        QStringList skipped;
        CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 100.0, 100.0, &skipped);
        QCOMPARE(skipped.size(), 1);
    }

    void emptyLabel_notAddedToSkipped()
    {
        QJsonArray dets;
        dets << makeDetection("", 0, 0, 10, 10);
        QStringList skipped;
        CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 100.0, 100.0, &skipped);
        QVERIFY(skipped.isEmpty());
    }

    // ── bounding box validation ───────────────────────────────────────────────
    void missingBoundingBox_skipped()
    {
        QJsonObject obj;
        obj["label"] = "cat";
        // no bounding_box key
        QJsonArray dets;
        dets << obj;
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 100.0, 100.0);
        QVERIFY(lines.isEmpty());
    }

    void shortBoundingBox_skipped()
    {
        QJsonObject obj;
        obj["label"] = "cat";
        obj["bounding_box"] = QJsonArray{10.0, 20.0};  // only 2 values
        QJsonArray dets;
        dets << obj;
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 100.0, 100.0);
        QVERIFY(lines.isEmpty());
    }

    void nonNumericBoundingBox_skipped()
    {
        QJsonObject obj;
        obj["label"] = "cat";
        obj["bounding_box"] = QJsonArray{QString("bad"), 0.0, 100.0, 100.0};
        QJsonArray dets;
        dets << obj;
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 200.0, 200.0);
        QVERIFY(lines.isEmpty());
    }

    void outOfRangeCoordinates_skipped()
    {
        // Box entirely outside image bounds → all corners clamp to edge → zero size → skipped
        QJsonArray dets;
        dets << makeDetection("cat", 300, 300, 600, 600);  // image is 200x200
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 200.0, 200.0);
        QVERIFY(lines.isEmpty());
    }

    void partiallyOffImageBox_clamped()
    {
        // Box extends beyond image edge on left → corners are clamped, valid box emitted
        // x1=-10 → 0, y1=10 → 10, x2=90 → 90, y2=90 → 90 in 100x100 image
        // cx=(0+90)/2/100=0.45, cy=(10+90)/2/100=0.5, nw=90/100=0.9, nh=80/100=0.8
        QJsonArray dets;
        dets << makeDetection("cat", -10, 10, 90, 90);
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 100.0, 100.0);
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines[0], QString("0 0.450000 0.500000 0.900000 0.800000"));
    }

    void zeroSizeBox_skipped()
    {
        // nw and nh would be 0, which fails nw <= 0.0 guard
        QJsonArray dets;
        dets << makeDetection("cat", 50, 50, 50, 50);
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 200.0, 200.0);
        QVERIFY(lines.isEmpty());
    }

    // ── multiple detections, mixed validity ───────────────────────────────────
    void multipleDetections_onlyValidEmitted()
    {
        QJsonArray dets;
        dets << makeDetection("cat", 0, 0, 100, 100);     // valid
        dets << makeDetection("ghost", 0, 0, 50, 50);     // unknown label
        dets << makeDetection("dog", 200, 0, 400, 200);   // valid, class 1
        QStringList skipped;
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat", "dog"}, 400.0, 400.0, &skipped);
        QCOMPARE(lines.size(), 2);
        QVERIFY(lines[0].startsWith("0 "));
        QVERIFY(lines[1].startsWith("1 "));
        QCOMPARE(skipped, QStringList{"ghost"});
    }

    void crlfObjList_matchesTrimmed()
    {
        // objList entries with CRLF (Windows-formatted class file) must still match
        QJsonArray dets;
        dets << makeDetection("cat", 0, 0, 50, 50);
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat\r", "dog\r"}, 100.0, 100.0);
        QCOMPARE(lines.size(), 1);
        QVERIFY(lines[0].startsWith("0 "));
    }

    void trailingSpaceLabel_matchesTrimmed()
    {
        QJsonArray dets;
        dets << makeDetection("dog", 0, 0, 50, 50);
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat", "dog  "}, 100.0, 100.0);
        QCOMPARE(lines.size(), 1);
        QVERIFY(lines[0].startsWith("1 "));
    }

    void emptyDetectionArray_returnsEmpty()
    {
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            QJsonArray{}, {"cat"}, 100.0, 100.0);
        QVERIFY(lines.isEmpty());
    }

    void nonObjectElement_ignored()
    {
        QJsonArray dets;
        dets << QJsonValue(42);              // not an object
        dets << makeDetection("cat", 0, 0, 50, 50);
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 100.0, 100.0);
        QCOMPARE(lines.size(), 1);
    }

    // ── coordinate math ───────────────────────────────────────────────────────
    void coordinateMath_correctNormalization()
    {
        // Box: x1=100, y1=200, x2=300, y2=600 in 1000x1000 image
        // cx = (100+300)/2 / 1000 = 0.2
        // cy = (200+600)/2 / 1000 = 0.4
        // nw = (300-100) / 1000 = 0.2
        // nh = (600-200) / 1000 = 0.4
        QJsonArray dets;
        dets << makeDetection("cat", 100, 200, 300, 600);
        QStringList lines = CloudAutoLabeler::parseLandingAIDetections(
            dets, {"cat"}, 1000.0, 1000.0);
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines[0], QString("0 0.200000 0.400000 0.200000 0.400000"));
    }
};

QTEST_MAIN(TestLandingAIParser)
#include "test_landing_ai_parser.moc"
