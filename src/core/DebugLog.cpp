#include "core/DebugLog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QSettings>
#include <QUrl>

namespace kl {

namespace {

const QString kEnabledKey = QStringLiteral("debug/logToFile");

QMutex g_mutex;
QFile *g_file = nullptr; // open while logging is enabled
QtMessageHandler g_previous = nullptr;

void handler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        QMutexLocker lock(&g_mutex);
        if (g_file) {
            static const char *levels[] = {"DEBUG", "WARN", "CRIT", "FATAL", "INFO"};
            const QString line = QStringLiteral("%1 [%2] %3\n")
                                     .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                          QLatin1String(levels[type]), message);
            g_file->write(line.toUtf8());
            g_file->flush();
        }
    }
    if (g_previous)
        g_previous(type, context, message);
}

} // namespace

DebugLog::DebugLog(const QString &filePath, QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_filePath(filePath)
    , m_settings(settings)
{
    g_previous = qInstallMessageHandler(handler);
    if (m_settings && m_settings->value(kEnabledKey, false).toBool())
        open();
}

DebugLog::~DebugLog()
{
    close();
    qInstallMessageHandler(g_previous);
    g_previous = nullptr;
}

bool DebugLog::enabled() const
{
    QMutexLocker lock(&g_mutex);
    return g_file != nullptr;
}

void DebugLog::setEnabled(bool enabled)
{
    if (enabled == this->enabled())
        return;
    if (m_settings)
        m_settings->setValue(kEnabledKey, enabled);
    if (enabled) {
        open();
    } else {
        qInfo() << "debug log stopped";
        close();
    }
    emit enabledChanged();
}

void DebugLog::open()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    auto *file = new QFile(m_filePath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    mode |= file->size() > kMaxSize ? QIODevice::Truncate : QIODevice::Append;
    if (!file->open(mode)) {
        qWarning().noquote() << "cannot write the debug log" << m_filePath << file->errorString();
        delete file;
        return;
    }
    {
        QMutexLocker lock(&g_mutex);
        g_file = file;
    }
    qInfo().noquote() << "debug log started:" << QCoreApplication::applicationName()
                      << QCoreApplication::applicationVersion();
}

void DebugLog::close()
{
    QMutexLocker lock(&g_mutex);
    delete g_file;
    g_file = nullptr;
}

void DebugLog::openFolder() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_filePath).absolutePath()));
}

} // namespace kl
