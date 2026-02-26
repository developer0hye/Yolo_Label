#include <QtTest>
#include <QApplication>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QPainter>
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
    // ── undo / redo / saveState ────────────────────────────────
    void undo_emptyHistoryReturnsFalse()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QCOMPARE(widget.undo(), false);
    }
    void undo_restoresPreviousState()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        // Add a box and save state
        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.1, 0.1, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box);
        widget.saveState(); // saves [box]

        // Add another box
        ObjectLabelingBox box2;
        box2.label = 0;
        box2.box = QRectF(0.5, 0.5, 0.2, 0.2);
        widget.m_objBoundingBoxes.append(box2);
        QCOMPARE(widget.m_objBoundingBoxes.size(), 2);

        // Undo should restore to [box] only
        QCOMPARE(widget.undo(), true);
        QCOMPARE(widget.m_objBoundingBoxes.size(), 1);
    }
    void redo_afterUndo()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        widget.saveState(); // saves []

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.1, 0.1, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box);

        widget.undo(); // back to []
        QCOMPARE(widget.m_objBoundingBoxes.size(), 0);

        QCOMPARE(widget.redo(), true);
        QCOMPARE(widget.m_objBoundingBoxes.size(), 1);
    }
    void redo_emptyReturnsFalse()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QCOMPARE(widget.redo(), false);
    }
    void redo_clearedAfterNewEdit()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        widget.saveState(); // saves []
        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.1, 0.1, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box);

        widget.undo(); // back to [], redo available
        QCOMPARE(widget.redo(), true); // redo works

        widget.undo(); // back to []
        widget.saveState(); // new edit clears redo
        QCOMPARE(widget.redo(), false); // redo cleared
    }
    void clearAllBoxes_savesStateThenClears()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.1, 0.1, 0.3, 0.3);
        widget.m_objBoundingBoxes.append(box);

        widget.clearAllBoxes();
        QCOMPARE(widget.m_objBoundingBoxes.size(), 0);

        // Should be able to undo back to 1 box
        QCOMPARE(widget.undo(), true);
        QCOMPARE(widget.m_objBoundingBoxes.size(), 1);
    }
    void clearUndoHistory_clearsBoth()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        widget.saveState();
        widget.saveState();
        widget.undo(); // creates redo

        widget.clearUndoHistory();
        QCOMPARE(widget.undo(), false);
        QCOMPARE(widget.redo(), false);
    }
    void maxUndoHistory_limitsStack()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        // Push 55 states (MAX_UNDO_HISTORY = 50)
        for (int i = 0; i < 55; ++i) {
            ObjectLabelingBox box;
            box.label = 0;
            box.box = QRectF(0.01 * i, 0.01 * i, 0.1, 0.1);
            widget.m_objBoundingBoxes.clear();
            widget.m_objBoundingBoxes.append(box);
            widget.saveState();
        }

        // Can undo at most 50 times
        int undoCount = 0;
        while (widget.undo()) ++undoCount;
        QCOMPARE(undoCount, 50);
    }

    // ── setFocusedObjectBoxLabel ─────────────────────────────────
    void setFocusedLabel_basic()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog"};
        widget.m_drawObjectBoxColor = {Qt::green, Qt::red};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.4, 0.4);
        widget.m_objBoundingBoxes.append(box);

        widget.setFocusedObjectBoxLabel(QPointF(0.3, 0.3), 1);
        QCOMPARE(widget.m_objBoundingBoxes[0].label, 1);
    }
    void setFocusedLabel_pointNotOnBox()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.4, 0.4);
        widget.m_objBoundingBoxes.append(box);

        widget.setFocusedObjectBoxLabel(QPointF(0.1, 0.1), 1);
        QCOMPARE(widget.m_objBoundingBoxes[0].label, 0); // unchanged
    }
    void setFocusedLabel_outOfRange()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.4, 0.4);
        widget.m_objBoundingBoxes.append(box);

        widget.setFocusedObjectBoxLabel(QPointF(0.3, 0.3), 5); // out of range
        QCOMPARE(widget.m_objBoundingBoxes[0].label, 0); // unchanged
    }
    void setFocusedLabel_savesUndoState()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat", "dog"};
        widget.m_drawObjectBoxColor = {Qt::green, Qt::red};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.4, 0.4);
        widget.m_objBoundingBoxes.append(box);

        widget.setFocusedObjectBoxLabel(QPointF(0.3, 0.3), 1);
        QCOMPARE(widget.m_objBoundingBoxes[0].label, 1);

        widget.undo();
        QCOMPARE(widget.m_objBoundingBoxes[0].label, 0);
    }

    // ── setContrastGamma ─────────────────────────────────────────
    void gamma_identity()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        // gamma=1.0 should not crash (showImage is no-op without image)
        widget.setContrastGamma(1.0f);
    }
    void gamma_zero()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.setContrastGamma(0.0f);
    }
    void gamma_high()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.setContrastGamma(3.0f);
    }

    // ── cvtRelativeToAbsoluteRectInUi ────────────────────────────
    void absRect_basicAtZoom1()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QRect r = widget.cvtRelativeToAbsoluteRectInUi(QRectF(0.0, 0.0, 0.5, 0.5));
        QCOMPARE(r.x(), 0);
        QCOMPARE(r.y(), 0);
        QCOMPARE(r.width(), 320);
        QCOMPARE(r.height(), 240);
    }
    void absRect_zeroSizeRect()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QRect r = widget.cvtRelativeToAbsoluteRectInUi(QRectF(0.5, 0.5, 0.0, 0.0));
        QCOMPARE(r.width(), 0);
        QCOMPARE(r.height(), 0);
    }

    // ── isOpened ─────────────────────────────────────────────────
    void isOpened_falseByDefault()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        QCOMPARE(widget.isOpened(), false);
    }
    void isOpened_trueAfterOpenImage()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        // Create a small temp PNG
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString imgPath = tmpDir.path() + "/test.png";
        QImage img(10, 10, QImage::Format_RGB888);
        img.fill(Qt::red);
        QVERIFY(img.save(imgPath));

        bool ret = false;
        widget.openImage(imgPath, ret);
        QCOMPARE(ret, true);
        QCOMPARE(widget.isOpened(), true);
    }

    // ── zoomIn / zoomOut / resetZoom ─────────────────────────────
    void resetZoom_resetsValues()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        // Create a small temp image so zoom functions work
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString imgPath = tmpDir.path() + "/test.png";
        QImage img(100, 100, QImage::Format_RGB888);
        img.fill(Qt::blue);
        QVERIFY(img.save(imgPath));

        bool ret = false;
        widget.openImage(imgPath, ret);
        QVERIFY(ret);

        // Zoom in a few times
        widget.zoomIn(QPoint(320, 240));
        widget.zoomIn(QPoint(320, 240));
        widget.zoomIn(QPoint(320, 240));

        // Reset
        widget.resetZoom();

        // Verify (0,0) at zoom 1.0 maps correctly
        QPoint p = widget.cvtRelativeToAbsolutePoint(QPointF(0.5, 0.5));
        QCOMPARE(p, QPoint(320, 240));
    }
    void zoomIn_increasesZoom()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString imgPath = tmpDir.path() + "/test.png";
        QImage img(100, 100, QImage::Format_RGB888);
        img.fill(Qt::blue);
        QVERIFY(img.save(imgPath));

        bool ret = false;
        widget.openImage(imgPath, ret);
        QVERIFY(ret);

        // At zoom 1.0, relative (1.0, 1.0) maps to (640, 480)
        QPoint before = widget.cvtRelativeToAbsolutePoint(QPointF(1.0, 1.0));
        QCOMPARE(before, QPoint(640, 480));

        widget.zoomIn(QPoint(320, 240));

        // After zoom in, relative (1.0, 1.0) should map beyond widget bounds
        QPoint after = widget.cvtRelativeToAbsolutePoint(QPointF(1.0, 1.0));
        QVERIFY(after.x() > 640);
        QVERIFY(after.y() > 480);
    }
    void zoomOut_doesNotGoBelowOne()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();

        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString imgPath = tmpDir.path() + "/test.png";
        QImage img(100, 100, QImage::Format_RGB888);
        img.fill(Qt::blue);
        QVERIFY(img.save(imgPath));

        bool ret = false;
        widget.openImage(imgPath, ret);
        QVERIFY(ret);

        // Zoom out when already at 1.0 — should stay at 1.0
        widget.zoomOut(QPoint(320, 240));
        widget.zoomOut(QPoint(320, 240));

        QPoint p = widget.cvtRelativeToAbsolutePoint(QPointF(0.5, 0.5));
        QCOMPARE(p, QPoint(320, 240));
    }

    // ── Edge cases for existing functions ─────────────────────────
    void moveBox_zeroMovement()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.3, 0.3, 0.2, 0.2);
        widget.m_objBoundingBoxes.append(box);

        widget.moveBox(0, 0.0, 0.0);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.x(), 0.3);
        QCOMPARE(widget.m_objBoundingBoxes[0].box.y(), 0.3);
    }
    void findBox_pointOnEdge()
    {
        label_img widget;
        widget.resize(640, 480);
        widget.init();
        widget.m_objList = {"cat"};

        ObjectLabelingBox box;
        box.label = 0;
        box.box = QRectF(0.2, 0.2, 0.4, 0.4);
        widget.m_objBoundingBoxes.append(box);

        // QRectF::contains includes left/top edges
        QVERIFY(widget.findBoxUnderCursor(QPointF(0.2, 0.2)) != -1);
    }
};

QTEST_MAIN(TestLabelImg)
#include "test_label_img.moc"
