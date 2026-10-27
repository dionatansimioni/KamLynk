#!/usr/bin/env bash
# Runs inside the portable build container (see build-appimage.sh).
set -euo pipefail

ROOT=/src
BUILD="$ROOT/build-portable"
APPDIR="$BUILD/AppDir"
TOOLS="$ROOT/build-deps/portable-tools"
DIST="$ROOT/dist"
export APPIMAGE_EXTRACT_AND_RUN=1 # no FUSE inside containers

# libxml2.so.2 for the SDK, built against this glibc.
LIBXML2_OUT="$BUILD/libxml2" LIBXML2_WORK="$BUILD/libxml2-work" "$ROOT/scripts/build-libxml2.sh"

mkdir -p "$TOOLS"
fetch() {
    local file="$1" url="$2"
    if [[ ! -x "$TOOLS/$file" ]]; then
        curl -fsSL -o "$TOOLS/$file" "$url"
        chmod +x "$TOOLS/$file"
    fi
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fetch appimagetool-x86_64.AppImage \
    https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage

cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DKAMLYNK_PORTABLE=ON -DLIBXML2_COMPAT_DIR="$BUILD/libxml2"
cmake --build "$BUILD"
# The tests need the newer libstdc++ (this base has GCC 11's) like an old host would.
(cd "$BUILD" && LD_LIBRARY_PATH="$LIBSTDCXX_DIR" ctest --output-on-failure)

rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD"
# Qt's CMake config links libOpenGL/libGLX into the executable (QtGui's GL calls make
# --as-needed keep them). The app makes no GL calls of its own and QtGui already gets GL
# from libGL.so.1, so drop them: libOpenGL.so.0 is missing on many desktops.
patchelf --remove-needed libOpenGL.so.0 --remove-needed libGLX.so.0 "$APPDIR/usr/bin/kamlynk"

# Fallback libraries, one per directory named after the soname; the AppRun hook adds a
# directory to the library path only when the system lacks that library (libstdc++: when it
# is older than GCC 12, which the SDK needs).
mkdir -p "$APPDIR/apprun-hooks"
mkdir -p "$APPDIR/usr/optional/libstdc++.so.6" "$APPDIR/usr/optional/libgcc_s.so.1"
cp "$LIBSTDCXX_DIR/libstdc++.so.6" "$APPDIR/usr/optional/libstdc++.so.6/"
cp "$LIBSTDCXX_DIR/libgcc_s.so.1" "$APPDIR/usr/optional/libgcc_s.so.1/"
# QtKeychain links libsecret (GNOME Keyring / KWallet's Secret Service) and Qt Network links
# Kerberos (GSSAPI); Qt Core links GLib. All are on virtually every desktop but some minimal
# installs lack one piece (openSUSE splits libgthread, minimal Debian has no OpenSSL). Since
# each is used only when absent, a newer system copy is never shadowed.
for so in libsecret-1.so.0 libgcrypt.so.20 libgpg-error.so.0 \
          libgssapi_krb5.so.2 libkrb5.so.3 libk5crypto.so.3 libkrb5support.so.0 libcom_err.so.2 \
          libkeyutils.so.1 libcrypto.so.3 \
          libglib-2.0.so.0 libgobject-2.0.so.0 libgio-2.0.so.0 libgmodule-2.0.so.0 libgthread-2.0.so.0 \
          libffi.so.8 libpcre2-8.so.0 libmount.so.1 libblkid.so.1 libselinux.so.1; do
    mkdir -p "$APPDIR/usr/optional/$so"
    cp -L "/usr/lib64/$so" "$APPDIR/usr/optional/$so/"
done
cp "$ROOT/packaging/portable/apprun-hook.sh" "$APPDIR/apprun-hooks/kamlynk-runtime.sh"

QT_PLUGINS="$("$QT_DIR/bin/qmake" -query QT_INSTALL_PLUGINS)"
# Qt Quick needs the Wayland EGL client integration, which the Qt plugin does not deploy.
mkdir -p "$APPDIR/usr/plugins"
cp -r "$QT_PLUGINS/wayland-graphics-integration-client" "$APPDIR/usr/plugins/"
WAYLAND_EXTRA=()
for so in "$APPDIR"/usr/plugins/wayland-graphics-integration-client/*.so; do
    WAYLAND_EXTRA+=(--deploy-deps-only "$so")
done

export QMAKE="$QT_DIR/bin/qmake"
export LD_LIBRARY_PATH="$QT_DIR/lib:/opt/qtkeychain/lib:$BUILD/lib"
export QML_SOURCES_PATHS="$ROOT/qml"
export EXTRA_QT_MODULES="svg;waylandclient"
# xcb + native Wayland for desktops; offscreen for headless self-checks (--sdk-check).
export EXTRA_PLATFORM_PLUGINS="libqwayland.so;libqoffscreen.so"
# Never bundle: the SDK's dlopen'ed plugins (bin/CrAdapter), libstdc++/libgcc_s (handled
# above), and libraries that must match the host's desktop session (GLib/D-Bus/systemd:
# always present and backward compatible, and host plugins such as libsecret need them).
# OpenSSL/Kerberos (needed by Qt Network) likewise: an older bundled libcrypto shadows the
# system one and breaks host libraries that need newer symbols.
export LINUXDEPLOY_EXCLUDED_LIBRARIES="libCr_PTP_IP.so;libCr_PTP_USB.so;libssh2.so;libusb-1.0.so;libstdc++.so*;libgcc_s.so*;libglib-2.0.so*;libgobject-2.0.so*;libgio-2.0.so*;libgmodule-2.0.so*;libgthread-2.0.so*;libdbus-1.so*;libsystemd.so*;libsecret-1.so*;libgcrypt.so*;libgpg-error.so*;libcrypto.so*;libssl.so*;libgssapi_krb5.so*;libkrb5.so*;libk5crypto.so*;libkrb5support.so*;libcom_err.so*;libkeyutils.so*;libselinux.so*;libOpenGL.so*;libGLX.so*"

"$TOOLS/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/kamlynk" \
    --desktop-file "$ROOT/packaging/kamlynk.desktop" \
    --icon-file "$ROOT/packaging/kamlynk.svg" \
    --plugin qt \
    "${WAYLAND_EXTRA[@]}"

# Drop libraries nothing in the AppDir needs (dependencies of the excluded GLib/systemd
# that linuxdeploy copied anyway), repeating until nothing changes.
while :; do
    needed="$(find "$APPDIR" -type f \( -name '*.so*' -o -path '*/usr/bin/kamlynk' \) -exec objdump -p {} \; 2>/dev/null \
        | awk '/NEEDED/ {print $2}' | sort -u)"
    removed=0
    for lib in "$APPDIR"/usr/lib/*.so*; do
        name="$(basename "$lib")"
        if ! grep -qxF "$name" <<<"$needed"; then
            echo "removendo $name (nada no AppImage precisa dele)"
            rm -f "$lib"
            removed=1
        fi
    done
    [[ $removed -eq 1 ]] || break
done

# Guards against silently losing pieces of the portable layout.
for f in usr/plugins/platforms/libqxcb.so usr/plugins/platforms/libqwayland.so \
         usr/plugins/platforms/libqoffscreen.so \
         usr/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so \
         usr/optional/libstdc++.so.6/libstdc++.so.6 usr/optional/libsecret-1.so.0/libsecret-1.so.0 \
         usr/bin/CrAdapter/libCr_PTP_IP.so usr/lib/libCr_Core.so \
         usr/lib/libxml2.so.2 apprun-hooks/kamlynk-runtime.sh; do
    [[ -e "$APPDIR/$f" ]] || { echo "AppDir sem $f" >&2; exit 1; }
done
# Nothing bundled may need a newer glibc than this base (2.34).
too_new="$(find "$APPDIR" -name '*.so*' -o -path '*/bin/kamlynk' | while read -r f; do
    v="$(objdump -T "$f" 2>/dev/null | grep -o 'GLIBC_[0-9.]*' | sed 's/GLIBC_//' | sort -V | tail -n1)"
    [[ -n "$v" && "$(printf '%s\n2.34\n' "$v" | sort -V | tail -n1)" != "2.34" ]] && echo "$f ($v)"
done || true)"
[[ -z "$too_new" ]] || { echo "Precisam de glibc > 2.34: $too_new" >&2; exit 1; }

VERSION="$(grep -oP 'project\(KamLynk VERSION \K[0-9.]+' "$ROOT/CMakeLists.txt")"
mkdir -p "$DIST"
ARCH=x86_64 VERSION="$VERSION" "$TOOLS/appimagetool-x86_64.AppImage" --no-appstream \
    "$APPDIR" "$DIST/KamLynk-$VERSION-x86_64.AppImage"
