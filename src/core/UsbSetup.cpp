#include "core/UsbSetup.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace kl {

namespace {
QString shellQuote(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + out + QLatin1Char('\'');
}
} // namespace

UsbSetup::UsbSetup(QObject *parent)
    : UsbSetup(QStringLiteral("/etc/udev/rules.d"),
               locateRule(QCoreApplication::applicationDirPath(), QStringLiteral(KAMLYNK_SOURCE_DIR)), parent)
{
}

UsbSetup::UsbSetup(const QString &rulesDir, const QString &sourceRule, QObject *parent)
    : QObject(parent)
    , m_rulesDir(rulesDir)
    , m_sourceRule(sourceRule)
{
}

QString UsbSetup::targetPath() const
{
    return QDir(m_rulesDir).filePath(QLatin1String(kRuleName));
}

bool UsbSetup::installed() const
{
    return QFileInfo::exists(targetPath());
}

QString UsbSetup::locateRule(const QString &applicationDir, const QString &sourceDir)
{
    const QString name = QLatin1String(kRuleName);
    const QStringList candidates = {
        QDir(applicationDir).filePath(QStringLiteral("../share/kamlynk/") + name), // install / AppImage
        QDir(sourceDir).filePath(QStringLiteral("packaging/") + name),             // build tree
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return QDir::cleanPath(QFileInfo(c).absoluteFilePath());
    }
    return {};
}

QStringList UsbSetup::installArguments(const QString &target)
{
    // The AppImage mount is not readable by root on some systems, so pipe the content.
    const QString script = QStringLiteral("install -m 0644 /dev/stdin %1 && udevadm control --reload-rules "
                                          "&& udevadm trigger --subsystem-match=usb")
                               .arg(shellQuote(target));
    return {QStringLiteral("sh"), QStringLiteral("-c"), script};
}

void UsbSetup::install()
{
    if (m_busy || !available())
        return;
    QFile rule(m_sourceRule);
    if (!rule.open(QIODevice::ReadOnly)) {
        emit finished(false, tr("Não foi possível ler a regra USB"));
        return;
    }
    const QByteArray content = rule.readAll();

    auto *process = new QProcess(this);
    m_busy = true;
    emit changed();
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        m_busy = false;
        const bool ok = status == QProcess::NormalExit && code == 0 && installed();
        emit changed();
        emit finished(ok, ok ? tr("Acesso USB liberado. Reconecte o cabo da câmera.")
                             : tr("Não foi possível instalar a regra USB (código %1)").arg(code));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return; // other errors are followed by finished()
        process->deleteLater();
        m_busy = false;
        emit changed();
        emit finished(false, tr("pkexec não está disponível"));
    });
    process->start(QStringLiteral("pkexec"), installArguments(targetPath()));
    process->write(content);
    process->closeWriteChannel();
}

} // namespace kl
