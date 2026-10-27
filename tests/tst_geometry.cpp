#include "core/Geometry.h"

#include <QTest>

using namespace kl;

class TestGeometry : public QObject
{
    Q_OBJECT
private slots:
    void fitLetterboxesWideFrame()
    {
        const QRectF r = geometry::fitRect(QSizeF(1024, 576), QRectF(0, 0, 1000, 1000));
        QCOMPARE(r.width(), 1000.0);
        QCOMPARE(r.height(), 562.5);
        QCOMPARE(r.y(), (1000 - 562.5) / 2);
        QCOMPARE(r.x(), 0.0);
    }

    void fitPillarboxesInWideArea()
    {
        const QRectF r = geometry::fitRect(QSizeF(640, 480), QRectF(10, 0, 1600, 600));
        QCOMPARE(r.height(), 600.0);
        QCOMPARE(r.width(), 800.0);
        QCOMPARE(r.x(), 10.0 + 400.0);
    }

    void fitEmpty()
    {
        QVERIFY(geometry::fitRect(QSizeF(), QRectF(0, 0, 10, 10)).isNull());
        QVERIFY(geometry::fitRect(QSizeF(10, 10), QRectF()).isNull());
    }

    void normalized()
    {
        const QRectF img(100, 50, 200, 100);
        QCOMPARE(geometry::normalizedIn(img, QPointF(100, 50)), QPointF(0, 0));
        QCOMPARE(geometry::normalizedIn(img, QPointF(200, 100)), QPointF(0.5, 0.5));
        QCOMPARE(geometry::normalizedIn(img, QPointF(50, 60)), QPointF(-1, -1));
    }

    void normalizedMirrored()
    {
        const QRectF img(100, 50, 200, 100);
        QCOMPARE(geometry::normalizedIn(img, QPointF(150, 75), true), QPointF(0.75, 0.25));
        QCOMPARE(geometry::normalizedIn(img, QPointF(200, 100), true), QPointF(0.5, 0.5));
        QCOMPARE(geometry::normalizedIn(img, QPointF(50, 60), true), QPointF(-1, -1));
    }
};

QTEST_GUILESS_MAIN(TestGeometry)
#include "tst_geometry.moc"
