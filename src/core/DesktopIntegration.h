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

// Writes (or refreshes, when the AppImage moved) the entry and icon under `dataHome`
// (normally ~/.local/share). Returns true when something was written.
bool install(const QString &appImagePath, const QString &dataHome, const QByteArray &iconSvg,
             QString *error = nullptr);

} // namespace desktop
} // namespace kl
