#include <QtTest>
#include <QApplication>
#include <QTemporaryFile>
#include <QTextStream>
#include "label_img.h"

class TestLabelImg : public QObject
{
    Q_OBJECT

private slots:
    // ── getRelativeRectFromTwoPoints ─────────────────────────────
    void relativeRect_normalDiagonal()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QRectF r = widget.getRelativeRectFromTwoPoints(QPointF(0.1, 0.2), QPointF(0.5, 0.6));
        QCOMPARE(r.left(),  0.1);
        QCOMPARE(r.top(),   0.2);
        QCOMPARE(r.width(), 0.4);
        QCOMPARE(r.height(),0.4);
    }
    void relativeRect_reversedOrder()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        // p2 is top-left, p1 is bottom-right — should still produce same rect
        QRectF r = widget.getRelativeRectFromTwoPoints(QPointF(0.5, 0.6), QPointF(0.1, 0.2));
        QCOMPARE(r.left(),  0.1);
        QCOMPARE(r.top(),   0.2);
        QCOMPARE(r.width(), 0.4);
        QCOMPARE(r.height(),0.4);
    }
    void relativeRect_samePoint()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QRectF r = widget.getRelativeRectFromTwoPoints(QPointF(0.3, 0.3), QPointF(0.3, 0.3));
        QCOMPARE(r.width(), 0.0);
        QCOMPARE(r.height(),0.0);
    }
    void relativeRect_zeroHeightLine()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QRectF r = widget.getRelativeRectFromTwoPoints(QPointF(0.1, 0.5), QPointF(0.9, 0.5));
        QCOMPARE(r.height(), 0.0);
        QCOMPARE(r.width(),  0.8);
    }

    // ── findBoxUnderCursor ───────────────────────────────────────
    void findBox_noBoxes()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QCOMPARE(widget.findBoxUnderCursor(QPointF(0.5, 0.5)), -1);
    }
    void findBox_pointInside()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.4, 0.4);
        widget.m_objBoundingBoxes.append(box);

        QCOMPARE(widget.findBoxUnderCursor(QPointF(0.3, 0.3)), 0);
    }
    void findBox_pointOutside()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.4, 0.4);
        widget.m_objBoundingBoxes.append(box);

        QCOMPARE(widget.findBoxUnderCursor(QPointF(0.1, 0.1)), -1);
    }
    void findBox_smallestBoxWins()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog"};

        ObjectLabelingBox bigBox;
        bigBox.label = 0;
        bigBox.box = QRectF(0.1, 0.1, 0.8, 0.8);
        widget.m_objBoundingBoxes.append(bigBox);

        ObjectLabelingBox smallBox;
        smallBox.label = 1;
        smallBox.box = QRectF(0.3, 0.3, 0.2, 0.2);
        widget.m_objBoundingBoxes.append(smallBox);

        // Point is inside both; smallest (width+height) should win
        QCOMPARE(widget.findBoxUnderCursor(QPointF(0.35, 0.35)), 1);
    }
    void findBox_overlapping()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog"};

        ObjectLabelingBox box1;
        box1.label = 0;
        box1.box = QRectF(0.1, 0.1, 0.5, 0.5);
        widget.m_objBoundingBoxes.append(box1);

        ObjectLabelingBox box2;
        box2.label = 1;
        box2.box = QRectF(0.3, 0.3, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box2);

        // Point in overlap region — box2 is smaller
        QCOMPARE(widget.findBoxUnderCursor(QPointF(0.4, 0.4)), 1);
    }

    // ── moveBox ──────────────────────────────────────────────────
    void moveBox_basic()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box);

        widget.moveBox(0, 0.1, 0.1);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.x(), 0.3);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.y(), 0.3);
    }
    void moveBox_clampLeft()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.1, 0.5, 0.2, 0.2);
        widget.m_objBoundingBoxes.append(box);

        widget.moveBox(0, -0.5, 0.0);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.x(), 0.0);
    }
    void moveBox_clampRight()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.5, 0.5, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box);

        widget.moveBox(0, 0.5, 0.0);
        // x should be clamped to 1.0 - width = 0.7
        QCOMPARE(widget.m_objBoundingBoxes[0].box.x(), 0.7);
    }
    void moveBox_clampTop()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.5, 0.1, 0.2, 0.2);
        widget.m_objBoundingBoxes.append(box);

        widget.moveBox(0, 0.0, -0.5);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.y(), 0.0);
    }
    void moveBox_clampBottom()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.5, 0.5, 0.2, 0.4);
        widget.m_objBoundingBoxes.append(box);

        widget.moveBox(0, 0.0, 0.5);
        // y should be clamped to 1.0 - height = 0.6
        QCOMPARE(widget.m_objBoundingBoxes[0].box.y(), 0.6);
    }
    void moveBox_invalidIndex()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        // Should not crash with invalid index
        widget.moveBox(-1, 0.1, 0.1);
        widget.moveBox(5, 0.1, 0.1);
    }

    // ── resizeBox ────────────────────────────────────────────────
    void resizeBox_basic()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box);

        widget.resizeBox(0, 0.1, 0.1);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.width(),  0.4);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.height(), 0.4);
    }
    void resizeBox_minBoxDimClamp()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.5, 0.5, 0.01, 0.01);
        widget.m_objBoundingBoxes.append(box);

        // Shrink below MIN_BOX_DIM (0.002)
        widget.resizeBox(0, -0.05, -0.05);
        QVERIFY(widget.m_objBoundingBoxes[0].box.width()  >= 0.002);
        QVERIFY(widget.m_objBoundingBoxes[0].box.height() >= 0.002);
    }
    void resizeBox_clampToEdge()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.8, 0.8, 0.1, 0.1);
        widget.m_objBoundingBoxes.append(box);

        // Try to grow beyond right/bottom edge
        widget.resizeBox(0, 0.5, 0.5);
        // Width should be clamped to 1.0 - x = 0.2
        QCOMPARE(widget.m_objBoundingBoxes[0].box.width(),  0.2);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.height(), 0.2);
    }
    void resizeBox_invalidIndex()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        // Should not crash
        widget.resizeBox(-1, 0.1, 0.1);
        widget.resizeBox(5, 0.1, 0.1);
    }

    // ── loadLabelData ────────────────────────────────────────────
    void loadLabel_validFile()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog", "bird"};

        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        QTextStream ts(&tmp);
        ts << "0 0.5 0.5 0.2 0.3\n";
        ts.flush();

        widget.loadLabelData(tmp.fileName());
        QCOMPARE(widget.m_objBoundingBoxes.size(), 1);
        QCOMPARE(widget.m_objBoundingBoxes[0].label, 0);
        // midX=0.5, width=0.2 -> leftX=0.4
        QCOMPARE(widget.m_objBoundingBoxes[0].box.x(), 0.4);
        // midY=0.5, height=0.3 -> topY=0.35
        QCOMPARE(widget.m_objBoundingBoxes[0].box.y(), 0.35);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.width(),  0.2);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.height(), 0.3);
    }
    void loadLabel_multipleBoxes()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog", "bird"};

        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        QTextStream ts(&tmp);
        ts << "0 0.5 0.5 0.2 0.3\n"
           << "1 0.1 0.2 0.3 0.4\n"
           << "2 0.8 0.8 0.1 0.1\n";
        ts.flush();

        widget.loadLabelData(tmp.fileName());
        QCOMPARE(widget.m_objBoundingBoxes.size(), 3);
        QCOMPARE(widget.m_objBoundingBoxes[0].label, 0);
        QCOMPARE(widget.m_objBoundingBoxes[1].label, 1);
        QCOMPARE(widget.m_objBoundingBoxes[2].label, 2);
    }
    void loadLabel_classOutOfRange()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"}; // only class 0 is valid

        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        QTextStream ts(&tmp);
        ts << "5 0.5 0.5 0.2 0.3\n"; // class 5 out of range
        ts.flush();

        widget.loadLabelData(tmp.fileName());
        QCOMPARE(widget.m_objBoundingBoxes.size(), 0);
    }
    void loadLabel_negativeClassId()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog"};

        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        QTextStream ts(&tmp);
        // Valid box, then box with negative class ID -> filtered
        ts << "0 0.5 0.5 0.2 0.3\n"
           << "-1 0.1 0.2 0.3 0.4\n";
        ts.flush();

        widget.loadLabelData(tmp.fileName());
        QCOMPARE(widget.m_objBoundingBoxes.size(), 1);
    }
    void loadLabel_emptyFile()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        // write nothing
        tmp.flush();

        widget.loadLabelData(tmp.fileName());
        QCOMPARE(widget.m_objBoundingBoxes.size(), 0);
    }
    void loadLabel_nonexistentFile()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        widget.loadLabelData("/nonexistent/path/file.txt");
        QCOMPARE(widget.m_objBoundingBoxes.size(), 0);
    }

    // ── cvtRelativeToAbsolutePoint / cvtAbsoluteToRelativePoint ─
    void coordConvert_identity()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        // At zoom=1.0, pan=(0,0): relative (0.5, 0.5) -> absolute (320, 240)
        QPoint abs = widget.cvtRelativeToAbsolutePoint(QPointF(0.5, 0.5));
        QCOMPARE(abs, QPoint(320, 240));
    }
    void coordConvert_reverse()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        // absolute (320, 240) -> relative (0.5, 0.5)
        QPointF rel = widget.cvtAbsoluteToRelativePoint(QPoint(320, 240));
        QCOMPARE(rel, QPointF(0.5, 0.5));
    }
    void coordConvert_roundTrip()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QPointF original(0.25, 0.75);
        QPoint abs = widget.cvtRelativeToAbsolutePoint(original);
        QPointF back = widget.cvtAbsoluteToRelativePoint(abs);
        // Allow small rounding error from int conversion
        QVERIFY(qAbs(back.x() - original.x()) < 0.01);
        QVERIFY(qAbs(back.y() - original.y()) < 0.01);
    }
    void coordConvert_zeroWidgetSizeGuard()
    {
        label_img widget;
        widget.resize(0, 0);
        widget.init();

        // Should not crash and should return (0,0)
        QPointF result = widget.cvtAbsoluteToRelativePoint(QPoint(100, 100));
        QCOMPARE(result, QPointF(0.0, 0.0));
    }
};

QTEST_MAIN(TestLabelImg)
#include "test_label_img.moc"
