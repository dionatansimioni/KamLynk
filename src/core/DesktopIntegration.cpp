#include "core/DesktopIntegration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

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

const QString kOwnEntry = QStringLiteral("kamlynk.desktop");
const QByteArray kOwnMarker = QByteArrayLiteral("X-KamLynk-AppImage=true");

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// True when one of the entry's Exec=/TryExec= lines runs `paths` (quoted or not, with or
// without arguments and environment variables in front).
bool runs(const QByteArray &entry, const QStringList &paths)
{
    for (const QByteArray &raw : entry.split('\n')) {
        const QByteArray line = raw.trimmed();
        if (!line.startsWith("Exec=") && !line.startsWith("TryExec="))
            continue;
        const QString value = QString::fromUtf8(line.mid(line.indexOf('=') + 1));
        for (const QString &path : paths) {
            const qsizetype at = value.indexOf(path);
            if (at < 0)
                continue;
            // Whole path only: "/a/kamlynk.appimage" must not match "/a/kamlynk.appimage.bak".
            const qsizetype end = at + path.size();
            if (end == value.size() || value.at(end).isSpace() || value.at(end) == u'\''
                || value.at(end) == u'"')
                return true;
        }
    }
    return false;
}

} // namespace

QString externalEntry(const QString &appImagePath, const QString &dataHome)
{
    QStringList paths{appImagePath};
    const QString canonical = QFileInfo(appImagePath).canonicalFilePath();
    if (!canonical.isEmpty() && canonical != appImagePath)
        paths << canonical;
    const QDir dir(QDir(dataHome).filePath(QStringLiteral("applications")));
    const QStringList files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name);
    for (const QString &name : files) {
        const QByteArray entry = readFile(dir.filePath(name));
        if (entry.contains(kOwnMarker) || !runs(entry, paths))
            continue;
        return name.chopped(int(sizeof(".desktop") - 1));
    }
    return {};
}

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
    const QString ownPath = home.filePath(QStringLiteral("applications/") + kOwnEntry);
    const QByteArray current = readFile(ownPath);
    const bool ours = current.isEmpty() || current.contains(kOwnMarker);
    if (!externalEntry(appImagePath, dataHome).isEmpty()) {
        // Integrated by another tool: a second entry would duplicate the app in menus,
        // in that tool and in the taskbar.
        if (ours && QFile::exists(ownPath) && QFile::remove(ownPath))
            return true;
        return false;
    }
    if (!ours)
        return false; // someone else's kamlynk.desktop (for another copy of the app)
    if (!writeIfChanged(ownPath, desktopEntry(appImagePath).toUtf8(), &changed, error))
        return false;
    if (!iconSvg.isEmpty()
        && !writeIfChanged(home.filePath(QStringLiteral("icons/hicolor/scalable/apps/kamlynk.svg")), iconSvg,
                           &changed, error))
        return false;
    return changed;
}

} // namespace kl::desktop
