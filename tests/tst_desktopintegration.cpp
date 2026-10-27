#include "core/DesktopIntegration.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace kl;

class TestDesktopIntegration : public QObject
{
    Q_OBJECT
private slots:
    void entryPointsToAppImage()
    {
        const QString entry = desktop::desktopEntry(QStringLiteral("/home/me/Apps/KamLynk 0.1.AppImage"));
        QVERIFY(entry.startsWith(QStringLiteral("[Desktop Entry]\n")));
        QVERIFY(entry.contains(QStringLiteral("Exec=\"/home/me/Apps/KamLynk 0.1.AppImage\"\n")));
        QVERIFY(entry.contains(QStringLiteral("Icon=kamlynk\n")));
        // Must match QGuiApplication::desktopFileName / the Wayland app id.
        QVERIFY(entry.contains(QStringLiteral("StartupWMClass=kamlynk\n")));
    }

    void execQuotesSpecialCharacters()
    {
        const QString entry = desktop::desktopEntry(QStringLiteral("/tmp/a\"b$c.AppImage"));
        QVERIFY(entry.contains(QStringLiteral("Exec=\"/tmp/a\\\"b\\$c.AppImage\"\n")));
    }

    void installWritesOnceAndRefreshesWhenMoved()
    {
        QTemporaryDir home;
        const QByteArray icon("<svg/>");
        QVERIFY(desktop::install(QStringLiteral("/opt/KamLynk.AppImage"), home.path(), icon));
        QFile entry(home.filePath(QStringLiteral("applications/kamlynk.desktop")));
        QVERIFY(entry.open(QIODevice::ReadOnly));
        QVERIFY(entry.readAll().contains("Exec=\"/opt/KamLynk.AppImage\""));
        entry.close();
        QFile svg(home.filePath(QStringLiteral("icons/hicolor/scalable/apps/kamlynk.svg")));
        QVERIFY(svg.open(QIODevice::ReadOnly));
        QCOMPARE(svg.readAll(), icon);

        // Same AppImage again: nothing to do.
        QVERIFY(!desktop::install(QStringLiteral("/opt/KamLynk.AppImage"), home.path(), icon));
        // AppImage moved: entry refreshed.
        QVERIFY(desktop::install(QStringLiteral("/home/me/KamLynk.AppImage"), home.path(), icon));
        QVERIFY(entry.open(QIODevice::ReadOnly));
        QVERIFY(entry.readAll().contains("Exec=\"/home/me/KamLynk.AppImage\""));
    }
};

QTEST_GUILESS_MAIN(TestDesktopIntegration)
#include "tst_desktopintegration.moc"
