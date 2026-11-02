#pragma once

#include <QObject>
#include <QString>

class QSettings;

namespace kl {

// Optional log file for bug reports (Settings › "Registrar log de depuração", off by default).
// While enabled, every Qt log line is appended to `filePath`; the console output is kept
// either way. The choice is saved, so a problem can be reproduced after a restart.
class DebugLog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)

public:
    // Only one instance may exist at a time (it installs the Qt message handler).
    // Does not take ownership of `settings` (may be null: nothing is saved).
    explicit DebugLog(const QString &filePath, QSettings *settings = nullptr, QObject *parent = nullptr);
    ~DebugLog() override;

    // Above this size the file starts over when logging begins.
    static constexpr qint64 kMaxSize = 5 * 1024 * 1024;

    bool enabled() const;
    void setEnabled(bool enabled);
    QString filePath() const { return m_filePath; }
    // Shows the file in the file manager.
    Q_INVOKABLE void openFolder() const;

signals:
    void enabledChanged();

private:
    void open();
    void close();

    QString m_filePath;
    QSettings *m_settings;
};

} // namespace kl
