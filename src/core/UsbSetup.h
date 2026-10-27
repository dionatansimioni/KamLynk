#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace kl {

// Installs the udev rule that lets a normal user talk to Sony cameras over USB.
// Uses pkexec (graphical password prompt); Wi-Fi does not need it.
class UsbSetup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
    static constexpr const char *kRuleName = "99-kamlynk-sony-camera.rules";

    explicit UsbSetup(QObject *parent = nullptr);
    UsbSetup(const QString &rulesDir, const QString &sourceRule, QObject *parent = nullptr);

    bool installed() const;
    bool available() const { return !m_sourceRule.isEmpty(); }
    bool busy() const { return m_busy; }
    QString targetPath() const;
    QString sourceRule() const { return m_sourceRule; }

    // First existing rule file among the usual install locations relative to the binary.
    static QString locateRule(const QString &applicationDir, const QString &sourceDir);
    // Arguments for pkexec: write the rule (read from stdin) and reload udev.
    static QStringList installArguments(const QString &target);

    Q_INVOKABLE void install();

signals:
    void changed();
    void finished(bool ok, const QString &message);

private:
    QString m_rulesDir;
    QString m_sourceRule;
    bool m_busy = false;
};

} // namespace kl
