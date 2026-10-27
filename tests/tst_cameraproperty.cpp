#include "core/CameraProperty.h"
#include "core/PropertyCodec.h"

#include "CrDefines.h"
#include "CrDeviceProperty.h"

#include <QSignalSpy>
#include <QTest>

using namespace kl;
namespace SDK = SCRSDK;

namespace {
struct Recorder
{
    QList<QPair<quint32, qint64>> calls;
    CameraProperty::Setter setter()
    {
        return [this](quint32 c, qint64 v, quint32) { calls.append({c, v}); };
    }
};

PropertyInfo makeInfo(quint32 code, quint32 type, qint64 current, QVector<qint64> table, bool writable = true)
{
    PropertyInfo p;
    p.code = code;
    p.dataType = type;
    p.current = current;
    p.readable = true;
    p.writable = writable;
    codec::applyValueTable(p, table);
    return p;
}
} // namespace

class TestCameraProperty : public QObject
{
    Q_OBJECT
private slots:
    void unavailableUntilUpdated()
    {
        Recorder r;
        CameraProperty p(SDK::CrDeviceProperty_IsoSensitivity, r.setter());
        QVERIFY(!p.available());
        QVERIFY(!p.writable());
        QCOMPARE(p.text(), QStringLiteral("--"));
        p.stepBy(1);
        QVERIFY(r.calls.isEmpty());
    }

    void discreteStepping()
    {
        Recorder r;
        CameraProperty p(SDK::CrDeviceProperty_IsoSensitivity, r.setter());
        QSignalSpy changed(&p, &CameraProperty::changed);
        p.update(makeInfo(SDK::CrDeviceProperty_IsoSensitivity, SDK::CrDataType_UInt32Array, 800, {400, 800, 1600}));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(p.text(), QStringLiteral("ISO 800"));
        QCOMPARE(p.options(), (QStringList{"ISO 400", "ISO 800", "ISO 1600"}));
        QCOMPARE(p.index(), 1);
        QVERIFY(p.canIncrease());
        QVERIFY(p.canDecrease());

        p.stepBy(1);
        QCOMPARE(r.calls.last(), qMakePair(quint32(SDK::CrDeviceProperty_IsoSensitivity), qint64(1600)));
        p.stepBy(-5); // clamps to first
        QCOMPARE(r.calls.last().second, qint64(400));
        p.setIndex(1); // same as current: no request
        QCOMPARE(r.calls.size(), 2);
        p.setIndex(99);
        QCOMPARE(r.calls.size(), 2);
    }

    void edgesDisableButtons()
    {
        Recorder r;
        CameraProperty p(SDK::CrDeviceProperty_FNumber, r.setter());
        p.update(makeInfo(SDK::CrDeviceProperty_FNumber, SDK::CrDataType_UInt16Array, 280, {280, 400}));
        QVERIFY(!p.canDecrease());
        QVERIFY(p.canIncrease());
    }

    void readOnlyIgnoresWrites()
    {
        Recorder r;
        CameraProperty p(SDK::CrDeviceProperty_FNumber, r.setter());
        p.update(makeInfo(SDK::CrDeviceProperty_FNumber, SDK::CrDataType_UInt16Array, 280, {280, 400}, false));
        QVERIFY(p.available());
        QVERIFY(!p.writable());
        p.stepBy(1);
        p.setValue(400);
        QVERIFY(r.calls.isEmpty());
    }

    void rangeExpandsAndSteps()
    {
        Recorder r;
        CameraProperty p(SDK::CrDeviceProperty_Colortemp, r.setter());
        p.update(makeInfo(SDK::CrDeviceProperty_Colortemp, SDK::CrDataType_UInt16Range, 5500, {2500, 9900, 100}));
        QCOMPARE(p.candidates().size(), 75);
        QCOMPARE(p.options().first(), QStringLiteral("2500K"));
        QCOMPARE(p.index(), 30);
        p.stepBy(2);
        QCOMPARE(r.calls.last().second, qint64(5700));
        p.stepBy(-1000);
        QCOMPARE(r.calls.last().second, qint64(2500));
    }

    void hugeRangeNotExpanded()
    {
        Recorder r;
        CameraProperty p(SDK::CrDeviceProperty_MediaSLOT1_RemainingTime, r.setter());
        p.update(makeInfo(SDK::CrDeviceProperty_MediaSLOT1_RemainingTime, SDK::CrDataType_UInt32Range, 10,
                          {0, 0x7FFFFFFF, 1}));
        QVERIFY(p.candidates().isEmpty());
    }

    void resetMakesUnavailable()
    {
        Recorder r;
        CameraProperty p(SDK::CrDeviceProperty_FNumber, r.setter());
        p.update(makeInfo(SDK::CrDeviceProperty_FNumber, SDK::CrDataType_UInt16Array, 280, {280, 400}));
        p.reset();
        QVERIFY(!p.available());
        QCOMPARE(p.code(), int(SDK::CrDeviceProperty_FNumber));
    }
};

QTEST_GUILESS_MAIN(TestCameraProperty)
#include "tst_cameraproperty.moc"
