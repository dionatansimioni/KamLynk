#include "core/DeviceHistory.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace kl;

namespace {
CameraDescriptor camera(const QString &model, const QString &address, const QString &uid,
                        const QString &connection = QStringLiteral("Wi-Fi/LAN"))
{
    CameraDescriptor d;
    d.model = model;
    d.address = address;
    d.uid = uid;
    d.connection = connection;
    return d;
}
const QDateTime t0(QDate(2026, 10, 26), QTime(12, 0));
} // namespace

class TestDeviceHistory : public QObject
{
    Q_OBJECT
private slots:
    void touchPutsMostRecentFirst()
    {
        DeviceHistory h;
        h.touch(camera("ZV-E10M2", "192.0.2.10", "AA"), t0);
        h.touch(camera("ILCE-7M4", "192.0.2.50", "BB"), t0.addSecs(10));
        QCOMPARE(h.entries().size(), 2);
        QCOMPARE(h.entries()[0].model, QStringLiteral("ILCE-7M4"));
        h.touch(camera("ZV-E10M2", "192.0.2.10", "AA"), t0.addSecs(20));
        QCOMPARE(h.entries().size(), 2); // updated, not duplicated
        QCOMPARE(h.entries()[0].model, QStringLiteral("ZV-E10M2"));
        QCOMPARE(h.entries()[0].lastUsed, t0.addSecs(20));
    }

    void matchesByMacEvenWhenIpChanged()
    {
        DeviceHistory h;
        h.touch(camera("ZV-E10M2", "192.0.2.10", "AA"), t0);
        QCOMPARE(h.find(camera("ZV-E10M2", "192.0.2.120", "AA")), 0);
        QCOMPARE(h.find(camera("ZV-E10M2", "192.0.2.10", "CC")), -1); // another body, same IP
        QVERIFY(h.refreshAddress(camera("ZV-E10M2", "192.0.2.120", "AA")));
        QCOMPARE(h.entries()[0].address, QStringLiteral("192.0.2.120"));
        QVERIFY(!h.refreshAddress(camera("ZV-E10M2", "192.0.2.120", "AA")));
    }

    void manualIpEntryMatchesByAddressAndLearnsMac()
    {
        DeviceHistory h;
        h.touch(camera("ZV-E10M2", "192.0.2.10", ""), t0); // connected by typing the IP
        QCOMPARE(h.find(camera("ZV-E10M2", "192.0.2.10", "AA")), 0);
        QVERIFY(h.refreshAddress(camera("ZV-E10M2", "192.0.2.10", "AA")));
        QCOMPARE(h.entries()[0].uid, QStringLiteral("AA"));
    }

    void keepsAtMostTwelve()
    {
        DeviceHistory h;
        for (int i = 0; i < 20; ++i)
            h.touch(camera("ZV-E10M2", QStringLiteral("10.0.0.%1").arg(i), QString::number(i)), t0.addSecs(i));
        QCOMPARE(h.entries().size(), DeviceHistory::kMaxEntries);
        QCOMPARE(h.entries()[0].address, QStringLiteral("10.0.0.19"));
    }

    void saveLoadRoundTrip()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath("h.ini"), QSettings::IniFormat);
        DeviceHistory h;
        auto &e = h.touch(camera("ZV-E10M2", "192.0.2.10", "AA"), t0);
        e.fingerprint = QStringLiteral("R/0Mq");
        e.savedLogin = true;
        h.touch(camera("ILCE-7M4", "", "SERIAL", "USB"), t0.addSecs(5));
        h.save(s);

        DeviceHistory loaded;
        loaded.load(s);
        QCOMPARE(loaded.entries().size(), 2);
        QCOMPARE(loaded.entries()[0].connection, QStringLiteral("USB"));
        const auto &z = loaded.entries()[1];
        QCOMPARE(z.model, QStringLiteral("ZV-E10M2"));
        QCOMPARE(z.fingerprint, QStringLiteral("R/0Mq"));
        QVERIFY(z.savedLogin);
        QCOMPARE(z.lastUsed, t0);
        QCOMPARE(loaded.findKey(z.key()), 1);
    }

    void migratesLastCameraSetting()
    {
        QTemporaryDir dir;
        QSettings s(dir.filePath("old.ini"), QSettings::IniFormat);
        s.setValue("lastCamera/model", "ZV-E10M2");
        s.setValue("lastCamera/connection", "Wi-Fi/LAN");
        s.setValue("lastCamera/address", "192.0.2.10");
        DeviceHistory h;
        h.load(s);
        QCOMPARE(h.entries().size(), 1);
        QCOMPARE(h.entries()[0].address, QStringLiteral("192.0.2.10"));
        h.save(s);
        QVERIFY(!s.contains("lastCamera/model"));
    }
};

QTEST_GUILESS_MAIN(TestDeviceHistory)
#include "tst_devicehistory.moc"
