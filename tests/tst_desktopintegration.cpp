#include "core/DesktopIntegration.h"

#include <QDir>
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

    // Gear Lever names its entry after the app (kamlynk.desktop) or, on a clash, adds a suffix;
    // Exec is shell-quoted and may carry arguments.
    void entryFromAnotherToolIsFound()
    {
        QTemporaryDir home;
        QVERIFY(QDir(home.path()).mkpath(QStringLiteral("applications")));
        const QString app = QStringLiteral("/home/me/AppImages/kamlynk.appimage");
        QVERIFY(desktop::externalEntry(app, home.path()).isEmpty());
        writeEntry(home, QStringLiteral("other.desktop"), "Exec=/usr/bin/other %U\n");
        writeEntry(home, QStringLiteral("backup.desktop"), "Exec=/home/me/AppImages/kamlynk.appimage.bak\n");
        QVERIFY(desktop::externalEntry(app, home.path()).isEmpty());
        writeEntry(home, QStringLiteral("kamlynk_1.desktop"),
                   "[Desktop Entry]\nName=KamLynk\nExec='/home/me/AppImages/kamlynk.appimage' %U\n");
        QCOMPARE(desktop::externalEntry(app, home.path()), QStringLiteral("kamlynk_1"));
    }

    void ownEntryIsNotExternal()
    {
        QTemporaryDir home;
        const QString app = QStringLiteral("/opt/KamLynk.AppImage");
        QVERIFY(desktop::install(app, home.path(), QByteArray("<svg/>")));
        QVERIFY(desktop::externalEntry(app, home.path()).isEmpty());
    }

    void installStepsAsideForAnotherTool()
    {
        QTemporaryDir home;
        const QString app = QStringLiteral("/home/me/AppImages/kamlynk.appimage");
        // Started once before Gear Lever integrated it: our entry exists.
        QVERIFY(desktop::install(app, home.path(), QByteArray("<svg/>")));
        const QString own = home.filePath(QStringLiteral("applications/kamlynk.desktop"));
        QVERIFY(QFile::exists(own));
        writeEntry(home, QStringLiteral("kamlynk_0.7.desktop"),
                   "TryExec=/home/me/AppImages/kamlynk.appimage\nExec=/home/me/AppImages/kamlynk.appimage\n");
        QVERIFY(desktop::install(app, home.path(), QByteArray("<svg/>")));
        QVERIFY(!QFile::exists(own));
        QVERIFY(!desktop::install(app, home.path(), QByteArray("<svg/>")));
        QVERIFY(!QFile::exists(own));
    }

    void foreignKamlynkEntryIsKept()
    {
        QTemporaryDir home;
        const QByteArray gearLever("[Desktop Entry]\nName=KamLynk\nExec=/home/me/AppImages/kamlynk.appimage\n");
        writeEntry(home, QStringLiteral("kamlynk.desktop"), gearLever);
        const QString own = home.filePath(QStringLiteral("applications/kamlynk.desktop"));
        // Started from Gear Lever: its kamlynk.desktop is the app's entry.
        QVERIFY(!desktop::install(QStringLiteral("/home/me/AppImages/kamlynk.appimage"), home.path(), "<svg/>"));
        QCOMPARE(desktop::externalEntry(QStringLiteral("/home/me/AppImages/kamlynk.appimage"), home.path()),
                 QStringLiteral("kamlynk"));
        // Another copy of the AppImage started elsewhere: still not overwritten.
        QVERIFY(!desktop::install(QStringLiteral("/tmp/KamLynk.AppImage"), home.path(), "<svg/>"));
        QFile file(own);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), gearLever);
    }

private:
    static void writeEntry(const QTemporaryDir &home, const QString &name, const QByteArray &content)
    {
        QVERIFY(QDir(home.path()).mkpath(QStringLiteral("applications")));
        QFile file(home.filePath(QStringLiteral("applications/") + name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
    }
};

QTEST_GUILESS_MAIN(TestDesktopIntegration)
#include "tst_desktopintegration.moc"
