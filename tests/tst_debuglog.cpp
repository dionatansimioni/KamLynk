#include "core/DebugLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace kl;

class TestDebugLog : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString logPath() const { return m_dir.filePath(QStringLiteral("logs/kamlynk.log")); }

    QByteArray logContents() const
    {
        QFile file(logPath());
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

private slots:
    void init()
    {
        QFile::remove(logPath());
        QFile::remove(m_dir.filePath(QStringLiteral("prefs.ini")));
    }

    void offByDefault()
    {
        QSettings settings(m_dir.filePath(QStringLiteral("prefs.ini")), QSettings::IniFormat);
        DebugLog log(logPath(), &settings);
        QVERIFY(!log.enabled());
        qInfo() << "not recorded";
        QVERIFY(!QFile::exists(logPath()));
    }

    void recordsOnlyWhileEnabled()
    {
        DebugLog log(logPath());
        QSignalSpy changed(&log, &DebugLog::enabledChanged);
        log.setEnabled(true);
        QCOMPARE(changed.count(), 1);
        qInfo() << "first line";
        qWarning() << "a warning";
        log.setEnabled(false);
        qInfo() << "after stop";
        const QByteArray text = logContents();
        QVERIFY(text.contains("[INFO] first line"));
        QVERIFY(text.contains("[WARN] a warning"));
        QVERIFY(!text.contains("after stop"));
    }

    void choiceIsRememberedAndFileKept()
    {
        QSettings settings(m_dir.filePath(QStringLiteral("prefs.ini")), QSettings::IniFormat);
        {
            DebugLog log(logPath(), &settings);
            log.setEnabled(true);
            qInfo() << "first session";
        }
        // Next start: still on, and the earlier session is still in the file.
        DebugLog log(logPath(), &settings);
        QVERIFY(log.enabled());
        qInfo() << "second session";
        const QByteArray text = logContents();
        QVERIFY(text.contains("first session"));
        QVERIFY(text.contains("second session"));
    }

    void oversizedFileStartsOver()
    {
        QDir().mkpath(m_dir.filePath(QStringLiteral("logs")));
        QFile big(logPath());
        QVERIFY(big.open(QIODevice::WriteOnly));
        big.write(QByteArray(DebugLog::kMaxSize + 1, 'x'));
        big.close();
        DebugLog log(logPath());
        log.setEnabled(true);
        qInfo() << "fresh";
        QVERIFY(QFileInfo(logPath()).size() < 4096);
    }
};

QTEST_GUILESS_MAIN(TestDebugLog)
#include "tst_debuglog.moc"
