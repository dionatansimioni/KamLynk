#include "core/DesktopIntegration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace kl::desktop {

namespace {

// Quotes an Exec argument as the Desktop Entry spec requires.
QString quoteExec(QString path)
{
    for (const char *c : {"\\", "\"", "`", "$"})
        path.replace(QLatin1String(c), QStringLiteral("\\") + QLatin1String(c));
    return QLatin1Char('"') + path + QLatin1Char('"');
}

bool writeIfChanged(const QString &path, const QByteArray &content, bool *changed, QString *error)
{
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == content)
        return true;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    *changed = true;
    return true;
}

} // namespace

QString desktopEntry(const QString &appImagePath)
{
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=KamLynk\n"
                          "GenericName=Controle remoto de câmera\n"
                          "Comment=Controle câmeras Sony pelo Wi-Fi ou USB: prévia, gravação, exposição e foco\n"
                          "Exec=%1\n"
                          "TryExec=%2\n"
                          "Icon=kamlynk\n"
                          "Terminal=false\n"
                          "Categories=AudioVideo;Video;Photography;\n"
                          "Keywords=sony;camera;zv-e10;remote;live view;\n"
                          "StartupWMClass=kamlynk\n"
                          "X-KamLynk-AppImage=true\n")
        .arg(quoteExec(appImagePath), appImagePath);
}

bool install(const QString &appImagePath, const QString &dataHome, const QByteArray &iconSvg, QString *error)
{
    bool changed = false;
    const QDir home(dataHome);
    if (!writeIfChanged(home.filePath(QStringLiteral("applications/kamlynk.desktop")),
                        desktopEntry(appImagePath).toUtf8(), &changed, error))
        return false;
    if (!iconSvg.isEmpty()
        && !writeIfChanged(home.filePath(QStringLiteral("icons/hicolor/scalable/apps/kamlynk.svg")), iconSvg,
                           &changed, error))
        return false;
    return changed;
}

} // namespace kl::desktop
