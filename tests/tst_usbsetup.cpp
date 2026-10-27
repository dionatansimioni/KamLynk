#include "core/UsbSetup.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace kl;

namespace {
void touch(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("SUBSYSTEM==\"usb\"\n");
}
} // namespace

class TestUsbSetup : public QObject
{
    Q_OBJECT
private slots:
    void installedReflectsRuleFile()
    {
        QTemporaryDir rules;
        UsbSetup usb(rules.path(), QStringLiteral("/src/rule"));
        QVERIFY(usb.available());
        QVERIFY(!usb.installed());
        QCOMPARE(usb.targetPath(), rules.filePath(QLatin1String(UsbSetup::kRuleName)));
        touch(usb.targetPath());
        QVERIFY(usb.installed());
    }

    void locatesInstalledRuleFirst()
    {
        QTemporaryDir root;
        const QString appDir = root.filePath(QStringLiteral("usr/bin"));
        const QString installed = root.filePath(QStringLiteral("usr/share/kamlynk/") + UsbSetup::kRuleName);
        const QString source = root.filePath(QStringLiteral("src"));
        QDir().mkpath(appDir);
        touch(installed);
        touch(source + QStringLiteral("/packaging/") + UsbSetup::kRuleName);
        QCOMPARE(UsbSetup::locateRule(appDir, source), QDir::cleanPath(installed));
    }

    void fallsBackToSourceTree()
    {
        QTemporaryDir root;
        const QString source = root.filePath(QStringLiteral("src"));
        touch(source + QStringLiteral("/packaging/") + UsbSetup::kRuleName);
        QCOMPARE(UsbSetup::locateRule(root.filePath(QStringLiteral("build")), source),
                 QDir::cleanPath(source + QStringLiteral("/packaging/") + UsbSetup::kRuleName));
        QVERIFY(UsbSetup::locateRule(root.filePath(QStringLiteral("x")), root.filePath(QStringLiteral("y"))).isEmpty());
    }

    void installArgumentsQuoteTarget()
    {
        const QStringList args = UsbSetup::installArguments(QStringLiteral("/etc/udev/rules.d/it's here.rules"));
        QCOMPARE(args.size(), 3);
        QCOMPARE(args[0], QStringLiteral("sh"));
        QCOMPARE(args[1], QStringLiteral("-c"));
        QVERIFY(args[2].startsWith(QStringLiteral("install -m 0644 /dev/stdin '/etc/udev/rules.d/it'\\''s here.rules'")));
        QVERIFY(args[2].contains(QStringLiteral("udevadm control --reload-rules")));
    }

    void installWithoutSourceIsNoop()
    {
        QTemporaryDir rules;
        UsbSetup usb(rules.path(), QString());
        QVERIFY(!usb.available());
        QSignalSpy finished(&usb, &UsbSetup::finished);
        usb.install();
        QVERIFY(!usb.busy());
        QCOMPARE(finished.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestUsbSetup)
#include "tst_usbsetup.moc"
