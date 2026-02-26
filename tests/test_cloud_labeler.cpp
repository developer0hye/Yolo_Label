#include <QtTest>
#include "cloud_labeler.h"

class TestCloudLabeler : public QObject
{
    Q_OBJECT

private slots:
    // ── mimeForImage ─────────────────────────────────────────────
    void mimeForImage_png()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.png"), QString("image/png"));
    }
    void mimeForImage_PNG_uppercase()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.PNG"), QString("image/png"));
    }
    void mimeForImage_jpg()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.jpg"), QString("image/jpeg"));
    }
    void mimeForImage_bmp()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.bmp"), QString("image/bmp"));
    }
    void mimeForImage_tif()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.tif"), QString("image/tiff"));
    }
    void mimeForImage_tiff()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.tiff"), QString("image/tiff"));
    }
    void mimeForImage_webp()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.webp"), QString("image/webp"));
    }
    void mimeForImage_unknownExtension()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo.xyz"), QString("image/jpeg"));
    }
    void mimeForImage_noExtension()
    {
        QCOMPARE(CloudAutoLabeler::mimeForImage("/tmp/photo"), QString("image/jpeg"));
    }

    // ── labelPathFor ─────────────────────────────────────────────
    void labelPathFor_basic()
    {
        QCOMPARE(CloudAutoLabeler::labelPathFor("/data/images/cat.jpg"),
                 QString("/data/images/cat.txt"));
    }
    void labelPathFor_differentExtension()
    {
        QCOMPARE(CloudAutoLabeler::labelPathFor("/data/images/cat.png"),
                 QString("/data/images/cat.txt"));
    }
    void labelPathFor_multiDotFilename()
    {
        // baseName() returns the part before the first dot
        QCOMPARE(CloudAutoLabeler::labelPathFor("/data/images/my.photo.v2.jpg"),
                 QString("/data/images/my.txt"));
    }
    void labelPathFor_relativePath()
    {
        QCOMPARE(CloudAutoLabeler::labelPathFor("images/dog.bmp"),
                 QString("images/dog.txt"));
    }

    // ── filterValidDetections ────────────────────────────────────
    void filterValid_singleValidLine()
    {
        QString result = CloudAutoLabeler::filterValidDetections(
            "0 0.5 0.5 0.2 0.3\n", 3);
        QCOMPARE(result, QString("0 0.5 0.5 0.2 0.3\n"));
    }
    void filterValid_multipleLines()
    {
        QString input = "0 0.5 0.5 0.2 0.3\n1 0.1 0.2 0.3 0.4\n";
        QString result = CloudAutoLabeler::filterValidDetections(input, 3);
        QCOMPARE(result, QString("0 0.5 0.5 0.2 0.3\n1 0.1 0.2 0.3 0.4\n"));
    }
    void filterValid_classOutOfRange()
    {
        // class 5 is out of range when numClasses=3
        QString result = CloudAutoLabeler::filterValidDetections(
            "5 0.5 0.5 0.2 0.3\n", 3);
        QCOMPARE(result, QString(""));
    }
    void filterValid_coordOutOfRange()
    {
        // cx = 1.5 is out of [0,1]
        QString result = CloudAutoLabeler::filterValidDetections(
            "0 1.5 0.5 0.2 0.3\n", 3);
        QCOMPARE(result, QString(""));
    }
    void filterValid_zeroWidth()
    {
        // width = 0.0 is rejected (must be > 0)
        QString result = CloudAutoLabeler::filterValidDetections(
            "0 0.5 0.5 0.0 0.3\n", 3);
        QCOMPARE(result, QString(""));
    }
    void filterValid_malformedWrongFieldCount()
    {
        // only 4 fields instead of 5
        QString result = CloudAutoLabeler::filterValidDetections(
            "0 0.5 0.5 0.2\n", 3);
        QCOMPARE(result, QString(""));
    }
    void filterValid_nonNumeric()
    {
        QString result = CloudAutoLabeler::filterValidDetections(
            "0 abc 0.5 0.2 0.3\n", 3);
        QCOMPARE(result, QString(""));
    }
    void filterValid_emptyInput()
    {
        QString result = CloudAutoLabeler::filterValidDetections("", 3);
        QCOMPARE(result, QString(""));
    }
    void filterValid_numClassesZero()
    {
        // numClasses=0 should reject everything
        QString result = CloudAutoLabeler::filterValidDetections(
            "0 0.5 0.5 0.2 0.3\n", 0);
        QCOMPARE(result, QString());
    }
    void filterValid_windowsLineEndings()
    {
        // Windows \r\n line endings should be handled
        QString result = CloudAutoLabeler::filterValidDetections(
            "0 0.5 0.5 0.2 0.3\r\n1 0.1 0.2 0.3 0.4\r\n", 3);
        QCOMPARE(result, QString("0 0.5 0.5 0.2 0.3\n1 0.1 0.2 0.3 0.4\n"));
    }

    // ── remapWithClassNames ──────────────────────────────────────
    void remap_basicMatch()
    {
        QStringList serverNames = {"person", "car"};
        QStringList localNames  = {"car", "person"};
        // server ID 0 = "person" -> local ID 1
        // server ID 1 = "car"    -> local ID 0
        QString result = CloudAutoLabeler::remapWithClassNames(
            "0 0.5 0.5 0.2 0.3\n", serverNames, localNames);
        QCOMPARE(result, QString("1 0.5 0.5 0.2 0.3\n"));
    }
    void remap_caseInsensitive()
    {
        QStringList serverNames = {"Person", "CAR"};
        QStringList localNames  = {"person", "car"};
        QString result = CloudAutoLabeler::remapWithClassNames(
            "0 0.5 0.5 0.2 0.3\n1 0.1 0.2 0.3 0.4\n",
            serverNames, localNames);
        QCOMPARE(result, QString("0 0.5 0.5 0.2 0.3\n1 0.1 0.2 0.3 0.4\n"));
    }
    void remap_unmatchedDropped()
    {
        QStringList serverNames = {"person", "helicopter"};
        QStringList localNames  = {"person", "car"};
        // server ID 1 = "helicopter" has no local match -> dropped
        QString result = CloudAutoLabeler::remapWithClassNames(
            "0 0.5 0.5 0.2 0.3\n1 0.1 0.2 0.3 0.4\n",
            serverNames, localNames);
        QCOMPARE(result, QString("0 0.5 0.5 0.2 0.3\n"));
    }
    void remap_emptyLocalClasses()
    {
        QStringList serverNames = {"person"};
        QStringList localNames;
        QString result = CloudAutoLabeler::remapWithClassNames(
            "0 0.5 0.5 0.2 0.3\n", serverNames, localNames);
        QCOMPARE(result, QString());
    }
    void remap_trimmedWhitespace()
    {
        QStringList serverNames = {"  person  "};
        QStringList localNames  = {"person"};
        QString result = CloudAutoLabeler::remapWithClassNames(
            "0 0.5 0.5 0.2 0.3\n", serverNames, localNames);
        QCOMPARE(result, QString("0 0.5 0.5 0.2 0.3\n"));
    }
    void remap_multipleLinesWithMixed()
    {
        QStringList serverNames = {"dog", "cat", "bird"};
        QStringList localNames  = {"cat", "dog"};
        // server 0 = "dog" -> local 1
        // server 1 = "cat" -> local 0
        // server 2 = "bird" -> dropped
        QString result = CloudAutoLabeler::remapWithClassNames(
            "0 0.5 0.5 0.2 0.3\n1 0.1 0.2 0.3 0.4\n2 0.8 0.8 0.1 0.1\n",
            serverNames, localNames);
        QCOMPARE(result, QString("1 0.5 0.5 0.2 0.3\n0 0.1 0.2 0.3 0.4\n"));
    }
    void remap_coordValidation()
    {
        // Invalid coords should be filtered even when class name matches
        QStringList serverNames = {"person"};
        QStringList localNames  = {"person"};
        QString result = CloudAutoLabeler::remapWithClassNames(
            "0 1.5 0.5 0.2 0.3\n", serverNames, localNames);
        QCOMPARE(result, QString(""));
    }
};

QTEST_GUILESS_MAIN(TestCloudLabeler)
#include "test_cloud_labeler.moc"
