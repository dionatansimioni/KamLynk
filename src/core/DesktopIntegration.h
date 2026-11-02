#pragma once

#include <QByteArray>
#include <QString>

namespace kl {

// Registers the running AppImage with the desktop: ~/.local/share/applications/kamlynk.desktop
// and the hicolor icon. Wayland compositors (KDE, GNOME) look up the window icon through the
// app id "kamlynk" -> kamlynk.desktop; without it they show a generic Wayland icon.
// It also puts KamLynk in the application menu.
namespace desktop {

QString desktopEntry(const QString &appImagePath);

// Desktop file id (file name without ".desktop") of an entry that another tool (Gear Lever,
// AppImageLauncher...) created for this AppImage under `dataHome`/applications; empty if none.
// The window must use that id as its app id, or KDE shows a second taskbar icon.
QString externalEntry(const QString &appImagePath, const QString &dataHome);

// Writes (or refreshes, when the AppImage moved) the entry and icon under `dataHome`
// (normally ~/.local/share). Returns true when something was written. When another tool
// already integrated the AppImage, writes nothing and removes the entry written before,
// so the app is not listed twice. A kamlynk.desktop written by someone else is left alone.
bool install(const QString &appImagePath, const QString &dataHome, const QByteArray &iconSvg,
             QString *error = nullptr);

} // namespace desktop
} // namespace kl
